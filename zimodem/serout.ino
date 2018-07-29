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

#include <cstring>

static void serialDirectWrite(uint8_t c)
{
  HWSerial.write(c);
  if(serialDelayMs > 0)
    delay(serialDelayMs);
  logSerialOut(c);
}

static void hwSerialFlush()
{
#ifdef ZIMODEM_ESP8266
  HWSerial.flush();
#endif
}

static void serialOutDeque()
{
#ifdef ZIMODEM_ESP32
  while((TBUFhead != TBUFtail)
  &&(SER_BUFSIZE - HWSerial.availableForWrite()<dequeSize))
#else
  if((TBUFhead != TBUFtail)
  &&(HWSerial.availableForWrite()>=SER_BUFSIZE))
#endif
  {
    serialDirectWrite(TBUF[TBUFhead]);
    TBUFhead++;
    if(TBUFhead >= SER_WRITE_BUFSIZE)
      TBUFhead = 0;
  }
}

static int serialOutBufferBytesRemaining()
{
  if(TBUFtail == TBUFhead)
    return SER_WRITE_BUFSIZE-1;
  else
  if(TBUFtail > TBUFhead)
  {
    int used = TBUFtail - TBUFhead;
    return SER_WRITE_BUFSIZE - used -1;
  }
  else
    return TBUFhead - TBUFtail - 1;
}

static void enqueSerialOut(uint8_t c)
{
  debugPrintf("q\n");
  TBUF[TBUFtail] = c;
  TBUFtail++;
  if(TBUFtail >= SER_WRITE_BUFSIZE)
    TBUFtail = 0;
}

static void clearSerialOutBuffer()
{
  TBUFtail=TBUFhead;
}

static void ensureSerialBytes(int num)
{
  if(serialOutBufferBytesRemaining()<1)
  {
    serialOutDeque();
    while(serialOutBufferBytesRemaining()<1)
      yield();
  }
}

static void flushSerial()
{
  while(TBUFtail != TBUFhead)
  {
    serialOutDeque();
    yield();
  }
  hwSerialFlush();
}

ZSerial::ZSerial()
{
}

void ZSerial::setPetsciiMode(bool petscii)
{
  petsciiMode = petscii;
}

bool ZSerial::isPetsciiMode()
{
  return petsciiMode;
}

void ZSerial::setFlowControlType(FlowControlType type)
{
  flowControlType = type;
#ifdef ZIMODEM_ESP32
  if(flowControlType == FCT_RTSCTS)
  {
    //uart_set_hw_flow_ctrl(UART_NUM_2,UART_HW_FLOWCTRL_DISABLE,0);
    uart_set_hw_flow_ctrl(UART_NUM_2,UART_HW_FLOWCTRL_CTS_RTS,SER_BUFSIZE);
  }
  else
    uart_set_hw_flow_ctrl(UART_NUM_2,UART_HW_FLOWCTRL_DISABLE,0);
#endif
}

FlowControlType ZSerial::getFlowControlType()
{
  return flowControlType;
}

void ZSerial::setXON(bool isXON)
{
  XON_STATE = isXON;
}

bool ZSerial::isXON()
{
  return XON_STATE;
}

bool ZSerial::isSerialOut()
{
  switch(flowControlType)
  {
  case FCT_RTSCTS:
    if(pinSupport[pinCTS])
    {
      //debugPrintf("CTS: pin %d (%d == %d)\n",pinCTS,digitalRead(pinCTS),ctsActive);
      return (digitalRead(pinCTS) == ctsActive);
    }
    return true;
  case FCT_NORMAL:
  case FCT_AUTOOFF:
  case FCT_MANUAL:
    break;
  case FCT_DISABLED:
    return true;
  case FCT_INVALID:
    return true;
  }
  return XON_STATE;
}

bool ZSerial::isSerialCancelled()
{
  if(flowControlType == FCT_RTSCTS)
  {
    if(pinSupport[pinCTS])
      return (digitalRead(pinCTS) == ctsInactive);
  }
  return false;
}

bool ZSerial::isSerialHalted()
{
  return !isSerialOut();
}

void ZSerial::enqueByte(uint8_t c)
{
  if(TBUFtail == TBUFhead)
  {
    switch(flowControlType)
    {
    case FCT_DISABLED:
    case FCT_INVALID:
      serialDirectWrite(c);
      return;
    case FCT_RTSCTS:
#ifdef ZIMODEM_ESP32
      if(isSerialOut())
#else
      if((HWSerial.availableForWrite() >= SER_BUFSIZE)
      &&(isSerialOut()))
#endif
      {
        serialDirectWrite(c);
        return;
      }
      break;
    case FCT_NORMAL:
    case FCT_AUTOOFF:
    case FCT_MANUAL:
      if((HWSerial.availableForWrite() >= SER_BUFSIZE)
      &&(HWSerial.available() == 0)
      &&(XON_STATE))
      {
        serialDirectWrite(c);
        return;
      }
      break;
    }
  }
  enqueSerialOut(c);
}

void ZSerial::prints(const char *expr)
{
  if(!petsciiMode)
  {
    for(int i=0;expr[i]!=0;i++)
    {
      enqueByte(expr[i]);
    }
  }
  else
  {
    for(int i=0;expr[i]!=0;i++)
    {
      enqueByte(ascToPetcii(expr[i]));
    }
  }
}

void ZSerial::printi(int i)
{
  char buf[12];
  prints(itoa(i, buf, 10));
}

void ZSerial::printd(double f)
{
  char buf[12];
  prints(dtostrf(f, 2, 2, buf));
}

void ZSerial::printc(const char c)
{
  if(!petsciiMode)
    enqueByte(c);
  else
    enqueByte(ascToPetcii(c));
}

void ZSerial::printc(uint8_t c)
{
  if(!petsciiMode)
    enqueByte(c);
  else
    enqueByte(ascToPetcii(c));
}

void ZSerial::printb(uint8_t c)
{
  enqueByte(c);
}

size_t ZSerial::write(uint8_t c)
{
  enqueByte(c);
  return 1;
}

void ZSerial::prints(String str)
{
  prints(str.c_str());
}


void ZSerial::printf(const char* format, ...) 
{
  int ret;
  va_list arglist;
  va_start(arglist, format);
  vsnprintf(FBUF, sizeof(FBUF), format, arglist);
  prints(FBUF);
  va_end(arglist);
}

void ZSerial::flushAlways()
{
  while(TBUFtail != TBUFhead)
  {
    hwSerialFlush();
    serialOutDeque();
    yield();
    delay(1);
  }
  hwSerialFlush();
}

void ZSerial::flush()
{
  while((TBUFtail != TBUFhead) && (isSerialOut()))
  {
    hwSerialFlush();
    serialOutDeque();
    yield();
    delay(1);
  }
  hwSerialFlush();
}

int ZSerial::availableForWrite()
{
  return serialOutBufferBytesRemaining();
}

char ZSerial::drainForXonXoff()
{
  char ch = '\0';
  while(HWSerial.available()>0)
  {
    ch=HWSerial.read();
    logSerialIn(ch);
    if(ch == 3)
      break;
    switch(flowControlType)
    {
      case FCT_NORMAL:
        if((!XON_STATE) && (ch == 17))
          XON_STATE=true;
        else
        if((XON_STATE) && (ch == 19))
          XON_STATE=false;
        break;
      case FCT_AUTOOFF:
      case FCT_MANUAL:
        if((!XON_STATE) && (ch == 17))
          XON_STATE=true;
        else
          XON_STATE=false;
        break;
      case FCT_INVALID:
        break;
      case FCT_RTSCTS:
        break;
    }
  }
  return ch;
}

int ZSerial::available() 
{ 
  int avail = HWSerial.available();
  if(avail == 0)
  {
      if((TBUFtail != TBUFhead) && isSerialOut())
        serialOutDeque();
  }
  return avail;
}

int ZSerial::read() 
{ 
  int c=HWSerial.read();
  if(c == -1)
  {
    if((TBUFtail != TBUFhead) && isSerialOut())
      serialOutDeque();
  }
  return c;
}

int ZSerial::peek() 
{ 
  return HWSerial.peek(); 
}
