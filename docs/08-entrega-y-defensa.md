# 8. Entrega y defensa

## 8.1 Checklist de entrega (fecha límite: 28/10)

Lo que se sube a la tarea de la UV:

- [ ] Reemplazar **`XX`** por el número de grupo en:
  - nombre de las carpetas y archivos (`sketch_envio_grupo_07/sketch_envio_grupo_07.ino`, etc.)
  - `OTA_NOMBRE` dentro de los dos sketches
  - el encabezado de los dos sketches (`Grupo XX`)
- [ ] En **la copia que se entrega**, completar `WIFI_NOMBRE`, `WIFI_CLAVE`, `CANAL_ID`, `WRITE_API_KEY` (en el repo de GitHub pueden quedar los placeholders).
- [ ] Ajustar pines y pantalla según lo que encontraron con `prueba_pines`.
- [ ] Probar en la placa que **los dos sketches funcionan** (ver 8.2). Para la Aprobación Directa tiene que andar en esta entrega.
- [ ] Completar `integrantes_grupo_XX.txt` (Apellido, Nombre, Legajo de todos + Channel ID + Write API Key) y renombrarlo con el número de grupo.
- [ ] Subir a la UV: `sketch_envio_grupo_XX.ino`, `sketch_lectura_grupo_XX.ino` y el `.txt`.
- [ ] Verificar que el canal de ThingSpeak está en **público** (pestaña *Sharing*).

## 8.2 Guion para probar / demostrar

**Sketch 1 (por USB)**

1. Subir por USB. Pantalla: "Calibrando touch / NO tocar" → "Conectando WiFi" → "WiFi OK + IP".
2. Girar el potenciómetro → el LED integrado cambia de brillo y `PWM int` va de 0 a 4095.
3. Echarle el aliento al DHT o agarrarlo con los dedos → sube la humedad/temperatura.
4. Tocar el pin "subir" → el LED externo sube y `LED ext` llega a 100 %. Tocar "bajar" → baja a 0 %.
5. Ver la última línea contar `TS:OK 15s … 1s` y en la página del canal aparecer un punto nuevo cada 16 s en los 6 gráficos.

**Sketch 2 (por OTA)**

6. Anotar los valores del último envío.
7. (Opcional) desenchufar el USB y alimentar con un cargador.
8. En el IDE elegir el puerto de red `esp32-grupo-XX` y subir el Sketch 2. Pantalla: "Actualizando OTA 10 % … 100 %".
9. Arranca el Sketch 2: muestra los valores anotados, rotando de a 3.
10. Apretar el pulsador varias veces → `Pulsos` aumenta de a uno.

## 8.3 Preguntas que les pueden hacer (con respuesta corta)

**¿Por qué 12 bits? ¿Qué rango tienen?**
2¹² = 4096 valores, de 0 a 4095. El ADC de la ESP32 es de 12 bits y configuramos el PWM igual, así el valor del potenciómetro se pasa directo al LED.

**¿Qué es PWM?**
Prender y apagar el pin muy rápido (5000 veces por segundo). El porcentaje del tiempo que está prendido (duty cycle) define el brillo promedio.

**¿Por qué el potenciómetro está en un pin de ADC1?**
Porque el ADC2 lo usa el WiFi y no se puede leer mientras está conectado.

**¿Cómo funciona el touch?**
Mide la capacitancia del pin; al tocarlo aumenta y `touchRead` devuelve un valor más bajo. Calibramos el valor en reposo al arrancar y detectamos toque por debajo del 70 %.

**¿Por qué cada 16 s?**
La cuenta gratuita de ThingSpeak acepta un dato cada 15 s como mínimo; 16 s da margen.

**¿Por qué usan `millis()` y no `delay()`?**
`delay` bloquea todo: no se actualizarían la pantalla ni los LEDs, y lo más grave, no se llamaría `ArduinoOTA.handle()`, entonces no se podría subir por OTA.

**¿Qué es OTA y cómo funciona?**
Subir el firmware por WiFi. El Sketch 1 escucha en el puerto 3232 y se anuncia por mDNS como `esp32-grupo-XX`. El IDE manda el binario, la ESP32 lo escribe en la otra partición de aplicación (`app1`), lo verifica y reinicia desde ahí.

**¿Qué pasa si se corta la luz en medio de un OTA?**
El programa nuevo se escribe en la otra partición; si no se completa, la placa sigue arrancando con el anterior.

**¿Por qué el Sketch 2 también tiene OTA?**
Porque al subirlo reemplaza al Sketch 1; sin OTA la única forma de volver a programarla sería por cable.

**¿Por qué el Sketch 2 muestra siempre los mismos valores?**
Porque el Sketch 1 ya no está corriendo (fue reemplazado); se ve la última fila que escribió.

**¿Cómo cuentan las pulsaciones sin perder ninguna?**
Con una interrupción por flanco descendente, que se ejecuta aunque el loop esté esperando a ThingSpeak, y un antirrebote de 200 ms.

**¿Qué diferencia hay entre Write API Key y Read API Key?**
La Write permite agregar datos al canal (la usa el Sketch 1). La Read permite leer un canal privado; como el nuestro es público, el Sketch 2 no la necesita.

**¿Cómo genera el número aleatorio?**
`random(100, 501)`; en la ESP32 usa el generador por hardware. El 501 es exclusivo, por eso el máximo es 500.
