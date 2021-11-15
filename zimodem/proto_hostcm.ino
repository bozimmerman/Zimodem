#ifdef INCLUDE_SD_SHELL
/* Converted from source reverse engineered from SP9000 roms by Rob Ferguson */
#include "proto_hostcm.h"
#include <libgen.h>

HostCM::HostCM(FS *fs)
{
  hFS = fs;
  for(int i=0;i<HCM_MAXFN;i++)
  {
    files[i].f = (File)0;
    files[i].descriptor = 'A' + i;
    delFileEntry(&files[i]);
  }
}

HostCM::~HostCM()
{
  closeAllFiles();
}


void HostCM::closeAllFiles()
{
  for(int i=0;i<HCM_MAXFN;i++)
    delFileEntry(&files[i]);
}

HCMFile *HostCM::addNewFileEntry()
{
  for(int i=0;i<HCM_MAXFN;i++)
  {
    if(files[i].f == 0)
      return &files[i];
  }
  return 0;
}

void HostCM::delFileEntry(HCMFile *e)
{
  if(e->f != 0)
    e->f.close();
  char d = e->descriptor;
  memset(e,sizeof(struct _HCMFile),0);
  e->descriptor = d;
}


HCMFile *HostCM::getFileByDescriptor(char c)
{
  for(int i=0;i<HCM_MAXFN;i++)
  {
    if(files[i].descriptor == c)
      return &files[i];
  }
  return 0;
}

int HostCM::numOpenFiles()
{
  int n=0;
  for(int i=0;i<HCM_MAXFN;i++)
  {
    if(files[i].f != 0)
      n++;
  }
  return n;
}

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

void HostCM::protoCloseFile()
{
  HCMFile *h = getFileByDescriptor((char)inbuf[1]);
  if (h==0) {
    sendError("error: invalid descriptor %c", inbuf[1]);
    return;
  }
  
  if (h->f == 0)
  {
    sendError("error: file not open %c", inbuf[1]); // should this be an error though?
    return;
  }
  delFileEntry(h);
  sendACK();
}

void HostCM::protoPutToFile()
{
  HCMFile *h = getFileByDescriptor((char)inbuf[1]);
  if (h==0) {
    sendError("error: invalid descriptor %c", inbuf[1]);
    return;
  }
  uint8_t eor = (uint8_t)lc((char)inbuf[2]);
  if (h->f == 0)
  {
    sendError("error: file not open %c", inbuf[1]);
    return;
  }
  /*if((h->mode)=='r')||(h->mode=='l'))
  {
    sendError("error: read-only file %c", inbuf[1]);
    return;
  }*/
  if(h->format == 'b') 
  {
    FROMHEX(&inbuf[3], pkti - 4);
    pkti = ((pkti - 4) / 2) + 4;
  }
  if((eor=='z')
  &&((h->format == 't')
     ||(h->type != 'f')))
  {
    inbuf[pkti-1] = opt.lineend;
    pkti++;
  } 
  if(h->f.write(&inbuf[3],pkti-4) != pkti-4)
  {
    sendError("error: write failed to file %c", inbuf[1]);
    return;
  }
  sendACK();
}

void HostCM::protoOpenFile()
{
  uint8_t *eobuf = inbuf + pkti;
  int n=numOpenFiles();
  if(n >= HCM_MAXFN)
  {
    sendError("error: too many open files");
    return;
  }
  if(pkti<8)
  {
    sendError("error: command too short");
    return;
  }
  uint8_t mode = (uint8_t)lc((char)inbuf[1]);
  if(strchr("rlwsua",(char)mode)==0)
  {
    sendError("error: illegal mode %c",(char)mode);
    return;
  }
  bool isRead = strchr("rl",(char)mode)!=0;
  uint8_t format = (uint8_t)lc((char)inbuf[2]);
  uint8_t *ptr = (uint8_t *)memchr(inbuf+3, '(', pkti-3);
  if(ptr == 0)
  {
    sendError("error: missing (");
    return;
  }
  uint8_t type = (uint8_t)lc((char)ptr[1]);
  uint8_t reclen = 0;
  if(ptr[2]==':')
    reclen=atoi((char *)(ptr+3));
  ptr = (uint8_t *)memchr(ptr+2, ')', eobuf-(ptr+1));
  if(ptr == 0)
  {
    sendError("error: missing )");
    return;
  }
  inbuf[pkti - 1] = 0; 
  uint8_t *fnptr = ptr + 1;
  HCMFile *newF = addNewFileEntry();
  if (type == 'f')
  {
    char *bn = basename((char *)ptr+1);
    char *dn = dirname((char *)ptr+1);
    if(reclen == 0)
      reclen = 80;
    snprintf(newF->filename, sizeof(newF->filename), "%s/(f:%d)%s", dn, reclen, bn);
  } 
  else
    strncpy(newF->filename, (char *)ptr+1, sizeof(newF->filename));
  if(isRead)
  {
    newF->f = SD.open(newF->filename);
    if(newF->f == 0)
    {
      if(strchr(basename(newF->filename), ',') == 0) 
      {
        if(strlen(newF->filename) + 5 < HCM_FNSIZ) 
        {
          if((mode == 'l') || (mode == 's')) 
            strcat(newF->filename, ",prg");
          else
          {
            if(type == 'f')
              strcat(newF->filename, ",rel");
            else
              strcat(newF->filename, ",seq");  
          }
          newF->f = SD.open(newF->filename);
          if(newF == 0)
          {
            sendError("error: file '%s' not found", newF->filename);
            return;
          }
        } 
      } 
      else 
      { 
        sendError("error: file '%s' not found", newF->filename);
        return;
      }
    }
  }
  else
  {
    newF->f = SD.open(newF->filename,FILE_WRITE);
    if(newF->f && (mode == 'a'))
      newF->f.seek(EOF);
  }
  newF->mode = mode;
  newF->format = format;
  newF->type = type;
  newF->reclen = reclen;
  
  lastOutSize = 0;
  lastOutBuf[lastOutSize++] = opt.response;
  lastOutBuf[lastOutSize++] = 'b';
  lastOutBuf[lastOutSize++] = newF->descriptor;
  lastOutBuf[lastOutSize++] = checksum(&(lastOutBuf[1]), 2);
  lastOutBuf[lastOutSize++] = opt.lineend;
  lastOutBuf[lastOutSize++] = opt.prompt;
  hserial.write(lastOutBuf, lastOutSize);
  hserial.flush();
}

char HostCM::checksum(uint8_t *b, int n)
{
  int i, s = 0;

  for (i = 0; i < n; i++)
    s += b[i] & 0xf;
  return ('A' + (s&0xf));
}

void HostCM::sendError(const char* format, ...)
{
  lastOutSize = 0;
  lastOutBuf[lastOutSize++] = opt.response;
  lastOutBuf[lastOutSize++] = 'x';
  va_list arglist;
  va_start(arglist, format);
  lastOutSize += vsnprintf((char *)&lastOutBuf[2], 80,  format, arglist); 
  va_end(arglist);
  lastOutBuf[lastOutSize] = checksum(&lastOutBuf[1], lastOutSize - 1);
  lastOutSize++;
  lastOutBuf[lastOutSize++] = opt.lineend;
  lastOutBuf[lastOutSize++] = opt.prompt;
  hserial.write(lastOutBuf, lastOutSize);
  hserial.flush();
}

void HostCM::sendNAK()
{
  lastOutSize = 0;
  lastOutBuf[lastOutSize++] = opt.response;
  lastOutBuf[lastOutSize++] = 'N';
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
        closeAllFiles();
        aborted=true;
        break;
      case 'o':
        protoOpenFile();
        break;
      case 'c':
        protoCloseFile();
        break;
      case 'p':
        protoPutToFile();
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
#endif