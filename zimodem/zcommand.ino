// CoCoWiFI modifications by Allen C. Huffman of www.subethasoftware.com
/*
   Copyright 2016-2017 Bo Zimmerman

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at


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
  strcpy(CRLF,"\r\n");
  strcpy(LFCR,"\r\n");
  strcpy(LF,"\n");
  strcpy(CR,"\r");
  strcpy(ECS,"+++");
  freeCharArray(&tempMaskOuts);
  freeCharArray(&tempDelimiters);
  setCharArray(&delimiters,"");
  setCharArray(&maskOuts,"");
}

byte ZCommand::CRC8(const byte *data, byte len) 
{
  byte crc = 0x00;
  logPrint("CRC8: ");
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
  logPrintf("\r\nFinal CRC8: %s\r\n",TOHEX(crc));
  return crc;
}

void ZCommand::setConfigDefaults()
{
  doEcho=true;
  autoStreamMode=false;
  preserveListeners=false;
  ringCounter=1;
  serial.setFlowControlType(DEFAULT_FCT);
  serial.setXON(true);
  packetXOn = true;
  serial.setPetsciiMode(false);
  binType=BTYPE_NORMAL;
  serialDelayMs=0;
  dcdActive=DEFAULT_DCD_HIGH;
  dcdInactive=DEFAULT_DCD_HIGH;
  ctsActive=DEFAULT_CTS_HIGH;
  ctsInactive=DEFAULT_CTS_LOW;
  rtsActive=DEFAULT_RTS_HIGH;
  rtsInactive=DEFAULT_RTS_LOW;
  riActive = DEFAULT_RTS_HIGH;
  riInactive = DEFAULT_RTS_LOW;
  dtrActive = DEFAULT_RTS_HIGH;
  dtrInactive = DEFAULT_RTS_LOW;
  dsrActive = DEFAULT_RTS_HIGH;
  dsrInactive = DEFAULT_RTS_LOW;
  pinDCD = DEFAULT_PIN_DCD;
  pinCTS = getDefaultCtsPin();
  pinRTS = DEFAULT_PIN_RTS;
  pinDTR = DEFAULT_PIN_DTR;
  pinDSR = DEFAULT_PIN_DSR;
  pinRI = DEFAULT_PIN_RI;
  dcdStatus = dcdInactive;
  if(pinSupport[pinRTS])
    pinMode(pinRTS,OUTPUT);
  if(pinSupport[pinCTS])
    pinMode(pinCTS,INPUT);
  if(pinSupport[pinDCD])
    pinMode(pinDCD,OUTPUT);
  if(pinSupport[pinDTR])
    pinMode(pinDTR,INPUT);
  if(pinSupport[pinDSR])
    pinMode(pinDSR,OUTPUT);
  if(pinSupport[pinRI])
    pinMode(pinRI,OUTPUT);
  if(pinSupport[pinRTS])
    digitalWrite(pinRTS,rtsActive);
  if(pinSupport[pinDCD])
    digitalWrite(pinDCD,dcdStatus);
  if(pinSupport[pinDSR])
    digitalWrite(pinDSR,dsrActive);
  if(pinSupport[pinRI])
    digitalWrite(pinRI,riInactive);
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
  WiFiServerNode::DestroyAllServers();
  setConfigDefaults();
  String argv[CFG_LAST+1];
  parseConfigOptions(argv);
  eon=0;
  serial.setXON(true);
  packetXOn = true;
  serial.setPetsciiMode(false);
  serialDelayMs=0;
  binType=BTYPE_NORMAL;
  serial.setFlowControlType(DEFAULT_FCT);
  setOptionsFromSavedConfig(argv);
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
  WiFiServerNode::DestroyAllServers();
  return ZOK;
}

FlowControlType ZCommand::getFlowControlType()
{
  return serial.getFlowControlType();
}

int pinModeCoder(int activeChk, int inactiveChk, int activeDefault)
{
  if(activeChk == activeDefault)
  {
    if(inactiveChk == activeDefault)
      return 2;
    return 0;
  }
  else
  {
    if(inactiveChk != activeDefault)
      return 3;
    return 1;
  }
}

void pinModeDecoder(int mode, int *active, int *inactive, int activeDef, int inactiveDef)
{
  switch(mode)
  {
  case 0:
    *active = activeDef;
    *inactive = inactiveDef;
    break;
  case 1:
    *inactive = activeDef;
    *active = inactiveDef;
    break;
  case 2:
    *active = activeDef;
    *inactive = activeDef;
    break;
  case 3:
    *active = inactiveDef;
    *inactive = inactiveDef;
    break;
  default:
    *active = activeDef;
    *inactive = inactiveDef;
    break;
  }
}

void pinModeDecoder(String newMode, int *active, int *inactive, int activeDef, int inactiveDef)
{
  if(newMode.length()>0)
  {
    int mode = atoi(newMode.c_str());
    pinModeDecoder(mode,active,inactive,activeDef,inactiveDef);
  }
}

void ZCommand::reSaveConfig()
{
  SPIFFS.remove("/zconfig.txt");
  delay(500);
  File f = SPIFFS.open("/zconfig.txt", "w");
  const char *eoln = EOLN.c_str();
  int dcdMode = pinModeCoder(dcdActive, dcdInactive, DEFAULT_DCD_HIGH);
  int ctsMode = pinModeCoder(ctsActive, ctsInactive, DEFAULT_CTS_HIGH);
  int rtsMode = pinModeCoder(rtsActive, rtsInactive, DEFAULT_RTS_HIGH);
  int riMode = pinModeCoder(riActive, riInactive, DEFAULT_RTS_HIGH);
  int dtrMode = pinModeCoder(dtrActive, dtrInactive, DEFAULT_DTR_HIGH);
  int dsrMode = pinModeCoder(dsrActive, dsrInactive, DEFAULT_DSR_HIGH);
  f.printf("%s,%s,%d,%s,"
           "%d,%d,%d,%d,"
           "%d,%d,%d,%d,%d,"
           "%d,%d,%d,%d,%d,%d,%d,"
           "%d,%d,%d,%d,%d,%d,"
           "%d,"
           "%s,%s", 
            wifiSSI.c_str(), wifiPW.c_str(), baudRate, eoln,
            serial.getFlowControlType(), doEcho, suppressResponses, numericResponses,
            longResponses, serial.isPetsciiMode(), dcdMode, serialConfig, ctsMode,
            rtsMode,pinDCD,pinCTS,pinRTS,autoStreamMode,ringCounter,preserveListeners,
            riMode,dtrMode,dsrMode,pinRI,pinDTR,pinDSR,
            zclock.isDisabled()?999:zclock.getTimeZoneCode(),
            zclock.getFormat().c_str(),zclock.getNtpServerHost().c_str()
            );
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
      debugPrintf("Saved Config: %s\n",str.c_str());
      for(int i=0;i<str.length();i++)
      {
        if((str[i]==',')&&(argn<=CFG_LAST))
          argn++;
      }
    }
    if(argn!=CFG_LAST)
    {
      delay(100);
      yield();
      reSaveConfig();
    }
  }
}

void ZCommand::setOptionsFromSavedConfig(String configArguments[])
{
  if(configArguments[CFG_EOLN].length()>0)
  {
    EOLN = configArguments[CFG_EOLN];
  }
  if(configArguments[CFG_FLOWCONTROL].length()>0)
  {
    int x = atoi(configArguments[CFG_FLOWCONTROL].c_str());
    if((x>=0)&&(x<FCT_INVALID))
      serial.setFlowControlType((FlowControlType)x);
    else
      serial.setFlowControlType(FCT_DISABLED);
    serial.setXON(true);
    packetXOn = true;
    if(serial.getFlowControlType() == FCT_MANUAL)
      packetXOn = false;
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
    serial.setPetsciiMode(atoi(configArguments[CFG_PETSCIIMODE].c_str()));
  pinModeDecoder(configArguments[CFG_DCDMODE],&dcdActive,&dcdInactive,DEFAULT_DCD_HIGH,DEFAULT_DCD_LOW);
  pinModeDecoder(configArguments[CFG_CTSMODE],&ctsActive,&ctsInactive,DEFAULT_CTS_HIGH,DEFAULT_CTS_LOW);
  pinModeDecoder(configArguments[CFG_RTSMODE],&rtsActive,&rtsInactive,DEFAULT_RTS_HIGH,DEFAULT_RTS_LOW);
  pinModeDecoder(configArguments[CFG_RIMODE],&riActive,&riInactive,DEFAULT_RI_HIGH,DEFAULT_RI_LOW);
  pinModeDecoder(configArguments[CFG_DTRMODE],&dtrActive,&dtrInactive,DEFAULT_DTR_HIGH,DEFAULT_DTR_LOW);
  pinModeDecoder(configArguments[CFG_DSRMODE],&dsrActive,&dsrInactive,DEFAULT_DSR_HIGH,DEFAULT_DSR_LOW);
  if(configArguments[CFG_DCDPIN].length()>0)
  {
    pinDCD = atoi(configArguments[CFG_DCDPIN].c_str());
    if(pinSupport[pinDCD])
      pinMode(pinDCD,OUTPUT);
    dcdStatus=dcdInactive;
  }
  if(pinSupport[pinDCD])
    digitalWrite(pinDCD,dcdStatus);
  if(configArguments[CFG_CTSPIN].length()>0)
  {
    pinCTS = atoi(configArguments[CFG_CTSPIN].c_str());
    if(pinSupport[pinCTS])
      pinMode(pinCTS,INPUT);
  }
  if(configArguments[CFG_RTSPIN].length()>0)
  {
    pinRTS = atoi(configArguments[CFG_RTSPIN].c_str());
    if(pinSupport[pinRTS])
      pinMode(pinRTS,OUTPUT);
  }
  if(pinSupport[pinRTS])
    digitalWrite(pinRTS,rtsActive);
  if(configArguments[CFG_RIPIN].length()>0)
  {
    pinRI = atoi(configArguments[CFG_RIPIN].c_str());
    if(pinSupport[pinRI])
      pinMode(pinRI,OUTPUT);
  }
  if(pinSupport[pinRI])
    digitalWrite(pinRI,riInactive);
  if(configArguments[CFG_DTRPIN].length()>0)
  {
    pinDTR = atoi(configArguments[CFG_DTRPIN].c_str());
    if(pinSupport[pinDTR])
      pinMode(pinDTR,INPUT);
  }
  if(configArguments[CFG_DSRPIN].length()>0)
  {
    pinDSR = atoi(configArguments[CFG_DSRPIN].c_str());
    if(pinSupport[pinDSR])
      pinMode(pinDSR,OUTPUT);
  }
  if(pinSupport[pinDSR])
    digitalWrite(pinDSR,dsrActive);
  if(configArguments[CFG_S0_RINGS].length()>0)
    ringCounter = atoi(configArguments[CFG_S0_RINGS].c_str());
  if(configArguments[CFG_S41_STREAM].length()>0)
    autoStreamMode = atoi(configArguments[CFG_S41_STREAM].c_str());
  if(configArguments[CFG_S60_LISTEN].length()>0)
  {
    preserveListeners = atoi(configArguments[CFG_S60_LISTEN].c_str());
    if(preserveListeners)
      WiFiServerNode::RestoreWiFiServers();
  }
  if(configArguments[CFG_TIMEZONE].length()>0)
  {
    int tzCode = atoi(configArguments[CFG_TIMEZONE].c_str());
    if(tzCode > 500)
      zclock.setDisabled(true);
    else
    {
      zclock.setDisabled(false);
      zclock.setTimeZoneCode(tzCode);
    }
  }
  if((!zclock.isDisabled())&&(configArguments[CFG_TIMEFMT].length()>0))
    zclock.setFormat(configArguments[CFG_TIMEFMT]);
  if((!zclock.isDisabled())&&(configArguments[CFG_TIMEURL].length()>0))
    zclock.setNtpServerHost(configArguments[CFG_TIMEURL]);
  if(!zclock.isDisabled())
    zclock.forceUpdate();
}

void ZCommand::parseConfigOptions(String configArguments[])
{
  delay(500);
  File f = SPIFFS.open("/zconfig.txt", "r");
  String str=f.readString();
  f.close();
  if((str!=null)&&(str.length()>0))
  {
    debugPrintf("Read Config: %s\n",str.c_str());
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
  if(WiFi.status() == WL_CONNECTED)
    WiFi.disconnect();
  setConfigDefaults();
  String argv[CFG_LAST+1];
  parseConfigOptions(argv);
  if(argv[CFG_BAUDRATE].length()>0)
    baudRate=atoi(argv[CFG_BAUDRATE].c_str());
  if(baudRate <= 0)
    baudRate=DEFAULT_BAUD_RATE;
  if(argv[CFG_UART].length()>0)
    serialConfig = (SerialConfig)atoi(argv[CFG_UART].c_str());
  if(serialConfig <= 0)
    serialConfig = DEFAULT_SERIAL_CONFIG;
  changeBaudRate(baudRate);
  changeSerialConfig(serialConfig);
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
  if((vval == 1)||(vval==5))
  {
    bool showAll = (vval==5);
    serial.prints("AT");
    serial.prints("B");
    serial.printi(baudRate);
    serial.prints(doEcho?"E1":"E0");
    if(suppressResponses)
    {
      serial.prints("Q1");
      if(showAll)
      {
        serial.prints(numericResponses?"V0":"V1");
        serial.prints(longResponses?"X1":"X0");
      }
    }
    else
    {
      serial.prints("Q0");
      serial.prints(numericResponses?"V0":"V1");
      serial.prints(longResponses?"X1":"X0");
    }
    switch(serial.getFlowControlType())
    {
    case FCT_RTSCTS:
      serial.prints("F0");
      break;
    case FCT_NORMAL: 
      serial.prints("F1");
      break;
    case FCT_AUTOOFF:
      serial.prints("F2");
      break;
    case FCT_MANUAL:
      serial.prints("F3");
      break;
    case FCT_DISABLED:
      serial.prints("F4");
      break;
    }
    if(EOLN==CR)
      serial.prints("R0");
    else
    if(EOLN==CRLF)
      serial.prints("R1");
    else
    if(EOLN==LFCR)
      serial.prints("R2");
    else
    if(EOLN==LF)
      serial.prints("R3");
  
    if((delimiters != NULL)&&(delimiters[0]!=0))
    {
      for(int i=0;i<strlen(delimiters);i++)
        serial.printf("&D%d",delimiters[i]);
    }
    else
    if(showAll)
      serial.prints("&D");
    if((maskOuts != NULL)&&(maskOuts[0]!=0))
    {
      for(int i=0;i<strlen(maskOuts);i++)
        serial.printf("&m%d",maskOuts[i]);
    }
    else
    if(showAll)
      serial.prints("&M");

    serial.prints("S0=");
    serial.printi((int)ringCounter);
    if((EC != '+')||(showAll))
    {
      serial.prints("S2=");
      serial.printi((int)EC);
    }
    if((CR[0]!='\r')||(showAll))
    {
      serial.prints("S3=");
      serial.printi((int)CR[0]);
    }
    if((LF[0]!='\n')||(showAll))
    {
      serial.prints("S4=");
      serial.printi((int)LF[0]);
    }
    if((BS != 8)||(showAll))
    {
      serial.prints("S5=");
      serial.printi((int)BS);
    }
    serial.prints("S40=");
    serial.printi(packetSize);
    if(autoStreamMode ||(showAll))
      serial.prints(autoStreamMode ? "S41=1" : "S41=0");
    
    WiFiServerNode *serv = servs;
    while(serv != null)
    {
      serial.prints("A");
      serial.printi(serv->port);
      serv=serv->next;
    }
    if(tempBaud > 0)
    {
      serial.prints("S43=");
      serial.printi(tempBaud);
    }
    else
    if(showAll)
      serial.prints("S43=0");
    if((serialDelayMs > 0)||(showAll))
    {
      serial.prints("S44=");
      serial.printi(serialDelayMs);
    }
    if((binType > 0)||(showAll))
    {
      serial.prints("S45=");
      serial.printi(binType);
    }
    if((dcdActive != DEFAULT_DCD_HIGH)||(dcdInactive == DEFAULT_DCD_HIGH)||(showAll))
      serial.printf("S46=%d",pinModeCoder(dcdActive,dcdInactive,DEFAULT_DCD_HIGH));
    if((pinDCD != DEFAULT_PIN_DCD)||(showAll))
      serial.printf("S47=%d",pinDCD);
    if((ctsActive != DEFAULT_CTS_HIGH)||(ctsInactive == DEFAULT_CTS_HIGH)||(showAll))
      serial.printf("S48=%d",pinModeCoder(ctsActive,ctsInactive,DEFAULT_CTS_HIGH));
    if((pinCTS != getDefaultCtsPin())||(showAll))
      serial.printf("S49=%d",pinCTS);
    if((rtsActive != DEFAULT_RTS_HIGH)||(rtsInactive == DEFAULT_RTS_HIGH)||(showAll))
      serial.printf("S50=%d",pinModeCoder(rtsActive,rtsInactive,DEFAULT_RTS_HIGH));
    if((pinRTS != DEFAULT_PIN_RTS)||(showAll))
      serial.printf("S51=%d",pinRTS);
    if((riActive != DEFAULT_RI_HIGH)||(riInactive == DEFAULT_RI_HIGH)||(showAll))
      serial.printf("S52=%d",pinModeCoder(riActive,riInactive,DEFAULT_RI_HIGH));
    if((pinRI != DEFAULT_PIN_RI)||(showAll))
      serial.printf("S53=%d",pinRI);
    if((dtrActive != DEFAULT_DTR_HIGH)||(dtrInactive == DEFAULT_DTR_HIGH)||(showAll))
      serial.printf("S54=%d",pinModeCoder(dtrActive,dtrInactive,DEFAULT_DTR_HIGH));
    if((pinDTR != DEFAULT_PIN_DTR)||(showAll))
      serial.printf("S55=%d",pinDTR);
    if((dsrActive != DEFAULT_DSR_HIGH)||(dsrInactive == DEFAULT_DSR_HIGH)||(showAll))
      serial.printf("S56=%d",pinModeCoder(dsrActive,dsrInactive,DEFAULT_DSR_HIGH));
    if((pinDSR != DEFAULT_PIN_DSR)||(showAll))
      serial.printf("S57=%d",pinDSR);
    if(preserveListeners ||(showAll))
      serial.prints(preserveListeners ? "S60=1" : "S60=0");
    if((serial.isPetsciiMode())||(showAll))
      serial.prints(serial.isPetsciiMode() ? "&P1" : "&P0");
    if(logFileOpen || showAll)
      serial.prints(logFileOpen ? "&O1" : "&O0");
    serial.prints(EOLN);
  }
  else
  if(vval == 2)
  {
    serial.prints(WiFi.localIP().toString().c_str());
    serial.prints(EOLN);
  }
  else
  if(vval == 3)
  {
    serial.prints(wifiSSI.c_str());
    serial.prints(EOLN);
  }
  else
  if(vval == 4)
  {
    serial.prints(ZIMODEM_VERSION);
    serial.prints(EOLN);
  }
  else
  if(vval == 6)
  {
    serial.prints(WiFi.macAddress());
    serial.prints(EOLN);
  }
  else
  if(vval == 7)
  {
    serial.prints(zclock.getCurrentTimeFormatted());
    serial.prints(EOLN);
  }
  else
    return ZERROR;
  return ZOK;
}

ZResult ZCommand::doBaudCommand(int vval, uint8_t *vbuf, int vlen)
{
  if(vval<=0)
  {
    char *commaLoc=strchr((char *)vbuf,',');
    if(commaLoc == NULL)
      return ZERROR;
    char *conStr=commaLoc+1;
    if(strlen(conStr)!=3)
      return ZERROR;
    *commaLoc=0;
    int baudChk=atoi((char *)vbuf);
    if((baudChk<128)||(baudChk>115200))
      return ZERROR;
    if((conStr[0]<'5')||(conStr[0]>'8'))
      return ZERROR;
    if((conStr[2]!='1')&&(conStr[2]!='2'))
      return ZERROR;
    char *parPtr=strchr("oemn",lc(conStr[1]));
    if(parPtr==NULL)
      return ZERROR;
    char parity=*parPtr;
    uint32_t configChk=0;
    switch(conStr[0])
    {
    case '5':
      configChk = UART_NB_BIT_5;
      break;
    case '6':
      configChk = UART_NB_BIT_6;
      break;
    case '7':
      configChk = UART_NB_BIT_7;
      break;
    case '8':
      configChk = UART_NB_BIT_8;
      break;
    }
    if(conStr[2]=='1')
      configChk = configChk | UART_NB_STOP_BIT_1;
    else
    if(conStr[2]=='2')
      configChk = configChk | UART_NB_STOP_BIT_2;
    switch(parity)
    {
      case 'o':
        configChk = configChk | UART_PARITY_ODD;
        break;
      case 'e':
        configChk = configChk | UART_PARITY_EVEN;
        break;
      case 'm':
        configChk = configChk | UART_PARITY_MASK;
        break;
      case 'n':
        configChk = configChk | UART_PARITY_NONE;
        break;
    }
    serialConfig=(SerialConfig)configChk;
    baudRate=baudChk;
  }
  else
  {
    baudRate=vval;
  }
  hwSerialFlush();
  changeBaudRate(baudRate);
  changeSerialConfig(serialConfig);
  return ZOK;
}

ZResult ZCommand::doConnectCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers)
{
  if(vlen == 0)
  {
    logPrintln("ConnCheck: CURRENT");
    if(strlen(dmodifiers)>0)
      return ZERROR;
    if(current == null)
      return ZERROR;
    else
    {
      if(current->isConnected())
      {
        current->answer();
        serial.prints("CONNECTED ");
        serial.printf("%d %s:%d",current->id,current->host,current->port);
        serial.prints(EOLN);
      }
      else
      if(current->isAnswered())
      {
        serial.prints("NO CARRIER ");
        serial.printf("%d %s:%d",current->id,current->host,current->port);
        serial.prints(EOLN);
      }
      return ZIGNORE;
    }
  }
  else
  if((vval >= 0)&&(isNumber))
  {
    if(vval == 0)
      logPrintln("ConnList0:\r\n");
    else
      logPrintfln("ConnSwitchTo: %d",vval);
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
          c->answer();
          serial.prints("CONNECTED ");
          serial.printf("%d %s:%d",c->id,c->host,c->port);
          serial.prints(EOLN);
        }
        else
        if(c->isAnswered())
        {
          serial.prints("NO CARRIER ");
          serial.printf("%d %s:%d",c->id,c->host,c->port);
          serial.prints(EOLN);
        }
        c=c->next;
      }
      WiFiServerNode *s=servs;
      while(s!=null)
      {
        serial.prints("LISTENING ");
        serial.printf("%d *:%d",s->id,s->port);
        serial.prints(EOLN);
        s=s->next;
      }
    }
  }
  else
  {
    logPrintln("Connnect-Start:");
    char *colon=strstr((char *)vbuf,":");
    int port=23;
    if(colon != null)
    {
      (*colon)=0;
      port=atoi((char *)(++colon));
    }
    ConnSettings flags(dmodifiers);
    int flagsBitmap = flags.getBitmap(serial.getFlowControlType());
    logPrintfln("Connnecting: %s %d %d",(char *)vbuf,port,flagsBitmap);
    WiFiClientNode *c = new WiFiClientNode((char *)vbuf,port,flagsBitmap);
    if(!c->isConnected())
    {
      logPrintln("Connnect: FAIL");
      delete c;
      return ZERROR;
    }
    else
    {
      logPrintfln("Connnect: SUCCESS: %d",c->id);
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
  serial.prints(hbuf);
}

bool ZCommand::doWebGetStream(const char *hostIp, int port, const char *req, WiFiClient *c, uint32_t *responseSize)
{
  *responseSize = 0;
  if(WiFi.status() != WL_CONNECTED)
    return false;
  c->setNoDelay(DEFAULT_NO_DELAY);
  if(!c->connect(hostIp, port))
  {
    c->stop();
    return false;
  }
  c->printf("GET /%s HTTP/1.1\r\n",req);
  c->printf("User-Agent: C64Net Firmware\r\n");
  c->printf("Host: %s\r\n",hostIp);
  c->printf("Connection: close\r\n\r\n");
  
  String ln = "";
  uint32_t respLength = 0;
  int respCode = -1;
  while(c->connected())
  {
    yield();
    if(c->available()<=0)
      continue;
      
    char ch = (char)c->read();
    //logSocketIn(ch); // this is NOT socket input!!
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
  if((!c->connected())
  ||(respCode != 200)
  ||(respLength <= 0))
  {
    c->stop();
    return false;
  }
  return true;
}

bool ZCommand::doWebGet(const char *hostIp, int port, const char *filename, const char *req, const bool doSSL)
{
  uint32_t respLength=0;
  WiFiClient *c = createWiFiClient(doSSL);
  if(!doWebGetStream(hostIp, port, req, c, &respLength))
  {
    c->stop();
    delete c;
    return false;
  }
    
  File f = SPIFFS.open(filename, "w");
  while((respLength>0) && (c->connected()))
  {
    if(c->available()>=0)
    {
      uint8_t ch=c->read();
      //logSocketIn(ch); // this is ALSO not socket input!
      f.write(ch);
      respLength--;
    }
    else
      yield();
  }
  f.flush();
  f.close();
  c->stop();
  delete c;
  return (respLength == 0);
}

bool ZCommand::doWebGetBytes(const char *hostIp, int port, const char *req, const bool doSSL, uint8_t *buf, int *bufSize)
{
  WiFiClient *c = createWiFiClient(doSSL);
  uint32_t respLength=0;
  if(!doWebGetStream(hostIp, port, req, c, &respLength))
  {
    c->stop();
    delete c;
    return false;
  }
  if((!c->connected())
  ||(respLength > *bufSize))
  {
    c->stop();
    delete c;
    return false;
  }
  *bufSize = (int)respLength;
  int index=0;
  while((respLength>0) && (c->connected()))
  {
    if(c->available()>=0)
    {
      uint8_t ch=c->read();
      //logSocketIn(ch); // again, NOT SOCKET INPUT!
      buf[index++] = ch;
      respLength--;
    }
    else
      yield();
  }
  c->stop();
  delete c;
  return (respLength == 0);
}

ZResult ZCommand::doWebStream(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *filename, bool cache)
{
  bool doSSL = false;
  if(strstr((char *)vbuf,"http://")==(char *)vbuf)
    vbuf = vbuf + 7;
  else
  if(strstr((char *)vbuf,"https://")==(char *)vbuf)
  {
    vbuf = vbuf + 8;
    doSSL = true;
  }
  else
  if(strstr((char *)vbuf,"http:")==(char *)vbuf)
    vbuf = vbuf + 5;
  else
  if(strstr((char *)vbuf,"https:")==(char *)vbuf)
  {
    vbuf = vbuf + 6;
    doSSL = true;
  }

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
            if(!doWebGet(hostIp, port, filename, req, doSSL))
              return ZERROR;
          }
        }
        else
        if(!doWebGet(hostIp, port, filename, req, doSSL))
          return ZERROR;
        int chk8=0;
        if(!cache)
        {
          delay(100);
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
        {
          headerOut(0,len,chk8);
          serial.flush(); // stupid important because otherwise apps that go xoff miss the header info
        }
        bool flowControl=!cache;
        BinType streamType = cache?BTYPE_NORMAL:binType;
        int bct=0;
        while(len>0)
        {
          if((!flowControl) || serial.isSerialOut())
          {
            len--;
            int c=f.read();
            if(c<0)
              break;
            if(cache && serial.isPetsciiMode())
              c=ascToPetcii(c);
            switch(streamType)
            {
              case BTYPE_NORMAL:
                serial.write((uint8_t)c);
                break;
              case BTYPE_HEX:
              {
                const char *hbuf = TOHEX((uint8_t)c);
                serial.printb(hbuf[0]); // prevents petscii
                serial.printb(hbuf[1]);
                if((++bct)>=39)
                {
                  serial.prints(EOLN);
                  bct=0;
                }
                break;
              }
              case BTYPE_DEC:
                serial.printf("%d%s",c,EOLN.c_str());
                break;
            }
          }
          if(serial.isSerialOut())
          {
            serialOutDeque();
            yield();
          }
          if(serial.drainForXonXoff()==3)
          {
            serial.setXON(true);
            f.close();
            return ZOK;
          }
          while(serial.availableForWrite()<5)
          {
            if(serial.isSerialOut())
            {
              serialOutDeque();
              yield();
            }
            if(serial.drainForXonXoff()==3)
            {
              serial.setXON(true);
              f.close();
              return ZOK;
            }
            delay(1);
          }
          yield();
        }
        if(bct > 0)
          serial.prints(EOLN);
        f.close();
      }
    }
  }
  return ZIGNORE;
}

ZResult ZCommand::doUpdateFirmware(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  serial.prints("Local firmware version ");
  serial.prints(ZIMODEM_VERSION);
  serial.prints(".");
  serial.prints(EOLN);
  
  uint8_t buf[255];
  int bufSize = 254;
  if((!doWebGetBytes(UPDATE_URL, 80, VERSION_FILE, false, buf, &bufSize))||(bufSize<=0))
    return ZERROR;

  if((!isNumber)&&(vlen>2))
  {
    if(vbuf[0]=='=')
    {
      for(int i=1;i<vlen;i++)
        buf[i-1]=vbuf[i];
      buf[vlen-1]=0;
      bufSize=vlen-1;
      isNumber=true;
      vval=6502;
    }
  }
  
  while((bufSize>0)
  &&((buf[bufSize-1]==10)||(buf[bufSize-1]==13)))
    bufSize--;
  
  if((strlen(ZIMODEM_VERSION)==bufSize) && memcmp(buf,ZIMODEM_VERSION,strlen(ZIMODEM_VERSION))==0)
  {
    serial.prints("Your modem is up-to-date.");
    serial.prints(EOLN);
  }
  else
  {
    serial.prints("Latest available version is ");
    buf[bufSize]=0;
    serial.prints((char *)buf);
    serial.prints(".");
    serial.prints(EOLN);
  }
  if(vval != 6502)
    return ZOK;
  
  serial.printf("Updating to %s, wait for modem restart...",buf);
  serial.flush();
  uint32_t respLength=0;
  WiFiClient c;
  char firmwareName[100];
  sprintf(firmwareName,UPDATE_FILE,buf);

  if(!doWebGetStream(UPDATE_URL, 80, firmwareName, &c, &respLength))
  {
    serial.prints(EOLN);
    return ZERROR;
  }

  if(!Update.begin(respLength))
    return ZERROR;

  serial.prints(".");
  serial.flush();
  int writeBytes = Update.writeStream(c);
  if(writeBytes != respLength)
  {
    serial.prints(EOLN);
    return ZERROR;
  }
  serial.prints(".");
  serial.flush();
  if(!Update.end())
  {
    serial.prints(EOLN);
    return ZERROR;
  }
  serial.prints("Done");
  serial.prints(EOLN);
  serial.prints("Please wait for modem to restart...");
  ESP.restart();
  return ZOK;
}

ZResult ZCommand::doWiFiCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers)
{
  bool doPETSCII = (strchr(dmodifiers,'p')!=null) || (strchr(dmodifiers,'P')!=null);
  if((vlen==0)||(vval>0))
  {
    int n = WiFi.scanNetworks();
    if((vval > 0)&&(vval < n))
      n=vval;
    for (int i = 0; i < n; ++i)
    {
      if((doPETSCII)&&(!serial.isPetsciiMode()))
      {
        String ssidstr=WiFi.SSID(i);
        char *c = (char *)ssidstr.c_str();
        for(;*c!=0;c++)
          serial.printc(ascToPetcii(*c));
      }
      else
        serial.prints(WiFi.SSID(i).c_str());
      serial.prints(" (");
      serial.printi(WiFi.RSSI(i));
      serial.prints(")");
      serial.prints((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      serial.prints(EOLN.c_str());
      serial.flush();
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
      bool connSuccess=false;
      if((doPETSCII)&&(!serial.isPetsciiMode()))
      {
        char *ssiP =(char *)malloc(strlen(ssi)+1);
        char *pwP = (char *)malloc(strlen(pw)+1);
        strcpy(ssiP,ssi);
        strcpy(pwP,pw);
        for(char *c=ssiP;*c!=0;c++)
          *c = ascToPetcii(*c);
        for(char *c=pwP;*c!=0;c++)
          *c = ascToPetcii(*c);
        connSuccess = connectWifi(ssiP,pwP);
        free(ssiP);
        free(pwP);
      }
      else
        connSuccess = connectWifi(ssi,pw);

      if(!connSuccess)
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

ZResult ZCommand::doTransmitCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers, int *crc8)
{
  bool doPETSCII = (strchr(dmodifiers,'p')!=null) || (strchr(dmodifiers,'P')!=null);
  int crcChk = *crc8;
  *crc8=-1;
  int rcvdCrc8=-1;
  if((vlen==0)||(current==null)||(!current->isConnected()))
    return ZERROR;
  else
  if(isNumber && (vval>0))
  {
    uint8_t buf[vval];
    int recvd = HWSerial.readBytes(buf,vval);
    if(logFileOpen)
    {
      for(int i=0;i<recvd;i++)
        logSerialIn(buf[i]);
    }
    if(recvd != vval)
      return ZERROR;
    rcvdCrc8=CRC8(buf,recvd);
    if((crcChk != -1)&&(rcvdCrc8!=crcChk))
      return ZERROR;
    if(current->isPETSCII() || doPETSCII)
    {
      for(int i=0;i<recvd;i++)
        buf[i]=petToAsc(buf[i]);
    }
    current->write(buf,recvd);
    if(logFileOpen)
    {
      for(int i=0;i<recvd;i++)
        logSocketOut(buf[i]);
    }
  }
  else
  {
    uint8_t buf[vlen];
    memcpy(buf,vbuf,vlen);
    rcvdCrc8=CRC8(buf,vlen);
    if((crcChk != -1)&&(rcvdCrc8!=crcChk))
      return ZERROR;
    if(current->isPETSCII() || doPETSCII)
    {
      for(int i=0;i<vlen;i++)
        buf[i] = petToAsc(buf[i]);
    }
    current->write(buf,vlen);
    current->write(13); // special case
    current->write(10); // special case
    if(logFileOpen)
    {
      for(int i=0;i<vlen;i++)
        logSocketOut(buf[i]);
      logSocketOut(13);
      logSocketOut(10);
    }
  }
  if(strchr(dmodifiers,'+')==null)
    return ZOK;
  else
  {
    HWSerial.printf("%d%s",rcvdCrc8,EOLN.c_str());
    return ZIGNORE_SPECIAL;
  }
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
    /*
    if(vval == 5517545) // slip no login
    {
      slipMode.switchTo();
    }
    */
    
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
    ConnSettings flags(dmodifiers);
    int flagsBitmap = flags.getBitmap(serial.getFlowControlType());
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
          serial.prints(nbuf);
          for(int i=0;i<10-strlen(nbuf);i++)
            serial.prints(" ");
          serial.prints(" ");
          serial.prints(phb->modifiers);
          for(int i=1;i<5-strlen(phb->modifiers);i++)
            serial.prints(" ");
          serial.prints(" ");
          serial.prints(phb->address);
          serial.prints(EOLN.c_str());
          serial.flush();
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
  PhoneBookEntry *found=PhoneBookEntry::findPhonebookEntry(number);
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
  if(!PhoneBookEntry::checkPhonebookEntry(colon))
      return ZERROR;
  if(found != null)
    delete found;
  PhoneBookEntry *newEntry = new PhoneBookEntry(number,rest,dmodifiers);
  PhoneBookEntry::savePhonebook();
  return ZOK;
}

ZResult ZCommand::doAnswerCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers)
{
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
            c->answer();
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
    ConnSettings flags(dmodifiers);
    int flagsBitmap = flags.getBitmap(serial.getFlowControlType());
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
    vval = lastPacketId;
  if(vval <= 0)
    cnode = current;
  else
  {
    WiFiClientNode *c=conns;
    while(c != null)
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
  while(HWSerial.available()>0)
  {
    uint8_t c=HWSerial.read();
    logSerialIn(c);
    if((c==CR[0])||(c==LF[0]))
    {
      if(doEcho)
      {
        serial.prints(EOLN);
        if(serial.isSerialOut())
          serialOutDeque();
      }
      crReceived=true;
      break;
    }
    
    if(c>0)
    {
      if(c!=EC)
        lastNonPlusTimeMs=millis();
      
      if((c==19)&&(serial.getFlowControlType() == FCT_NORMAL))
      {
        serial.setXON(false);
      }
      else
      if((c==19)
      &&((serial.getFlowControlType() == FCT_AUTOOFF)
         ||(serial.getFlowControlType() == FCT_MANUAL)))
      {
        packetXOn = false;
      }
      else
      if((c==17)
      &&(serial.getFlowControlType() == FCT_NORMAL))
      {
        serial.setXON(true);
      }
      else
      if((c==17)
      &&((serial.getFlowControlType() == FCT_AUTOOFF)
         ||(serial.getFlowControlType() == FCT_MANUAL)))
      {
        packetXOn = true;
        if(serial.getFlowControlType() == FCT_MANUAL)
        {
          sendNextPacket();
        }
      }
      else
      {
        if(doEcho)
        {
          serial.write(c);
          if(serial.isSerialOut())
            serialOutDeque();
        }
        if((c==BS)||((BS==8)&&((c==20)||(c==127))))
        {
          if(eon>0)
            nbuf[--eon]=0;
          continue;
        }
        nbuf[eon++]=c;
        if((eon>=MAX_COMMAND_SIZE)
        ||((eon==2)&&(nbuf[1]=='/')&&lc(nbuf[0])=='a'))
        {
          eon--;
          crReceived=true;
        }
      }
    }
  }
  return crReceived;
}

String ZCommand::getNextSerialCommand()
{
  int len=eon;
  String currentCommand = (char *)nbuf;
  currentCommand.trim();
  memset(nbuf,0,MAX_COMMAND_SIZE);
  if(serial.isPetsciiMode())
  {
    for(int i=0;i<len;i++)
      currentCommand[i]=petToAsc(currentCommand[i]);
  }
  eon=0;

  if(logFileOpen)
    logPrintfln("Command: %s",currentCommand.c_str());
  return currentCommand;
}

ZResult ZCommand::doTimeZoneSetupCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  if((strcmp((char *)vbuf,"disabled")==0)
  ||(strcmp((char *)vbuf,"DISABLED")==0))
  {
    zclock.setDisabled(true);
    return ZOK;
  }
  zclock.setDisabled(false);
  char *c1=strchr((char *)vbuf,',');
  if(c1 == 0)
    return zclock.setTimeZone((char *)vbuf) ? ZOK : ZERROR;
  else
  {
    *c1=0;
    c1++;
    if((strlen((char *)vbuf)>0)&&(!zclock.setTimeZone((char *)vbuf)))
      return ZERROR;
    if(strlen(c1)==0)
      return ZOK;
    char *c2=strchr(c1,',');
    if(c2 == 0)
    {
      zclock.setFormat(c1);
      return ZOK;
    }
    else
    {
      *c2=0;
      c2++;
      if(strlen(c1)>0)
        zclock.setFormat(c1);
      if(strlen(c2)>0)
        zclock.setNtpServerHost(c2);
    }
  }
  return ZOK;
}

ZResult ZCommand::doSerialCommand()
{
  int len=eon;
  String sbuf = getNextSerialCommand();

  if((sbuf.length()==2)
  &&(lc(sbuf[0])=='a')
  &&(sbuf[1]=='/'))
  {
    sbuf = previousCommand;
    len=previousCommand.length();
  }
  if(logFileOpen)
    logPrintfln("Command: %s",sbuf.c_str());
  
  int crc8=-1;  
  ZResult result=ZOK;
  int index=0;
  while((index<len-1)
  &&((lc(sbuf[index])!='a')||(lc(sbuf[index+1])!='t')))
  {
    index++;
  }
  
  String saveCommand = (index < len) ? sbuf : previousCommand;

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
      if(index<len)
      {
        if(lastCmd=='+')
        {
          vlen += len-index;
          index=len;
        }
        else
        if((lastCmd=='&')||(lastCmd=='%'))
        {
          index++;//protect our one and only letter.
          secCmd = sbuf[vstart];
          vstart++;
        }
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
        if(strchr("dcpatw", lastCmd) != null)
        {
          const char *DMODIFIERS=",exprts+";
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
        &&(sbuf[index]!='&')
        &&(sbuf[index]!='%'))
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
        memcpy(vbuf,sbuf.c_str()+vstart,vlen);
        if((vlen > 0)&&(isNumber))
        {
          String finalNum="";
          for(uint8_t *v=vbuf;v<(vbuf+vlen);v++)
            if((*v>='0')&&(*v<='9'))
              finalNum += (char)*v;
          vval=atol(finalNum.c_str());
        }
      }
      
      if(vlen > 0)
        logPrintfln("Proc: %c %lu '%s'",lastCmd,vval,vbuf);
      else
        logPrintfln("Proc: %c %lu ''",lastCmd,vval);

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
            packetXOn = true;
            serial.setXON(true);
            serial.setFlowControlType((FlowControlType)vval);
            if(serial.getFlowControlType() == FCT_MANUAL)
              packetXOn = false;
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
        result = doTransmitCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str(),&crc8);
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
          result = isNumber ? doDialStreamCommand(vval,vbuf,vlen,isNumber,"") : ZERROR;
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
        result = doWiFiCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str());
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
               serialDelayMs=sval;
               break;
             case 45:
               if((sval>=0)&&(sval<BTYPE_INVALID))
                 binType=(BinType)sval;
               else
                 result=ZERROR;
               break;
             case 46:
             {
               bool wasActive=(dcdStatus==dcdActive);
               pinModeDecoder(sval,&dcdActive,&dcdInactive,DEFAULT_DCD_HIGH,DEFAULT_DCD_LOW);
               dcdStatus = wasActive?dcdActive:dcdInactive;
               if(pinSupport[pinDCD])
                 digitalWrite(pinDCD,dcdStatus);
               break;
             }
             case 47:
               if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
               {
                 pinDCD=sval;
                 pinMode(pinDCD,OUTPUT);
                 digitalWrite(pinDCD,dcdStatus);
               }
               else
                 result=ZERROR;
               break;
             case 48:
               pinModeDecoder(sval,&ctsActive,&ctsInactive,DEFAULT_CTS_HIGH,DEFAULT_CTS_LOW);
               break;
             case 49:
               if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
               {
                 pinCTS=sval;
                 pinMode(pinCTS,INPUT);
               }
               else
                 result=ZERROR;
               break;
             case 50:
               pinModeDecoder(sval,&rtsActive,&rtsInactive,DEFAULT_RTS_HIGH,DEFAULT_RTS_LOW);
               if(pinSupport[pinRTS])
                 digitalWrite(pinRTS,rtsActive);
               break;
             case 51:
               if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
               {
                 pinRTS=sval;
                 pinMode(pinRTS,OUTPUT);
                 digitalWrite(pinRTS,rtsActive);
               }
               else
                 result=ZERROR;
               break;
             case 52:
               pinModeDecoder(sval,&riActive,&riInactive,DEFAULT_RI_HIGH,DEFAULT_RI_LOW);
               if(pinSupport[pinRI])
                 digitalWrite(pinRI,riInactive);
               break;
             case 53:
               if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
               {
                 pinRI=sval;
                 pinMode(pinRI,OUTPUT);
                 digitalWrite(pinRTS,riInactive);
               }
               else
                 result=ZERROR;
               break;
             case 54:
               pinModeDecoder(sval,&dtrActive,&dtrInactive,DEFAULT_DTR_HIGH,DEFAULT_DTR_LOW);
               break;
             case 55:
               if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
               {
                 pinDTR=sval;
                 pinMode(pinDTR,INPUT);
               }
               else
                 result=ZERROR;
               break;
             case 56:
               pinModeDecoder(sval,&dsrActive,&dsrInactive,DEFAULT_DSR_HIGH,DEFAULT_DSR_LOW);
               if(pinSupport[pinDSR])
                 digitalWrite(pinDSR,dsrActive);
               break;
             case 57:
               if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
               {
                 pinDSR=sval;
                 pinMode(pinDSR,OUTPUT);
                 digitalWrite(pinDSR,dsrActive);
               }
               else
                 result=ZERROR;
               break;
             case 60:
               if(sval >= 0)
               {
                 preserveListeners=(sval != 0);
                 if(preserveListeners)
                   WiFiServerNode::SaveWiFiServers();
                 else
                   SPIFFS.remove("/zlisteners.txt");
               }
               else
                 result=ZERROR;
                 break;
             default:
                break;
              }
            }
          }
        }
        break;
      case '+':
        for(int i=0;vbuf[i]!=0;i++)
          vbuf[i]=lc(vbuf[i]);
        if(strcmp((const char *)vbuf,"config")==0)
        {
            configMode.switchTo();
            result = ZOK;
        }
#ifdef INCLUDE_SD_SHELL
        else
        if((strcmp((const char *)vbuf,"shell")==0)&&(browseEnabled))
        {
            browseMode.switchTo();
            result = ZOK;
        }
#endif
        else
          result=ZERROR; //todo: branch based on vbuf contents
        break;
      case '%':
        result=ZERROR;
        break;
      case '&':
        switch(lc(secCmd))
        {
        case 'k':
          if((!isNumber)||(vval>=FCT_INVALID))
            result=ZERROR;
          else
          {
              packetXOn = true;
              serial.setXON(true);
              switch(vval)
              {
                case 0: case 1: case 2:
                  serial.setFlowControlType(FCT_DISABLED);
                  break;
                case 3: case 6:
                  serial.setFlowControlType(FCT_RTSCTS);
                  break;
                case 4: case 5:
                  serial.setFlowControlType(FCT_NORMAL);
                  break;
                default:
                  result=ZERROR;
                  break;
              }
          }
          break;
        case 'l':
          loadConfig();
          break;
        case 'w':
          reSaveConfig();
          break;
        case 'f':
          if(vval == 86)
          {
            loadConfig();
            result = SPIFFS.format() ? ZOK : ZERROR;
            reSaveConfig();
          }
          else
          {
            SPIFFS.remove("/zconfig.txt");
            SPIFFS.remove("/zphonebook.txt");
            SPIFFS.remove("/zlisteners.txt");
            PhoneBookEntry::clearPhonebook();
            if(WiFi.status() == WL_CONNECTED)
              WiFi.disconnect();
            wifiSSI="";
            wifiConnected=false;
            delay(500);
            zclock.reset();
            result=doResetCommand();
            showInitMessage();
          }
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
              logFile.flush();
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
                if(serial.availableForWrite() > 1)
                {
                  serial.printc((char)buf[i++]);
                }
                else
                {
                  if(serial.isSerialOut())
                  {
                    serialOutDeque();
                    hwSerialFlush();
                  }
                  delay(1);
                  yield();
                }
                if(serial.drainForXonXoff()==3)
                {
                  serial.setXON(true);
                  while(logFile.available()>0)
                    logFile.read();
                  break;
                }
                yield();
              }
              numBytes = logFile.available();
            }
            logFile.close();
            serial.prints(EOLN);
            result=ZOK;
          }
          else
          if(logFileOpen)
            result=ZERROR;
          else
          if(vval==86)
          {
            result = SPIFFS.exists("/logfile.txt") ? ZOK : ZERROR;
            if(result)
              SPIFFS.remove("/logfile.txt");
          }
          else
          if(vval==87)
              SPIFFS.remove("/logfile.txt");
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
            int oldDelay = serialDelayMs;
            serialDelayMs = vval;
            uint8_t buf[100];
            sprintf((char *)buf,"www.zimmers.net:80/otherprojs%s",filename);
            serial.prints("Control-C to Abort.");
            serial.prints(EOLN);
            result = doWebStream(0,buf,strlen((char *)buf),false,filename,true);
            serialDelayMs = oldDelay;
            if((result == ZERROR)
            &&(WiFi.status() != WL_CONNECTED))
            {
              serial.prints("Not Connected.");
              serial.prints(EOLN);
              serial.prints("Use ATW to list access points.");
              serial.prints(EOLN);
              serial.prints("ATW\"[SSI],[PASSWORD]\" to connect.");
              serial.prints(EOLN);
            }
          }
          break;
        }
        case 'g':
          result = doWebStream(vval,vbuf,vlen,isNumber,"/temp.web",false);
          break;
        case 'p':
          serial.setPetsciiMode(vval > 0);
          break;
        case 'n':
             if(isNumber && (vval >=0) && (vval <= MAX_PIN_NO) && pinSupport[vval])
             {
               int pinNum = vval;
               int r = digitalRead(pinNum);
               //if(pinNum == pinCTS)
               //  serial.printf("Pin %d READ=%s %s.%s",pinNum,r==HIGH?"HIGH":"LOW",enableRtsCts?"ACTIVE":"INACTIVE",EOLN.c_str());
               //else
                 serial.printf("Pin %d READ=%s.%s",pinNum,r==HIGH?"HIGH":"LOW",EOLN.c_str());
             }
             else
             if(!isNumber)
             {
                char *eq = strchr((char *)vbuf,'=');
                if(eq == 0)
                  result = ZERROR;
                else
                {
                  *eq = 0;
                  int pinNum = atoi((char *)vbuf);
                  int sval = atoi(eq+1);
                  if((pinNum < 0) || (pinNum >= MAX_PIN_NO) || (!pinSupport[pinNum]))
                    result = ZERROR;
                  else
                  {
                    digitalWrite(pinNum,sval);
                    serial.printf("Pin %d FORCED %s.%s",pinNum,(sval==LOW)?"LOW":(sval==HIGH)?"HIGH":"UNK",EOLN.c_str());
                  }
                }
             }
             else
              result = ZERROR;
             break;
        case 't':
          if(vlen == 0)
          {
            serial.prints(zclock.getCurrentTimeFormatted());
            serial.prints(EOLN);
            result = ZIGNORE_SPECIAL;
          }
          else
            result = doTimeZoneSetupCommand(vval, vbuf, vlen, isNumber);
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
      previousCommand = saveCommand;
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
      if(crc8 >= 0)
        result=ZERROR; // setting S42 without a T command is now Bad.
      switch(result)
      {
      case ZOK:
        if(index >= len)
        {
          logPrintln("Response: OK");
          if(numericResponses)
            serial.prints("0");
          else
            serial.prints("OK");
          serial.prints(EOLN);
        }
        break;
      case ZERROR:
        logPrintln("Response: ERROR");
        if(numericResponses)
          serial.prints("4");
        else
          serial.prints("ERROR");
        serial.prints(EOLN);
        // on error, cut and run
        return ZERROR;
      case ZCONNECT:
        logPrintln("Response: Connected!");
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
  serial.prints(commandMode.EOLN);
#ifdef ZIMODEM_ESP32
  int totalSPIFFSSize = SPIFFS.totalBytes();
  serial.prints("GuruModem WiFi Firmware v");
#else
  FSInfo info;
  SPIFFS.info(info);
  int totalSPIFFSSize = info.totalBytes;
  serial.prints("C64Net WiFi Firmware v");
#endif
  HWSerial.setTimeout(60000);
  serial.prints(ZIMODEM_VERSION);
  serial.prints(" (");
  serial.prints(compile_date);
  serial.prints(")");
  serial.prints(commandMode.EOLN);
  serial.prints("CoCoWiFi Version.");
  serial.prints(commandMode.EOLN);
  char s[100];
#ifdef ZIMODEM_ESP32
  sprintf(s,"sdk=%s chipid=%d cpu@%d",ESP.getSdkVersion(),ESP.getChipRevision(),ESP.getCpuFreqMHz());
#else
  sprintf(s,"sdk=%s chipid=%d cpu@%d",ESP.getSdkVersion(),ESP.getFlashChipId(),ESP.getCpuFreqMHz());
#endif
  serial.prints(s);
  serial.prints(commandMode.EOLN);
#ifdef ZIMODEM_ESP32
  sprintf(s,"totsize=%dk hsize=%dk fsize=%dk speed=%dm",(ESP.getFlashChipSize()/1024),(ESP.getFreeHeap()/1024),totalSPIFFSSize/1024,(ESP.getFlashChipSpeed()/1000000));
#else
  sprintf(s,"totsize=%dk ssize=%dk fsize=%dk speed=%dm",(ESP.getFlashChipRealSize()/1024),(ESP.getSketchSize()/1024),totalSPIFFSSize/1024,(ESP.getFlashChipSpeed()/1000000));
#endif
  
  serial.prints(s);
  serial.prints(commandMode.EOLN);
  if(wifiSSI.length()>0)
  {
    if(wifiConnected)
      serial.prints(("CONNECTED TO " + wifiSSI + " (" + WiFi.localIP().toString().c_str() + ")").c_str());
    else
      serial.prints(("ERROR ON " + wifiSSI).c_str());
  }
  else
    serial.prints("INITIALIZED");
  serial.prints(commandMode.EOLN);
  serial.prints("READY.");
  serial.prints(commandMode.EOLN);
  serial.flush();
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
    int i=0;
    while(i < bufLen)
    {
      uint8_t c=buf[i++];
      switch(binType)
      {
        case BTYPE_NORMAL:
          serial.write(c);
          break;
        case BTYPE_HEX:
        {
          const char *hbuf = TOHEX(c);
          serial.printb(hbuf[0]); // prevents petscii
          serial.printb(hbuf[1]);
          if((++bct)>=39)
          {
            serial.prints(EOLN);
            bct=0;
          }
          break;
        }
        case BTYPE_DEC:
          serial.printf("%d%s",c,EOLN.c_str());
          break;
      }
      while(serial.availableForWrite()<5)
      {
        if(serial.isSerialOut())
        {
          serialOutDeque();
          hwSerialFlush();
        }
        serial.drainForXonXoff();
        delay(1);
        yield();
      }
      yield();
    }
    if(bct > 0)
      serial.prints(EOLN);
    free(buf);
  }
}

bool ZCommand::clearPlusProgress()
{
  if(currentExpiresTimeMs > 0)
    currentExpiresTimeMs = 0;
  if((strcmp((char *)nbuf,ECS)==0)&&((millis()-lastNonPlusTimeMs)>1000))
    currentExpiresTimeMs = millis() + 1000;
}

bool ZCommand::checkPlusEscape()
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
          {
            serial.prints("3");
            serial.prints(EOLN);
          }
          else
          if(current->isAnswered())
          {
            serial.prints("NO CARRIER ");
            serial.printf("%d %s:%d",current->id,current->host,current->port);
            serial.prints(EOLN);
          }
        }
        delete current;
        current = conns;
        nextConn = conns;
      }
      memset(nbuf,0,MAX_COMMAND_SIZE);
      eon=0;
      return true;
    }
  }
  return false;
}

void ZCommand::serialIncoming()
{
  bool crReceived=readSerialStream();
  clearPlusProgress(); // every serial incoming, without a plus, breaks progress
  if((!crReceived)||(eon==0))
    return;
  //delay(200); // give a pause after receiving command before responding
  // the delay doesn't affect xon/xoff because its the periodic transmitter that manages that.
  doSerialCommand();
}

void ZCommand::sendNextPacket()
{
  if(serial.availableForWrite()<packetSize)
    return;

  WiFiClientNode *firstConn = nextConn;
  if((nextConn == null)||(nextConn->next == null))
  {
    firstConn = null;
    nextConn = conns;
  }
  else
    nextConn = nextConn->next;
  while(serial.isSerialOut() && (nextConn != null) && (packetXOn))
  {
    if((nextConn->isConnected())
    && (nextConn->available()>0))
    {
      int availableBytes = nextConn->available();
      int maxBytes=packetSize;
      if(availableBytes<maxBytes)
        maxBytes=availableBytes;
      //if(maxBytes > SerialX.availableForWrite()-15) // how much we read should depend on how much we can IMMEDIATELY write
      //maxBytes = SerialX.availableForWrite()-15;    // .. this is because resendLastPacket ensures everything goes out
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
            uint8_t c=nextConn->read();
            logSocketIn(c);
            lastBuf[lastLen++] = c;
            bytesRemain--;
          }
          nextConn->lastPacketLen = lastLen;
          if((lastLen >= packetSize)
          ||((lastLen>0)
            &&((strchr(nextConn->delimiters,lastBuf[lastLen-1]) != null)
              ||(strchr(delimiters,lastBuf[lastLen-1]) != null))))
            maxBytes = lastLen;
          else
          {
            if(serial.getFlowControlType() == FCT_MANUAL)
            {
              headerOut(0,0,0);
              packetXOn = false;
            }
            else
            if(serial.getFlowControlType() == FCT_AUTOOFF)
              packetXOn = false;
            return;
          }
        }
        else
        {
          maxBytes = nextConn->read(nextConn->lastPacketBuf,maxBytes);
          logSocketIn(nextConn->lastPacketBuf,maxBytes);
        }
        nextConn->lastPacketLen=maxBytes;
        lastPacketId=nextConn->id;
        reSendLastPacket(nextConn);
        if(serial.getFlowControlType() == FCT_AUTOOFF)
        {
          packetXOn = false;
        }
        else
        if(serial.getFlowControlType() == FCT_MANUAL)
        {
          packetXOn = false;
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
          {
            serial.prints("3");
            serial.prints(EOLN);
          }
          else
          if(nextConn->isAnswered())
          {
            serial.prints("NO CARRIER ");
            serial.printi(nextConn->id);
            serial.prints(EOLN);
          }
          if(serial.getFlowControlType() == FCT_MANUAL)
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
  if((serial.getFlowControlType() == FCT_MANUAL) && (packetXOn))
  {
    packetXOn = false;
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
      serial.prints("1");
    else
    if(baudRate < 1200)
      serial.prints("1");
    else
    if(baudRate < 2400)
      serial.prints("5");
    else
    if(baudRate < 4800)
      serial.prints("10");
    else
    if(baudRate < 7200)
      serial.prints("11");
    else
    if(baudRate < 9600)
      serial.prints("24");
    else
    if(baudRate < 12000)
      serial.prints("12");
    else
    if(baudRate < 14400)
      serial.prints("25");
    else
    if(baudRate < 19200)
      serial.prints("13");
    else
      serial.prints("28");
  }
  else
  {
    serial.prints("CONNECT");
    if(longResponses)
    {
      serial.prints(" ");
      serial.printi(id);
    }
  }
  serial.prints(EOLN);
}

void ZCommand::acceptNewConnection()
{
  WiFiServerNode *serv = servs;
  while(serv != null)
  {
    if(serv->hasClient())
    {
      WiFiClient newClient = serv->server->available();
      if(newClient.connected())
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
          int futureRings = (ringCounter > 0)?(ringCounter-1):5;
          WiFiClientNode *newClientNode = new WiFiClientNode(newClient, serv->flagsBitmap, futureRings * 2);
          setCharArray(&(newClientNode->delimiters),serv->delimiters);
          setCharArray(&(newClientNode->maskOuts),serv->maskOuts);
          if(pinSupport[pinRI])
            digitalWrite(pinRI,riActive);
          serial.prints(numericResponses?"2":"RING");
          serial.prints(EOLN);
          lastServerClientId = newClientNode->id;
          if(newClientNode->isAnswered())
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
  // handle rings properly
  WiFiClientNode *conn = conns;
  long now=millis();
  while(conn != null)
  {
    WiFiClientNode *nextConn = conn->next;
    if((!conn->isAnswered())&&(conn->isConnected()))
    {
      if(now > conn->nextRingTime(0))
      {
        conn->nextRingTime(3000);
        int rings=conn->ringsRemaining(-1);
        if(rings <= 0)
        {
          if(pinSupport[pinRI])
            digitalWrite(pinRI,riInactive);
          if(ringCounter > 0)
          {
            serial.prints(numericResponses?"2":"RING");
            serial.prints(EOLN);
            conn->answer();
            if(autoStreamMode)
            {
              sendConnectionNotice(baudRate);
              doAnswerCommand(0, (uint8_t *)"", 0, false, "");
              break;
            }
            else
              sendConnectionNotice(conn->id);
          }
          else
            delete conn;
        }
        else
        if((rings % 2) == 0)
        {
          if(pinSupport[pinRI])
            digitalWrite(pinRI,riActive);
          serial.prints(numericResponses?"2":"RING");
          serial.prints(EOLN);
        }
        else
        if(pinSupport[pinRI])
          digitalWrite(pinRI,riInactive);
      }
    }
    conn = nextConn;
  }
  if(checkOpenConnections()==0)
  {
    if(pinSupport[pinRI])
      digitalWrite(pinRI,riInactive);
  }
}

void ZCommand::loop()
{
  checkPlusEscape();
  acceptNewConnection();
  if(serial.isSerialOut())
  {
    sendNextPacket();
    serialOutDeque();
  }
  checkBaudChange();
}
