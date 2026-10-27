/*
  ===========================================================================
  TPI IoT 4K3 - SKETCH 1: SENSADO + ENVÍO A THINGSPEAK + OTA
  Grupo XX
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

// ===========================================================================
// 1. CONFIGURACIÓN  (lo único que deberían tener que tocar)
// ===========================================================================

// --- WiFi (la ESP32 sólo se conecta a redes de 2.4 GHz) ---
const char* WIFI_NOMBRE = "TU_WIFI";
const char* WIFI_CLAVE  = "TU_CLAVE_WIFI";

// --- ThingSpeak (sale de la pestaña "API Keys" del canal) ---
const unsigned long CANAL_ID      = 0;                    // Channel ID
const char*         WRITE_API_KEY = "TU_WRITE_API_KEY";   // Write API Key
const unsigned long INTERVALO_ENVIO_MS = 16000;           // 16 s (ThingSpeak gratis pide >= 15 s)

// --- OTA (subida de programas por WiFi) ---
const char*    OTA_NOMBRE = "esp32-grupo-XX";  // así aparece el "puerto de red" en el Arduino IDE
const uint16_t OTA_PUERTO = 3232;              // puerto establecido para OTA (3232 es el estándar)
const char*    OTA_CLAVE  = "";                // "" = sin contraseña

// --- Pines de la placa ---
const int PIN_POTENCIOMETRO = 34;  // DEBE ser ADC1 (GPIO 32 a 39): el ADC2 no funciona con el WiFi prendido
const int PIN_LED_INTEGRADO = 2;   // LED azul que trae la ESP32 DevKit
const int PIN_LED_EXTERNO   = 18;
const int PIN_TOUCH_SUBIR   = 32;  // pin táctil T9
const int PIN_TOUCH_BAJAR   = 33;  // pin táctil T8
const int PIN_DHT           = 15;
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

// --- PWM ---
const int PWM_FRECUENCIA = 5000;   // Hz
const int PWM_RESOLUCION = 12;     // bits -> valores de 0 a 4095
const int PWM_MAXIMO     = 4095;   // 2^12 - 1

// --- Touch ---
const float TOUCH_SENSIBILIDAD = 0.7;     // "tocado" si la lectura cae por debajo del 70% del reposo
const int   TOUCH_PASO = 41;              // cuánto sube/baja el PWM por paso (~100 pasos de 0 a 4095)
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

void pantallaIniciar() {
#if TIPO_PANTALLA == PANTALLA_OLED
  if (!oled.begin(SSD1306_SWITCHCAPVCC, DIRECCION_PANTALLA)) {
    Serial.println("No se encontro la pantalla OLED (revisar direccion y pines I2C)");
  }
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
#else
  lcd.init();
  lcd.backlight();
#endif
}

// Muestra hasta PANTALLA_FILAS renglones de texto.
void pantallaMostrar(const String lineas[], int cantidad) {
#if TIPO_PANTALLA == PANTALLA_OLED
  oled.clearDisplay();
  for (int i = 0; i < cantidad && i < PANTALLA_FILAS; i++) {
    oled.setCursor(0, i * 16);
    oled.print(lineas[i]);
  }
  oled.display();
#else
  for (int i = 0; i < PANTALLA_FILAS; i++) {
    String texto = (i < cantidad) ? lineas[i] : "";
    while ((int)texto.length() < LCD_COLUMNAS) texto += ' ';  // rellenar con espacios en vez de borrar: no parpadea
    lcd.setCursor(0, i);
    lcd.print(texto.substring(0, LCD_COLUMNAS));
  }
#endif
}

void mostrarMensaje(const String& linea1, const String& linea2) {
  String lineas[2] = {linea1, linea2};
  pantallaMostrar(lineas, 2);
}

// ===========================================================================
// 3. PWM  (funciona con el core ESP32 versión 2.x y 3.x)
// ===========================================================================

void configurarPWM(int pin, int canal) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pin, PWM_FRECUENCIA, PWM_RESOLUCION);
#else
  ledcSetup(canal, PWM_FRECUENCIA, PWM_RESOLUCION);
  ledcAttachPin(pin, canal);
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
String estadoThingSpeak = "esperando";

unsigned long ultimoEnvioMs = 0;
unsigned long ultimaLecturaDhtMs = 0;
unsigned long ultimoTouchMs = 0;
unsigned long ultimaPantallaMs = 0;
unsigned long ultimoReintentoWiFiMs = 0;

// ===========================================================================
// 5. WIFI Y OTA
// ===========================================================================

void conectarWiFi(unsigned long esperaMaximaMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_NOMBRE, WIFI_CLAVE);
  Serial.print("Conectando a WiFi");
  mostrarMensaje("Conectando WiFi", WIFI_NOMBRE);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < esperaMaximaMs) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\nConectado. IP: ");
    Serial.println(WiFi.localIP());
    mostrarMensaje("WiFi OK", WiFi.localIP().toString());
    delay(2000);
  } else {
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
    Serial.println("OTA: empezando actualizacion");
    mostrarMensaje("Actualizando", "por OTA...");
  });
  ArduinoOTA.onProgress([](unsigned int recibido, unsigned int total) {
    static int ultimoPorcentaje = -1;
    int porcentaje = recibido * 100 / total;
    if (porcentaje / 10 != ultimoPorcentaje / 10) {   // refrescar cada 10% para no frenar la subida
      ultimoPorcentaje = porcentaje;
      Serial.printf("OTA: %d%%\n", porcentaje);
      mostrarMensaje("Actualizando OTA", String(porcentaje) + " %");
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA: listo, reiniciando");
    mostrarMensaje("OTA completa", "Reiniciando...");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA: error %u\n", error);
    mostrarMensaje("Error OTA", String(error));
  });

  ArduinoOTA.begin();
  otaIniciado = true;
  Serial.printf("OTA listo: %s (%s) puerto %u\n", OTA_NOMBRE, WiFi.localIP().toString().c_str(), OTA_PUERTO);
}

void atenderWiFiYOTA() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaIniciado) iniciarOTA();
    ArduinoOTA.handle();   // revisa si la PC quiere subir un programa nuevo
  } else if (millis() - ultimoReintentoWiFiMs > 30000) {
    ultimoReintentoWiFiMs = millis();
    WiFi.reconnect();
  }
}

// ===========================================================================
// 6. SENSORES Y ACTUADORES
// ===========================================================================

void leerPotenciometro() {
  pwmLedIntegrado = analogRead(PIN_POTENCIOMETRO);   // 0..4095 (ADC de 12 bits)
  escribirPWM(PIN_LED_INTEGRADO, CANAL_LED_INTEGRADO, pwmLedIntegrado);
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
  touchReposoSubir = medirReposoTouch(PIN_TOUCH_SUBIR);
  touchReposoBajar = medirReposoTouch(PIN_TOUCH_BAJAR);
  Serial.printf("Touch en reposo: subir=%u bajar=%u\n", (unsigned)touchReposoSubir, (unsigned)touchReposoBajar);
}

void leerTouch() {
  if (millis() - ultimoTouchMs < TOUCH_INTERVALO_MS) return;
  ultimoTouchMs = millis();

  // En la ESP32 el valor de touchRead BAJA cuando se toca el pin
  bool tocandoSubir = touchRead(PIN_TOUCH_SUBIR) < touchReposoSubir * TOUCH_SENSIBILIDAD;
  bool tocandoBajar = touchRead(PIN_TOUCH_BAJAR) < touchReposoBajar * TOUCH_SENSIBILIDAD;

  if (tocandoSubir && !tocandoBajar) pwmLedExterno += TOUCH_PASO;
  if (tocandoBajar && !tocandoSubir) pwmLedExterno -= TOUCH_PASO;
  pwmLedExterno = constrain(pwmLedExterno, 0, PWM_MAXIMO);

  porcentajeLedExterno = round(pwmLedExterno * 100.0 / PWM_MAXIMO);
  escribirPWM(PIN_LED_EXTERNO, CANAL_LED_EXTERNO, pwmLedExterno);
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
    estadoThingSpeak = "sin WiFi";
    return;
  }

  int  aleatorio = random(100, 501);          // 100..500 (el 501 no se incluye)
  long segundos = millis() / 1000;            // tiempo desde que arrancó la placa

  ThingSpeak.setField(1, aleatorio);
  if (!isnan(temperatura)) ThingSpeak.setField(2, temperatura);
  if (!isnan(humedad))     ThingSpeak.setField(3, humedad);
  ThingSpeak.setField(4, segundos);
  ThingSpeak.setField(5, pwmLedIntegrado);
  ThingSpeak.setField(6, porcentajeLedExterno);

  int respuesta = ThingSpeak.writeFields(CANAL_ID, WRITE_API_KEY);   // 200 = OK
  estadoThingSpeak = (respuesta == 200) ? "OK" : "err " + String(respuesta);

  Serial.printf("ThingSpeak [%d] -> aleat=%d temp=%.1f hum=%.1f t=%lds pwm=%d ext=%d%%\n",
                respuesta, aleatorio, temperatura, humedad, segundos, pwmLedIntegrado, porcentajeLedExterno);
}

// ===========================================================================
// 8. PANTALLA EN TIEMPO REAL
// ===========================================================================

void actualizarPantalla() {
  if (millis() - ultimaPantallaMs < 250) return;   // 4 veces por segundo alcanza
  ultimaPantallaMs = millis();

  String temp = isnan(temperatura) ? "--" : String(temperatura, 1);
  String hum  = isnan(humedad) ? "--" : String(humedad, 0);
  long proximoEnvio = (long)(INTERVALO_ENVIO_MS - (millis() - ultimoEnvioMs)) / 1000;

  String lineas[4] = {
    "PWM int: " + String(pwmLedIntegrado),
    "T:" + temp + "C H:" + hum + "%",
    "LED ext: " + String(porcentajeLedExterno) + "%",
    WiFi.status() == WL_CONNECTED ? "TS:" + estadoThingSpeak + " " + String(proximoEnvio) + "s"
                                  : "Sin WiFi"
  };

  // Si la pantalla tiene menos de 4 renglones (LCD 16x2) se muestran de a grupos que van rotando
  int paginas = (4 + PANTALLA_FILAS - 1) / PANTALLA_FILAS;
  int pagina = (millis() / 2500) % paginas;
  int desde = pagina * PANTALLA_FILAS;
  pantallaMostrar(lineas + desde, min(PANTALLA_FILAS, 4 - desde));
}

// ===========================================================================
// 9. SETUP Y LOOP
// ===========================================================================

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);
  pantallaIniciar();
  mostrarMensaje("TPI IoT 4K3", "Sketch 1: envio");

  analogReadResolution(12);
  configurarPWM(PIN_LED_INTEGRADO, CANAL_LED_INTEGRADO);
  configurarPWM(PIN_LED_EXTERNO, CANAL_LED_EXTERNO);
  dht.begin();
  calibrarTouch();

  conectarWiFi(15000);
  ThingSpeak.begin(clienteWiFi);
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
