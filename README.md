# TPI IoT 4K3 — ESP32 + ThingSpeak + OTA

Resolución del Trabajo Práctico Integrador de **Tecnologías para la Automatización** ([enunciado](TPI%204K3.pdf)).

## ¿Qué hay que hacer? (en criollo)

Tenemos una ESP32 (un microcontrolador con WiFi) montada en la placa de la cátedra. Hay que hacer **dos programas**:

| | Sketch 1 — ENVÍO | Sketch 2 — LECTURA |
|---|---|---|
| Archivo | [sketch_envio_grupo_XX.ino](sketch_envio_grupo_XX/sketch_envio_grupo_XX.ino) | [sketch_lectura_grupo_XX.ino](sketch_lectura_grupo_XX/sketch_lectura_grupo_XX.ino) |
| ¿Cómo se sube? | Por **cable USB** | Por **WiFi (OTA)**, usando lo que dejó preparado el Sketch 1 |
| ¿Qué hace? | Potenciómetro → brillo LED integrado · Temp/humedad · 2 touch → brillo LED externo · todo en pantalla · cada 16 s manda 6 datos a ThingSpeak | Lee esos 6 datos de ThingSpeak · cuenta pulsaciones de un botón · los muestra en pantallas rotativas |

```
  ┌──────────── Sketch 1 ────────────┐         ┌──────────────┐         ┌──────────── Sketch 2 ────────────┐
  │ pot, DHT, touch → LEDs + pantalla │──WiFi──▶│  ThingSpeak  │──WiFi──▶│ lee 6 campos + cuenta pulsador   │
  │ cada 16 s: 6 valores               │ escribe │ (la "nube")  │   lee   │ → pantallas rotativas            │
  └───────────────────────────────────┘         └──────────────┘         └───────────────────────────────────┘
            ▲ USB                                                                   ▲ OTA (por WiFi, puerto 3232)
```

## Orden de lectura

Cada documento explica un tema desde cero. Leelos en orden: cada uno se apoya en el anterior.

0. [Conceptos básicos](docs/00-conceptos-basicos.md) — qué es una ESP32, un sketch, `setup()` y `loop()`, pines
1. [Preparar el entorno](docs/01-preparar-entorno.md) — instalar Arduino IDE, la ESP32 y las librerías
2. [ADC y PWM de 12 bits](docs/02-adc-pwm-12bits.md) — potenciómetro → LED integrado
3. [Pantalla y sensor DHT](docs/03-display-y-dht.md) — I2C, temperatura y humedad
4. [Touch capacitivo](docs/04-touch-capacitivo.md) — LED externo con dos pines táctiles
5. [WiFi y ThingSpeak](docs/05-wifi-thingspeak.md) — crear el canal y mandar datos
6. [OTA](docs/06-ota.md) — subir programas por WiFi
7. [Sketch de lectura](docs/07-sketch-lectura.md) — leer de ThingSpeak, pulsador, pantallas rotativas
8. [Entrega y defensa](docs/08-entrega-y-defensa.md) — checklist y preguntas para la evaluación

## Si tienen poco tiempo (camino rápido)

1. Instalar todo → [doc 01](docs/01-preparar-entorno.md).
2. Subir [prueba_pines.ino](pruebas/prueba_pines/prueba_pines.ino) y descubrir los pines de la placa.
3. Crear el canal de ThingSpeak → [doc 05](docs/05-wifi-thingspeak.md).
4. Completar la sección **CONFIGURACIÓN** de los dos sketches (WiFi, canal, API key, pines, pantalla).
5. Subir el Sketch 1 por USB, verificar que ThingSpeak recibe datos.
6. Subir el Sketch 2 por el puerto de red (OTA) → [doc 06](docs/06-ota.md).
7. Reemplazar `XX` por el número de grupo, completar [integrantes_grupo_XX.txt](integrantes_grupo_XX.txt) y subir a la UV → [doc 08](docs/08-entrega-y-defensa.md).

## Estructura

```
README.md                         ← este archivo
TPI 4K3.pdf                       ← enunciado
docs/                             ← explicaciones paso a paso
pruebas/prueba_pines/             ← sketch para descubrir los pines de la placa
sketch_envio_grupo_XX/            ← Sketch 1 (Arduino exige carpeta con el mismo nombre que el .ino)
sketch_lectura_grupo_XX/          ← Sketch 2
integrantes_grupo_XX.txt          ← datos para la entrega
```

> ⚠️ En el repo las contraseñas y API keys están como `TU_WIFI`, `TU_WRITE_API_KEY`, etc. Los valores reales pónganlos sólo en la copia que suben a la UV, para no publicar la clave del WiFi en GitHub.
