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
extern "C" void esp_schedule();
extern "C" void esp_yield();

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

void ZCommand::setConfigDefaults()
{
  doEcho=true;
  doFlowControl=false;
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
  String argv[10];
  parseConfigOptions(argv);
  eon=0;
  XON=true;
  flowControlType=FCT_NORMAL;
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
  int flowControl = doFlowControl;
  switch(flowControlType)
  {
  case FCT_MANUAL:
    flowControl = 3;
    break;
  case FCT_AUTOOFF:
    flowControl = 2;
    break;
  }
  f.printf("%s,%s,%d,%s,%d,%d,%d,%d,%d",wifiSSI.c_str(),wifiPW.c_str(),baudRate,EOLN.c_str(),flowControl,doEcho,suppressResponses,numericResponses,longResponses);
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
        if((str[i]==',')&&(argn<9))
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
    EOLN = configArguments[CFG_EOLN];
  if(configArguments[CFG_FLOWCONTROL].length()>0)
  {
    int x = atoi(configArguments[CFG_FLOWCONTROL].c_str());
    switch(x)
    {
      case 2:
        doFlowControl =true;
        flowControlType = FCT_AUTOOFF;
        XON=true;
        break;
      case 3:
        doFlowControl =true;
        flowControlType = FCT_MANUAL;
        XON=false;
        break;
      default:
        doFlowControl = x;
        flowControlType = FCT_NORMAL;
        XON=true;
        break;
    }
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
  delay(500);
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
    Serial.begin(1200);  //Start Serial
  }
  String argv[10];
  parseConfigOptions(argv);
  if(argv[CFG_BAUDRATE].length()>0)
    baudRate=atoi(argv[CFG_BAUDRATE].c_str());
  if(baudRate <= 0)
    baudRate=1200;
  Serial.begin(baudRate);  //Start Serial
  wifiSSI=argv[CFG_WIFISSI];
  wifiPW=argv[CFG_WIFIPW];
  if(wifiSSI.length()>0)
  {
    connectWifi(wifiSSI.c_str(),wifiPW.c_str());
  }
  doResetCommand();
  showInitMessage();
}

void ZCommand::showAtStatusMessage()
{
  Serial.print("AT");
  Serial.print("B");
  Serial.print(baudRate);
  Serial.print(doEcho?"E1":"E0");
  if(suppressResponses)
    Serial.print("Q1");
  else
  {
    Serial.print("Q0");
    Serial.print(numericResponses?"V0":"V1");
    Serial.print(longResponses?"X1":"X0");
  }
  if(!doFlowControl)
    Serial.print("F0");
  else
  switch(flowControlType)
  {
  case FCT_NORMAL: 
    Serial.print("F1");
    break;
  case FCT_AUTOOFF:
    Serial.print("F2");
    break;
  case FCT_MANUAL:
    Serial.print("F3");
    break;
  }
  if(EOLN==CR)
    Serial.print("R0");
  else
  if(EOLN==CRLF)
    Serial.print("R1");
  else
  if(EOLN==LFCR)
    Serial.print("R2");
  else
  if(EOLN==LF)
    Serial.print("R3");

  Serial.print("S0=");
  Serial.print((int)ringCounter);
  Serial.print("S2=");
  Serial.print((int)EC);
  Serial.print("S3=");
  Serial.print((int)CR[0]);
  Serial.print("S4=");
  Serial.print((int)LF[0]);
  Serial.print("S5=");
  Serial.print((int)BS);
  Serial.print("S40=");
  Serial.print(packetSize);
  Serial.print(autoStreamMode ? "S41=1" : "S41=0");
  
  WiFiServerNode *serv = servs;
  while(serv != null)
  {
    Serial.print("A");
    Serial.print(serv->port);
    serv=serv->next;
  }
  
  Serial.println("");
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
  bool doPETSCII = (strchr(dmodifiers,'P') != null) || (strchr(dmodifiers,'p') != null);
  if(vlen == 0)
  {
    if(strlen(dmodifiers)>0)
      return ZERROR;
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
    if(strlen(dmodifiers)>0)
      return ZERROR;
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
    WiFiClientNode *c = new WiFiClientNode((char *)vbuf,port,doPETSCII);
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

ZResult ZCommand::doTransmitCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers, const int crc8)
{
  bool doPETSCII = (strchr(dmodifiers,'p')!=null);
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
    if(current->doPETSCII || doPETSCII)
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
    if(current->doPETSCII || doPETSCII)
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

ZResult ZCommand::doDialStreamCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber,const char *dmodifiers)
{
  bool doPETSCII = (strchr(dmodifiers,'p')!=null);
  bool doTelnet = (strchr(dmodifiers,'t')!=null);
  bool doBBS = (strchr(dmodifiers,'b')!=null);
  if(vlen == 0)
  {
    if((current == null)||(!current->isConnected()))
      return ZERROR;
    else
    {
      streamMode.switchTo(current,false,doPETSCII|| current->doPETSCII,doTelnet);
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
      current=c;
      streamMode.switchTo(c,false,doPETSCII || c->doPETSCII,doTelnet,doBBS);
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
    WiFiClientNode *c = new WiFiClientNode((char *)vbuf,port,doPETSCII);
    if(!c->isConnected())
    {
      delete c;
      return ZERROR;
    }
    else
    {
      current=c;
      streamMode.switchTo(c,true,doPETSCII,doTelnet,doBBS);
    }
  }
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
          streamMode.switchTo(c,false,false,false);
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
    
    WiFiServerNode *s=servs;
    while(s != null)
    {
      if(s->port == vval)
        return ZOK;
      s=s->next;
    }
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
  while(Serial.available()>0)
  {
    uint8_t c=Serial.read();
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
      if((c==19)&&(doFlowControl))
      {
        XON=false;
      }
      else
      if((c==17)&&(doFlowControl))
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
  return crReceived;
}

ZResult ZCommand::doSerialCommand()
{
  int len=eon;
  uint8_t sbuf[len];
  memcpy(sbuf,nbuf,len);
  memset(nbuf,0,MAX_COMMAND_SIZE);
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
    int vstart=0;
    int vlen=0;
    String dmodifiers="";
    while(index<len)
    {
      lastCmd=lc(sbuf[index++]);
      vstart=index;
      vlen=0;
      bool isNumber=true;
      if((lastCmd=='&')&&(index<len))
      {
        index++;//protect our one and only letter.
        vstart++;
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
        || (lastCmd=='t')||(lastCmd=='T'))
        {
          const char *DMODIFIERS=",lbprtw";
          while((index<len)&&(strchr(DMODIFIERS,lc(sbuf[index]))!=null))
            dmodifiers += lc((char)sbuf[index++]);
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
        &&(!((lc(sbuf[index])>='a')&&(lc(sbuf[index])<='z'))))
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
      
      if(logFileOpen)
      {
        if(vlen > 0)
          logFile.printf("Proc: %c %d '%s'\r\n",lastCmd,vval,vbuf);
        else
          logFile.printf("Proc: %c %d ''\r\n",lastCmd,vval);
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
        if(!isNumber)
          result=ZERROR;
        else
        {
          flowControlType = FCT_NORMAL;
          if(vval == 2)
            flowControlType = FCT_AUTOOFF;
          else
          if(vval == 3)
          {
            flowControlType = FCT_MANUAL;
            XON=false;
          }
          doFlowControl = (vval > 0);
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
      case 'o':
        if((vlen == 0)&&(!isNumber))
        {
          result = ZOK;
          if(current == null)
            result = ZERROR;
          else
            streamMode.switchTo(current);
        }
        else
          result = isNumber ? ZOK : ZERROR;
        break;
      case 'c':
        result = doConnectCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str());
        break;
      case 'i':
        if(vval == 0)
          showInitMessage();
        else
          showAtStatusMessage();
        break;
      case 'l':
        doLastPacket(vval,vbuf,vlen,isNumber);
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
             default:
                break;
              }
            }
          }
        }
        break;
      case '&':
        switch(lc(sbuf[vstart-1]))
        {
        case 'l':
          loadConfig();
          break;
        case 'w':
          reSaveConfig();
          break;
        case 'g':
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
                  Serial.write(buf[i++]);
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
        sendConnectionNotice((current == null) ? baudRate : current->id);
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
  if(conn == NULL)
  {
    Serial.print("[ 0 0 0 ]");
    Serial.print(EOLN);
    if(logFileOpen)
        logFile.printf("SER-OUT: [ 0 0 0 ]\r\n");
  }
  else
  if(conn->lastPacketLen == 0) // never used, or empty
  {
    Serial.printf("[ %d %d %d ]",conn->id,conn->lastPacketLen,0);
    Serial.print(EOLN);
    if(logFileOpen)
        logFile.printf("SER-OUT: [ %d %d 0 ]\r\n",conn->id,conn->lastPacketLen);
  }
  else
  {
    uint8_t crc=CRC8(conn->lastPacketBuf,conn->lastPacketLen);
    Serial.printf("[ %d %d %d ]",conn->id,conn->lastPacketLen,(int)crc);
    Serial.print(EOLN);
    if(logFileOpen)
        logFile.printf("SER-OUT: [ %d %d %d ]\r\n",conn->id,conn->lastPacketLen,(int)crc);
    Serial.write(conn->lastPacketBuf,conn->lastPacketLen);
    if(logFileOpen)
    {
        int c=0;
        logFile.print("SER-OUT: ");
        for(int i=0;i<conn->lastPacketLen;i++)
        {
            logFile.print(TOHEX(conn->lastPacketBuf[i]));
            if((++c)>20)
            {
                logFile.print("\r\nSER-OUT: ");
                c=0;
            }
            else
                logFile.print(" ");
        }
        logFile.print("\r\n");
    }
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
  WiFiClientNode *firstConn = nextConn;
  if(firstConn == null)
    firstConn = conns;
  if((nextConn == null)||(nextConn->next == null))
    nextConn = conns;
  else
    nextConn = nextConn->next;

  while(XON && (nextConn != null))
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
        maxBytes = nextConn->read(nextConn->lastPacketBuf,maxBytes);
        if(nextConn->doPETSCII)
        {
          int bytesToRead=maxBytes;
          for(int i=0, b=0;i<bytesToRead;i++,b++)
          {
            nextConn->lastPacketBuf[b]=nextConn->lastPacketBuf[i];
            if(!ascToPet((char *)&nextConn->lastPacketBuf[b],nextConn))
            {
              b--;
              maxBytes--;
            }
          }
        }
        nextConn->lastPacketLen=maxBytes;
        reSendLastPacket(nextConn);
        if(flowControlType != FCT_NORMAL)
        {
          XON=false;
        }
        if(flowControlType == FCT_MANUAL)
        {
          return;
        }
      }
      break;
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
            Serial.printf("NO CARRIER %d",nextConn->id);
          Serial.print(EOLN);
          if(flowControlType == FCT_MANUAL)
          {
            return;
          }
        }
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
  if(flowControlType == FCT_MANUAL)
  {
    XON=false;
    firstConn = conns;
    while(firstConn != NULL)
    {
      firstConn->lastPacketLen = 0;
      firstConn = firstConn->next;
    }
    Serial.print("[ 0 0 0 ]");
    Serial.print(EOLN);
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
    Serial.print("CONNECT");
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
          WiFiClientNode *newClientNode = new WiFiClientNode(newClient);
          int i=0;
          do
          {
            if(numericResponses)
              Serial.print("2");
            else
            {
              Serial.print("RING");
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
    acceptNewConnection();
    
    sendNextPacket();
    
  } //TODO: consider local buffering with XOFF, until then, trust the socket buffers.
}

