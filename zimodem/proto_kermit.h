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
      debug=0,             /* indicates level of debugging output (0=none) */
      filnamcnv=0,         /* -1 means do file name case conversions */
      filecount=0,         /* Number of files left to send */
      mflg=0,              /* Flag for MacKermit mode */
      xflg=0,              /* flag for xmit directory structure */
      aflg=0,              /* flag for -a switch filenames in pairs */
      tflg=0;              /* Flag for Tymnet mode */
  char state,              /* Present state of the automaton */
       padchar,            /* Padding character to send */
       eol,                /* End-Of-Line character to send */
       escchr,             /* Connect command escape character */
       quote,              /* Quote character in incoming data */
       **filelist,         /* List of files to be sent */
       *filnam,            /* Current file name */
       *filnamo,           /* File name sent */
       *ttyline,           /* Pointer to tty line */
       recpkt[MAXPACKSIZ], /* Receive packet buffer */
       packet[MAXPACKSIZ], /* Packet buffer */
       ldata[1024];        /* First line of data to send over connection */
  int  (*recvChar)(int);
  void (*sendChar)(char);
  bool (*dataHandler)(unsigned long number, char *buffer, int len);

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
public:
  String rootpath = "";

  KModem(int (*recvChar)(int), void (*sendChar)(char));
  KModem(int (*recvChar)(int), void (*sendChar)(char),
         bool (*dataHandler)(unsigned long, char*, int));
  void setTransmitList(char *fileList[], int numFiles);
  bool receive();
  bool transmit();
};

static File kfp;
static bool kfpClosed = true;
static String *errStr = 0;
static FS *kfileSystem = &SD;
static ZSerial kserial;

static int kReceiveSerial(int del)
{
  unsigned long end=millis() + (del * 1000L);
  while(millis() < end)
  {
    serialOutDeque();
    if(kserial.available() > 0)
    {
      int c=kserial.read();
      logSerialIn(c);
      return c;
    }
    yield();
  }
  return -1;
}

static void kSendSerial(char c)
{
  kserial.write((uint8_t)c);
  kserial.flush();
}

static bool kUDataHandler(unsigned long number, char *buf, int sz)
{
  for(int i=0;i<sz;i++)
    kfp.write((uint8_t)buf[i]);
  return true;
}

static bool kDDataHandler(unsigned long number, char *buf, int sz)
{
  for(int i=0;i<sz;i++)
  {
    int c=kfp.read();
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

static boolean kDownload(FS &fs, String filePath, String &errors)
{
  kfileSystem = &fs;
  errStr = &errors;
  char *list[1];
  list[0] = (char *)filePath.c_str();
  KModem kmo(kReceiveSerial, kSendSerial, kDDataHandler);
  kmo.setTransmitList((char **)list,1);
  bool result = kmo.transmit();
  kserial.flushAlways();
  return result;
}

static boolean kUpload(FS &fs, String rootPath, String &errors)
{
  kfileSystem = &fs;
  errStr = &errors;
  KModem kmo(kReceiveSerial, kSendSerial, kUDataHandler);
  kmo.rootpath = rootPath;
  bool result = kmo.receive();
  kserial.flushAlways();
  return result;
}

static void initKSerial(FlowControlType commandFlow)
{
  kserial.setFlowControlType(FCT_DISABLED);
  if(commandFlow==FCT_RTSCTS)
    kserial.setFlowControlType(FCT_RTSCTS);
  kserial.setPetsciiMode(false);
  kserial.setXON(true);
}

