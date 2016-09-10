/*
   Copyright 2016-2016 Bo Zimmerman

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

void WiFiClientNode::finishConnectionLink()
{
  client.setNoDelay(true);
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

WiFiClientNode::WiFiClientNode(char *hostIp, int newport, bool PETSCII)
{
  port=newport;
  host=new char[strlen(hostIp)+1];
  strcpy(host,hostIp);
  id=++WiFiNextClientId;
  doPETSCII=PETSCII;
  WiFiClient *newC = new WiFiClient();
  client = *newC;
  if(!client.connect(hostIp, port))
  {
    // deleted when it returns and is deleted
  }
  else
  {
    finishConnectionLink();
  }
}
    
WiFiClientNode::WiFiClientNode(WiFiClient newClient)
{
  port=newClient.localPort();
  String remoteIPStr = newClient.remoteIP().toString();
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
  client.stop();
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
  if(commandMode.nextConn == this)
    commandMode.nextConn = conns;
  next=null;
}

bool WiFiClientNode::isConnected()
{
  return client.connected();
}

size_t WiFiClientNode::write(uint8_t c)
{
  return client.write(c);
}

int WiFiClientNode::read()
{
  return client.read();
}

int WiFiClientNode::peek()
{
  return client.peek();
}

void WiFiClientNode::flush()
{
  return client.flush();
}

int WiFiClientNode::available()
{
  return client.available();
}

int WiFiClientNode::read(uint8_t *buf, size_t size)
{
  return client.read(buf,size);
}

size_t WiFiClientNode::write(const uint8_t *buf, size_t size)
{
  return client.write(buf,size);
}



