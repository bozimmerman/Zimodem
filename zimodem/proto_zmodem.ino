
ZModem::ZModem(Stream &modemIn, ZSerial &modemOut)
{
  mdmIn = &modemIn;
  mdmOt = &modemOut;
}

String ZModem::getLastErrors()
{
  switch(lastStatus)
  {
  case ZSTATUS_DATA:
  case ZSTATUS_HEADER:
  case ZSTATUS_CONTINUE:
    return "Premature ending";
  case ZSTATUS_TIMEOUT:
    return "Timeout";
  case ZSTATUS_FINISH:
    return "";
  case ZSTATUS_CANCEL:
    return "Cancelled";
  case ZSTATUS_INVALIDCHECKSUM:
    return "Checksum Error";
  }
  return "";
}


bool ZModem::isZByteIgnored(uint8_t b)
{
  switch(b)
  {
  case 0x11:
  case 0x13:
  case 0x91:
  case 0x93:
    return true;
  default:
    return false;
  }
}

uint8_t ZModem::readZModemByte(long timeout, ZStatus *status)
{
  for(int i=0;i<timeout;i++)
  {
    if(mdmIn->available()>0)
    {
      uint8_t b = (uint8_t)mdmIn->read();
      if(!isZByteIgnored(b))
        return b;
    }
    delay(1);
    serialOutDeque();
  }
  *status = ZSTATUS_TIMEOUT;
  return 0;
}

ZModem::ZAction ZModem::detectEzcape(uint8_t byt, uint8_t *size)
{
  ZAction act = ZACTION_ESCAPE;
  *size = 0;
  switch(byt)
  {
  case 'A':
    act = ZACTION_HEADER;
    *size = 7;
    break;
  case 'B':
    act = ZACTION_HEADER;
    *size = 16;
    break;
  case 'C':
    act = ZACTION_HEADER;
    *size = 9;
    break;
  case 'h':
  case 'i':
  case 'j':
  case 'k':
    act = ZACTION_DATA;
    *size =2;
    break;
  }
  if((!acceptsHeader) && (act == ZACTION_HEADER))
    return ZACTION_ESCAPE;
  return act;
}

uint8_t ZModem::ezcape(uint8_t byt)
{
  switch(byt)
  {
  case 0x7f:
    return ZMOCHAR_ZRUB0;
  case 0xff:
    return ZMOCHAR_ZRUB1;
  case ZMOCHAR_ZRUB0:
    return 0x7f;
  case ZMOCHAR_ZRUB1:
    return 0xff;
  default:
    return byt^0x40;
  }
}

uint32_t ZModem::updateZCrc(uint8_t byt, uint8_t bits, uint32_t crc)
{
  if(bits == 16)
  {
    uint16_t crc16 = (uint16_t)crc;
    uint16_t crc16index = ((crc16 >> 8) & 0xff);
    uint16_t crc16tabval = pgm_read_word_near(crc16tab + crc16index);
    return (uint16_t) ( crc16tabval ^ (crc16 << 8) ^ byt );
  }
  else
  {
    uint32_t crc32 = crc;
    uint8_t crc32index = ((crc32 ^ byt) & 0xff);
    uint32_t crc32tabval = pgm_read_dword_near(crc32tab + crc32index);
    return (crc32tabval ^ ((crc32 >> 8) & 0x00FFFFFF));
  }
}

ZModem::ZStatus ZModem::readZModemPacket(uint8_t *buf, uint16_t *bufSize)
{
  ZModem::ZAction act=ZACTION_ESCAPE;
  int beforeStop = -1;
  int countCan = 0;
  bool doread=true;
  uint32_t crc = 0;
  const long normalTimeout = 2000;
debugPrintf("\n");
  while(doread)
  {
    uint8_t n = readZModemByte(normalTimeout,&lastStatus);
    if (lastStatus == ZSTATUS_TIMEOUT)
      return ZSTATUS_TIMEOUT;
debugPrintf("%d ",n);
    
    if((n=='O')&&(gotFIN))
    {
      n = readZModemByte(normalTimeout,&lastStatus);
      if (lastStatus == ZSTATUS_TIMEOUT)
        return ZSTATUS_TIMEOUT;
debugPrintf("%d ",n);
      if(n=='O')
        return ZSTATUS_FINISH;
    }
    if(n==ZMOCHAR_ZDLE)
    {
      n = readZModemByte(normalTimeout,&lastStatus);
      if (lastStatus == ZSTATUS_TIMEOUT)
        return ZSTATUS_TIMEOUT;
debugPrintf("%d ",n);
      if(n == ZMOCHAR_ZDLE)
        countCan+=2;
      else
        countCan=0;
      
      uint8_t size=0;
      ZAction escAct = detectEzcape(n,&size);
      if(escAct != ZACTION_ESCAPE && (beforeStop<0))
      {
        act = escAct;
        if(act == ZACTION_DATA)
          beforeStop = (crcBits == 16) ? 2 : 4;
        else
          beforeStop = size;
        crc = updateZCrc(n, crcBits, crc);
      }
      else
        n = ezcape(n);
    }
    buf[(*bufSize)++] = n;
    if(beforeStop<0)
      crc = updateZCrc(n, crcBits, crc);
    else
    if(beforeStop==0)
      doread = false;
    else
    if(beforeStop>0)
      beforeStop--;
    if(countCan>=5)
    {
      doread = false;
      act = ZACTION_CANCEL;
    }
  }
  switch(act)
  {
  case ZACTION_HEADER:
  {
    int i=0;
    ZModem::ZHFormat fmt = ZHFORMAT_UNK;
    uint8_t hcrcBits = 16;
    while((i<*bufSize) && (fmt == ZHFORMAT_UNK))
    {
      switch(buf[i++])
      {
      case 'C':
        fmt = ZHFORMAT_BIN32;
        hcrcBits = 32;
        break;
      case 'A':
        fmt = ZHFORMAT_BIN;
        break;
      case 'B':
        fmt = ZHFORMAT_HEX;
        break;
      }
    }
    // i is now one past the type char
    if(fmt == ZHFORMAT_HEX)
    {
      int oldBufSize = *bufSize;
      *bufSize = i;
      for(int x=i;x<oldBufSize-1;x+=2)
        buf[(*bufSize)++]=FROMHEX(buf[x],buf[x+1]);
    }
debugPrintf("\nBINBUF: ");
for(int ii=0;ii<*bufSize;ii++)
  debugPrintf("%d ",buf[ii]);
debugPrintf("\n");
    uint32_t hcrc = (hcrcBits == 16) ? 0 : 0xffffffff;
    uint8_t htyp = buf[i++];
    buf[0]=htyp; //building the final header packet
    hcrc = updateZCrc(htyp, hcrcBits, hcrc);
    if((htyp > 19)
    &&(htyp!=ZMOCHAR_ZDLE)
    &&(htyp!=ZMOCHAR_ZDLEE)
    &&(strchr("*ABChijklm",(char)htyp)<0))
      htyp=0xff;
    for(int x=0;x<4 && i<*bufSize;x++)
    {
      uint8_t b=buf[i++];
      hcrc = updateZCrc(b, hcrcBits, hcrc);
      buf[x+1]=b;
    }
    if(hcrcBits == 16)
      hcrc = updateZCrc(0, hcrcBits, updateZCrc(0, hcrcBits, hcrc)) & 0xffff;
    else
      hcrc = ~hcrc;
    while(hcrcBits > 0)
    {
      if(i>=*bufSize)
        return ZSTATUS_INVALIDCHECKSUM;
      uint8_t hCrcChkByt = buf[i++];
      hcrcBits -=8;
      if(hCrcChkByt != ((hcrc >> hcrcBits) & 0xff))
        return ZSTATUS_INVALIDCHECKSUM;
    }
    crcBits = (fmt == ZHFORMAT_BIN32) ? 32 : 16;
    crc = (crcBits == 16) ? 0 : 0xffffffff;
    if(htyp==ZMOCHAR_ZFIN)
      gotFIN=true;
    if(htyp==ZMOCHAR_ZDATA || htyp==ZMOCHAR_ZFILE)
      acceptsHeader = false;
    *bufSize = 5;
    return ZSTATUS_HEADER;
  }
  case ZACTION_DATA:
  {
    uint8_t dtyp = buf[0];
    if(crcBits == 16)
      crc = updateZCrc(0, crcBits, updateZCrc(0, crcBits, crc)) & 0xffff;
    else
      crc = ~crc;
    /*
    debugPrintf("RA-BUF: ");
    int x=0;
    for(x=0;x<(*bufSize)-(crcBits/8);x++)
      debugPrintf("%s ",TOHEX(buf[x]));
    debugPrintf("CHK: %s ",TOHEX(buf[x++]));
    for(;x<(*bufSize);x++)
      debugPrintf("%s ",TOHEX(buf[x]));
    debugPrintf("\n");
    debugPrintf("RA-CRCCHK of data type %d\n",dtyp);
    */
    uint8_t dcrcBits = crcBits;
    int i=(*bufSize - (dcrcBits/8));
    if(i < 0)
      return ZSTATUS_INVALIDCHECKSUM;
    while(dcrcBits > 0)
    {
      if(i>=*bufSize)
        return ZSTATUS_INVALIDCHECKSUM;
      uint8_t dCrcChkByt = buf[i++];
      dcrcBits -=8;
      if(dCrcChkByt != ((crc >> dcrcBits) & 0xff))
        return ZSTATUS_INVALIDCHECKSUM;
    }
    *bufSize = *bufSize - (crcBits / 8);
    crc = (crcBits == 16) ? 0 : 0xffffffff;
    if(dtyp==ZMOCHAR_ZCRCG)
      acceptsHeader = false;
    else
      acceptsHeader = true;
    return ZSTATUS_DATA;
  }
  case ZACTION_ESCAPE:
    return ZSTATUS_TIMEOUT;
  case ZACTION_CANCEL:
    crc = (crcBits == 16) ? 0 : 0xffffffff;
    return ZSTATUS_CANCEL;
  default:
    return ZSTATUS_INVALIDCHECKSUM;
  }
}

void ZModem::sendCancel()
{
  for(int i=0;i<8;i++)
    mdmOt->printb(0x18);
  for(int i=0;i<8;i++)
    mdmOt->printb(0x08);
  mdmOt->flush();
}

bool ZModem::isZByteEscaped(uint8_t b, uint8_t prev_b, bool ctlFlag)
{
  switch(b)
  {
  case 0xd:
  case (byte)0x8d:
    if (ctlFlag &&  prev_b=='@')
      return true;
    break;
  case 0x18:
  case 0x10:
  case 0x11:
  case 0x13:
  case 0x7f:
  case 0x90:
  case 0x91:
  case 0x93:
  case 0xff:
    return true;
  default:
    if (ctlFlag && ((b & 0x60)==0) )
      return true;
  }
  return false;
}

uint8_t ZModem::addZDLE(uint8_t b, uint8_t *buf, uint8_t *index, uint8_t prev_b)
{
  if (isZByteEscaped(b, prev_b, false))
  {
    buf[(*index)++]=ZMOCHAR_ZDLE;
    buf[(*index)++]=ezcape(b);
    prev_b = buf[(*index)-1];
  }
  else
  {
    buf[(*index)++]=b;
    prev_b = b;
  }
  return prev_b;
}

void ZModem::sendDataPacket(uint8_t type, uint8_t* data, int dataSize)
{
  //debugPrintf("Send data packet: %d\n",type);
  uint8_t *hbuf = (uint8_t *)malloc((dataSize*2)+64);
  uint8_t hbufIndex=0;
  const uint8_t hcrcBits = 16; // because bin, but not bin32
  uint32_t hcrc = 0; // because always bin, but not bin32

  uint8_t prev=0;
  for(int i=0;i<dataSize;i++)
  {
    hcrc = updateZCrc(data[i],hcrcBits,hcrc);
    prev = addZDLE(data[i], hbuf, &hbufIndex, prev);
  }
  hbuf[hbufIndex++] = ZMOCHAR_ZDLE;
  
  hcrc = updateZCrc(type,hcrcBits,hcrc);
  hbuf[hbufIndex++] = type;
  
  if(hcrcBits == 16)
    hcrc = updateZCrc(0, hcrcBits, updateZCrc(0, hcrcBits, hcrc)) & 0xffff;
  else
    hcrc = ~hcrc;
  
  prev=0;
  prev = addZDLE((uint8_t)((hcrc >> 8) & 0xff), hbuf, &hbufIndex, prev);
  prev = addZDLE((uint8_t)(hcrc & 0xff), hbuf, &hbufIndex, prev);
  
  mdmOt->printb(ZMOCHAR_ZPAD);
  
  // now the format
  mdmOt->printb(ZMOCHAR_ZDLE);
  mdmOt->printb(ZMOCHAR_ZBIN);
  mdmOt->flush();
  
  // the actual stuff (type, flags, crc, etc)
  for(int i=0;i<hbufIndex;i++)
    mdmOt->printb(hbuf[i]);
  mdmOt->flush();
  
  if(type == ZMOCHAR_ZCRCW)
  {
    mdmOt->printb(0x11);
    mdmOt->flush();
  }
  free(hbuf); // this is kinda important
}

void ZModem::sendBinHeader(uint8_t type, uint8_t* flags)
{
  //debugPrintf("Send bin header: %d\n",type);
  uint8_t hbuf[15]; 
  uint8_t hbufIndex=0;
  const uint8_t hcrcBits = 16; // because bin, but not bin32
  uint32_t hcrc = 0; // because always bin, but not bin32
  
  hbuf[hbufIndex++]=type;
  hcrc = updateZCrc(type,hcrcBits,hcrc);
  
  uint8_t prev=0;
  for(int i=0;i<4;i++)
  {
    hcrc = updateZCrc(flags[i],hcrcBits,hcrc);
    prev = addZDLE(flags[i], hbuf, &hbufIndex, prev);
  }
  if(hcrcBits == 16)
    hcrc = updateZCrc(0, hcrcBits, updateZCrc(0, hcrcBits, hcrc)) & 0xffff;
  else
    hcrc = ~hcrc;

  prev=0;
  prev = addZDLE((uint8_t)((hcrc >> 8) & 0xff), hbuf, &hbufIndex, prev);
  prev = addZDLE((uint8_t)(hcrc & 0xff), hbuf, &hbufIndex, prev);

  mdmOt->printb(ZMOCHAR_ZPAD);
  
  // now the format
  mdmOt->printb(ZMOCHAR_ZDLE);
  mdmOt->printb(ZMOCHAR_ZBIN);
  mdmOt->flush();
  
  // the actual stuff (type, flags, crc, etc)
  for(int i=0;i<hbufIndex;i++)
    mdmOt->printb(hbuf[i]);
  mdmOt->flush();
}

void ZModem::sendHexHeader(uint8_t type, uint8_t* flags)
{
  //debugPrintf("Send hex header: %d\n",type);
  uint8_t hbuf[15]; // because flags is always 4, so 2 + (4*2) + (2*2) = 14 +0
  uint8_t hbufIndex=0;
  const uint8_t hcrcBits = 16; // because always hex
  uint32_t hcrc = 0; // because always hex
  
  sprintf((char *)(hbuf+hbufIndex),"%s",tohex(type));
  hbufIndex+=2;
  hcrc = updateZCrc(type,hcrcBits,hcrc);
  for(int i=0;i<4;i++)
  {
    hcrc = updateZCrc(flags[i],hcrcBits,hcrc);
    sprintf((char *)(hbuf+hbufIndex),"%s",tohex(flags[i]));
    hbufIndex+=2;
  }
  if(hcrcBits == 16)
    hcrc = updateZCrc(0, hcrcBits, updateZCrc(0, hcrcBits, hcrc)) & 0xffff;
  else
    hcrc = ~hcrc;
  
  sprintf((char *)(hbuf+hbufIndex),"%s",tohex((hcrc >> 8) & 0xff));
  hbufIndex+=2;
  sprintf((char *)(hbuf+hbufIndex),"%s",tohex((hcrc) & 0xff));
  hbufIndex+=2;
  
  for(int i=0;i<2;i++) // hex byte width
    mdmOt->printb(ZMOCHAR_ZPAD);
  
  // now the format
  mdmOt->printb(ZMOCHAR_ZDLE);
  mdmOt->printb(ZMOCHAR_ZHEX);
  mdmOt->flush();
  
  // the actual stuff (type, flags, crc, etc)
  for(int i=0;i<hbufIndex;i++)
    mdmOt->printb(hbuf[i]);
  mdmOt->flush();
  
  mdmOt->printb(0x0d); // eol
  mdmOt->printb(0x0a);
  mdmOt->printb(0x11); // xon
  //if(o instanceof DataPacket) if( ((DataPacket)o).type()==ZModemCharacter.ZCRCW)
  //  implWrite(ASCII.XON.value());  //*/
  mdmOt->flush();
}

bool ZModem::receive(FS &fs, String dirPath)
{
  debugPrintf("Begin Z-Modem Receive\n");
  uint8_t recvStart[4] ={0,4,0,ZMOPT_ESCCTL|ZMOPT_ESC8};

  sendHexHeader(ZMOCHAR_ZRINIT, recvStart);
  
  bool fileOpen=false;
  File F;
  String filename = "";
  uint32_t Foffset=0;
  ZExpect expect = ZEXPECT_NOTHING;
  int errorCount = 0;
  long timeouts=0;
  uint8_t buf[4096];
  while(lastStatus == ZSTATUS_CONTINUE)
  {
    uint16_t bufSize = 0;
    ZStatus packetStatus = readZModemPacket(buf,&bufSize);
    if(packetStatus == ZSTATUS_TIMEOUT)
    {
      ++timeouts;
      debugPrintf("RCV: TIMEOUT %d\n",timeouts);
      if(timeouts>=10)
      {
        sendCancel();
        lastStatus = packetStatus;
        break;
      }
      lastStatus = ZSTATUS_CONTINUE;
    }
    else
    if(packetStatus == ZSTATUS_INVALIDCHECKSUM)
    {
      ++errorCount;
      debugPrintf("RCV: INVALIDCHECKSUM %d\n",errorCount);
      if(errorCount>=3)
      {
        sendCancel();
        lastStatus = packetStatus;
        break;
      }
      sendHexHeader(ZMOCHAR_ZRINIT, recvStart);
      lastStatus = ZSTATUS_CONTINUE;
    }
    else
    if((packetStatus == ZSTATUS_CANCEL)||(packetStatus == ZSTATUS_FINISH))
    {
      debugPrintf("RCV: CANCEL/FINISH \n");
      lastStatus = packetStatus;
      break;
    }
    else
    if(packetStatus == ZSTATUS_HEADER)
    {
      lastStatus = ZSTATUS_CONTINUE;
      errorCount=0;
      timeouts=5;
      switch(buf[0])
      {
      case ZMOCHAR_ZRQINIT:
      {
        debugPrintf("RCV: ZRQINIT\n");
        uint8_t recvOpt[4] ={0,4,0,ZMOPT_ESCCTL|ZMOPT_ESC8};
        debugPrintf("SND: ZRINIT \n");
        sendHexHeader(ZMOCHAR_ZRINIT, recvOpt);
        break;
      }
      case ZMOCHAR_ZFILE:
        debugPrintf("RCV: ZFILE\n");
        expect = ZEXPECT_FILENAME;
        break;
      case ZMOCHAR_ZEOF:
      {
        debugPrintf("RCV: ZEOF\n");
        uint8_t recvOpt[4] ={0,4,0,ZMOPT_ESCCTL|ZMOPT_ESC8};
        debugPrintf("SND: ZRINIT \n");
        sendHexHeader(ZMOCHAR_ZRINIT, recvOpt);
        expect = ZEXPECT_NOTHING;
        filename = "";
        if(fileOpen)
          F.close();
        fileOpen=false;
        Foffset=0;
        break;
      }
      case ZMOCHAR_ZDATA:
      {
        uint32_t pos=(buf[1]) | ((uint32_t)buf[2] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[2] << 24);
        debugPrintf("RCV: ZDATA %d\n",pos);
        if(fileOpen)
        {
          if(pos != Foffset)
          {
            uint8_t byts[4]={(pos & 0xff),((pos >> 8)&0xff),((pos >> 16)&0xff),((pos >> 24)&0xff)};
            debugPrintf("SND: ZRPOS %d!=%d \n",pos,Foffset);
            sendHexHeader(ZMOCHAR_ZRPOS, byts);
          }
          else
            expect = ZEXPECT_DATA;
        }
        else
        if(filename.length() > 0)
        {
          Foffset=0;
          if(pos != 0)
          {
            File tchk = fs.open(dirPath + filename,FILE_READ);
            if(tchk)
            {
              if(tchk.size()==pos)
                Foffset=pos;
              tchk.close();
            }
          }
          F=fs.open(dirPath + filename,Foffset>0?FILE_APPEND:FILE_WRITE);
          fileOpen=true;
          expect = ZEXPECT_DATA;
        }
        else
          lastStatus = ZSTATUS_CANCEL;
        break;
      }
      case ZMOCHAR_ZFIN:
      {
        debugPrintf("RCV: ZFIN\n");
        uint8_t finOpt[4] ={0,0,0,0};
        debugPrintf("SND: ZFIN\n");
        sendHexHeader(ZMOCHAR_ZFIN, finOpt);
        lastStatus = ZSTATUS_FINISH;
        break;
      }
      case ZMOCHAR_ZSINIT:
      {
        debugPrintf("RCV: ZSINIT\n");
        uint8_t finOpt[4] ={0,0,0,0};
        debugPrintf("SND: ZACK\n");
        sendHexHeader(ZMOCHAR_ZACK, finOpt);
        expect = ZEXPECT_ZSINIT;
        break;
      }
      default:
        debugPrintf("RCV: **UNKNOWN**: ");
        for(int i=0;i<4;i++)
          debugPrintf("%d ",buf[i]);
        debugPrintf("\n");
        sendCancel();
        lastStatus = ZSTATUS_CANCEL;
        break;
      }
    }
    else
    if(packetStatus == ZSTATUS_DATA)
    {
      lastStatus = ZSTATUS_CONTINUE;
      switch(expect)
      {
      case ZMOCHAR_ZSINIT:
      {
        debugPrintf("RCV: ZSINIT\n");
        //TODO?
        uint8_t finOpt[4] ={0,0,0,0};
        debugPrintf("SND: ZACK\n");
        sendHexHeader(ZMOCHAR_ZACK, finOpt);
        expect = ZEXPECT_ZSINIT;
        break;
      }
      case ZEXPECT_NOTHING:
      {
        debugPrintf("RCV: DATAxNOTHING\n");
        uint8_t recvOpt[4] ={0,4,0,ZMOPT_ESCCTL|ZMOPT_ESC8};
        debugPrintf("SND: ZRINIT\n");
        sendHexHeader(ZMOCHAR_ZRINIT, recvOpt);
        break;
      }
      case ZEXPECT_FILENAME:
      {
        debugPrintf("RCV: DATAxFILENAME\n");
        filename = "";
        debugPrintf("RCV: DEBUG:FILENAME:\n");
        for(int i=0;i<bufSize;i++)
          if(buf[i]<32)
            debugPrintf("$%s",TOHEX(buf[i]));
          else
            debugPrintf("%c",buf[i]);
        debugPrintf("\n");
        for(int i=0;i<bufSize && buf[i]!=0;i++)
          if(buf[i] >= 32)
            filename += (char)buf[i];
        File fchk = fs.open(dirPath + filename,FILE_READ);
        int pos = 0;
        if(fchk)
        {
          pos = fchk.size();
          fchk.close();
        }
        uint8_t byts[4]={(pos & 0xff),((pos >> 8)&0xff),((pos >> 16)&0xff),((pos >> 24)&0xff)};
        debugPrintf("SND: ZRPOS %d\n",pos);
        sendHexHeader(ZMOCHAR_ZRPOS, byts);
        expect = ZEXPECT_NOTHING;
        break;
      }
      case ZEXPECT_DATA:
        if(fileOpen)
        {
          for(int i=1;i<bufSize;i++,Foffset++)
            F.write(buf[i]);
        }
        switch(buf[0])
        {
        case ZMOCHAR_ZCRCW:
          debugPrintf("RCV: DATAxDATA:ZCRCW %d\n",bufSize);
          expect = ZEXPECT_NOTHING;
        case ZMOCHAR_ZCRCQ:
        {
          debugPrintf("RCV: DATAxDATA:ZCRCW %d\n",bufSize);
          uint8_t byts[4]={(Foffset & 0xff),((Foffset >> 8)&0xff),((Foffset >> 16)&0xff),((Foffset >> 24)&0xff)};
          debugPrintf("SBD: ZACK %d\n",Foffset);
          sendHexHeader(ZMOCHAR_ZACK, byts);
          break;
        }
        case ZMOCHAR_ZCRCE:
          debugPrintf("RCV: DATAxDATA:ZCRCE %d\n",bufSize);
          expect = ZEXPECT_NOTHING;
          break;
        default:
          debugPrintf("RCV: DATAxDATA:UNKNOWN %d, %d\n",buf[0],bufSize);
          expect = ZEXPECT_NOTHING;
          break;
        }
      }
    }
  }
  return lastStatus == ZSTATUS_FINISH;
}

bool ZModem::transmit(File &rfile)
{
  debugPrintf("Begin Z-Modem Transmit\n");
  mdmOt->prints("rz\r");
  bool atEOF = false;
  uint32_t Foffset=0;
  int errorCount = 0;
  long timeouts=0;
  uint8_t buf[4096];
  while(lastStatus == ZSTATUS_CONTINUE)
  {
    uint16_t bufSize = 0;
    ZStatus packetStatus = readZModemPacket(buf,&bufSize);
    if(packetStatus == ZSTATUS_TIMEOUT)
    {
      ++timeouts;
      debugPrintf("RCV: TIMEOUT %d\n",timeouts);
      if(timeouts>=10)
      {
        sendCancel();
        lastStatus = packetStatus;
        break;
      }
      uint8_t recvOpt[4] ={0,4,0,ZMOPT_ESCCTL|ZMOPT_ESC8};
      sendHexHeader(ZMOCHAR_ZRQINIT, recvOpt);
      lastStatus = ZSTATUS_CONTINUE;
    }
    else
    if(packetStatus == ZSTATUS_INVALIDCHECKSUM)
    {
      ++errorCount;
      debugPrintf("RCV: INVALIDCHECKSUM %d\n",errorCount);
      if(errorCount>=3)
      {
        sendCancel();
        lastStatus = packetStatus;
        break;
      }
      lastStatus = ZSTATUS_CONTINUE;
    }
    else
    if((packetStatus == ZSTATUS_CANCEL)||(packetStatus == ZSTATUS_FINISH))
    {
      debugPrintf("RCV: CANCEL/FINISH\n");
      lastStatus = packetStatus;
      break;
    }
    else
    if(packetStatus == ZSTATUS_HEADER)
    {
      lastStatus = ZSTATUS_CONTINUE;
      errorCount=0;
      timeouts=7;
      switch(buf[0])
      {
      case ZMOCHAR_ZRINIT:
        debugPrintf("RCV: ZRINIT\n");
        if(atEOF) // nextFile would go here!
        {
          uint8_t byts[4]={0,0,0,0};
          debugPrintf("SND: ZFIN\n");
          sendBinHeader(ZMOCHAR_ZFIN, byts);
        }
        else
        {
          uint8_t byts[4] ={0,0,0,ZMOPT_ZCBIN};
          sendBinHeader(ZMOCHAR_ZFILE, byts);
          String rfileSzStr="";
          rfileSzStr += rfile.size();
          String p=rfile.name();
          int x=p.lastIndexOf("/");
          if((x>=0)&&(x<p.length()-1))
            p = p.substring(x+1);
          uint8_t *pbuf = (uint8_t *)malloc(p.length() + (rfileSzStr.length()*2) + 25);
          uint16_t pbuflen=0;
          for(int i=0;i<p.length();i++)
            pbuf[pbuflen++] = p[i];
          pbuf[pbuflen++] = 0;
          for(int i=0;i<rfileSzStr.length();i++)
            pbuf[pbuflen++] = rfileSzStr[i];
          //String nums=" 0 0 0 1 ";
          String nums=" 0 0 0";
          for(int i=0;i<nums.length();i++)
            pbuf[pbuflen++] = nums[i];
          pbuf[pbuflen++] = 0;
          
          debugPrintf("SND: ZCRCW of len %d\n",pbuflen);
          sendDataPacket(ZMOCHAR_ZCRCW, pbuf, pbuflen);
          free(pbuf);
        }
        break;
      case ZMOCHAR_ZRPOS:
      {
        if(!atEOF)
        {
          uint32_t pos=(buf[1]) | ((uint32_t)buf[2] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[2] << 24);
          debugPrintf("RCV: ZRPOS %d==%d\n",pos,Foffset);
          if(pos!=Foffset)
          {
            rfile.seek(pos);
            Foffset = pos;
          }
        }
        else
          debugPrintf("RCV: ZRPOS ??\n");
      }
      //FALLTHROUGH OK!
      case ZMOCHAR_ZACK:
      {
        if(buf[0] == ZMOCHAR_ZACK)
          debugPrintf("RCV: ZACK\n");
        
        {
          uint8_t byts[4]={(Foffset & 0xff),((Foffset >> 8)&0xff),((Foffset >> 16)&0xff),((Foffset >> 24)&0xff)};
          debugPrintf("SND: ZDATA: %d\n",Foffset);
          sendBinHeader(ZMOCHAR_ZDATA, byts);
          uint8_t data[1024];
          int len = rfile.read(data, 1024);
          uint8_t type = ZMOCHAR_ZCRCW;
          if(rfile.available()==0)
          {
            atEOF = true;
            type=ZMOCHAR_ZCRCE;
          }
          Foffset += len;
          debugPrintf("SND: ZDATA: DATA LEN=%d\n",len);
          sendDataPacket(type, data, len);
        }
        if(atEOF)
        {
          uint8_t byts[4]={(Foffset & 0xff),((Foffset >> 8)&0xff),((Foffset >> 16)&0xff),((Foffset >> 24)&0xff)};
          debugPrintf("SND: ZEOF\n");
          sendHexHeader(ZMOCHAR_ZEOF, byts);
        }
        break;
      }
      case ZMOCHAR_ZFIN:
        debugPrintf("RCV: ZFIN\n");
        for(int i=0;i<2;i++)
          mdmOt->printb('O');
        lastStatus = ZSTATUS_FINISH;
        break;
      default:
        debugPrintf("RCV: %d??!! CANCELLING!\n",buf[0]);
        sendCancel();
        lastStatus = ZSTATUS_CANCEL;
        break;
      }
    }
  }
  return false;
}

