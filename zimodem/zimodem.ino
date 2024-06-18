/*
   Copyright 2016-2024 Bo Zimmerman

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
//#define TCP_SND_BUF                     4 * TCP_MSS
#define ZIMODEM_VERSION "4.0.2"
const char compile_date[] = __DATE__ " " __TIME__;
#define DEFAULT_NO_DELAY true
#define null 0
#define INCLUDE_IRCC true
//#define INCLUDE_CMDRX16 true // enable this if you are David or Kevin
//# define USE_DEVUPDATER true // only enable this if your name is Bo

#ifdef ARDUINO_ESP32_DEV
# define ZIMODEM_ESP32
#elif ARDUINO_ESP32S3_DEV
# define ZIMODEM_ESP32
#elif defined(ESP32)
# define ZIMODEM_ESP32
#elif defined(ARDUINO_ESP320)
# define ZIMODEM_ESP32
#elif defined(ARDUINO_NANO32)
# define ZIMODEM_ESP32
#elif defined(ARDUINO_LoLin32)
# define ZIMODEM_ESP32
#elif defined(ARDUINO_ESPea32)
# define ZIMODEM_ESP32
#elif defined(ARDUINO_QUANTUM)
# define ZIMODEM_ESP32
#else
# define ZIMODEM_ESP8266
#endif

#ifdef SUPPORT_LED_PINS
# ifdef GPIO_NUM_0
#   define DEFAULT_PIN_AA GPIO_NUM_35
#   define DEFAULT_PIN_HS GPIO_NUM_34
#   define DEFAULT_PIN_WIFI GPIO_NUM_26
# else
#   define DEFAULT_PIN_AA 35
#   define DEFAULT_PIN_HS 34
#   define DEFAULT_PIN_WIFI 26
# endif
# define DEFAULT_HS_BAUD 38400
# define DEFAULT_AA_ACTIVE LOW
# define DEFAULT_AA_INACTIVE HIGH
# define DEFAULT_HS_ACTIVE LOW
# define DEFAULT_HS_INACTIVE HIGH
# define DEFAULT_WIFI_ACTIVE LOW
# define DEFAULT_WIFI_INACTIVE HIGH
#endif

#define DEFAULT_BAUD_RATE 1200
#define DEFAULT_SERIAL_CONFIG SERIAL_8N1
/*
 * Unused pins on WROOM32:
 * 2, 20, 21, 22.   36,39 (sensor)
 */

#ifdef ZIMODEM_ESP32
# define PIN_FACTORY_RESET GPIO_NUM_0
# define INCLUDE_SD_SHELL true            /* ****** Delete this line if you do not have an external SD card interface *****/
# define DEFAULT_FCT FCT_DISABLED
# ifdef INCLUDE_CMDRX16                   /* Configuration for the Commander X16 I/O Card */
#  undef INCLUDE_SD_SHELL
#  undef DEFAULT_FCT
#  define DEFAULT_FCT FCT_RTSCTS
#  undef DEFAULT_BAUD_RATE
#  define DEFAULT_BAUD_RATE 115200
#  define DEFAULT_PIN_DCD GPIO_NUM_22
#  define DEFAULT_PIN_CTS GPIO_NUM_19
#  define DEFAULT_PIN_RTS GPIO_NUM_21
#  define DEFAULT_PIN_RI GPIO_NUM_18
#  define DEFAULT_PIN_DSR GPIO_NUM_23
#  define DEFAULT_PIN_DTR GPIO_NUM_25
//#  define DEFAULT_PIN_OPA 26
#  define DEFAULT_PIN_OPB GPIO_NUM_27
#  define DEFAULT_PIN_OTH GPIO_NUM_26 // stream/command mode flag
#  define DEFAULT_PIN_TXD GPIO_NUM_32
#  define DEFAULT_PIN_RXD GPIO_NUM_33
#  define DEFAULT_PIN_SND GPIO_NUM_4
# elif defined(ARDUINO_ESP32S3_DEV)       /* Configuration for the Esp32S3 16MB Dev Board */
#  define DEFAULT_PIN_DCD GPIO_NUM_14
#  define DEFAULT_PIN_CTS GPIO_NUM_19 // espdev rts pin
#  define DEFAULT_PIN_RTS GPIO_NUM_20 // espdev cts pin
#  define DEFAULT_PIN_RI GPIO_NUM_10
#  define DEFAULT_PIN_DSR GPIO_NUM_12
#  define DEFAULT_PIN_SND GPIO_NUM_11
#  define DEFAULT_PIN_OTH GPIO_NUM_46 // pulse pin
#  define DEFAULT_PIN_DTR GPIO_NUM_13
# else                                    /* Configuration for standard ESP32 4 & 8MB boards */
#  define DEFAULT_PIN_DCD GPIO_NUM_14
#  define DEFAULT_PIN_CTS GPIO_NUM_13
#  define DEFAULT_PIN_RTS GPIO_NUM_15 // unused?
#  define DEFAULT_PIN_RI GPIO_NUM_32
#  define DEFAULT_PIN_DSR GPIO_NUM_12
#  define DEFAULT_PIN_SND GPIO_NUM_25
#  define DEFAULT_PIN_OTH GPIO_NUM_4 // pulse pin
#  define DEFAULT_PIN_DTR GPIO_NUM_27
# endif
# define debugPrintf DBSerial.printf
# define SerialConfig uint32_t
# define UART_CONFIG_MASK 0x8000000
# define UART_NB_BIT_MASK      0B00001100 | UART_CONFIG_MASK
# define UART_NB_BIT_5         0B00000000 | UART_CONFIG_MASK
# define UART_NB_BIT_6         0B00000100 | UART_CONFIG_MASK
# define UART_NB_BIT_7         0B00001000 | UART_CONFIG_MASK
# define UART_NB_BIT_8         0B00001100 | UART_CONFIG_MASK
# define UART_PARITY_MASK      0B00000011
# define UART_PARITY_NONE      0B00000000
# define UART_NB_STOP_BIT_MASK 0B00110000
# define UART_NB_STOP_BIT_0    0B00000000
# define UART_NB_STOP_BIT_1    0B00010000
# define UART_NB_STOP_BIT_15   0B00100000
# define UART_NB_STOP_BIT_2    0B00110000
# define preEOLN(...)
//# define preEOLN serial.prints
# define echoEOLN(...) serial.prints(EOLN)
//# define echoEOLN serial.write
//# define HARD_DCD_HIGH 1
//# define HARD_DCD_LOW 1
# define INCLUDE_SSH true
# define INCLUDE_SLIP true  // Disable this before checkin, until it works
# ifdef INCLUDE_SD_SHELL
#  define INCLUDE_HOSTCM true // safe to remove if you need space
#  define INCLUDE_FTP true
#  define INCLUDE_CBMMODEM true  // 1650, 1660, 1670, and Pulse Dialing support
#  define INCLUDE_COMET64 true
# endif
#else  // ESP-8266, e.g. ESP-01, ESP-12E
# define DEFAULT_PIN_DSR 13
# define DEFAULT_PIN_DTR 12
# define DEFAULT_PIN_RI 14
# define DEFAULT_PIN_RTS 4
# define DEFAULT_PIN_CTS 5 // is 0 for ESP-01
# define DEFAULT_PIN_DCD 2
# define DEFAULT_PIN_OTH MAX_PIN_NO // pulse pin
# define DEFAULT_FCT FCT_DISABLED
# define debugPrintf doNothing
# define preEOLN(...)
# define echoEOLN(...) serial.prints(EOLN)
#endif

# define INCLUDE_PING true

# define DEFAULT_DCD_ACTIVE  LOW
# define DEFAULT_DCD_INACTIVE  HIGH
# define DEFAULT_CTS_ACTIVE  LOW
# define DEFAULT_CTS_INACTIVE  HIGH
# define DEFAULT_RTS_ACTIVE  LOW
# define DEFAULT_RTS_INACTIVE  HIGH
# define DEFAULT_RI_ACTIVE  LOW
# define DEFAULT_RI_INACTIVE  HIGH
# define DEFAULT_DSR_ACTIVE  LOW
# define DEFAULT_DSR_INACTIVE  HIGH
# define DEFAULT_DTR_ACTIVE  LOW
# define DEFAULT_DTR_INACTIVE  HIGH
# define DEFAULT_OTH_ACTIVE  LOW
# define DEFAULT_OTH_INACTIVE  HIGH

#define MAX_PIN_NO 50
#define INTERNAL_FLOW_CONTROL_DIV 380
#define DEFAULT_RECONNECT_DELAY 60000
#define MAX_RECONNECT_DELAY 1800000

class ZMode
{
  public:
    virtual void serialIncoming();
    virtual void loop();
};

#include "pet2asc.h"
#include "rt_clock.h"
#include "filelog.h"
#include "serout.h"
#include "connSettings.h"
#include "wificlientnode.h"
#include "stringstream.h"
#include "phonebook.h"
#include "wifiservernode.h"
#include "zstream.h"
#include "proto_http.h"
#include "proto_ftp.h"
#include "zconfigmode.h"
#include "zcommand.h"
#include "zprint.h"

#ifdef INCLUDE_SD_SHELL
#  ifdef INCLUDE_HOSTCM
#    include "zhostcmmode.h"
#  endif
#  ifdef INCLUDE_COMET64
#    include "zcomet64mode.h"
#  endif
#  include "proto_xmodem.h"
#  include "proto_zmodem.h"
#  include "proto_punter.h"
#  include "proto_kermit.h"
#  include "zbrowser.h"
#endif
#ifdef INCLUDE_SLIP
#  include "zslipmode.h"
#endif
#ifdef INCLUDE_IRCC
#  include "zircmode.h"
#endif

static WiFiClientNode *conns = null;
static WiFiServerNode *servs = null;
static PhoneBookEntry *phonebook = null;
static bool pinSupport[MAX_PIN_NO];
static int pinCache[MAX_PIN_NO];
static String termType = DEFAULT_TERMTYPE;
static String busyMsg = DEFAULT_BUSYMSG;
static bool debugUart = false;

static OpModes altOpMode = OPMODE_NONE;
static ZMode *currMode = null;
static ZStream streamMode;
//static ZSlip slipMode; // not yet implemented
static ZCommand commandMode;
static ZPrint printMode;
static ZConfig configMode;
static RealTimeClock zclock(0);
#ifdef INCLUDE_SD_SHELL
#  ifdef INCLUDE_HOSTCM
     static ZHostCMMode hostcmMode;
#  endif
#  ifdef INCLUDE_COMET64
     static ZComet64Mode comet64Mode;
#  endif
   static ZBrowser browseMode;
#endif
#ifdef INCLUDE_SLIP
   static ZSLIPMode slipMode;
#endif
#ifdef INCLUDE_IRCC
   static ZIRCMode ircMode;
#endif

enum BaudState
{
  BS_NORMAL,
  BS_SWITCH_TEMP_NEXT,
  BS_SWITCHED_TEMP,
  BS_SWITCH_NORMAL_NEXT
};

static String wifiSSI;
static String wifiPW;
static String hostname;
static IPAddress *staticIP = null;
static IPAddress *staticDNS = null;
static IPAddress *staticGW = null;
static IPAddress *staticSN = null;
static unsigned long lastConnectAttempt = 0;
static unsigned long nextReconnectDelay = 0; // zero means don't attempt reconnects
static SerialConfig serialConfig = DEFAULT_SERIAL_CONFIG;
static int baudRate=DEFAULT_BAUD_RATE;
static int dequeSize=1+(DEFAULT_BAUD_RATE/INTERNAL_FLOW_CONTROL_DIV);
static BaudState baudState = BS_NORMAL; 
static unsigned long resetPushTimer=0;
static int tempBaud = -1; // -1 do nothing
static int dcdStatus = DEFAULT_DCD_INACTIVE;
static int pinDCD = DEFAULT_PIN_DCD;
static int pinCTS = DEFAULT_PIN_CTS;
static int pinRTS = DEFAULT_PIN_RTS;
static int pinDSR = DEFAULT_PIN_DSR;
static int pinDTR = DEFAULT_PIN_DTR;
static int pinOTH = DEFAULT_PIN_OTH;
static int pinRI = DEFAULT_PIN_RI;
static int dcdActive = DEFAULT_DCD_ACTIVE;
static int dcdInactive = DEFAULT_DCD_INACTIVE;
static int ctsActive = DEFAULT_CTS_ACTIVE;
static int ctsInactive = DEFAULT_CTS_INACTIVE;
static int rtsActive = DEFAULT_RTS_ACTIVE;
static int rtsInactive = DEFAULT_RTS_INACTIVE;
static int riActive = DEFAULT_RI_ACTIVE;
static int riInactive = DEFAULT_RI_INACTIVE;
static int dtrActive = DEFAULT_DTR_ACTIVE;
static int dtrInactive = DEFAULT_DTR_INACTIVE;
static int dsrActive = DEFAULT_DSR_ACTIVE;
static int dsrInactive = DEFAULT_DSR_INACTIVE;
static int othActive = DEFAULT_OTH_ACTIVE;
static int othInactive = DEFAULT_OTH_INACTIVE;

static int getDefaultCtsPin()
{
  return DEFAULT_PIN_CTS;
}

static void doNothing(const char* format, ...) 
{
}

static void s_pinWrite(uint8_t pinNo, uint8_t value)
{
  if(pinSupport[pinNo])
  {
    pinCache[pinNo] = value;
    digitalWrite(pinNo, value);
  }
}

static void setHostName(const char *hname)
{
#ifdef ZIMODEM_ESP32
      tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hname);
#else
      WiFi.hostname(hname);
#endif
}

static void setNewStaticIPs(IPAddress *ip, IPAddress *dns, IPAddress *gateWay, IPAddress *subNet)
{
  if(staticIP != null)
    free(staticIP);
  staticIP = ip;
  if(staticDNS != null)
    free(staticDNS);
  staticDNS = dns;
  if(staticGW != null)
    free(staticGW);
  staticGW = gateWay;
  if(staticSN != null)
    free(staticSN);
  staticSN = subNet;
}

static bool connectWifi(const char* ssid, const char* password, IPAddress *ip, IPAddress *dns, IPAddress *gateWay, IPAddress *subNet)
{
  while(WiFi.status() == WL_CONNECTED)
  {
    WiFi.disconnect();
    delay(100);
    yield();
  }
#ifndef ZIMODEM_ESP32
  if(hostname.length() > 0)
    setHostName(hostname.c_str());
#endif
  WiFi.mode(WIFI_STA);
  if((ip != null)&&(gateWay != null)&&(dns != null)&&(subNet!=null))
  {
    if(!WiFi.config(*ip,*gateWay,*subNet,*dns))
      return false;
  }
  WiFi.begin(ssid, password);
  if(hostname.length() > 0)
    setHostName(hostname.c_str());
  bool amConnected = (WiFi.status() == WL_CONNECTED) && (strcmp(WiFi.localIP().toString().c_str(), "0.0.0.0")!=0);
  int WiFiCounter = 0;
  while ((!amConnected) && (WiFiCounter < 20))
  {
    WiFiCounter++;
    if(!amConnected)
      delay(500);
    amConnected = (WiFi.status() == WL_CONNECTED) && (strcmp(WiFi.localIP().toString().c_str(), "0.0.0.0")!=0);
  }
  lastConnectAttempt = millis();
  if(lastConnectAttempt == 0)  // it IS possible for millis() to be 0, but we need to ignore it.
    lastConnectAttempt = 1; // 0 is a special case, so skip it

  if(!amConnected)
  {
    nextReconnectDelay = 0; // assume no retry is desired.. let the caller set it up, as it could be bad PW
    WiFi.disconnect();
  }
  else
    nextReconnectDelay = DEFAULT_RECONNECT_DELAY; // if connected, we always want to try reconns in the future

#ifdef SUPPORT_LED_PINS
  s_pinWrite(DEFAULT_PIN_WIFI,(WiFi.status() == WL_CONNECTED)?DEFAULT_WIFI_ACTIVE:DEFAULT_WIFI_INACTIVE);
#endif
  if(WiFi.status() == WL_CONNECTED)
    debugPrintf("Connected to %s with IP %s.\r\n",ssid,WiFi.localIP().toString().c_str());
  return (WiFi.status() == WL_CONNECTED);
}

static void checkBaudChange()
{
  switch(baudState)
  {
    case BS_SWITCH_TEMP_NEXT:
      changeBaudRate(tempBaud);
      baudState = BS_SWITCHED_TEMP;
      break;
    case BS_SWITCH_NORMAL_NEXT:
      changeBaudRate(baudRate);
      baudState = BS_NORMAL;
      break;
    default:
      break;
  }
}

static void changeBaudRate(int baudRate)
{
  flushSerial(); // blocking, but very very necessary
  delay(500); // give the client half a sec to catch up
  logPrintfln("Baud change to %d.",baudRate);
  dequeSize=1+(baudRate/INTERNAL_FLOW_CONTROL_DIV);
  debugPrintf("Baud %d, Deque constant now: %d\r\n",baudRate,dequeSize);
#ifdef ZIMODEM_ESP32
  HWSerial.updateBaudRate(baudRate);
#else
  HWSerial.begin(baudRate, serialConfig);  //Change baud rate
#endif
#ifdef SUPPORT_LED_PINS
  s_pinWrite(DEFAULT_PIN_HS,(baudRate>=DEFAULT_HS_BAUD)?DEFAULT_HS_ACTIVE:DEFAULT_HS_INACTIVE);
#endif  
}

static void changeSerialConfig(SerialConfig conf)
{
  flushSerial(); // blocking, but very very necessary
  delay(500); // give the client half a sec to catch up
  debugPrintf("Config changing to %d.\r\n",(int)conf);
  dequeSize=1+(baudRate/INTERNAL_FLOW_CONTROL_DIV);
  debugPrintf("Deque constant now: %d\r\n",dequeSize);
# ifdef DEFAULT_PIN_RXD
    HWSerial.begin(baudRate, conf, DEFAULT_PIN_RXD, DEFAULT_PIN_TXD);
# else
    HWSerial.begin(baudRate, conf);  //Change baud rate
# endif
  debugPrintf("Config changed.\r\n");
}

static int checkOpenConnections()
{
  int num=WiFiClientNode::getNumOpenWiFiConnections();
  if(num == 0)
  {
    if((dcdStatus == dcdActive)
    &&(dcdStatus != dcdInactive))
    {
      dcdStatus = dcdInactive;
      s_pinWrite(pinDCD,dcdStatus);
      if(baudState == BS_SWITCHED_TEMP)
        baudState = BS_SWITCH_NORMAL_NEXT;
      if(currMode == &commandMode)
        clearSerialOutBuffer();
    }
  }
  else
  {
    if((dcdStatus == dcdInactive)
    &&(dcdStatus != dcdActive))
    {
      dcdStatus = dcdActive;
      s_pinWrite(pinDCD,dcdStatus);
      if((tempBaud > 0) && (baudState == BS_NORMAL))
        baudState = BS_SWITCH_TEMP_NEXT;
    }
  }
  return num;
}

void setup() 
{
  for(int i=0;i<MAX_PIN_NO;i++)
    pinSupport[i]=false;
#ifdef ZIMODEM_ESP32
  DBSerial.begin(115200); //the debug port
  DBSerial.setDebugOutput(true);
# ifdef ARDUINO_ESP32S3_DEV
  pinSupport[1]=true;
  for(int i=5;i<=17;i++)
    pinSupport[i]=true;
  for(int i=19;i<=21;i++)
    pinSupport[i]=true;
  for(int i=36;i<=38;i++)
    pinSupport[i]=true;
  pinSupport[47]=true;
  pinSupport[48]=true;
# else
   pinSupport[2]=true;
   pinSupport[4]=true;
   pinSupport[5]=true;
   for(int i=12;i<=23;i++)
     pinSupport[i]=true;
   for(int i=25;i<=27;i++)
     pinSupport[i]=true;
   for(int i=32;i<=36;i++)
     pinSupport[i]=true;
   pinSupport[39]=true;
# endif
#else
  pinSupport[0]=true;
  pinSupport[2]=true;
  if((ESP.getFlashChipRealSize()/1024)>=4096) // assume this is a strykelink/esp12e
  {
    pinSupport[4]=true;
    pinSupport[5]=true;
    for(int i=9;i<=16;i++)
      pinSupport[i]=true;
    pinSupport[11]=false;
  }
#endif
#ifdef DEFAULT_PIN_OPB
  static bool OPB_stat=0;
  pinMode(DEFAULT_PIN_OPB, INPUT);
  OPB_stat=digitalRead(DEFAULT_PIN_OPB);
#endif

  debugPrintf("Zimodem %s firmware starting initialization\r\n",ZIMODEM_VERSION);
  initSDShell();
  currMode = &commandMode;
  if(!SPIFFS.begin())
  {
    SPIFFS.format();
    SPIFFS.begin();
    debugPrintf("SPIFFS Formatted.\r\n");
  }
# ifdef DEFAULT_PIN_RXD
    HWSerial.begin(DEFAULT_BAUD_RATE, DEFAULT_SERIAL_CONFIG, DEFAULT_PIN_RXD, DEFAULT_PIN_TXD);
# else
    HWSerial.begin(DEFAULT_BAUD_RATE, DEFAULT_SERIAL_CONFIG);  //Start Serial
# endif
#ifdef ZIMODEM_ESP8266
  HWSerial.setRxBufferSize(1024);
#endif
  commandMode.loadConfig();
  PhoneBookEntry::loadPhonebook();
  dcdStatus = dcdInactive;
  s_pinWrite(pinDCD,dcdStatus);
  flushSerial();
#ifdef SUPPORT_LED_PINS
  s_pinWrite(DEFAULT_PIN_WIFI,(WiFi.status() == WL_CONNECTED)?DEFAULT_WIFI_ACTIVE:DEFAULT_WIFI_INACTIVE);
  s_pinWrite(DEFAULT_PIN_HS,(baudRate>=DEFAULT_HS_BAUD)?DEFAULT_HS_ACTIVE:DEFAULT_HS_INACTIVE);
#endif
}

void checkReconnect()
{
  if((WiFi.status() != WL_CONNECTED)
  &&(nextReconnectDelay>0)
  &&(lastConnectAttempt>0)
  &&(wifiSSI.length()>0))
  {
     unsigned long now=millis();
     if(lastConnectAttempt > now)
       lastConnectAttempt=1;
     if(now > lastConnectAttempt + nextReconnectDelay)
     {
        debugPrintf("Attempting Reconnect to %s\r\n",wifiSSI.c_str());
        unsigned long oldReconnectDelay = nextReconnectDelay;
        if(!connectWifi(wifiSSI.c_str(),wifiPW.c_str(),staticIP,staticDNS,staticGW,staticSN))
          debugPrintf("Unable to reconnect to %s.\r\n",wifiSSI.c_str());
        nextReconnectDelay = oldReconnectDelay * 2;
        if(nextReconnectDelay > MAX_RECONNECT_DELAY)
          nextReconnectDelay = DEFAULT_RECONNECT_DELAY;
     }
  }
}

void checkFactoryReset()
{
#ifdef ZIMODEM_ESP32
    if(!digitalRead(PIN_FACTORY_RESET))
    {
      if(resetPushTimer != 1)
      {
        if(resetPushTimer==0)
        {
          resetPushTimer=millis();
          if(resetPushTimer==1)
            resetPushTimer++;
        }
        else
        if((millis() - resetPushTimer) > 5000)
        {
          SPIFFS.remove(CONFIG_FILE);
          SPIFFS.remove(CONFIG_FILE_OLD);
          SPIFFS.remove("/zphonebook.txt");
          SPIFFS.remove("/zlisteners.txt");
          SPIFFS.remove("/znick.txt");
          PhoneBookEntry::clearPhonebook();
          SPIFFS.end();
          SPIFFS.format();
          SPIFFS.begin();
          PhoneBookEntry::clearPhonebook();
          if(WiFi.status() == WL_CONNECTED)
            WiFi.disconnect();
          baudRate = DEFAULT_BAUD_RATE;
          commandMode.loadConfig();
          PhoneBookEntry::loadPhonebook();
          dcdStatus = dcdInactive;
          s_pinWrite(pinDCD,dcdStatus);
          wifiSSI="";
          delay(500);
          zclock.reset();
          commandMode.reset();
          resetPushTimer=1;
        }
      }
    }
    else
    if(resetPushTimer != 0)
      resetPushTimer=0;
#endif
}

void loop() 
{
  checkFactoryReset();
  checkReconnect();
  if(HWSerial.available())
  {
    currMode->serialIncoming();
  }
  currMode->loop();
  zclock.tick();
}
