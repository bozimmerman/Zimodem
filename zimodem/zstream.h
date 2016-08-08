class ZStream : public ZMode
{
  private:
    WiFiClientNode *current = null;
    unsigned long lastNonPlusTimeMs = 0;
    unsigned long currentExpiresTimeMs = 0;
    int plussesInARow=0;
    bool disconnectOnExit=true;
    bool petscii=false;
    bool telnet=false;
    bool XON=true;
  
  public:
    void reset(WiFiClientNode *conn, bool dodisconnect, bool doPETSCII, bool doTelnet);
    
    void serialIncoming();
    void loop();
};

