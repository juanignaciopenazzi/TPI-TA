# 4. Touch capacitivo — LED externo

> Requisito: *"Ajustar el brillo del LED EXTERNO usando dos pines touch capacitivos como entrada. Un pin aumenta la señal y otro la disminuye. Mostrar el porcentaje de brillo en pantalla. Trabajar con 12 bits."*

## 4.1 ¿Cómo detecta la ESP32 que la tocamos?

Algunos pines de la ESP32 pueden medir **capacitancia**: cuánta carga eléctrica "acumula" lo que esté conectado al pin. Nuestro cuerpo es conductor; cuando tocamos el pin (o un cable conectado a él) la capacitancia **aumenta**.

La función `touchRead(pin)` devuelve un número que, en la ESP32 clásica, **baja cuando tocamos**:

```
sin tocar:  ~60   ████████████
tocando:    ~15   ███
```

(Los números exactos dependen de la placa, el largo del cable, la humedad… por eso no conviene usar un número fijo.)

Pines con touch en la ESP32: GPIO 4 (T0), 2 (T2), 15 (T3), 13 (T4), 12 (T5), 14 (T6), 27 (T7), 33 (T8), 32 (T9).

## 4.2 Calibración automática

En vez de inventar un umbral, al arrancar medimos el valor **en reposo** (sin tocar) de cada pin, promediando 20 lecturas. Después consideramos "tocado" cuando la lectura baja del 70 % de ese reposo:

```cpp
touchReposoSubir = medirReposoTouch(PIN_TOUCH_SUBIR);   // ej: 62

bool tocandoSubir = touchRead(PIN_TOUCH_SUBIR) < touchReposoSubir * 0.7;   // < 43 → tocado
```

⚠️ Por eso **no hay que tocar los pines mientras arranca la placa** (la pantalla dice *"Calibrando touch / NO tocar"*). Si se calibró mal, apretar el botón EN/RST de la ESP32 para reiniciar.

Si detecta toques sin tocar, o no detecta nada: ajustar `TOUCH_SENSIBILIDAD` (más cerca de 1 = más sensible, más cerca de 0 = menos sensible).

## 4.3 Subir y bajar el brillo

Cada 20 ms miramos los dos pines:

```cpp
if (tocandoSubir && !tocandoBajar) pwmLedExterno += TOUCH_PASO;   // +41
if (tocandoBajar && !tocandoSubir) pwmLedExterno -= TOUCH_PASO;   // -41
pwmLedExterno = constrain(pwmLedExterno, 0, 4095);                // no pasarse de los límites
```

Con paso 41, ir de 0 a 4095 lleva unos 100 pasos × 20 ms = **2 segundos** de dedo apoyado. Se puede cambiar `TOUCH_PASO` para que sea más rápido o más fino.

## 4.4 El porcentaje

El PWM trabaja en 12 bits (0–4095), pero en pantalla pide **porcentaje**: regla de tres.

```
porcentaje = pwm × 100 / 4095

    0  →   0 %
 2048  →  50 %
 4095  → 100 %
```

```cpp
porcentajeLedExterno = round(pwmLedExterno * 100.0 / PWM_MAXIMO);
```

(El `100.0` con punto hace que la cuenta sea con decimales; si no, C++ haría división entera.)

## 4.5 En el código

Funciones `calibrarTouch()` y `leerTouch()` en [sketch_envio_grupo_13.ino](../sketch_envio_grupo_13/sketch_envio_grupo_13.ino). En pantalla: `LED ext: 37%`.

➡️ Siguiente: [5. WiFi y ThingSpeak](05-wifi-thingspeak.md)
