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
  XON=true;
  currMode=&streamMode;
  expectedSerialTime = (1000 / (baudRate / 8))+1;
  if(expectedSerialTime < 1)
    expectedSerialTime = 1;
  streamStartTime = 0;
  checkBaudChange();
}

static char HD[3];
static char HDL[9];

char *TOHEX(uint8_t a)
{
  HD[0] = "0123456789ABCDEF"[(a >> 4) & 0x0f];
  HD[1] = "0123456789ABCDEF"[a & 0x0f];
  HD[2] = 0;
  return HD;
}

char *TOHEX(unsigned long a)
{
  for(int i=7;i>=0;i--)
  {
    HDL[i] = "0123456789ABCDEF"[a & 0x0f];
    a = a >> 4;
  }
  HDL[8] = 0;
  char *H=HDL;
  if((strlen(H)>2) && (strstr(H,"00")==H))
    H+=2;
  return H;
}

char *TOHEX(unsigned int a)
{
  for(int i=3;i>=0;i--)
  {
    HDL[i] = "0123456789ABCDEF"[a & 0x0f];
    a = a >> 4;
  }
  HDL[4] = 0;
  char *H=HDL;
  if((strlen(H)>2) && (strstr(H,"00")==H))
    H+=2;
  return H;
}

char *TOHEX(int a)
{
  return TOHEX((unsigned int)a);
}

char *TOHEX(long a)
{
  return TOHEX((unsigned long)a);
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
      XON=false;
    else
    if((c==17)&&(isXonXoff()))
      XON=true;
    else
    {
      if(isEcho())
        enqueSerial(c);
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
        Serial.printf("3");
      else
        Serial.printf("NO CARRIER");
      Serial.print(commandMode.EOLN);
    }
    delete current;
  }
  current = null;
  currMode = &commandMode;
}

void ZStream::socketWrite(uint8_t c)
{
  if(!logFileOpen)
  {
    if(current->isConnected())
      current->write(c);
  }
  else
  {
    if(streamStartTime == 0)
      streamStartTime = millis();

    if(current->isConnected())
      current->write(c);
    if((logFileCtrW > 0)
    ||(++logFileCtrR > DBG_BYT_CTR)
    ||((millis()-lastSerialRead)>expectedSerialTime))
    {
      logFileCtrR=1;
      logFileCtrW=0;
      logFile.println("");
      logFile.printf("%s Ser: ",TOHEX(millis()-streamStartTime));
    }
    lastSerialRead=millis();
    /*if((c>=32)&&(c<=127))
    {
      logFile.print('_');
      logFile.print((char)c);
    }
    else*/
      logFile.print(TOHEX(c));
    logFile.print(" ");
  }
  current->flush(); // rendered safe by available check
  //delay(0);
  //yield();
}

void ZStream::serialWrite(uint8_t c)
{
  if(!logFileOpen)
  {
    Serial.write(c);
  }
  else
  {
    if(streamStartTime == 0)
      streamStartTime = millis();
  
    Serial.write(c);
    if(logFileOpen)
    {
      if((logFileCtrR > 0)
      ||(++logFileCtrW > DBG_BYT_CTR)
      ||((millis()-lastSerialWrite)>expectedSerialTime))
      {
        logFileCtrR=0;
        logFileCtrW=1;
        logFile.println("");
        logFile.printf("%s Soc: ",TOHEX(millis()-streamStartTime));
      }
      lastSerialWrite=millis();
      /*if((c>=32)&&(c<=127))
      {
        logFile.print('_');
        logFile.print((char)c);
      }
      else*/
        logFile.print(TOHEX(c));
      logFile.print(" ");
    }
  }
  if(commandMode.delayMs > 0)
    delay(commandMode.delayMs);
}
    
void ZStream::serialDeque()
{
  if((TBUFhead != TBUFtail)
  &&(Serial.availableForWrite()>0)
  &&((commandMode.writeClear<=0)||(Serial.availableForWrite()>=commandMode.writeClear)))
  {
    serialWrite(TBUF[TBUFhead]);
    TBUFhead++;
    if(TBUFhead >= BUFSIZE)
      TBUFhead = 0;
  }
}

int serialBufferBytesRemaining()
{
  if(TBUFtail == TBUFhead)
    return BUFSIZE-1;
  else
  if(TBUFtail > TBUFhead)
  {
    int used = TBUFtail - TBUFhead;
    return BUFSIZE - used -1;
  }
  else
    return TBUFhead - TBUFtail - 1;
}

void ZStream::enqueSerial(uint8_t c)
{
  TBUF[TBUFtail] = c;
  TBUFtail++;
  if(TBUFtail >= BUFSIZE)
    TBUFtail = 0;
}

void ZStream::clearSerialBuffer()
{
  TBUFtail=TBUFhead;
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
  if((!isXonXoff())||(XON))
  {
    if((current->isConnected()) && (current->available()>0))
    {
      int bufferRemaining=serialBufferBytesRemaining();
      if(bufferRemaining > 1)
      {
        int maxBytes=  BUFSIZE; //baudRate / 100; //watchdog'll get you if you're in here too long
        int bytesAvailable = current->available();
        if(bytesAvailable > maxBytes)
          bytesAvailable = maxBytes;
        if(bytesAvailable > bufferRemaining)
          bytesAvailable = bufferRemaining;
        if(bytesAvailable>0)
        {
          for(int i=0;(i<bytesAvailable) && (current->available()>0);i++)
          {
            uint8_t c=current->read();
            if((!isTelnet() || handleAsciiIAC((char *)&c,current))
            && (!isPETSCII() || ascToPet((char *)&c,current)))
              enqueSerial(c);
          }
        }
      }
    }
    serialDeque();
  }
  checkBaudChange();
}

