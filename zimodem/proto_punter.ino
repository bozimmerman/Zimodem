/*
   Copyright 2024-2024 Bo Zimmerman

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifdef INCLUDE_SD_SHELL

Punter::Punter(File &f, FlowControlType commandFlow, RecvChar recvChar, SendChar sendChar, DataHandler dataHandler)
{
  this->pfile = &f;
  this->sendChar = sendChar;
  this->recvChar = recvChar;
  this->dataHandler = dataHandler;
  this->pserial.setFlowControlType(FCT_DISABLED);
  if(commandFlow==FCT_RTSCTS)
    this->pserial.setFlowControlType(FCT_RTSCTS);
  this->pserial.setPetsciiMode(false);
  this->pserial.setXON(true);
}

PunterCode Punter::readPunterCode(int waitTime)
{
  unsigned long start = millis();
  char nxt = '\0';
  int num = 0;
  int shortWait = Punter::receiveByteDelay;
  if(waitTime >= Punter::receiveFrameDelay)
    shortWait = waitTime;
  while((millis()-start)<waitTime)
  {
    yield();
    int c = serialRead(shortWait);
    if(c < 0)
      return PUNTER_TIMEOUT;
    if(num == 0)
    {
        switch(c)
        {
        case 'G': nxt = 'O'; start = millis(); break;
        case 'B': nxt = 'A'; start = millis(); break;
        case 'A': nxt = 'C'; start = millis(); break;
        case 'S':
          {
            start = millis();
            c = serialRead(Punter::receiveByteDelay);
            if(c < 0)
              return PUNTER_TIMEOUT;
            if(c == 'Y')
              nxt = 'N';
            else
            if(c == '/')
              nxt = 'B';
            else
              num = -1; // nope, skip!
            break;
          }
        default:
          num = -1; // nope, skip it
          break;
        }
        num++;
    }
    else
    if(c != nxt)
      num = 0;
    else
    {
        switch(c)
        {
        case 'O':
          if(num >= 2)
            return PUNTER_GOO;
          break;
        case 'A': nxt = 'D'; break;
        case 'C': nxt = 'K'; break;
        case 'D': return PUNTER_BAD;
        case 'K': return PUNTER_ACK;
        case 'N': return PUNTER_SYN;
        case 'B': return PUNTER_SNB;
        default:
          num = -1; // nope, skip it
          break;
        }
        num++;
    }
  }
  return PUNTER_TIMEOUT;
}

bool Punter::readPunterCode(PunterCode code, int waitTime)
{
  unsigned long start = millis();
  while((millis()-start)<(waitTime*5))
  {
    yield();
    PunterCode c = this->readPunterCode(waitTime);
    if(c == PUNTER_TIMEOUT)
      return false;
    if(c == code)
      return true;
  }
  return false;
}

int Punter::serialRead(int delay)
{
  return this->recvChar(&pserial,delay);
}

void Punter::serialWrite(char symbol)
{
 this->pserial.write(symbol);
}

void Punter::sendPunterCode(PunterCode code)
{
  char *str;
  switch(code)
  {
    case PUNTER_GOO: str = "GOO"; break;
    case PUNTER_BAD: str = "BAD"; break;
    case PUNTER_ACK: str = "ACK"; break;
    case PUNTER_SYN: str = "SYN"; break;
    case PUNTER_SNB: str = "S/B"; break;
    default: return;
  }
  for(int i=0;i<3;i++)
    serialWrite(str[i]);
  pserial.flush();
}

void Punter::calcBufChecksums(uint16_t* chks)
{
  chks[0] = 0;
  chks[1] = 0;
  for(int i=PUNTER_HDR_NBLKSZ;i<this->bufSize;i++)
  {
    chks[0] += buf[i];
    chks[1] ^= buf[i];
    chks[1] = (chks[1] << 1) | (chks[1] >> 15);
  }
}

bool Punter::transmit()
{
  bool success = exchangePunterCodes(PUNTER_GOO, PUNTER_GOO, Punter::retryLimit*3);
  if(!success) // abort!
    return false;
  this->sendPunterCode(PUNTER_ACK);
  if(!this->readPunterCode(PUNTER_SNB, Punter::receiveFrameDelay * 2))
    return false;

  // send filetype packet
  this->buf[0] = 1; // prg=1, seq=2
  this->bufSize = 1;
  if(!this->sendBufBlock(65535, 4, Punter::retryLimit))
    return false;

  this->sendPunterCode(PUNTER_SYN);
  if(!this->readPunterCode(PUNTER_SYN, Punter::receiveFrameDelay))
    return false;
  this->sendPunterCode(PUNTER_SNB);
  delay(1000);
  this->sendPunterCode(PUNTER_SNB);
  delay(1000);
  this->sendPunterCode(PUNTER_SNB);
  delay(1000);
  if(!this->readPunterCode(PUNTER_GOO, Punter::receiveFrameDelay*3))
    return false;

  this->sendPunterCode(PUNTER_ACK);
  if(!this->readPunterCode(PUNTER_SNB, Punter::receiveByteDelay))
    return false;

  unsigned int bytesRemain = (unsigned int)this->pfile->size();
  int nextBlockSize = (bytesRemain > 248) ? 255 : bytesRemain + PUNTER_HDR_SIZE;
  this->bufSize = 0;
  if(!this->sendBufBlock(0, nextBlockSize, Punter::retryLimit))
    return false;
  int blockNum = 1;
  while(bytesRemain > 0)
  {
    this->bufSize = 0;
    int bytesToRead = (bytesRemain > 248) ? 248 : bytesRemain;
    if(this->dataHandler != NULL)
    {
      if(!this->dataHandler(this->pfile, blockNum, (char *)this->buf, bytesToRead))
        return false;
      this->bufSize += bytesToRead;
    }
    int sendBlockNo = ((bytesRemain <= 248)?65280:0) + blockNum;
    bytesRemain -= bytesToRead;
    nextBlockSize = (bytesRemain > 248) ? 255 : bytesRemain + PUNTER_HDR_SIZE;
    if(!this->sendBufBlock(sendBlockNo,nextBlockSize,Punter::retryLimit))
      return false;
    blockNum++;
  }
  this->sendPunterCode(PUNTER_SYN);
  if(!this->readPunterCode(PUNTER_SYN, Punter::receiveFrameDelay))
    return false;
  this->sendPunterCode(PUNTER_SNB);
  return true;
}

bool Punter::sendBufBlock(unsigned int blkNum, unsigned int nextBlockSize, int tries)
{
  // make room for the header, safely.  Do Not Use memcpy!
  for(int i=this->bufSize-1;i>=0;i--)
    this->buf[i+PUNTER_HDR_SIZE] = this->buf[i];
  buf[PUNTER_HDR_NBLKSZ] = (uint8_t)nextBlockSize;
  buf[PUNTER_HDR_BLKNLB] = (uint8_t)(blkNum & 0xff);
  buf[PUNTER_HDR_BLKNHB] = (uint8_t)((blkNum & 0xffff) >> 8);
  this->bufSize += PUNTER_HDR_SIZE;
  uint16_t chksums[2];
  this->calcBufChecksums(chksums);
  buf[PUNTER_HDR_CHK1LB] = (uint8_t)(chksums[0] & 0xff);
  buf[PUNTER_HDR_CHK1HB] = (uint8_t)((chksums[0] & 0xffff) >> 8);
  buf[PUNTER_HDR_CHK2LB] = (uint8_t)(chksums[1] & 0xff);
  buf[PUNTER_HDR_CHK2HB] = (uint8_t)((chksums[1] & 0xffff) >> 8);
  for(int tri=0;tri<tries;tri++)
  {
    for(int i=0;i<this->bufSize;i++)
      this->serialWrite(this->buf[i]);
    delay(1);
    PunterCode pc = this->readPunterCode(Punter::receiveFrameDelay * 2);
    if(pc == PUNTER_GOO)
    {
      this->sendPunterCode(PUNTER_ACK);
      pc = this->readPunterCode(Punter::receiveByteDelay);
      return pc == PUNTER_SNB;
    }
    if (pc == PUNTER_BAD)
    {
      this->sendPunterCode(PUNTER_ACK);
      this->readPunterCode(Punter::receiveByteDelay); // better be s/b
    }
  }
  return false;
}

bool Punter::receiveBlock(int size, int tries)
{
  for(int tri = 0; tri < tries; tri++)
  {
    this->bufSize = 0;
    this->sendPunterCode(PUNTER_SNB);
    int c = this->serialRead(Punter::receiveFrameDelay);
    if(c >= 0)
    {
      this->buf[this->bufSize++] = c;
      while(this->bufSize < size)
      {
        yield();
        int c = this->serialRead(Punter::receiveByteDelay);
        if(c < 0)
          break;
        this->buf[this->bufSize++] = c;
      }
      if(this->bufSize == size)
      {
        uint16_t chksums[2];
        this->calcBufChecksums(chksums);
        uint16_t chksum1 = chksums[0];
        uint16_t chksum2 = chksums[1];
        if(((chksum1&0xff) == buf[PUNTER_HDR_CHK1LB])
        &&((chksum1>>8) == buf[PUNTER_HDR_CHK1HB])
        &&((chksum2&0xff) == buf[PUNTER_HDR_CHK2LB])
        &&((chksum2>>8) == buf[PUNTER_HDR_CHK2HB]))
        {
            if(this->exchangePunterCodes(PUNTER_GOO, PUNTER_ACK, Punter::retryLimit))
              return true;
        }
      }
    }
    if(tries < 2)
      return false;
    this->sendPunterCode(PUNTER_BAD);
  }
  return false;
}

bool Punter::exchangePunterCodes(PunterCode sendCode, PunterCode expectCode, int tries)
{
  for(int i=0;i<tries;i++)
  {
    yield();
    this->sendPunterCode(sendCode);
    if(readPunterCode(expectCode,Punter::receiveByteDelay))
      return true;
  }
  return false;
}

bool Punter::receive()
{
  // first say hello -- thirty goos, thirty chances;
  bool success = exchangePunterCodes(PUNTER_GOO, PUNTER_ACK, Punter::retryLimit*3);
  if(!success) // abort!
    return false;

  // next do filetype block
  success=this->receiveBlock(PUNTER_HDR_SIZE+1, Punter::retryLimit); // 7 regular + 1 payload
  if(!success) // abort!
    return false;

  this->sendPunterCode(PUNTER_SNB);
  if(!this->readPunterCode(PUNTER_SYN, Punter::receiveFrameDelay))
    return false;
  this->sendPunterCode(PUNTER_SYN);
  if(!this->readPunterCode(PUNTER_SNB, Punter::receiveFrameDelay))
    return false;
  if(!this->readPunterCode(PUNTER_SNB, Punter::receiveFrameDelay))
    return false;
  //if(!this->readPunterCode(PUNTER_SNB, Punter::receiveFrameDelay))
  //  return false;
  if(!exchangePunterCodes(PUNTER_GOO, PUNTER_ACK, Punter::retryLimit))
    return false;

  uint8_t blockSize = buf[PUNTER_HDR_NBLKSZ]; // from the filetype!
  // next do useless block
  success=this->receiveBlock(PUNTER_HDR_SIZE,Punter::retryLimit); //
  if(!success) // abort!
    return false;

  blockSize = buf[PUNTER_HDR_NBLKSZ]; // from the useless!
  while(true)
  {
    // receive a for-real block
    success=this->receiveBlock(blockSize,Punter::retryLimit);
    if(!success)
      return false;
    unsigned long blockNum = buf[PUNTER_HDR_BLKNLB] + (256 * buf[PUNTER_HDR_BLKNHB]);
    // write the buffer data to disk
    if(this->dataHandler != NULL)
    {
      char *dataBuf = (char *)(this->buf + PUNTER_HDR_SIZE);
      int dataSize = this->bufSize - PUNTER_HDR_SIZE;
      if(!this->dataHandler(this->pfile, blockNum, dataBuf, dataSize))
        return false;
    }
    if(blockNum >= 65280) // are we actually done? Check for a magic number!
    {
      if(!exchangePunterCodes(PUNTER_SNB,PUNTER_SYN, Punter::retryLimit))
        return false;
      if(!exchangePunterCodes(PUNTER_SYN,PUNTER_SNB, Punter::retryLimit))
        return false;
      return true;
    }
    // now get the size of the NEXT block and move along
    blockSize = buf[PUNTER_HDR_NBLKSZ];
  }
  return false;
}

#endif
