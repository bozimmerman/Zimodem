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

class ZCommand : public ZMode
{
  private:
    uint8_t nbuf[MAX_COMMAND_SIZE];
    int eon=0;
    WiFiClientNode *current = null;
    bool XON=true;
    bool flowControl=false;
    WiFiClientNode *nextConn=null;
    unsigned long lastNonPlusTimeMs = 0;
    unsigned long currentExpiresTimeMs = 0;
    String previousCommand = "";
    String currentCommand = "";
 
    byte CRC8(const byte *data, byte len);

    bool readSerialStream();
    ZResult doSerialCommand();
    void reSaveConfig();
    
    ZResult doResetCommand(bool quiet);
    ZResult doBaudCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doTransmitCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doConnectCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber);
    ZResult doWiFiCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doDialStreamCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers);
    ZResult doAnswerCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers);
    ZResult doHangupCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber);
    ZResult doEOLNCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber);
  
  public:
    bool suppressResponses=false;
    bool numericResponses=false;
    bool longResponses=true;
    boolean echoOn=false;
    String EOLN = "\r\n";
    String wifiSSI;
    String wifiPW;
    int baudRate=115200;
  
    ZCommand() : ZMode()
    {
      doResetCommand(true);
    }
    void loadConfig();
    void serialIncoming();
    void loop();
};

