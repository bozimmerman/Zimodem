#include "proto_hostcm.h"


void HostCM::checkDoPlusPlusPlus(const int c, const unsigned long tm)
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
  if(aborted)
    return true;
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

void HostCM::receiveLoop()
{

}
bool HostCM::isAborted()
{
  return aborted;
}

char HostCM::checksum(char *b, int n)
{
  int i, s = 0;

  for (i = 0; i < n; i++) {
    s += b[i] & 0xf;
  }
  return (sumchar[s&0xf]);
}

void HostCM::receiveLoop()
{
  serialOutDeque();
  unsigned long tm = millis();
  if(checkPlusPlusPlusExpire(tm))
    return;
  while(hserial.available() > 0)
  {
    c=hserial.read();
    logSerialIn(c);
    if(pkti<BUFSIZ)
      inbuf[pkti++]=c;
    checkDoPlusPlusPlus(c, tm);
    if(checkPlusPlusPlusExpire(tm))
      return;
    yield();
    if(c==opt.lineend)
    {
      inbuf[pkti-1]=c;
      break;
    }
  }
  if((pkti==0)
  ||(inbuf[pkti-1]!=opt.lineend))
    return;
  pkti--;
  if((pkti>1)&&(inbuf[pkti-1]!=checksum(inbuf,pkti-1)))
  {
    //TODO: compose a NAK
  }

  //TODO: process our command!
  switch(inbuf[0])
  {
    default:
      //TODO: compose another NAK!
      break;
  }
  pkti=0; // we are ready for the next packet!
  serialOutDeque();
}
