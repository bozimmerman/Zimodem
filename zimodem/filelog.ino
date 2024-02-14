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

#include <cstdio>

static char HD[3];
static char HDL[17];

static unsigned long logStartTime = millis();
static unsigned long lastLogTime = millis();
static unsigned long logCurCount = 0;
static LogOutputState logOutputState = LOS_NADA;
static char LBUF[256];
static int pinLook[8];

static uint8_t FROMHEXDIGIT(uint8_t a1)
{
  a1 = lc(a1);
  if((a1 >= '0')&&(a1 <= '9'))
    return a1-'0';
  if((a1 >= 'a')&&(a1 <= 'f'))
    return 10 + (a1-'a');
  return 0;
}

static uint8_t FROMHEX(uint8_t a1, uint8_t a2)
{
  return (FROMHEXDIGIT(a1) * 16) + FROMHEXDIGIT(a2);
}

static char *FROMHEX(const char *hex, char *s, const size_t len)
{
  int i=0;
  for(const char *h=hex;*h != 0 && (*(h+1)!=0) && (i<len-1);i++,h+=2)
    s[i]=FROMHEX((uint8_t)*h,(uint8_t)*(h+1));
  s[i]=0;
  return s;
}

static uint8_t *FROMHEX(uint8_t *s, const size_t len)
{
  int i=0;
  for(int i=0;i<len;i+=2)
    s[i/2]=FROMHEX(s[i],s[i+1]);
  s[i]=0;
  return s;
}

static char *TOHEX(const char *s, char *hex, const size_t len)
{
  int i=0;
  for(const char *t=s;*t != 0 && (i<len-2);i+=2,t++)
  {
    char *x=TOHEX(*t);
    hex[i]=x[0];
    hex[i+1]=x[1];
  }
  hex[i]=0;
  return hex;
}

static char *TOHEX(uint8_t a)
{
  HD[0] = "0123456789ABCDEF"[(a >> 4) & 0x0f];
  HD[1] = "0123456789ABCDEF"[a & 0x0f];
  HD[2] = 0;
  return HD;
}

static char *tohex(uint8_t a)
{
  HD[0] = "0123456789abcdef"[(a >> 4) & 0x0f];
  HD[1] = "0123456789abcdef"[a & 0x0f];
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

static bool isChangedBit()
{
  int val;
  bool changed=false;
  for(int i=0;i<7;i++)
  {
    switch(i)
    {
      case 0: val = pinCache[pinDCD]; break;
      case 1: val = digitalRead(pinCTS); break;
      case 2: val = pinCache[pinRTS]; break;
      case 3: val = pinCache[pinDSR]; break;
      case 4: val = digitalRead(pinDTR); break;
      case 5: val = digitalRead(pinOTH); break;
      case 6: val = pinCache[pinRI]; break;
    }
    if(pinLook[i] != val)
    {
      pinLook[i] = val;
      changed=true;
    }
  }
  return changed;
}

static void logFileLoop()
{
  if(logFileOpen && logFile2Uart)
  {
    if(isChangedBit())
    {
      char bits[8];
      for(int i=0;i<7;i++)
        bits[i] = pinLook[i]?'h':'l';
      bits[7]=0;
      logPrintfln("Signals: DCRSTOI (%s)",bits);
    }
  }
}


static void logInternalOut(const LogOutputState m, const uint8_t c)
{
  if(logFileOpen)
  {
    unsigned long diff = (millis()-lastLogTime);
    if((m != logOutputState)
    ||(++logCurCount > DBG_BYT_CTR)
    ||(diff>expectedSerialTime))
    {
      if((diff<=expectedSerialTime)
      &&(logCurCount<= DBG_BYT_CTR-3)
      &&(logOutputState != LOS_NADA)
      &&(m != LOS_NADA))
      {
          switch(m)
          {
          case LOS_NADA:
            break;
          case LOS_SocketIn:
            rawLogPrintf(", SocI: ");
            break;
          case LOS_SocketOut:
            rawLogPrintf(", SocO: ");
            break;
          case LOS_SerialIn:
            rawLogPrintf(", SerI: ");
            break;
          case LOS_SerialOut:
            rawLogPrintf(", SerO: ");
            break;
          }
          logCurCount+=4;
      }
      else
      {
        rawLogPrintln("");
        switch(m)
        {
        case LOS_NADA:
          break;
        case LOS_SocketIn:
          rawLogPrintf("%s SocI: ",TOHEX(millis()-logStartTime));
          break;
        case LOS_SocketOut:
          rawLogPrintf("%s SocO: ",TOHEX(millis()-logStartTime));
          break;
        case LOS_SerialIn:
          rawLogPrintf("%s SerI: ",TOHEX(millis()-logStartTime));
          break;
        case LOS_SerialOut:
          rawLogPrintf("%s SerO: ",TOHEX(millis()-logStartTime));
          break;
        }
        logCurCount=0;
      }
      logOutputState = m;
    }
    lastLogTime=millis();
    rawLogPrint(TOHEX(c));
    rawLogPrint(" ");
  }
}

static void logSerialOut(const uint8_t c)
{
  logInternalOut(LOS_SerialOut,c);
}

static void logSocketOut(const uint8_t c)
{
  logInternalOut(LOS_SocketOut,c);
}

static void logSerialIn(const uint8_t c)
{
  logInternalOut(LOS_SerialIn,c);
}

static void logSocketIn(const uint8_t c)
{
  logInternalOut(LOS_SocketIn,c);
}

static void logSocketIn(const uint8_t *c, int n)
{
  if(logFileOpen)
  {
    for(int i=0;i<n;i++)
      logInternalOut(LOS_SocketIn,c[i]);
  }
}

static void rawLogPrintf(const char* format, ...)
{
  va_list arglist;
  va_start(arglist, format);
  vsnprintf(LBUF,sizeof(LBUF), format, arglist);
  rawLogPrint(LBUF);
  va_end(arglist);

}

static void rawLogPrint(const char* str)
{
  if(logFile2Uart)
    debugPrintf(str);
  else
    logFile.print(str);
}

static void rawLogPrintln(const char* str)
{
  if(logFile2Uart)
  {
    debugPrintf(str);
    debugPrintf("\n");
  }
  else
    logFile.println(str);
}

static void logPrintfln(const char* format, ...) 
{
  if(logFileOpen)
  {
    if(logOutputState != LOS_NADA)
    {
      rawLogPrintln("");
      logOutputState = LOS_NADA;
    }
    va_list arglist;
    va_start(arglist, format);
    vsnprintf(LBUF,sizeof(LBUF), format, arglist);
    rawLogPrintln(LBUF);
    va_end(arglist);
  }
}

static void logPrintf(const char* format, ...) 
{
  if(logFileOpen)
  {
    if(logOutputState != LOS_NADA)
    {
      rawLogPrintln("");
      logOutputState = LOS_NADA;
    }
    va_list arglist;
    va_start(arglist, format);
    vsnprintf(LBUF, sizeof(LBUF), format, arglist);
    rawLogPrint(LBUF);
    va_end(arglist);
  }
}

static void logPrint(const char* msg)
{
  if(logFileOpen)
  {
    if(logOutputState != LOS_NADA)
    {
      rawLogPrintln("");
      logOutputState = LOS_NADA;
    }
    rawLogPrint(msg);
  }
}

static void logPrintln(const char* msg)
{
  if(logFileOpen)
  {
    if(logOutputState != LOS_NADA)
    {
      rawLogPrintln("");
      logOutputState = LOS_NADA;
    }
    rawLogPrintln(msg);
  }
}

