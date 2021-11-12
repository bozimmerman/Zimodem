#include "proto_hostcm.h"


void HostCM::checkDoPlusPlusPlus(int c, const unsigned long tm)
{
  if(c == '+')
  {
      if((plussesInARow>0)||((tm-lastNonPlusTm)>800))
      {
        plussesInARow++;
        if(plussesInARow > 2)
          plusTimeExpire = tm + 800;
      }
  }
  else
  {
    plusTimeExpire = 0;
    lastNonPlusTm = tm;
    plussesInARow = 0;
  }
}

bool HostCM::checkPlusPlusPlusExpire(const unsigned long tm)
{
  if((plusTimeExpire>0)&&(tm>plusTimeExpire)&&(plussesInARow>2))
  {
    aborted = true;
    plusTimeExpire = 0;
    lastNonPlusTm = tm;
    plussesInARow = 0;
    return true;
  }
  return false;
}


int HostCM::receive(char *buf, int sz)
{
  int c, t = 0;
  unsigned long lastC = 0;
  unsigned long tm = millis();
  if(aborted)
    return -2;

  do
  {
    tm = millis();
    if((tm - lastC) > 1000) // the timeout
      return -1;
    serialOutDeque();
    if(hserial.available() > 0)
    {
      c=hserial.read();
      logSerialIn(c);
      buf[t++]=c;
      checkDoPlusPlusPlus(c,tm);
      lastC = tm;
    }
    if(checkPlusPlusPlusExpire(tm))
      return -2;
  } while ((t < sz) && (buf[t-1] != opt.lineend));

  return(t-1);
}
