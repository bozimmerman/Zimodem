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
  checkOpenConnections();
}

WiFiClientNode::WiFiClientNode(char *hostIp, int newport, int flagsBitmap)
{
  port=newport;
  host=new char[strlen(hostIp)+1];
  strcpy(host,hostIp);
  id=++WiFiNextClientId;
  this->flagsBitmap = flagsBitmap;
  clientPtr = createWiFiClient((flagsBitmap&FLAG_SECURE)==FLAG_SECURE);
  client = *clientPtr;
  client.setNoDelay(DEFAULT_NO_DELAY);
  setCharArray(&delimiters,"");
  setCharArray(&maskOuts,"");
  if(!client.connect(hostIp, port))
  {
    // deleted when it returns and is deleted
  }
  else
  {
    finishConnectionLink();
  }
}
    
WiFiClientNode::WiFiClientNode(WiFiClient newClient, int flagsBitmap)
{
  this->flagsBitmap = flagsBitmap;
  clientPtr=null;
  newClient.setNoDelay(DEFAULT_NO_DELAY);
  port=newClient.localPort();
  setCharArray(&delimiters,"");
  setCharArray(&maskOuts,"");
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
  lastPacketLen=0;
  if(host!=null)
  {
    client.stop();
    delete host;
    host=null;
  }
  if(clientPtr != null)
  {
    delete clientPtr;
    clientPtr = null;
  }
  if(conns == this)
    conns = next;
  else
  {
    WiFiClientNode *last = conns;
    while((last != null) && (last->next != this)) // don't change this!
      last = last->next;
    if(last != null)
      last->next = next;
  }
  if(commandMode.current == this)
    commandMode.current = conns;
  if(commandMode.nextConn == this)
    commandMode.nextConn = conns;
  freeCharArray(&delimiters);
  freeCharArray(&maskOuts);
  next=null;
  checkOpenConnections();
}

bool WiFiClientNode::isConnected()
{
  return (host != null) && client.connected();
}

bool WiFiClientNode::isPETSCII()
{
  return (flagsBitmap & FLAG_PETSCII) == FLAG_PETSCII;
}

bool WiFiClientNode::isEcho()
{
  return (flagsBitmap & FLAG_ECHO) == FLAG_ECHO;
}

FlowControlType WiFiClientNode::getFlowControl()
{
  if((flagsBitmap & FLAG_RTSCTS) == FLAG_RTSCTS)
    return FCT_RTSCTS;
  if((flagsBitmap & FLAG_XONXOFF) == FLAG_XONXOFF)
    return FCT_NORMAL;
  return FCT_DISABLED;
}

bool WiFiClientNode::isTelnet()
{
  return (flagsBitmap & FLAG_TELNET) == FLAG_TELNET;
}

bool WiFiClientNode::isDisconnectedOnStreamExit()
{
  return (flagsBitmap & FLAG_DISCONNECT_ON_EXIT) == FLAG_DISCONNECT_ON_EXIT;
}

void WiFiClientNode::setDisconnectOnStreamExit(bool tf)
{
  if(tf)
    flagsBitmap = flagsBitmap | FLAG_DISCONNECT_ON_EXIT;
  else
    flagsBitmap = flagsBitmap & ~FLAG_DISCONNECT_ON_EXIT;
}


int WiFiClientNode::read()
{
  if(host == null)
    return 0;
  return client.read();
}

int WiFiClientNode::peek()
{
  if(host == null)
    return 0;
  return client.peek();
}

void WiFiClientNode::flush()
{
  if((host != null)&&(client.available()==0))
  {
    flushOverflowBuffer();
    client.flush();
  }
}

int WiFiClientNode::available()
{
  if(host == null)
    return 0;
  return client.available();
}

int WiFiClientNode::read(uint8_t *buf, size_t size)
{
  if(host == null)
    return 0;
  return client.read(buf,size);
}

int WiFiClientNode::flushOverflowBuffer()
{
  if(overflowBufLen > 0)
  {
    // because overflowBuf is not a const char* for some reason
    // we need to explicitly declare that we want one
    // The simplest thing to do is pin down the first character of the
    // array and call it a day.
    // This avoids client.write<T>(buffer, length) from being seen by the
    // compiler as a better way to poke at it. 
    const uint8_t* overflowbuf_ptr = &overflowBuf[0];
    int bufWrite=client.write(overflowbuf_ptr,overflowBufLen);
    if(bufWrite >= overflowBufLen)
    {
      overflowBufLen = 0;
      if(pinSupport[pinRTS])
        digitalWrite(pinRTS,rtsActive);
      // fall-through
    }
    else
    {
      if(bufWrite > 0)
      {
        for(int i=bufWrite;i<overflowBufLen;i++)
          overflowBuf[i-bufWrite]=overflowBuf[i];
        overflowBufLen -= bufWrite;
      }
      if(pinSupport[pinRTS])
        digitalWrite(pinRTS,rtsInactive);
      return bufWrite;
    }
  }
  return 0;
}

size_t WiFiClientNode::write(const uint8_t *buf, size_t size)
{
  if(host == null)
  {
    if(overflowBufLen>0)
    {
      if(pinSupport[pinRTS])
        digitalWrite(pinRTS,rtsActive);
    }
    overflowBufLen=0;
    return 0;
  }
  int written = 0;
  written += flushOverflowBuffer();
  if(written > 0)
  {
    for(int i=0;i<size && overflowBufLen<OVERFLOW_BUF_SIZE;i++,overflowBufLen++)
      overflowBuf[overflowBufLen]=buf[i];
    return written;
  }
  written += client.write(buf, size);
  if(written < size)
  {
    for(int i=written;i<size && overflowBufLen<OVERFLOW_BUF_SIZE;i++,overflowBufLen++)
      overflowBuf[overflowBufLen]=buf[i];
    if(pinSupport[pinRTS])
      digitalWrite(pinRTS,rtsInactive);
  }
  return written;
}

size_t WiFiClientNode::write(uint8_t c)
{
  const uint8_t one[] = {c};
  write(one,1);
}

int WiFiClientNode::getNumOpenWiFiConnections()
{
  int num = 0;
  WiFiClientNode *conn = conns;
  while(conn != null)
  {
    if(conn->isConnected())
      num++;
    conn = conn->next;
  }
  return num;
}

