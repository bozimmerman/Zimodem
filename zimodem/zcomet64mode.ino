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
#ifdef INCLUDE_SD_SHELL
#ifdef INCLUDE_COMET64

void ZComet64Mode::switchBackToCommandMode()
{
  debugPrintf("\r\nMode:Command\r\n");
  if(proto != 0)
    delete proto;
  proto = 0;
  currMode = &commandMode;
  if(baudState == BS_SWITCHED_TEMP)
    baudState = BS_SWITCH_NORMAL_NEXT;
}

void ZComet64Mode::switchTo()
{
  debugPrintf("\r\nMode:Comet64\r\n");
  currMode=&comet64Mode;
  if(proto == 0)
    proto = new Comet64(&SD,commandMode.serial.getFlowControlType());
  if((tempBaud > 0) && (baudState == BS_NORMAL))
    baudState = BS_SWITCH_TEMP_NEXT;
  checkBaudChange();
}

void ZComet64Mode::serialIncoming()
{
  if(proto != 0)
    proto->receiveLoop();
}

void ZComet64Mode::loop()
{
  serialOutDeque();
  if((proto != 0) && (proto->isAborted()))
    switchBackToCommandMode();
  logFileLoop();
  checkBaudChange();
}

#endif
#endif
