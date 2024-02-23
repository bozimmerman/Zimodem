/*
   Copyright 2024-2024 Bo Zimmerman

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
#include <FS.h>

typedef int (*RecvChar)(ZSerial *ser, int);
typedef void (*SendChar)(ZSerial *ser, char);
typedef bool (*DataHandler)(File *pfile, unsigned long, char*, int);

enum PunterCode
{
  PUNTER_GOO,
  PUNTER_BAD,
  PUNTER_ACK,
  PUNTER_SYN,
  PUNTER_SNB,
  PUNTER_TIMEOUT
};

static int PUNTER_HDR_SIZE = 7;
static int PUNTER_HDR_CHK1LB = 0;
static int PUNTER_HDR_CHK1HB = 1;
static int PUNTER_HDR_CHK2LB = 2;
static int PUNTER_HDR_CHK2HB = 3;
static int PUNTER_HDR_NBLKSZ = 4;
static int PUNTER_HDR_BLKNLB = 5;
static int PUNTER_HDR_BLKNHB = 6;

class Punter
{
  private:
    ZSerial pserial;
    uint8_t buf[300];
    int bufSize = 0;

    int  (*recvChar)(ZSerial *ser, int);
    void (*sendChar)(ZSerial *ser, char);
    bool (*dataHandler)(File *pfile, unsigned long number, char *buffer, int len);

    int serialRead(int delay);
    void serialWrite(char symbol);

    PunterCode readPunterCode(int waitTime);
    bool readPunterCode(PunterCode code, int waitTime);
    void sendPunterCode(PunterCode code);
    bool exchangePunterCodes(PunterCode sendCode, PunterCode expectCode, int tries);

    void calcBufChecksums(uint16_t* chks);
    bool sendBufBlock(unsigned int blkNum, unsigned int nextBlockSize, int tries);
    bool receiveBlock(int size, int tries);

  protected:
    FS *fileSystem = null;
    File *pfile = null;

  public:
    static const int receiveFrameDelay=4000;
    static const int receiveByteDelay=2000;
    static const int retryLimit = 10;


    Punter(File &f, FlowControlType commandFlow, RecvChar recvChar, SendChar sendChar, DataHandler dataHandler);
    bool receive();
    bool transmit();
    bool receiveBlock(int size);
};

static int pReceiveSerial(ZSerial *ser, int del)
{
  unsigned long end=micros() + (del * 1000L);
  while(micros() < end)
  {
    serialOutDeque();
    if(ser->available() > 0)
    {
      int c=ser->read();
      return c;
    }
    yield();
  }
  return -1;
}

static void pSendSerial(ZSerial *ser, char c)
{
  ser->write((uint8_t)c);
  ser->flush();
}

static bool pUDataHandler(File *pfile, unsigned long number, char *buf, int sz)
{
  for(int i=0;i<sz;i++)
    pfile->write((uint8_t)buf[i]);
  return true;
}

static bool pDDataHandler(File *pfile, unsigned long number, char *buf, int sz)
{
  for(int i=0;i<sz;i++)
  {
    int c=pfile->read();
    if(c<0)
      return false;
    else
      buf[i] = (uint8_t)c;
  }
  return true;
}

static bool pDownload(FlowControlType commandFlow, File &f, String &errors)
{
  Punter c1(f, commandFlow, pReceiveSerial, pSendSerial, pDDataHandler);
  bool result = c1.transmit();
  return result;
}

static bool pUpload(FlowControlType commandFlow, File &f, String &errors)
{
  Punter c1(f, commandFlow, pReceiveSerial, pSendSerial, pUDataHandler);
  bool result = c1.receive();
  return result;
}
