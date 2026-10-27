# 3. Pantalla y sensor DHT

> Requisito: *"Mostrar temperatura y humedad en pantalla en tiempo real."*

## 3.1 La pantalla y el bus I2C

La pantalla se conecta con sólo 2 cables de datos usando **I2C** (se pronuncia "i-cuadrado-c"):

- **SDA** (datos) → GPIO 21 en la ESP32
- **SCL** (reloj) → GPIO 22

Por esos dos cables pueden colgar varios dispositivos; cada uno tiene una **dirección** (un número). Por eso en `prueba_pines` hacemos un *escaneo I2C*: probamos todas las direcciones y vemos quién responde.

| Dirección | Seguramente es… | Librería |
|---|---|---|
| `0x3C` (o `0x3D`) | OLED 128×64 SSD1306 (pantallita negra con letras blancas/azules) | Adafruit SSD1306 |
| `0x27` o `0x3F` | LCD de caracteres con módulo I2C (verde/azul, 16×2 o 20×4) | LiquidCrystal I2C |

### Elegir la pantalla en el sketch

Al principio de los dos sketches:

```cpp
#define TIPO_PANTALLA PANTALLA_OLED          // o PANTALLA_LCD
const uint8_t DIRECCION_PANTALLA = 0x3C;     // la que encontró el escaneo
const int LCD_COLUMNAS = 16;                 // sólo para LCD
const int LCD_FILAS    = 2;
```

El resto del código usa funciones propias (`pantallaMostrar`, `mostrarMensaje`) que funcionan igual con cualquiera de las dos. Así, si mañana resulta que es un LCD, sólo cambiamos esas líneas.

En la OLED mostramos **4 renglones** (cada uno de 16 píxeles de alto). En un LCD de 2 filas no entran los 4, así que el sketch los muestra de a 2 y va rotando cada 2.5 s.

## 3.2 El sensor DHT (temperatura y humedad)

Es un sensor digital que mide las dos cosas. Hay dos modelos:

| | DHT11 (azul) | DHT22 / AM2302 (blanco) |
|---|---|---|
| Temperatura | 0–50 °C, ±2 °C | −40–80 °C, ±0.5 °C |
| Humedad | 20–80 %, ±5 % | 0–100 %, ±2 % |

Ambos usan un solo pin de datos con un protocolo propio que ya resuelve la librería de Adafruit:

```cpp
#include <DHT.h>
DHT dht(PIN_DHT, DHT11);           // pin y modelo

void setup() { dht.begin(); }

float t = dht.readTemperature();   // °C
float h = dht.readHumidity();      // %
```

Dos cosas a tener en cuenta:

1. **Es lento**: no se puede leer más de una vez cada ~2 segundos. Si lo leyéramos en cada `loop()` nos devolvería errores. Por eso usamos el patrón `millis()` para leerlo cada 2 s (función `leerDHT()`).
2. **A veces falla** y devuelve `NaN` (*Not a Number*). Lo chequeamos con `isnan()` y, si falla, nos quedamos con la última lectura buena.

"Tiempo real" acá significa que la pantalla se refresca 4 veces por segundo y el valor del DHT tiene como mucho 2 s de antigüedad — que es lo máximo que permite el sensor.

## 3.3 En el código

- Sección **2** del sketch: funciones de pantalla.
- Función `leerDHT()`: lectura cada 2 s.
- Función `actualizarPantalla()`: arma los 4 renglones:
  ```
  PWM int: 2048
  T:24.5C H:51%
  LED ext: 37%
  TS:OK 12s        ← estado del último envío a ThingSpeak y segundos para el próximo
  ```

➡️ Siguiente: [4. Touch capacitivo](04-touch-capacitivo.md)
