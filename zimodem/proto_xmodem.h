/*
   Copyright 2018-2024 Bo Zimmerman

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
typedef bool (*DataHandler)(File *xfile, unsigned long, char*, int);

class XModem 
{
  typedef enum
  {
    Crc,
    ChkSum  
  } transfer_t;
  
  private:
    size_t fileSizeLimit = 10000000;
    //holds readed byte (due to dataAvail())
    int byte;
    //expected block number
    unsigned char blockNo;
    //extended block number, send to dataHandler()
    unsigned long blockNoExt;
    //retry counter for NACK
    int retries;
    //buffer
    char buffer[1024];
    //repeated block flag
    bool repeatedBlock;

    ZSerial xserial;

    int  (*recvChar)(ZSerial *ser, int);
    void (*sendChar)(ZSerial *ser, char);
    bool (*dataHandler)(File *xfile, unsigned long number, char *buffer, int len);
    unsigned short crc16_ccitt(char *buf, int size);
    bool serialAvail(int delay);
    int serialRead(int delay);
    void serialWrite(char symbol);
    bool receiveFrameNo(void);
    bool receiveData(int blkSize);
    bool checkCrc(int blkSize);
    bool checkChkSum(void);
    bool receiveFrames(transfer_t transfer);
    bool sendNack(void);
    void init(void);
    
    bool transmitFrame(transfer_t transfer, int blkSize);
    bool transmitFrames(transfer_t);
    unsigned char generateChkSum(void);

  protected:
    FS *fileSystem = null;
    File yfile;
    File *xfile = null;
    File *dfile = null;
    int blockSize = 128;
    bool send0block = false;
    
  public:
    static const unsigned char XMO_NACK = 21;
    static const unsigned char XMO_ACK =  6;
    static const unsigned char XMO_CRC = 'C';

    static const unsigned char XMO_SOH =  1;
    static const unsigned char XMO_STX =  2;
    static const unsigned char XMO_EOT =  4;
    static const unsigned char XMO_CAN =  0x18;

    static const int receiveFrameDelay=7000;
    static const int receiveByteDelay=3000;
    static const int rcvRetryLimit = 10;

    size_t fileSize = 0;

    XModem(File &f, FlowControlType commandFlow, RecvChar recvChar, SendChar sendChar, DataHandler dataHandler);
    bool receive();
    bool transmit();
};

class YModem : public XModem
{
  public:
    YModem(FS *fileSystem, File &f, FlowControlType commandFlow, RecvChar recvChar, SendChar sendChar, DataHandler dataHandler);
};

static int xReceiveSerial(ZSerial *ser, int del)
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

static void xSendSerial(ZSerial *ser, char c)
{
  ser->write((uint8_t)c);
  ser->flush();
}

static bool xUDataHandler(File *xfile, unsigned long number, char *buf, int sz)
{
  for(int i=0;i<sz;i++)
    xfile->write((uint8_t)buf[i]);
  return true;
}

static bool xDDataHandler(File *xfile, unsigned long number, char *buf, int sz)
{
  for(int i=0;i<sz;i++)
  {
    int c=xfile->read();
    if(c<0)
    {
      if(i==0)
        return false;
      buf[i] = (char)26;
    }
    else
      buf[i] = (char)c;
  }
  return true;  
}

static bool xDownload(FlowControlType commandFlow, File &f, String &errors)
{
  XModem xmo(f, commandFlow, xReceiveSerial, xSendSerial, xDDataHandler);
  bool result = xmo.transmit();
  return result;
}

static bool xUpload(FlowControlType commandFlow, File &f, String &errors)
{
  XModem xmo(f, commandFlow, xReceiveSerial, xSendSerial, xUDataHandler);
  bool result = xmo.receive();
  return result;
}

static bool yDownload(FS *fileSystem, FlowControlType commandFlow, File &f, String &errors)
{
  YModem ymo(fileSystem, f, commandFlow, xReceiveSerial, xSendSerial, xDDataHandler);
  ymo.fileSize = f.size();  // multi-upload will need this
  bool result = ymo.transmit();
  return result;
}

static bool yUpload(FS *fileSystem, FlowControlType commandFlow, File &dirF, String &errors)
{
  YModem ymo(fileSystem, dirF, commandFlow, xReceiveSerial, xSendSerial, xUDataHandler);
  bool result = ymo.receive();
  return result;
}
