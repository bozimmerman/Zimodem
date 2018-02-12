
ZModem::ZModem(Stream &modemIO)
{
  mdmIO = &modemIO;
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

uint8_t ZModem::readZModemByte(long timeout)
{
  for(int i=0;i<timeout;i++)
  {
    if(mdmIO->available()>0)
    {
      uint8_t b = (uint8_t)mdmIO->read();
      if(!isZByteIgnored(b))
        return b;
    }
    delay(1);
    serialOutDeque();
  }
  lastStatus = ZSTATUS_TIMEOUT;
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
    uint8_t n = readZModemByte(normalTimeout);
    if (lastStatus == ZSTATUS_TIMEOUT)
      return ZSTATUS_TIMEOUT;
    if((n=='O')&&(gotFIN))
    {
      n = readZModemByte(normalTimeout);
      if (lastStatus == ZSTATUS_TIMEOUT)
        return ZSTATUS_TIMEOUT;
      if(n=='O')
        return ZSTATUS_FINISH;
    }
    if(n==0x18)
    {
      n = readZModemByte(normalTimeout);
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
    buf[0]=fmt; // building the final header packet
    buf[1]=htyp; // building the final header packet
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
      buf[x+2]=b;
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
    *bufSize = 6;
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

bool ZModem::receive(FS &fs, String dirPath)
{
  ZExpect expect = ZEXPECT_NOTHING;
  int errorCount = 0;
  long timeouts=0;
  uint8_t buf[4096];
  while(lastStatus == ZSTATUS_CONTINUE)
  {
    uint16_t bufSize = 0;
    ZStatus packetStatus = readZModemPacket(buf,&bufSize);
    // read packet, less-sensitive to timeout at first
    // more sensitive later
    // on crc fail, increase error count. 
    
    
    // exit on cancel or finish packets
    
    // if the packet is a header, do header stuff
    
    // if the packet is data, do data stuff
    
  }
  return false;
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

