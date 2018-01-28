#include <Fs.h>

class XModem 
{
  typedef enum {
    Crc,
    ChkSum  
  } transfer_t;
  
  private:
    //delay when receive bytes in frame - 7 secs
    static const int receiveDelay;
    //retry limit when receiving
    static const int rcvRetryLimit;
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

    int  (*recvChar)(int);
    void (*sendChar)(char);
    bool (*dataHandler)(unsigned long number, char *buffer, int len);
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
    static const unsigned char NACK;
    static const unsigned char ACK;
    static const unsigned char SOH;
    static const unsigned char EOT;
    static const unsigned char CAN;
  
    XModem(int (*recvChar)(int), void (*sendChar)(char));
    XModem(int (*recvChar)(int), void (*sendChar)(char), 
                bool (*dataHandler)(unsigned long, char*, int));
    bool receive();
    bool transmit();
};

static File *xfile = null;
static ZSerial xserial;

static int xReceiveSerial(int del)
{
  for(int i=0;i<del;i++)
  {
    if(HWSerial.available() > 0)
      return HWSerial.read();
    delay(1);
  }
  return -1;
}

static void xSendSerial(char c)
{
  xserial.write((uint8_t)c);
}

static bool xUDataHandler(unsigned long number, char *buf, int sz)
{
  for(int i=0;i<sz;i++)
    xfile->write((uint8_t)buf[i]);
  return true;
}

static bool xDDataHandler(unsigned long number, char *buf, int sz)
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

static boolean xDownload(File &f, String &errors)
{
  xfile = &f;
  XModem xmo(xReceiveSerial, xSendSerial, xDDataHandler);
  xfile = null;
  return xmo.transmit();
}

static boolean xUpload(File &f, String &errors)
{
  xfile = &f;
  XModem xmo(xReceiveSerial, xSendSerial, xUDataHandler);
  xfile = null;
  return xmo.receive();
}


