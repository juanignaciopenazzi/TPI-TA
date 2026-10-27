# 0. Conceptos básicos

Antes de tocar código, el vocabulario mínimo.

## ¿Qué es una ESP32?

Es un **microcontrolador**: una computadora diminuta en un solo chip, con procesador, memoria y patitas (pines) para conectar cosas. A diferencia de una PC, no tiene sistema operativo con ventanas: ejecuta **un solo programa** que le cargamos, una y otra vez, desde que se enciende hasta que se apaga.

Lo especial de la ESP32 es que trae **WiFi** incorporado, por eso es muy usada en IoT (*Internet of Things*, "internet de las cosas": aparatos que mandan o reciben datos por internet).

La "placa de la cátedra" es una plaqueta donde ya está soldada la ESP32 junto con un potenciómetro, LEDs, un sensor de temperatura/humedad, un pulsador y una pantalla. Nosotros sólo tenemos que saber **a qué pin está conectada cada cosa**.

## Pines (GPIO)

Cada patita de la ESP32 tiene un número: GPIO 2, GPIO 34, etc. (*General Purpose Input/Output*). Un pin puede ser:

- **Salida digital**: lo ponemos en 3.3 V (HIGH, 1) o 0 V (LOW, 0). Ej: prender/apagar un LED.
- **Entrada digital**: leemos si hay 3.3 V o 0 V. Ej: saber si un botón está apretado.
- **Entrada analógica**: leemos un voltaje *intermedio* entre 0 y 3.3 V, como un número. Ej: la posición del potenciómetro.
- **Salida PWM**: simula un voltaje intermedio prendiendo y apagando muy rápido. Ej: regular el brillo de un LED.
- **Touch**: algunos pines detectan si los tocamos con el dedo.

## ¿Qué es un sketch?

En el mundo Arduino, un programa se llama **sketch** y es un archivo `.ino`. Está escrito en C++ (parecido a C/Java). Todo sketch tiene dos funciones obligatorias:

```cpp
void setup() {
  // Se ejecuta UNA vez, al encender o resetear la placa.
  // Acá se configuran pines, se conecta el WiFi, etc.
}

void loop() {
  // Se ejecuta UNA Y OTRA VEZ, para siempre, miles de veces por segundo.
  // Acá se leen sensores, se actualizan LEDs y pantalla, etc.
}
```

## Compilar y subir

1. **Compilar** (verificar): el Arduino IDE traduce el C++ a código máquina de la ESP32. Si hay errores de sintaxis, aparecen acá.
2. **Subir** (upload): manda ese código a la memoria de la ESP32. Normalmente por el **cable USB**; en este TP, el segundo sketch se sube **por WiFi** (eso es OTA, ver [doc 06](06-ota.md)).

Al subir un sketch nuevo, **el anterior se borra**. La ESP32 tiene un solo programa a la vez.

## El Monitor Serie

Es una ventanita del Arduino IDE donde vemos los mensajes que la ESP32 manda por el USB con `Serial.println("hola")`. Es nuestra principal herramienta para ver qué está pasando adentro. Tiene que estar configurado a la misma velocidad que el sketch: **115200 baudios**.

## `delay()` vs `millis()` — idea clave del TP

- `delay(16000)` **congela** el programa 16 segundos. Durante ese tiempo no se lee el potenciómetro, no se actualiza la pantalla, no responden los touch. Malo.
- `millis()` devuelve cuántos milisegundos pasaron desde que arrancó la placa. Lo usamos como un reloj: *"¿ya pasaron 16 s desde el último envío? Si sí, envío; si no, sigo con lo demás"*.

```cpp
unsigned long ultimoEnvio = 0;

void loop() {
  if (millis() - ultimoEnvio >= 16000) {   // ¿pasaron 16 s?
    ultimoEnvio = millis();
    enviarDatos();
  }
  leerPotenciometro();   // esto se sigue ejecutando todo el tiempo
}
```

Este patrón aparece en todos lados en nuestros sketches: así varias tareas "conviven" sin bloquearse.

## Librerías

Son código ya escrito por otros que reutilizamos: una para la pantalla, otra para el sensor DHT, otra para ThingSpeak. Se instalan una vez en el IDE y se incluyen con `#include <Nombre.h>`.

➡️ Siguiente: [1. Preparar el entorno](01-preparar-entorno.md)
