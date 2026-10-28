# 7. Sketch 2 — lectura desde ThingSpeak

> Requisito: *"1. Leer datos del mismo canal de ThingSpeak. 2. Contar las presiones realizadas al pulsador. 3. Mostrar en el display los valores leídos y la cantidad de pulsaciones, de forma legible (puede ser de a 3 campos por pantalla rotativa)."*

## 7.1 Ojo con esto: ¿de dónde salen los datos?

Cuando subimos el Sketch 2 a la ESP32, **el Sketch 1 deja de existir en esa placa** (ver [doc 06](06-ota.md)). O sea: nadie sigue mandando datos nuevos al canal. El Sketch 2 va a mostrar **la última fila que dejó el Sketch 1** antes de ser reemplazado.

Es esperable y es lo que pide el TP: demostrar el "ida y vuelta" ESP32 → nube → ESP32. Si quieren ver datos cambiando, una segunda ESP32 con el Sketch 1 puede seguir escribiendo en el mismo canal.

Para que coincida bien en la defensa: antes de subir el Sketch 2, mover el potenciómetro / touch, esperar a que salga un envío (`TS:OK`), y anotar los valores; después del OTA el Sketch 2 debería mostrar esos mismos valores.

## 7.2 Leer de ThingSpeak

```cpp
int estado = ThingSpeak.readMultipleFields(CANAL_ID);   // canal público: no necesita clave
if (estado == 200) {
  String temperatura = ThingSpeak.getFieldAsString(2);  // campo 2 de la ÚLTIMA fila
}
```

- `readMultipleFields` pide de una sola vez **la última fila completa** del canal (los 6 campos) — una sola consulta HTTP en vez de seis.
- Después, `getFieldAsString(n)` devuelve el campo `n` de esa fila. Si un campo vino vacío, devuelve `""` y mostramos `--`.
- Si el canal fuera privado, se usaría la *Read API Key*: `readMultipleFields(CANAL_ID, READ_API_KEY)`. El sketch elige solo según si `READ_API_KEY` está vacío.
- Leemos **cada 20 s** con el patrón `millis()`: no tiene sentido leer más seguido que lo que se escribe (16 s).

Los números se formatean con la cantidad de decimales que tiene sentido para cada campo (temperatura con 1 decimal, PWM sin decimales, etc.) usando las tablas `NOMBRES_CAMPOS`, `UNIDADES` y `DECIMALES`.

## 7.3 Contar pulsaciones: interrupciones

**El problema**: `readMultipleFields` tarda entre medio segundo y un par de segundos esperando la respuesta de internet. Si revisáramos el botón dentro del `loop()`, una pulsación rápida durante esa espera **se perdería**.

**La solución: una interrupción.** Le pedimos al hardware: *"cada vez que el pin del pulsador cambie, frená lo que estés haciendo un instante y ejecutá esta función"*:

```cpp
attachInterrupt(digitalPinToInterrupt(PIN_PULSADOR), alPresionarPulsador, FALLING);

void IRAM_ATTR alPresionarPulsador() {   // IRAM_ATTR: guardarla en memoria rápida (obligatorio en ESP32)
  contadorPulsaciones = contadorPulsaciones + 1;
}
```

- `FALLING` ("flanco descendente"): se dispara cuando el pin pasa de 1 a 0, o sea, en el momento de apretar un botón conectado a GND con *pull-up*.
- `volatile` en la declaración de la variable avisa al compilador que puede cambiar "por sorpresa" (desde la interrupción), así no la optimiza.
- Una función de interrupción tiene que ser **cortita**: nada de `Serial.print` ni de pantallas. Sólo suma; el `loop()` se encarga de mostrar.

### Pull-up

Un pin de entrada "al aire" lee cualquier cosa (ruido). Con `INPUT_PULLUP` la ESP32 lo conecta internamente a 3.3 V a través de una resistencia: sin apretar lee **1**; al apretar, el botón lo une a GND y lee **0**. Si el pulsador de la placa estuviera cableado al revés (va a 3.3 V al apretar), poner `PULSADOR_ACTIVO_EN_BAJO = false` y el sketch usa `INPUT_PULLDOWN` y `RISING`.

> Los GPIO 34 a 39 no tienen pull-up/pull-down internos. Si el pulsador está en uno de esos, la placa ya debe tener la resistencia soldada.

### Antirrebote (debounce)

Un botón mecánico no hace un contacto limpio: al apretarlo "rebota" varias veces en unos pocos milisegundos:

```
lo que pensamos:   ‾‾‾‾‾‾\______________
lo que pasa:       ‾‾‾‾‾‾\_/‾\_/‾\______     → contaría 3 pulsaciones
```

Por eso ignoramos cualquier flanco que llegue menos de 200 ms después del último que contamos (`ANTIRREBOTE_MS`).

Para probar sin la placa de la cátedra, `PIN_PULSADOR = 0` es el botón **BOOT** que trae toda ESP32 DevKit.

## 7.4 Pantallas rotativas

En la OLED entran 4 renglones. El primero muestra siempre las pulsaciones y en qué pantalla estamos; los otros 3 muestran 3 campos. Cada 3 segundos se pasa a la siguiente:

```
┌────────────────┐        ┌────────────────┐
│Pulsos:7 1/2    │  3 s   │Pulsos:7 2/2    │  3 s
│Aleat: 342      │ ─────▶ │Tiempo: 1234 s  │ ─────▶ (vuelve a 1/2)
│Temp: 24.5 C    │        │PWM int: 2048   │
│Hum: 51.0 %     │        │LED ext: 37 %   │
└────────────────┘        └────────────────┘
```

¿Cómo sabe en qué pantalla está, sin variables extra? Con una cuenta:

```cpp
int pantalla = (millis() / 3000) % 2;   // 0,0,0 (3 s) → 1,1,1 (3 s) → 0 ...
```

`millis() / 3000` cuenta "bloques de 3 segundos" y `% 2` (resto de dividir por 2) lo hace alternar entre 0 y 1. Con un LCD de 2 filas el mismo código muestra 1 campo por pantalla y rota entre 6.

## 7.5 En el código

[sketch_lectura_grupo_13.ino](../sketch_lectura_grupo_13/sketch_lectura_grupo_13.ino): `alPresionarPulsador()`, `leerThingSpeak()`, `actualizarPantalla()`.

➡️ Siguiente: [8. Entrega y defensa](08-entrega-y-defensa.md)
