/*
   zoled.ino - Display OLED SSD1306 para Zimodem (ESP8266)

   Layout del display 128x64:
   ┌────────────────────────────┐
   │ ZIMODEM  4.0.3    [W]   ^ │  ← título + indicador WiFi + actividad
   │────────────────────────────│
   │ 192.168.1.42               │  ← IP local
   │ bbs.retro64.net:6400       │  ← host:puerto (o "Sin conexion")
   │────────────────────────────│
   │ 2400 bps                   │  ← baud rate
   │ TX:  1.2 KB  RX:  4.8 KB  │  ← contadores
   └────────────────────────────┘

   Pines I2C (ESP8266):
     D2 (GPIO4) → SDA
     D1 (GPIO5) → SCL
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── Configuración ─────────────────────────────────────────────────────────────
#define OLED_SDA_PIN    4       // D2
#define OLED_SCL_PIN    5       // D1
#define OLED_ADDR       0x3C   // Cambiar a 0x3D si el display no responde
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_REFRESH_MS 500

// ── Contadores de tráfico (incrementados desde zstream.ino) ───────────────────
static unsigned long oledTxBytes = 0;
static unsigned long oledRxBytes = 0;

static inline void oledCountTx(unsigned long bytes = 1) { oledTxBytes += bytes; }
static inline void oledCountRx(unsigned long bytes = 1) { oledRxBytes += bytes; }

// ── Objeto display ────────────────────────────────────────────────────────────
static Adafruit_SSD1306 oledDisplay(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool oledAvailable = false;
static unsigned long oledLastRefresh = 0;

// Indicador de actividad rotatoria (TX/RX blink)
static bool oledActivityTx = false;
static bool oledActivityRx = false;
static unsigned long oledLastTxCount = 0;
static unsigned long oledLastRxCount = 0;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Formatea bytes en forma legible: "1.2 KB", "345 B", "1.0 MB"
static String oledFormatBytes(unsigned long b)
{
  if (b < 1024)
  {
    return String(b) + " B ";
  }
  else if (b < 1024UL * 1024)
  {
    float kb = b / 1024.0f;
    String s = String(kb, 1);
    return s + " KB";
  }
  else
  {
    float mb = b / (1024.0f * 1024.0f);
    String s = String(mb, 1);
    return s + " MB";
  }
}

// Recorta un String al ancho máximo en caracteres (fuente 1 = 6px/char)
static String oledTruncate(const String &s, int maxChars)
{
  if ((int)s.length() <= maxChars)
    return s;
  return s.substring(0, maxChars - 1) + "~";
}

// ── Dibuja el frame completo ──────────────────────────────────────────────────
static void oledDraw()
{
  oledDisplay.clearDisplay();
  oledDisplay.setTextColor(SSD1306_WHITE);

  // ── Línea 1: título + estado WiFi + actividad ─────────────────────────────
  oledDisplay.setTextSize(1);
  oledDisplay.setCursor(0, 0);
  oledDisplay.print(F("ZIMODEM "));
  oledDisplay.print(ZIMODEM_VERSION);

  // Icono WiFi: relleno si conectado, hueco si no
  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  oledDisplay.setCursor(96, 0);
  oledDisplay.print(wifiOk ? F("[W]") : F("[ ]"));

  // Indicador de actividad TX/RX
  oledDisplay.setCursor(116, 0);
  if (oledActivityTx)
    oledDisplay.print(F("^"));  // TX activo
  else if (oledActivityRx)
    oledDisplay.print(F("v"));  // RX activo
  else
    oledDisplay.print(F("."));

  // Línea separadora
  oledDisplay.drawFastHLine(0, 10, OLED_WIDTH, SSD1306_WHITE);

  // ── Línea 2: dirección IP ─────────────────────────────────────────────────
  oledDisplay.setCursor(0, 13);
  if (wifiOk)
    oledDisplay.print(WiFi.localIP().toString());
  else
    oledDisplay.print(F("Sin WiFi"));

  // ── Línea 3: host remoto o estado ─────────────────────────────────────────
  oledDisplay.setCursor(0, 23);
  bool hasConn = (WiFiClientNode::getNumOpenWiFiConnections() > 0);
  if (hasConn && conns != null)
  {
    // Busca la primera conexión activa
    WiFiClientNode *c = conns;
    while (c != null && !c->isConnected())
      c = c->next;
    if (c != null)
    {
      String hostLine = String(c->host) + ":" + String(c->port);
      oledDisplay.print(oledTruncate(hostLine, 21));
    }
    else
    {
      oledDisplay.print(F("Conectando..."));
    }
  }
  else
  {
    oledDisplay.print(F("Sin conexion"));
  }

  // Línea separadora
  oledDisplay.drawFastHLine(0, 33, OLED_WIDTH, SSD1306_WHITE);

  // ── Línea 4: baud rate ────────────────────────────────────────────────────
  oledDisplay.setCursor(0, 36);
  oledDisplay.print(baudRate);
  oledDisplay.print(F(" bps"));

  // ── Línea 5: contadores TX / RX ───────────────────────────────────────────
  oledDisplay.setCursor(0, 50);
  oledDisplay.print(F("TX:"));
  oledDisplay.print(oledFormatBytes(oledTxBytes));
  oledDisplay.print(F(" RX:"));
  oledDisplay.print(oledFormatBytes(oledRxBytes));

  oledDisplay.display();
}

// ── Setup ─────────────────────────────────────────────────────────────────────
static void oledSetup()
{
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  if (!oledDisplay.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
  {
    // El display no respondió — continuamos sin él
    oledAvailable = false;
    return;
  }

  oledAvailable = true;
  oledDisplay.clearDisplay();
  oledDisplay.setTextColor(SSD1306_WHITE);
  oledDisplay.setTextSize(1);

  // Pantalla de arranque
  oledDisplay.setCursor(20, 20);
  oledDisplay.print(F("ZIMODEM "));
  oledDisplay.print(ZIMODEM_VERSION);
  oledDisplay.setCursor(10, 35);
  oledDisplay.print(F("C64 WiFi Modem"));
  oledDisplay.display();
  delay(1500);
}

// ── Loop ──────────────────────────────────────────────────────────────────────
static void oledLoop()
{
  if (!oledAvailable)
    return;

  unsigned long now = millis();

  // Detecta actividad comparando contadores
  oledActivityTx = (oledTxBytes != oledLastTxCount);
  oledActivityRx = (oledRxBytes != oledLastRxCount);
  oledLastTxCount = oledTxBytes;
  oledLastRxCount = oledRxBytes;

  if (now - oledLastRefresh >= OLED_REFRESH_MS)
  {
    oledLastRefresh = now;
    oledDraw();
  }
}
