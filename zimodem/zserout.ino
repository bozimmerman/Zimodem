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
    enqueSerialOut(c);
}

void ZSerial::printc(uint8_t c)
{
  if(!petsciiMode)
    enqueSerialOut(c);
  else
    enqueSerialOut(c);
}

void ZSerial::printb(uint8_t c)
{
  enqueSerialOut(c);
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

void ZSerial::flush()
{
  flushSerial();  
}

int ZSerial::availableForWrite()
{
  return serialOutBufferBytesRemaining();
}


