
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
  String remoteIPStr = newClient->remoteIP().toString();
  const char *remoteIP=remoteIPStr.c_str();
  host=new char[remoteIPStr.length()+1];
  strcpy(host,remoteIP);
  id=++WiFiNextClientId;
  client = newClient;
  finishConnectionLink();
  serverClient=true;
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
  if(commandMode.current == this)
    commandMode.current = conns;
}

bool WiFiClientNode::isConnected()
{
  return (client != null) && (client->connected());
}

