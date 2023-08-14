/*
   Copyright 2016-2019 Bo Zimmerman

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

void ZStream::switchTo(WiFiClientNode *conn)
{
  current = conn;
  currentExpiresTimeMs = 0;
  lastNonPlusTimeMs = 0;
  plussesInARow=0;
  serial.setXON(true);
  serial.setPetsciiMode(isPETSCII());
  serial.setFlowControlType(getFlowControl());
  currMode=&streamMode;
  checkBaudChange();
  if(pinSupport[pinDTR])
    lastDTR = digitalRead(pinDTR);
}

bool ZStream::isPETSCII()
{
  return (current != null) && (current->isPETSCII());
}

bool ZStream::isEcho()
{
  return (current != null) && (current->isEcho());
}

FlowControlType ZStream::getFlowControl()
{
  return (current != null) ? (current->getFlowControl()) : FCT_DISABLED;
}

bool ZStream::isTelnet()
{
  return (current != null) && (current->isTelnet());
}

bool ZStream::isDisconnectedOnStreamExit()
{
  return (current != null) && (current->isDisconnectedOnStreamExit());
}

void ZStream::baudDelay()
{
  if(baudRate<1200)
    delay(5);
  else
  if(baudRate==1200)
    delay(3);
  else
    delay(1);
  yield();
}

void ZStream::serialIncoming()
{
  int bytesAvailable = HWSerial.available();
  if(bytesAvailable == 0)
    return;
  uint8_t escBufDex = 0;
  while(--bytesAvailable >= 0)
  {
    uint8_t c=HWSerial.read();
    if(((c==27)||(escBufDex>0))
    &&(!isPETSCII()))
    {
      escBuf[escBufDex++] = c;
      if(((c>='a')&&(c<='z'))
      ||((c>='A')&&(c<='Z'))
      ||(escBufDex>=ZSTREAM_ESC_BUF_MAX)
      ||((escBufDex==2)&&(c!='[')))
      {
        logSerialIn(c);
        break;
      }
      if(bytesAvailable==0)
      {
        baudDelay();
        bytesAvailable=HWSerial.available();
      }
    }
    logSerialIn(c);
    if((c==commandMode.EC)
    &&((plussesInARow>0)||((millis()-lastNonPlusTimeMs)>800)))
      plussesInARow++;
    else
    if(c!=commandMode.EC)
    {
      plussesInARow=0;
      lastNonPlusTimeMs=millis();
    }
    if((c==19)&&(getFlowControl()==FCT_NORMAL))
      serial.setXON(false);
    else
    if((c==17)&&(getFlowControl()==FCT_NORMAL))
      serial.setXON(true);
    else
    {
      if(isEcho())
        serial.printb(c);
      if(isPETSCII())
        c = petToAsc(c);
      if(escBufDex==0)
        socketWrite(c);
    }
  }

  if(escBufDex>0)
    socketWrite(escBuf,escBufDex);
  currentExpiresTimeMs = 0;
  if(plussesInARow==3)
    currentExpiresTimeMs=millis()+800;
}

void ZStream::switchBackToCommandMode(bool logout)
{
  if(logout && (current != null) && isDisconnectedOnStreamExit())
  {
    if(!commandMode.suppressResponses)
    {
      if(commandMode.numericResponses)
      {
        preEOLN(commandMode.EOLN);
        serial.prints("3");
        serial.prints(commandMode.EOLN);
      }
      else
      if(current->isAnswered())
      {
        preEOLN(commandMode.EOLN);
        serial.prints("NO CARRIER");
        serial.prints(commandMode.EOLN);
      }
    }
    delete current;
  }
  current = null;
  currMode = &commandMode;
}

void ZStream::socketWrite(uint8_t *buf, uint8_t len)
{
  if(current->isConnected())
  {
    uint8_t escapedBuf[len*2];
    if(isTelnet())
    {
      int eDex=0;
      for(int i=0;i<len;i++)
      {
          escapedBuf[eDex++] = buf[i];
          if(buf[i]==0xff)
            escapedBuf[eDex++] = buf[i];
      }
      buf=escapedBuf;
      len=eDex;
    }
    for(int i=0;i<len;i++)
      logSocketOut(buf[i]);
    current->write(buf,len);
    nextFlushMs=millis()+250;
  }
}

void ZStream::socketWrite(uint8_t c)
{
  if(current->isConnected())
  {
    if(c == 0xFF && isTelnet()) 
      current->write(c);
    current->write(c);
    logSocketOut(c);
    nextFlushMs=millis()+250;
    //current->flush(); // rendered safe by available check
    //delay(0);
    //yield();
  }
}

void ZStream::loop()
{
  WiFiServerNode *serv = servs;
  while(serv != null)
  {
    if(serv->hasClient())
    {
      WiFiClient newClient = serv->server->available();
      if(newClient.connected())
      {
        int port=newClient.localPort();
        String remoteIPStr = newClient.remoteIP().toString();
        const char *remoteIP=remoteIPStr.c_str();
        bool found=false;
        WiFiClientNode *c=conns;
        while(c!=null)
        {
          if((c->isConnected())
          &&(c->port==port)
          &&(strcmp(remoteIP,c->host)==0))
            found=true;
          c=c->next;
        }
        if(!found)
          new WiFiClientNode(newClient, serv->flagsBitmap, 5); // constructing is enough
        // else // auto disconnect when from same 
      }
    }
    serv=serv->next;
  }
  
  WiFiClientNode *conn = conns;
  unsigned long now=millis();
  while(conn != null)
  {
    WiFiClientNode *nextConn = conn->next;
    if((!conn->isAnswered())
    &&(conn->isConnected())
    &&(conn!=current)
    &&(!conn->isMarkedForDisconnect()))
    {
      conn->write((uint8_t *)busyMsg.c_str(), busyMsg.length());
      conn->flushAlways();
      conn->markForDisconnect();
    }
    conn = nextConn;
  }
  
  WiFiClientNode::checkForAutoDisconnections();
  
  if(pinSupport[pinDTR])
  {
    if(lastDTR==dtrActive)
    {
      lastDTR = digitalRead(pinDTR);
      if((lastDTR==dtrInactive)
      &&(dtrInactive != dtrActive))
      {
        if(current != null)
          current->setDisconnectOnStreamExit(true);
        switchBackToCommandMode(true);
      }
    }
    lastDTR = digitalRead(pinDTR);
  }
  if((current==null)||(!current->isConnected()))
  {
    switchBackToCommandMode(true);
  }
  else
  if((currentExpiresTimeMs > 0) && (millis() > currentExpiresTimeMs))
  {
    currentExpiresTimeMs = 0;
    if(plussesInARow == 3)
    {
      plussesInARow=0;
      if(current != 0)
      {
        commandMode.sendOfficialResponse(ZOK);
        switchBackToCommandMode(false);
      }
    }
  }
  else
  if(serial.isSerialOut())
  {
    if(current->available()>0)
    //&&(current->isConnected()) // not a requirement to have available bytes to read
    {
      int bufferRemaining=serialOutBufferBytesRemaining();
      if(bufferRemaining > 0)
      {
        int bytesAvailable = current->available();
        if(bytesAvailable > bufferRemaining)
          bytesAvailable = bufferRemaining;
        if(bytesAvailable>0)
        {
          for(int i=0;(i<bytesAvailable) && (current->available()>0);i++)
          {
            if(serial.isSerialCancelled())
              break;
            uint8_t c=current->read();
            logSocketIn(c);
            if((!isTelnet() || handleAsciiIAC((char *)&c,current))
            && (!isPETSCII() || ascToPet((char *)&c,current)))
              serial.printb(c);
          }
        }
      }
    }
    if(serial.isSerialOut())
    {
      if((nextFlushMs > 0) && (millis() > nextFlushMs))
      {
        nextFlushMs = 0;
        serial.flush();
      }
      serialOutDeque();
    }
  }
  checkBaudChange();
}

