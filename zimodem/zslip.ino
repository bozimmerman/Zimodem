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

#include "ESP8266WiFi.h"

void ZSlip::switchTo()
{
  currentExpiresTimeMs = 0;
  lastNonPlusTimeMs = 0;
  plussesInARow=0;
  serial.setXON(true);
  serial.setPetsciiMode(false);
  serial.setFlowControlType(FCT_RTSCTS);
  currMode=&slipMode;
  checkBaudChange();
}

void ZSlip::serialIncoming()
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
    //socketWrite(c); *** do the slip packet decode
  }
  
  currentExpiresTimeMs = 0;
  if(plussesInARow==3)
    currentExpiresTimeMs=millis()+800;
}

void ZSlip::switchBackToCommandMode(bool logout)
{
  currMode = &commandMode;
}

void ZSlip::socketWrite(uint8_t c)
{
  // do something special?
}

void ZSlip::loop()
{
  if((currentExpiresTimeMs > 0) && (millis() > currentExpiresTimeMs))
  {
    currentExpiresTimeMs = 0;
    if(plussesInARow == 3)
    {
      plussesInARow=0;
      switchBackToCommandMode(false);
    }
  }
  else
  if(serial.isSerialOut())
  {
    // probably loop through all the open sockets and do something interesting.
  }
  checkBaudChange();
}

