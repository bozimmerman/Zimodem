

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

void ZCommand::reset()
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

ZResult ZCommand::doBaudCommand(int vval, uint8_t *vbuf, int vlen)
{
  if(vval<=0)
    return ZERROR;
  else
  {
      baudRate=vval;
      Serial.begin(baudRate);
      File f = SPIFFS.open("/zconfig.txt", "w");
      f.printf("%s,%s,%d",wifiSSI.c_str(),wifiPW.c_str(),baudRate);
      f.close();
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
        Serial.printf("CONNECTED %d %s:%d\r\n",current->id,current->host,current->port);
      else
        Serial.printf("NOCARRIER %d %s:%d\r\n",current->id,current->host,current->port);
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
          Serial.printf("CONNECTED %d %s:%d\r\n",c->id,c->host,c->port);
        else
          Serial.printf("NOCARRIER %d %s:%d\r\n",c->id,c->host,c->port);
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
      current=c;
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
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
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
        File f = SPIFFS.open("/zconfig.txt", "w");
        wifiSSI=ssi;
        wifiPW=pw;
        f.printf("%s,%s,%d",ssi,pw,baudRate);
        f.close();
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
    current->client->write("\n\r",2);
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
      streamMode.reset(current,false,doPETSCII,doTelnet,echoOn);
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
      streamMode.reset(current,false,doPETSCII,doTelnet,echoOn);
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
      streamMode.reset(current,true,doPETSCII,doTelnet,echoOn);
    }
  }
  return ZOK;
}

void ZCommand::serialIncoming()
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
      ZResult result=ZOK;
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
          result = doBaudCommand(vval,vbuf,vlen);
          break;
        case 't':
        case 'T':
          result = doTransmitCommand(vval,vbuf,vlen);
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
        default:
          break;
        }
      }
      switch(result)
      {
      case ZOK:
        Serial.println("OK");
        break;
      case ZERROR:
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

void ZCommand::loop()
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

