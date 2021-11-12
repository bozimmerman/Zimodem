#include <FS.h>

class HostCM
{
private:
  ZSerial hserial;
  struct _HCOpts
  {
    unsigned int  speed = 15; //B9600
    unsigned int  parity;
    unsigned int  stopb=0;
    unsigned char lineend=0xd;
    unsigned char prompt=0x11;
    unsigned char response=0x13;
    unsigned char ext=0;
  } opt;

  bool aborted = false;
  unsigned long lastNonPlusTm = 0;
  unsigned int plussesInARow = 0;
  unsigned long plusTimeExpire = 0;

  void checkDoPlusPlusPlus(int c, const unsigned long tm);
  bool checkPlusPlusPlusExpire(const unsigned long tm);
  int receive(char *buf, int sz);
};
