#ifdef INCLUDE_SD_SHELL
#ifdef INCLUDE_HOSTCM
/* Converted from source reverse engineered from SP9000 roms by Rob Ferguson */
#include <FS.h>

# define HCM_BUFSIZ  104
# define HCM_SENDBUF (208/2 - 6)
# define HCM_FNSIZ 32
# define HCM_MAXFN 16

typedef struct _HCMFile
{
  char descriptor;
  File f;
  uint8_t mode;
  uint8_t format;
  uint8_t type;
  int reclen;
  char filename[HCM_FNSIZ+1];
  struct _HCMFile *nxt;
} HCMFile;

class HostCM
{
private:
  ZSerial hserial;
  const struct _HCOpts
  {
    unsigned int  speed = 15; //B9600
    unsigned int  parity = 0;// ?!
    unsigned int  stopb=0;
    unsigned char lineend=0xd;
    unsigned char prompt=0x11;
    unsigned char response=0x13;
    unsigned char ext=0;
  } opt PROGMEM;

  uint8_t outbuf[HCM_BUFSIZ];
  int odex = 0;

  uint8_t inbuf[HCM_BUFSIZ+1];
  int idex = 0;

  bool aborted = false;
  unsigned long lastNonPlusTm = 0;
  unsigned int plussesInARow = 0;
  unsigned long plusTimeExpire = 0;
  HCMFile files[HCM_MAXFN];
  FS *hFS = &SD;
  File openDirF = (File)0;
  File renameF = (File)0;

  char checksum(uint8_t *b, int n);
  void checkDoPlusPlusPlus(const int c, const unsigned long tm);
  bool checkPlusPlusPlusExpire(const unsigned long tm);
  void sendNAK();
  void sendACK();
  void sendError(const char* format, ...);
  bool closeAllFiles();
  HCMFile *addNewFileEntry();
  void delFileEntry(HCMFile *e);
  HCMFile *getFileByDescriptor(char c);
  int numOpenFiles();

  void protoOpenFile();
  void protoCloseFile();
  void protoPutToFile();
  void protoGetFileBytes();
  void protoOpenDir();
  void protoNextDirFile();
  void protoCloseDir();
  void protoSetRenameFile();
  void protoFinRenameFile();
  void protoEraseFile();
  void protoSeekFile();

public:
  void receiveLoop();
  bool isAborted();
  HostCM(FS *fs);
  ~HostCM();
};
#endif
#endif
