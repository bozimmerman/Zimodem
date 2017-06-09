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

#define DBG_BYT_CTR 20

#define SER_WRITE_BUFSIZE 4096

static int SER_BUFSIZE = 128;
static uint8_t TBUF[SER_WRITE_BUFSIZE];
static char FBUF[256];
static int TBUFhead=0;
static int TBUFtail=0;
static int serialDelayMs = 0;

static void serialDirectWrite(uint8_t c);
static void serialOutDeque();
static int serialOutBufferBytesRemaining();
static void enqueSerialOut(uint8_t c);
static void clearSerialOutBuffer();

class ZSerial
{
  private:
    bool petsciiMode = false;
  public:
    ZSerial();
    void setPetsciiMode(bool petscii);
    bool isPetsciiMode();
    
    void prints(const char *expr);
    void printc(const char c);
    void printc(uint8_t c);
    void printb(uint8_t c);
    void printd(double f);
    void printi(int i);
    void printf(const char* format, ...);
    void flush();
    int availableForWrite();
};

