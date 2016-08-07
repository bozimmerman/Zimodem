class WiFiServerNode
{
  public:
    int port;
    WiFiServer *server;
    WiFiServerNode *next = null;
    
    WiFiServerNode(int port);
    ~WiFiServerNode();
};

