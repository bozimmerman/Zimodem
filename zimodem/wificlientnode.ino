static int WiFiNextClientId = 0;

void WiFiClientNode::finishConnectionLink()
{
  wasConnected=true;
  if(conns == null)
    conns = this;
  else
  {
    WiFiClientNode *last = conns;
    while(last->next != null)
      last = last->next;
    last->next = this;
  }
}

WiFiClientNode::WiFiClientNode(char *hostIp, int newport)
{
  port=newport;
  host=new char[strlen(hostIp)+1];
  strcpy(host,hostIp);
  id=++WiFiNextClientId;
  client = new WiFiClient();
  if(!client->connect(hostIp, port))
  {
    // deleted when it returns and is deleted
  }
  else
  {
    finishConnectionLink();
  }
}
    
WiFiClientNode::WiFiClientNode(WiFiClient *newClient)
{
  port=newClient->localPort();
  String remoteIP = newClient->remoteIP().toString();
  host=new char[remoteIP.length()+1];
  strcpy(host,remoteIP.c_str());
  id=++WiFiNextClientId;
  client = newClient;
  finishConnectionLink();
}
    
WiFiClientNode::~WiFiClientNode()
{
    if(client != null)
    {
      client->stop();
      delete client;
    }
    delete host;
    if(conns == null)
    {
    }
    else
    if(conns == this)
      conns = next;
    else
    {
      WiFiClientNode *last = conns;
      while(last->next != this)
        last = last->next;
      if(last != null)
        last->next = next;
    }
}

bool WiFiClientNode::isConnected()
{
  return (client != null) && (client->connected());
}

