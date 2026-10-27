# 6. OTA — subir programas por WiFi

> Requisito: *"El Sketch 1 debe habilitar OTA para hacer el upload del Sketch 2 mediante el puerto establecido en el Sketch 1."*

## 6.1 ¿Qué es OTA?

**OTA = Over The Air** ("por el aire"). En vez de subir el programa por el cable USB, lo mandamos **por la red WiFi**. Es lo que hacen los celulares o los autos cuando se actualizan solos: imaginate tener 100 sensores en un techo; no vas a subir con una notebook y un cable a cada uno.

## 6.2 ¿Cómo funciona? (la idea)

1. El **Sketch 1** (subido por USB) se conecta al WiFi y se pone a **escuchar en el puerto 3232** — como abrir una "ventanilla" numerada esperando que alguien le hable.
2. Además se **anuncia** en la red con un nombre (`esp32-grupo-XX`) usando un protocolo llamado **mDNS**. Gracias a eso el Arduino IDE la descubre solo y la muestra como un **"puerto de red"**.
3. Cuando en el IDE elegimos ese puerto y subimos el **Sketch 2**, la compu se conecta a esa ventanilla y le manda el programa nuevo.
4. La ESP32 lo guarda en una **segunda zona de memoria**, verifica que llegó bien, y **se reinicia ejecutando el programa nuevo**.

### Las dos zonas de memoria (particiones)

La memoria de programa de la ESP32 está dividida en partes. El esquema *"Default 4MB with spiffs"* tiene **dos lugares para programas** (`app0` y `app1`):

```
┌──────────┬──────────────────┬──────────────────┬────────┐
│ arranque │ app0: Sketch 1 ▶ │ app1: (vacío)    │ datos  │   antes
├──────────┼──────────────────┼──────────────────┼────────┤
│ arranque │ app0: Sketch 1   │ app1: Sketch 2 ▶ │ datos  │   después del OTA
└──────────┴──────────────────┴──────────────────┴────────┘
```

Mientras corre uno, el nuevo se escribe en el otro. Si la transmisión se corta a mitad de camino, la placa sigue con el programa viejo: no queda "brickeada". Por eso **no hay que elegir un esquema de particiones "No OTA"** en *Tools → Partition Scheme*.

## 6.3 El código (Sketch 1)

```cpp
#include <ArduinoOTA.h>

void iniciarOTA() {
  ArduinoOTA.setHostname("esp32-grupo-XX");   // nombre que aparece en el IDE
  ArduinoOTA.setPort(3232);                   // el "puerto establecido"
  ArduinoOTA.begin();                         // empieza a escuchar
}

void loop() {
  ArduinoOTA.handle();   // ¿alguien quiere subirme un programa? → si sí, lo recibe
  // ... resto del programa
}
```

- `iniciarOTA()` se llama **cuando ya hay WiFi** (si no, no tiene en qué red anunciarse).
- `ArduinoOTA.handle()` tiene que llamarse **muy seguido**, en cada vuelta del `loop()`. Por eso es tan importante no usar `delay()` largos: si el loop estuviera bloqueado 16 s, la ESP32 no atendería al IDE.
- `onStart`, `onProgress`, `onEnd`, `onError` son funciones que se ejecutan solas durante la actualización. Las usamos para mostrar el progreso en la pantalla (`Actualizando OTA 40 %`). Están escritas como `[]() { ... }`: es una **lambda**, una función sin nombre escrita en el mismo lugar donde se usa.
- `OTA_CLAVE`: si le ponen texto, el IDE va a pedir esa contraseña antes de subir (evita que cualquiera en la red le suba un programa). Por defecto está vacía para simplificar.

### ¿Por qué el Sketch 2 también tiene OTA?

Cuando subimos el Sketch 2, **reemplaza por completo** al Sketch 1. Si el Sketch 2 no incluyera OTA, la placa dejaría de escuchar en el puerto 3232 y la única forma de volver a programarla sería por cable. Incluyéndolo, podemos ir y venir por WiFi las veces que queramos (por ejemplo, durante la defensa).

## 6.4 Paso a paso: subir el Sketch 2 por OTA

1. Completar WiFi, `CANAL_ID`, etc. en los dos sketches.
2. Subir el **Sketch 1 por USB**. En el Monitor Serie debería aparecer:
   ```
   Conectado. IP: 192.168.43.120
   OTA listo: esp32-grupo-XX (192.168.43.120) puerto 3232
   ```
3. Conectar **la compu a la misma red WiFi** que la ESP32.
4. Abrir el **Sketch 2** en el IDE.
5. **Tools → Port**: debería aparecer una sección **Network ports** con `esp32-grupo-XX at 192.168.43.120`. Elegirlo. (Puede tardar hasta un minuto en aparecer; si no aparece, cerrar y abrir el menú.)
6. Subir (flecha →). Se ve el progreso en el IDE y en la pantalla de la placa. Al terminar, la ESP32 se reinicia con el Sketch 2.

> Para la prueba se puede **desenchufar el USB y alimentar la placa con un cargador**: así queda claro que se subió por WiFi.

## 6.5 Si no aparece el puerto de red

| Problema | Solución |
|---|---|
| La compu y la ESP32 están en redes distintas | Conectar las dos al mismo WiFi |
| WiFi de la facultad / red pública con "aislamiento de clientes" (los dispositivos no se ven entre sí) | **Usar el hotspot de un celular** para las dos |
| Firewall de la compu bloquea la conexión | Permitir el acceso a "espota" / Arduino IDE cuando el sistema pregunte. En Mac: *Ajustes → Red → Firewall* |
| Se cerró el puerto USB pero el IDE no refresca | Cerrar y volver a abrir *Tools → Port* |
| Hotspot de iPhone | Activar "Maximizar compatibilidad" (fuerza 2.4 GHz) |
| Nada de lo anterior | Mirar la IP en el Monitor Serie / pantalla y verificar con `ping <IP>` desde la Terminal |

➡️ Siguiente: [7. Sketch de lectura](07-sketch-lectura.md)
