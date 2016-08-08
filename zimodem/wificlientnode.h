class WiFiClientNode
{
  private:
    void finishConnectionLink();

  public:
    int id=0;
    char *host;
    int port;
    bool wasConnected=false;
    bool serverClient=false;
    WiFiClient *client;
    WiFiClientNode *next = null;
    
    WiFiClientNode(char *hostIp, int newport);
    WiFiClientNode(WiFiClient *newClient);
    ~WiFiClientNode();
    bool isConnected();
};

