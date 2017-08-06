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

static void serialDirectWrite(uint8_t c)
{
  Serial.write(c);
  if(serialDelayMs > 0)
    delay(serialDelayMs);
  logSerialOut(c);
}

static void serialOutDeque()
{
  if((TBUFhead != TBUFtail)&&(Serial.availableForWrite()>=SER_BUFSIZE))
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
  Serial.flush();
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
}

FlowControlType ZSerial::getFlowControlType()
{
  return flowControlType;
}

void ZSerial::setXON(bool isXON)
{
  XON = isXON;
}

bool ZSerial::isXON()
{
  return XON;
}

bool ZSerial::isSerialOut()
{
  switch(flowControlType)
  {
  case FCT_RTSCTS:
    //if(enableRtsCts)
    return (digitalRead(pinCTS) == ctsActive);
    //return true;
  case FCT_NORMAL:
  case FCT_AUTOOFF:
  case FCT_MANUAL:
    break;
  case FCT_DISABLED:
    return true;
  case FCT_INVALID:
    return true;
  }
  return XON;
}

bool ZSerial::isSerialCancelled()
{
  if(flowControlType == FCT_RTSCTS)
  {
    //if(enableRtsCts)
    return (digitalRead(pinCTS) == ctsInactive);
  }
  return false;
}

bool ZSerial::isSerialHalted()
{
  return !isSerialOut();
}


void ZSerial::prints(const char *expr)
{
  if(!petsciiMode)
  {
    for(int i=0;expr[i]!=0;i++)
    {
      enqueSerialOut(expr[i]);
    }
  }
  else
  {
    for(int i=0;expr[i]!=0;i++)
    {
      enqueSerialOut(ascToPetcii(expr[i]));
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
    enqueSerialOut(c);
  else
    enqueSerialOut(ascToPetcii(c));
}

void ZSerial::printc(uint8_t c)
{
  if(!petsciiMode)
    enqueSerialOut(c);
  else
    enqueSerialOut(ascToPetcii(c));
}

void ZSerial::printb(uint8_t c)
{
  enqueSerialOut(c);
}

void ZSerial::write(uint8_t c)
{
  enqueSerialOut(c);
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
  vsprintf(FBUF, format, arglist);
  prints(FBUF);
  va_end(arglist);
}

void ZSerial::flushAlways()
{
  while(TBUFtail != TBUFhead)
  {
    Serial.flush();
    serialOutDeque();
    yield();
    delay(1);
  }
  Serial.flush();
}

void ZSerial::flush()
{
  while((TBUFtail != TBUFhead) && (isSerialOut()))
  {
    Serial.flush();
    serialOutDeque();
    yield();
    delay(1);
  }
  Serial.flush();
}

int ZSerial::availableForWrite()
{
  return serialOutBufferBytesRemaining();
}

char ZSerial::drainForXonXoff()
{
  char ch = '\0';
  while(Serial.available()>0)
  {
    ch=Serial.read();
    logSerialIn(ch);
    if(ch == 3)
      break;
    switch(flowControlType)
    {
      case FCT_NORMAL:
        if((!XON) && (ch == 17))
          XON=true;
        else
        if((XON) && (ch == 19))
          XON=false;
        break;
      case FCT_AUTOOFF:
      case FCT_MANUAL:
        if((!XON) && (ch == 17))
          XON=true;
        else
          XON=false;
        break;
      case FCT_INVALID:
        break;
      case FCT_RTSCTS:
        break;
    }
  }
  return ch;
}

