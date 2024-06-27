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
extern "C" void esp_schedule();
extern "C" void esp_yield();

#ifdef INCLUDE_PING
#  include "proto_ping.h"
#endif

ZCommand::ZCommand()
{
  strcpy(CRLF,"\r\n");
  strcpy(LFCR,"\n\r");
  strcpy(LF,"\n");
  strcpy(CR,"\r");
  strcpy(ECS,"+++");
  freeCharArray(&tempMaskOuts);
  freeCharArray(&tempDelimiters);
  freeCharArray(&tempStateMachine);
  setCharArray(&delimiters,"");
  setCharArray(&maskOuts,"");
  setCharArray(&stateMachine,"");
  machineState = stateMachine;
}

static void parseHostInfo(uint8_t *vbuf, char **hostIp, int *port, char **username, char **password)
{
  char *at=strstr((char *)vbuf,"@");
  *hostIp = (char *)vbuf;
  if(at != null)
  {
    *at = 0;
    *hostIp = at+1;
    char *ucol = strstr((char *)vbuf,":");
    *username = (char *)vbuf;
    if(ucol != null)
    {
      *ucol = 0;
      *password = ucol + 1;
    }
  }
  char *colon=strstr(*hostIp,":");
  if(colon != null)
  {
    (*colon)=0;
    *port=atoi((char *)(++colon));
  }
}

static bool validateHostInfo(uint8_t *vbuf)
{
  String cmd = (char *)vbuf;
  int atDex=cmd.indexOf('@');
  int colonDex=cmd.indexOf(':');
  int lastColonDex=cmd.lastIndexOf(':');
  bool fail = cmd.indexOf(',') >= 0;
  if(atDex >=0)
  {
    colonDex=cmd.indexOf(':',atDex+1);
    fail = cmd.indexOf(',') >= atDex;
  }
  fail = fail || (colonDex <= 0) || (colonDex == cmd.length()-1);
  fail = fail || (colonDex != cmd.lastIndexOf(':'));
  if(!fail)
  {
    for(int i=colonDex+1;i<cmd.length();i++)
    {
      if(strchr("0123456789",cmd[i]) == 0)
      {
        fail=true;
        break;
      }
    }
  }
  return !fail;
}

void ZCommand::reset()
{
  doResetCommand(true);
  if(altOpMode == OPMODE_NONE)
    showInitMessage();
}

byte ZCommand::CRC8(const byte *data, byte len) 
{
  byte crc = 0x00;
  //logPrint("CRC8: ");
  int c=0;
  while (len--) 
  {
    byte extract = *data++;
    /* save for later debugging
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
    */
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
  altOpMode = OPMODE_NONE;
  doEcho=true;
  busyMode=false;
  autoStreamMode=false; // should prob have been true all along, ats0 takes care of this.
  telnetSupport=true;
  preserveListeners=false;
  ringCounter=1;
  streamMode.setHangupType(HANGUP_NONE);
  serial.setFlowControlType(DEFAULT_FCT);
  serial.setXON(true);
  packetXOn = true;
# ifdef INCLUDE_CMDRX16
    packetXOn = false;
# endif
  serial.setPetsciiMode(false);
  binType=BTYPE_NORMAL;
  serialDelayMs=0;
  dcdActive=DEFAULT_DCD_ACTIVE;
  dcdInactive=DEFAULT_DCD_INACTIVE;
# ifdef HARD_DCD_HIGH
  dcdInactive=DEFAULT_DCD_ACTIVE;
# elif defined(HARD_DCD_LOW)
  dcdActive=DEFAULT_DCD_INACTIVE;
# endif 
  ctsActive=DEFAULT_CTS_ACTIVE;
  ctsInactive=DEFAULT_CTS_INACTIVE;
  rtsActive=DEFAULT_RTS_ACTIVE;
  rtsInactive=DEFAULT_RTS_INACTIVE;
  riActive = DEFAULT_RTS_ACTIVE;
  riInactive = DEFAULT_RTS_INACTIVE;
  dtrActive = DEFAULT_RTS_ACTIVE;
  dtrInactive = DEFAULT_RTS_INACTIVE;
  dsrActive = DEFAULT_RTS_ACTIVE;
  dsrInactive = DEFAULT_RTS_INACTIVE;
  othActive = DEFAULT_OTH_ACTIVE;
  othInactive = DEFAULT_OTH_INACTIVE;
  pinDCD = DEFAULT_PIN_DCD;
  pinCTS = DEFAULT_PIN_CTS;
  pinRTS = DEFAULT_PIN_RTS;
  pinDTR = DEFAULT_PIN_DTR;
  pinOTH = DEFAULT_PIN_OTH;
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
  if(pinSupport[pinOTH])
    pinMode(pinOTH,INPUT);
  if(pinSupport[pinDSR])
    pinMode(pinDSR,OUTPUT);
  if(pinSupport[pinRI])
    pinMode(pinRI,OUTPUT);
  s_pinWrite(pinRTS,rtsActive);
  s_pinWrite(pinDCD,dcdStatus);
  s_pinWrite(pinDSR,dsrActive);
  s_pinWrite(pinRI,riInactive);
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
  freeCharArray(&tempStateMachine);
  setCharArray(&delimiters,"");
  setCharArray(&stateMachine,"");
  machineState = stateMachine;
  termType = DEFAULT_TERMTYPE;
  busyMsg = DEFAULT_BUSYMSG;
#ifdef SUPPORT_LED_PINS
  if(pinSupport[DEFAULT_PIN_AA])
    pinMode(DEFAULT_PIN_AA,OUTPUT);
  if(pinSupport[DEFAULT_PIN_WIFI])
    pinMode(DEFAULT_PIN_WIFI,OUTPUT);
  if(pinSupport[DEFAULT_PIN_HS])
    pinMode(DEFAULT_PIN_HS,OUTPUT);
#endif
}

char lc(char c)
{
  if((c>=65) && (c<=90))
    return c+32;
  if((c>=193) && (c<=218))
    return c-96;
  return c;
}

void ZCommand::connectionArgs(WiFiClientNode *c)
{
  setCharArray(&(c->delimiters),tempDelimiters);
  setCharArray(&(c->maskOuts),tempMaskOuts);
  setCharArray(&(c->stateMachine),tempStateMachine);
  freeCharArray(&tempDelimiters);
  freeCharArray(&tempMaskOuts);
  freeCharArray(&tempStateMachine);
  c->machineState = c->stateMachine;
}

ZResult ZCommand::doResetCommand(bool resetOpMode)
{
  while(conns != null)
  {
    WiFiClientNode *c=conns;
    delete c;
  }
  current = null;
  nextConn = null;
  WiFiServerNode::DestroyAllServers();
  OpModes oldMode = altOpMode;
  setConfigDefaults();
  String argv[CFG_LAST+1];
  parseConfigOptions(argv);
  eon=0;
  serial.setXON(true);
  packetXOn = true;
# ifdef INCLUDE_CMDRX16
    packetXOn = false;
# endif
  serial.setPetsciiMode(false);
  serialDelayMs=0;
  binType=BTYPE_NORMAL;
  serial.setFlowControlType(DEFAULT_FCT);
  if(!resetOpMode)
    argv[CFG_ALTOPMODE] = String(oldMode);
   setOptionsFromSavedConfig(argv);
  memset(nbuf,0,MAX_COMMAND_SIZE);
  busyMode=false;
  return ZOK;
}

ZResult ZCommand::doNoListenCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  if(vval>0 && isNumber)
  {
    WiFiServerNode *s=servs;
    while(s!=null)
    {
      if(vval == s->id)
      {
        delete s;
        updateAutoAnswer();
        return ZOK;
      }
      s=s->next;
    }
    return ZERROR;
  }
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

bool ZCommand::reSaveConfig(int retries)
{
  char hex[256];
  SPIFFS.remove(CONFIG_FILE_OLD);
  SPIFFS.remove(CONFIG_FILE);
  delay(500);
  const char *eoln = EOLN.c_str();
  int dcdMode = pinModeCoder(dcdActive, dcdInactive, DEFAULT_DCD_ACTIVE);
  int ctsMode = pinModeCoder(ctsActive, ctsInactive, DEFAULT_CTS_ACTIVE);
  int rtsMode = pinModeCoder(rtsActive, rtsInactive, DEFAULT_RTS_ACTIVE);
  int riMode = pinModeCoder(riActive, riInactive, DEFAULT_RTS_ACTIVE);
  int dtrMode = pinModeCoder(dtrActive, dtrInactive, DEFAULT_DTR_ACTIVE);
  int dsrMode = pinModeCoder(dsrActive, dsrInactive, DEFAULT_DSR_ACTIVE);
  String wifiSSIhex = TOHEX(wifiSSI.c_str(),hex,256);
  String wifiPWhex = TOHEX(wifiPW.c_str(),hex,256);
  String zclockFormathex = TOHEX(zclock.getFormat().c_str(),hex,256);
  String zclockHosthex = TOHEX(zclock.getNtpServerHost().c_str(),hex,256);
  String hostnamehex = TOHEX(hostname.c_str(),hex,256);
  String printSpechex = TOHEX(printMode.getLastPrinterSpec(),hex,256);
  String termTypehex = TOHEX(termType.c_str(),hex,256);
  String busyMsghex = TOHEX(busyMsg.c_str(),hex,256);
  String staticIPstr;
  ConnSettings::IPtoStr(staticIP,staticIPstr);
  String staticDNSstr;
  ConnSettings::IPtoStr(staticDNS,staticDNSstr);
  String staticGWstr;
  ConnSettings::IPtoStr(staticGW,staticGWstr);
  String staticSNstr;
  ConnSettings::IPtoStr(staticSN,staticSNstr);

  File f = SPIFFS.open(CONFIG_FILE, "w");
  f.printf("%s,%s,%d,%s,",wifiSSIhex.c_str(), wifiPWhex.c_str(), baudRate, eoln);
  f.printf("%d,%d,%d,%d,",serial.getFlowControlType(), doEcho, suppressResponses, numericResponses);
  f.printf("%d,%d,%d,%d,%d,",longResponses, serial.isPetsciiMode(), dcdMode, serialConfig, ctsMode);
  f.printf("%d,%d,%d,%d,%d,%d,%d,",rtsMode,pinDCD,pinCTS,pinRTS,ringCounter,autoStreamMode,preserveListeners);
  f.printf("%d,%d,%d,%d,%d,%d,",riMode,dtrMode,dsrMode,pinRI,pinDTR,pinDSR);
  f.printf("%d,",zclock.isDisabled()?999:zclock.getTimeZoneCode());
  f.printf("%s,%s,%s,",zclockFormathex.c_str(),zclockHosthex.c_str(),hostnamehex.c_str());
  f.printf("%d,%s,%s,",printMode.getTimeoutDelayMs(),printSpechex.c_str(),termTypehex.c_str());
  f.printf("%s,%s,%s,%s,",staticIPstr.c_str(),staticDNSstr.c_str(),staticGWstr.c_str(),staticSNstr.c_str());
  f.printf("%s,%d,%d,%d",busyMsghex.c_str(),telnetSupport,streamMode.getHangupType(),altOpMode);
  f.close();
  delay(500);
  if(SPIFFS.exists(CONFIG_FILE))
  {
    File f=SPIFFS.open(CONFIG_FILE, "r");
    String str=f.readString();
    f.close();
    int argn=0;
    if((str!=null)&&(str.length()>0))
    {
      debugPrintf("Saved Config: %s\r\n",str.c_str());
      for(int i=0;i<str.length();i++)
      {
        if((str[i]==',')&&(argn<=CFG_LAST))
          argn++;
      }
    }
    if(argn==CFG_LAST)
      return true;
  }
  if(retries>0)
  {
    debugPrintf("Error saving config: Trying again.\r\n");
    delay(100);
    yield();
    return reSaveConfig(retries-1);
  }
  return false;
}

int ZCommand::getConfigFlagBitmap()
{
  return serial.getConfigFlagBitmap() | (doEcho ? FLAG_ECHO : 0);
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
#ifndef INCLUDE_CMDRX16
    packetXOn = true;
#endif
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
  pinModeDecoder(configArguments[CFG_DCDMODE],&dcdActive,&dcdInactive,DEFAULT_DCD_ACTIVE,DEFAULT_DCD_INACTIVE);
  pinModeDecoder(configArguments[CFG_CTSMODE],&ctsActive,&ctsInactive,DEFAULT_CTS_ACTIVE,DEFAULT_CTS_INACTIVE);
  pinModeDecoder(configArguments[CFG_RTSMODE],&rtsActive,&rtsInactive,DEFAULT_RTS_ACTIVE,DEFAULT_RTS_INACTIVE);
  pinModeDecoder(configArguments[CFG_RIMODE],&riActive,&riInactive,DEFAULT_RI_ACTIVE,DEFAULT_RI_INACTIVE);
  pinModeDecoder(configArguments[CFG_DTRMODE],&dtrActive,&dtrInactive,DEFAULT_DTR_ACTIVE,DEFAULT_DTR_INACTIVE);
  pinModeDecoder(configArguments[CFG_DSRMODE],&dsrActive,&dsrInactive,DEFAULT_DSR_ACTIVE,DEFAULT_DSR_INACTIVE);
  if(configArguments[CFG_DCDPIN].length()>0)
  {
    pinDCD = atoi(configArguments[CFG_DCDPIN].c_str());
    if(pinSupport[pinDCD])
      pinMode(pinDCD,OUTPUT);
    dcdStatus=dcdInactive;
  }
  s_pinWrite(pinDCD,dcdStatus);
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
  s_pinWrite(pinRTS,rtsActive);
#ifdef ZIMODEM_ESP32
  serial.setFlowControlType(serial.getFlowControlType());
#endif
  if(configArguments[CFG_RIPIN].length()>0)
  {
    pinRI = atoi(configArguments[CFG_RIPIN].c_str());
    if(pinSupport[pinRI])
      pinMode(pinRI,OUTPUT);
  }
  s_pinWrite(pinRI,riInactive);
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
  s_pinWrite(pinDSR,dsrActive);
  if(configArguments[CFG_S0_RINGS].length()>0)
    ringCounter = atoi(configArguments[CFG_S0_RINGS].c_str());
  if(configArguments[CFG_S41_STREAM].length()>0)
    autoStreamMode = atoi(configArguments[CFG_S41_STREAM].c_str());
  if(configArguments[CFG_S62_TELNET].length()>0)
    telnetSupport = atoi(configArguments[CFG_S62_TELNET].c_str());
  if(configArguments[CFG_S63_HANGUP].length()>0)
    streamMode.setHangupType((HangupType)atoi(configArguments[CFG_S63_HANGUP].c_str()));
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
  if((!zclock.isDisabled()) && (WiFi.status() == WL_CONNECTED))
    zclock.forceUpdate();
  //if(configArguments[CFG_PRINTDELAYMS].length()>0) // since you can't change it, what's the point?
  //  printMode.setTimeoutDelayMs(atoi(configArguments[CFG_PRINTDELAYMS].c_str()));
  printMode.setLastPrinterSpec(configArguments[CFG_PRINTSPEC].c_str());
  if(configArguments[CFG_TERMTYPE].length()>0)
    termType = configArguments[CFG_TERMTYPE];
  if(configArguments[CFG_BUSYMSG].length()>0)
    busyMsg = configArguments[CFG_BUSYMSG];
  if(configArguments[CFG_ALTOPMODE].length()>0)
  {
    altOpMode = (OpModes)atoi(configArguments[CFG_ALTOPMODE].c_str());
    setAltOpModeAdjustments();
  }
  updateAutoAnswer();
}

void ZCommand::parseConfigOptions(String configArguments[])
{
  delay(500);
  bool v2=SPIFFS.exists(CONFIG_FILE);
  File f;
  if(v2)
    f = SPIFFS.open(CONFIG_FILE, "r");
  else
    f = SPIFFS.open(CONFIG_FILE_OLD, "r");
  String str=f.readString();
  f.close();
  if((str!=null)&&(str.length()>0))
  {
    debugPrintf("Read Config: %s\r\n",str.c_str());
    int argn=0;
    for(int i=0;i<str.length();i++)
    {
      if(str[i]==',')
      {
        if(argn<=CFG_LAST)
          argn++;
        else
          break;
      }
      else
        configArguments[argn] += str[i];
    }
    if(v2)
    {
      for(const ConfigOptions *opt = v2HexCfgs ; *opt != 255; opt++)
      {
        char hex[256];
        configArguments[*opt] = FROMHEX(configArguments[*opt].c_str(),hex,256);
      }
    }
  }
}

void ZCommand::loadConfig()
{
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
  hostname = argv[CFG_HOSTNAME];
  setNewStaticIPs(
      ConnSettings::parseIP(argv[CFG_STATIC_IP].c_str()),
      ConnSettings::parseIP(argv[CFG_STATIC_DNS].c_str()),
      ConnSettings::parseIP(argv[CFG_STATIC_GW].c_str()),
      ConnSettings::parseIP(argv[CFG_STATIC_SN].c_str()));
  if(wifiSSI.length()>0)
  {
    debugPrintf("Connecting to %s\r\n",wifiSSI.c_str());
    connectWifi(wifiSSI.c_str(),wifiPW.c_str(),staticIP,staticDNS,staticGW,staticSN);
    nextReconnectDelay = DEFAULT_RECONNECT_DELAY;
  }
  debugPrintf("Resetting...\r\n");
  doResetCommand(true);
  if(altOpMode == OPMODE_NONE)
    showInitMessage();
  debugPrintf("Init complete.\r\n");
}

void ZCommand::setAltOpModeAdjustments()
{
  switch(altOpMode)
  {
  case OPMODE_1650:
  {
    suppressResponses=true;
    doEcho=false;
    busyMode=false;
    autoStreamMode=true;
    telnetSupport=false;
    streamMode.setDefaultEcho(false);
    streamMode.setHangupType(HANGUP_PDP);
    ringCounter=1;
    serial.setFlowControlType(FCT_DISABLED);
    if(baudRate != 300)
    {
      baudRate=300;
      changeBaudRate(baudRate);
    }
    riActive = HIGH; // remember, this are inverted, so active LOW
    riInactive = LOW;
    s_pinWrite(pinRI,riInactive); // drive it high
    othActive = DEFAULT_OTH_ACTIVE;
    othInactive = DEFAULT_OTH_INACTIVE;
    dcdActive = HIGH;
    dcdInactive = LOW;
    checkOpenConnections();
    break;
  }
  case OPMODE_1660:
  {
    suppressResponses=true;
    doEcho=false;
    busyMode=false;
    autoStreamMode=true;
    telnetSupport=false;
    streamMode.setDefaultEcho(false);
    streamMode.setHangupType(HANGUP_PDP);
    ringCounter=1;
    serial.setFlowControlType(FCT_DISABLED);
    if(baudRate != 300)
    {
      baudRate=300;
      changeBaudRate(baudRate);
    }
    riActive = HIGH; // remember, this are inverted, so active LOW
    riInactive = LOW;
    s_pinWrite(pinRI,riInactive); // drive it high
    othActive = DEFAULT_OTH_INACTIVE;
    othInactive = DEFAULT_OTH_ACTIVE;
    checkOpenConnections();
    break;
  }
  case OPMODE_1670:
  {
    busyMode=false;
    autoStreamMode=true;
    telnetSupport=false;
    streamMode.setDefaultEcho(false);
    streamMode.setHangupType(HANGUP_PPPHARD);
    serial.setPetsciiMode(true);
    serial.setXON(true);
    packetXOn = true;
    if(baudRate != 1200)
    {
      baudRate=1200;
      changeBaudRate(baudRate);
    }
    dcdActive = HIGH;
    dcdInactive = LOW;
    checkOpenConnections();
    break;
  }
  default:
    break;
  }
}

ZResult ZCommand::doInfoCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  if(vval == 0)
  {
    showInitMessage();
  }
  else
  switch(vval)
  {
  case 1:
  case 5:
  {
    bool showAll = (vval==5);
#   ifdef INCLUDE_CMDRX16
    serial.prints("X16:");
#   endif
    switch(altOpMode)
    {
      case OPMODE_NONE:
        break;
      case OPMODE_1650:
        serial.prints("1650:");
        break;
      case OPMODE_1660:
        serial.prints("1660:");
        break;
      case OPMODE_1670:
        serial.prints("1670:");
        break;
    }
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
    if(busyMode)
      serial.prints("H1");
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
    if((dcdActive != DEFAULT_DCD_ACTIVE)||(dcdInactive == DEFAULT_DCD_ACTIVE)||(showAll))
      serial.printf("S46=%d",pinModeCoder(dcdActive,dcdInactive,DEFAULT_DCD_ACTIVE));
    if((pinDCD != DEFAULT_PIN_DCD)||(showAll))
      serial.printf("S47=%d",pinDCD);
    if((ctsActive != DEFAULT_CTS_ACTIVE)||(ctsInactive == DEFAULT_CTS_ACTIVE)||(showAll))
      serial.printf("S48=%d",pinModeCoder(ctsActive,ctsInactive,DEFAULT_CTS_ACTIVE));
    if((pinCTS != DEFAULT_PIN_CTS)||(showAll))
      serial.printf("S49=%d",pinCTS);
    if((rtsActive != DEFAULT_RTS_ACTIVE)||(rtsInactive == DEFAULT_RTS_ACTIVE)||(showAll))
      serial.printf("S50=%d",pinModeCoder(rtsActive,rtsInactive,DEFAULT_RTS_ACTIVE));
    if((pinRTS != DEFAULT_PIN_RTS)||(showAll))
      serial.printf("S51=%d",pinRTS);
    if((riActive != DEFAULT_RI_ACTIVE)||(riInactive == DEFAULT_RI_ACTIVE)||(showAll))
      serial.printf("S52=%d",pinModeCoder(riActive,riInactive,DEFAULT_RI_ACTIVE));
    if((pinRI != DEFAULT_PIN_RI)||(showAll))
      serial.printf("S53=%d",pinRI);
    if((dtrActive != DEFAULT_DTR_ACTIVE)||(dtrInactive == DEFAULT_DTR_ACTIVE)||(showAll))
      serial.printf("S54=%d",pinModeCoder(dtrActive,dtrInactive,DEFAULT_DTR_ACTIVE));
    if((pinDTR != DEFAULT_PIN_DTR)||(showAll))
      serial.printf("S55=%d",pinDTR);
    if((dsrActive != DEFAULT_DSR_ACTIVE)||(dsrInactive == DEFAULT_DSR_ACTIVE)||(showAll))
      serial.printf("S56=%d",pinModeCoder(dsrActive,dsrInactive,DEFAULT_DSR_ACTIVE));
    if((pinDSR != DEFAULT_PIN_DSR)||(showAll))
      serial.printf("S57=%d",pinDSR);
    if(preserveListeners ||(showAll))
      serial.prints(preserveListeners ? "S60=1" : "S60=0");
    if(!telnetSupport ||(showAll))
      serial.prints(telnetSupport ? "S62=1" : "S62=0");
    if((streamMode.getHangupType()!=HANGUP_NONE) ||(showAll))
    {
      switch(streamMode.getHangupType())
      {
        case HANGUP_PPPHARD:
          serial.prints("S63=1");
          break;
        case HANGUP_DTR:
          serial.prints("S63=2");
          break;
        case HANGUP_PDP:
          serial.prints("S63=3");
          break;
        default:
          serial.prints("S63=0");
          break;
      }
    }
    if((serial.isPetsciiMode())||(showAll))
      serial.prints(serial.isPetsciiMode() ? "&P1" : "&P0");
    if((logFileOpen) || showAll)
      serial.prints((logFileOpen && !logFile2Uart) ? "&O1" : ((logFileOpen && logFile2Uart) ? "&O88" : "&O0"));
    serial.prints(EOLN);
    break;
  }
  case 2:
  {
    serial.prints(WiFi.localIP().toString().c_str());
    serial.prints(EOLN);
    break;
  }
  case 3:
  {
    serial.prints(wifiSSI.c_str());
    serial.prints(EOLN);
    break;
  }
  case 4:
  {
    serial.prints(ZIMODEM_VERSION);
    serial.prints(EOLN);
    break;
  }
  case 6:
  {
    serial.prints(WiFi.macAddress());
    serial.prints(EOLN);
    break;
  }
  case 7:
  {
    serial.prints(zclock.getCurrentTimeFormatted());
    serial.prints(EOLN);
    break;
  }
  case 8:
  {
    serial.prints(compile_date);
    serial.prints(EOLN);
    break;
  }
  case 9:
  {
    serial.prints(wifiSSI.c_str());
    serial.prints(EOLN);
    if(staticIP != null)
    {
      String str;
      ConnSettings::IPtoStr(staticIP,str);
      serial.prints(str.c_str());
      serial.prints(EOLN);
      ConnSettings::IPtoStr(staticDNS,str);
      serial.prints(str.c_str());
      serial.prints(EOLN);
      ConnSettings::IPtoStr(staticGW,str);
      serial.prints(str.c_str());
      serial.prints(EOLN);
      ConnSettings::IPtoStr(staticSN,str);
      serial.prints(str.c_str());
      serial.prints(EOLN);
    }
    break;
  }
  case 10:
    serial.printf("%s%s",printMode.getLastPrinterSpec(),EOLN.c_str());
    break;
  case 11:
  {
    serial.printf("%d%s",ESP.getFreeHeap(),EOLN.c_str());
    break;
  }
  default:
    return ZERROR;
  }
  return ZOK;
}

ZResult ZCommand::doBaudCommand(int vval, uint8_t *vbuf, int vlen)
{
  int baudChk = baudRate;
  uint32_t configChk = serialConfig;
  if(vval<=0)
  {
    char *commaLoc=strchr((char *)vbuf,',');
    if(commaLoc == NULL)
      return ZERROR;
    char *conStr=commaLoc+1;
    if(strlen(conStr)!=3)
      return ZERROR;
    *commaLoc=0;
    configChk = 0;
    baudChk=atoi((char *)vbuf);
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
    default:
      return ZERROR;
    }
    switch(conStr[1])
    {
      case 'o': case 'O':
        configChk = configChk | UART_PARITY_ODD;
        break;
      case 'e': case 'E':
        configChk = configChk | UART_PARITY_EVEN;
        break;
      case 'm': case 'M':
        configChk = configChk | UART_PARITY_MASK;
        break;
      case 'n': case 'N':
        configChk = configChk | UART_PARITY_NONE;
        break;
      default:
        return ZERROR;
    }
    switch(conStr[2])
    {
      case '1':
        configChk = configChk | UART_NB_STOP_BIT_1;
        break;
      case '2':
        configChk = configChk | UART_NB_STOP_BIT_2;
        break;
      default:
        return ZERROR;
    }
  }
  else
    baudChk=vval;
  hwSerialFlush();
  if(baudChk != baudRate)
  {
    if((baudChk<128)||(baudChk>921600))
      return ZERROR;
    baudRate = baudChk;
    changeBaudRate(baudRate);
  }
  if(serialConfig != configChk)
  {
    serialConfig = (SerialConfig)configChk;
    changeSerialConfig(serialConfig);
  }
  return ZOK;
}

ZResult ZCommand::doConnectCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers)
{
  if((WiFi.status() != WL_CONNECTED)
  &&((vlen==0)||(isNumber && (vval==0)))
  &&(conns==null))
  {
    if(wifiSSI.length()==0)
      return ZERROR;
    debugPrintf("Connecting to %s\r\n",wifiSSI.c_str());
    bool doconn = connectWifi(wifiSSI.c_str(),wifiPW.c_str(),staticIP,staticDNS,staticGW,staticSN);
    debugPrintf("Done attempting connect to %s\r\n",wifiSSI.c_str());
    return doconn ? ZOK : ZERROR;
  }
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
        serial.flush();
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
        connectionArgs(c);
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
          serial.flush();
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
    char *host = 0;
    int port = -1;
    char *username = 0;
    char *password = 0;
    parseHostInfo(vbuf, &host, &port, &username, &password);
    int flagsBitmap=0;
    {
      ConnSettings flags(dmodifiers);
      flagsBitmap = flags.getBitmap(serial.getFlowControlType());
    }
    if(port < 0)
      port = ((username != 0) && ((flagsBitmap&FLAG_SECURE)==FLAG_SECURE))?22:23;
    if(username != null)
      logPrintfln("Connnecting: %s@%s:%d %d",username,host,port,flagsBitmap);
    else
      logPrintfln("Connnecting: %s %d %d",host,port,flagsBitmap);
    WiFiClientNode *c = new WiFiClientNode(host,port,username,password,flagsBitmap);
    if(!c->isConnected())
    {
      logPrintln("Connnect: FAIL");
      delete c;
      return ZNOANSWER;
    }
    else
    {
      logPrintfln("Connnect: SUCCESS: %d",c->id);
      current=c;
      connectionArgs(c);
      return ZCONNECT;
    }
  }
  return ZOK;
}

void ZCommand::headerOut(const int channel, const int num, const int sz, const int crc8)
{
  switch(binType)
  {
  case BTYPE_NORMAL:
    sprintf(hbuf,"[ %d %d %d ]%s",channel,sz,crc8,EOLN.c_str());
    break;
  case BTYPE_NORMAL_PLUS:
    sprintf(hbuf,"[ %d %d %d %d ]%s",channel,num,sz,crc8,EOLN.c_str());
    break;
  case BTYPE_HEX:
    sprintf(hbuf,"[ %s %s %s ]%s",
        String(TOHEX(channel)).c_str(),
        String(TOHEX(sz)).c_str(),
        String(TOHEX(crc8)).c_str(),EOLN.c_str());
    break;
  case BTYPE_HEX_PLUS:
    sprintf(hbuf,"[ %s %s %s %s ]%s",
        String(TOHEX(channel)).c_str(),
        String(TOHEX(num)).c_str(),
        String(TOHEX(sz)).c_str(),
        String(TOHEX(crc8)).c_str(),EOLN.c_str());
    break;
  case BTYPE_DEC:
    sprintf(hbuf,"[%s%d%s%d%s%d%s]%s",EOLN.c_str(),channel,EOLN.c_str(),sz,EOLN.c_str(),crc8,EOLN.c_str(),EOLN.c_str());
    break;
  case BTYPE_DEC_PLUS:
    sprintf(hbuf,"[%s%d%s%d%s%d%s%d%s]%s",EOLN.c_str(),
        channel,EOLN.c_str(),
        num,EOLN.c_str(),
        sz,EOLN.c_str(),
        crc8,EOLN.c_str(),
        EOLN.c_str());
    break;
  case BTYPE_NORMAL_NOCHK:
    sprintf(hbuf,"[ %d %d ]%s",channel,sz,EOLN.c_str());
    break;
  }
  serial.prints(hbuf);
}

ZResult ZCommand::doWebStream(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *filename, bool cache)
{
  char *hostIp;
  char *req;
  int port;
  bool doSSL;
  bool gopher = false;
#ifdef INCLUDE_FTP
  FTPHost *ftpHost = 0;
  if((strstr((char *)vbuf,"ftp:")==(char *)vbuf)
  ||(strstr((char *)vbuf,"ftps:")==(char *)vbuf))
  {
    ftpHost = makeFTPHost(true,ftpHost,vbuf,&req);
    if((req == 0)||(ftpHost==0))
      return ZERROR;
  }
  else
#endif
  if((strstr((char *)vbuf,"gopher:")==(char *)vbuf)
  ||(strstr((char *)vbuf,"gophers:")==(char *)vbuf))
    gopher=true;
  if(!parseWebUrl(vbuf,&hostIp,&req,&port,&doSSL))
    return ZERROR;

  if(cache)
  {
    if(!SPIFFS.exists(filename))
    {
#ifdef INCLUDE_FTP
      if(ftpHost != 0)
      {
        if(!ftpHost->doGet(&SPIFFS,filename,req))
        {
          delete ftpHost;
          return ZERROR;
        }
      }
      else
#endif
      if(gopher)
      {
        if(!doGopherGet(hostIp, port, &SPIFFS, filename, req, doSSL))
          return ZERROR;
      }
      else
      if(!doWebGet(hostIp, port, &SPIFFS, filename, req, doSSL))
        return ZERROR;
    }
  }
  else
  if((binType == BTYPE_NORMAL_NOCHK)
  &&(machineQue.length()==0))
  {
    uint32_t respLength=0;
    WiFiClient *c;
    
#ifdef INCLUDE_FTP
    if(ftpHost != 0)
    {
      c=ftpHost->doGetStream(req, &respLength);
      if(c==null)
        delete ftpHost;
    }
    else
#endif
    if(gopher)
      c = doGopherGetStream(hostIp, port, req, doSSL, &respLength);
    else
      c = doWebGetStream(hostIp, port, req, doSSL, &respLength);
    if(c==null)
    {
      serial.prints(EOLN);
      return ZERROR;
    }
    headerOut(0,1,respLength,0);
    serial.flush(); // stupid important because otherwise apps that go xoff miss the header info
    ZResult res = doWebDump(c,respLength,false);
#ifdef INCLUDE_FTP
    if(ftpHost != 0)
      delete ftpHost;
    else
#endif
    {
      c->stop();
      delete c;
    }
    serial.prints(EOLN);
    return res;
  }
  else
  {
    if(SPIFFS.exists(filename))
      SPIFFS.remove(filename);
#ifdef INCLUDE_FTP
    if(ftpHost != 0)
    {
      if(!ftpHost->doGet(&SPIFFS,filename,req))
      {
        delete ftpHost;
        return ZERROR;
      }
    }
    else
#endif
    if(gopher)
    {
      if(!doGopherGet(hostIp, port, &SPIFFS, filename, req, doSSL))
        return ZERROR;
    }
    else
    if(!doWebGet(hostIp, port, &SPIFFS, filename, req, doSSL))
      return ZERROR;
  }
  return doWebDump(filename, cache);
}

ZResult ZCommand::doWebDump(Stream *in, int len, const bool cacheFlag)
{
  bool flowControl=!cacheFlag;
  BinType streamType = cacheFlag?BTYPE_NORMAL:binType;
  uint8_t *buf = (uint8_t *)malloc(1);
  uint16_t bufLen = 1;
  int bct=0;
  unsigned long now = millis();
  while(((len>0)||(in->available()>0))
  && ((millis()-now)<10000))
  {
    if(((!flowControl) || serial.isSerialOut())
    &&(in->available()>0))
    {
      now=millis();
      len--;
      int c=in->read();
      if(c<0)
        break;
      buf[0] = (uint8_t)c;
      bufLen = 1;
      buf = doMaskOuts(buf,&bufLen,maskOuts);
      buf = doStateMachine(buf,&bufLen,&machineState,&machineQue,stateMachine);
      for(int i=0;i<bufLen;i++)
      {
        c=buf[i];
        if(cacheFlag && serial.isPetsciiMode())
          c=ascToPetcii(c);
        
        switch(streamType)
        {
          case BTYPE_NORMAL:
          case BTYPE_NORMAL_NOCHK:
          case BTYPE_NORMAL_PLUS:
            serial.write((uint8_t)c);
            break;
          case BTYPE_HEX:
          case BTYPE_HEX_PLUS:
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
          case BTYPE_DEC_PLUS:
            serial.printf("%d%s",c,EOLN.c_str());
            break;
        }
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
      free(buf);
      machineState = stateMachine;
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
        free(buf);
        machineState = stateMachine;
        return ZOK;
      }
      delay(1);
    }
    yield();
  }
  free(buf);
  machineState = stateMachine;
  if(bct > 0)
    serial.prints(EOLN);
  return ZOK;
}

ZResult ZCommand::doWebDump(const char *filename, const bool cache)
{
  machineState = stateMachine;
  int chk8=0;
  uint16_t bufLen = 1;
  int len = 0;
  
  {
    File f = SPIFFS.open(filename, "r");
    int flen = f.size();
    if((binType != BTYPE_NORMAL_NOCHK)
    &&(machineQue.length()==0))
    {
      uint8_t *buf = (uint8_t *)malloc(1);
      delay(100);
      char *oldMachineState = machineState;
      String oldMachineQue = machineQue;
      for(int i=0;i<flen;i++)
      {
        int c=f.read();
        if(c<0)
          break;
        buf[0]=c;
        bufLen = 1;
        buf = doMaskOuts(buf,&bufLen,maskOuts);
        buf = doStateMachine(buf,&bufLen,&machineState,&machineQue,stateMachine);
        len += bufLen;
        if(!cache)
        {
          for(int i1=0;i1<bufLen;i1++)
          {
            chk8+=buf[i1];
            if(chk8>255)
              chk8-=256;
          }
        }
      }
      machineState = oldMachineState;
      machineQue = oldMachineQue;
      free(buf);
    }
    else
      len=flen;
    f.close();
  }
  File f = SPIFFS.open(filename, "r");
  if(!cache)
  {
    headerOut(0,1,len,chk8);
    serial.flush(); // stupid important because otherwise apps that go xoff miss the header info
  }
  len = f.size();
  ZResult res = doWebDump(&f, len, cache);
  f.close();
  return res;
}

ZResult ZCommand::doUpdateFirmware(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  serial.prints("Local firmware version ");
  serial.prints(ZIMODEM_VERSION);
  serial.prints(".");
  serial.prints(EOLN);

  uint8_t buf[255];
  int bufSize = 254;
  char firmwareName[100];
#ifdef USE_DEVUPDATER
  char *updaterHost = "192.168.1.10";
  int updaterPort = 8080;
#else
# ifdef INCLUDE_CMDRX16
   char *updaterHost = "www.zimmers.net"; // changeme!
# else
   char *updaterHost = "www.zimmers.net";
# endif
  int updaterPort = 80;
#endif
#ifdef INCLUDE_CMDRX16
  char *updaterPrefix = "/otherprojs/x16";
#else
# ifdef ZIMODEM_ESP32
   char *updaterPrefix = "/otherprojs/guru2";
# else
  char *updaterPrefix = "/otherprojs/c64net2";
# endif
#endif
  sprintf(firmwareName,"%s-latest-version.txt",updaterPrefix);
  if((!doWebGetBytes(updaterHost, updaterPort, firmwareName, false, buf, &bufSize))||(bufSize<=0))
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
  {
    bufSize--;
    buf[bufSize] = 0;
  }
  
  if((strlen(ZIMODEM_VERSION)==bufSize) && memcmp(buf,ZIMODEM_VERSION,strlen(ZIMODEM_VERSION))==0)
  {
    serial.prints("Your modem is up-to-date.");
    serial.prints(EOLN);
    if(vval == 6502)
      return ZOK;
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
  sprintf(firmwareName,"%s-firmware-%s.bin", updaterPrefix, buf);
  uint32_t respLength=0;
  WiFiClient *c = doWebGetStream(updaterHost, updaterPort, firmwareName, false, &respLength);
  if(c==null)
  {
    serial.prints(EOLN);
    return ZERROR;
  }

  if(!Update.begin((respLength == 0) ? 4096 : respLength))
  {
    c->stop();
    delete c;
    return ZERROR;
  }

  serial.prints(".");
  serial.flush();
  int writeBytes = Update.writeStream(*c);
  if(writeBytes != respLength)
  {
    c->stop();
    delete c;
    serial.prints(EOLN);
    return ZERROR;
  }
  serial.prints(".");
  serial.flush();
  if(!Update.end())
  {
    c->stop();
    delete c;
    serial.prints(EOLN);
    return ZERROR;
  }
  c->stop();
  delete c;
  serial.prints("Done");
  serial.prints(EOLN);
  serial.prints("Modem will now restart, but you should power-cycle or reset your modem.");
  ESP.restart();
  return ZOK;
}

ZResult ZCommand::doWiFiCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers)
{
  bool doPETSCII = (strchr(dmodifiers,'p')!=null) || (strchr(dmodifiers,'P')!=null);
  if((vlen==0)||((isNumber && (vval>=0))))
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
    char *ssi=(char *)vbuf;
    char *pw=ssi + strlen(ssi);
    IPAddress *ip[4];
    for(int i=0;i<4;i++)
      ip[i]=null;
    if(x != 0)
    {
      *x=0;
      pw=x+1;
      x=strstr(pw,",");
      if(x != 0)
      {
        int numCommasFound=0;
        int numDotsFound=0;
        char *comPos[4];
        for(char *e=pw+strlen(pw)-1;e>pw;e--)
        {
          if(*e==',')
          {
            if(numDotsFound!=3)
              break;
            numDotsFound=0;
            if(numCommasFound<4)
            {
              numCommasFound++;
              comPos[4-numCommasFound]=e;
            }
            if(numCommasFound==4)
                break;
          }
          else
          if(*e=='.')
            numDotsFound++;
          else
          if(strchr("0123456789 ",*e)==null)
            break;
        }
        if(numCommasFound==4)
        {
          for(int i=0;i<4;i++)
            *(comPos[i])=0;
          for(int i=0;i<4;i++)
          {
            ip[i]=ConnSettings::parseIP(comPos[i]+1);
            if(ip[i]==null)
            {
              while(--i>=0)
              {
                free(ip[i]);
                ip[i]=null;
              }
              break;
            }
          }
        }
      }
    }
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
      connSuccess = connectWifi(ssiP,pwP,ip[0],ip[1],ip[2],ip[3]);
      free(ssiP);
      free(pwP);
    }
    else
      connSuccess = connectWifi(ssi,pw,ip[0],ip[1],ip[2],ip[3]);

    if(!connSuccess)
    {
      for(int ii=0;ii<4;ii++)
      {
        if(ip[ii]!=null)
          free(ip[ii]);
      }
      return ZERROR;
    }
    else
    {
      wifiSSI=ssi;
      wifiPW=pw;
      setNewStaticIPs(ip[0],ip[1],ip[2],ip[3]);
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
  if((current==null)||(!current->isConnected()))
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
    uint8_t buf[vlen+2];
    memcpy(buf,vbuf,vlen);
    rcvdCrc8=CRC8(buf,vlen);
    if((crcChk != -1)&&(rcvdCrc8!=crcChk))
      return ZERROR;
    if(current->isPETSCII() || doPETSCII)
    {
      for(int i=0;i<vlen;i++)
        buf[i] = petToAsc(buf[i]);
    }
    buf[vlen]=13; // special case
    buf[vlen+1]=10; // special case
    current->write(buf,vlen+2);
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
    PhoneBookEntry *phb=PhoneBookEntry::findPhonebookEntry(vval);
    if(phb != null)
    {
        int addrLen=strlen(phb->address);
        uint8_t *vbuf = new uint8_t[addrLen+1];
        strcpy((char *)vbuf,phb->address);
        ZResult res = doDialStreamCommand(0,vbuf,addrLen,false,phb->modifiers);
        free(vbuf);
        return res;
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
      connectionArgs(c);
      if(strchr(dmodifiers,';')==0)
        streamMode.switchTo(c);
      return ZCONNECT;
    }
    else
      return ZERROR;
  }
  else
  {
    ConnSettings flags(dmodifiers);
    if(!telnetSupport)
      flags.setFlag(FLAG_TELNET, false);
    int flagsBitmap = flags.getBitmap(serial.getFlowControlType());
    char *host = 0;
    int port = -1;
    char *username = 0;
    char *password = 0;
    parseHostInfo(vbuf, &host, &port, &username, &password);
    if(port < 0)
      port = ((username != 0) && ((flagsBitmap&FLAG_SECURE)==FLAG_SECURE))?22:23;
    WiFiClientNode *c = new WiFiClientNode(host,port,username,password,flagsBitmap | FLAG_DISCONNECT_ON_EXIT);
    if(!c->isConnected())
    {
      delete c;
      return ZNOANSWER;
    }
    else
    {
      current=c;
      connectionArgs(c);
      if(strchr(dmodifiers,';')==0)
        streamMode.switchTo(c);
      return ZCONNECT;
    }
  }
  return ZOK;
}

ZResult ZCommand::doPhonebookCommand(unsigned long vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers)
{
  if((vlen==0)||(isNumber)||((vlen==1)&&(*vbuf='?')))
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
          if(!isNumber)
          {
            serial.prints(" (");
            serial.prints(phb->notes);
            serial.prints(")");
          }
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
    if(strchr("0123456789",*cptr) == 0)
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
  char *colon = strchr(rest,':');
  if(colon == NULL)
    return ZERROR;
  char *comma = strchr(colon,',');
  char *notes = "";
  if(comma != NULL)
  {
    *comma=0;
    notes = comma+1;
  }
  if(!PhoneBookEntry::checkPhonebookEntry(colon))
      return ZERROR;
  if(found != null)
    delete found;
  PhoneBookEntry *newEntry = new PhoneBookEntry(number,rest,dmodifiers,notes);
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
        &&(c->id == lastServerClientId))
        {
          current=c;
          checkOpenConnections();
          if((!c->isAnswered()) || (ringCounter==0))
          {
            if(autoStreamMode)
              sendConnectionNotice(baudRate);
            else
              sendConnectionNotice(c->id);
            c->answer();
            ringCounter = 0;
            streamMode.switchTo(c);
            checkOpenConnections();
            busyMode=false;
            return ZIGNORE;
          }
          else
          {
              busyMode=false;
              streamMode.switchTo(c);
              checkOpenConnections();
              return ZOK;
          }
          break;
        }
        c=c->next;
      }
      busyMode=false;
      return ZOK; // not really doing anything important...
  }
  else
  {
    while(WiFi.status() != WL_CONNECTED)
      return ZERROR;
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
    setCharArray(&(newServer->stateMachine),tempStateMachine);
    freeCharArray(&tempDelimiters);
    freeCharArray(&tempMaskOuts);
    freeCharArray(&tempStateMachine);
    updateAutoAnswer();
    busyMode=false;
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
    lastServerClientId=0;
    current = null;
    nextConn = null;
    busyMode=false;
    return ZOK;
  }
  else
  if(isNumber && (vval == 0))
  {
      if(current != 0)
      {
          if(lastServerClientId==current->id)
            lastServerClientId=0;
        delete current;
        current = conns;
        nextConn = conns;
        busyMode=false;
        return ZOK;
      }
      if(busyMode)
      {
        busyMode=false;
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
        if(lastServerClientId==c->id)
          lastServerClientId=0;
        delete c;
        return ZOK;
      }
      c=c->next;
    }
    if(vval == 1) // enter busy mode ATH1 in command mode...
    {
      busyMode=true;
      return ZOK;
    }
    return ZERROR;
  }
  return ZERROR;
}

int ZCommand::getStatusRegister(const int snum, int crc8)
{
  switch(snum)
  {
  case 0:
    return ringCounter;
  case 2:
    return (int)ECS[0];
  case 3:
    return (int)CR[0];
  case 4:
    return (int)LF[0];
  case 5:
    return (int)BS;
  case 40:
    return packetSize;
 case 41:
   return autoStreamMode?1:0;
 case 42:
   return crc8;
 case 43:
   return tempBaud;
 case 44:
   return serialDelayMs;
 case 45:
   return binType;
 case 46:
   return pinModeCoder(dcdActive,dcdInactive,DEFAULT_DCD_ACTIVE);
 case 47:
   return pinDCD;
 case 48:
   return pinModeCoder(ctsActive,ctsInactive,DEFAULT_CTS_ACTIVE);
 case 49:
   return pinCTS;
 case 50:
   return pinModeCoder(rtsActive,rtsInactive,DEFAULT_RTS_ACTIVE);
 case 51:
   return pinRTS;
 case 52:
   return pinModeCoder(riActive,riInactive,DEFAULT_RI_ACTIVE);
 case 53:
   return pinRI;
 case 54:
   return pinModeCoder(dtrActive,dtrInactive,DEFAULT_DTR_ACTIVE);
 case 55:
   return pinDTR;
 case 56:
   return pinModeCoder(dsrActive,dsrInactive,DEFAULT_DSR_ACTIVE);
 case 57:
   return pinDSR;
 case 60:
   return (preserveListeners)?1:0;
 case 61:
   return (int)round(printMode.getTimeoutDelayMs()/1000);
 case 62:
   return  telnetSupport ? 1 : 0;
 case 63:
   switch(streamMode.getHangupType())
   {
     case HANGUP_NONE: return 0;
     case HANGUP_PPPHARD: return 1;
     case HANGUP_DTR: return 2;
     case HANGUP_PDP: return 3;
   }
 default:
    break;
  }
  return 0;
}

ZResult ZCommand::setStatusRegister(const int snum, const int sval, int *crc8, const ZResult oldRes)
{
  ZResult result = oldRes;
  switch(snum)
  {
  case 0:
    if((sval < 0)||(sval>255))
      result=ZERROR;
    else
    {
      ringCounter = sval;
      updateAutoAnswer();
    }
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
 {
    autoStreamMode = (sval > 0);
    updateAutoAnswer();
    break;
 }
 case 42:
   *crc8=sval;
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
   pinModeDecoder(sval,&dcdActive,&dcdInactive,DEFAULT_DCD_ACTIVE,DEFAULT_DCD_INACTIVE);
   dcdStatus = wasActive?dcdActive:dcdInactive;
   result=ZOK;
   s_pinWrite(pinDCD,dcdStatus);
   break;
 }
 case 47:
   if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
   {
     pinDCD=sval;
     pinMode(pinDCD,OUTPUT);
     s_pinWrite(pinDCD,dcdStatus);
     result=ZOK;
   }
   else
     result=ZERROR;
   break;
 case 48:
   pinModeDecoder(sval,&ctsActive,&ctsInactive,DEFAULT_CTS_ACTIVE,DEFAULT_CTS_INACTIVE);
   result=ZOK;
   break;
 case 49:
   if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
   {
     pinCTS=sval;
     pinMode(pinCTS,INPUT);
     serial.setFlowControlType(serial.getFlowControlType());
     result=ZOK;
   }
   else
     result=ZERROR;
   break;
 case 50:
   pinModeDecoder(sval,&rtsActive,&rtsInactive,DEFAULT_RTS_ACTIVE,DEFAULT_RTS_INACTIVE);
   if(pinSupport[pinRTS])
   {
     serial.setFlowControlType(serial.getFlowControlType());
     s_pinWrite(pinRTS,rtsActive);
   }
   result=ZOK;
   break;
 case 51:
   if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
   {
     pinRTS=sval;
     pinMode(pinRTS,OUTPUT);
     serial.setFlowControlType(serial.getFlowControlType());
     s_pinWrite(pinRTS,rtsActive);
     result=ZOK;
   }
   else
     result=ZERROR;
   break;
 case 52:
   pinModeDecoder(sval,&riActive,&riInactive,DEFAULT_RI_ACTIVE,DEFAULT_RI_INACTIVE);
   if(pinSupport[pinRI])
     s_pinWrite(pinRI,riInactive);
   result=ZOK;
   break;
 case 53:
   if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
   {
     pinRI=sval;
     pinMode(pinRI,OUTPUT);
     s_pinWrite(pinRI,riInactive);
     result=ZOK;
   }
   else
     result=ZERROR;
   break;
 case 54:
   pinModeDecoder(sval,&dtrActive,&dtrInactive,DEFAULT_DTR_ACTIVE,DEFAULT_DTR_INACTIVE);
   result=ZOK;
   break;
 case 55:
   if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
   {
     pinDTR=sval;
     pinMode(pinDTR,INPUT);
     result=ZOK;
   }
   else
     result=ZERROR;
   break;
 case 56:
   pinModeDecoder(sval,&dsrActive,&dsrInactive,DEFAULT_DSR_ACTIVE,DEFAULT_DSR_INACTIVE);
   s_pinWrite(pinDSR,dsrActive);
   result=ZOK;
   break;
 case 57:
   if((sval >= 0) && (sval <= MAX_PIN_NO) && pinSupport[sval])
   {
     pinDSR=sval;
     pinMode(pinDSR,OUTPUT);
     s_pinWrite(pinDSR,dsrActive);
     result=ZOK;
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
 case 61:
   if(sval > 0)
     printMode.setTimeoutDelayMs(sval * 1000);
   else
     result=ZERROR;
     break;
 case 62:
   telnetSupport = (sval > 0);
   break;
 case 63:
   streamMode.setHangupType(HANGUP_NONE);
   if(sval == 1)
     streamMode.setHangupType(HANGUP_PPPHARD);
   else
   if(sval == 2)
     streamMode.setHangupType(HANGUP_DTR);
   else
   if(sval == 3)
     streamMode.setHangupType(HANGUP_PDP);
   break;
 default:
    break;
  }
  return result;
}

void ZCommand::updateAutoAnswer()
{
#ifdef SUPPORT_LED_PINS
    bool setPin = (ringCounter>0) && (autoStreamMode) && (servs != NULL);
    s_pinWrite(DEFAULT_PIN_AA,setPin?DEFAULT_AA_ACTIVE:DEFAULT_AA_INACTIVE);
#endif
}

ZResult ZCommand::doLastPacket(int vval, uint8_t *vbuf, int vlen, bool isNumber)
{
  if(!isNumber)
    return ZERROR;
  WiFiClientNode *cnode=null;
  uint8_t which = 1;
  if(vval == 0)
    vval = lastPacketId;
  else
  {
    uint8_t *c = vbuf;
    while(*c++ == '0')
      which++;
  }
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
  reSendLastPacket(cnode,which);
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
  while((HWSerial.available()>0)
        &&(!crReceived))
  {
    uint8_t c=HWSerial.read();
    logSerialIn(c);
    if((c==CR[0])||(c==LF[0]))
    {
      if(doEcho)
      {
        echoEOLN(c);
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

  debugPrintf("Next command: %s\r\n",currentCommand.c_str());
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
  else
    debugPrintf("Command: %s\r\n",sbuf.c_str());
  
  int crc8=-1;  
  ZResult result=ZOK;
  
  if((sbuf.length()==4)
  &&(strcmp(sbuf.c_str(),"%!PS")==0))
  {
    result = printMode.switchToPostScript("%!PS\n");
    sendOfficialResponse(result);
    return result;
  }
  else
  if((sbuf.length()==12)
  &&(strcmp(sbuf.c_str(),"\x04grestoreall")==0))
  {
    result = printMode.switchToPostScript("%!PS\ngrestoreall\n");
    sendOfficialResponse(result);
    return result;
  }
  
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
        if((lastCmd=='+')||(lastCmd=='$'))
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
          const char *NUMMODIFIERS;
          if(strchr("d", lastCmd) != null)
            NUMMODIFIERS="-@,;";
          else
            NUMMODIFIERS="-";
          char c='\0';
          for(int i=vstart;i<vstart+vlen;i++)
          {
            c=sbuf[i];
            isNumber = ((strchr(NUMMODIFIERS,c)!=null) || ((c>='0')&&(c<='9'))) && isNumber;
          }
          if((strchr("d", lastCmd) != null)
          &&(isNumber)
          && (c == ';')) // when at the end of the number, overrides the auto-stream-modem var
            dmodifiers += ";";
        }
        else
        while((index<len)
        &&(!((lc(sbuf[index])>='a')&&(lc(sbuf[index])<='z')))
        &&(sbuf[index]!='&')
        &&(sbuf[index]!='%')
        &&(sbuf[index]!='+')
        &&(sbuf[index]!=' '))
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
        result = doResetCommand(false);
        break;
      case 'n':
        doNoListenCommand(vval,vbuf,vlen,isNumber);
        break;
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
        if(altOpMode != OPMODE_NONE)
        {
          streamMode.setDefaultEcho((isNumber && vval == 0));
          break;
        }
        else
        if((!isNumber)||(vval>=FCT_INVALID))
          result=ZERROR;
        else
        {
#ifndef INCLUDE_CMDRX16
            packetXOn = true;
#endif
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
            lastServerClientId = current->id;
            result = doAnswerCommand(vval,vbuf,vlen,isNumber,dmodifiers.c_str());
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
          char *eq=strchr((char *)vbuf,'=');
          if(eq == null)
          {
              int snum = vval;
              eq=strchr((char *)vbuf,'?');
              if((eq != null) && (!isNumber))
              {
                *eq = 0;
                snum = atoi((char *)vbuf);
              }
              if((snum == 0)&&(vbuf[0]!='0'))
                result=ZERROR;
              else
              {
                int res = getStatusRegister(snum,crc8);
                serial.printf("%d%s",res,EOLN.c_str());
                result = ZOK;
              }
          }
          else
          if((vlen<3)||(eq == (char *)vbuf)||(eq>=(char *)&(vbuf[vlen-1])))
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
              result = setStatusRegister(snum,sval,&crc8,result);
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
        if((strstr((const char *)vbuf,"shell")==(char *)vbuf)
        &&(SD.cardType() != CARD_NONE))
        {
            char *colon=strchr((const char*)vbuf,':');
            result = ZOK;
            if(colon == 0)
              browseMode.switchTo();
            else
            {
              String line = colon+1;
              line.trim();
              browseMode.init();
              browseMode.doModeCommand(line,false);
            }
        }
#  ifdef INCLUDE_HOSTCM
        else
        if((strstr((const char *)vbuf,"hostcm")==(char *)vbuf))
        {
            result = ZOK;
            hostcmMode.switchTo();
        }
#  endif
#  ifdef INCLUDE_COMET64
        else
        if((strstr((const char *)vbuf,"comet64")==(char *)vbuf)
        &&(SD.cardType() != CARD_NONE))
        {
            result = ZOK;
            comet64Mode.switchTo();
        }
#  endif
#endif
#  ifdef INCLUDE_CBMMODEM
        else
        if(strstr((const char *)vbuf,"1650")==(char *)vbuf)
        {
          altOpMode=OPMODE_1650;
          setAltOpModeAdjustments();
          result = ZOK;
        }
        else
        if(strstr((const char *)vbuf,"1660")==(char *)vbuf)
        {
          altOpMode=OPMODE_1660;
          setAltOpModeAdjustments();
          result = ZOK;
        }
        else
        if(strstr((const char *)vbuf,"1670")==(char *)vbuf)
        {
          altOpMode=OPMODE_1670;
          setAltOpModeAdjustments();
          result = ZOK;
        }
#  endif
#  ifdef INCLUDE_IRCC
        else
        if((strstr((const char *)vbuf,"irc")==(char *)vbuf))
        {
            result = ZOK;
            ircMode.switchTo();
        }
#  endif
#ifdef INCLUDE_SLIP
        else
        if((strstr((const char *)vbuf,"slip")==(char *)vbuf))
        {
            result = ZOK;
            slipMode.switchTo();
        }
#  endif
#  ifdef INCLUDE_PING
        else
        if((strstr((const char *)vbuf,"ping")==(char *)vbuf))
        {
          char *host = (char *)vbuf + 4;
          while((*host == '"' || *host==' ')&&(*host != 0))
            host++;
          while(strlen(host)>0)
          {
            char c = *(host + strlen(host)-1);
            if((c!='"') && (c!=' '))
              break;
            *(host + strlen(host)-1) = 0;
          }
          if(strlen(host)==0)
            result = ZERROR;
          else
            result = (ping(host) >= 0 )? ZOK : ZERROR;
        }
#  endif
        else
        if((strstr((const char *)vbuf,"print")==(char *)vbuf)||(strstr((const char *)vbuf,"PRINT")==(char *)vbuf))
            result = printMode.switchTo((char *)vbuf+5,vlen-5,serial.isPetsciiMode());
        else
          result=ZERROR; //todo: branch based on vbuf contents
        break;
      case '$':
      {
        int eqMark=0;
        for(int i=0;vbuf[i]!=0;i++)
          if(vbuf[i]=='=')
          {
            eqMark=i;
            break;
          }
          else
            vbuf[i]=lc(vbuf[i]);
        if(eqMark==0)
          result=ZERROR; // no EQ means no good
        else
        {
          vbuf[eqMark]=0;
          String var=(char *)vbuf;
          var.trim();
          String val=(char *)(vbuf+eqMark+1);
          val.trim();
          result = ((val.length()==0)&&((strcmp(var.c_str(),"pass")!=0))) ? ZERROR : ZOK;
          if(result == ZOK)
          {
            if(strcmp(var.c_str(),"ssid")==0)
              wifiSSI = val;
            else
            if(strcmp(var.c_str(),"pass")==0)
              wifiPW = val;
            else
            if(strcmp(var.c_str(),"mdns")==0)
              hostname = val;
            else
            if(strcmp(var.c_str(),"sb")==0)
              result = doBaudCommand(atoi(val.c_str()),(uint8_t *)val.c_str(),val.length());
            else
              result = ZERROR;
          }
        }
        break;
      }
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
              packetXOn = true; // left for x16
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
          {
            loadConfig();
            break;
          }
        case 'w':
          if(!reSaveConfig(3))
            result = ZERROR;
          break;
        case 'f':
          if(vval == 86)
          {
            loadConfig();
            zclock.reset();
            result = SPIFFS.format() ? ZOK : ZERROR;
            if(!reSaveConfig(10))
              result = ZERROR;
          }
          else
          {
            SPIFFS.remove(CONFIG_FILE);
            SPIFFS.remove(CONFIG_FILE_OLD);
            SPIFFS.remove("/zphonebook.txt");
            SPIFFS.remove("/zlisteners.txt");
            PhoneBookEntry::clearPhonebook();
            if(WiFi.status() == WL_CONNECTED)
              WiFi.disconnect();
            wifiSSI="";
            delay(500);
            zclock.reset();
            result=doResetCommand(true);
            if(altOpMode == OPMODE_NONE)
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
        case 'y':
          {
            if(isNumber && ((vval > 0)||(vbuf[0]=='0')))
            {
              machineState = stateMachine;
              machineQue = "";
              if(current != null)
              {
                current->machineState = current->stateMachine;
                current->machineQue = "";
              }
              while(vval > 0)
              {
                  vval--;
                  if((machineState != null)&&(machineState[0]!=0))
                    machineState += ZI_STATE_MACHINE_LEN;
                  if(current != null)
                  {
                    if((current->machineState != null)&&(current->machineState[0]!=0))
                      current->machineState += ZI_STATE_MACHINE_LEN;
                  }
              }
            }
            else
            if((vlen % ZI_STATE_MACHINE_LEN) != 0)
              result=ZERROR;
            else
            {
              bool ok = true;
              const char *HEX_DIGITS = "0123456789abcdefABCDEF";
              for(int i=0;ok && (i<vlen);i+=ZI_STATE_MACHINE_LEN)
              {
                if((strchr(HEX_DIGITS,vbuf[i]) == NULL)
                ||(strchr(HEX_DIGITS,vbuf[i+1]) == NULL)
                ||(strchr("epdqxr",lc(vbuf[i+2])) == NULL)
                ||(strchr(HEX_DIGITS,vbuf[i+5]) == NULL)
                ||(strchr(HEX_DIGITS,vbuf[i+6]) == NULL))
                  ok = false;
                else
                if((lc(vbuf[i+2])=='r')
                &&((strchr(HEX_DIGITS,vbuf[i+3]) == NULL)
                  ||(strchr(HEX_DIGITS,vbuf[i+4]) == NULL)))
                  ok=false;
                else
                if((strchr("epdqx-",lc(vbuf[i+3])) == NULL)
                ||(strchr("epdqx-",lc(vbuf[i+4])) == NULL))
                  ok=false;
              }
              if(ok)
              {
                char newStateMachine[vlen+1];
                newStateMachine[vlen]=0;
                if(vlen > 0)
                  memcpy(newStateMachine,vbuf,vlen);
                setCharArray(&tempStateMachine,newStateMachine);
                result=ZOK;
              }
              else
              {
                result=ZERROR;
              }
            }
          }
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
          logFile2Uart=false;
          if(vval == 0)
          {
            if(logFileOpen)
            {
              logFile.flush();
              logFile.close();
              logFileOpen = false;
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
            if(vval==88)
              logFile2Uart=true;
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
        case 's':
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
              if((snum == 0)&&((vbuf[0]!='0')||(eq != (char *)(vbuf+1))))
                result=ZERROR;
              else
              {
                eq++;
                switch(snum)
                {
                  case 40:
                    if(*eq == 0)
                      result=ZERROR;
                    else
                    {
                      hostname = eq;
                      if(WiFi.status()==WL_CONNECTED)
                          connectWifi(wifiSSI.c_str(),wifiPW.c_str(),staticIP,staticDNS,staticGW,staticSN);
                      result=ZOK;
                    }
                    break;
                  case 41:
                    if(*eq == 0)
                      result=ZERROR;
                    else
                    {
                      termType = eq;
                      result=ZOK;
                    }
                    break;
                  case 42:
                    if((*eq == 0)||(strlen(eq)>250))
                      result=ZERROR;
                    else
                    {
                      busyMsg = eq;
                      busyMsg.replace("\\n","\n");
                      busyMsg.replace("\\r","\r");
                      result=ZOK;
                    }
                    break;
                  default:
                    result=ZERROR;
                    break;
                }
              }
            }
          }
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
                    s_pinWrite(pinNum,sval);
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

    if(tempDelimiters != NULL)
    {
      setCharArray(&delimiters,tempDelimiters);
      freeCharArray(&tempDelimiters);
    }
    if(tempMaskOuts != NULL)
    {
      setCharArray(&maskOuts,tempMaskOuts);
      freeCharArray(&tempMaskOuts);
    }
    if(tempStateMachine != NULL)
    {
      setCharArray(&stateMachine,tempStateMachine);
      freeCharArray(&tempStateMachine);
      machineState = stateMachine;
    }
    if(result != ZIGNORE_SPECIAL)
      previousCommand = saveCommand;
    if((suppressResponses)&&(result == ZERROR))
      return ZERROR;
    if(crc8 >= 0)
      result=ZERROR; // setting S42 without a T command is now Bad.
    if((result != ZOK)||(index >= len))
      sendOfficialResponse(result);
    if(result == ZERROR) // on error, cut and run
      return ZERROR;
  }
  return result;
}

void ZCommand::sendOfficialResponse(ZResult res)
{
  if(!suppressResponses)
  {
    switch(res)
    {
    case ZOK:
      logPrintln("Response: OK");
      preEOLN(EOLN);
      if(numericResponses)
        serial.prints("0");
      else
        serial.prints("OK");
      serial.prints(EOLN);
      break;
    case ZERROR:
      logPrintln("Response: ERROR");
      preEOLN(EOLN);
      if(numericResponses)
        serial.prints("4");
      else
        serial.prints("ERROR");
      serial.prints(EOLN);
      break;
    case ZNOANSWER:
      logPrintln("Response: NOANSWER");
      preEOLN(EOLN);
      if(numericResponses)
        serial.prints("8");
      else
        serial.prints("NO ANSWER");
      serial.prints(EOLN);
      break;
    case ZCONNECT:
      logPrintln("Response: Connected!");
      sendConnectionNotice(((current == null)||(autoStreamMode)) ? baudRate : current->id);
      break;
    default:
      break;
    }
  }
}

void ZCommand::showInitMessage()
{
  serial.prints(commandMode.EOLN);
#ifdef ZIMODEM_ESP32
  int totalSPIFFSSize = SPIFFS.totalBytes();
#else
  FSInfo info;
  SPIFFS.info(info);
  int totalSPIFFSSize = info.totalBytes;
#endif
  serial.prints("Zimodem ");
#ifdef ZIMODEM_ESP32
  serial.prints("ESP32 ");
#else
  serial.prints("ESP8266 ");
#endif
  serial.prints("Firmware v");
  HWSerial.setTimeout(60000);
  serial.prints(ZIMODEM_VERSION);
  //serial.prints(" (");
  //serial.prints(compile_date);
  //serial.prints(")");
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
  sprintf(s,"tot=%dk heap=%dk fsize=%dk",(ESP.getFlashChipSize()/1024),(ESP.getFreeHeap()/1024),totalSPIFFSSize/1024);
#else
  sprintf(s,"tot=%dk heap=%dk fsize=%dk",(ESP.getFlashChipRealSize()/1024),(ESP.getSketchSize()/1024),totalSPIFFSSize/1024);
#endif
  
  serial.prints(s);
  serial.prints(commandMode.EOLN);
  if(wifiSSI.length()>0)
  {
    if(WiFi.status() == WL_CONNECTED)
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

uint8_t *ZCommand::doStateMachine(uint8_t *buf, uint16_t *bufLen, char **machineState, String *machineQue, char *stateMachine)
{
  if((stateMachine != NULL) && ((stateMachine)[0] != 0) && (*machineState != NULL) && ((*machineState)[0] != 0))
  {
    String newBuf = "";
    for(int i=0;i<*bufLen;)
    {
      char matchChar = FROMHEX((*machineState)[0],(*machineState)[1]);
      if((matchChar == 0)||(matchChar == buf[i]))
      {
        char c= buf[i++];
        short cmddex=1;
        do
        {
          cmddex++;
          switch(lc((*machineState)[cmddex]))
          {
            case '-': // do nothing
            case 'e': // do nothing
              break; 
            case 'p': // push to the que 
              if(machineQue->length() < 256)
                *machineQue += c;
              break;
            case 'd': // display this char
              newBuf += c;
              break;
            case 'x': // flush queue
              *machineQue = "";
              break;
            case 'q': // eat this char, but flush the queue
              if(machineQue->length()>0)
              {
                newBuf += *machineQue;
                *machineQue = "";
              }
              break;
            case 'r': // replace this char
              if(cmddex == 2)
              {
                char newChar = FROMHEX((*machineState)[cmddex+1],(*machineState)[cmddex+2]);
                newBuf += newChar;
              }
              break;
            default:
              break;
          }
        }
        while((cmddex<4) && (lc((*machineState)[cmddex])!='r'));
        char *newstate = stateMachine + (ZI_STATE_MACHINE_LEN * FROMHEX((*machineState)[5],(*machineState)[6]));
        char *test = stateMachine;
        while(test[0] != 0)
        {
          if(test == newstate)
          {
            (*machineState) = test;
            break;
          }
          test += ZI_STATE_MACHINE_LEN;
        }
      }
      else
      {
        *machineState += ZI_STATE_MACHINE_LEN;
        if((*machineState)[0] == 0)
        {
          *machineState = stateMachine;
          i++;
        }
      }
    }
    if((*bufLen != newBuf.length()) || (memcmp(buf,newBuf.c_str(),*bufLen)!=0))
    {
      if(newBuf.length() > 0)
      {
        if(newBuf.length() > *bufLen)
        {
          free(buf);
          buf = (uint8_t *)malloc(newBuf.length());
        }
        memcpy(buf,newBuf.c_str(),newBuf.length());
      }
      *bufLen = newBuf.length();
    }
  }
  return buf;
}

uint8_t *ZCommand::doMaskOuts(uint8_t *buf, uint16_t *bufLen, char *maskOuts)
{
  if(maskOuts[0] != 0)
  {
    uint16_t oldLen=*bufLen;
    for(int i=0,o=0;i<oldLen;i++,o++)
    {
      if(strchr(maskOuts,buf[i])!=null)
      {
        o--;
        (*bufLen)--;
      }
      else
        buf[o]=buf[i];
    }
  }
  return buf;
}

void ZCommand::packetOut(uint8_t id, uint8_t *buf, uint16_t bufLen, uint8_t num)
{
  uint8_t crc=CRC8(buf,bufLen);
  headerOut(id,num,bufLen,(int)crc);
  int bct=0;
  int i=0;
  while(i < bufLen)
  {
    uint8_t c=buf[i++];
    switch(binType)
    {
      case BTYPE_NORMAL:
      case BTYPE_NORMAL_NOCHK:
      case BTYPE_NORMAL_PLUS:
        serial.write(c);
        break;
      case BTYPE_HEX:
      case BTYPE_HEX_PLUS:
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
      case BTYPE_DEC_PLUS:
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

void ZCommand::reSendLastPacket(WiFiClientNode *conn, uint8_t which)
{
  if((conn == NULL)
  ||(which>2))
  {
    headerOut(conn->id,0,0,0);
  }
  else
  if(conn->blankPackets>=which)
  {
    headerOut(conn->id,conn->nextPacketNum-which,0,0);
  }
  else
  if(conn->lastPacket[which].len == 0) // never used, or empty
  {
    headerOut(conn->id,conn->lastPacket[which].num,0,0);
  }
  else
  {
    uint16_t bufLen = conn->lastPacket[which].len;
    uint8_t *cbuf = conn->lastPacket[which].buf;
    uint8_t num = conn->lastPacket[which].num;
    uint8_t *buf = (uint8_t *)malloc(bufLen);
    memcpy(buf,cbuf,bufLen);
    buf = doMaskOuts(buf,&bufLen,maskOuts);
    buf = doMaskOuts(buf,&bufLen,conn->maskOuts);
    buf = doStateMachine(buf,&bufLen,&machineState,&machineQue,stateMachine);
    buf = doStateMachine(buf,&bufLen,&(conn->machineState),&(conn->machineQue),conn->stateMachine);
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
    packetOut(conn->id,buf,bufLen,num);
  }
}

void ZCommand::clearPlusProgress()
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
          preEOLN(EOLN);
          if(numericResponses)
          {
            serial.prints("3");
            serial.prints(EOLN);
          }
          else
          if(current->isAnswered())
          {
            serial.prints("NO CARRIER");
            if(longResponses && (!autoStreamMode))
              serial.printf(" %d %s:%d",current->id,current->host,current->port);
            serial.prints(EOLN);
            serial.flush();
          }
        }
        delete current;
        current = conns;
        nextConn = conns;
      }
    }
    memset(nbuf,0,MAX_COMMAND_SIZE);
    eon=0;
    return true;
  }
  return false;
}

void ZCommand::sendNextPacket()
{
  if((serial.availableForWrite()<packetSize)
  ||(altOpMode != OPMODE_NONE))
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
    if((nextConn->available()>0)
    &&(nextConn->isAnswered())) // being unanswered is a server thing that must be respected
    //&& (nextConn->isConnected())) // being connected is not required to have buffered bytes waiting!
    {
      int availableBytes = nextConn->available();
      int maxBytes=packetSize;
      if(availableBytes<maxBytes)
        maxBytes=availableBytes;
      // how much we read should depend on how much we can IMMEDIATELY write
      // .. this is because resendLastPacket ensures everything goes out
      if(maxBytes > 0)
      {
        if((nextConn->delimiters[0] != 0) || (delimiters[0] != 0))
        {
          uint16_t lastLen = nextConn->lastPacket[0].len;
          uint8_t *lastBuf = nextConn->lastPacket[0].buf;
          
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
          nextConn->lastPacket[0].len = lastLen;
          if((lastLen >= packetSize)
          ||((lastLen>0)
            &&((strchr(nextConn->delimiters,lastBuf[lastLen-1]) != null)
              ||(strchr(delimiters,lastBuf[lastLen-1]) != null))))
            maxBytes = lastLen;
          else
          {
            if(serial.getFlowControlType() == FCT_MANUAL)
            {
              if(nextConn->blankPackets == 0)
                memcpy(&nextConn->lastPacket[2],&nextConn->lastPacket[1],sizeof(struct Packet));
              nextConn->blankPackets++;
              headerOut(nextConn->id,nextConn->nextPacketNum++,0,0);
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
          maxBytes = nextConn->read(nextConn->lastPacket[0].buf,maxBytes);
          logSocketIn(nextConn->lastPacket[0].buf,maxBytes);
        }
        nextConn->lastPacket[0].num=nextConn->nextPacketNum++;
        nextConn->lastPacket[0].len=maxBytes;
        lastPacketId=nextConn->id;
        if(nextConn->blankPackets>0)
        {
          nextConn->lastPacket[2].num=nextConn->nextPacketNum-1;
          nextConn->lastPacket[2].len=0;
        }
        else
          memcpy(&nextConn->lastPacket[2],&nextConn->lastPacket[1],sizeof(struct Packet));
        memcpy(&nextConn->lastPacket[1],&nextConn->lastPacket[0],sizeof(struct Packet));
        nextConn->blankPackets=0;
        reSendLastPacket(nextConn,1);
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
            preEOLN(EOLN);
            serial.prints("3");
            serial.prints(EOLN);
          }
          else
          if(nextConn->isAnswered())
          {
            preEOLN(EOLN);
            serial.prints("NO CARRIER");
            if(longResponses && (!autoStreamMode))
            {
              serial.prints(" ");
              serial.printi(nextConn->id);
            }
            serial.prints(EOLN);
            serial.flush();
          }
          if(serial.getFlowControlType() == FCT_MANUAL)
          {
            checkOpenConnections();
            return; // don't handle more than one socket!
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
    if(firstConn != NULL)
      headerOut(firstConn->id,firstConn->nextPacketNum++,0,0);
    else
      headerOut(0,0,0,0);
    while(firstConn != NULL)
    {
      firstConn->lastPacket[0].len = 0;
      if(firstConn->blankPackets == 0)
        memcpy(&firstConn->lastPacket[2],&firstConn->lastPacket[1],sizeof(struct Packet));
      firstConn->blankPackets++;
      firstConn = firstConn->next;
    }
  }
}

void ZCommand::sendConnectionNotice(int id)
{
  if((altOpMode == OPMODE_1650)||(altOpMode == OPMODE_1660))
    return;
  preEOLN(EOLN);
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

bool ZCommand::acceptNewConnection()
{
  WiFiServerNode *serv = servs;
  while(serv != null)
  {
    if(serv->hasClient())
    {
      WiFiClient newClient = serv->server->available();
      if(newClient.connected())
      {
        if(busyMode)
        {
            newClient.write((uint8_t *)busyMsg.c_str(), busyMsg.length());
            newClient.flush();
            delay(100); // yes, i know, but seriously....
            newClient.stop();
            serv=serv->next;
            continue;
        }
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
          WiFiClientNode *newClientNode = new WiFiClientNode(newClient, serv->flagsBitmap, futureRings>0?(futureRings+1) * 2:0);
          setCharArray(&(newClientNode->delimiters),serv->delimiters);
          setCharArray(&(newClientNode->maskOuts),serv->maskOuts);
          setCharArray(&(newClientNode->stateMachine),serv->stateMachine);
          newClientNode->machineState = newClientNode->stateMachine;
          s_pinWrite(pinRI,riActive);
          if(futureRings>0)
          {
            preEOLN(EOLN);
            serial.prints(numericResponses?"2":"RING");
            serial.prints(EOLN);
          }
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
  unsigned long now=millis();
  while(conn != null)
  {
    WiFiClientNode *nextConn = conn->next;
    if((!conn->isAnswered())&&(conn->isConnected()))
    {
      if(now > conn->nextRingTime(0))
      {
        conn->nextRingTime(3000);
        int rings=conn->ringsRemaining(-1);
        if(rings <= 1)
        {
          s_pinWrite(pinRI,riInactive);
          if(ringCounter > 0)
          {
            preEOLN(EOLN);
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
          s_pinWrite(pinRI,riActive);
          preEOLN(EOLN);
          serial.prints(numericResponses?"2":"RING");
          serial.prints(EOLN);
        }
        else
          s_pinWrite(pinRI,riInactive);
      }
    }
    conn = nextConn;
  }
  return currMode != this;
}

void ZCommand::checkPulseDial()
{
#ifdef INCLUDE_CBMMODEM
  if(pinSupport[pinOTH])
  {
    /*
     * One 'pulse' is:
     * 1->0 over 50 ms
     * 0->1 over 50ms
     * (repeat)
     * if 1->0 lasts 300-320ms, it is between numbers
     */
    int bit = digitalRead(pinOTH);
    if(bit != lastPulseState)
    {
      if(lastPulseTimeMs == 0)
          lastPulseTimeMs = millis();
      else
      if(bit == othActive)
      {
        unsigned short diff = (millis()-lastPulseTimeMs);
        if((diff > 15) && (diff < 60))
        {
          if(pulseWork < 10)
            pulseWork++;
          else
          {
              logPrintf("\n\rP.D.: ERROR -- OVERPULSE!\n\r");
              pulseBuf = ""; // error out
              pulseWork = 0;
              return;
          }
        }
        else
        {
          logPrintf("\n\rP.D.: ERROR (%u ms)!\n\r",(unsigned int)diff);
          pulseBuf = ""; // error out
          lastPulseTimeMs = 0;
          pulseWork = 0;
          return;
        }
      }
      else
      if(bit == othInactive)
      {
        unsigned short diff = (millis()-lastPulseTimeMs);
        if((diff > 225)
        && (diff < 350)
        && (pulseWork > 0))
        {
            char nums[2];
            if(pulseWork > 9)
              sprintf(nums,"0");
            else
              sprintf(nums,"%u",pulseWork);
            debugPrintf("\n\rP.D.: got digit: %u\n\r",(unsigned int)pulseWork);
            pulseBuf += nums;
            pulseWork=0;
        }
        else
        if((diff > 15)
        && (diff < 60))
        {} // between bits
        else
          logPrintf("\n\rP.D.: Ignoring FAIL (%u)\n\r",(unsigned int)diff);
        //else if this happens too quickly, do nothing
      }
      lastPulseState = bit;
      lastPulseTimeMs = millis();
    }
    else
    if((lastPulseTimeMs != 0)
    &&((millis() - lastPulseTimeMs) > 350))
    {
      if(pulseWork > 0)
      {
        char nums[2];
        if(pulseWork > 9)
          sprintf(nums,"0");
        else
          sprintf(nums,"%u",pulseWork);
        debugPrintf("\n\rP.D.: got digit: %u\n\r",(unsigned int)pulseWork);
        pulseBuf += nums;
        pulseWork = 0;
        lastPulseTimeMs = millis();
        if(pulseBuf.length() > 2) //2 digits is minimum to prevent false dials
        {
          unsigned long vval = atoi(pulseBuf.c_str());
          PhoneBookEntry *pb=PhoneBookEntry::findPhonebookEntry(vval);
          if(pb != null)
          {
            logPrintf("\n\rP.D.: Dialing: %lu\n\r",vval);
            doDialStreamCommand(vval, (uint8_t *)pulseBuf.c_str(), pulseBuf.length(), true, "");
            pulseBuf = "";
            lastPulseTimeMs = 0;
          }
        }
      }
      else
      {
        pulseBuf = "";
        lastPulseTimeMs = 0;
      }
    }
  }
#endif
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

void ZCommand::loop()
{
  checkPlusEscape();
  if(acceptNewConnection())
  {
      checkBaudChange();
      return;
  }
  if(serial.isSerialOut())
  {
    sendNextPacket();
    serialOutDeque();
  }
#ifdef INCLUDE_CBMMODEM
  checkPulseDial();
#endif
  checkBaudChange();
  logFileLoop();
}
