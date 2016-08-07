#include <ESP8266WiFi.h>
#include <FS.h>
#include <spiffs/spiffs.h>

#define null 0
#define ZIMODEM_VERSION "0.1"

#include "pet2asc.h"
#include "zmode.h"
#include "wificlientnode.h"
#include "wifiservernode.h"
#include "zstream.h"
#include "zcommand.h"

static WiFiClientNode *conns = null;
static WiFiServerNode *servs = null;
static ZMode *currMode = null;
static ZStream streamMode;
static ZCommand commandMode;

static bool connectWifi(const char* ssid, const char* password)
{
  int WiFiCounter = 0;
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && WiFiCounter < 30) 
  {
    delay(1000);
    WiFiCounter++;
  }
  return WiFi.status() == WL_CONNECTED;
}

void setup() 
{
  delay(10);
  currMode = &commandMode;
  SPIFFS.begin();
  commandMode.loadConfig();
  Serial.begin(commandMode.baudRate);  //Start Serial
  Serial.print(commandMode.EOLN);
  Serial.print("ZiModem v");
  Serial.setTimeout(60000);
  Serial.print(ZIMODEM_VERSION);
  Serial.print(commandMode.EOLN);
  Serial.printf("sdk=%s core=%s cpu@%d",ESP.getSdkVersion(),ESP.getCoreVersion().c_str(),ESP.getCpuFreqMHz());
  Serial.print(commandMode.EOLN);
  Serial.printf("flash chipid=%d size=%d rsize=%d speed=%d",ESP.getFlashChipId(),ESP.getFlashChipSize(),ESP.getFlashChipRealSize(),ESP.getFlashChipSpeed());
  Serial.print(commandMode.EOLN);
  if(commandMode.wifiSSI.length()>0)
  {
    if(connectWifi(commandMode.wifiSSI.c_str(),commandMode.wifiPW.c_str()))
      Serial.print("CONNECTED TO " + commandMode.wifiSSI + " (" + WiFi.localIP().toString().c_str() + ")");
    else
      Serial.print("ERROR ON " + commandMode.wifiSSI);
  }
  else
    Serial.print("INITAILIZED");
  Serial.print(commandMode.EOLN);
  Serial.print("READY.");
  Serial.print(commandMode.EOLN);
}

void loop() 
{
  if(Serial.available())
  {
    currMode->serialIncoming();
  }
  currMode->loop();
}
