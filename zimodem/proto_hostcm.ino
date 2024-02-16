#ifdef INCLUDE_SD_SHELL
#ifdef INCLUDE_HOSTCM
/* Converted from Rob Ferguson's code by Bo Zimmerman 
 * 
 * Copyright (c) 2013, Robert Ferguson All rights reserved.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS 
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT 
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

char *basename(char *path)
{
  char *base = strrchr(path, '/');
  return base ? base+1 : path;
}

char *dirname(char *path)
{
  char *base = strrchr(path, '/');
  if(base)
  {
    *base = 0;
    return path;
  }
  return "";
}

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


bool HostCM::closeAllFiles()
{
  for(int i=0;i<HCM_MAXFN;i++)
    delFileEntry(&files[i]);
  if(renameF != 0)
    renameF.close();
  renameF = (File)0;
  if(openDirF != 0)
    openDirF.close();
  openDirF = (File)0;
  return true;
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
    FROMHEX(&inbuf[3], idex - 4);
    idex = ((idex - 4) / 2) + 4;
  }
  if((eor=='z')
  &&((h->format == 't')
     ||(h->type != 'f')))
  {
    inbuf[idex-1] = opt.lineend;
    idex++;
  } 
  if(h->f.write(&inbuf[3],idex-4) != idex-4)
  {
    sendError("error: write failed to file %c", inbuf[1]);
    return;
  }
  sendACK();
}

void HostCM::protoGetFileBytes()
{
  int c;
  HCMFile *h = getFileByDescriptor((char)inbuf[1]);
  if (h==0) 
  {
    sendError("error: invalid descriptor %c", inbuf[1]);
    return;
  }
  if((h->f == 0)
  ||((h->mode != 'r') && (h->mode != 'u') && (h->mode != 'l')))
  {
    sendError("error: file not open/readable %c", inbuf[1]);
    return;
  }

  odex = 0;
  outbuf[odex++] = opt.response;
  if(h->format == 't')
  {
    outbuf[odex++] = 'b';
    outbuf[odex] = 'z';
    do
    {
      odex++;
      c = h->f.read();
      outbuf[odex] = (uint8_t)(c & 0xff);
    }
    while((c >= 0) && (odex < (HCM_SENDBUF - 2)) && (outbuf[odex] != 0xd));
    
    if(c<0)
    {
      outbuf[1] = 'e';
      outbuf[2] = checksum(&outbuf[1], 1);
      odex=3; // abandon anything between EOL and EOF
    }
    else 
    {
      if (odex >= (HCM_SENDBUF - 2))
        outbuf[2] = 'n';
      outbuf[odex] = checksum(&outbuf[1], odex - 1);
      odex++;
    }
  } 
  else 
  if (h->format == 'b') 
  {
    int rdcount = HCM_SENDBUF;
    char eor = 'n';
    if(h->reclen)
    {
      if (h->reclen < HCM_SENDBUF) 
      {
        rdcount = h->reclen;
        eor = 'z';
      } 
      else 
      {
        int pos = h->f.position();
        if((pos & h->reclen) > ((pos + rdcount) & h->reclen))
        {
          rdcount = ((pos + rdcount) / h->reclen) * h->reclen - pos;
          eor = 'z';
        }
      }
    }
    uint8_t rdbuf[rdcount];
    int n = h->f.read(rdbuf, rdcount);
    if (n <= 0) 
    {
      outbuf[odex++] = 'e';
      outbuf[odex] = checksum(&outbuf[1], odex - 1);
      odex++;
    } 
    else 
    {
      outbuf[odex++] = 'b';
      outbuf[odex++] = eor;
      for(int i=0;i<n;i++)
        memcpy(TOHEX(rdbuf[i]), &outbuf[odex+(i*2)], 2);
      odex += n * 2;
      outbuf[odex] = checksum(&outbuf[1], odex - 1);
      odex++;
    }
  }

  outbuf[odex++] = opt.lineend;
  outbuf[odex++] = opt.prompt;
  hserial.write(outbuf, odex);
  hserial.flush();
}

void HostCM::protoOpenFile()
{
  uint8_t *eobuf = inbuf + idex;
  int n=numOpenFiles();
  if(n >= HCM_MAXFN)
  {
    sendError("error: too many open files");
    return;
  }
  if(idex<8)
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
  uint8_t *ptr = (uint8_t *)memchr(inbuf+3, '(', idex-3);
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
  inbuf[idex - 1] = 0; 
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
    if(newF->f == 0)
    {
      sendError("error: failed to open '%s'", newF->filename);
      return;
    }
    if(newF->f && (mode == 'a'))
      newF->f.seek(EOF);
  }
  newF->mode = mode;
  newF->format = format;
  newF->type = type;
  newF->reclen = reclen;
  
  odex = 0;
  outbuf[odex++] = opt.response;
  outbuf[odex++] = 'b';
  outbuf[odex++] = newF->descriptor;
  outbuf[odex++] = checksum(&(outbuf[1]), 2);
  outbuf[odex++] = opt.lineend;
  outbuf[odex++] = opt.prompt;
  hserial.write(outbuf, odex);
  hserial.flush();
}

void HostCM::protoOpenDir()
{
  if(openDirF != 0) 
  {
    sendError("error: directory open");
    return;
  }
  if(idex > 2) 
    inbuf[idex - 1] = 0;
  else 
  {
    strcpy((char *)&inbuf[1], "/");
    idex++;
  }

  openDirF = SD.open((char *)&inbuf[1]);
  if((openDirF == 0)||(!openDirF.isDirectory())) 
  {
    sendError("error: directory not found %s",(char *)&inbuf[1]);
    return;
  }
  sendACK();
}

void HostCM::protoCloseDir()
{
  if(openDirF == 0) 
  {
    sendError("error: directory not open"); // should this really be an error?
    return;
  }
  openDirF.close();
  openDirF = (File)0;
  sendACK();
}

void HostCM::protoNextDirFile()
{
  if(openDirF == 0) 
  {
    sendError("error: directory not open"); // should this really be an error?
    return;
  }

  odex = 0;
  outbuf[odex++] = opt.response;
  
  File nf = openDirF.openNextFile();
  if(nf == 0)
    outbuf[odex++] = 'e';
  else 
  {
    char *fname = (char *)nf.name();
    char *paren = strchr(fname,')');
    int reclen = 0;
    if((strncmp("(f:", fname, 3) == 0) && (paren != 0)) 
    {
      fname = paren + 1;
      reclen = atoi((char *)&fname[3]);
    }
    outbuf[odex++] = 'b';

    if(reclen) 
      odex += snprintf((char *)&outbuf[odex], HCM_BUFSIZ - odex, "%-20s  %8llu (%d)", 
          fname, (unsigned long long)nf.size(), reclen);
    else 
    {
      if(nf.isDirectory()) 
        odex += snprintf((char *)&outbuf[odex], HCM_BUFSIZ - odex, "%s/", fname);
      else 
      {
        odex += snprintf((char *)&outbuf[odex], HCM_BUFSIZ - odex, "%-20s  %8llu", 
            fname, (unsigned long long)nf.size());
      }
    }
  }
  nf.close();
  outbuf[odex] = checksum(&(outbuf[1]), odex - 1);
  odex++;
  outbuf[odex++] = opt.lineend;
  outbuf[odex++] = opt.prompt;
  hserial.write(outbuf, odex);
  hserial.flush();
}

void HostCM::protoSetRenameFile()
{
  if (renameF != 0) 
  {
    sendError("error: rename in progress");
    return;
  }

  if (idex > 2) 
  {
    inbuf[idex - 1] = 0;
    renameF = SD.open((char *)&inbuf[1]);
  } 
  else 
  {
    sendError("error: missing filename");
    return;
  }
  sendACK();
}

void HostCM::protoFinRenameFile()
{
  if (renameF == 0) 
  {
    sendError("error: rename not started");
    return;
  }

  if (idex > 2) 
    inbuf[idex - 1] = 0;
  else 
  {
    sendError("error: missing filename");
    return;
  }

  String on = renameF.name();
  renameF.close();
  if(!SD.rename(on,(char *)&inbuf[1]))
  {
    renameF = (File)0;
    sendError("error: rename %s failed",on);
    return;
  }
  renameF = (File)0;
  sendACK();
}

void HostCM::protoEraseFile()
{
  if (idex > 2) 
    inbuf[idex - 1] = 0;
  else 
  {
    sendError("error: missing filename");
    return;
  }
  if(!SD.remove((char *)&inbuf[1]))
  {
    sendError("error: erase %s failed",(char *)&inbuf[1] );
    return;
  }
  sendACK();
}

void HostCM::protoSeekFile()
{
  HCMFile *h = getFileByDescriptor((char)inbuf[1]);
  if (h==0) 
  {
    sendError("error: invalid descriptor %c", inbuf[1]);
    return;
  }
  if(h->f == 0)
  {
    sendError("error: file not open/readable %c", inbuf[1]);
    return;
  }
  
  inbuf[idex - 1] = 0;

  unsigned long offset = atoi((char *)&inbuf[2]) * ((h->reclen == 0)? 1 : h->reclen);
  if(!h->f.seek(offset))
  {
    sendError("error: seek failed on %s @ %ul", h->f.name(),offset);
    return;
  }
  sendACK();
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
  odex = 0;
  outbuf[odex++] = opt.response;
  outbuf[odex++] = 'x';
  va_list arglist;
  va_start(arglist, format);
  odex += vsnprintf((char *)&outbuf[2], 80,  format, arglist); 
  va_end(arglist);
  outbuf[odex] = checksum(&outbuf[1], odex - 1);
  odex++;
  outbuf[odex++] = opt.lineend;
  outbuf[odex++] = opt.prompt;
  hserial.write(outbuf, odex);
  hserial.flush();
}

void HostCM::sendNAK()
{
  odex = 0;
  outbuf[odex++] = opt.response;
  outbuf[odex++] = 'N';
  outbuf[odex++] = opt.lineend;
  outbuf[odex++] = opt.prompt;
  hserial.write(outbuf, odex);
  hserial.flush();
}

void HostCM::sendACK()
{
  odex = 0;
  outbuf[odex++] = opt.response;
  outbuf[odex++] = 'b';
  outbuf[odex++] = checksum(&(outbuf[1]), 1);
  outbuf[odex++] = opt.lineend;
  outbuf[odex++] = opt.prompt;
  hserial.write(outbuf, odex);
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
    if(idex<HCM_BUFSIZ)
      inbuf[idex++]=c;
    checkDoPlusPlusPlus(c, tm);
    if(checkPlusPlusPlusExpire(tm))
      return;
    yield();
    if(c==opt.lineend)
    {
      inbuf[idex-1]=c;
      break;
    }
  }
  
  if((idex==0)
  ||(inbuf[idex-1]!=opt.lineend))
  {
    serialOutDeque();
    return;
  }
  
  idex--;
  if((idex>1)&&(inbuf[idex-1]!=checksum(inbuf,idex-1)))
    sendNAK();
  else
  {
    logPrintf("HOSTCM received: %c\r\n",inbuf[0]);
    switch(inbuf[0])
    {
      case 'N':
        hserial.write(outbuf, odex);
        hserial.flush();
        break;
      case 'v': sendACK(); break;
      case 'q': aborted = closeAllFiles(); break;
      case 'o': protoOpenFile(); break;
      case 'c': protoCloseFile(); break;
      case 'p': protoPutToFile(); break;
      case 'g': protoGetFileBytes(); break;
      case 'd': protoOpenDir(); break;
      case 'f': protoNextDirFile(); break;
      case 'k': protoCloseDir(); break;
      case 'w': protoSetRenameFile(); break;
      case 'b': protoFinRenameFile(); break;
      case 'y': protoEraseFile(); break;
      case 'r': protoSeekFile(); break;
      default:
        sendNAK();
        break;
    }
  }
  idex=0; // we are ready for the next packet!
  serialOutDeque();
}
#endif
#endif
