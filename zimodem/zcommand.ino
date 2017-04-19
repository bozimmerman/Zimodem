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
extern "C" void esp_schedule();
extern "C" void esp_yield();

ZCommand::ZCommand()
{
  freeCharArray(&tempMaskOuts);
  freeCharArray(&tempDelimiters);
  setCharArray(&delimiters,"");
  setCharArray(&maskOuts,"");
}

void ZCommand::Serialprint(const char *expr)
{
  if(!petsciiMode)
    Serial.print(expr);
  else
  {
    for(int i=0;expr[i]!=0;i++)
    {
      Serial.write(ascToPetcii(expr[i]));
    }
  }
}

byte ZCommand::CRC8(const byte *data, byte len) 
{
  byte crc = 0x00;
  if(logFileOpen)
      logFile.print("CRC8: ");
  int c=0;
  while (len--) 
  {
    byte extract = *data++;
    if(logFileOpen)
    {
        logFile.print(TOHEX(extract));
        if((++c)>20)
        {
          logFile.print("\r\ncrc8: ");
          c=0;
        }
        else
          logFile.print(" ");
    }
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
  if(logFileOpen)
    logFile.printf("\r\nFinal CRC8: %s\r\n",TOHEX(crc));
  return crc;
}

int ZCommand::makeStreamFlagsBitmap(const char *dmodifiers)
{
    int flagsBitmap = 0;
    if((strchr(dmodifiers,'p')!=null) || (strchr(dmodifiers,'P')!=null))
      flagsBitmap = flagsBitmap | FLAG_PETSCII;
    if((strchr(dmodifiers,'t')!=null) || (strchr(dmodifiers,'T')!=null))
      flagsBitmap = flagsBitmap | FLAG_TELNET;
    if((strchr(dmodifiers,'e')!=null) || (strchr(dmodifiers,'E')!=null))
      flagsBitmap = flagsBitmap | FLAG_ECHO;
    if((strchr(dmodifiers,'x')!=null) || (strchr(dmodifiers,'X')!=null))
      flagsBitmap = flagsBitmap | FLAG_XONXOFF;
    return flagsBitmap;
}  

void ZCommand::setConfigDefaults()
{
  doEcho=true;
  flowControlType=FCT_DISABLED;
  binType=BTYPE_NORMAL;
  XON=true;
  petsciiMode=false;
  delayMs=0;
  DCD_HIGH=HIGH;
  DCD_LOW=LOW;
  suppressResponses=false;
  numericResponses=false;
  longResponses=true;
  packetSize=127;
  strcpy(CRLF,"\r\n");
  strcpy(LFCR,"\n\r");
  strcpy(LF,"\n");
  strcpy(CR,"\r");
  EC='+';
  strcpy(ECS,"+++");
  BS=8;
  EOLN = CRLF;
  tempBaud = -1;
  freeCharArray(&tempMaskOuts);
  freeCharArray(&tempDelimiters);
  setCharArray(&delimiters,"");
  setCharArray(&maskOuts,"");
}

char lc(char c)
{
  if((c>=65) && (c<=90))
    return c+32;
  if((c>=193) && (c<=218))
    return c-96;
  return c;
}

ZResult ZCommand::doResetCommand()
{
  while(conns != null)
  {
    WiFiClientNode *c=conns;
    delete c;
  }
  current = null;
  nextConn = null;
  while(servs != null)
  {
    WiFiServerNode *s=servs;
    delete s;
  }
  setConfigDefaults();
  String argv[CFG_LAST+1];
  parseConfigOptions(argv);
  eon=0;
  XON=true;
  petsciiMode=false;
  delayMs=0;
  binType=BTYPE_NORMAL;
  flowControlType=FCT_DISABLED;
  setBaseConfigOptions(argv);
  memset(nbuf,0,MAX_COMMAND_SIZE);
  return ZOK;
}

ZResult ZCommand::doNoListenCommand()
{
  /*
  WiFiClientNode *c=conns;
  while(c != null)
  {
    WiFiClientNode *c2=c->next;
    if(c->serverClient)
      delete c;
    c=c2;
  }
  */
  while(servs != null)
  {
    WiFiServerNode *s=servs;
    delete s;
  }
  return ZOK;
}

void ZCommand::reSaveConfig()
{
  SPIFFS.remove("/zconfig.txt");
  delay(500);
  File f = SPIFFS.open("/zconfig.txt", "w");
  const char *eoln = EOLN.c_str();
  int dcdMode = (DCD_HIGH == HIGH) ? 0 : 1;
  f.printf("%s,%s,%d,%s,%d,%d,%d,%d,%d,%d,%d",wifiSSI.c_str(),wifiPW.c_str(),baudRate,eoln,flowControlType,doEcho,suppressResponses,numericResponses,longResponses,petsciiMode,dcdMode);
  f.close();
  delay(500);
  if(SPIFFS.exists("/zconfig.txt"))
  {
    File f = SPIFFS.open("/zconfig.txt", "r");
    String str=f.readString();
    f.close();
    int argn=0;
    if((str!=null)&&(str.length()>0))
    {
      for(int i=0;i<str.length();i++)
      {
        if((str[i]==',')&&(argn<=CFG_LAST))
          argn++;
      }
    }
    if(argn!=CFG_LAST)
    {
      delay(100);
      reSaveConfig();
    }
  }
}

void ZCommand::setBaseConfigOptions(String configArguments[])
{
  if(configArguments[CFG_EOLN].length()>0)
  {
    EOLN = configArguments[CFG_EOLN];
  }
  if(configArguments[CFG_FLOWCONTROL].length()>0)
  {
    int x = atoi(configArguments[CFG_FLOWCONTROL].c_str());
    if((x>=0)&&(x<FCT_INVALID))
      flowControlType=(FlowControlType)x;
    else
      x=FCT_DISABLED;
    XON=true;
    if(flowControlType == FCT_MANUAL)
      XON=false;
  }
  if(configArguments[CFG_ECHO].length()>0)
    doEcho = atoi(configArguments[CFG_ECHO].c_str());
  if(configArguments[CFG_RESP_SUPP].length()>0)
    suppressResponses = atoi(configArguments[CFG_RESP_SUPP].c_str());
  if(configArguments[CFG_RESP_NUM].length()>0)
    numericResponses = atoi(configArguments[CFG_RESP_NUM].c_str());
  if(configArguments[CFG_RESP_LONG].length()>0)
    longResponses = atoi(configArguments[CFG_RESP_LONG].c_str());
  if(configArguments[CFG_PETSCIIMODE].length()>0)
    petsciiMode = atoi(configArguments[CFG_PETSCIIMODE].c_str());
  if(configArguments[CFG_DCDMODE].length()>0)
  {
    int dcdMode = atoi(configArguments[CFG_DCDMODE].c_str());
    if(dcdMode == 1)
    {
      DCD_HIGH=LOW;
      DCD_LOW=HIGH;
    }
    else
    {
      DCD_HIGH=HIGH;
      DCD_LOW=LOW;
    }
  }
}

void ZCommand::parseConfigOptions(String configArguments[])
{
  delay(500);
  File f = SPIFFS.open("/zconfig.txt", "r");
  String str=f.readString();
  f.close();
  if((str!=null)&&(str.length()>0))
  {
    int argn=0;
    for(int i=0;i<str.length();i++)
    {
      if((str[i]==',')&&(argn<=CFG_LAST))
        argn++;
      else
        configArguments[argn] += str[i];
    }
  }
}

void ZCommand::loadConfig()
{
  wifiConnected=false;
  WiFi.disconnect();
  setConfigDefaults();
  if(!SPIFFS.exists("/zconfig.txt"))
  {
    SPIFFS.format();
    reSaveConfig();
    Serial.begin(1200);  //Start Serial
  }
  String argv[CFG_LAST+1];
  parseConfigOptions(argv);
  if(argv[CFG_BAUDRATE].length()>0)
    baudRate=atoi(argv[CFG_BAUDRATE].c_str());
  if(baudRate <= 0)
    baudRate=1200;
  Serial.begin(baudRate);  //Start Serial
  writeClear=Serial.availableForWrite();
  wifiSSI=argv[CFG_WIFISSI];
  wifiPW=argv[CFG_WIFIPW];
  if(wifiSSI.length()>0)
  {
    connectWifi(wifiSSI.c_str(),wifiPW.c_str());
  }
  doResetCommand();
  showInitMessage();
}

ZResult ZCommand::doInfoCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  if(vval == 0)
  {
    showInitMessage();
  }
  else
  if(vval == 1)
  {
    Serialprint("AT");
    Serialprint("B");
    Serial.print(baudRate);
    Serialprint(doEcho?"E1":"E0");
    if(suppressResponses)
      Serialprint("Q1");
    else
    {
      Serialprint("Q0");
      Serialprint(numericResponses?"V0":"V1");
      Serialprint(longResponses?"X1":"X0");
    }
    switch(flowControlType)
    {
    case FCT_DISABLED:
      Serialprint("F0");
      break;
    case FCT_NORMAL: 
      Serialprint("F1");
      break;
    case FCT_AUTOOFF:
      Serialprint("F2");
      break;
    case FCT_MANUAL:
      Serialprint("F3");
      break;
    }
    if(EOLN==CR)
      Serialprint("R0");
    else
    if(EOLN==CRLF)
      Serialprint("R1");
    else
    if(EOLN==LFCR)
      Serialprint("R2");
    else
    if(EOLN==LF)
      Serialprint("R3");
  
    if((delimiters != NULL)&&(delimiters[0]!=0))
      for(int i=0;i<strlen(delimiters);i++)
        Serial.printf("&d%d",delimiters[i]);
    if((maskOuts != NULL)&&(maskOuts[0]!=0))
      for(int i=0;i<strlen(maskOuts);i++)
        Serial.printf("&m%d",maskOuts[i]);

    Serialprint("S0=");
    Serial.print((int)ringCounter);
    Serialprint("S2=");
    Serial.print((int)EC);
    Serialprint("S3=");
    Serial.print((int)CR[0]);
    Serialprint("S4=");
    Serial.print((int)LF[0]);
    Serialprint("S5=");
    Serial.print((int)BS);
    Serialprint("S40=");
    Serial.print(packetSize);
    Serialprint(autoStreamMode ? "S41=1" : "S41=0");
    
    WiFiServerNode *serv = servs;
    while(serv != null)
    {
      Serialprint("A");
      Serial.print(serv->port);
      serv=serv->next;
    }
    if(tempBaud > 0)
    {
      Serialprint("S43=");
      Serial.print(tempBaud);
    }
    if(delayMs > 0)
    {
      Serialprint("S44=");
      Serial.print(delayMs);
    }
    if(binType > 0)
    {
      Serialprint("S45=");
      Serial.print(binType);
    }
    if(DCD_HIGH != HIGH)
      Serialprint("S46=1");
    Serialprint(petsciiMode ? "&P1" : "&P0");
    Serial.print(EOLN);
  }
  else
  if(vval == 2)
  {
    Serialprint(WiFi.localIP().toString().c_str());
    Serial.print(EOLN);
  }
  else
  if(vval == 3)
  {
    Serialprint(wifiSSI.c_str());
    Serial.print(EOLN);
  }
  else
  if(vval == 4)
  {
    Serialprint(ZIMODEM_VERSION);
    Serial.print(EOLN);
  }
  else
    return ZERROR;
  return ZOK;
}

ZResult ZCommand::doBaudCommand(int vval, uint8_t *vbuf, int vlen)
{
  if(vval<=0)
    return ZERROR;
  else
  {
    baudRate=vval;
    Serial.flush();
    Serial.begin(baudRate);
  }
  return ZOK;
}

ZResult ZCommand::doConnectCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers)
{
  if(vlen == 0)
  {
    if(logFileOpen)
      logFile.printf("ConnCheck: CURRENT\r\n");
    if(strlen(dmodifiers)>0)
      return ZERROR;
    if(current == null)
      return ZERROR;
    else
    {
      if(current->isConnected())
      {
        Serialprint("CONNECTED ");
        Serial.printf("%d %s:%d",current->id,current->host,current->port);
      }
      else
      {
        Serialprint("NO CARRIER ");
        Serial.printf("%d %s:%d",current->id,current->host,current->port);
      }
      Serial.print(EOLN);
      return ZIGNORE;
    }
  }
  else
  if((vval >= 0)&&(isNumber))
  {
    if(logFileOpen)
      if(vval == 0)
        logFile.printf("ConnList0:\r\n");
      else
        logFile.printf("ConnSwitchTo: %d\r\n",vval);
    if(strlen(dmodifiers)>0) // would be nice to allow petscii/telnet changes here, but need more flags
      return ZERROR;
    WiFiClientNode *c=conns;
    if(vval > 0)
    {
      while((c!=null)&&(c->id != vval))
        c=c->next;
      if((c!=null)&&(c->id == vval))
      {
        current = c;
        setCharArray(&(c->delimiters),tempDelimiters);
        setCharArray(&(c->maskOuts),tempMaskOuts);
        freeCharArray(&tempDelimiters);
        freeCharArray(&tempMaskOuts);
      }
      else
        return ZERROR;
    }
    else
    {
      c=conns;
      while(c!=null)
      {
        if(c->isConnected())
        {
          Serialprint("CONNECTED ");
          Serial.printf("%d %s:%d",c->id,c->host,c->port);
        }
        else
        {
          Serialprint("NO CARRIER ");
          Serial.printf("%d %s:%d",c->id,c->host,c->port);
        }
        Serial.print(EOLN);
        c=c->next;
      }
      WiFiServerNode *s=servs;
      while(s!=null)
      {
        Serialprint("LISTENING ");
        Serial.printf("%d *:%d",s->id,s->port);
        Serial.print(EOLN);
        s=s->next;
      }
    }
  }
  else
  {
    if(logFileOpen)
        logFile.printf("Connnect-Start:\r\n");
    char *colon=strstr((char *)vbuf,":");
    int port=23;
    if(colon != null)
    {
      (*colon)=0;
      port=atoi((char *)(++colon));
    }
    int flagsBitmap = makeStreamFlagsBitmap(dmodifiers);
    if(logFileOpen)
        logFile.printf("Connnecting: %s %d %d\r\n",(char *)vbuf,port,flagsBitmap);
    WiFiClientNode *c = new WiFiClientNode((char *)vbuf,port,flagsBitmap);
    if(!c->isConnected())
    {
      if(logFileOpen)
          logFile.printf("Connnect: FAIL\r\n");
      delete c;
      return ZERROR;
    }
    else
    {
      if(logFileOpen)
          logFile.printf("Connnect: SUCCESS: %d\r\n",c->id);
      current=c;
      setCharArray(&(c->delimiters),tempDelimiters);
      setCharArray(&(c->maskOuts),tempMaskOuts);
      freeCharArray(&tempDelimiters);
      freeCharArray(&tempMaskOuts);
      return ZCONNECT;
    }
  }
  return ZOK;
}

void ZCommand::headerOut(const int channel, const int sz, const int crc8)
{
  switch(binType)
  {
  case BTYPE_NORMAL:
    sprintf(hbuf,"[ %d %d %d ]%s",channel,sz,crc8,EOLN.c_str());
    break;
  case BTYPE_HEX:
    sprintf(hbuf,"[ %s %s %s ]%s",String(TOHEX(channel)).c_str(),String(TOHEX(sz)).c_str(),String(TOHEX(crc8)).c_str(),EOLN.c_str());
    break;
  case BTYPE_DEC:
    sprintf(hbuf,"[%s%d%s%d%s%d%s]%s",EOLN.c_str(),channel,EOLN.c_str(),sz,EOLN.c_str(),crc8,EOLN.c_str(),EOLN.c_str());
    break;
  }
  Serialprint(hbuf);
  if(logFileOpen)
      logFile.printf("%sSER-OUT: %s",EOLN.c_str(),hbuf);
}

bool ZCommand::doWebGetStream(const char *hostIp, int port, const char *req, WiFiClient &c, uint32_t *responseSize)
{
  *responseSize = 0;
  if(WiFi.status() != WL_CONNECTED)
    return false;
  c.setNoDelay(true);
  if(!c.connect(hostIp, port))
  {
    c.stop();
    return false;
  }
  c.printf("GET /%s HTTP/1.1\r\n",req);
  c.printf("User-Agent: C64Net Firmware\r\n",hostIp);
  c.printf("Host: %s\r\n",hostIp);
  c.printf("Connection: close\r\n\r\n");
  String ln = "";
  uint32_t respLength = 0;
  int respCode = -1;
  while(c.connected())
  {
    yield();
    if(c.available()<=0)
      continue;
      
    char ch = (char)c.read();
    if(ch == '\r')
      continue;
    else
    if(ch == '\n')
    {
      if(ln.length()==0)
        break;
      if(respCode < 0)
      {
        int sp = ln.indexOf(' ');
        if(sp<=0)
          break;
        ln.remove(0,sp+1);
        sp = ln.indexOf(' ');
        if(sp<=0)
          break;
        ln.remove(sp);
        respCode = atoi(ln.c_str());
      }
      else
      if(ln.startsWith("Content-length: ")
      ||ln.startsWith("Content-Length: "))
      {
        ln.remove(0,16);
        respLength = atoi(ln.c_str());
      }
      ln = "";
    }
    else
      ln.concat(ch);
  }
  *responseSize = respLength;
  if((!c.connected())
  ||(respCode != 200)
  ||(respLength <= 0))
  {
    c.stop();
    return false;
  }
  return true;
}

bool ZCommand::doWebGet(const char *hostIp, int port, const char *filename, const char *req)
{
  uint32_t respLength=0;
  WiFiClient c;
  if(!doWebGetStream(hostIp, port, req, c, &respLength))
    return false;
    
  File f = SPIFFS.open(filename, "w");
  while((respLength>0) && (c.connected()))
  {
    if(c.available()>=0)
    {
      f.write(c.read());
      respLength--;
    }
    else
      yield();
  }
  f.flush();
  f.close();
  c.stop();
  return (respLength == 0);
}

bool ZCommand::doWebGetBytes(const char *hostIp, int port, const char *req, uint8_t *buf, int *bufSize)
{
  WiFiClient c;
  uint32_t respLength=0;
  if(!doWebGetStream(hostIp, port, req, c, &respLength))
    return false;
  if((!c.connected())
  ||(respLength > *bufSize))
  {
    c.stop();
    return false;
  }
  *bufSize = (int)respLength;
  int index=0;
  while((respLength>0) && (c.connected()))
  {
    if(c.available()>=0)
    {
      buf[index++] = c.read();
      respLength--;
    }
    else
      yield();
  }
  c.stop();
  return (respLength == 0);
}

ZResult ZCommand::doWebStream(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *filename, bool cache)
{
  char *portB=strchr((char *)vbuf,':');
  bool success = true;
  if(portB == NULL)
     success = false;
  else
  {
    char *hostIp = (char *)vbuf;
    *portB = 0;
    portB++;
    char *req = strchr(portB,'/');
    if(req == NULL)
      success = false;
    else
    {
      *req = 0;
      req++;
      int port = atoi(portB);
      if(port <=0)
        success = false;
      else
      {
        if(cache)
        {
          if(!SPIFFS.exists(filename))
          {
            if(!doWebGet(hostIp, port, filename, req))
              return ZERROR;
          }
        }
        else
        if(!doWebGet(hostIp, port, filename, req))
          return ZERROR;
        int chk8=0;
        if(!cache)
        {
          File f = SPIFFS.open(filename, "r");
          int len = f.size();
          for(int i=0;i<len;i++)
          {
            chk8+=f.read();
            if(chk8>255)
              chk8-=256;
          }
          f.close();
        }
        File f = SPIFFS.open(filename, "r");
        int len = f.size();
        if(!cache)
          headerOut(0,len,chk8);
        bool flowControl=!cache;
        BinType streamType = cache?BTYPE_NORMAL:binType;
        if(flowControl)
        {
          XON=true;
          if(flowControlType == FCT_MANUAL)
            XON=false;
        }
        int mark = Serial.availableForWrite();
        int bct=0;
        int lct=0;
        if(logFileOpen)
          logFile.print("SER-OUT: ");
        while(len>0)
        {
          if((!flowControl) || XON)
          {
            len--;
            int c=f.read();
            if(c<0)
              break;
            if(cache && petsciiMode)
              c=ascToPetcii(c);
            switch(streamType)
            {
              case BTYPE_NORMAL:
                Serial.write((uint8_t)c);
                break;
              case BTYPE_HEX:
                Serial.print(TOHEX((uint8_t)c));
                if((++bct)>=39)
                {
                  Serial.print(EOLN);
                  bct=0;
                }
                break;
              case BTYPE_DEC:
                Serial.printf("%d%s",c,EOLN.c_str());
                break;
            }
            if(logFileOpen)
            {
              switch(streamType)
              {
              case BTYPE_NORMAL:
              case BTYPE_HEX:
                logFile.printf("%s ",TOHEX((uint8_t)c));
                if((++lct)>=39)
                {
                  lct=0;
                  logFile.printf("%sSER-OUT: ",EOLN.c_str());
                }
                break;
              case BTYPE_DEC:
                logFile.printf("%d ",c);
                if((++lct)>=39)
                {
                  lct=0;
                  logFile.printf("%sSER-OUT: ",EOLN.c_str());
                }
                break;
              }
            }
            while(Serial.availableForWrite()<mark)
            {
              delay(1);
              yield();
            }
            if(delayMs > 0)
              delay(delayMs);
          }
          while(Serial.available()>0)
          {
            char ch=Serial.read();
            if(ch == 3)
            {
              f.close();
              return ZOK;
            }
            if(flowControl)
            {
              switch(flowControlType)
              {
                case FCT_NORMAL:
                  if((!XON) && (ch == 17))
                    XON=true;
                  else
                  if((XON) && (ch == 19))
                    XON=false;
                  break;
                case FCT_AUTOOFF:
                case FCT_MANUAL:
                  if((!XON) && (ch == 17))
                    XON=true;
                  else
                    XON=false;
                  break;
                case FCT_INVALID:
                  break;
              }
            }
          }
          yield();
        }
        if(bct > 0)
          Serial.print(EOLN);
        if(logFileOpen && (lct > 0))
          logFile.println("");
        f.close();
      }
    }
  }
  return ZIGNORE;
}

ZResult ZCommand::doUpdateFirmware(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  uint8_t buf[255];
  int bufSize = 254;
  if((!doWebGetBytes("www.zimmers.net", 80, "/otherprojs/c64net-latest-version.txt", buf, &bufSize))||(bufSize<=0))
    return ZERROR;
    
  while((bufSize>0)
  &&((buf[bufSize-1]==10)||(buf[bufSize-1]==13)))
    bufSize--;
  
  Serialprint("Local firmware version ");
  Serial.print(ZIMODEM_VERSION);
  Serial.print(".");
  Serial.print(EOLN);
  
  if((strlen(ZIMODEM_VERSION)==bufSize) && memcmp(buf,ZIMODEM_VERSION,strlen(ZIMODEM_VERSION))==0)
  {
    Serialprint("Your modem is up-to-date.");
    Serial.print(EOLN);
  }
  else
  {
    Serialprint("Latest available version is ");
    buf[bufSize]=0;
    Serial.print((char *)buf);
    Serial.print(".");
    Serial.print(EOLN);
  }
  if(vval != 6502)
    return ZOK;
  
  Serialprint("Updating...");
  uint32_t respLength=0;
  WiFiClient c;
  char firmwareName[100];
  sprintf(firmwareName,"/otherprojs/c64net-firmware-%s.bin",buf);
  if(!doWebGetStream("www.zimmers.net", 80, firmwareName, c, &respLength))
  {
    Serial.print(EOLN);
    return ZERROR;
  }

  if(!Update.begin(respLength))
    return ZERROR;

  Serial.print(".");
  if(Update.writeStream(c) != respLength)
  {
    Serial.print(EOLN);
    return ZERROR;
  }
  Serial.print(".");
  if(!Update.end())
  {
    Serial.print(EOLN);
    return ZERROR;
  }
  Serial.print("Done");
  Serial.print(EOLN);
  Serial.print("Press the Reset button on your modem.");
  ESP.restart();
  return ZOK;
}

ZResult ZCommand::doWiFiCommand(int vval, uint8_t *vbuf, int vlen)
{
  if((vlen==0)||(vval>0))
  {
    int n = WiFi.scanNetworks();
    if((vval > 0)&&(vval < n))
      n=vval;
    for (int i = 0; i < n; ++i)
    {
      Serialprint(WiFi.SSID(i).c_str());
      Serialprint(" (");
      Serial.print(WiFi.RSSI(i));
      Serialprint(")");
      Serialprint((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      Serialprint(EOLN.c_str());
      delay(10);
    }
  }
  else
  {
    char *x=strstr((char *)vbuf,",");
    if(x <= 0)
      return ZERROR;
    else
    {
      *x=0;
      char *ssi=(char *)vbuf;
      char *pw=x+1;
      if(!connectWifi(ssi,pw))
        return ZERROR;
      else
      {
        wifiSSI=ssi;
        wifiPW=pw;
      }
    }
  }
  return ZOK;
}

ZResult ZCommand::doTransmitCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers, const int crc8)
{
  bool doPETSCII = (strchr(dmodifiers,'p')!=null) || (strchr(dmodifiers,'P')!=null);
  if((vlen==0)||(current==null)||(!current->isConnected()))
    return ZERROR;
  else
  if(isNumber && (vval>0))
  {
    uint8_t buf[vval];
    int recvd = Serial.readBytes(buf,vval);
    if(recvd != vval)
      return ZERROR;
    if((crc8 != -1)&&(CRC8(buf,recvd)!=crc8))
      return ZERROR;
    if(current->isPETSCII() || doPETSCII)
    {
      for(int i=0;i<recvd;i++)
        buf[i]=petToAsc(buf[i]);
    }
    current->write(buf,recvd);
  }
  else
  {
    uint8_t buf[vlen];
    memcpy(buf,vbuf,vlen);
    if((crc8 != -1)&&(CRC8(buf,vlen)!=crc8))
      return ZERROR;
    if(current->isPETSCII() || doPETSCII)
    {
      for(int i=0;i<vlen;i++)
        buf[i] = petToAsc(buf[i]);
    }
    current->write(buf,vlen);
    current->write(13); // special case
    current->write(10); // special case
  }
  return ZOK;
}

ZResult ZCommand::doDialStreamCommand(unsigned long vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers)
{
  if(vlen == 0)
  {
    if((current == null)||(!current->isConnected()))
      return ZERROR;
    else
    {
      streamMode.switchTo(current);
    }
  }
  else
  if((vval >= 0)&&(isNumber))
  {
    PhoneBookEntry *phb = phonebook;
    while(phb != null)
    {
      if(phb->number == vval)
      {
        int addrLen=strlen(phb->address);
        uint8_t *vbuf = new uint8_t[addrLen+1];
        strcpy((char *)vbuf,phb->address);
        ZResult res = doDialStreamCommand(0,vbuf,addrLen,false,phb->modifiers);
        free(vbuf);
        return res;
      }
      phb = phb->next;
    }
    
    WiFiClientNode *c=conns;
    while((c!=null)&&(c->id != vval))
      c=c->next;
    if((c!=null)&&(c->id == vval)&&(c->isConnected()))
    {
      current=c;
      setCharArray(&(c->delimiters),tempDelimiters);
      setCharArray(&(c->maskOuts),tempMaskOuts);
      freeCharArray(&tempDelimiters);
      freeCharArray(&tempMaskOuts);
      streamMode.switchTo(c);
      return ZCONNECT;
    }
    else
      return ZERROR;
  }
  else
  {
    int flagsBitmap = makeStreamFlagsBitmap(dmodifiers);
    char *colon=strstr((char *)vbuf,":");
    int port=23;
    if(colon != null)
    {
      (*colon)=0;
      port=atoi((char *)(++colon));
    }
    WiFiClientNode *c = new WiFiClientNode((char *)vbuf,port,flagsBitmap | FLAG_DISCONNECT_ON_EXIT);
    if(!c->isConnected())
    {
      delete c;
      return ZERROR;
    }
    else
    {
      current=c;
      setCharArray(&(c->delimiters),tempDelimiters);
      setCharArray(&(c->maskOuts),tempMaskOuts);
      freeCharArray(&tempDelimiters);
      freeCharArray(&tempMaskOuts);
      streamMode.switchTo(c);
      return ZCONNECT;
    }
  }
  return ZOK;
}

ZResult ZCommand::doPhonebookCommand(unsigned long vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers)
{
  if((vlen==0)||(isNumber))
  {
    PhoneBookEntry *phb=phonebook;
    char nbuf[30];
    while(phb != null)
    {
      if((!isNumber)
      ||(vval==0)
      ||(vval == phb->number))
      {
        if((strlen(dmodifiers)==0) 
        || (modifierCompare(dmodifiers,phb->modifiers)==0))
        {
          sprintf(nbuf,"%lu",phb->number);
          Serialprint(nbuf);
          for(int i=0;i<10-strlen(nbuf);i++)
            Serialprint(" ");
          Serialprint(" ");
          Serialprint(phb->modifiers);
          for(int i=1;i<5-strlen(phb->modifiers);i++)
            Serialprint(" ");
          Serialprint(" ");
          Serialprint(phb->address);
          Serialprint(EOLN.c_str());
          delay(10);
        }
      }
      phb=phb->next;
    }
    return ZOK;
  }
  char *eq=strchr((char *)vbuf,'=');
  if(eq == NULL)
    return ZERROR;
  for(char *cptr=(char *)vbuf;cptr!=eq;cptr++)
  {
    if(strchr("0123456789",*cptr) < 0)
      return ZERROR;
  }
  char *rest=eq+1;
  *eq=0;
  if(strlen((char *)vbuf)>9)
    return ZERROR;

  unsigned long number = atol((char *)vbuf);
  PhoneBookEntry *found=null;
  PhoneBookEntry *phb=phonebook;
  while(phb != null)
  {
    if(phb->number == number)
    {
      found=phb;
      break;
    }
    phb = phb->next;
  }
  if((strcmp("DELETE",rest)==0)
  ||(strcmp("delete",rest)==0))
  {
    if(found==null)
      return ZERROR;
    delete found;
    PhoneBookEntry::savePhonebook();
    return ZOK;
  }
  char *comma = strchr(rest,',');
  if(comma != NULL)
    return ZERROR;
  char *colon = strchr(rest,':');
  if(colon == NULL)
    return ZERROR;
  for(char *cptr=colon;*cptr!=0;cptr++)
  {
    if(strchr("0123456789",*cptr) < 0)
      return ZERROR;
  }
  if(found != null)
    delete found;
  PhoneBookEntry *newEntry = new PhoneBookEntry(number,rest,dmodifiers);
  PhoneBookEntry::savePhonebook();
  return ZOK;
}

ZResult ZCommand::doAnswerCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers)
{
  if((vlen == 1)&&(vbuf[0]=='/'))
  {
    if(previousCommand.length()==0)
      return ZERROR;
    else
    if(previousCommand[previousCommand.length()-1] == '/')
      return ZERROR;
    else
    {
      strcpy((char *)nbuf,previousCommand.c_str());
      eon=previousCommand.length();
      doSerialCommand();
      return ZIGNORE_SPECIAL;
    }
  }
  else
  if(vval <= 0)
  {
      WiFiClientNode *c=conns;
      while(c!=null)
      {
        if((c->isConnected())
        &&(c->id = lastServerClientId))
        {
          current=c;
          streamMode.switchTo(c);
          lastServerClientId=0;
          if(ringCounter == 0)
          {
            sendConnectionNotice(c->id);
            return ZIGNORE;
          }
          break;
        }
        c=c->next;
      }
      //TODO: possibly go to streaming mode, turn on DCD, and do nothing?
      return ZOK; // not really doing anything important...
  }
  else
  {
    int flagsBitmap = makeStreamFlagsBitmap(dmodifiers);
    WiFiServerNode *s=servs;
    while(s != null)
    {
      if(s->port == vval)
        return ZOK;
      s=s->next;
    }
    WiFiServerNode *newServer = new WiFiServerNode(vval, flagsBitmap);
    setCharArray(&(newServer->delimiters),tempDelimiters);
    setCharArray(&(newServer->maskOuts),tempMaskOuts);
    freeCharArray(&tempDelimiters);
    freeCharArray(&tempMaskOuts);
    return ZOK;
  }
}

ZResult ZCommand::doHangupCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  if(vlen == 0)
  {
    while(conns != null)
    {
      WiFiClientNode *c=conns;
      delete c;
    }
    current = null;
    nextConn = null;
    return ZOK;
  }
  else
  if(isNumber && (vval == 0))
  {
      if(current != 0)
      {
        delete current;
        current = conns;
        nextConn = conns;
        return ZOK;
      }
      return ZERROR;
  }
  else
  if(vval > 0)
  {
    WiFiClientNode *c=conns;
    while(c != 0)
    {
      if(vval == c->id)
      {
        if(current == c)
          current = conns;
        if(nextConn == c)
          nextConn = conns;
        delete c;
        return ZOK;
      }
      c=c->next;
    }
    WiFiServerNode *s=servs;
    while(s!=null)
    {
      if(vval == s->id)
      {
        delete s;
        return ZOK;
      }
      s=s->next;
    }
    return ZERROR;
  }
}

ZResult ZCommand::doLastPacket(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  if(!isNumber)
    return ZERROR;
  WiFiClientNode *cnode=null;
  if(vval == 0)
    cnode=current;
  else
  {
    WiFiClientNode *c=conns;
    while(c != 0)
    {
      if(vval == c->id)
      {
        cnode=c;
        break;
      }
      c=c->next;
    }
  }
  if(cnode == null)
    return ZERROR;
  reSendLastPacket(cnode);
  return ZIGNORE;
}

ZResult ZCommand::doEOLNCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  if(isNumber)
  {
    if((vval>=0)&&(vval < 4))
    {
      switch(vval)
      {
      case 0:
        EOLN = CR;
        break;
      case 1:
        EOLN = CRLF;
        break;
      case 2:
        EOLN = LFCR;
        break;
      case 3:
        EOLN = LF;
        break;
      }
      return ZOK;
    }
  }
  return ZERROR;
}

bool ZCommand::readSerialStream()
{
  bool crReceived=false;
  int logCharNum=0;
  while(Serial.available()>0)
  {
    uint8_t c=Serial.read();
    if(logFileOpen)
    {
      if(logCharNum==0)
        logFile.print("SER-IN: ");
      logFile.print(TOHEX(c));
      logFile.print(" ");
      logCharNum++;
    }
    if((c==CR[0])||(c==LF[0]))
    {
      if(eon == 0)
        continue;
      else
      {
        if(doEcho)
          Serial.print(EOLN);
        crReceived=true;
        break;
      }
    }
    
    if(c>0)
    {
      if(c!=EC)
        lastNonPlusTimeMs=millis();
      if((c==19)&&(flowControlType != FCT_DISABLED))
      {
        XON=false;
      }
      else
      if((c==17)&&(flowControlType != FCT_DISABLED))
      {
        XON=true;
        if(flowControlType == FCT_MANUAL)
        {
          sendNextPacket();
        }
      }
      else
      {
        if(doEcho)
          Serial.write(c);
        if((c==BS)||((BS==8)&&((c==20)||(c==127))))
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
  }
  if(logFileOpen && (logCharNum>0))
    logFile.println("");
  return crReceived;
}

ZResult ZCommand::doSerialCommand()
{
  int len=eon;
  uint8_t sbuf[len];
  memcpy(sbuf,nbuf,len);
  memset(nbuf,0,MAX_COMMAND_SIZE);
  if(petsciiMode)
  {
    for(int i=0;i<len;i++)
      sbuf[i]=petToAsc(sbuf[i]);
  }
      
  eon=0;
  String currentCommand = (char *)sbuf;
  int crc8=-1;
  
  ZResult result=ZOK;
  int index=0;
  while((index<len-1)
  &&((lc(sbuf[index])!='a')||(lc(sbuf[index+1])!='t')))
      index++;

  if(logFileOpen)
  {
      char cmdbuf[len+1];
      memcpy(cmdbuf,sbuf,len);
      cmdbuf[len]=0;
      logFile.print("Command: ");
      logFile.println(cmdbuf);
  }

  if((index<len-1)
  &&(lc(sbuf[index])=='a')
  &&(lc(sbuf[index+1])=='t'))
  {
    index+=2;
    char lastCmd=' ';
    char secCmd=' ';
    int vstart=0;
    int vlen=0;
    String dmodifiers="";
    while(index<len)
    {
      while((index<len)
      &&((sbuf[index]==' ')||(sbuf[index]=='\t')))
        index++;
      lastCmd=lc(sbuf[index++]);
      vstart=index;
      vlen=0;
      bool isNumber=true;
      if((lastCmd=='&')&&(index<len))
      {
        index++;//protect our one and only letter.
        secCmd = sbuf[vstart];
        vstart++;
      }
      while((index<len)
      &&((sbuf[index]==' ')||(sbuf[index]=='\t')))
      {
        vstart++;
        index++;
      }
      if(index<len)
      {
        if(sbuf[index]=='\"')
        {
          isNumber=false;
          vstart++;
          while((++index<len)
          &&((sbuf[index]!='\"')||(sbuf[index-1]=='\\')))
            vlen++;
          if(index<len)
            index++;
        }
        else
        if((lastCmd=='d')||(lastCmd=='D')
        || (lastCmd=='c')||(lastCmd=='C')
        || (lastCmd=='p')||(lastCmd=='P')
        || (lastCmd=='t')||(lastCmd=='T'))
        {
          const char *DMODIFIERS=",lbexprtw+";
          while((index<len)&&(strchr(DMODIFIERS,lc(sbuf[index]))!=null))
            dmodifiers += lc((char)sbuf[index++]);
          while((index<len)
          &&((sbuf[index]==' ')||(sbuf[index]=='\t')))
            index++;
          vstart=index;
          if(sbuf[index]=='\"')
          {
            vstart++;
            while((++index<len)
            &&((sbuf[index]!='\"')||(sbuf[index-1]=='\\')))
              vlen++;
            if(index<len)
              index++;
          }
          else
          {
            vlen += len-index;
            index=len;
          }
          for(int i=vstart;i<vstart+vlen;i++)
          {
            char c=sbuf[i];
            isNumber = ((c=='-') || ((c>='0')&&(c<='9'))) && isNumber;
          }
        }
        else
        while((index<len)
        &&(!((lc(sbuf[index])>='a')&&(lc(sbuf[index])<='z')))
        &&(sbuf[index]!='&'))
        {
          char c=sbuf[index];
          isNumber = ((c=='-')||((c>='0') && (c<='9'))) && isNumber;
          vlen++;
          index++;
        }
      }
      long vval=0;
      uint8_t vbuf[vlen+1];
      memset(vbuf,0,vlen+1);
      if(vlen>0)
      {
        memcpy(vbuf,sbuf+vstart,vlen);
        if((vlen > 0)&&(isNumber))
        {
          String finalNum="";
          for(uint8_t *v=vbuf;v<(vbuf+vlen);v++)
            if((*v>='0')&&(*v<='9'))
              finalNum += (char)*v;
          vval=atol(finalNum.c_str());
        }
      }
      
      if(logFileOpen)
      {
        if(vlen > 0)
          logFile.printf("Proc: %c %lu '%s'\r\n",lastCmd,vval,vbuf);
        else
          logFile.printf("Proc: %c %lu ''\r\n",lastCmd,vval);
      }

      /*
       * We have cmd and args, time to DO!
       */
      switch(lastCmd)
      {
      case 'z':
        result = doResetCommand();
        break;
      case 'n':
        if(isNumber && (vval == 0))
        {
          doNoListenCommand();
          break;
        }
      case 'a':
        result = doAnswerCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str());
        break;
      case 'e':
        if(!isNumber)
          result=ZERROR;
        else
          doEcho=(vval > 0);
        break;
      case 'f':
        if((!isNumber)||(vval>=FCT_INVALID))
          result=ZERROR;
        else
        {
            flowControlType = (FlowControlType)vval;
            if(flowControlType == FCT_MANUAL)
              XON=false;
        }
        break;
      case 'x':
        if(!isNumber)
          result=ZERROR;
        else
          longResponses = (vval > 0);
        break;
      case 'r':
        result = doEOLNCommand(vval,vbuf,vlen,isNumber);
        break;
      case 'b':
        result = doBaudCommand(vval,vbuf,vlen);
        break;
      case 't':
        result = doTransmitCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str(),crc8);
        break;
      case 'h':
        result = doHangupCommand(vval,vbuf,vlen,isNumber);
        break;
      case 'd':
        result = doDialStreamCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str());
        break;
      case 'p':
        result = doPhonebookCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str());
        break;
      case 'o':
        if((vlen == 0)||(vval==0))
        {
          if((current == null)||(!current->isConnected()))
            result = ZERROR;
          else
          {
            streamMode.switchTo(current);
            result = ZOK;
          }
        }
        else
          result = isNumber ? ZOK : ZERROR;
        break;
      case 'c':
        result = doConnectCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str());
        break;
      case 'i':
        result = doInfoCommand(vval,vbuf,vlen,isNumber);
        break;
      case 'l':
        result = doLastPacket(vval,vbuf,vlen,isNumber);
        break;
      case 'm':
      case 'y':
        result = isNumber ? ZOK : ZERROR;
        break;
      case 'w':
        result = doWiFiCommand(vval,vbuf,vlen);
        break;
      case 'v':
        if(!isNumber)
          result=ZERROR;
        else
          numericResponses = (vval == 0);
        break;
      case 'q':
        if(!isNumber)
          result=ZERROR;
        else
          suppressResponses = (vval > 0);
        break;
      case 's':
        {
          if(vlen<3)
            result=ZERROR;
          else
          {
            char *eq=strchr((char *)vbuf,'=');
            if((eq == null)||(eq == (char *)vbuf)||(eq>=(char *)&(vbuf[vlen-1])))
              result=ZERROR;
            else
            {
              *eq=0;
              int snum = atoi((char *)vbuf);
              int sval = atoi((char *)(eq + 1));
              if((snum == 0)&&((vbuf[0]!='0')||(eq != (char *)(vbuf+1))))
                result=ZERROR;
              else
              if((sval == 0)&&((*(eq+1)!='0')||(*(eq+2) != 0)))
                result=ZERROR;
              else
              switch(snum)
              {
              case 0:
                if((sval < 0)||(sval>255))
                  result=ZERROR;
                else
                  ringCounter = sval;
                break;
              case 2:
                if((sval < 0)||(sval>255))
                  result=ZERROR;
                else
                {
                  EC=(char)sval;
                  ECS[0]=EC;
                  ECS[1]=EC;
                  ECS[2]=EC;
                }
                break;
              case 3:
                if((sval < 0)||(sval>127))
                  result=ZERROR;
                else
                {
                  CR[0]=(char)sval;
                  CRLF[0]=(char)sval;
                  LFCR[1]=(char)sval;
                }
                break;
              case 4:
                if((sval < 0)||(sval>127))
                  result=ZERROR;
                else
                {
                  LF[0]=(char)sval;
                  CRLF[1]=(char)sval;
                  LFCR[0]=(char)sval;
                }
                break;
              case 5:
                if((sval < 0)||(sval>32))
                  result=ZERROR;
                else
                {
                  BS=(char)sval;
                }
                break;
              case 40:
                if(sval < 1)
                  result=ZERROR;
                else
                  packetSize=sval;
                break;
             case 41:
                autoStreamMode = (sval > 0);
                break;
             case 42:
               crc8=sval;
               break;
             case 43:
               if(sval > 0)
                 tempBaud = sval;
               else
                 tempBaud = -1;
               break;
             case 44:
               delayMs=sval;
               break;
             case 45:
               if((sval>=0)&&(sval<BTYPE_INVALID))
                 binType=(BinType)sval;
               else
                 result=ZERROR;
               break;
             case 46:
               if(sval <=0)
               {
                 DCD_HIGH = HIGH;
                 DCD_LOW = LOW;
               }
               else
               {
                 DCD_HIGH = LOW;
                 DCD_LOW = HIGH;
               }
               break;
             default:
                break;
              }
            }
          }
        }
        break;
      case '&':
        switch(lc(secCmd))
        {
        case 'l':
          loadConfig();
          break;
        case 'w':
          reSaveConfig();
          break;
        case 'f':
          SPIFFS.remove("/zconfig.txt");
          SPIFFS.remove("/zphonebook.txt");
          PhoneBookEntry::clearPhonebook();
          delay(500);
          result=doResetCommand();
          showInitMessage();
          break;
        case 'm':
          if(vval > 0)
          {
            int len = (tempMaskOuts != NULL) ? strlen(tempMaskOuts) : 0;
            char newMaskOuts[len+2]; // 1 for the new char, and 1 for the 0 never counted
            if(len > 0)
              strcpy(newMaskOuts,tempMaskOuts);
            newMaskOuts[len] = vval;
            newMaskOuts[len+1] = 0;
            setCharArray(&tempMaskOuts,newMaskOuts);
          }
          else
          {
            char newMaskOuts[vlen+1];
            newMaskOuts[vlen]=0;
            if(vlen > 0)
              memcpy(newMaskOuts,vbuf,vlen);
            setCharArray(&tempMaskOuts,newMaskOuts);
          }
          result=ZOK;
          break;
        case 'd':
          if(vval > 0)
          {
            int len = (tempDelimiters != NULL) ? strlen(tempDelimiters) : 0;
            char newDelimiters [len+2]; // 1 for the new char, and 1 for the 0 never counted
            if(len > 0)
              strcpy(newDelimiters,tempDelimiters);
            newDelimiters[len] = vval;
            newDelimiters[len+1] = 0;
            setCharArray(&tempDelimiters,newDelimiters);
          }
          else
          {
            char newDelimiters[vlen+1];
            newDelimiters[vlen]=0;
            if(vlen > 0)
              memcpy(newDelimiters,vbuf,vlen);
            setCharArray(&tempDelimiters,newDelimiters);
          }
          result=ZOK;
          break;
        case 'o':
          if(vval == 0)
          {
            if(logFileOpen)
            {
              logFileOpen = false;
              logFile.close();
            }
            logFile = SPIFFS.open("/logfile.txt", "r");
            int numBytes = logFile.available();
            while (numBytes > 0) 
            {
              if(numBytes > 128)
                numBytes = 128;
              byte buf[numBytes];
              int numRead = logFile.read(buf,numBytes);
              int i=0;
              while(i < numRead)
              {
                if(Serial.availableForWrite() > 0)
                {
                  Serial.write(buf[i++]);
                  if(delayMs > 0)
                    delay(delayMs);
                }
                else
                  delay(1);
                yield();
              }
              numBytes = logFile.available();
            }
            logFile.close();
            Serial.println("");
            result=ZOK;
          }
          else
          if(logFileOpen)
            result=ZERROR;
          else
          {
            logFileOpen = true;
            SPIFFS.remove("/logfile.txt");
            logFile = SPIFFS.open("/logfile.txt", "w");              
            result=ZOK;
          }
          break;
        case 'h':
        {
          char filename[50];
          sprintf(filename,"/c64net-help-%s.txt",ZIMODEM_VERSION);
          if(vval == 6502)
          {
            SPIFFS.remove(filename);
            result=ZOK;
          }
          else
          {
            int oldDelay = delayMs;
            delayMs = vval;
            uint8_t buf[100];
            sprintf((char *)buf,"www.zimmers.net:80/otherprojs%s",filename);
            Serialprint("Control-C to Abort.");
            Serial.print(EOLN);
            result = doWebStream(0,buf,strlen((char *)buf),false,filename,true);
            delayMs = oldDelay;
            if(result == ZERROR)
            {
              Serialprint("Not Connected.");
              Serial.print(EOLN);
              Serialprint("Use ATW to list access points.");
              Serial.print(EOLN);
              Serialprint("ATW\"[SSI],[PASSWORD]\" to connect.");
              Serial.print(EOLN);
            }
          }
          break;
        }
        case 'g':
          result = doWebStream(vval,vbuf,vlen,isNumber,"/temp.web",false);
          break;
        case 'p':
          petsciiMode = vval > 0;
          break;
        case 'u':
          result=doUpdateFirmware(vval,vbuf,vlen,isNumber);
          break;
        default:
          result=ZERROR;
          break;
        }
        break;
      default:
        result=ZERROR;
        break;
      }
    }

    setCharArray(&delimiters,tempDelimiters);
    freeCharArray(&tempDelimiters);
    setCharArray(&maskOuts,tempMaskOuts);
    freeCharArray(&tempMaskOuts);
  
    if(result != ZIGNORE_SPECIAL)
      previousCommand = currentCommand;
    if(suppressResponses)
    {
      if(result == ZERROR)
      {
        // on error, cut and run
        return ZERROR;
      }
    }
    else
    {
      switch(result)
      {
      case ZOK:
        if(index >= len)
        {
          if(logFileOpen)
              logFile.printf("Response: OK\r\n");
          if(numericResponses)
            Serial.print("0");
          else
            Serialprint("OK");
          Serial.print(EOLN);
        }
        break;
      case ZERROR:
        if(logFileOpen)
            logFile.printf("Response: ERROR\r\n");
        if(numericResponses)
          Serial.print("4");
        else
          Serialprint("ERROR");
        Serial.print(EOLN);
        // on error, cut and run
        return ZERROR;
      case ZCONNECT:
        if(logFileOpen)
            logFile.printf("Response: Connected\r\n");
        sendConnectionNotice((current == null) ? baudRate : current->id);
        break;
      default:
        break;
      }
    }
  }
  return result;
}

void ZCommand::showInitMessage()
{
  FSInfo info;
  SPIFFS.info(info);
  Serial.print(commandMode.EOLN);
  Serialprint("C64Net WiFi Firmware v");
  Serial.setTimeout(60000);
  Serialprint(ZIMODEM_VERSION);
  Serial.print(commandMode.EOLN);
  char s[100];
  sprintf(s,"sdk=%s chipid=%d cpu@%d",ESP.getSdkVersion(),ESP.getFlashChipId(),ESP.getCpuFreqMHz());
  Serialprint(s);
  Serial.print(commandMode.EOLN);
  sprintf(s,"totsize=%dk ssize=%dk fsize=%dk speed=%dm",(ESP.getFlashChipSize()/1024),(ESP.getSketchSize()/1024),info.totalBytes/1024,(ESP.getFlashChipSpeed()/1000000));
  Serialprint(s);
  Serial.print(commandMode.EOLN);
  if(wifiSSI.length()>0)
  {
    if(wifiConnected)
      Serialprint(("CONNECTED TO " + wifiSSI + " (" + WiFi.localIP().toString().c_str() + ")").c_str());
    else
      Serialprint(("ERROR ON " + wifiSSI).c_str());
  }
  else
    Serialprint("INITIALIZED");
  Serial.print(commandMode.EOLN);
  Serialprint("READY.");
  Serial.print(commandMode.EOLN);
  Serial.flush();
}

void ZCommand::reSendLastPacket(WiFiClientNode *conn)
{
  if(conn == NULL)
  {
    headerOut(0,0,0);
  }
  else
  if(conn->lastPacketLen == 0) // never used, or empty
  {
    headerOut(conn->id,conn->lastPacketLen,0);
  }
  else
  {
    int bufLen = conn->lastPacketLen;
    uint8_t *buf = (uint8_t *)malloc(bufLen);
    memcpy(buf,conn->lastPacketBuf,bufLen);

    if((conn->maskOuts[0] != 0) || (maskOuts[0] != 0))
    {
      int oldLen=bufLen;
      for(int i=0,o=0;i<oldLen;i++,o++)
      {
        if((strchr(conn->maskOuts,buf[i])!=null)
        ||(strchr(maskOuts,buf[i])!=null))
        {
          o--;
          bufLen--;
        }
        else
          buf[o]=buf[i];
      }
    }
    if(nextConn->isPETSCII())
    {
      int oldLen=bufLen;
      for(int i=0, b=0;i<oldLen;i++,b++)
      {
        buf[b]=buf[i];
        if(!ascToPet((char *)&buf[b],conn))
        {
          b--;
          bufLen--;
        }
      }
    }
    
    uint8_t crc=CRC8(buf,bufLen);
    headerOut(conn->id,bufLen,(int)crc);
    int bct=0;
    int lct=0;
    int i=0;
    if(logFileOpen)
      logFile.print("SER-OUT: ");
    while(i < bufLen)
    {
      uint8_t c=buf[i++];
      switch(binType)
      {
        case BTYPE_NORMAL:
          Serial.write(c);
          break;
        case BTYPE_HEX:
          Serial.print(TOHEX(c));
          if((++bct)>=39)
          {
            Serial.print(EOLN);
            bct=0;
          }
          break;
        case BTYPE_DEC:
          Serial.printf("%d%s",c,EOLN.c_str());
          break;
      }
      if(logFileOpen)
      {
        switch(binType)
        {
        case BTYPE_NORMAL:
        case BTYPE_HEX:
          logFile.printf("%s ",TOHEX(c));
          if((++lct)>=39)
          {
            lct=0;
            logFile.printf("%sSER-OUT: ",EOLN.c_str());
          }
          break;
        case BTYPE_DEC:
          logFile.printf("%d ",c);
          if((++lct)>=39)
          {
            lct=0;
            logFile.printf("%sSER-OUT: ",EOLN.c_str());
          }
          break;
        }
      }
      if(delayMs > 0)
        delay(delayMs);
      while(Serial.availableForWrite()<10)
      {
        delay(1);
        yield();
      }
      yield();
    }
    if(bct > 0)
      Serial.print(EOLN);
    if(logFileOpen && (lct > 0))
      logFile.println("");
    free(buf);
  }
}

void ZCommand::serialIncoming()
{
  bool crReceived=readSerialStream();
  if(currentExpiresTimeMs > 0)
    currentExpiresTimeMs = 0;
  if((strcmp((char *)nbuf,ECS)==0)&&((millis()-lastNonPlusTimeMs)>1000))
    currentExpiresTimeMs = millis() + 1000;
  if(!crReceived)
    return;
  //delay(200); // give a pause after receiving command before responding
  // the delay doesn't affect xon/xoff because its the periodic transmitter that manages that.
  doSerialCommand();
}

void ZCommand::sendNextPacket()
{
  if(Serial.availableForWrite()<100)
    return;

  WiFiClientNode *firstConn = nextConn;
  if((nextConn == null)||(nextConn->next == null))
  {
    firstConn = null;
    nextConn = conns;
  }
  else
    nextConn = nextConn->next;
  while(((flowControlType==FCT_DISABLED)||XON) && (nextConn != null))
  {
    if((nextConn->isConnected())
    && (nextConn->available()>0))
    {
      int availableBytes = nextConn->available();
      int maxBytes=packetSize;
      if(availableBytes<maxBytes)
        maxBytes=availableBytes;
      if(maxBytes > Serial.availableForWrite()-15)
        maxBytes = Serial.availableForWrite()-15;
      if(maxBytes > 0)
      {
        if((nextConn->delimiters[0] != 0) || (delimiters[0] != 0))
        {
          int lastLen = nextConn->lastPacketLen;
          uint8_t *lastBuf = nextConn->lastPacketBuf;
          
          if((lastLen >= packetSize)
          ||((lastLen>0)
              &&((strchr(nextConn->delimiters,lastBuf[lastLen-1]) != null)
                ||(strchr(delimiters,lastBuf[lastLen-1]) != null))))
            lastLen = 0;
          int bytesRemain = maxBytes;
          while((bytesRemain > 0)
          &&(lastLen < packetSize)
          &&((lastLen==0)
            ||((strchr(nextConn->delimiters,lastBuf[lastLen-1]) == null)
              &&(strchr(delimiters,lastBuf[lastLen-1]) == null))))
          {
            lastBuf[lastLen++] = nextConn->read();
            bytesRemain--;
          }
          nextConn->lastPacketLen = lastLen;
          //BUG: PETSCII conversion screws up the last packet
          if((lastLen >= packetSize)
          ||((lastLen>0)
            &&((strchr(nextConn->delimiters,lastBuf[lastLen-1]) != null)
              ||(strchr(delimiters,lastBuf[lastLen-1]) != null))))
            maxBytes = lastLen;
          else
          {
            if(flowControlType == FCT_MANUAL)
            {
              headerOut(0,0,0);
              XON=false;
            }
            else
            if(flowControlType == FCT_AUTOOFF)
              XON=false;
            return;
          }
        }
        else
          maxBytes = nextConn->read(nextConn->lastPacketBuf,maxBytes);
        nextConn->lastPacketLen=maxBytes;
        reSendLastPacket(nextConn);
        if(flowControlType == FCT_AUTOOFF)
        {
          XON=false;
        }
        else
        if(flowControlType == FCT_MANUAL)
        {
          XON=false;
          return;
        }
        break;
      }
    }
    else
    if(!nextConn->isConnected())
    {
      if(nextConn->wasConnected)
      {
        nextConn->wasConnected=false;
        if(!suppressResponses)
        {
          if(numericResponses)
            Serial.printf("3");
          else
          {
            Serialprint("NO CARRIER ");
            Serial.print(nextConn->id);
          }
          Serial.print(EOLN);
          if(flowControlType == FCT_MANUAL)
          {
            return;
          }
        }
        checkOpenConnections();
      }
      if(nextConn->serverClient)
      {
        delete nextConn;
        nextConn = null;
        break; // messes up the order, so just leave and start over
      }
    }

    if(nextConn->next == null)
      nextConn = null; // will become CONNs
    else
      nextConn = nextConn->next;
    if(nextConn == firstConn)
      break;
  }
  if(flowControlType == FCT_MANUAL)
  {
    XON=false;
    firstConn = conns;
    while(firstConn != NULL)
    {
      firstConn->lastPacketLen = 0;
      firstConn = firstConn->next;
    }
    headerOut(0,0,0);
  }
}

void ZCommand::sendConnectionNotice(int id)
{
  if(numericResponses)
  {
    if(!longResponses)
      Serial.print("1");
    else
    if(baudRate < 1200)
      Serial.print("1");
    else
    if(baudRate < 2400)
      Serial.print("5");
    else
    if(baudRate < 4800)
      Serial.print("10");
    else
    if(baudRate < 7200)
      Serial.print("11");
    else
    if(baudRate < 9600)
      Serial.print("24");
    else
    if(baudRate < 12000)
      Serial.print("12");
    else
    if(baudRate < 14400)
      Serial.print("25");
    else
    if(baudRate < 19200)
      Serial.print("13");
    else
      Serial.print("28");
  }
  else
  {
    Serialprint("CONNECT");
    if(longResponses)
    {
      Serial.print(" ");
      Serial.print(id);
    }
  }
  Serial.print(EOLN);
}

void ZCommand::acceptNewConnection()
{
  WiFiServerNode *serv = servs;
  while(serv != null)
  {
    if(serv->hasClient())
    {
      WiFiClient newClient = serv->server->available();
      if((newClient != null)&&(newClient.connected()))
      {
        int port=newClient.localPort();
        String remoteIPStr = newClient.remoteIP().toString();
        const char *remoteIP=remoteIPStr.c_str();
        bool found=false;
        WiFiClientNode *c=conns;
        while(c!=null)
        {
          if((c->isConnected())
          &&(c->port==port)
          &&(strcmp(remoteIP,c->host)==0))
            found=true;
          c=c->next;
        }
        if(!found)
        {
          //BZ:newClient.setNoDelay(true);
          WiFiClientNode *newClientNode = new WiFiClientNode(newClient, serv->flagsBitmap);
          setCharArray(&(newClientNode->delimiters),serv->delimiters);
          setCharArray(&(newClientNode->maskOuts),serv->maskOuts);
          int i=0;
          do
          {
            if(numericResponses)
              Serial.print("2");
            else
            {
              Serialprint("RING");
              if(longResponses)
              {
                Serial.print(" ");
                Serial.print(newClientNode->id);
              }
            }
            Serial.print(EOLN);
          }
          while((++i)<ringCounter);
          
          lastServerClientId = newClientNode->id;
          if(ringCounter > 0)
          {
            if(autoStreamMode)
            {
              sendConnectionNotice(baudRate);
              doAnswerCommand(0, (uint8_t *)"", 0, false, "");
              break;
            }
            else
              sendConnectionNotice(newClientNode->id);
          }
        }
      }
    }
    serv=serv->next;
  }
}

void ZCommand::loop()
{
  if((currentExpiresTimeMs > 0) && (millis() > currentExpiresTimeMs))
  {
    currentExpiresTimeMs = 0;
    if(strcmp((char *)nbuf,ECS)==0)
    {
      if(current != null)
      {
        if(!suppressResponses)
        {
          if(numericResponses)
            Serial.printf("3");
          else
          {
            Serialprint("NO CARRIER ");
            Serial.printf("%d %s:%d",current->id,current->host,current->port);
          }
          Serial.print(EOLN);
        }
        delete current;
        current = conns;
        nextConn = conns;
      }
      memset(nbuf,0,MAX_COMMAND_SIZE);
      eon=0;
    }
  }
  
  acceptNewConnection();
  if((flowControlType==FCT_DISABLED)||(XON))
  {
    sendNextPacket();
  } //TODO: consider local buffering with XOFF, until then, trust the socket buffers.
  checkBaudChange();
}

