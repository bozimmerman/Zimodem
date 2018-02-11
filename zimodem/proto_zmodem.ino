bool isZByteIgnored(uint8_t b)
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

//----------------------------------------------------

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

ZModem::ZModem(Stream &modemIO)
{
  mdmIO = &modemIO;
}

bool ZModem::receive(FS &fs, String dirPath)
{
  ZExpect expect = ZEXPECT_NOTHING;
  int errorCount = 0;
  long timeout=10000;
  while(lastStatus == ZSTATUS_CONTINUE)
  {
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

