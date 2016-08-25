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

void ZCommand::setConfigDefaults()
{
  doEcho=false;
  doFlowControl=false;
  EOLN = "\r\n";
  suppressResponses=false;
  numericResponses=false;
  longResponses=true;
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
  String argv[10];
  parseConfigOptions(argv);
  eon=0;
  XON=true;
  singlePacket=false;
  setBaseConfigOptions(argv);
  memset(nbuf,0,MAX_COMMAND_SIZE);
  showInitMessage();
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
  File f = SPIFFS.open("/zconfig.txt", "w");
  int flowControl=(doFlowControl && singlePacket) ? 2 : doFlowControl;
  f.printf("%s,%s,%d,%s,%d,%d,%d,%d,%d",wifiSSI.c_str(),wifiPW.c_str(),baudRate,EOLN.c_str(),flowControl,doEcho,suppressResponses,numericResponses,longResponses);
  f.close();
}

void ZCommand::setBaseConfigOptions(String configArguments[])
{
  if(configArguments[CFG_EOLN].length()>0)
    EOLN = configArguments[CFG_EOLN];
  if(configArguments[CFG_FLOWCONTROL].length()>0)
  {
    int x = atoi(configArguments[CFG_FLOWCONTROL].c_str());
    if(x == 2)
    {
      doFlowControl =true;
      singlePacket = true;
    }
    else
    {
      doFlowControl = x;
      singlePacket = false;
    }
    XON=true;
  }
  if(configArguments[CFG_ECHO].length()>0)
    doEcho = atoi(configArguments[CFG_ECHO].c_str());
  if(configArguments[CFG_RESP_SUPP].length()>0)
    suppressResponses = atoi(configArguments[CFG_RESP_SUPP].c_str());
  if(configArguments[CFG_RESP_NUM].length()>0)
    numericResponses = atoi(configArguments[CFG_RESP_NUM].c_str());
  if(configArguments[CFG_RESP_LONG].length()>0)
    longResponses = atoi(configArguments[CFG_RESP_LONG].c_str());
}

void ZCommand::parseConfigOptions(String configArguments[])
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
    Serial.begin(115200);  //Start Serial
  }
  String argv[10];
  parseConfigOptions(argv);
  if(argv[CFG_BAUDRATE].length()>0)
    baudRate=atoi(argv[CFG_BAUDRATE].c_str());
  if(baudRate <= 0)
    baudRate=115200;
  Serial.begin(baudRate);  //Start Serial
  wifiSSI=argv[CFG_WIFISSI];
  wifiPW=argv[CFG_WIFIPW];
  if(wifiSSI.length()>0)
  {
    connectWifi(wifiSSI.c_str(),wifiPW.c_str());
  }
  doResetCommand();
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
        Serial.printf("CONNECTED %d %s:%d",current->id,current->host,current->port);
      else
        Serial.printf("NO CARRIER %d %s:%d",current->id,current->host,current->port);
      Serial.print(EOLN);
      return ZIGNORE;
    }
  }
  else
  if((vval >= 0)&&(isNumber))
  {
    WiFiClientNode *c=conns;
    if(vval > 0)
    {
      while((c!=null)&&(c->id != vval))
        c=c->next;
      if((c!=null)&&(c->id == vval))
        current = c;
      else
        return ZERROR;
    }
    else
    {
      c=conns;
      while(c!=null)
      {
        if(c->isConnected())
          Serial.printf("CONNECTED %d %s:%d",c->id,c->host,c->port);
        else
          Serial.printf("NO CARRIER %d %s:%d",c->id,c->host,c->port);
        Serial.print(EOLN);
        c=c->next;
      }
      WiFiServerNode *s=servs;
      while(s!=null)
      {
        Serial.printf("LISTENING %d *:%d",s->id,s->port);
        Serial.print(EOLN);
        s=s->next;
      }
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
    current->client->print("\r\n"); // special case
  }
  return ZOK;
}

ZResult ZCommand::doDialStreamCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber,const char *dmodifiers)
{
  bool doPETSCII = (strchr(dmodifiers,'P')!=null)||(strchr(dmodifiers,'p')!=null);
  bool doTelnet = (strchr(dmodifiers,'T')!=null)||(strchr(dmodifiers,'t')!=null);
  if(vlen == 0)
  {
    if((current == null)||(!current->isConnected()))
      return ZERROR;
    else
    {
      currMode=&streamMode;
      streamMode.reset(current,false,doPETSCII,doTelnet);
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
      streamMode.reset(c,false,doPETSCII,doTelnet);
      currMode=&streamMode;
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
      current=c;
      streamMode.reset(c,true,doPETSCII,doTelnet);
      currMode=&streamMode;
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
        if(doEcho)
          Serial.print(EOLN);
        crReceived=true;
        break;
      }
    }
    
    if(c>0)
    {
      if(c!='+')
        lastNonPlusTimeMs=millis();
      if((c==19)&&(doFlowControl))
        XON=false;
      else
      if((c==17)&&(doFlowControl))
        XON=true;
      else
      {
        if(doEcho)
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
  if(cnode->lastPacketLen == 0) // means it was never used!
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
  int index=0;
  while((index<len-1)
  &&(((sbuf[index]!='A')&&(sbuf[index]!='a'))
    ||((sbuf[index+1]!='T')&&(sbuf[index+1]!='t'))))
      index++;

  if((index<len-1)
  &&((sbuf[index]=='A')||(sbuf[index]=='a'))
  &&((sbuf[index+1]=='T')||(sbuf[index+1]=='t')))
  {
    index+=2;
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
      if((lastCmd=='&')&&(index<len))
      {
        index++;//protect our one and only letter.
        vlen++;
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
        if((lastCmd=='d')||(lastCmd=='D'))
        {
          const char *DMODIFIERS="LPRTW,lprtw";
          while((index<len)&&(strchr(DMODIFIERS,sbuf[index])!=null))
            dmodifiers += (char)sbuf[index++];
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
              isNumber = (sbuf[i]>='0') && (sbuf[i]<='9') && isNumber;
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
        result = doResetCommand();
        break;
      case 'n':
      case 'N':
        if(isNumber && (vval == 0))
        {
          doNoListenCommand();
          break;
        }
      case 'a':
      case 'A':
        result = doAnswerCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str());
        break;
      case 'e':
      case 'E':
        if(!isNumber)
          result=ZERROR;
        else
          doEcho=(vval > 0);
        break;
      case 'f':
      case 'F':
        if(!isNumber)
          result=ZERROR;
        else
        {
          singlePacket = (vval == 2);
          doFlowControl = (vval > 0);
        }
        break;
      case 'x':
      case 'X':
        if(!isNumber)
          result=ZERROR;
        else
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
        result = isNumber ? ZOK : ZERROR;
        break;
      case 'c':
      case 'C':
        result = doConnectCommand(vval,vbuf,vlen,isNumber);
        break;
      case 'i':
      case 'I':
        showInitMessage();
        break;
      case 'l':
      case 'L':
        doLastPacket(vval,vbuf,vlen,isNumber);
        break;
      case 'm':
      case 'M':
      case 'y':
      case 'Y':
        result = isNumber ? ZOK : ZERROR;
        break;
      case 'w':
      case 'W':
        result = doWiFiCommand(vval,vbuf,vlen);
        break;
      case 'v':
      case 'V':
        if(!isNumber)
          result=ZERROR;
        else
          numericResponses = (vval == 0);
        break;
      case 'q':
      case 'Q':
        if(!isNumber)
          result=ZERROR;
        else
          suppressResponses = (vval > 0);
        break;
      case 's':
      case 'S':
        {
          if(vlen<3)
            result=ZERROR;
          else
          {
            char *eq=strchr((char *)vbuf,'=');
            if((eq == null)||(eq == (char *)vbuf)||(eq==(char *)&(vbuf[vlen-1])))
              result=ZERROR;
            else
            {
              // ok enough for now
            }
          }
        }
        break;
      case '&':
        if(vlen > 0)
        {
          switch(vbuf[0])
          {
          case 'l':
          case 'L':
            loadConfig();
            break;
          case 'w':
          case 'W':
            reSaveConfig();
            break;
          default:
            result=ZERROR;
            break;
          }
        }
        else
          result=ZERROR;
        break;
      default:
        result=ZERROR;
        break;
      }
    }

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
          if(numericResponses)
            Serial.print("0");
          else
            Serial.print("OK");
          Serial.print(EOLN);
        }
        break;
      case ZERROR:
        if(numericResponses)
          Serial.print("4");
        else
          Serial.print("ERROR");
        Serial.print(EOLN);
        // on error, cut and run
        return ZERROR;
      case ZCONNECT:
        if(numericResponses)
          Serial.print(longResponses?"5":"1");
        else
        {
          Serial.print("CONNECT");
          if(longResponses)
          {
            Serial.print(" ");
            if(current != null)
              Serial.print(current->id);
            else
              Serial.print(baudRate);
          }
        }
        Serial.print(EOLN);
        break;
      default:
        break;
      }
    }
  }

  return result;
}

void ZCommand::reSendLastPacket(WiFiClientNode *conn)
{
  uint8_t crc=CRC8(conn->lastPacketBuf,conn->lastPacketLen);
  Serial.printf("[ %d %d %d ]",conn->id,conn->lastPacketLen,(int)crc);
  Serial.print(EOLN);
  Serial.write(conn->lastPacketBuf,conn->lastPacketLen);
  Serial.flush();
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
      if(current != null)
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
        nextConn = conns;
      }
      memset(nbuf,0,MAX_COMMAND_SIZE);
      eon=0;
    }
  }
  
  if((!doFlowControl)||(XON))
  {
    WiFiServerNode *serv = servs;
    while(serv != null)
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
          WiFiClient *newClientLoc=new WiFiClient(newClient);
          newClientLoc->setNoDelay(true);
          WiFiClientNode *newClientNode = new WiFiClientNode(newClientLoc);
          if(numericResponses)
            Serial.print(longResponses?"5":"1");
          else
          {
            Serial.print("CONNECT");
            if(longResponses)
            {
              Serial.print(" ");
              Serial.print(newClientNode->id);
            }
          }
          Serial.print(EOLN);
        }
      }
      serv=serv->next;
    }

    WiFiClientNode *firstConn = nextConn;
    if(firstConn == null)
      firstConn = conns;
    if((nextConn == null)||(nextConn->next == null))
      nextConn = conns;
    else
      nextConn = nextConn->next;

    while(XON && (nextConn != null))
    {
      if((nextConn->client != null) 
      && (nextConn->client->available()>0))
      {
        int availableBytes = nextConn->client->available();
        int maxBytes=256;
        if(availableBytes<maxBytes)
          maxBytes=availableBytes;
        nextConn->client->read(nextConn->lastPacketBuf,maxBytes);
        if(maxBytes > 0)
        {
          nextConn->lastPacketLen=maxBytes;
          reSendLastPacket(nextConn);
          if(singlePacket)
          {
            XON=false;
          }
        }
        break;
      }
      else
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
        if(nextConn->serverClient)
        {
          delete nextConn;
          nextConn = null;
          break; // messes up the order, so just leave and start over
        }
      }
      
      if(nextConn->next == null)
        nextConn = conns;
      else
        nextConn = nextConn->next;
      if(nextConn == firstConn)
        break;
    }
  } //TODO: consider local buffering with XOFF, until then, trust the socket buffers.
}

