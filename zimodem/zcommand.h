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

class ZCommand : public ZMode
{
  friend class WiFiClientNode;
  
  private:
    uint8_t nbuf[MAX_COMMAND_SIZE];
    int eon=0;
    WiFiClientNode *current = null;
    bool XON=true;
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
    
    ZResult doResetCommand();
    ZResult doBaudCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doTransmitCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doConnectCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber);
    ZResult doWiFiCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doDialStreamCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers);
    ZResult doAnswerCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers);
    ZResult doHangupCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber);
    ZResult doEOLNCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber);
  
  public:
    bool suppressResponses;
    bool numericResponses;
    bool longResponses;
    boolean doEcho;
    bool doFlowControl;
    String EOLN;
  
    void loadConfig();
    void serialIncoming();
    void loop();
};

