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

void ZStream::switchTo(WiFiClientNode *conn, bool dodisconnect, bool doPETSCII, bool doTelnet)
{
  switchTo(conn);
  disconnectOnExit=dodisconnect;
  petscii=doPETSCII;
  telnet=doTelnet;
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
    
void ZStream::serialIncoming()
{
  int serialAvailable = Serial.available();
  int wasSerialAvailable = serialAvailable;
  if(serialAvailable > 0)
  {
    //Serial.write('*'); Serial.flush();
  }
  while((serialAvailable--)>0)
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
    if(c>0)
    {
      if((c==19)&&(commandMode.doFlowControl))
        XON=false;
      else
      if((c==17)&&(commandMode.doFlowControl))
        XON=true;
      else
      {
        if(commandMode.doEcho)
          Serial.write(c);
        if(petscii)
          c = petToAsc(c,&Serial);
        if(current->isConnected() && (c != 0))
          current->client->write(c);
      }
    }
  }
  currentExpiresTimeMs = 0;
  if(plussesInARow==3)
    currentExpiresTimeMs=millis()+1000;
  if(wasSerialAvailable > 0)
  {
    //Serial.write('&'); Serial.flush();
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


void ZStream::loop()
{
  WiFiServerNode *serv = servs;
  while(serv != null)
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
  if(((!commandMode.doFlowControl)||(XON))
  &&(current->isConnected())
  &&(current->client->available()>0))
  {
    int maxBytes= 1;//baudRate / 100; // commandMode.packetSize ; //watchdog'll get you if you're in here too long
    int bytesAvailable = current->client->available();
    if(bytesAvailable > maxBytes)
      bytesAvailable = maxBytes;
    while(current->isConnected() && ((bytesAvailable--)>0))
    {
      uint8_t c=current->client->read();
      if(telnet)
        c=handleAsciiIAC(c,current->client);
      if(petscii)
        c=ascToPet(c,current->client);
      if(c>0)
        Serial.write(c);
      if((commandMode.doFlowControl)&&(Serial.available()>0))
        break;
    }
  }
}

