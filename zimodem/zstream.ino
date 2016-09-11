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

#define DBG_BYT_CTR 23

void ZStream::switchTo(WiFiClientNode *conn, bool dodisconnect, bool doPETSCII, bool doTelnet)
{
  switchTo(conn);
  disconnectOnExit=dodisconnect;
  petscii=doPETSCII;
  telnet=doTelnet;
}
    
void ZStream::switchTo(WiFiClientNode *conn, bool dodisconnect, bool doPETSCII, bool doTelnet, bool bbs)
{
  switchTo(conn,dodisconnect,doPETSCII,doTelnet);
  doBBS=bbs;
}
void ZStream::switchTo(WiFiClientNode *conn)
{
  current = conn;
  currentExpiresTimeMs = 0;
  lastNonPlusTimeMs = 0;
  plussesInARow=0;
  XON=true;
  dcdStatus = HIGH;
  digitalWrite(2,dcdStatus);
  currMode=&streamMode;
}

static char HD[3];

char *TOHEX(uint8_t a)
{
  HD[0] = "0123456789ABCDEF"[(a >> 4) & 0x0f];
  HD[1] = "0123456789ABCDEF"[a & 0x0f];
  HD[2] = 0;
  return HD;
}

void ZStream::serialIncoming()
{
  int serialAvailable = Serial.available();
  if(serialAvailable > 0)
  {
    uint8_t c=Serial.read();
    if((c==commandMode.EC)
    &&((plussesInARow>0)||((millis()-lastNonPlusTimeMs)>1000)))
      plussesInARow++;
    else
    if(c!=commandMode.EC)
    {
      plussesInARow=0;
      lastNonPlusTimeMs=millis();
    }
    if((c==19)&&(commandMode.doFlowControl) && (!doBBS))
      XON=false;
    else
    if((c==17)&&(commandMode.doFlowControl) && (!doBBS))
      XON=true;
    else
    {
      if(commandMode.doEcho && (!doBBS))
        Serial.write(c);
      if(petscii)
        c = petToAsc(c);
      if(current->isConnected())
      {
        //BZ: Requires changing delay(5000) in ESP8266 ClientContext.h
        current->write(c);
        current->flush();
        if(logFileOpen)
        {
          if((logFileCtrW > 0)
          ||(++logFileCtrR > DBG_BYT_CTR))
          {
            logFileCtrR=1;
            logFileCtrW=0;
            logFile.println("");
            logFile.print("Serial: ");
          }
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
    }
    
    currentExpiresTimeMs = 0;
    if(plussesInARow==3)
      currentExpiresTimeMs=millis()+1000;
  }
}

void ZStream::switchBackToCommandMode(bool logout)
{
    if(disconnectOnExit && logout && (current != null))
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
    dcdStatus = LOW;
    digitalWrite(2,dcdStatus);
    currMode = &commandMode;
}

void ZStream::serialWrite(uint8_t c)
{
  Serial.write(c);
  if(logFileOpen)
  {
    if((logFileCtrR > 0)
    ||(++logFileCtrW > DBG_BYT_CTR))
    {
      logFileCtrR=0;
      logFileCtrW=1;
      logFile.println("");
      logFile.print("Socket: ");
    }
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
    
void ZStream::serialDeque()
{
  if((TBUFhead != TBUFtail)&&(Serial.availableForWrite()>0))
  {
    serialWrite(TBUF[TBUFhead]);
    TBUFhead++;
    if(TBUFhead >= BUFSIZE)
      TBUFhead = 0;
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
  if((!commandMode.doFlowControl)||(XON)||(doBBS))
  {
    if((current->isConnected()) && (current->available()>0))
    {
      int maxBytes=  BUFSIZE; //baudRate / 100; //watchdog'll get you if you're in here too long
      int bytesAvailable = current->available();
      if(bytesAvailable > maxBytes)
        bytesAvailable = maxBytes;
      if(bytesAvailable>0)
      {
        for(int i=0;(i<bytesAvailable) && (current->available()>0);i++)
        {
          uint8_t c=current->read();
          if((!telnet || handleAsciiIAC((char *)&c,current))
          && (!petscii || ascToPet((char *)&c,current)))
          {
            TBUF[TBUFtail] = c;
            TBUFtail++;
            if(TBUFtail >= BUFSIZE)
              TBUFtail = 0;
          }
        }
      }
    }
    serialDeque();
   }
}

