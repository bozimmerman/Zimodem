    
void ZStream::reset(WiFiClientNode *conn, bool dodisconnect, bool doPETSCII, bool doTelnet)
{
  current = conn;
  currentExpiresTimeMs = 0;
  lastNonPlusTimeMs = 0;
  disconnectOnExit=dodisconnect;
  plussesInARow=0;
  petscii=doPETSCII;
  telnet=doTelnet;
  XON=true;
}
    
void ZStream::serialIncoming()
{
  while(Serial.available()>0)
  {
      uint8_t c=Serial.read();
      if((c=='+')
      &&((plussesInARow>0)||((millis()-lastNonPlusTimeMs)>1000)))
        plussesInARow++;
      else
      if(c!='+')
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
              Serial.printf("NO CARRIER %d %s:%d",current->id,current->host,current->port);
            Serial.print(commandMode.EOLN);
          }
          delete current;
        }
        Serial.println("READY.");
        current = null;
        currMode = &commandMode;
      }
    }
  }
  else
  if(((!commandMode.doFlowControl)||(XON))
  &&(current->client->available()>0))
  {
    while(current->client->available()>0)
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
    Serial.flush();
  }
}

