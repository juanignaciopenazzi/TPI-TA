# 2. ADC y PWM de 12 bits — potenciómetro → LED integrado

> Requisito: *"Ajustar el brillo del LED integrado de la ESP32 usando el potenciómetro y mostrando el valor de PWM en pantalla. Trabajar con 12 bits."*

## 2.1 El potenciómetro

Es una perilla: una resistencia con un contacto deslizante en el medio. Conectado entre 0 V y 3.3 V, el pin del medio entrega un voltaje que va de **0 V** (perilla a un lado) a **3.3 V** (al otro), pasando por todos los valores intermedios.

## 2.2 ADC: de voltaje a número

El microcontrolador sólo entiende números. El **ADC** (*Analog to Digital Converter*) mide el voltaje del pin y lo convierte en un número entero. Cuántos números distintos puede dar depende de los **bits**:

| Bits | Valores posibles | Rango |
|---|---|---|
| 8  | 2⁸ = 256   | 0 – 255 |
| 10 | 2¹⁰ = 1024 | 0 – 1023 (Arduino UNO) |
| **12** | **2¹² = 4096** | **0 – 4095 (ESP32)** |

Con 12 bits: 0 V → `0`, 3.3 V → `4095`, la mitad → ~`2048`.

```cpp
analogReadResolution(12);                       // 12 bits (es el valor por defecto en ESP32)
int valor = analogRead(PIN_POTENCIOMETRO);      // 0..4095
```

⚠️ **Detalle importante de la ESP32**: tiene dos ADC, ADC1 (GPIO 32–39) y ADC2 (otros pines). **El ADC2 no funciona mientras el WiFi está prendido**, porque el WiFi lo usa internamente. Como nuestro sketch usa WiFi, el potenciómetro tiene que estar en un pin de **ADC1**. Si en la placa de la cátedra el potenciómetro está en un pin de ADC2, avísenle al docente / consulten.

## 2.3 PWM: "voltaje intermedio" en una salida digital

Un pin de salida sólo puede estar en 0 V o 3.3 V. Para que un LED brille *a medias*, lo prendemos y apagamos miles de veces por segundo. El ojo no ve el parpadeo; ve el **promedio**.

```
100%  ███████████████████████   siempre prendido      → brillo máximo
 50%  ████____████____████____   mitad del tiempo      → brillo medio
 10%  █_______█_______█_______   10% del tiempo        → brillo bajo
```

- **Frecuencia**: cuántas veces por segundo se repite el ciclo. Usamos 5000 Hz.
- **Duty cycle** (ciclo de trabajo): qué porcentaje del ciclo está prendido.
- **Resolución**: con cuántos pasos medimos el duty. Con **12 bits**, el duty va de `0` (apagado) a `4095` (siempre prendido).

¡Mismos 12 bits que el ADC! Por eso el valor del potenciómetro se puede pasar **directo** al PWM, sin cuentas:

```cpp
pwmLedIntegrado = analogRead(PIN_POTENCIOMETRO);   // 0..4095
ledcWrite(PIN_LED_INTEGRADO, pwmLedIntegrado);     // 0..4095
```

## 2.4 LEDC: el PWM de la ESP32

En la ESP32 el PWM lo hace un periférico llamado **LEDC**. Hay que configurarlo una vez en `setup()`:

```cpp
ledcAttach(PIN_LED_INTEGRADO, 5000, 12);   // pin, frecuencia (Hz), resolución (bits)
```

La forma de escribirlo cambió entre versiones del "core" de ESP32:

| Core 3.x (actual) | Core 2.x (viejo) |
|---|---|
| `ledcAttach(pin, frec, bits)` | `ledcSetup(canal, frec, bits)` + `ledcAttachPin(pin, canal)` |
| `ledcWrite(pin, valor)` | `ledcWrite(canal, valor)` |

Nuestro sketch usa `#if ESP_ARDUINO_VERSION_MAJOR >= 3` para elegir automáticamente la forma correcta (funciones `configurarPWM` y `escribirPWM`). Así compila en cualquier compu del laboratorio.

## 2.5 En el código

Ver sección **6** de [sketch_envio_grupo_XX.ino](../sketch_envio_grupo_XX/sketch_envio_grupo_XX.ino), función `leerPotenciometro()`. En la pantalla aparece como `PWM int: 2048`.

➡️ Siguiente: [3. Pantalla y sensor DHT](03-display-y-dht.md)
