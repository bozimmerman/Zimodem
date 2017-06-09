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

static char HD[3];
static char HDL[9];

static char *TOHEX(uint8_t a)
{
  HD[0] = "0123456789ABCDEF"[(a >> 4) & 0x0f];
  HD[1] = "0123456789ABCDEF"[a & 0x0f];
  HD[2] = 0;
  return HD;
}

static char *TOHEX(unsigned long a)
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

static char *TOHEX(unsigned int a)
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

static char *TOHEX(int a)
{
  return TOHEX((unsigned int)a);
}

static char *TOHEX(long a)
{
  return TOHEX((unsigned long)a);
}

static void logSerialOut(uint8_t c)
{
  if(logFileOpen)
  {
    if(streamStartTime == 0)
      streamStartTime = millis();
      
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

static void logSocketOut(uint8_t c)
{
  if(logFileOpen)
  {
    if(streamStartTime == 0)
      streamStartTime = millis();

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
}
