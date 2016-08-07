#include <ESP8266WiFi.h>
#include <FS.h>
#include <spiffs/spiffs.h>

#define null 0
#define ZIMODEM_VERSION "0.1"

#include "pet2asc.h"
#include "zmode.h"
#include "wificlientnode.h"
#include "zstream.h"
#include "zcommand.h"

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

static WiFiClientNode *conns = null;
//static WiFiServerNode *servs = null;
static ZMode *currMode = null;
static ZStream streamMode;
static ZCommand commandMode;

void setup() 
{
  delay(10);
  currMode = &commandMode;
  SPIFFS.begin();
  String msg;
  String argv[10];
  if(!SPIFFS.exists("/zconfig.txt"))
  {
    msg="INITAILIZED\r\n";
    SPIFFS.format();
    File f = SPIFFS.open("/zconfig.txt", "w");
    f.printf(",,115200");
    f.close();
  }
  else
  {
    File f = SPIFFS.open("/zconfig.txt", "r");
    String str=f.readString();
    f.close();
    if((str!=null)&&(str.length()>0))
    {
      int argn=0;
      for(int i=0;i<str.length();i++)
      {
        if((str[i]==',')&&(argn<9))
          argn++;
        else
          argv[argn] += str[i];
      }
    }
  }
  if(argv[2].length()>0)
    commandMode.baudRate=atoi(argv[2].c_str());
  if(commandMode.baudRate <= 0)
    commandMode.baudRate=115200;
  Serial.begin(commandMode.baudRate);  //Start Serial
  Serial.print("\r\nZiModem v");
  Serial.setTimeout(60000);
  Serial.println(ZIMODEM_VERSION);
  Serial.printf("sdk=%s core=%s cpu@%d\r\n",ESP.getSdkVersion(),ESP.getCoreVersion().c_str(),ESP.getCpuFreqMHz());
  Serial.printf("flash chipid=%d size=%d rsize=%d speed=%d\r\n",ESP.getFlashChipId(),ESP.getFlashChipSize(),ESP.getFlashChipRealSize(),ESP.getFlashChipSpeed());
  if(argv[0].length()>0)
  {
    commandMode.wifiSSI=argv[0];
    commandMode.wifiPW=argv[1];
    if(connectWifi(argv[0].c_str(),argv[1].c_str()))
      msg = "CONNECTED TO " + argv[0] + " (" + WiFi.localIP().toString().c_str() + ")\r\n";
    else
      msg = "ERROR ON " + argv[0] + "\r\n";
  }
  Serial.print(msg);
  Serial.println("READY.");
}

void loop() 
{
  if(Serial.available())
  {
    currMode->serialIncoming();
  }
  currMode->loop();
}
