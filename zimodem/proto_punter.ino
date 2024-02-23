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
  while((millis()-start)<waitTime)
  {
    yield();
    int c = serialRead(Punter::receiveByteDelay);
    if(c < 0)
      return PUNTER_TIMEOUT;
    if(num == 0)
    {
        switch(c)
        {
        case 'g': nxt = 'o'; start = millis(); break;
        case 'b': nxt = 'a'; start = millis(); break;
        case 'a': nxt = 'c'; start = millis(); break;
        case 's':
          {
            start = millis();
            c = serialRead(Punter::receiveByteDelay);
            if(c < 0)
              return PUNTER_TIMEOUT;
            if(c == 'y')
              nxt = 'n';
            else
            if(c == '/')
              nxt = 'b';
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
        case 'o':
          if(num >= 2)
            return PUNTER_GOO;
          break;
        case 'a': nxt = 'd'; break;
        case 'c': nxt = 'k'; break;
        case 'd': return PUNTER_BAD;
        case 'k': return PUNTER_ACK;
        case 'n': return PUNTER_SYN;
        case 'b': return PUNTER_SNB;
        default:
          num = -1; // nope, skip it
          break;
        }
        num++;
    }
  }
  return PUNTER_TIMEOUT;
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
    case PUNTER_GOO: str = "goo"; break;
    case PUNTER_BAD: str = "bad"; break;
    case PUNTER_ACK: str = "ack"; break;
    case PUNTER_SYN: str = "syn"; break;
    case PUNTER_SNB: str = "s/b"; break;
    default: return;
  }
  for(int i=0;i<3;i++)
    serialWrite(str[i]);
  pserial.flush();
}

/*

#region Handshaking Constatnts

public static byte[] MULTIPUNTER_FILE_PREAMBLE = { 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09 };
public static byte[] MULTIPUNTER_FINALIZE      = { 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };

public bool SendMulti(List<Punter_File> files)
{
    bool b = false;
    foreach (Punter_File pf in files)
    {
        DataOut.Send(MULTIPUNTER_FILE_PREAMBLE);
        DataOut.Send(pf.Filename);
        DataOut.Send(0x2c);
        DataOut.Send(pf.FileType);
        DataOut.Send(0x0d);
        SendHandshake(HS_GOO);
        Send(pf.FileData, pf.FileType.Equals(0x01));
    }
    //Send finalization
    DataOut.Send(MULTIPUNTER_FILE_PREAMBLE);
    DataOut.Send(MULTIPUNTER_FINALIZE);
    DataOut.Send(0x0d);
    return b;
}

public List<Punter_File> RecvMulti()
{
    List<Punter_File> ReceivedFiles = new List<Punter_File>();
    bool FinalReceived = false;
    while (!FinalReceived)
    {
        //Listen for pre-amble of 0x09
        ReceivedBytes preamble = GetLine(30000);
        if (!preamble.TimedOut)
        {
            if (preamble.bytes[preamble.bytes.Count - 1] != 0x04)
            {
                Punter_File pf = new Punter_File();
                //Receive File
                pf.FileData = Receive();
                if (pf.FileData != null)
                {
                    //Fish our filename and type out of the preamble
                    int FilenameStart = preamble.bytes.LastIndexOf(0x09)+1;
                    int FilenameLen = preamble.bytes.Count - (FilenameStart+2);
                    pf.Filename = ArraySegment(preamble.bytes.ToArray(), FilenameStart, FilenameLen);
                    pf.FileType = preamble.bytes[preamble.bytes.Count - 1];

                    ReceivedFiles.Add(pf);
                }
                else
                {
                    FinalReceived = true;
                }
            }
            else
            {
                FinalReceived = true;
            }

        }
        else
        {
            FinalReceived = true; //Timeout on pre-amble, we;'re done.
        }
    }
    return ReceivedFiles;
}

*/

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
  if(this->readPunterCode(Punter::receiveFrameDelay * 10) != PUNTER_GOO)
    return false;
  this->sendPunterCode(PUNTER_ACK);
  if(this->readPunterCode(Punter::receiveFrameDelay * 2) != PUNTER_SNB)
    return false;

  // send filetype packet
  this->buf[0] = 1; // prg=1, seq=2
  this->bufSize = 1;
  if(!this->sendBufBlock(65535, 4, Punter::retryLimit))
    return false;

  this->sendPunterCode(PUNTER_SYN);
  if(this->readPunterCode(Punter::receiveFrameDelay) != PUNTER_SYN)
    return false;
  this->sendPunterCode(PUNTER_SNB);
  if(this->readPunterCode(Punter::receiveFrameDelay) != PUNTER_GOO)
    return false;
  this->sendPunterCode(PUNTER_ACK);
  if(this->readPunterCode(Punter::receiveFrameDelay) != PUNTER_SNB)
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
      if(!this->dataHandler(this->pfile, blockNum, (char *)this->buf+PUNTER_HDR_SIZE, bytesToRead))
        return false;
    }
    bytesRemain -= bytesToRead;
    nextBlockSize = (bytesRemain > 248) ? 255 : bytesRemain + PUNTER_HDR_SIZE;
    int sendBlockNo = ((bytesRemain <= 248)?65280:0) + blockNum;
    if(!this->sendBufBlock(sendBlockNo,nextBlockSize,Punter::retryLimit))
      return false;
    blockNum++;
  }
  this->sendPunterCode(PUNTER_SYN);
  if(this->readPunterCode(Punter::receiveFrameDelay) != PUNTER_SYN)
    return false;
  this->sendPunterCode(PUNTER_SNB);
  return true;
}

bool Punter::sendBufBlock(unsigned int blkNum, unsigned int nextBlockSize, int tries)
{
  memcpy(this->buf+PUNTER_HDR_SIZE,this->buf,(size_t)this->bufSize);
  buf[PUNTER_HDR_NBLKSZ] = (uint8_t)nextBlockSize;
  buf[PUNTER_HDR_BLKNLB] = (uint8_t)(blkNum & 0xff);
  buf[PUNTER_HDR_BLKNHB] = (uint8_t)((blkNum & 0xffff) >> 8);
  this->bufSize += PUNTER_HDR_SIZE;
  uint16_t chksums[2];
  this->calcBufChecksums(chksums);
  buf[PUNTER_HDR_CHK1LB] = (uint8_t)(chksums[0] & 0xff);
  buf[PUNTER_HDR_CHK1HB] = (uint8_t)((chksums[0] & 0xffff) >> 8);
  buf[PUNTER_HDR_CHK2LB] = (uint8_t)(chksums[1] & 0xff);
  buf[PUNTER_HDR_CHK2LB] = (uint8_t)((chksums[1] & 0xffff) >> 8);
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
  for(int i=0;i<tries; i++)
  {
    yield();
    this->sendPunterCode(sendCode);
    if(this->readPunterCode(Punter::receiveByteDelay) == expectCode)
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
  success=this->receiveBlock(PUNTER_HDR_SIZE+1,Punter::retryLimit); // 7 regular + 1 payload
  if(!success) // abort!
    return false;

  uint8_t blockSize = buf[PUNTER_HDR_NBLKSZ]; // from the filetype!
  // next do useless block
  success=this->receiveBlock(blockSize,Punter::retryLimit); //
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
    if(blockNum >= 65280) // are we actually done? Check for a magic number!
    {
      if(!exchangePunterCodes(PUNTER_SNB,PUNTER_SYN, Punter::retryLimit))
        return false;
      if(!exchangePunterCodes(PUNTER_SYN,PUNTER_SNB, Punter::retryLimit))
        return false;
      return true;
    }
    // write the buffer data to disk
    if(this->dataHandler != NULL)
    {
      char *dataBuf = (char *)(this->buf + PUNTER_HDR_SIZE);
      int dataSize = this->bufSize - PUNTER_HDR_SIZE;
      if(!this->dataHandler(this->pfile, blockNum, dataBuf, dataSize))
        return false;
    }
    // now get the size of the NEXT block and move along
    blockSize = buf[PUNTER_HDR_NBLKSZ];
  }
  return false;
}

#endif
