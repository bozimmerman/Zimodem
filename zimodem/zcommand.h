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

const int MAX_COMMAND_SIZE=256;

enum ZResult
{
  ZOK,
  ZERROR,
  ZCONNECT,
  ZNOCARRIER,
  ZIGNORE,
  ZIGNORE_SPECIAL
};

enum ConfigOptions
{
  CFG_WIFISSI=0,
  CFG_WIFIPW,
  CFG_BAUDRATE,
  CFG_EOLN,
  CFG_FLOWCONTROL,
  CFG_ECHO,
  CFG_RESP_SUPP,
  CFG_RESP_NUM,
  CFG_RESP_LONG,
};

enum FlowControlType
{
  FCT_NORMAL,
  FCT_AUTOOFF,
  FCT_MANUAL
};

class ZCommand : public ZMode
{
  friend class WiFiClientNode;

  private:
    char *CRLF="\r\n";
    char *LFCR="\r\n";
    char *LF="\n";
    char *CR="\r";
    char BS=8;
    char ringCounter = 1;
    
    uint8_t nbuf[MAX_COMMAND_SIZE];
    int eon=0;
    int lastServerClientId = 0;
    WiFiClientNode *current = null;
    bool XON=true;
    bool autoStreamMode=false;
    FlowControlType flowControlType=FCT_NORMAL;
    unsigned long lastNonPlusTimeMs = 0;
    unsigned long currentExpiresTimeMs = 0;
    String previousCommand = "";
    String currentCommand = "";
    WiFiClientNode *nextConn=null;

    byte CRC8(const byte *data, byte len);

    bool readSerialStream();
    ZResult doSerialCommand();
    void setConfigDefaults();
    void parseConfigOptions(String configArguments[]);
    void setBaseConfigOptions(String configArguments[]);
    void reSaveConfig();
    void reSendLastPacket(WiFiClientNode *conn);
    void acceptNewConnection();
    void sendNextPacket();

    ZResult doResetCommand();
    ZResult doNoListenCommand();
    ZResult doBaudCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doTransmitCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doLastPacket(int vval, uint8_t *vbuf, int vlen, bool isNumber);
    ZResult doConnectCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers);
    ZResult doWiFiCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doDialStreamCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers);
    ZResult doAnswerCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers);
    ZResult doHangupCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber);
    ZResult doEOLNCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber);

  public:
    int packetSize = 127;
    bool suppressResponses;
    bool numericResponses;
    bool longResponses;
    boolean doEcho;
    bool doFlowControl;
    String EOLN;
    char EC='+';
    char *ECS="+++";

    void loadConfig();
    void serialIncoming();
    void loop();
};

