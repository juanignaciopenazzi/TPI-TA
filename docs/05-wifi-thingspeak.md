# 5. WiFi y ThingSpeak — mandar datos a la nube

> Requisito: *"Enviar cada 16 segundos a un canal de ThingSpeak: 1) aleatorio entre 100 y 500, 2) temperatura, 3) humedad, 4) segundos desde el inicio, 5) PWM del LED integrado, 6) % de brillo del LED externo."*

## 5.1 ¿Qué es ThingSpeak?

Es un servicio web gratuito (de MathWorks, los de MATLAB) que funciona como una **planilla en la nube** para dispositivos IoT:

- Un **canal** es como una planilla. Tiene hasta **8 columnas** llamadas *fields* (campos).
- Cada vez que la ESP32 manda datos, se agrega una **fila nueva** con la fecha/hora.
- La página del canal dibuja automáticamente un gráfico por campo.

Cualquier dispositivo con internet puede **escribir** en el canal (con la *Write API Key*) o **leer** de él (si es público, sin clave).

## 5.2 Crear la cuenta y el canal (paso a paso)

1. Ir a <https://thingspeak.mathworks.com> → **Sign Up**. Crear una cuenta MathWorks (con el mail de la facultad o personal).
2. **Channels → My Channels → New Channel**.
3. Completar:
   - **Name**: `TPI IoT 4K3 - Grupo XX`
   - Tildar **Field 1 a Field 6** y ponerles nombre:

     | Field | Nombre |
     |---|---|
     | 1 | Aleatorio |
     | 2 | Temperatura |
     | 3 | Humedad |
     | 4 | Tiempo (s) |
     | 5 | PWM LED integrado |
     | 6 | Brillo LED externo (%) |
4. **Save Channel**.
5. Pestaña **Sharing** → elegir **"Share channel view with everyone"** (el enunciado pide canal **público**).
6. Pestaña **API Keys** → anotar:
   - **Channel ID** (número, ej. `2712345`) → va en `CANAL_ID` de los dos sketches.
   - **Write API Key** (ej. `A1B2C3D4E5F6G7H8`) → va en `WRITE_API_KEY` del Sketch 1.

### Probar el canal sin la ESP32

Pegar en el navegador (reemplazando la clave):

```
https://api.thingspeak.com/update?api_key=TU_WRITE_API_KEY&field1=123
```

Si responde un número (`1`, `2`…) es el número de fila que se agregó: el canal anda. Si responde `0`, la clave está mal o mandaron dos veces en menos de 15 s.

## 5.3 Conectarse al WiFi

```cpp
WiFi.mode(WIFI_STA);                     // modo "estación": cliente de un router, como un celular
WiFi.begin(WIFI_NOMBRE, WIFI_CLAVE);
while (WiFi.status() != WL_CONNECTED) delay(500);
```

Detalles prácticos:

- La ESP32 sólo ve redes de **2.4 GHz** (no 5 GHz).
- El WiFi de la facultad suele tener portal de login o bloquear dispositivos. **Lo más seguro es usar el hotspot de un celular** (y conectar la compu al mismo hotspot, ver [doc 06](06-ota.md)).
- En nuestro sketch, si en 15 s no conecta, sigue funcionando sin WiFi (LEDs, pantalla, sensores) y reintenta cada 30 s. Apenas conecta, habilita OTA y empieza a enviar.

## 5.4 Enviar los 6 valores

La librería ThingSpeak hace todo el trabajo HTTP. Se cargan los campos y se manda todo junto:

```cpp
ThingSpeak.begin(clienteWiFi);                    // una vez, en setup()

ThingSpeak.setField(1, random(100, 501));         // 100..500  (el límite superior NO se incluye)
ThingSpeak.setField(2, temperatura);
ThingSpeak.setField(3, humedad);
ThingSpeak.setField(4, (long)(millis() / 1000));  // milisegundos → segundos
ThingSpeak.setField(5, pwmLedIntegrado);          // 0..4095
ThingSpeak.setField(6, porcentajeLedExterno);     // 0..100
int respuesta = ThingSpeak.writeFields(CANAL_ID, WRITE_API_KEY);
```

- `random(100, 501)`: en la ESP32 usa el generador de números aleatorios **por hardware** (basado en ruido de radio), así que no hace falta `randomSeed()`.
- `millis() / 1000`: `millis()` cuenta milisegundos desde que arrancó la placa; dividido 1000 da segundos.

## 5.5 ¿Por qué cada 16 s y no 15 o 10?

La cuenta gratuita de ThingSpeak acepta **como mucho un dato cada 15 segundos** por canal. Si mandamos antes, lo rechaza. 16 s deja un segundo de margen.

El temporizador usa el patrón `millis()` (ver [doc 00](00-conceptos-basicos.md)), **no** `delay(16000)`: mientras esperamos, el potenciómetro, los touch y la pantalla siguen funcionando.

## 5.6 Códigos de respuesta

`writeFields` devuelve un número. Lo mostramos en pantalla (`TS:OK` o `TS:err -301`) y en el Monitor Serie:

| Código | Significa | Qué revisar |
|---|---|---|
| `200` | OK | 🎉 |
| `-301` | No se pudo conectar al servidor | WiFi / internet |
| `-304` | Timeout | Internet lento, reintenta solo |
| `-401` | Rechazado por mandar muy seguido | Que no haya otra placa escribiendo en el mismo canal |
| `400` / `404` | API Key incorrecta | `WRITE_API_KEY` y `CANAL_ID` |

## 5.7 En el código

Funciones `conectarWiFi()` y `enviarAThingSpeak()` en [sketch_envio_grupo_XX.ino](../sketch_envio_grupo_XX/sketch_envio_grupo_XX.ino).

➡️ Siguiente: [6. OTA](06-ota.md)
