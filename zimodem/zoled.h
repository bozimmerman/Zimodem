/*
   zoled.h - Display OLED SSD1306 para Zimodem (ESP8266)
   
   Muestra en pantalla:
     - Estado WiFi e IP
     - Host remoto conectado
     - Baud rate actual
     - Contadores de bytes RX y TX
     - Indicador de actividad (parpadeo)

   Hardware: SSD1306 128x64 I2C
     ESP8266  -> OLED
     3.3V     -> VCC
     GND      -> GND
     D1 (GPIO5)  -> SCL
     D2 (GPIO4)  -> SDA

   Librería requerida: Adafruit SSD1306 + Adafruit GFX
   Instalar desde el Library Manager de Arduino IDE.
*/

#ifndef ZHEADER_OLED_H
#define ZHEADER_OLED_H

// ── Pines I2C para ESP8266 (D1=SCL, D2=SDA) ──────────────────────────────────
#define OLED_SDA_PIN 4   // D2
#define OLED_SCL_PIN 5   // D1
#define OLED_ADDR    0x3C

// ── Dimensiones del display ───────────────────────────────────────────────────
#define OLED_WIDTH  128
#define OLED_HEIGHT 64

// ── Intervalo de refresco (ms) ────────────────────────────────────────────────
#define OLED_REFRESH_MS 500

// Contadores globales de tráfico (incrementados desde zstream.ino)
static unsigned long oledTxBytes = 0;
static unsigned long oledRxBytes = 0;

// Función de inicialización — llamar desde setup()
static void oledSetup();

// Función de actualización — llamar desde loop() o logSocketIn/Out
static void oledLoop();

// Notificación de bytes enviados al socket (TX desde C64 → red)
static inline void oledCountTx(unsigned long bytes = 1) { oledTxBytes += bytes; }

// Notificación de bytes recibidos del socket (RX desde red → C64)
static inline void oledCountRx(unsigned long bytes = 1) { oledRxBytes += bytes; }

#endif // ZHEADER_OLED_H
