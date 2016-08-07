

byte ZCommand::CRC8(const byte *data, byte len) 
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

ZResult ZCommand::doResetCommand(bool quiet)
{
  while(conns != null)
  {
    WiFiClientNode *c=conns;
    delete c;
  }
  while(servs != null)
  {
    WiFiServerNode *s=servs;
    delete s;
  }
  echoOn=false;
  eon=0;
  XON=99;
  memset(nbuf,0,MAX_COMMAND_SIZE);
  if(!quiet)
  {
    Serial.print(EOLN);
    Serial.print("ZiModem v");
    Serial.print(ZIMODEM_VERSION);
    Serial.print(EOLN);
    Serial.printf("sdk=%s core=%s cpu@%d%s",ESP.getSdkVersion(),ESP.getCoreVersion().c_str(),ESP.getCpuFreqMHz(),EOLN.c_str());
    Serial.printf("flash chipid=%d size=%d rsize=%d speed=%d%s",ESP.getFlashChipId(),ESP.getFlashChipSize(),ESP.getFlashChipRealSize(),ESP.getFlashChipSpeed(),EOLN.c_str());
  }
  return ZOK;
}

void ZCommand::reSaveConfig()
{
  File f = SPIFFS.open("/zconfig.txt", "w");
  f.printf("%s,%s,%d,%s",wifiSSI.c_str(),wifiPW.c_str(),baudRate,EOLN.c_str());
  f.close();
}

void ZCommand::loadConfig()
{
  if(!SPIFFS.exists("/zconfig.txt"))
  {
    SPIFFS.format();
    reSaveConfig();
  }
  else
  {
    String argv[10];
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
    if(argv[2].length()>0)
      baudRate=atoi(argv[2].c_str());
    if(commandMode.baudRate <= 0)
      baudRate=115200;
    wifiSSI=argv[0];
    wifiPW=argv[1];
    if(argv[3].length()>0)
      EOLN = argv[3];
  }
}


ZResult ZCommand::doBaudCommand(int vval, uint8_t *vbuf, int vlen)
{
  if(vval<=0)
    return ZERROR;
  else
  {
      baudRate=vval;
      Serial.begin(baudRate);
  }
  return ZOK;
}

ZResult ZCommand::doConnectCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  if(vlen == 0)
  {
    if(current == null)
      return ZERROR;
    else
    {
      if(current->isConnected())
        Serial.printf("CONNECT %d %s:%d%s",current->id,current->host,current->port,EOLN.c_str());
      else
        Serial.printf("NO CARRIER %d %s:%d%s",current->id,current->host,current->port,EOLN.c_str());
      return ZIGNORE;
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
          Serial.printf("CONNECTED %d %s:%d%s",c->id,c->host,c->port,EOLN.c_str());
        else
          Serial.printf("NO CARRIER %d %s:%d%s",c->id,c->host,c->port,EOLN.c_str());
        c=c->next;
      }
      return ZERROR;
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
      return ZERROR;
    }
    else
    {
      current=c;
      return ZCONNECT;
    }
  }
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
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.print((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      Serial.print(EOLN);
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
        reSaveConfig();
      }
    }
  }
  return ZOK;
}

ZResult ZCommand::doTransmitCommand(int vval, uint8_t *vbuf, int vlen)
{
  if((vlen==0)||(current==null)||(!current->isConnected()))
    return ZERROR;
  else
  if(vval>0)
  {
    uint8_t buf[vval];
    int recvd = Serial.readBytes(buf,vval);
    if(recvd == vval)
      current->client->write(buf,recvd);
    else
      return ZERROR;
  }
  else
  {
    uint8_t buf[vlen];
    memcpy(buf,vbuf,vlen);
    current->client->write(buf,vlen);
    current->client->print(EOLN);
  }
  return ZOK;
}

ZResult ZCommand::doDialStreamCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber,const char *dmodifiers)
{
  if(vlen == 0)
  {
    if((current == null)||(!current->isConnected()))
      return ZERROR;
    else
    {
      currMode=&streamMode;
      bool doPETSCII = strchr(dmodifiers,'P')||strchr(dmodifiers,'p');
      bool doTelnet = strchr(dmodifiers,'T')||strchr(dmodifiers,'t');
      streamMode.reset(current,false,doPETSCII,doTelnet,flowControl);
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
      currMode=&streamMode;
      bool doPETSCII = strchr(dmodifiers,'P')||strchr(dmodifiers,'p');
      bool doTelnet = strchr(dmodifiers,'T')||strchr(dmodifiers,'t');
      streamMode.reset(current,false,doPETSCII,doTelnet,flowControl);
    }
    else
      return ZERROR;
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
      return ZERROR;
    }
    else
    {
      currMode=&streamMode;
      bool doPETSCII = strchr(dmodifiers,'P')||strchr(dmodifiers,'p');
      bool doTelnet = strchr(dmodifiers,'T')||strchr(dmodifiers,'t');
      streamMode.reset(current,true,doPETSCII,doTelnet,flowControl);
    }
  }
  return ZOK;
}

bool ZCommand::readSerialStream()
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
          Serial.print(EOLN);
        crReceived=true;
        break;
      }
    }
    
    if(c>0)
    {
      if(c!='+')
        lastNonPlusTimeMs=millis();
      if((c==19)&&(flowControl))
        XON=false;
      else
      if((c==17)&&(flowControl))
        XON=true;
      else
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
  }
  return crReceived;
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
      return ZERROR;
  else
  {
    WiFiServerNode *newServer = new WiFiServerNode(vval);
    return ZOK;
  }
}

ZResult ZCommand::doHangupCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  if(isNumber && (vval == 0))
  {
      if(current != 0)
      {
        if(!suppressResponses)
        {
          if(numericResponses)
            Serial.printf("3");
          else
            Serial.printf("NO CARRIER %d %s:%d%s",current->id,current->host,current->port);
          Serial.print(EOLN);
        }
        delete current;
        current = conns;
        return ZIGNORE;
      }
      return ZERROR;
  }
  else
  if(vval > 0)
  {
      if(current != 0)
      {
        if(current->isConnected())
          return ZOK;
        if(current->client == null)
          return ZERROR;
        if(current->client->connect(current->host,current->port))
        {
          if(!suppressResponses)
          {
            if(numericResponses)
              Serial.print(longResponses?"5":"1");
            else
            if(longResponses)
              Serial.printf("CONNECT %d",baudRate);
            else
              Serial.print("CONNECT");
            Serial.print(EOLN);
            return ZIGNORE;
          }
        }
      }
      return ZERROR;
  }
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
        EOLN = "\r";
        break;
      case 1:
        EOLN = "\r\n";
        break;
      case 2:
        EOLN = "\n\r";
        break;
      case 3:
        EOLN = "\n";
        break;
      }
      reSaveConfig();
      return ZOK;
    }
  }
  return ZERROR;
}

ZResult ZCommand::doSerialCommand()
{
  int len=eon;
  uint8_t sbuf[len];
  memcpy(sbuf,nbuf,len);
  memset(nbuf,0,MAX_COMMAND_SIZE);
  eon=0;
  String currentCommand = (char *)sbuf;
  
  ZResult result=ZOK;
  if((len>1)
  &&((sbuf[0]=='A')||(sbuf[0]=='a'))
  &&((sbuf[1]=='T')||(sbuf[1]=='t')))
  {
    if(len > 2)
    {
      int index=2;
      char lastCmd=' ';
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
          result = doResetCommand(false);
          break;
        case 'a':
        case 'A':
          result = doAnswerCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str());
          break;
        case 'e':
        case 'E':
          echoOn=(vval > 0);
          break;
        case 'f':
        case 'F':
          flowControl=(vval > 0);
          break;
        case 'x':
        case 'X':
          longResponses = (vval > 0);
          break;
        case 'r':
        case 'R':
          result = doEOLNCommand(vval,vbuf,vlen,isNumber);
          break;
        case 'b':
        case 'B':
          result = doBaudCommand(vval,vbuf,vlen);
          break;
        case 't':
        case 'T':
          result = doTransmitCommand(vval,vbuf,vlen);
          break;
        case 'h':
        case 'H':
          result = doHangupCommand(vval,vbuf,vlen,isNumber);
          break;
        case 'd':
        case 'D':
          result = doDialStreamCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str());
          break;
        case 'o':
        case 'O':
        case 'c':
        case 'C':
          result = doConnectCommand(vval,vbuf,vlen,isNumber);
          break;
        case 'w':
        case 'W':
          result = doWiFiCommand(vval,vbuf,vlen);
          break;
        case 'v':
        case 'V':
          numericResponses = (isNumber && (vval == 0));
          break;
        case 'q':
        case 'Q':
          suppressResponses = (vval > 0);
          break;
        default:
          break;
        }
      }
    }
  }
  else
    result = ZERROR;

  if(result != ZIGNORE_SPECIAL)
    previousCommand = currentCommand;
  if(!suppressResponses)
  {
    switch(result)
    {
    case ZOK:
      if(numericResponses)
        Serial.print("0");
      else
        Serial.print("OK");
      Serial.print(EOLN);
      break;
    case ZERROR:
      if(numericResponses)
        Serial.print("4");
      else
        Serial.print("ERROR");
      Serial.print(EOLN);
      break;
    case ZCONNECT:
      if(numericResponses)
        Serial.print(longResponses?"5":"1");
      else
      {
        Serial.print("CONNECT");
        if(longResponses)
          Serial.print(" " + baudRate);
      }
      Serial.print(EOLN);
      break;
    default:
      break;
    }
  }
  return result;
}

void ZCommand::serialIncoming()
{
  bool crReceived=readSerialStream();
  if(currentExpiresTimeMs > 0)
    currentExpiresTimeMs = 0;
  if((strcmp((char *)nbuf,"+++")==0)&&((millis()-lastNonPlusTimeMs)>1000))
    currentExpiresTimeMs = millis() + 1000;
  if(!crReceived)
    return;
  doSerialCommand();
}

void ZCommand::loop()
{
  if((currentExpiresTimeMs > 0) && (millis() > currentExpiresTimeMs))
  {
    currentExpiresTimeMs = 0;
    if(strcmp((char *)nbuf,"+++")==0)
    {
      if(current != 0)
      {
        if(!suppressResponses)
        {
          if(numericResponses)
            Serial.printf("3");
          else
            Serial.printf("NO CARRIER %d %s:%d%s",current->id,current->host,current->port);
          Serial.print(EOLN);
        }
        delete current;
        current = conns;
      }
      memset(nbuf,0,MAX_COMMAND_SIZE);
      eon=0;
    }
  }
  
  if((!flowControl)||(XON))
  {
    WiFiServerNode *serv = servs;
    while(serv != null)
    {
      WiFiClient newClient = serv->server->available();
      if((newClient != null)&&(newClient.connected()))
      {
        WiFiClientNode *newClientNode = new WiFiClientNode(&newClient);
        if(numericResponses)
          Serial.print(longResponses?"5":"1");
        else
        {
          Serial.print("CONNECT");
          if(longResponses)
            Serial.print(" " + baudRate);
        }
        Serial.print(EOLN);
      }
      serv=serv->next;
    }
    
    WiFiClientNode *firstConn = nextConn;
    if((nextConn == null)||(nextConn->next == null))
      nextConn = conns;
    else
      nextConn = nextConn->next;
    while(XON && (nextConn != null))
    {
      if(!nextConn->isConnected())
      {
        if(nextConn->wasConnected)
        {
          if(!suppressResponses)
          {
            if(numericResponses)
              Serial.printf("3");
            else
              Serial.printf("NO CARRIER %d",nextConn->id);
            Serial.print(EOLN);
          }
          nextConn->wasConnected=false;
        }
      }
      else
      if(nextConn->client->available()>0)
      {
        int maxBytes=256;
        if(nextConn->client->available()<maxBytes)
          maxBytes=nextConn->client->available();
        uint8_t buf[maxBytes];
        nextConn->client->read(buf,maxBytes);
        uint8_t crc=CRC8(buf,maxBytes);
        Serial.printf("[ %d %d %d ]%s",nextConn->id,maxBytes,(int)crc,EOLN.c_str());
        Serial.write(buf,maxBytes);
        Serial.flush();
        break;
      }
      if((nextConn == null)||(nextConn->next == null))
        nextConn = conns;
      else
        nextConn = nextConn->next;
      if(nextConn == firstConn)
        break;
    }
  } //TODO: consider local buffering with XOFF, until then, trust the socket buffers.
}

