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

ConnSettings::ConnSettings(int flagBitmap)
{
  petscii = (flagBitmap & FLAG_PETSCII) > 0;
  telnet = (flagBitmap & FLAG_TELNET) > 0;
  echo = (flagBitmap & FLAG_ECHO) > 0;
  xonxoff = (flagBitmap & FLAG_XONXOFF) > 0;
  rtscts = (flagBitmap & FLAG_RTSCTS) > 0;
  secure = (flagBitmap & FLAG_SECURE) > 0;
}

ConnSettings::ConnSettings(const char *dmodifiers)
{
  petscii=((strchr(dmodifiers,'p')!=null) || (strchr(dmodifiers,'P')!=null));
  telnet=((strchr(dmodifiers,'t')!=null) || (strchr(dmodifiers,'T')!=null));
  echo=((strchr(dmodifiers,'e')!=null) || (strchr(dmodifiers,'E')!=null));
  xonxoff=((strchr(dmodifiers,'x')!=null) || (strchr(dmodifiers,'X')!=null));
  rtscts=((strchr(dmodifiers,'r')!=null) || (strchr(dmodifiers,'R')!=null));
  secure=((strchr(dmodifiers,'s')!=null) || (strchr(dmodifiers,'S')!=null));
}

ConnSettings::ConnSettings(String modifiers) : ConnSettings(modifiers.c_str())
{
}

int ConnSettings::getBitmap()
{
  int flagsBitmap = 0;
  if(petscii)
    flagsBitmap = flagsBitmap | FLAG_PETSCII;
  if(telnet)
    flagsBitmap = flagsBitmap | FLAG_TELNET;
  if(echo)
    flagsBitmap = flagsBitmap | FLAG_ECHO;
  if(xonxoff)
    flagsBitmap = flagsBitmap | FLAG_XONXOFF;
  if(rtscts)
    flagsBitmap = flagsBitmap | FLAG_RTSCTS;
  if(secure)
    flagsBitmap = flagsBitmap | FLAG_SECURE;
  return flagsBitmap;
}

int ConnSettings::getBitmap(FlowControlType forceCheck)
{
  int flagsBitmap = getBitmap();
  if(((flagsBitmap & (FLAG_XONXOFF | FLAG_RTSCTS))==0)
  &&(forceCheck==FCT_RTSCTS))
    flagsBitmap |= FLAG_RTSCTS;
  return flagsBitmap;
}

String ConnSettings::getFlagString()
{
  String lastOptions =(petscii?"p":"");
  lastOptions += (petscii?"p":"");
  lastOptions += (telnet?"t":"");
  lastOptions += (echo?"e":"");
  lastOptions += (xonxoff?"x":"");
  lastOptions += (rtscts?"r":"");
  lastOptions += (secure?"s":"");
  return lastOptions;
}
