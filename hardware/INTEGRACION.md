# Integración del display OLED en Zimodem (ESP8266)

## Hardware necesario
- Display OLED SSD1306 128×64 I2C
- 4 cables dupont

## Conexión de pines

| ESP8266 (NodeMCU/ESP-12E) | OLED SSD1306 |
|---------------------------|--------------|
| 3.3V                      | VCC          |
| GND                       | GND          |
| D2 (GPIO4)                | SDA          |
| D1 (GPIO5)                | SCL          |

> **ESP-01**: Los pines I2C estándar (GPIO4/5) no están disponibles.
> Usar GPIO0 (SDA) y GPIO2 (SCL) y cambiar `OLED_SDA_PIN`/`OLED_SCL_PIN` en `zoled.h`.

---

## Librerías requeridas (Arduino IDE)

Instalar desde *Sketch → Include Library → Manage Libraries*:

1. **Adafruit SSD1306** (by Adafruit)
2. **Adafruit GFX Library** (by Adafruit)

---

## Archivos a agregar al proyecto

Copiar estos dos archivos dentro de la carpeta `zimodem/` (junto a `zimodem.ino`):

```
zimodem/
  zoled.h       ← nuevo
  zoled.ino     ← nuevo
  zimodem.ino   ← modificar (ver abajo)
  zstream.ino   ← modificar (ver abajo)
```

---

## Modificaciones en zimodem.ino

### 1. Incluir el header — después de los demás `#include`

Buscar el bloque de includes al final de los defines (alrededor de línea 230):

```cpp
#include "zprint.h"
```

Agregar inmediatamente después:

```cpp
#include "zoled.h"
```

### 2. Llamar a `oledSetup()` — al final de `setup()`

Buscar el final de la función `setup()`:

```cpp
  flushSerial();
```

Agregar justo después (antes del cierre `}`):

```cpp
  oledSetup();
```

### 3. Llamar a `oledLoop()` — dentro de `loop()`

Buscar la función `loop()`:

```cpp
void loop() 
{
  checkFactoryReset();
  checkReconnect();
```

Agregar `oledLoop();` al final del cuerpo de `loop()`, antes del cierre `}`:

```cpp
  zclock.tick();
  oledLoop();   // ← agregar esta línea
}
```

---

## Modificaciones en zstream.ino

Estas dos líneas instrumentan los contadores de bytes TX y RX.

### TX: bytes enviados al socket (C64 → red)

Buscar la función `ZStream::socketWrite(uint8_t c)`:

```cpp
void ZStream::socketWrite(uint8_t c)
{
  if(current->isConnected())
  {
    if(c == 0xFF && isTelnet()) 
      current->write(c);
    current->write(c);
    logSocketOut(c);
```

Agregar `oledCountTx();` justo después de `logSocketOut(c);`:

```cpp
    logSocketOut(c);
    oledCountTx();    // ← agregar esta línea
    nextFlushMs=millis()+250;
```

### TX bulk: bytes enviados en buffer (C64 → red, variante de múltiples bytes)

Buscar `ZStream::socketWrite(uint8_t *buf, uint8_t len)`:

```cpp
    for(int i=0;i<len;i++)
      logSocketOut(buf[i]);
    current->write(buf,len);
```

Agregar después de `current->write(buf,len);`:

```cpp
    current->write(buf,len);
    oledCountTx(len);    // ← agregar esta línea
```

### RX: bytes recibidos del socket (red → C64)

Buscar en `ZStream::loop()` donde se lee del socket:

```cpp
            uint8_t c=current->read();
            logSocketIn(c);
```

Agregar `oledCountRx();` después de `logSocketIn(c);`:

```cpp
            logSocketIn(c);
            oledCountRx();    // ← agregar esta línea
```

---

## Verificación rápida

Al compilar no debe haber errores nuevos. Si el display no aparece al encender:

1. Verificar dirección I2C: algunos módulos usan `0x3D` en vez de `0x3C`.
   Cambiar `OLED_ADDR` en `zoled.h`.
2. Verificar conexión SDA/SCL no invertida.
3. Verificar que la alimentación sea 3.3V (no 5V).

---

## Lo que muestra el display

```
┌──────────────────────────────┐
│ ZIMODEM 4.0.3        [W]  ^  │  ← WiFi OK, actividad TX
│──────────────────────────────│
│ 192.168.1.42                 │  ← IP local
│ bbs.retro64.net:6400         │  ← host conectado (o "Sin conexion")
│──────────────────────────────│
│ 2400 bps                     │  ← velocidad configurada
│ TX:1.2 KB  RX:4.8 KB        │  ← tráfico acumulado
└──────────────────────────────┘
```

- `[W]` = WiFi conectado / `[ ]` = desconectado  
- `^` = transmitiendo, `v` = recibiendo, `.` = inactivo  
- Los contadores se resetean al reiniciar el ESP8266
