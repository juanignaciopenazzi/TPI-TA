/*
  ===========================================================================
  TPI IoT 4K3 - SKETCH 2: LECTURA DESDE THINGSPEAK
  Grupo XX
  ===========================================================================

  Este sketch se sube POR WIFI (OTA), eligiendo en el Arduino IDE el puerto de red
  "esp32-grupo-XX" que dejó habilitado el Sketch 1. Una vez cargado, la ESP32:
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

// ===========================================================================
// 1. CONFIGURACIÓN
// ===========================================================================

// --- WiFi ---
const char* WIFI_NOMBRE = "TU_WIFI";
const char* WIFI_CLAVE  = "TU_CLAVE_WIFI";

// --- ThingSpeak (el MISMO canal donde escribe el Sketch 1) ---
const unsigned long CANAL_ID     = 0;
const char*         READ_API_KEY = "";      // "" si el canal es público
const unsigned long INTERVALO_LECTURA_MS = 20000;

// --- OTA ---
const char*    OTA_NOMBRE = "esp32-grupo-XX";
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
    while ((int)texto.length() < LCD_COLUMNAS) texto += ' ';
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
// 3. VARIABLES GLOBALES
// ===========================================================================

WiFiClient clienteWiFi;

const int CANTIDAD_CAMPOS = 6;
const char* NOMBRES_CAMPOS[CANTIDAD_CAMPOS] = {"Aleat", "Temp", "Hum", "Tiempo", "PWM int", "LED ext"};
const char* UNIDADES[CANTIDAD_CAMPOS]       = {"",      " C",   " %",  " s",     "",        " %"};
const int   DECIMALES[CANTIDAD_CAMPOS]      = {0,       1,      1,     0,        0,         0};
String valores[CANTIDAD_CAMPOS];   // lo último que se leyó de cada campo, ya formateado
bool   hayDatos = false;
String estadoLectura = "esperando";

// "volatile" porque las modifica la interrupción del pulsador
volatile unsigned long contadorPulsaciones = 0;
volatile unsigned long ultimaPulsacionMs = 0;
unsigned long pulsacionesInformadas = 0;

bool otaIniciado = false;
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
    Serial.println("OTA: empezando actualizacion");
    mostrarMensaje("Actualizando", "por OTA...");
  });
  ArduinoOTA.onProgress([](unsigned int recibido, unsigned int total) {
    static int ultimoPorcentaje = -1;
    int porcentaje = recibido * 100 / total;
    if (porcentaje / 10 != ultimoPorcentaje / 10) {
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
    ArduinoOTA.handle();
  } else if (millis() - ultimoReintentoWiFiMs > 30000) {
    ultimoReintentoWiFiMs = millis();
    WiFi.reconnect();
  }
}

// ===========================================================================
// 6. LECTURA DE THINGSPEAK
// ===========================================================================

void leerThingSpeak() {
  if (!primeraLectura && millis() - ultimaLecturaMs < INTERVALO_LECTURA_MS) return;
  if (WiFi.status() != WL_CONNECTED) {
    estadoLectura = "sin WiFi";
    return;
  }
  primeraLectura = false;
  ultimaLecturaMs = millis();

  // Pide la ÚLTIMA entrada del canal con sus 6 campos de una sola vez
  int estado = (strlen(READ_API_KEY) > 0) ? ThingSpeak.readMultipleFields(CANAL_ID, READ_API_KEY)
                                          : ThingSpeak.readMultipleFields(CANAL_ID);
  if (estado != 200) {
    estadoLectura = "error " + String(estado);
    Serial.printf("ThingSpeak: error al leer (%d)\n", estado);
    return;
  }

  Serial.print("ThingSpeak leido:");
  for (int i = 0; i < CANTIDAD_CAMPOS; i++) {
    String crudo = ThingSpeak.getFieldAsString(i + 1);   // los campos van del 1 al 6
    valores[i] = (crudo.length() == 0) ? "--" : String(crudo.toFloat(), DECIMALES[i]);
    Serial.printf(" %s=%s", NOMBRES_CAMPOS[i], valores[i].c_str());
  }
  Serial.println();
  hayDatos = true;
  estadoLectura = "OK";
}

// ===========================================================================
// 7. PANTALLAS ROTATIVAS
// ===========================================================================

void actualizarPantalla() {
  if (millis() - ultimaPantallaMs < 250) return;
  ultimaPantallaMs = millis();

  unsigned long pulsaciones = contadorPulsaciones;

  if (!hayDatos) {
    String lineas[3] = {"Pulsos: " + String(pulsaciones), "Esperando datos", "TS: " + estadoLectura};
    pantallaMostrar(lineas, min(3, PANTALLA_FILAS));
    return;
  }

  // Renglón 1: pulsaciones + número de pantalla. Resto: campos (3 por pantalla en la OLED).
  int camposPorPantalla = max(1, PANTALLA_FILAS - 1);
  int cantidadPantallas = (CANTIDAD_CAMPOS + camposPorPantalla - 1) / camposPorPantalla;
  int pantalla = (millis() / TIEMPO_POR_PANTALLA_MS) % cantidadPantallas;

  String lineas[PANTALLA_FILAS];
  lineas[0] = "Pulsos:" + String(pulsaciones) + " " + String(pantalla + 1) + "/" + String(cantidadPantallas);
  int cantidad = 1;
  for (int i = pantalla * camposPorPantalla;
       i < CANTIDAD_CAMPOS && cantidad < PANTALLA_FILAS; i++) {
    lineas[cantidad++] = String(NOMBRES_CAMPOS[i]) + ": " + valores[i] + UNIDADES[i];
  }
  pantallaMostrar(lineas, cantidad);
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

  for (int i = 0; i < CANTIDAD_CAMPOS; i++) valores[i] = "--";
  configurarPulsador();

  conectarWiFi(15000);
  ThingSpeak.begin(clienteWiFi);
}

void loop() {
  atenderWiFiYOTA();
  leerThingSpeak();

  if (contadorPulsaciones != pulsacionesInformadas) {
    pulsacionesInformadas = contadorPulsaciones;
    Serial.printf("Pulsaciones: %lu\n", pulsacionesInformadas);
  }

  actualizarPantalla();
}
