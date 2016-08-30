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
  current = conn;
  currentExpiresTimeMs = 0;
  lastNonPlusTimeMs = 0;
  disconnectOnExit=dodisconnect;
  plussesInARow=0;
  petscii=doPETSCII;
  telnet=doTelnet;
  XON=true;
  dcdStatus = HIGH;
  digitalWrite(2,dcdStatus);
  currMode=&streamMode;
}
    
void ZStream::serialIncoming()
{
  while(Serial.available()>0)
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
}

void ZStream::loop()
{
  if((current==null)||(!current->isConnected()))
  {
    if(!commandMode.suppressResponses)
    {
      if(commandMode.numericResponses)
        Serial.printf("3");
      else
        Serial.printf("NO CARRIER");
      Serial.print(commandMode.EOLN);
    }
    currMode = &commandMode;
    dcdStatus = LOW;
    digitalWrite(2,dcdStatus);
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
        if(disconnectOnExit)
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
        Serial.println("READY.");
        current = null;
        
        currMode = &commandMode;
        dcdStatus = LOW;
        digitalWrite(2,dcdStatus);
      }
    }
  }
  else
  if(((!commandMode.doFlowControl)||(XON))
  &&(current->client->available()>0))
  {
    int maxBytes=commandMode.packetSize; // watchdog'll get you if you're in here too long
    while((current->client->available()>0)&&(--maxBytes > 0))
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

