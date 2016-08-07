const int MAX_COMMAND_SIZE=256;

enum ZResult
{
  ZOK,
  ZERROR,
  ZIGNORE
};

class ZCommand : public ZMode
{
  private:
    uint8_t nbuf[MAX_COMMAND_SIZE];
    int eon=0;
    WiFiClientNode *current = null;
    int XON=99;
    unsigned long currentExpires = 0;
 
    void reset();
    byte CRC8(const byte *data, byte len);
    ZResult doBaudCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doTransmitCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doConnectCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber);
    ZResult doWiFiCommand(int vval, uint8_t *vbuf, int vlen);
    ZResult doDialStreamCommand(int vval, uint8_t *vbuf, int vlen, bool isNumber, const char *dmodifiers);
  
  public:
    boolean echoOn=false;
    String wifiSSI;
    String wifiPW;
    int baudRate=115200;
  
    ZCommand() : ZMode()
    {
      reset();
    }
    void serialIncoming();
    void loop();
};

