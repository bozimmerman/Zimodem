/* Converted from source reverse engineered from SP9000 roms by Rob Ferguson */
#include <FS.h>

class HostCM
{
private:
  ZSerial hserial;
  const struct _HCOpts
  {
    unsigned int  speed = 15; //B9600
    unsigned int  parity;
    unsigned int  stopb=0;
    unsigned char lineend=0xd;
    unsigned char prompt=0x11;
    unsigned char response=0x13;
    unsigned char ext=0;
  } opt PROGMEM;

# define HCM_BUFSIZ  128
  const char *sumchar PROGMEM = "ABCDEFGHIJKLMNOP";

  uint8_t lastOutBuf[HCM_BUFSIZ];
  int lastOutSize = 0;

  uint8_t inbuf[HCM_BUFSIZ];
  int pkti = 0;

  bool aborted = false;
  unsigned long lastNonPlusTm = 0;
  unsigned int plussesInARow = 0;
  unsigned long plusTimeExpire = 0;

  char checksum(uint8_t *b, int n);
  void checkDoPlusPlusPlus(const int c, const unsigned long tm);
  bool checkPlusPlusPlusExpire(const unsigned long tm);
  void sendNAK();
  void sendACK();
public:
  void receiveLoop();
  bool isAborted();
};
