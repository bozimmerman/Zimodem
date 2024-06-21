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
  &&((SER_BUFSIZE - HWSerial.availableForWrite())<dequeSize))
#else
  if((TBUFhead != TBUFtail)
  &&(HWSerial.availableForWrite()>=SER_BUFSIZE)) // necessary for esp8266 flow control
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
    uart_set_hw_flow_ctrl(MAIN_UART_NUM,UART_HW_FLOWCTRL_DISABLE,0);
    uint32_t invertMask = 0;
    if(pinSupport[pinCTS])
    {
      uart_set_pin(MAIN_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, /*cts_io_num*/pinCTS);
      // cts is input to me, output to true RS232
      if(ctsActive == HIGH)
#       ifdef UART_INVERSE_CTS
          invertMask = invertMask | UART_INVERSE_CTS;
#       else
          invertMask = invertMask | UART_SIGNAL_CTS_INV;
#       endif
    }
    if(pinSupport[pinRTS])
    {
      uart_set_pin(MAIN_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, /*rts_io_num*/ pinRTS, UART_PIN_NO_CHANGE);
      s_pinWrite(pinRTS, rtsActive);
      // rts is output to me, input to true RS232
      if(rtsActive == HIGH)
#       ifdef UART_INVERSE_RTS
          invertMask = invertMask | UART_INVERSE_RTS;
#       else
          invertMask = invertMask | UART_SIGNAL_RTS_INV;
#       endif
    }
    //debugPrintf("invert = %d magic values = %d %d, RTS_HIGH=%d, RTS_LOW=%d HIGHHIGH=%d LOWLOW=%d\r\n",
    //            invertMask,ctsActive,rtsActive, DEFAULT_RTS_ACTIVE, DEFAULT_RTS_INACTIVE, HIGH, LOW);
    if(invertMask != 0)
      uart_set_line_inverse(MAIN_UART_NUM, invertMask);
    const int CUTOFF = 100;
    if(pinSupport[pinRTS])
    {
      if(pinSupport[pinCTS])
        uart_set_hw_flow_ctrl(MAIN_UART_NUM,UART_HW_FLOWCTRL_CTS_RTS,CUTOFF);
      else
        uart_set_hw_flow_ctrl(MAIN_UART_NUM,UART_HW_FLOWCTRL_CTS_RTS,CUTOFF);
    }
    else
    if(pinSupport[pinCTS])
      uart_set_hw_flow_ctrl(MAIN_UART_NUM,UART_HW_FLOWCTRL_CTS_RTS,CUTOFF);
  }
  else
    uart_set_hw_flow_ctrl(MAIN_UART_NUM,UART_HW_FLOWCTRL_DISABLE,0);
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

int ZSerial::getConfigFlagBitmap()
{
  return 
       (isPetsciiMode()?FLAG_PETSCII:0)
      |((getFlowControlType()==FCT_RTSCTS)?FLAG_RTSCTS:0)
      |((getFlowControlType()==FCT_NORMAL)?FLAG_XONXOFF:0)
      |((getFlowControlType()==FCT_AUTOOFF)?FLAG_XONXOFF:0)
      |((getFlowControlType()==FCT_MANUAL)?FLAG_XONXOFF:0);
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
      //debugPrintf("CTS: pin %d (%d == %d)\r\n",pinCTS,digitalRead(pinCTS),ctsActive);
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
    {
      //debugPrintf("CTS: pin %d (%d == %d)\r\n",pinCTS,digitalRead(pinCTS),ctsActive);
      return (digitalRead(pinCTS) == ctsInactive);
    }
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
#ifndef ZIMODEM_ESP32
      if((HWSerial.availableForWrite() > 0)
      &&(HWSerial.available() == 0))
#endif
      {
        serialDirectWrite(c);
        return;
      }
      break;
    case FCT_RTSCTS:
#ifdef ZIMODEM_ESP32
      if(isSerialOut())
#else
      if((HWSerial.availableForWrite() >= SER_BUFSIZE) // necessary for esp8266 flow control
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
  // the car jam of blocked bytes stops HERE
  //debugPrintf("%d\r\n",serialOutBufferBytesRemaining());
  while(serialOutBufferBytesRemaining()<1)
  {
    if(!isSerialOut())
      delay(1);
    else
      serialOutDeque();
    yield();
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

size_t ZSerial::write(uint8_t *buf, int bufSz)
{
  for(int i=0;i<bufSz;i++)
    enqueByte(buf[i]);
  return bufSz;
}

void ZSerial::prints(String str)
{
  prints(str.c_str());
}


void ZSerial::printf(const char* format, ...) 
{
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
  else
    logSerialIn(c);
  return c;
}

int ZSerial::peek() 
{ 
  return HWSerial.peek(); 
}
