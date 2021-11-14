/*
   Copyright 2018-2019 Bo Zimmerman

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

class XModem 
{
  typedef enum
  {
    Crc,
    ChkSum  
  } transfer_t;
  
  private:
    //holds readed byte (due to dataAvail())
    int byte;
    //expected block number
    unsigned char blockNo;
    //extended block number, send to dataHandler()
    unsigned long blockNoExt;
    //retry counter for NACK
    int retries;
    //buffer
    char buffer[128];
    //repeated block flag
    bool repeatedBlock;
    File *xfile = null;
    ZSerial xserial;

    int  (*recvChar)(ZSerial *ser, int);
    void (*sendChar)(ZSerial *ser, char);
    bool (*dataHandler)(File *xfile, unsigned long number, char *buffer, int len);
    unsigned short crc16_ccitt(char *buf, int size);
    bool dataAvail(int delay);
    int dataRead(int delay);
    void dataWrite(char symbol);
    bool receiveFrameNo(void);
    bool receiveData(void);
    bool checkCrc(void);
    bool checkChkSum(void);
    bool receiveFrames(transfer_t transfer);
    bool sendNack(void);
    void init(void);
    
    bool transmitFrames(transfer_t);
    unsigned char generateChkSum(void);
    
  public:
    static const unsigned char XMO_NACK = 21;
    static const unsigned char XMO_ACK =  6;

    static const unsigned char XMO_SOH =  1;
    static const unsigned char XMO_EOT =  4;
    static const unsigned char XMO_CAN =  0x18;

    static const int receiveDelay=7000;
    static const int rcvRetryLimit = 10;

  
    XModem(File &f,
           FlowControlType commandFlow,
           int (*recvChar)(ZSerial *ser, int),
           void (*sendChar)(ZSerial *ser, char),
           bool (*dataHandler)(File *xfile, unsigned long, char*, int));
    bool receive();
    bool transmit();
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
      logSerialIn(c);
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

static boolean xDownload(FlowControlType commandFlow, File &f, String &errors)
{
  XModem xmo(f,commandFlow, xReceiveSerial, xSendSerial, xDDataHandler);
  bool result = xmo.transmit();
  return result;
}

static boolean xUpload(FlowControlType commandFlow, File &f, String &errors)
{
  XModem xmo(f,commandFlow, xReceiveSerial, xSendSerial, xUDataHandler);
  bool result = xmo.receive();
  return result;
}
