#include <FS.h>

class KModem
{
private:
  static const int MAXPACKSIZ = 94; /* Maximum packet size */
  static const int MAXTRY = 20;     /* Times to retry a packet */
  static const int MYTIME = 10;     /* (10) Seconds after which I should be timed out */
  static const int MAXTIM = 60;     /* (60) Maximum timeout interval */
  static const int MINTIM = 2;      /* (2) Minumum timeout interval */
  static const char MYQUOTE ='#';   /* Quote character I will use */
  static const int MYPAD = 0;       /* Number of padding characters I will need */
  static const int MYPCHAR = 0;     /* Padding character I need (NULL) */
  static const char MYEOL = '\n';   /* End-Of-Line character I need */
  static const char SOH = 1;        /* Start of header */
  static const char CR = 13;        /* ASCII Carriage Return */
  static const char SP = 32;        /* ASCII space */
  static const char DEL = 127;      /* Delete (rubout) */
  static const char ESCCHR = '^';   /* Default escape character for CONNECT */
  static const char NUL = '\0';     /* Null character */
  static const char FALSE = 0;
  static const char TRUE = -1;

  int size=0,              /* Size of present data */
      rpsiz=0,             /* Maximum receive packet size */
      spsiz=0,             /* Maximum send packet size */
      pad=0,               /* How much padding to send */
      timint=0,            /* Timeout for foreign host on sends */
      n=0,                 /* Packet number */
      numtry=0,            /* Times this packet retried */
      oldtry=0,            /* Times previous packet retried */
      image=1,             /* -1 means 8-bit mode */
      debug=99,            /* indicates level of debugging output (0=none) */
      filecount=0,         /* Number of files left to send */
      filenum=0,
      mflg=0,              /* Flag for MacKermit mode */
      xflg=0;              /* flag for xmit directory structure */
  char state,              /* Present state of the automaton */
       padchar,            /* Padding character to send */
       eol,                /* End-Of-Line character to send */
       escchr,             /* Connect command escape character */
       quote,              /* Quote character in incoming data */
       *filnam,            /* Current file name */
       *filnamo,           /* File name sent */
       *ttyline,           /* Pointer to tty line */
       recpkt[MAXPACKSIZ], /* Receive packet buffer */
       packet[MAXPACKSIZ], /* Packet buffer */
       ldata[1024];        /* First line of data to send over connection */
  String **filelist = 0;
  int  (*recvChar)(ZSerial *ser, int);
  void (*sendChar)(ZSerial *ser, char);
  bool (*dataHandler)(File *kfp, unsigned long number, char *buffer, int len);

  void flushinput();
  void rpar(char data[]);
  void spar(char data[]);
  int gnxtfl();
  void bufemp(char buffer[], int len);
  void prerrpkt(char *msg);
  int bufill(char buffer[]);
  int rpack(int *len, int *num, char *data);
  int spack(char type, int num, int len, char *data);
  char rdata();
  char rinit();
  char rfile();
  char sfile();
  char sinit();
  char sdata();
  char sbreak();
  char seof();

  bool kfpClosed = true;
  String *errStr = 0;
public:
  String rootpath = "";
  FS *kfileSystem = &SD;
  File kfp;
  ZSerial kserial;

  KModem(FlowControlType commandFlow,
         int (*recvChar)(ZSerial *ser, int),
         void (*sendChar)(ZSerial *ser, char),
         bool (*dataHandler)(File *kfp, unsigned long, char*, int),
         String &errors);
  void setTransmitList(String **fileList, int numFiles);
  bool receive();
  bool transmit();
};

static int kReceiveSerial(ZSerial *ser, int del)
{
  unsigned long end=millis() + (del * 1000L);
  while(millis() < end)
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

static void kSendSerial(ZSerial *ser, char c)
{
  ser->write((uint8_t)c);
  ser->flush();
}

static bool kUDataHandler(File *kfp, unsigned long number, char *buf, int sz)
{
  for(int i=0;i<sz;i++)
    kfp->write((uint8_t)buf[i]);
  return true;
}

static bool kDDataHandler(File *kfp, unsigned long number, char *buf, int sz)
{
  for(int i=0;i<sz;i++)
  {
    int c=kfp->read();
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

static bool kDownload(FlowControlType commandFlow, FS &fs, String **fileList, int fileCount, String &errors)
{
  KModem kmo(commandFlow, kReceiveSerial, kSendSerial, kDDataHandler, errors);
  kmo.kfileSystem = &fs;
  kmo.setTransmitList(fileList,fileCount);
  bool result = kmo.transmit();
  return result;
}

static bool kUpload(FlowControlType commandFlow, FS &fs, String rootPath, String &errors)
{
  KModem kmo(commandFlow, kReceiveSerial, kSendSerial, kUDataHandler, errors);
  kmo.kfileSystem = &fs;
  kmo.rootpath = rootPath;
  bool result = kmo.receive();
  return result;
}


