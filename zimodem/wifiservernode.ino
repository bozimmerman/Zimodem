WiFiServerNode::WiFiServerNode(int newport)
{
  id=++WiFiNextClientId;
  port=newport;
  server = new WiFiServer(newport);
  server->setNoDelay(true);
  server->begin();
  if(servs==null)
    servs=this;
  else
  {
    WiFiServerNode *s=servs;
    while(s->next != null)
      s=s->next;
    s->next=this;
  }
}

WiFiServerNode::~WiFiServerNode()
{
    if(server != null)
    {
      server->stop();
      server->close();
      delete server;
    }
    if(servs == null)
    {
    }
    else
    if(servs == this)
      servs = next;
    else
    {
      WiFiServerNode *last = servs;
      while(last->next != this)
        last = last->next;
      if(last != null)
        last->next = next;
    }
}

