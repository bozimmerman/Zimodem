/*
   Copyright 2016-2016 Bo Zimmerman

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#define TCP_SND_BUF                     4 * TCP_MSS
// this failed to change ClientContext.h delay(5000)! Therefore, that change needs to be made by hand for now.
#define delay(a) if((strstr(__FILE__,"ClientContext")!=null)&&(a==5000)) delay(10); else delay(a);

#include <ESP8266WiFi.h>
#include <FS.h>
#include <spiffs/spiffs.h>

#define null 0
#define ZIMODEM_VERSION "1.5"

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

static bool wifiConnected =false;
static String wifiSSI;
static String wifiPW;
static int baudRate=1200;
static int dcdStatus = LOW; 
static bool logFileOpen = false;
static File logFile; 
static const int BUFSIZE = 4096;
static uint8_t TBUF[BUFSIZE];
static int TBUFhead=0;
static int TBUFtail=0;

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
  wifiConnected = WiFi.status() == WL_CONNECTED;
  return wifiConnected;
}

static void showInitMessage()
{
  Serial.print(commandMode.EOLN);
  Serial.print("ZiModem v");
  Serial.setTimeout(60000);
  Serial.print(ZIMODEM_VERSION);
  Serial.print(commandMode.EOLN);
  Serial.printf("sdk=%s core=%s cpu@%d",ESP.getSdkVersion(),ESP.getCoreVersion().c_str(),ESP.getCpuFreqMHz());
  Serial.print(commandMode.EOLN);
  Serial.printf("chipid=%d size=%dk rsize=%dk speed=%dm",ESP.getFlashChipId(),(ESP.getFlashChipSize()/1024),(ESP.getFlashChipRealSize()/1024),(ESP.getFlashChipSpeed()/1000000));
  Serial.print(commandMode.EOLN);
  if(wifiSSI.length()>0)
  {
    if(wifiConnected)
      Serial.print("CONNECTED TO " + wifiSSI + " (" + WiFi.localIP().toString().c_str() + ")");
    else
      Serial.print("ERROR ON " + wifiSSI);
  }
  else
    Serial.print("INITIALIZED");
  Serial.print(commandMode.EOLN);
  Serial.print("READY.");
  Serial.print(commandMode.EOLN);
  Serial.flush();
}

void setup() 
{
  delay(10);
  currMode = &commandMode;
  SPIFFS.begin();
  commandMode.loadConfig();
  pinMode(0,OUTPUT);
  digitalWrite(0,dcdStatus);
  pinMode(2,OUTPUT);
  digitalWrite(2,dcdStatus);
}

void loop() 
{
  if(Serial.available())
  {
    currMode->serialIncoming();
  }
  currMode->loop();
}
