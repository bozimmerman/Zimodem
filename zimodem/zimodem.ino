#include <ESP8266WiFi.h>
#include <FS.h>
#include <spiffs/spiffs.h>

#define ZIMODEM_VERSION "0.1"
#define null 0

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

void resetMode();
class WiFiClientNode;
static WiFiClientNode *conns = null;
static int WiFiNextClientId = 0;

class WiFiClientNode
{
  public:
    int id=0;
    char *host;
    int port;
    bool wasConnected=false;
    WiFiClient *client;
    WiFiClientNode *next = null;
    WiFiClientNode(char *hostIp, int newport)
    {
      port=newport;
      host=new char[strlen(hostIp)+1];
      strcpy(host,hostIp);
      id=++WiFiNextClientId;
      client = new WiFiClient();
      if(!client->connect(hostIp, port))
      {
        // deleted when it returns and is deleted
      }
      else
      {
        wasConnected=true;
        if(conns == null)
          conns = this;
        else
        {
          WiFiClientNode *last = conns;
          while(last->next != null)
            last = last->next;
          last->next = this;
        }
      }
    }
    ~WiFiClientNode()
    {
        if(client != null)
        {
          client->stop();
          delete client;
        }
        delete host;
        if(conns == null)
        {
        }
        else
        if(conns == this)
          conns = next;
        else
        {
          WiFiClientNode *last = conns;
          while(last->next != this)
            last = last->next;
          if(last != null)
            last->next = next;
        }
    }
    bool isConnected()
    {
      return (client != null) && (client->connected());
    }
};

byte CRC8(const byte *data, byte len) 
{
  byte crc = 0x00;
  while (len--) 
  {
    byte extract = *data++;
    for (byte tempI = 8; tempI; tempI--) 
    {
      byte sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) {
        crc ^= 0x8C;
      }
      extract >>= 1;
    }
  }
  return crc;
}    

static ZMode *currMode = null;

const int MAX_COMMAND_SIZE=256;
  
class ZStream : public ZMode
{
  WiFiClientNode *current = null;
  unsigned long currentExpires = 0;
  bool disconnectOnExit=true;
  int plussesInARow=0;
  bool echo=false;
  bool petscii=false;
  
  public:
    ZStream() : ZMode()
    {
    }
    
    void reset(WiFiClientNode *conn, bool dodisconnect, bool doPETSCII, bool doecho)
    {
      current = conn;
      currentExpires = 0;
      disconnectOnExit=dodisconnect;
      plussesInARow=0;
      echo=doecho;
      petscii=doPETSCII;
    }
    
    void serialIncoming()
    {
        while(Serial.available()>0)
        {
            uint8_t c=Serial.read();
            if(c=='+')
              plussesInARow++;
            else
              plussesInARow=0;
            if(c>0)
            {
              if(echo)
                Serial.write(c);
              if(current->isConnected())
                current->client->write(c);
            }
        }
        currentExpires=0;
        if(plussesInARow==3)
          currentExpires=millis()+1000;
    }
    void loop()
    {
      if((current==null)||(!current->isConnected()))
      {
        Serial.println("NOCARRIER");
        resetMode();
      }
      else
      if((currentExpires > 0) && (millis() > currentExpires))
      {
        currentExpires = 0;
        if(plussesInARow == 3)
        {
          plussesInARow=0;
          if(current != 0)
          {
            if(disconnectOnExit)
            {
              Serial.printf("NOCARRIER %d %s:%d\r\n",current->id,current->host,current->port);
              delete current;
            }
            Serial.println("READY.");
            current = null;
            resetMode();
          }
        }
      }
      else
      {
        while(current->client->available()>0)
        {
          int maxBytes=4096;
          if(current->client->available()<maxBytes)
            maxBytes=current->client->available();
          uint8_t buf[maxBytes];
          current->client->read(buf,maxBytes);
          Serial.write(buf,maxBytes);
          Serial.flush();
        }
      }
    }
};

static ZStream  *streamMode = null;

class ZCommand : public ZMode
{
  uint8_t nbuf[MAX_COMMAND_SIZE];
  int eon=0;
  WiFiClientNode *current = null;
  int XON=99;
  unsigned long currentExpires = 0;
  
  private:
    void reset()
    {
      while(conns != null)
      {
        WiFiClientNode *c=conns;
        delete c;
      }
      echoOn=false;
      eon=0;
      XON=99;
      memset(nbuf,0,MAX_COMMAND_SIZE);
    }
  
  public:
    boolean echoOn=false;
    String wifiSSI;
    String wifiPW;
    int baudRate=115200;
  
    ZCommand() : ZMode()
    {
      reset();
    }
    void serialIncoming()
    {
      bool crReceived=false;
      while(Serial.available()>0)
      {
        uint8_t c=Serial.read();
        if((c=='\r')||(c=='\n'))
        {
          if(eon == 0)
            continue;
          else
          {
            if(echoOn)
              Serial.write("\n\r");
            crReceived=true;
            break;
          }
        }
        
        if(c>0)
        {
          if(echoOn)
            Serial.write(c);
          if((c==8)||(c==20)||(c==127))
          {
            if(eon>0)
              nbuf[--eon]=0;
            continue;
          }
          nbuf[eon++]=c;
          if(eon>=MAX_COMMAND_SIZE)
            crReceived=true;
        }
      }
      if(currentExpires > 0)
        currentExpires = 0;
      if(strcmp((char *)nbuf,"+++")==0)
      {
        currentExpires = millis() + 1000;
      }
      
      if(!crReceived)
        return;
      int len=eon;
      uint8_t sbuf[len];
      memcpy(sbuf,nbuf,len);
      memset(nbuf,0,MAX_COMMAND_SIZE);
      eon=0;
      if((len>1)
      &&((sbuf[0]=='A')||(sbuf[0]=='a'))
      &&((sbuf[1]=='T')||(sbuf[1]=='t')))
      {
        if(len == 2)
          Serial.println("OK");
        else
        {
          int index=2;
          char lastCmd=' ';
          int result=0;
          int vstart=0;
          int vlen=0;
          String dmodifiers="";
          while(index<len)
          {
            lastCmd=sbuf[index++];
            vstart=index;
            vlen=0;
            bool isNumber=true;
            if(index<len)
            {
              if(sbuf[index]=='\"')
              {
                isNumber=false;
                vstart++;
                while((++index<len)
                &&((sbuf[index]!='\"')||(sbuf[index-1]=='\\')))
                  vlen++;
              }
              else
              if((lastCmd=='d')||(lastCmd=='D'))
              {
                isNumber=false;
                const char *DMODIFIERS="LPRTW,lprtw";
                while((index<len)&&(strchr(DMODIFIERS,sbuf[index])!=null))
                  dmodifiers += sbuf[index++];
                vlen += len-index;
                index=len;
              }
              else
              while((index<len)
              &&(! (((sbuf[index]>='a')&&(sbuf[index]<='z'))
                 ||((sbuf[index]>='A')&&(sbuf[index]<='Z')))))
              {
                isNumber = (sbuf[index]>='0') && (sbuf[index]<='9') && isNumber;
                vlen++;
                index++;
              }
            }
            int vval=0;
            uint8_t vbuf[vlen+1];
            memset(vbuf,0,vlen+1);
            if(vlen>0)
            {
              memcpy(vbuf,sbuf+vstart,vlen);
              if((vlen > 0)&&(isNumber))
                vval=atoi((char *)vbuf);
            }
            /*
             * We have cmd and args, time to DO!
             */
            switch(lastCmd)
            {
            case 'z':
            case 'Z':
            {
              reset();
              Serial.print("\r\nZiModem v");
              Serial.println(ZIMODEM_VERSION);
              Serial.printf("sdk=%s core=%s cpu@%d\r\n",ESP.getSdkVersion(),ESP.getCoreVersion().c_str(),ESP.getCpuFreqMHz());
              Serial.printf("flash chipid=%d size=%d rsize=%d speed=%d\r\n",ESP.getFlashChipId(),ESP.getFlashChipSize(),ESP.getFlashChipRealSize(),ESP.getFlashChipSpeed());
              break;
            }
            case 'a':
            case 'A':
              //TODO accept a connection on a port
              break;
            case 'e':
            case 'E':
              echoOn=(vval != 0);
              break;
            case 'x':
            case 'X':
              XON=vval;
              break;
            case 'b':
            case 'B':
              if(vval<=0)
                result=1;
              else
              {
                  baudRate=vval;
                  Serial.begin(baudRate);
                  File f = SPIFFS.open("/zconfig.txt", "w");
                  f.printf("%s,%s,%d",wifiSSI.c_str(),wifiPW.c_str(),baudRate);
                  f.close();
              }
              break;
            case 't':
            case 'T':
              if((vlen==0)||(current==null)||(!current->isConnected()))
                result=1;
              else
              if(vval>0)
              {
                uint8_t buf[vval];
                int recvd = Serial.readBytes(buf,vval);
                if(recvd == vval)
                  current->client->write(buf,recvd);
                else
                  result=1;
              }
              else
              {
                current->client->write(vbuf,vlen);
                current->client->write("\n\r",2);
              }
            case 'd':
            case 'D':
              if(vlen == 0)
              {
                if((current == null)||(!current->isConnected()))
                  result=1;
                else
                {
                  currMode=streamMode;
                  bool doPETSCII = strchr(dmodifiers.c_str(),'P')||strchr(dmodifiers.c_str(),'p');
                  streamMode->reset(current,false,doPETSCII,echoOn);
                }
              }
              else
              if((vval >= 0)&&(isNumber))
              {
                WiFiClientNode *c=conns;
                while((c!=null)&&(c->id != vval))
                  c=c->next;
                if((c!=null)&&(c->id == vval)&&(c->isConnected()))
                {
                  currMode=streamMode;
                  bool doPETSCII = strchr(dmodifiers.c_str(),'P')||strchr(dmodifiers.c_str(),'p');
                  streamMode->reset(current,false,doPETSCII,echoOn);
                }
                else
                  result=1;
              }
              else
              {
                char *colon=strstr((char *)vbuf,":");
                int port=23;
                if(colon != null)
                {
                  (*colon)=0;
                  port=atoi((char *)(++colon));
                }
                WiFiClientNode *c = new WiFiClientNode((char *)vbuf,port);
                if(!c->isConnected())
                {
                  delete c;
                  result=1;
                }
                else
                {
                  currMode=streamMode;
                  bool doPETSCII = strchr(dmodifiers.c_str(),'P')||strchr(dmodifiers.c_str(),'p');
                  streamMode->reset(current,true,doPETSCII,echoOn);
                }
              }
              break;
            case 'o':
            case 'O':
            case 'c':
            case 'C':
              if(vlen == 0)
              {
                if(current == null)
                  result=1;
                else
                {
                  result=99;
                  if(current->isConnected())
                    Serial.printf("CONNECTED %d %s:%d\r\n",current->id,current->host,current->port);
                  else
                    Serial.printf("NOCARRIER %d %s:%d\r\n",current->id,current->host,current->port);
                }
              }
              else
              if((vval >= 0)&&(isNumber))
              {
                WiFiClientNode *c=conns;
                while((c!=null)&&(c->id != vval))
                  c=c->next;
                if((c!=null)&&(c->id == vval))
                  current=c;
                else
                {
                  c=conns;
                  while(c!=null)
                  {
                    if(current->isConnected())
                      Serial.printf("CONNECTED %d %s:%d\r\n",c->id,c->host,c->port);
                    else
                      Serial.printf("NOCARRIER %d %s:%d\r\n",c->id,c->host,c->port);
                    c=c->next;
                  }
                  result=1;
                }
              }
              else
              {
                char *colon=strstr((char *)vbuf,":");
                int port=23;
                if(colon != null)
                {
                  (*colon)=0;
                  port=atoi((char *)(++colon));
                }
                WiFiClientNode *c = new WiFiClientNode((char *)vbuf,port);
                if(!c->isConnected())
                {
                  delete c;
                  result=1;
                }
                else
                  current=c;
              }
              break;
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
                    wifiSSI=ssi;
                    wifiPW=pw;
                    f.printf("%s,%s,%d",ssi,pw,baudRate);
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

    void loop()
    {
      if((currentExpires > 0) && (millis() > currentExpires))
      {
        currentExpires = 0;
        if(strcmp((char *)nbuf,"+++")==0)
        {
          if(current != 0)
          {
            Serial.printf("NOCARRIER %d %s:%d\r\n",current->id,current->host,current->port);
            delete current;
            current = conns;
          }
          memset(nbuf,0,MAX_COMMAND_SIZE);
          eon=0;
        }
      }
      
      if(XON>0)
      {
        WiFiClientNode *curr=conns;
        while((XON>0)&&(curr != null))
        {
          if(!curr->isConnected())
          {
            if(curr->wasConnected)
            {
              Serial.printf("NOCARRIER %d\r\n",curr->id);
              curr->wasConnected=false;
            }
          }
          else
          if(curr->client->available()>0)
          {
            if(XON < 10)
              XON--;
            int maxBytes=256;
            if(curr->client->available()<maxBytes)
              maxBytes=curr->client->available();
            uint8_t buf[maxBytes];
            curr->client->read(buf,maxBytes);
            uint8_t crc=CRC8(buf,maxBytes);
            Serial.printf("[ %d %d %d ]\r\n",curr->id,maxBytes,(int)crc);
            Serial.write(buf,maxBytes);
            Serial.flush();
          }
          curr = curr->next;
        }
      } //TODO: consider local buffering with XOFF, until then, trust the socket buffers.
    }
    
};

static ZCommand *commandMode = null;

void resetMode()
{
  currMode = commandMode;
}

void setup() 
{
  delay(10);
  commandMode = new ZCommand();
  streamMode = new ZStream();
  resetMode();
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
    commandMode->baudRate=atoi(argv[2].c_str());
  if(commandMode->baudRate <= 0)
    commandMode->baudRate=115200;
  Serial.begin(commandMode->baudRate);  //Start Serial
  Serial.print("\r\nZiModem v");
  Serial.setTimeout(60000);
  Serial.println(ZIMODEM_VERSION);
  Serial.printf("sdk=%s core=%s cpu@%d\r\n",ESP.getSdkVersion(),ESP.getCoreVersion().c_str(),ESP.getCpuFreqMHz());
  Serial.printf("flash chipid=%d size=%d rsize=%d speed=%d\r\n",ESP.getFlashChipId(),ESP.getFlashChipSize(),ESP.getFlashChipRealSize(),ESP.getFlashChipSpeed());
  if(argv[0].length()>0)
  {
    commandMode->wifiSSI=argv[0];
    commandMode->wifiPW=argv[1];
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
