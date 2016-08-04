#include <ESP8266WiFi.h>
#include <FS.h>
#include <spiffs/spiffs.h>

#define ZIMODEM_VERSION "0.1"

class ZMode
{
  public:
    virtual void serialIncoming();
    virtual void loop();
};

bool connectWifi(const char* ssid, const char* password) 
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

class ZCommand : public ZMode
{
  const int MAX_COMMAND_SIZE=256;
  
  public:
    void serialIncoming()
    {
      uint8_t sbuf[MAX_COMMAND_SIZE];
      memset(sbuf,0,MAX_COMMAND_SIZE);
      int n = Serial.readBytesUntil('\n',sbuf, MAX_COMMAND_SIZE);
      if((n > 0)&&(sbuf[n-1]=='\r'))
        sbuf[--n]=0;
      if(n > 0)
      {
        if((n>1)
        &&((sbuf[0]=='A')||(sbuf[0]=='a'))
        &&((sbuf[1]=='T')||(sbuf[1]=='t')))
        {
          if(n == 2)
            Serial.println("OK");
          else
          {
            int index=2;
            char lastCmd=' ';
            int result=0;
            int vstart=0;
            int vlen=0;
            while(index<n)
            {
              lastCmd=sbuf[index++];
              vstart=index;
              vlen=0;
              if(index<n)
              {
                if(sbuf[index]=='\"')
                {
                  vstart++;
                  while((++index<n)&&(sbuf[index]!='\"'))
                    vlen++;
                }
                else
                while((index<n)
                &&(! (((sbuf[index]>='a')&&(sbuf[index]<='z'))
                   ||((sbuf[index]>='A')&&(sbuf[index]<='Z')))))
                {
                  vlen++;
                  index++;
                }
              }
              int vval=0;
              uint8_t vbuf[vlen+1];
              memset(vbuf,0,vlen+1);
              if(vlen > 0)
              {
                memcpy(vbuf,sbuf+vstart,vlen);
                vval=atoi((char *)vbuf);
              }
              switch(lastCmd)
              {
              case 'w':
              case 'W':
              {
                if((vlen==0)||(vval>0))
                {
                  int n = WiFi.scanNetworks();
                  if((vval > 0)&&(vval < n))
                    n=vval;
                  for (int i = 0; i < n; ++i)
                  {
                    Serial.print(WiFi.SSID(i));
                    Serial.print(" (");
                    Serial.print(WiFi.RSSI(i));
                    Serial.print(")");
                    Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
                    delay(10);
                  }
                }
                else
                {
                  char *x=strstr((char *)vbuf,",");
                  if(x <= 0)
                    result=1;
                  else
                  {
                    *x=0;
                    char *ssi=(char *)vbuf;
                    char *pw=x+1;
                    if(!connectWifi(ssi,pw))
                      result=1;
                    else
                    {
                      File f = SPIFFS.open("/zconfig.txt", "w");
                      f.print(ssi);
                      f.print(",");
                      f.print(pw);
                      f.close();
                    }
                  }
                }
                break;
              }
              default:
                break;
              }
            }
            switch(result)
            {
            case 0:
              Serial.println("OK");
              break;
            case 1:
              Serial.println("ERROR");
              break;
            default:
              break;
            }
          }
        }
        else
          Serial.println("ERROR");
      }
    }

    void loop()
    {
      
    }
};

ZCommand commandMode;

ZMode *currMode = &commandMode;

void setup() 
{
  Serial.begin(115200);  //Start Serial
  delay(10);
  Serial.print("\r\nZiModem v");
  Serial.println(ZIMODEM_VERSION);
  Serial.printf("sdk=%s core=%s cpu@%d\r\n",ESP.getSdkVersion(),ESP.getCoreVersion().c_str(),ESP.getCpuFreqMHz());
  Serial.printf("flash chipid=%d size=%d rsize=%d speed=%d\r\n",ESP.getFlashChipId(),ESP.getFlashChipSize(),ESP.getFlashChipRealSize(),ESP.getFlashChipSpeed());
  SPIFFS.begin();
  if(!SPIFFS.exists("/zconfig.txt"))
  {
    Serial.println("Initializing...");
    SPIFFS.format();
    File f = SPIFFS.open("/zconfig.txt", "w");
    f.println("");
    f.println("");
    f.close();
  }
  else
  {
    File f = SPIFFS.open("/zconfig.txt", "r");
    String str=f.readString();
    f.close();
    if((str!=0)&&(str.length()>0))
    {
      char ssi[str.length()+1];
      strcpy(ssi,str.c_str());
      char *pw=strstr(ssi,",");
      if(pw>ssi)
      {
        *pw=0;
        pw++;
        if(connectWifi(ssi,pw))
          Serial.printf("CONNECTED TO %s (%s)\r\n",ssi,WiFi.localIP().toString().c_str());
        else
          Serial.printf("ERROR ON %s\r\n",ssi);
      }
    }
  }
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
