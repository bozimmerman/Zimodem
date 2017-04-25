/*
   Copyright 2016-2017 Bo Zimmerman

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
#define null 0
#define ZIMODEM_VERSION "2.6"

#include "pet2asc.h"
#include "zmode.h"
#include "wificlientnode.h"
#include "wifiservernode.h"
#include "zstream.h"
#include "zcommand.h"

static WiFiClientNode *conns = null;
static WiFiServerNode *servs = null;
static PhoneBookEntry *phonebook = null;

static ZMode *currMode = null;
static ZStream streamMode;
static ZCommand commandMode;

enum BaudState
{
  BS_NORMAL,
  BS_SWITCH_TEMP_NEXT,
  BS_SWITCHED_TEMP,
  BS_SWITCH_NORMAL_NEXT
};

static bool wifiConnected =false;
static String wifiSSI;
static String wifiPW;
static SerialConfig serialConfig = SERIAL_8N1;
static int baudRate=1200;
static BaudState baudState = BS_NORMAL; 
static int tempBaud = -1; // -1 do nothing
static int dcdStatus = LOW; 
static int DCD_HIGH = HIGH;
static int DCD_LOW = LOW;
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

static void checkBaudChange()
{
  switch(baudState)
  {
    case BS_SWITCH_TEMP_NEXT:
      delay(500); // give the client half a sec to catch up
      Serial.begin(tempBaud, serialConfig);  //Change baud rate
      baudState = BS_SWITCHED_TEMP;
      break;
    case BS_SWITCH_NORMAL_NEXT:
      delay(500); // give the client half a sec to catch up
      Serial.begin(baudRate, serialConfig);  //Change baud rate
      baudState = BS_NORMAL;
      break;
    default:
      break;
  }
}

static int checkOpenConnections()
{
  int num = 0;
  WiFiClientNode *conn = conns;
  while(conn != null)
  {
    if(conn->isConnected())
      num++;
    conn = conn->next;
  }
  if(num == 0)
  {
    if(dcdStatus == DCD_HIGH)
    {
      dcdStatus = DCD_LOW;
      digitalWrite(2,dcdStatus);
      if(baudState == BS_SWITCHED_TEMP)
        baudState = BS_SWITCH_NORMAL_NEXT;
      if(currMode == &commandMode)
        streamMode.clearSerialBuffer();
    }
  }
  else
  {
    if(dcdStatus == DCD_LOW)
    {
      dcdStatus = DCD_HIGH;
      digitalWrite(2,dcdStatus);
      if((tempBaud > 0) && (baudState == BS_NORMAL))
        baudState = BS_SWITCH_TEMP_NEXT;
    }
  }
  return num;
}

void setup() 
{
  delay(10);
  currMode = &commandMode;
  SPIFFS.begin();
  commandMode.loadConfig();
  PhoneBookEntry::loadPhonebook();
  dcdStatus = DCD_LOW;
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
