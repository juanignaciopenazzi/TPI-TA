/*
  ===========================================================================
  TPI IoT 4K3 - SKETCH 2: LECTURA DESDE THINGSPEAK
  Grupo 13
  ---------------------------------------------------------------------------
  Integrantes (Apellido, Nombre - Legajo):
    - Vinciguerra, Nicolas - 95645
    - Carle, Lucia         - 94221
    - Castro, Julieta      - 94855
    - Penazzi, Juan Ignacio - 95653
  ===========================================================================

  Este sketch se sube POR WIFI (OTA), eligiendo en el Arduino IDE el puerto de red
  "ESP32-OTA-G13" que dejó habilitado el Sketch 1. Una vez cargado, la ESP32:
    1. Lee los 6 campos del mismo canal de ThingSpeak (cada 20 s).
    2. Cuenta cuántas veces se presionó el pulsador.
    3. Muestra los valores en pantallas que van rotando (3 campos por pantalla en la OLED)
       con la cantidad de pulsaciones siempre visible arriba.

  También mantiene OTA habilitado: así se le puede volver a subir un programa por WiFi.

  Todo lo que hay que adaptar a la placa y a la red está en la sección 1 (CONFIGURACIÓN).
*/

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ThingSpeak.h>
#include <Wire.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===========================================================================
// 1. CONFIGURACIÓN
// ===========================================================================

// --- WiFi ---
const char* WIFI_SSID     = "Grupo13";
const char* WIFI_PASSWORD = "12345678";
const unsigned long WIFI_REINTENTO_MS = 10000;            // reintento cada 10 s si se cae el WiFi

// --- ThingSpeak (el MISMO canal donde escribe el Sketch 1) ---
const unsigned long CANAL_ID = 3556336;                   // Mismo Channel ID del Sketch 1 (Grupo 13)
const char* READ_API_KEY = "";                            // Vacío porque el canal es público (Read Key: DU6B97XMJS3UASUC)
const unsigned long INTERVALO_LECTURA_MS = 20000;
const unsigned long REINTENTO_LECTURA_INICIAL_MS = 5000;  // reintento rápido si falla la primera lectura

// --- OTA ---
const char*    OTA_NOMBRE = "ESP32-OTA-G13";
const uint16_t OTA_PUERTO = 3232;
const char*    OTA_CLAVE  = "";

// --- Pulsador ---
const int  PIN_PULSADOR = 0;                // 0 = botón "BOOT" de la ESP32 (sirve para probar)
const bool PULSADOR_ACTIVO_EN_BAJO = true;  // true si al apretarlo el pin va a GND (lo más común)
const unsigned long ANTIRREBOTE_MS = 200;

// --- Pantalla ---
#define PANTALLA_OLED 1
#define PANTALLA_LCD  2
#define TIPO_PANTALLA PANTALLA_OLED
const uint8_t DIRECCION_PANTALLA = 0x3C;
const int LCD_COLUMNAS = 16;
const int LCD_FILAS    = 2;
const int PIN_SDA = 21;
const int PIN_SCL = 22;
const unsigned long TIEMPO_POR_PANTALLA_MS = 3000;

// ===========================================================================
// 2. PANTALLA  (mismas funciones que en el Sketch 1)
// ===========================================================================

#if TIPO_PANTALLA == PANTALLA_OLED
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  const int PANTALLA_FILAS = 4;
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
// 3. VARIABLES GLOBALES
// ===========================================================================

WiFiClient clienteWiFi;

const int CANTIDAD_CAMPOS = 6;
const char* NOMBRES_CAMPOS[CANTIDAD_CAMPOS] = {"Aleat", "Temp", "Hum", "Tiempo", "PWM int", "LED ext"};
const char* UNIDADES[CANTIDAD_CAMPOS]       = {"",      " C",   " %",  " s",     "",        " %"};
const int   DECIMALES[CANTIDAD_CAMPOS]      = {0,       1,      1,     0,        0,         0};
char valores[CANTIDAD_CAMPOS][16]; // último valor válido mostrado por campo
bool   hayDatos = false;
char estadoLectura[20] = "esperando";

// "volatile" porque las modifica la interrupción del pulsador
volatile unsigned long contadorPulsaciones = 0;
volatile unsigned long ultimaPulsacionMs = 0;
unsigned long pulsacionesInformadas = 0;

bool otaIniciado = false;
int  ultimoPorcentajeOta = -1;
bool primeraLectura = true;
unsigned long ultimaLecturaMs = 0;
unsigned long ultimaPantallaMs = 0;
unsigned long ultimoReintentoWiFiMs = 0;

// ===========================================================================
// 4. PULSADOR (por interrupción)
// ===========================================================================

// Esta función la llama el hardware AUTOMÁTICAMENTE cada vez que el pin cambia,
// aunque el loop esté ocupado esperando a ThingSpeak. Así no se pierde ninguna pulsación.
void IRAM_ATTR alPresionarPulsador() {
  unsigned long ahora = millis();
  if (ahora - ultimaPulsacionMs > ANTIRREBOTE_MS) {   // ignorar los "rebotes" mecánicos del botón
    contadorPulsaciones = contadorPulsaciones + 1;
    ultimaPulsacionMs = ahora;
  }
}

void configurarPulsador() {
  pinMode(PIN_PULSADOR, PULSADOR_ACTIVO_EN_BAJO ? INPUT_PULLUP : INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(PIN_PULSADOR), alPresionarPulsador,
                  PULSADOR_ACTIVO_EN_BAJO ? FALLING : RISING);
}

// ===========================================================================
// 5. WIFI Y OTA  (igual que en el Sketch 1)
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
    Serial.println("\nNo se pudo conectar. Reintento en segundo plano.");
    mostrarMensaje("Sin WiFi", "reintentando...");
    delay(1500);
  }
}

void iniciarOTA() {
  ArduinoOTA.setHostname(OTA_NOMBRE);
  ArduinoOTA.setPort(OTA_PUERTO);
  if (strlen(OTA_CLAVE) > 0) ArduinoOTA.setPassword(OTA_CLAVE);

  ArduinoOTA.onStart([]() {
    ultimoPorcentajeOta = -1;
    Serial.println("OTA: empezando actualizacion");
    mostrarMensaje("Actualizando", "por OTA...");
  });
  ArduinoOTA.onProgress([](unsigned int recibido, unsigned int total) {
    if (total == 0) return;
    int porcentaje = (recibido * 100U) / total;
    if (porcentaje / 10 != ultimoPorcentajeOta / 10) {
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
    ArduinoOTA.handle();
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
// 6. LECTURA DE THINGSPEAK
// ===========================================================================

void leerThingSpeak() {
  unsigned long intervaloActual = hayDatos ? INTERVALO_LECTURA_MS : REINTENTO_LECTURA_INICIAL_MS;
  if (!primeraLectura && millis() - ultimaLecturaMs < intervaloActual) return;
  if (WiFi.status() != WL_CONNECTED) {
    snprintf(estadoLectura, sizeof(estadoLectura), "%s", "sin WiFi");
    return;
  }
  primeraLectura = false;

  // Pide la ÚLTIMA entrada del canal con sus 6 campos de una sola vez
  int estado = (strlen(READ_API_KEY) > 0) ? ThingSpeak.readMultipleFields(CANAL_ID, READ_API_KEY)
                                          : ThingSpeak.readMultipleFields(CANAL_ID);
  ultimaLecturaMs = millis();  // contamos el intervalo desde que finaliza la petición HTTP

  if (estado != 200) {
    snprintf(estadoLectura, sizeof(estadoLectura), "error %d", estado);
    Serial.printf("ThingSpeak: error al leer (%d)\n", estado);
    return;
  }

  Serial.print("ThingSpeak leido:");
  for (int i = 0; i < CANTIDAD_CAMPOS; i++) {
    String crudo = ThingSpeak.getFieldAsString(i + 1);   // los campos van del 1 al 6
    if (crudo.length() == 0) {
      snprintf(valores[i], sizeof(valores[i]), "%s", "--");
    } else {
      float valorNumerico = strtof(crudo.c_str(), nullptr);
      snprintf(valores[i], sizeof(valores[i]), "%.*f", DECIMALES[i], valorNumerico);
    }
    Serial.printf(" %s=%s", NOMBRES_CAMPOS[i], valores[i]);
  }
  Serial.println();
  hayDatos = true;
  snprintf(estadoLectura, sizeof(estadoLectura), "%s", "OK");
}

// ===========================================================================
// 7. PANTALLAS ROTATIVAS
// ===========================================================================

void actualizarPantalla() {
  if (millis() - ultimaPantallaMs < 250) return;
  ultimaPantallaMs = millis();

  noInterrupts();
  unsigned long pulsaciones = contadorPulsaciones;
  interrupts();

  if (!hayDatos) {
    char lineaPulsos[32];
    char lineaEstado[32];
    snprintf(lineaPulsos, sizeof(lineaPulsos), "Pulsos: %lu", pulsaciones);
    snprintf(lineaEstado, sizeof(lineaEstado), "TS: %s", estadoLectura);
    const char* lineas[3] = {lineaPulsos, "Esperando datos", lineaEstado};
    pantallaMostrar(lineas, min(3, PANTALLA_FILAS));
    return;
  }

  // Renglón 1: pulsaciones + número de pantalla. Resto: campos (3 por pantalla en la OLED).
  int camposPorPantalla = max(1, PANTALLA_FILAS - 1);
  int cantidadPantallas = (CANTIDAD_CAMPOS + camposPorPantalla - 1) / camposPorPantalla;
  int pantalla = (millis() / TIEMPO_POR_PANTALLA_MS) % cantidadPantallas;

  char lineas[PANTALLA_FILAS][32];
  const char* vistas[PANTALLA_FILAS];
  snprintf(lineas[0], sizeof(lineas[0]), "Pulsos:%lu %d/%d",
           pulsaciones, pantalla + 1, cantidadPantallas);
  vistas[0] = lineas[0];
  int cantidad = 1;
  for (int i = pantalla * camposPorPantalla;
       i < CANTIDAD_CAMPOS && cantidad < PANTALLA_FILAS; i++) {
    snprintf(lineas[cantidad], sizeof(lineas[cantidad]), "%s: %s%s",
             NOMBRES_CAMPOS[i], valores[i], UNIDADES[i]);
    vistas[cantidad] = lineas[cantidad];
    cantidad++;
  }
  pantallaMostrar(vistas, cantidad);
}

// ===========================================================================
// 8. SETUP Y LOOP
// ===========================================================================

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);
  pantallaIniciar();
  mostrarMensaje("TPI IoT 4K3", "Sketch 2: lectura");
  delay(1500);

  for (int i = 0; i < CANTIDAD_CAMPOS; i++) {
    snprintf(valores[i], sizeof(valores[i]), "%s", "--");
  }
  configurarPulsador();

  conectarWiFi(15000);
  ThingSpeak.begin(clienteWiFi);
  actualizarPantalla();  // muestra la interfaz antes de iniciar la primera consulta HTTP
}

void loop() {
  atenderWiFiYOTA();

  noInterrupts();
  unsigned long pulsaciones = contadorPulsaciones;
  interrupts();
  if (pulsaciones != pulsacionesInformadas) {
    pulsacionesInformadas = pulsaciones;
    Serial.printf("Pulsaciones: %lu\n", pulsacionesInformadas);
  }

  actualizarPantalla();
  leerThingSpeak();
}
