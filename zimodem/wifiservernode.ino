/*
   Copyright 2016-2017 Bo Zimmerman

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

WiFiServerNode::WiFiServerNode(int newport, int flagsBitmap)
{
  id=++WiFiNextClientId;
  port=newport;
  setCharArray(&delimiters,"");
  setCharArray(&maskOuts,"");
  this->flagsBitmap = flagsBitmap;
  server = new WiFiServer(newport);
  //BZ:server->setNoDelay(DEFAULT_NO_DELAY);
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
  if(servs == this)
    servs = next;
  else
  {
    WiFiServerNode *last = servs;
    while((last != null) && (last->next != this)) // don't change this!
      last = last->next;
    if(last != null)
      last->next = next;
  }
  freeCharArray(&delimiters);
  freeCharArray(&maskOuts);
}

bool WiFiServerNode::hasClient()
{
  if(server != null)
    return server->hasClient();
  return false;
}

bool WiFiServerNode::ReadWiFiServer(File &f, WiFiServerNode &node)
{
  if(f.available()>0)
  {
    String str="";
    char c=f.read();
    while((c != ',') && (f.available()>0))
    {
      str += c;
      c=f.read();
    }
    if(str.length()==0)
      return false;
    node.port = atoi(str.c_str());
    str = "";
    c=f.read();
    while((c != '\n') && (f.available()>0))
    {
      str += c;
      c=f.read();
    }
    if(str.length()==0)
      return false;
    node.flagsBitmap = atoi(str.c_str());
    str = "";
    c=f.read();
    while((c != ',') && (f.available()>0))
    {
      str += c;
      c=f.read();
    }
    if(str.length()==0)
      return false;
    int chars=atoi(str.c_str());
    str = "";
    for(int i=0;i<chars && f.available()>0;i++)
      str += (char)f.read();
    setCharArray(&node.maskOuts,str.c_str());
    if(f.available()<=0 || f.read()!='\n')
      return false;
    str = "";
    c=f.read();
    while((c != ',') && (f.available()>0))
    {
      str += c;
      c=f.read();
    }
    if(str.length()==0)
      return false;
    chars=atoi(str.c_str());
    str = "";
    for(int i=0;i<chars && f.available()>0;i++)
      str += (char)f.read();
    setCharArray(&node.delimiters,str.c_str());
    if(f.available()<=0 || f.read()!='\n')
      return false;
  }
  return true;
}

void WiFiServerNode::SaveWiFiServers()
{
  SPIFFS.remove("/zlisteners.txt");
  delay(500);
  File f = SPIFFS.open("/zlisteners.txt", "w");
  int ct=0;
  WiFiServerNode *serv = servs;
  while(serv != null)
  {
    f.printf("%d,%d\n",serv->port,serv->flagsBitmap);
    if(serv->maskOuts == NULL)
      f.printf("0,\n");
    else
      f.printf("%d,%s\n",strlen(serv->maskOuts),serv->maskOuts);
    if(serv->delimiters == NULL)
      f.printf("0,\n");
    else
      f.printf("%d,%s\n",strlen(serv->delimiters),serv->delimiters);
    ct++;
    serv=serv->next;
  }
  f.close();
  delay(500);
  if(SPIFFS.exists("/zlisteners.txt"))
  {
    File f = SPIFFS.open("/zlisteners.txt", "r");
    bool fail=false;
    WiFiServerNode *snode = (WiFiServerNode *)malloc(sizeof(WiFiServerNode));
    memset(snode,0,sizeof(WiFiServerNode));
    while(f.available()>0)
    {
      freeCharArray(&(snode->delimiters));
      freeCharArray(&(snode->maskOuts));
      memset(snode,0,sizeof(WiFiServerNode));
      if(!ReadWiFiServer(f,*snode))
      {
        fail=true;
        break;
      }
    }
    freeCharArray(&(snode->delimiters));
    freeCharArray(&(snode->maskOuts));
    free(snode);
    f.close();
    if(fail)
    {
      delay(100);
      SaveWiFiServers();
    }
  }
}

void WiFiServerNode::RestoreWiFiServers()
{
  if(SPIFFS.exists("/zlisteners.txt"))
  {
    File f = SPIFFS.open("/zlisteners.txt", "r");
    bool fail=false;
    WiFiServerNode *snode = (WiFiServerNode *)malloc(sizeof(WiFiServerNode));
    memset(snode,0,sizeof(WiFiServerNode));
    while(f.available()>0)
    {
      freeCharArray(&(snode->delimiters));
      freeCharArray(&(snode->maskOuts));
      memset(snode,0,sizeof(WiFiServerNode));
      if(!ReadWiFiServer(f,*snode))
      {
        debugPrintf("Server: FAIL\n");
        fail=true;
        break;
      }
      WiFiServerNode *s=servs;
      while(s != null)
      {
        if(s->port == snode->port)
          break;
        s=s->next;
      }
      if(s==null)
      {
        WiFiServerNode *node = new WiFiServerNode(snode->port, snode->flagsBitmap);
        setCharArray(&node->delimiters,snode->delimiters);
        setCharArray(&node->maskOuts,snode->maskOuts);
        debugPrintf("Server: %d, %d: '%s' '%s'\n",node->port,node->flagsBitmap,node->delimiters,node->maskOuts);
      }
      else
        debugPrintf("Server: DUP %d\n",snode->port);
    }
    freeCharArray(&(snode->delimiters));
    freeCharArray(&(snode->maskOuts));
    free(snode);
    f.close();
  }
}


