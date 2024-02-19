/*
   Copyright 2016-2024 Bo Zimmerman

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
  if(host != 0)
    debugPrintf("\r\nConnected to %s:%d\r\n",host,port);
}

void WiFiClientNode::constructNode()
{
  id=++WiFiNextClientId;
  setCharArray(&delimiters,"");
  setCharArray(&maskOuts,"");
  setCharArray(&stateMachine,"");
  machineState = stateMachine;
}

void WiFiClientNode::constructNode(char *hostIp, int newport, int flagsBitmap, int ringDelay)
{
  constructNode();
  port=newport;
  host=new char[strlen(hostIp)+1];
  strcpy(host,hostIp);
  this->flagsBitmap = flagsBitmap;
  answered=(ringDelay == 0);
  if(ringDelay > 0)
  {
    ringsRemain = ringDelay;
    nextRingMillis = millis() + 3000;
  }
}

void WiFiClientNode::constructNode(char *hostIp, int newport, char *username, char *password, int flagsBitmap, int ringDelay)
{
  constructNode(hostIp, newport, flagsBitmap, ringDelay);
# ifdef INCLUDE_SSH
  if(((flagsBitmap&FLAG_SECURE)==FLAG_SECURE)
  && (username != 0))
  {
    WiFiSSHClient *c = new WiFiSSHClient();
    c->setLogin(username, password);
    clientPtr = c;
  }
  else
#endif
    clientPtr = createWiFiClient((flagsBitmap&FLAG_SECURE)==FLAG_SECURE);
  client = *clientPtr;
  if(!clientPtr->connect(hostIp, newport))
  {
    debugPrintf("\r\nFailed to Connected to %s:%d\r\n",hostIp,newport);
    // deleted when it returns and is deleted
  }
  else
  {
    clientPtr->setNoDelay(DEFAULT_NO_DELAY);
    finishConnectionLink();
  }
}

WiFiClientNode::WiFiClientNode(char *hostIp, int newport, char *username, char *password, int flagsBitmap)
{
  constructNode(hostIp, newport, username, password, flagsBitmap, 0);
}

WiFiClientNode::WiFiClientNode(char *hostIp, int newport, int flagsBitmap)
{
  constructNode(hostIp, newport, 0, 0, flagsBitmap, 0);
}

void WiFiClientNode:: setNoDelay(bool tf)
{
  if(clientPtr != 0)
    clientPtr->setNoDelay(tf);
  else
    client.setNoDelay(tf);
}

WiFiClientNode::WiFiClientNode(WiFiClient newClient, int flagsBitmap, int ringDelay)
{
  constructNode();
  this->flagsBitmap = flagsBitmap;
  clientPtr=null; // why is this so important?!
  newClient.setNoDelay(DEFAULT_NO_DELAY);
  port=newClient.localPort();
  String remoteIPStr = newClient.remoteIP().toString();
  const char *remoteIP=remoteIPStr.c_str();
  host=new char[remoteIPStr.length()+1];
  strcpy(host,remoteIP);
  client = newClient;
  answered=(ringDelay == 0);
  if(ringDelay > 0)
  {
    ringsRemain = ringDelay;
    nextRingMillis = millis() + 3000;
  }
  finishConnectionLink();
  serverClient=true;
}
    
WiFiClientNode::~WiFiClientNode()
{
  lastPacket[0].len=0;
  lastPacket[1].len=0;
  if(host!=null)
  {
    debugPrintf("\r\nDisconnected from %s:%d\r\n",host,port);
    if(clientPtr != null)
      clientPtr->stop();
    else
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
  //underflowBuf.len = 0;
  freeCharArray(&delimiters);
  freeCharArray(&maskOuts);
  freeCharArray(&stateMachine);
  machineState = NULL;
  next=null;
  checkOpenConnections();
}

bool WiFiClientNode::isConnected()
{
  if(host != null)
  {
    if(clientPtr != null)
      return clientPtr->connected();
    else
      return client.connected();
  }
  return false;
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

void WiFiClientNode::fillUnderflowBuf()
{
  /*
  //underflow buf is deprecated
  int newAvail = client.available();
  if(newAvail > 0)
  {
    int maxBufAvail = PACKET_BUF_SIZE-underflowBuf.len;
    if(newAvail>maxBufAvail)
      newAvail=maxBufAvail;
    if(newAvail > 0)
      underflowBuf.len += client.read(underflowBuf.buf+underflowBuf.len, newAvail);
  }
  */
}

int WiFiClientNode::read()
{
  if((host == null)||(!answered))
    return 0;
  /*
  // underflow buf is no longer needed
  if(underflowBuf.len > 0)
  {
    int b = underflowBuf.buf[0];
    memcpy(underflowBuf.buf,underflowBuf.buf+1,--underflowBuf.len);
    return b;
  }
   */
  int c;
  if(clientPtr != null)
    c = clientPtr->read();
  else
    c= client.read();
  //fillUnderflowBuf();
  return c;
}

int WiFiClientNode::peek()
{
  if((host == null)||(!answered))
    return 0;
  /*
  // underflow buf is no longer needed
  if(underflowBuf.len > 0)
    return underflowBuf.buf[0];
  */
  if(clientPtr != null)
    return clientPtr->peek();
  else
    return client.peek();
}

void WiFiClientNode::flush()
{
  if(host != null)
  {
    if(clientPtr != null)
    {
      if(clientPtr->available()==0)
        flushAlways();
    }
    else
    if(client.available()==0)
      flushAlways();
  }
}

void WiFiClientNode::flushAlways()
{
  if(host != null)
  {
    flushOverflowBuffer();
    if(clientPtr != null)
      clientPtr->flush();
    else
      client.flush();
  }
}

int WiFiClientNode::available()
{
  if((host == null)||(!answered))
    return 0;
  if(clientPtr != null)
    return clientPtr->available();
  else
    return client.available(); // +underflowBuf.len;
}

int WiFiClientNode::read(uint8_t *buf, size_t size)
{
  if((host == null)||(!answered))
    return 0;
  // this whole underflow buf len thing is to get around yet another
  // problem in the underlying library where a socket disconnection
  // eats away any stray available bytes in their buffers.
  /*
  // this was changed to be handled a different, so underBuf is also deprecated;
  int previouslyRead = 0;
  if(underflowBuf.len > 0)
  {
    if(underflowBuf.len <= size)
    {
      previouslyRead += underflowBuf.len;
      memcpy(buf,underflowBuf.buf,underflowBuf.len);
      size -= underflowBuf.len;
      underflowBuf.len = 0;
      buf += previouslyRead;
    }
    else
    {
      memcpy(buf,underflowBuf.buf,size);
      underflowBuf.len -= size;
      memcpy(underflowBuf.buf,underflowBuf.buf+size,underflowBuf.len);
      return size;
    }
  }
  if(size == 0)
    return previouslyRead;
   */

  int bytesRead;
  if(clientPtr != null)
    bytesRead = clientPtr->read(buf,size);
  else
    bytesRead = client.read(buf,size);
  //fillUnderflowBuf();
  return bytesRead;// + previouslyRead;
}

int WiFiClientNode::flushOverflowBuffer()
{
  /*
   * I've never gotten any of this to trigger, and could use those
   * extra 260 bytes per connection
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
      s_pinWrite(pinRTS,rtsActive);
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
      s_pinWrite(pinRTS,rtsInactive);
      return bufWrite;
    }
  }
   */
  return 0;
}

size_t WiFiClientNode::write(const uint8_t *buf, size_t size)
{
  int written = 0;
  /* overflow buf is pretty much useless now
  if(host == null)
  {
    if(overflowBufLen>0)
    {
      s_pinWrite(pinRTS,rtsActive);
    }
    overflowBufLen=0;
    return 0;
  }
  written += flushOverflowBuffer();
  if(written > 0)
  {
    for(int i=0;i<size && overflowBufLen<OVERFLOW_BUF_SIZE;i++,overflowBufLen++)
      overflowBuf[overflowBufLen]=buf[i];
    return written;
  }
  */
  if(clientPtr != null)
    written += clientPtr->write(buf, size);
  else
    written += client.write(buf, size);
  /*
  if(written < size)
  {
    for(int i=written;i<size && overflowBufLen<OVERFLOW_BUF_SIZE;i++,overflowBufLen++)
      overflowBuf[overflowBufLen]=buf[i];
    s_pinWrite(pinRTS,rtsInactive);
  }
  */
  return written;
}

void WiFiClientNode::print(String s)
{
  int size=s.length();
  write((const uint8_t *)s.c_str(),size);
}

String WiFiClientNode::readLine(unsigned int timeout)
{
  unsigned long now=millis();
  String line = "";
  while(((millis()-now < timeout) || (available()>0)))
  {
    yield();
    if(available()>0)
    {
      char c=read();
      if((c=='\n')||(c=='\r'))
      {
          if(line.length()>0)
            return line;
      }
      else
      if((c >= 32 ) && (c <= 127))
        line += c;
    }
  }
  return line;
}

void WiFiClientNode::answer()
{
  answered=true;
  ringsRemain=0;
  nextRingMillis=0;
}

bool WiFiClientNode::isAnswered()
{
  return answered;
}

int WiFiClientNode::ringsRemaining(int delta)
{
  ringsRemain += delta;
  return ringsRemain;
}

unsigned long WiFiClientNode::nextRingTime(long delta)
{
  nextRingMillis += delta;
  return nextRingMillis;
}

size_t WiFiClientNode::write(uint8_t c)
{
  const uint8_t one[] = {c};
  write(one,1);
  return 1;
}

int WiFiClientNode::getNumOpenWiFiConnections()
{
  int num = 0;
  WiFiClientNode *conn = conns;
  while(conn != null)
  {
    WiFiClientNode *chkConn = conn;
    conn = conn->next;
    if((chkConn->nextDisconnect != 0)
    &&(millis() > chkConn->nextDisconnect))
      delete(chkConn);
    else
    if((chkConn->isConnected()
     ||(chkConn->available()>0)
     ||((chkConn == conns)
       &&((serialOutBufferBytesRemaining() <SER_WRITE_BUFSIZE-1)
         ||(HWSerial.availableForWrite()<SER_BUFSIZE))))
    && chkConn->isAnswered())
      num++;
  }
  return num;
}

void WiFiClientNode::markForDisconnect()
{
  if(nextDisconnect == 0)
    nextDisconnect = millis() + 5000; // 5 sec
}

bool WiFiClientNode::isMarkedForDisconnect()
{
  return nextDisconnect != 0;
}

void WiFiClientNode::checkForAutoDisconnections()
{
  WiFiClientNode *conn = conns;
  while(conn != null)
  {
    WiFiClientNode *chkConn = conn;
    conn = conn->next;
    if((chkConn->nextDisconnect != 0)
    &&(millis() > chkConn->nextDisconnect))
      delete(chkConn);
  }
}
