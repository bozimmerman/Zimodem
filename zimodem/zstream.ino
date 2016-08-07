    
void ZStream::reset(WiFiClientNode *conn, bool dodisconnect, bool doPETSCII, bool doTelnet, bool doecho)
{
  current = conn;
  currentExpires = 0;
  disconnectOnExit=dodisconnect;
  plussesInARow=0;
  echo=doecho;
  petscii=doPETSCII;
  telnet=doTelnet;
}
    
void ZStream::serialIncoming()
{
    while(Serial.available()>0)
    {
        uint8_t c=Serial.read();
        if(c=='+')
          plussesInARow++;
        else
          plussesInARow=0;
        if(c>0)
        {
          if(echo)
            Serial.write(c);
          if(petscii)
            c = petToAsc(c,&Serial);
          if(current->isConnected() && (c != 0))
            current->client->write(c);
        }
    }
    currentExpires=0;
    if(plussesInARow==3)
      currentExpires=millis()+1000;
}

void ZStream::loop()
{
  if((current==null)||(!current->isConnected()))
  {
    Serial.println("NOCARRIER");
    currMode = &commandMode;
  }
  else
  if((currentExpires > 0) && (millis() > currentExpires))
  {
    currentExpires = 0;
    if(plussesInARow == 3)
    {
      plussesInARow=0;
      if(current != 0)
      {
        if(disconnectOnExit)
        {
          Serial.printf("NOCARRIER %d %s:%d\r\n",current->id,current->host,current->port);
          delete current;
        }
        Serial.println("READY.");
        current = null;
        currMode = &commandMode;
      }
    }
  }
  else
  if(current->client->available()>0)
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
    }
    Serial.flush();
  }
}

