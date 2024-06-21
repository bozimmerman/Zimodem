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

void ConnSettings::setFlag(ConnFlag flagMask, bool newVal)
{
  switch(flagMask)
  {
    case FLAG_DISCONNECT_ON_EXIT: break;
    case FLAG_PETSCII: petscii = newVal; break;
    case FLAG_TELNET: telnet = newVal; break;
    case FLAG_ECHO: echo = newVal; break;
    case FLAG_XONXOFF: xonxoff = newVal; break;
    case FLAG_SECURE: secure = newVal; break;
    case FLAG_RTSCTS: rtscts = newVal; break;
  }
}

void ConnSettings::IPtoStr(IPAddress *ip, String &str)
{
  if(ip == null)
  {
    str="";
    return;
  }
  char temp[20];
  sprintf(temp,"%d.%d.%d.%d",(*ip)[0],(*ip)[1],(*ip)[2],(*ip)[3]);
  str = temp;
}

IPAddress *ConnSettings::parseIP(const char *ipStr)
{
  uint8_t dots[4];
  int dotDex=0;
  char *le = (char *)ipStr;
  const char *ld = ipStr+strlen(ipStr);
  if(strlen(ipStr)<7)
    return null;
  for(char *e=le;e<=ld;e++)
  {
    if((*e=='.')||(e==ld))
    {
      if(le==e)
        break;
      *e=0;
      String sdot = le;
      sdot.trim();
      if((sdot.length()==0)||(dotDex>3))
      {
        dotDex=99;
        break;
      }
      dots[dotDex++]=(uint8_t)atoi(sdot.c_str());
      if(e==ld)
        le=e;
      else
        le=e+1;
    }
  }
  if((dotDex!=4)||(*le != 0))
    return null;
  return new IPAddress(dots[0],dots[1],dots[2],dots[3]);
}

