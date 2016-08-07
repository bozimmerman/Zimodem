class ZStream : public ZMode
{
  private:
    WiFiClientNode *current = null;
    unsigned long currentExpires = 0;
    bool disconnectOnExit=true;
    int plussesInARow=0;
    bool echo=false;
    bool petscii=false;
    bool telnet=false;
  
  public:
    void reset(WiFiClientNode *conn, bool dodisconnect, bool doPETSCII, bool doTelnet, bool doecho);
    
    void serialIncoming();
    void loop();
};

