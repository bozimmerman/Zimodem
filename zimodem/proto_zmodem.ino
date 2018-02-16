
ZModem::ZModem(Stream &modemIn, ZSerial &modemOut)
{
  mdmIn = &modemIn;
  mdmOt = &modemOut;
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
    return 'l';
  case 0xff:
    return 'm';
  case 'l':
    return 0x7f;
  case 'm':
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
    uint16_t crc16tabval = pgm_read_byte_near(crc16tab + ((crc16 >> 8) & 0xff));
    return (uint16_t) ( crc16tabval ^ (crc16 << 8) ^ byt );
  }
  else
  {
    uint32_t crc32 = crc;
    uint32_t crc32tabval = pgm_read_byte_near(crc32tab + ((crc32 ^ byt) & 0xff));
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
  while(doread)
  {
    uint8_t n = readZModemByte(normalTimeout,&lastStatus);
    if (lastStatus == ZSTATUS_TIMEOUT)
      return ZSTATUS_TIMEOUT;
    if((n=='O')&&(gotFIN))
    {
      n = readZModemByte(normalTimeout,&lastStatus);
      if (lastStatus == ZSTATUS_TIMEOUT)
        return ZSTATUS_TIMEOUT;
      if(n=='O')
        return ZSTATUS_FINISH;
    }
    if(n==0x18)
    {
      n = readZModemByte(normalTimeout,&lastStatus);
      if (lastStatus == ZSTATUS_TIMEOUT)
        return ZSTATUS_TIMEOUT;
      if(n == 0x18)
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
    if(beforeStop==0)
      doread = false;
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
    if(fmt == ZHFORMAT_HEX)
    {
      int oldBufSize = *bufSize;
      *bufSize = i;
      for(int x=i;x<oldBufSize-1;x+=2)
        buf[(*bufSize)++]=FROMHEX(buf[x],buf[x+1]);
    }
    uint32_t hcrc = (hcrcBits == 16) ? 0 : 0xffffffff;
    uint8_t htyp = buf[i++];
    buf[0]=htyp; // building the final header packet
    hcrc = updateZCrc(htyp, hcrcBits, hcrc);
    if((htyp > 19)
    &&(htyp!=0x18)
    &&(htyp!=(0x18^0x40))
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
    break;
  }
  case ZACTION_DATA:
  {
    uint8_t dtyp = buf[0];
    if(crcBits == 16)
      crc = updateZCrc(0, crcBits, updateZCrc(0, crcBits, crc)) & 0xffff;
    else
      crc = ~crc;
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
    break;
  }
  case ZACTION_ESCAPE:
    return ZSTATUS_TIMEOUT;
  case ZACTION_CANCEL:
    crc = (crcBits == 16) ? 0 : 0xffffffff;
    return ZSTATUS_CANCEL;
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

void ZModem::sendHexHeader(uint8_t type, uint8_t* flags)
{
  uint8_t hbuf[14]; // because flags is always 4, so 2 + (4*2) + (2*2) = 14
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
      if(timeouts>=10)
      {
        sendCancel();
        lastStatus = packetStatus;
        break;
      }
    }
    else
    if(packetStatus == ZSTATUS_INVALIDCHECKSUM)
    {
      ++errorCount;
      if(errorCount>=3)
      {
        sendCancel();
        lastStatus = packetStatus;
        break;
      }
    }
    else
    if((packetStatus == ZSTATUS_CANCEL)||(packetStatus == ZSTATUS_FINISH))
    {
      lastStatus = packetStatus;
      break;
    }
    else
    if(packetStatus == ZSTATUS_HEADER)
    {
      lastStatus = ZSTATUS_CONTINUE;
      switch(buf[0])
      {
      case ZMOCHAR_ZRQINIT:
      {
        uint8_t recvOpt[4] ={0,4,0,ZMOPT_ESCCTL|ZMOPT_ESC8};
        sendHexHeader(ZMOCHAR_ZRINIT, recvOpt);
        break;
      }
      case ZMOCHAR_ZFILE:
        expect = ZEXPECT_FILENAME;
        break;
      case ZMOCHAR_ZEOF:
      {
        uint8_t recvOpt[4] ={0,4,0,ZMOPT_ESCCTL|ZMOPT_ESC8};
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
        uint8_t finOpt[4] ={0,0,0,0};
        sendHexHeader(ZMOCHAR_ZFIN, finOpt);
        lastStatus = ZSTATUS_FINISH;
        break;
      }
      default:
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
      case ZEXPECT_NOTHING:
      {
        uint8_t recvOpt[4] ={0,4,0,ZMOPT_ESCCTL|ZMOPT_ESC8};
        sendHexHeader(ZMOCHAR_ZRINIT, recvOpt);
        break;
      }
      case ZEXPECT_FILENAME:
      {
        filename = "";
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
          expect = ZEXPECT_NOTHING;
        case ZMOCHAR_ZCRCQ:
        {
          uint8_t byts[4]={(Foffset & 0xff),((Foffset >> 8)&0xff),((Foffset >> 16)&0xff),((Foffset >> 24)&0xff)};
          sendHexHeader(ZMOCHAR_ZACK, byts);
          break;
        }
        case ZMOCHAR_ZCRCE:
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
  while(lastStatus == ZSTATUS_CONTINUE)
  {
    // read packet, less-sensitive to timeout at first
    // more sensitive later
    // on crc fail, increase error count. 
    
    
    // exit on cancel or finish packets
    
    // if the packet is a header, do header stuff
    // headers can trigger file transfers
    
  }
  return false;
}

