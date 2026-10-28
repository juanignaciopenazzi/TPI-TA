/*
  ===========================================================================
  TPI IoT 4K3 - SKETCH 1: SENSADO + ENVÍO A THINGSPEAK + OTA
  Grupo 13
  ---------------------------------------------------------------------------
  Integrantes (Apellido, Nombre - Legajo):
    - Vinciguerra, Nicolas - 95645
    - Carle, Lucia         - 94221
    - Castro, Julieta      - 94855
    - Penazzi, Juan Ignacio - 95653
  ===========================================================================

  Este sketch se sube por CABLE USB. Una vez cargado, la ESP32:
    1. Lee el potenciómetro y con ese valor ajusta el brillo del LED integrado (PWM 12 bits).
    2. Lee temperatura y humedad del sensor DHT.
    3. Con dos pines táctiles sube / baja el brillo del LED externo (PWM 12 bits).
    4. Muestra todo en la pantalla en tiempo real.
    5. Cada 16 segundos manda 6 valores a un canal de ThingSpeak.
    6. Habilita OTA: queda escuchando en la red para que le subamos el Sketch 2 por WiFi.

  Todo lo que hay que adaptar a la placa y a la red está en la sección 1 (CONFIGURACIÓN).
  Explicación detallada de cada parte: carpeta docs/ del repositorio.
*/

#include <WiFi.h>         // conectarse a una red WiFi (viene con el core ESP32)
#include <ArduinoOTA.h>   // subir programas por WiFi (viene con el core ESP32)
#include <ThingSpeak.h>   // librería "ThingSpeak" de MathWorks
#include <DHT.h>          // librería "DHT sensor library" de Adafruit
#include <Wire.h>         // comunicación I2C con la pantalla
#include <stdio.h>
#include <string.h>

// ===========================================================================
// 1. CONFIGURACIÓN  (lo único que deberían tener que tocar)
// ===========================================================================

// --- WiFi (la ESP32 sólo se conecta a redes de 2.4 GHz) ---
const char* WIFI_SSID     = "TU_WIFI";
const char* WIFI_PASSWORD = "TU_CLAVE_WIFI";
const unsigned long WIFI_REINTENTO_MS = 10000;            // reintento cada 10 s si se cae el WiFi

// --- ThingSpeak (sale de la pestaña "API Keys" del canal) ---
const unsigned long CANAL_ID = 3556336;                   // Channel ID del Grupo 13
const char* WRITE_API_KEY = "JQUB9QEZPB1XHC5K";           // Write API Key del canal
const unsigned long INTERVALO_ENVIO_MS = 16000;           // 16 s (ThingSpeak gratis pide >= 15 s)

// --- OTA (subida de programas por WiFi) ---
const char*    OTA_NOMBRE = "ESP32-OTA-G13";  // así aparece el "puerto de red" en el Arduino IDE
const uint16_t OTA_PUERTO = 3232;             // puerto establecido para OTA (3232 es el estándar)
const char*    OTA_CLAVE  = "";               // "" = sin contraseña

// --- Pines de la placa ---
const int PIN_POTENCIOMETRO = 34;  // DEBE ser ADC1 (GPIO 32 a 39): el ADC2 no funciona con el WiFi prendido
const int PIN_LED_INTEGRADO = 2;   // LED azul que trae la ESP32 DevKit
const int PIN_LED_EXTERNO   = 18;
const int PIN_TOUCH_SUBIR   = 32;  // pin táctil T9
const int PIN_TOUCH_BAJAR   = 33;  // pin táctil T8
const int PIN_DHT           = 4;   // Pin GPIO seguro para el arranque; verificar con la placa
#define   TIPO_DHT DHT11           // DHT11 (azul) o DHT22 (blanco)
const int PIN_SDA = 21;            // I2C de la pantalla
const int PIN_SCL = 22;

// --- Pantalla ---
#define PANTALLA_OLED 1            // OLED 128x64 SSD1306
#define PANTALLA_LCD  2            // LCD de caracteres con módulo I2C
#define TIPO_PANTALLA PANTALLA_OLED
const uint8_t DIRECCION_PANTALLA = 0x3C;  // OLED: 0x3C  |  LCD: 0x27 o 0x3F (ver prueba_pines)
const int LCD_COLUMNAS = 16;
const int LCD_FILAS    = 2;

// --- PWM y ADC ---
const int PWM_FRECUENCIA = 5000;   // Hz
const int PWM_RESOLUCION = 12;     // bits -> valores de 0 a 4095
const int PWM_MAXIMO     = 4095;   // 2^12 - 1
const int POT_MUESTRAS   = 8;      // cantidad de lecturas promediadas para filtrar ruido del ADC
const unsigned long POT_INTERVALO_MS = 20;

// --- Touch ---
const float TOUCH_SENSIBILIDAD = 0.7;     // "tocado" si la lectura cae por debajo del 70% del reposo
const int   TOUCH_PASO = 41;              // cuánto sube/baja el PWM por paso (~100 pasos de 0 a 4095)
const int   TOUCH_MUESTRAS = 3;           // lecturas promediadas para evitar falsos toques por ruido WiFi
const unsigned long TOUCH_INTERVALO_MS = 20;

// ===========================================================================
// 2. PANTALLA  (funciones que sirven igual para OLED o LCD)
// ===========================================================================

#if TIPO_PANTALLA == PANTALLA_OLED
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  const int PANTALLA_FILAS = 4;                 // 4 renglones de 16 píxeles en 64 de alto
  Adafruit_SSD1306 oled(128, 64, &Wire, -1);
#else
  #include <LiquidCrystal_I2C.h>
  const int PANTALLA_FILAS = LCD_FILAS;
  LiquidCrystal_I2C lcd(DIRECCION_PANTALLA, LCD_COLUMNAS, LCD_FILAS);
#endif

bool pantallaIniciada = false;

void pantallaIniciar() {
#if TIPO_PANTALLA == PANTALLA_OLED
  pantallaIniciada = oled.begin(SSD1306_SWITCHCAPVCC, DIRECCION_PANTALLA);
  if (!pantallaIniciada) {
    Serial.println("No se encontro la pantalla OLED (revisar direccion y pines I2C)");
    return;
  }
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);  // evita que un renglón largo invada el renglón siguiente
#else
  lcd.init();
  lcd.backlight();
  pantallaIniciada = true;
#endif
}

// Muestra hasta PANTALLA_FILAS renglones de texto.
void pantallaMostrar(const char* const lineas[], int cantidad) {
  if (!pantallaIniciada) return;
#if TIPO_PANTALLA == PANTALLA_OLED
  oled.clearDisplay();
  for (int i = 0; i < cantidad && i < PANTALLA_FILAS; i++) {
    oled.setCursor(0, i * 16);
    oled.print(lineas[i]);
  }
  oled.display();
#else
  for (int i = 0; i < PANTALLA_FILAS; i++) {
    char texto[LCD_COLUMNAS + 1];
    snprintf(texto, sizeof(texto), "%-*.*s", LCD_COLUMNAS, LCD_COLUMNAS,
             i < cantidad ? lineas[i] : "");
    lcd.setCursor(0, i);
    lcd.print(texto);
  }
#endif
}

void mostrarMensaje(const char* linea1, const char* linea2) {
  const char* lineas[2] = {linea1, linea2};
  pantallaMostrar(lineas, 2);
}

// ===========================================================================
// 3. PWM  (funciona con el core ESP32 versión 2.x y 3.x)
// ===========================================================================

bool configurarPWM(int pin, int canal) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  return ledcAttach(pin, PWM_FRECUENCIA, PWM_RESOLUCION);
#else
  if (ledcSetup(canal, PWM_FRECUENCIA, PWM_RESOLUCION) == 0) return false;
  ledcAttachPin(pin, canal);  // en Core 2.x devuelve void
  return true;
#endif
}

void escribirPWM(int pin, int canal, int valor) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, valor);
#else
  ledcWrite(canal, valor);
#endif
}

const int CANAL_LED_INTEGRADO = 0;  // sólo se usan con el core 2.x
const int CANAL_LED_EXTERNO   = 1;

// ===========================================================================
// 4. VARIABLES GLOBALES  (el "estado" del programa)
// ===========================================================================

WiFiClient clienteWiFi;
DHT dht(PIN_DHT, TIPO_DHT);

int   pwmLedIntegrado = 0;        // 0..4095, sale del potenciómetro
int   pwmLedExterno = 0;          // 0..4095, lo mueven los touch
int   porcentajeLedExterno = 0;   // 0..100
float temperatura = NAN;          // NAN = "todavía no hay lectura válida"
float humedad = NAN;

uint32_t touchReposoSubir = 0;    // valor del touch sin tocar (se mide al arrancar)
uint32_t touchReposoBajar = 0;

bool   otaIniciado = false;
int    ultimoPorcentajeOta = -1;
bool   pwmLedIntegradoIniciado = false;
bool   pwmLedExternoIniciado = false;
char   estadoThingSpeak[20] = "esperando";

unsigned long ultimoEnvioMs = 0;
unsigned long ultimaLecturaDhtMs = 0;
unsigned long ultimoPotMs = 0;
unsigned long ultimoTouchMs = 0;
unsigned long ultimaPantallaMs = 0;
unsigned long ultimoReintentoWiFiMs = 0;

// ===========================================================================
// 5. WIFI Y OTA
// ===========================================================================

void formatearIP(char* buffer, size_t tamano) {
  IPAddress ip = WiFi.localIP();
  snprintf(buffer, tamano, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void conectarWiFi(unsigned long esperaMaximaMs) {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a WiFi");
  mostrarMensaje("Conectando WiFi", WIFI_SSID);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < esperaMaximaMs) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    char ip[16];
    formatearIP(ip, sizeof(ip));
    Serial.printf("\nConectado. IP: %s\n", ip);
    mostrarMensaje("WiFi OK", ip);
    delay(2000);
  } else {
    ultimoReintentoWiFiMs = millis();
    Serial.println("\nNo se pudo conectar. Sigo sin WiFi y reintento en segundo plano.");
    mostrarMensaje("Sin WiFi", "reintentando...");
    delay(1500);
  }
}

void iniciarOTA() {
  ArduinoOTA.setHostname(OTA_NOMBRE);
  ArduinoOTA.setPort(OTA_PUERTO);
  if (strlen(OTA_CLAVE) > 0) ArduinoOTA.setPassword(OTA_CLAVE);

  // Estas funciones se ejecutan solas durante una actualización por OTA
  ArduinoOTA.onStart([]() {
    ultimoPorcentajeOta = -1;
    Serial.println("OTA: empezando actualizacion");
    mostrarMensaje("Actualizando", "por OTA...");
  });
  ArduinoOTA.onProgress([](unsigned int recibido, unsigned int total) {
    if (total == 0) return;
    int porcentaje = (recibido * 100U) / total;
    if (porcentaje / 10 != ultimoPorcentajeOta / 10) {   // refrescar cada 10% para no frenar la subida
      ultimoPorcentajeOta = porcentaje;
      Serial.printf("OTA: %d%%\n", porcentaje);
      char detalle[22];
      snprintf(detalle, sizeof(detalle), "%d %%", porcentaje);
      mostrarMensaje("Actualizando OTA", detalle);
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA: listo, reiniciando");
    mostrarMensaje("OTA completa", "Reiniciando...");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA: error %u\n", error);
    char detalle[22];
    snprintf(detalle, sizeof(detalle), "Error %u", static_cast<unsigned int>(error));
    mostrarMensaje("Error OTA", detalle);
  });

  ArduinoOTA.begin();
  otaIniciado = true;
  char ip[16];
  formatearIP(ip, sizeof(ip));
  Serial.printf("OTA listo: %s (%s) puerto %u\n", OTA_NOMBRE, ip, OTA_PUERTO);
}

void atenderWiFiYOTA() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaIniciado) iniciarOTA();
    ArduinoOTA.handle();   // revisa si la PC quiere subir un programa nuevo
  } else {
    if (otaIniciado) {
      ArduinoOTA.end();
      otaIniciado = false;
      Serial.println("WiFi perdido; OTA detenido hasta reconectar.");
    }
    if (millis() - ultimoReintentoWiFiMs >= WIFI_REINTENTO_MS) {
      ultimoReintentoWiFiMs = millis();
      Serial.println("WiFi: reintentando conexion en segundo plano...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
}

// ===========================================================================
// 6. SENSORES Y ACTUADORES
// ===========================================================================

void leerPotenciometro() {
  if (millis() - ultimoPotMs < POT_INTERVALO_MS) return;
  ultimoPotMs = millis();

  uint32_t suma = 0;
  for (int i = 0; i < POT_MUESTRAS; i++) {
    suma += analogRead(PIN_POTENCIOMETRO);   // 0..4095 (ADC de 12 bits)
  }
  pwmLedIntegrado = suma / POT_MUESTRAS;

  if (pwmLedIntegradoIniciado) {
    escribirPWM(PIN_LED_INTEGRADO, CANAL_LED_INTEGRADO, pwmLedIntegrado);
  }
}

uint32_t leerTouchPromedio(int pin, int muestras) {
  uint32_t suma = 0;
  for (int i = 0; i < muestras; i++) {
    suma += touchRead(pin);
  }
  return suma / muestras;
}

uint32_t medirReposoTouch(int pin) {
  uint32_t suma = 0;
  for (int i = 0; i < 20; i++) {
    suma += touchRead(pin);
    delay(10);
  }
  return suma / 20;
}

void calibrarTouch() {
  mostrarMensaje("Calibrando touch", "NO tocar");
  delay(500);  // da tiempo a soltar la placa antes de medir el reposo
  touchReposoSubir = medirReposoTouch(PIN_TOUCH_SUBIR);
  touchReposoBajar = medirReposoTouch(PIN_TOUCH_BAJAR);
  Serial.printf("Touch en reposo: subir=%u bajar=%u\n", (unsigned)touchReposoSubir, (unsigned)touchReposoBajar);
}

void leerTouch() {
  if (millis() - ultimoTouchMs < TOUCH_INTERVALO_MS) return;
  ultimoTouchMs = millis();

  // En la ESP32 el valor de touchRead BAJA cuando se toca el pin
  uint32_t lecturaSubir = leerTouchPromedio(PIN_TOUCH_SUBIR, TOUCH_MUESTRAS);
  uint32_t lecturaBajar = leerTouchPromedio(PIN_TOUCH_BAJAR, TOUCH_MUESTRAS);

  bool tocandoSubir = lecturaSubir < touchReposoSubir * TOUCH_SENSIBILIDAD;
  bool tocandoBajar = lecturaBajar < touchReposoBajar * TOUCH_SENSIBILIDAD;

  if (tocandoSubir && !tocandoBajar) pwmLedExterno += TOUCH_PASO;
  if (tocandoBajar && !tocandoSubir) pwmLedExterno -= TOUCH_PASO;
  pwmLedExterno = constrain(pwmLedExterno, 0, PWM_MAXIMO);

  porcentajeLedExterno = round(pwmLedExterno * 100.0 / PWM_MAXIMO);
  if (pwmLedExternoIniciado) {
    escribirPWM(PIN_LED_EXTERNO, CANAL_LED_EXTERNO, pwmLedExterno);
  }
}

void leerDHT() {
  if (millis() - ultimaLecturaDhtMs < 2000) return;   // el DHT no puede leerse más de 1 vez cada 2 s
  ultimaLecturaDhtMs = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperatura = t;   // si la lectura falla, conservamos la última buena
  if (!isnan(h)) humedad = h;
  if (isnan(t) || isnan(h)) Serial.println("DHT: lectura fallida (revisar pin y TIPO_DHT)");
}

// ===========================================================================
// 7. THINGSPEAK
// ===========================================================================

void enviarAThingSpeak() {
  if (millis() - ultimoEnvioMs < INTERVALO_ENVIO_MS) return;
  ultimoEnvioMs = millis();

  if (WiFi.status() != WL_CONNECTED) {
    snprintf(estadoThingSpeak, sizeof(estadoThingSpeak), "%s", "sin WiFi");
    return;
  }

  // Si aún no hubo lectura válida del DHT, intentamos una lectura fresca ahora mismo
  if (isnan(temperatura) || isnan(humedad)) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) temperatura = t;
    if (!isnan(h)) humedad = h;
    ultimaLecturaDhtMs = millis();
  }

  // Si el sensor sigue sin responder, enviamos 0.0 como respaldo para no bloquear los 6 campos
  float tempEnvio = isnan(temperatura) ? 0.0f : temperatura;
  float humEnvio  = isnan(humedad)     ? 0.0f : humedad;
  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("Aviso: DHT sin lectura valida; enviando 0.0 por defecto en Temp/Hum.");
  }

  int  aleatorio = random(100, 501);          // 100..500 (el 501 no se incluye)
  long segundos = millis() / 1000;            // tiempo desde que arrancó la placa

  ThingSpeak.setField(1, aleatorio);
  ThingSpeak.setField(2, tempEnvio);
  ThingSpeak.setField(3, humEnvio);
  ThingSpeak.setField(4, segundos);
  ThingSpeak.setField(5, pwmLedIntegrado);
  ThingSpeak.setField(6, porcentajeLedExterno);

  int respuesta = ThingSpeak.writeFields(CANAL_ID, WRITE_API_KEY);   // 200 = OK
  ultimoEnvioMs = millis();  // El intervalo se cuenta desde que termina la petición HTTP
  if (respuesta == 200) {
    snprintf(estadoThingSpeak, sizeof(estadoThingSpeak), "%s", "OK");
  } else {
    snprintf(estadoThingSpeak, sizeof(estadoThingSpeak), "err %d", respuesta);
  }

  Serial.printf("ThingSpeak [%d] -> aleat=%d temp=%.1f hum=%.1f t=%lds pwm=%d ext=%d%%\n",
                respuesta, aleatorio, tempEnvio, humEnvio, segundos, pwmLedIntegrado, porcentajeLedExterno);
}

// ===========================================================================
// 8. PANTALLA EN TIEMPO REAL
// ===========================================================================

void actualizarPantalla() {
  if (millis() - ultimaPantallaMs < 250) return;   // 4 veces por segundo alcanza
  ultimaPantallaMs = millis();

  char lineas[4][32];
  const char* vistas[4] = {lineas[0], lineas[1], lineas[2], lineas[3]};
  char temp[10];
  char hum[10];
  if (isnan(temperatura)) snprintf(temp, sizeof(temp), "%s", "--");
  else snprintf(temp, sizeof(temp), "%.1f", temperatura);
  if (isnan(humedad)) snprintf(hum, sizeof(hum), "%s", "--");
  else snprintf(hum, sizeof(hum), "%.0f", humedad);

  unsigned long transcurrido = millis() - ultimoEnvioMs;
  unsigned long restanteMs = transcurrido >= INTERVALO_ENVIO_MS
                                ? 0
                                : INTERVALO_ENVIO_MS - transcurrido;
  unsigned long proximoEnvio = restanteMs / 1000;

  snprintf(lineas[0], sizeof(lineas[0]), "PWM int: %d", pwmLedIntegrado);
  snprintf(lineas[1], sizeof(lineas[1]), "T:%sC H:%s%%", temp, hum);
  snprintf(lineas[2], sizeof(lineas[2]), "LED ext: %d%%", porcentajeLedExterno);
  if (WiFi.status() == WL_CONNECTED) {
    snprintf(lineas[3], sizeof(lineas[3]), "TS:%s %lus", estadoThingSpeak, proximoEnvio);
  } else {
    snprintf(lineas[3], sizeof(lineas[3]), "%s", "Sin WiFi");
  }

  // Si la pantalla tiene menos de 4 renglones (LCD 16x2) se muestran de a grupos que van rotando
  int paginas = (4 + PANTALLA_FILAS - 1) / PANTALLA_FILAS;
  int pagina = (millis() / 2500) % paginas;
  int desde = pagina * PANTALLA_FILAS;
  pantallaMostrar(vistas + desde, min(PANTALLA_FILAS, 4 - desde));
}

// ===========================================================================
// 9. SETUP Y LOOP
// ===========================================================================

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);
  pantallaIniciar();
  mostrarMensaje("TPI IoT 4K3", "Sketch 1: envio");
  delay(1000);

  analogReadResolution(12);
  pwmLedIntegradoIniciado = configurarPWM(PIN_LED_INTEGRADO, CANAL_LED_INTEGRADO);
  pwmLedExternoIniciado = configurarPWM(PIN_LED_EXTERNO, CANAL_LED_EXTERNO);
  if (!pwmLedIntegradoIniciado) Serial.println("Error: no se pudo iniciar PWM del LED integrado.");
  if (!pwmLedExternoIniciado) Serial.println("Error: no se pudo iniciar PWM del LED externo.");
  dht.begin();
  calibrarTouch();

  conectarWiFi(15000);
  ThingSpeak.begin(clienteWiFi);
  leerDHT();                  // primera lectura tras el tiempo de estabilización del DHT
  ultimoEnvioMs = millis();   // el primer envío sale 16 s después de arrancar
}

void loop() {
  atenderWiFiYOTA();
  leerPotenciometro();
  leerTouch();
  leerDHT();
  enviarAThingSpeak();
  actualizarPantalla();
}
