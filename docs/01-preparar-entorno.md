# 1. Preparar el entorno

## 1.1 Instalar Arduino IDE

Descargar **Arduino IDE 2.x** desde <https://www.arduino.cc/en/software> e instalarlo.

## 1.2 Agregar soporte para ESP32

El IDE viene preparado para placas Arduino, no para la ESP32. Hay que agregarla:

1. **Arduino IDE → Settings / Preferencias** (en Mac: `Arduino IDE → Settings…`).
2. En **"Additional boards manager URLs"** pegar:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. Ir a **Tools → Board → Boards Manager**, buscar **esp32** e instalar **"esp32 by Espressif Systems"** (la versión 3.x; los sketches también funcionan con la 2.x).

## 1.3 Instalar las librerías

**Tools → Manage Libraries…** (o el ícono de libros a la izquierda) y buscar/instalar:

| Librería | Autor | Para qué |
|---|---|---|
| DHT sensor library | Adafruit | sensor de temperatura y humedad (si pregunta, instalar también "Adafruit Unified Sensor") |
| Adafruit SSD1306 | Adafruit | pantalla OLED (si pregunta, instalar también "Adafruit GFX Library") |
| LiquidCrystal I2C | Frank de Brabander | sólo si la pantalla resulta ser un LCD |
| ThingSpeak | MathWorks | mandar y leer datos de ThingSpeak |

`WiFi`, `ArduinoOTA` y `Wire` ya vienen con la ESP32: no hay que instalarlas.

## 1.4 Conectar la placa

1. Enchufar la ESP32 con un cable USB **de datos** (algunos cables sólo cargan y no sirven).
2. **Tools → Board → esp32 → "ESP32 Dev Module"**.
3. **Tools → Port** → elegir el puerto que aparece al enchufarla (en Mac algo como `/dev/cu.usbserial-0001` o `/dev/cu.SLAB_USBtoUART`; en Windows `COM3`, `COM4`…).
   - Si no aparece ningún puerto: falta el driver del chip USB. Según la placa es **CP210x** (Silicon Labs) o **CH340** (WCH). Instalarlo y reenchufar.
4. **Tools → Partition Scheme**: dejar **"Default 4MB with spiffs"** (es importante para OTA, ver [doc 06](06-ota.md)).

## 1.5 Primera prueba: Blink

`File → Examples → 01.Basics → Blink`, cambiar `LED_BUILTIN` por `2` si da error, y subir con la flecha →. El LED azul de la ESP32 debería parpadear.

**Si se queda en `Connecting........_____`**: mantener apretado el botón **BOOT** de la ESP32 hasta que empiece a subir y soltarlo.

**Si en una Mac con chip Apple (M1/M2/M3…) aparece `ctags: bad CPU type in executable`**: una herramienta interna del IDE todavía es para procesadores Intel. Se arregla instalando Rosetta (una sola vez) desde la Terminal:

```bash
softwareupdate --install-rosetta --agree-to-license
```

## 1.6 Descubrir los pines de la placa

No tenemos el esquema de la placa de la cátedra, así que lo averiguamos con [prueba_pines.ino](../pruebas/prueba_pines/prueba_pines.ino):

1. Abrir y subir `pruebas/prueba_pines/prueba_pines.ino`.
2. Abrir el **Monitor Serie** (lupa arriba a la derecha), poner **115200 baud** y **"New Line"**.
3. Escribir cada opción + Enter y anotar:

| Escribir | Qué hacer | Qué anotar |
|---|---|---|
| `i` | nada | dirección de la pantalla: `0x3C` → OLED, `0x27`/`0x3F` → LCD |
| `a` | girar el potenciómetro | el GPIO cuyo número va de ~0 a ~4095 → `PIN_POTENCIOMETRO` |
| `t` | tocar cada pin táctil | los GPIO cuyo número BAJA al tocar → `PIN_TOUCH_SUBIR` / `PIN_TOUCH_BAJAR` |
| `p` | apretar el pulsador | el GPIO que aparece y si va a 0 al apretar → `PIN_PULSADOR`, `PULSADOR_ACTIVO_EN_BAJO` |
| `d` | esperar | pin y tipo del DHT con valores razonables → `PIN_DHT`, `TIPO_DHT` |
| `18` (u otro número) | mirar los LEDs | si parpadea el LED externo, ese es `PIN_LED_EXTERNO` |

Con eso completamos la sección **CONFIGURACIÓN** de los dos sketches. Si en la placa hay serigrafía (texto impreso) con los pines, mejor todavía: confirmarlos igual con esta prueba.

➡️ Siguiente: [2. ADC y PWM de 12 bits](02-adc-pwm-12bits.md)
