/*
   Copyright 2016-2017 Bo Zimmerman

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.https://www.amazon.com/#
   See the License for the specific language governing permissions and
   limitations under the License.
*/
//#define TCP_SND_BUF                     4 * TCP_MSS
#define ZIMODEM_VERSION "3.3"
#define DEFAULT_NO_DELAY true
#define null 0

#ifdef ARDUINO_ESP32_DEV
#define SerialConfig uint32_t
#define DEFAULT_PIN_DCD GPIO_NUM_14
#define DEFAULT_PIN_CTS GPIO_NUM_15
#define DEFAULT_PIN_RTS GPIO_NUM_13
#define DEFAULT_PIN_RI GPIO_NUM_32
#define DEFAULT_PIN_DSR GPIO_NUM_12
#define DEFAULT_PIN_DTR GPIO_NUM_27

#else
#define DEFAULT_PIN_DSR 12
#define DEFAULT_PIN_DTR 11
#define DEFAULT_PIN_RI 10
#define DEFAULT_PIN_DCD 2
#define DEFAULT_PIN_CTS 5
#define DEFAULT_PIN_RTS 4
#endif

#define DEFAULT_DCD_HIGH  HIGH
#define DEFAULT_DCD_LOW  LOW
#define DEFAULT_CTS_HIGH  HIGH
#define DEFAULT_CTS_LOW  LOW
#define DEFAULT_RTS_HIGH  HIGH
#define DEFAULT_RTS_LOW  LOW
#define DEFAULT_RI_HIGH  HIGH
#define DEFAULT_RI_LOW  LOW
#define DEFAULT_DSR_HIGH  HIGH
#define DEFAULT_DSR_LOW  LOW
#define DEFAULT_DTR_HIGH  HIGH
#define DEFAULT_DTR_LOW  LOW
#define DEFAULT_BAUD_RATE 1200
#define DEFAULT_SERIAL_CONFIG SERIAL_8N1


class ZMode
{
  public:
    virtual void serialIncoming();
    virtual void loop();
};

#include "pet2asc.h"
#include "zlog.h"
#include "zserout.h"
#include "wificlientnode.h"
#include "wifiservernode.h"
#include "zstream.h"
#include "zslip.h"
#include "zcommand.h"

static WiFiClientNode *conns = null;
static WiFiServerNode *servs = null;
static PhoneBookEntry *phonebook = null;

static ZMode *currMode = null;
static ZStream streamMode;
static ZSlip slipMode;
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
static SerialConfig serialConfig = DEFAULT_SERIAL_CONFIG;
static int baudRate=DEFAULT_BAUD_RATE;
static BaudState baudState = BS_NORMAL; 
static int tempBaud = -1; // -1 do nothing
static int dcdStatus = LOW;
static int pinDCD = DEFAULT_PIN_DCD;
static int pinCTS = DEFAULT_PIN_CTS;
static int pinRTS = DEFAULT_PIN_RTS;
static int pinDSR = DEFAULT_PIN_DSR;
static int pinDTR = DEFAULT_PIN_DTR;
static int pinRI = DEFAULT_PIN_RI;
static int dcdActive = DEFAULT_DCD_HIGH;
static int dcdInactive = DEFAULT_DCD_LOW;
static int ctsActive = DEFAULT_CTS_HIGH;
static int ctsInactive = DEFAULT_CTS_LOW;
static int rtsActive = DEFAULT_RTS_HIGH;
static int rtsInactive = DEFAULT_RTS_LOW;
static int riActive = DEFAULT_RTS_HIGH;
static int riInactive = DEFAULT_RTS_LOW;
static int dtrActive = DEFAULT_RTS_HIGH;
static int dtrInactive = DEFAULT_RTS_LOW;
static int dsrActive = DEFAULT_RTS_HIGH;
static int dsrInactive = DEFAULT_RTS_LOW;

static int getDefaultCtsPin()
{
#ifdef ARDUINO_ESP32_DEV
  return DEFAULT_PIN_CTS;
#else
  if((ESP.getFlashChipSize()/1024)>=4096) // assume this is a striketerm/esp12e
    return DEFAULT_PIN_CTS;
  else
    return 0;
#endif 
}

static bool connectWifi(const char* ssid, const char* password)
{
  int WiFiCounter = 0;
  if(WiFi.status() == WL_CONNECTED)
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
      flushSerial(); // blocking, but very very necessary
      delay(500); // give the client half a sec to catch up
      HWSerial.begin(tempBaud, serialConfig);  //Change baud rate
      baudState = BS_SWITCHED_TEMP;
      break;
    case BS_SWITCH_NORMAL_NEXT:
      flushSerial(); // blocking, but very very necessary
      delay(500); // give the client half a sec to catch up
      HWSerial.begin(baudRate, serialConfig);  //Change baud rate
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
    if(dcdStatus == dcdActive)
    {
      dcdStatus = dcdInactive;
      digitalWrite(pinDCD,dcdStatus);
      if(baudState == BS_SWITCHED_TEMP)
        baudState = BS_SWITCH_NORMAL_NEXT;
      if(currMode == &commandMode)
        clearSerialOutBuffer();
    }
  }
  else
  {
    if(dcdStatus == dcdInactive)
    {
      dcdStatus = dcdActive;
      digitalWrite(pinDCD,dcdStatus);
      if((tempBaud > 0) && (baudState == BS_NORMAL))
        baudState = BS_SWITCH_TEMP_NEXT;
    }
  }
  return num;
}

void setup() 
{
  currMode = &commandMode;
  if(!SPIFFS.begin())
  {
    SPIFFS.format();
    SPIFFS.begin();
  }
  commandMode.loadConfig();
  PhoneBookEntry::loadPhonebook();
  dcdStatus = dcdInactive;
  digitalWrite(pinDCD,dcdStatus);
  flushSerial();
  //enableRtsCts = digitalRead(pinCTS) == ctsActive;
}

void loop() 
{
  if(HWSerial.available())
  {
    currMode->serialIncoming();
  }
  currMode->loop();
}
