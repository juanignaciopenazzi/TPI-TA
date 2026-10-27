/*
  ===========================================================================
  PRUEBA DE PINES - para descubrir cómo está conectada la placa de la cátedra
  ===========================================================================

  Subilo por USB, abrí el Monitor Serie (115200 baudios, "Nueva línea") y escribí
  una de estas opciones + Enter:

    i   -> busca dispositivos I2C (la pantalla). 0x3C = OLED, 0x27 / 0x3F = LCD
    a   -> muestra los pines analógicos (ADC1). Mové el potenciómetro y mirá cuál cambia
    t   -> muestra los pines táctiles. Tocá cada pin y mirá cuál BAJA
    p   -> avisa cuando cambia algún pin de entrada. Apretá el pulsador y mirá cuál aparece
    d   -> busca el sensor DHT probando varios pines (tarda unos segundos)
    18  -> (cualquier número) hace parpadear ese GPIO. Sirve para encontrar el LED externo
    s   -> detener la lectura continua

  Anotá lo que encuentres y copialo en la sección CONFIGURACIÓN de los dos sketches.
*/

#include <Wire.h>
#include <DHT.h>

const int PIN_SDA = 21;
const int PIN_SCL = 22;

const int PINES_ANALOGICOS[]  = {32, 33, 34, 35, 36, 39};             // ADC1 (funcionan con WiFi)
const int PINES_TOUCH[]       = {4, 2, 15, 13, 12, 14, 27, 33, 32};    // T0, T2..T9
const int PINES_DHT[]         = {4, 5, 13, 14, 15, 16, 17, 18, 19, 23, 25, 26, 27};
const int PINES_PULSADOR[]    = {0, 4, 5, 13, 14, 15, 16, 17, 18, 19, 23, 25, 26, 27, 34, 35, 36, 39};

#define CANTIDAD(arreglo) (sizeof(arreglo) / sizeof(arreglo[0]))

char modo = 's';
int estadoAnterior[CANTIDAD(PINES_PULSADOR)];

void escanearI2C() {
  Wire.begin(PIN_SDA, PIN_SCL);
  Serial.println("Buscando dispositivos I2C...");
  int encontrados = 0;
  for (uint8_t direccion = 1; direccion < 127; direccion++) {
    Wire.beginTransmission(direccion);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Encontrado en 0x%02X\n", direccion);
      encontrados++;
    }
  }
  if (encontrados == 0) Serial.println("  Nada. Probá con otros pines SDA/SCL.");
}

void buscarDHT() {
  Serial.println("Buscando DHT (DHT11 y DHT22) en varios pines...");
  for (unsigned int i = 0; i < CANTIDAD(PINES_DHT); i++) {
    int tipos[2] = {DHT11, DHT22};
    for (int tipo : tipos) {
      DHT dht(PINES_DHT[i], tipo);
      dht.begin();
      delay(100);
      float t = dht.readTemperature(false, true);
      float h = dht.readHumidity();
      if (!isnan(t) && !isnan(h)) {
        Serial.printf("  GPIO %d como %s: %.1f C  %.1f %%\n", PINES_DHT[i], tipo == DHT11 ? "DHT11" : "DHT22", t, h);
      }
    }
  }
  Serial.println("Listo. Elegí el pin/tipo cuyos valores sean razonables (ej: 24 C, 50 %).");
}

void hacerParpadear(int pin) {
  Serial.printf("Parpadeando GPIO %d...\n", pin);
  pinMode(pin, OUTPUT);
  for (int i = 0; i < 6; i++) {
    digitalWrite(pin, HIGH);
    delay(300);
    digitalWrite(pin, LOW);
    delay(300);
  }
  pinMode(pin, INPUT);
}

void prepararPulsador() {
  for (unsigned int i = 0; i < CANTIDAD(PINES_PULSADOR); i++) {
    int pin = PINES_PULSADOR[i];
    pinMode(pin, pin >= 34 ? INPUT : INPUT_PULLUP);   // 34..39 no tienen pull-up interno
    estadoAnterior[i] = digitalRead(pin);
  }
  Serial.println("Apretá el pulsador...");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nPrueba de pines. Opciones: i, a, t, p, d, <numero de GPIO>, s");
}

void loop() {
  if (Serial.available()) {
    String entrada = Serial.readStringUntil('\n');
    entrada.trim();
    if (entrada.length() > 0) {
      if (isDigit(entrada[0])) {
        hacerParpadear(entrada.toInt());
      } else {
        modo = entrada[0];
        if (modo == 'i') { escanearI2C(); modo = 's'; }
        if (modo == 'd') { buscarDHT(); modo = 's'; }
        if (modo == 'p') prepararPulsador();
      }
    }
  }

  if (modo == 'a') {
    for (unsigned int i = 0; i < CANTIDAD(PINES_ANALOGICOS); i++) {
      Serial.printf("G%d=%4d  ", PINES_ANALOGICOS[i], analogRead(PINES_ANALOGICOS[i]));
    }
    Serial.println();
    delay(300);
  } else if (modo == 't') {
    for (unsigned int i = 0; i < CANTIDAD(PINES_TOUCH); i++) {
      Serial.printf("G%d=%3u  ", PINES_TOUCH[i], (unsigned int)touchRead(PINES_TOUCH[i]));
    }
    Serial.println();
    delay(300);
  } else if (modo == 'p') {
    for (unsigned int i = 0; i < CANTIDAD(PINES_PULSADOR); i++) {
      int estado = digitalRead(PINES_PULSADOR[i]);
      if (estado != estadoAnterior[i]) {
        Serial.printf("GPIO %d cambio a %d\n", PINES_PULSADOR[i], estado);
        estadoAnterior[i] = estado;
      }
    }
    delay(10);
  }
}
