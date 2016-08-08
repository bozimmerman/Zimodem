static int WiFiNextClientId = 0;

class WiFiServerNode
{
  public:
    int port;
    int id;
    WiFiServer *server;
    WiFiServerNode *next = null;
    
    WiFiServerNode(int port);
    ~WiFiServerNode();
};

