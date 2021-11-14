/* Converted from source reverse engineered from SP9000 roms by Rob Ferguson */
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

bool HostCM::isAborted()
{
  return aborted;
}

char HostCM::checksum(uint8_t *b, int n)
{
  int i, s = 0;

  for (i = 0; i < n; i++) {
    s += b[i] & 0xf;
  }
  return (sumchar[s&0xf]);
}

void HostCM::sendNAK()
{
  lastOutSize = 0;
  lastOutBuf[lastOutSize++] = opt.response;
  lastOutBuf[lastOutSize++] = 'N';

  /* NAK does not to include checksum */  
/*      outbuf[count++] = checksum(&(outbuf[1]), 1);  */ 

  lastOutBuf[lastOutSize++] = opt.lineend;
  lastOutBuf[lastOutSize++] = opt.prompt;
  hserial.write(lastOutBuf, lastOutSize);
  hserial.flush();
}

void HostCM::sendACK()
{
  lastOutSize = 0;
  lastOutBuf[lastOutSize++] = opt.response;
  lastOutBuf[lastOutSize++] = 'b';
  lastOutBuf[lastOutSize++] = checksum(&(lastOutBuf[1]), 1);
  lastOutBuf[lastOutSize++] = opt.lineend;
  lastOutBuf[lastOutSize++] = opt.prompt;
  hserial.write(lastOutBuf, lastOutSize);
  hserial.flush();
}

void HostCM::receiveLoop()
{
  serialOutDeque();
  unsigned long tm = millis();
  if(checkPlusPlusPlusExpire(tm))
    return;
  int c;
  while(hserial.available() > 0)
  {
    c=hserial.read();
    logSerialIn(c);
    if(pkti<HCM_BUFSIZ)
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
  {
    serialOutDeque();
    return;
  }
  
  pkti--;
  if((pkti>1)&&(inbuf[pkti-1]!=checksum(inbuf,pkti-1)))
    sendNAK();
  else
  {
    logPrintf("HOSTCM received: %c\n",inbuf[0]);
    switch(inbuf[0])
    {
      case 'N':
        hserial.write(lastOutBuf, lastOutSize);
        hserial.flush();
        break;
      case 'v':
        sendACK();
        break;
      case 'q':
        //TODO: close all open files
        aborted=true;
        break;
      case 'o':
        //TODO: count = hostopen(inbuf, n, outbuf);
        break;
      case 'c':
        //TODO: count = hostclose(inbuf, n, outbuf);
        break;
      case 'p':
        //TODO: count = hostput(inbuf, n, outbuf);
        break;
      case 'g':
        //TODO: count = hostget(inbuf, n, outbuf);
        break;
      case 'd':
        //TODO: count = hostdiropen(inbuf, n, outbuf);
        break;
      case 'f':
        //TODO: count = hostdirread(inbuf, n, outbuf);
        break;
      case 'k':
        //TODO: count = hostdirclose(inbuf, n, outbuf);
        break;
      case 'w':
        //TODO: count = hostrenamefm(inbuf, n, outbuf);
        break;
      case 'b':
        //TODO: count = hostrenameto(inbuf, n, outbuf);
        break;
      case 'y':
        //TODO: count = hostscratch(inbuf, n, outbuf);
        break;
      case 'r':
        //TODO: count = hostseek(inbuf, n, outbuf);
        break;
      default:
        sendNAK();
        break;
    }
  }
  pkti=0; // we are ready for the next packet!
  serialOutDeque();
}
