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

#define DBG_BYT_CTR 20

void ZStream::switchTo(WiFiClientNode *conn)
{
  current = conn;
  currentExpiresTimeMs = 0;
  lastNonPlusTimeMs = 0;
  plussesInARow=0;
  serial.setXON(true);
  serial.setPetsciiMode(isPETSCII());
  serial.setFlowControlType(isXonXoff()?FCT_NORMAL:FCT_RTSCTS);
  currMode=&streamMode;
  checkBaudChange();
}

bool ZStream::isPETSCII()
{
  return (current != null) && (current->isPETSCII());
}

bool ZStream::isEcho()
{
  return (current != null) && (current->isEcho());
}

bool ZStream::isXonXoff()
{
  return (current != null) && (current->isXonXoff());
}

bool ZStream::isTelnet()
{
  return (current != null) && (current->isTelnet());
}

bool ZStream::isDisconnectedOnStreamExit()
{
  return (current != null) && (current->isDisconnectedOnStreamExit());
}

void ZStream::serialIncoming()
{
  int serialAvailable = Serial.available();
  if(serialAvailable == 0)
    return;
  while(--serialAvailable >= 0)
  {
    uint8_t c=Serial.read();
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
    if((c==19)&&(isXonXoff()))
      serial.setXON(false);
    else
    if((c==17)&&(isXonXoff()))
      serial.setXON(true);
    else
    {
      if(isEcho())
        enqueSerialOut(c);
      if(isPETSCII())
        c = petToAsc(c);
      socketWrite(c);
    }
  }
  
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
        serial.prints("3");
      else
        serial.prints("NO CARRIER");
      serial.prints(commandMode.EOLN);
    }
    delete current;
  }
  current = null;
  currMode = &commandMode;
}

void ZStream::socketWrite(uint8_t c)
{
  if(current->isConnected())
  {
    current->write(c);
    logSocketOut(c);
    current->flush(); // rendered safe by available check
  }
  //delay(0);
  //yield();
}

void ZStream::loop()
{
  WiFiServerNode *serv = servs;
  while(serv != null)
  {
    if(serv->hasClient())
    {
      WiFiClient newClient = serv->server->available();
      if((newClient != null)&&(newClient.connected()))
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
        {
          newClient.write("\r\n\r\n\r\n\r\n\r\nBUSY\r\n7\r\n");
          newClient.flush();
          newClient.stop();
        }
      }
    }
    serv=serv->next;
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
        switchBackToCommandMode(false);
      }
    }
  }
  else
  if(serial.isSerialOut())
  {
    if((current->isConnected()) && (current->available()>0))
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
            if((!isTelnet() || handleAsciiIAC((char *)&c,current))
            && (!isPETSCII() || ascToPet((char *)&c,current)))
              enqueSerialOut(c);
          }
        }
      }
    }
    if(serial.isSerialOut())
      serialOutDeque();
  }
  checkBaudChange();
}

