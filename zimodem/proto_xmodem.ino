/*
   Copyright 2018-2024 Bo Zimmerman

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

XModem::XModem(File &f, FlowControlType commandFlow, RecvChar recvChar, SendChar sendChar, DataHandler dataHandler)
{
  this->xfile = &f;
  this->sendChar = sendChar;
  this->recvChar = recvChar;
  this->dataHandler = dataHandler;  
  this->xserial.setFlowControlType(FCT_DISABLED);
  if(commandFlow==FCT_RTSCTS)
    this->xserial.setFlowControlType(FCT_RTSCTS);
  this->xserial.setPetsciiMode(false);
  this->xserial.setXON(true);
}

YModem::YModem(FS *fileSystem, File &f, FlowControlType commandFlow, RecvChar recvChar, SendChar sendChar, DataHandler dataHandler)
        : XModem(f,commandFlow,recvChar,sendChar,dataHandler)
{
  this->fileSystem = fileSystem;
  this->dfile = 0;
  if(f.isDirectory())
  {
    this->dfile = &f;
    this->xfile = 0;
  }
  this->blockSize = 1024;
  this->send0block = true;
}

bool XModem::serialAvail(int delay)
{
  if (this->byte != -1)
    return true;
  if ((this->byte = this->recvChar(&xserial,delay)) != -1)
    return true;
  else
    return false;    
}

int XModem::serialRead(int delay)
{
  int b;
  if(this->byte != -1)
  {
    b = this->byte;
    this->byte = -1;
    return b;
  }
  return this->recvChar(&xserial,delay);
}

void XModem::serialWrite(char symbol)
{
 this->xserial.write(symbol);
}

bool XModem::receiveFrameNo()
{
  unsigned char num = (unsigned char)this->serialRead(XModem::receiveFrameDelay);
  unsigned char invnum = (unsigned char)this-> serialRead(XModem::receiveByteDelay);
  this->repeatedBlock = false;
  //check for repeated block
  if (invnum == (255-num) && num == this->blockNo-1) {
    this->repeatedBlock = true;
    return true;  
  }
  
  if(num != this-> blockNo || invnum != (255-num))
    return false;
  else
    return true;
}

bool XModem::receiveData(int blkSize)
{
  for(int i = 0; i < blkSize; i++) {
    int byte = this->serialRead(XModem::receiveByteDelay);
    if(byte != -1)
      this->buffer[i] = (unsigned char)byte;
    else
      return false;
  }
  return true;  
}

bool XModem::checkCrc(int blkSize)
{
  unsigned short frame_crc = ((unsigned char)this->serialRead(XModem::receiveByteDelay)) << 8;
  
  frame_crc |= (unsigned char)this->serialRead(XModem::receiveByteDelay);
  //now calculate crc on data
  unsigned short crc = this->crc16_ccitt(this->buffer, blkSize);
  
  if(frame_crc != crc)
    return false;
  else
    return true;  
}

bool XModem::checkChkSum()
{
  unsigned char frame_chksum = (unsigned char)this->serialRead(XModem::receiveByteDelay);
  //calculate chksum
  unsigned char chksum = 0;
  for(int i = 0; i< blockSize; i++) {
    chksum += this->buffer[i];
  }
  if(frame_chksum == chksum)
    return true;
  else
    return false;
}

bool XModem::sendNack()
{
  this->serialWrite(XModem::XMO_NACK);
  this->retries++;
  if(this->retries < XModem::rcvRetryLimit)
    return true;
  else
    return false;  
}

bool XModem::receiveFrames(transfer_t transfer)
{
  this->blockNo = send0block ? 0 : 1;
  this->blockNoExt = send0block ? 0 : 1;
  this->retries = 0;

  while (1)
  {
    xserial.flush();
    char cmd = this->serialRead(XModem::receiveFrameDelay);
    this->blockSize = 128;
    switch(cmd)
    {
      case XModem::XMO_STX:
        this->blockSize = 1024;
      case XModem::XMO_SOH:
      {
        if (!this->receiveFrameNo()) {
          if (this->sendNack())
            break;
          else
            return false;
        }
        if (!this->receiveData(this->blockSize)) {
          if (this->sendNack())
            break;
          else
            return false;
        };
        if (transfer == Crc) {
          if (!this->checkCrc(this->blockSize)) {
            if (this->sendNack())
              break;
            else
              return false;
          }
        } else {
          if(!this->checkChkSum()) {
            if (this->sendNack())
              break;
            else
              return false;
          }
        }
        //callback
        if(this->send0block && (this->blockNo == 0) && (this->dfile != 0) && (this->fileSystem != 0))
        {
          if(xfile != 0)
            xfile->close();
          xfile = 0;
          char *fname = buffer;
          char *fsize = 0;
          if(this->buffer[0] == 0)
          {
            this->serialWrite(XModem::XMO_ACK);
            xserial.flush();
            return true; // received no file (or last file)!
          }
          int i=0;
          for(;i<128;i++)
          {
            if(this->buffer[i]==0)
            {
              if(fsize == 0)
                fsize = buffer + i + 1;
              else
                break;
            }
          }
          if((fsize == 0)||(i>=128))
          {
            debugPrintf("Ymodem fail: no args.\r\n");
            return false;
          }
          this->fileSize = atoi(fsize);
          if((this->fileSize == 0)||(this->fileSize > fileSizeLimit))
          {
            debugPrintf("Ymodem fail: no size.\r\n");
            return false;
          }
          if(fname[0] == '/')
            fname++;
          String path = this->dfile->name();
          if(path.endsWith("/"))
            path = path.substring(0,path.length()-1);
          path += "/";
          path += fname;
          debugPrintf("YModem opened %s for %u bytes.\r\n",path.c_str(),this->fileSize);
          if(fileSystem->exists(path))
              fileSystem->remove(path);
          yfile = fileSystem->open(path, FILE_WRITE);
          if(!yfile)
          {
            debugPrintf("Ymodem fail: no file.\r\n");
            return false;
          }
          this->xfile = &yfile;
          this->repeatedBlock = false;
        }
        else
        {
          if(this->dataHandler != NULL && this->repeatedBlock == false)
          {
            int dataSize = this->blockSize;
            if(this->send0block && (this->fileSize < this->blockSize))
              dataSize = this->fileSize;
            if(!this->dataHandler(this->xfile,this->blockNoExt, this->buffer, dataSize)) {
              return false;
            }
            this->fileSize -= dataSize;
          }
        }
        //ack
        this->serialWrite(XModem::XMO_ACK);
        if(this->blockNo == 0)
          this->serialWrite(XModem::XMO_CRC);
        if(this->repeatedBlock == false)
        {
          this->blockNo++;
          this->blockNoExt++;
        }
        break;
      }
      case XModem::XMO_EOT:
        this->serialWrite(XModem::XMO_ACK);
        if(send0block)
        {
            this->serialWrite(XModem::XMO_CRC);
            this->blockNo=0;
            this->blockNoExt=0;
            this->retries = 0;
            break;
        }
        return true;
      case XModem::XMO_CAN:
        //wait second CAN
        if(this->serialRead(XModem::receiveByteDelay) ==XModem::XMO_CAN)
        {
          this->serialWrite(XModem::XMO_ACK);
          //this->flushInput();
          return false;
        }
        //something wrong
        this->serialWrite(XModem::XMO_CAN);
        this->serialWrite(XModem::XMO_CAN);
        this->serialWrite(XModem::XMO_CAN);
        return false;
      default:
        //something wrong
        this->serialWrite(XModem::XMO_CAN);
        this->serialWrite(XModem::XMO_CAN);
        this->serialWrite(XModem::XMO_CAN);
        return false;
    }
  }
}

void XModem::init()
{
  //set preread byte    
  this->byte = -1;
}

bool XModem::receive()
{
  this->init();
  
  for (int i =0; i <  16; i++)
  {
    this->serialWrite(XModem::XMO_CRC);
    if (this->serialAvail(1500))
    {
      bool ok = receiveFrames(Crc);
      xserial.flushAlways();
      return ok;
    }
  }
  for (int i =0; i <  16; i++)
  {
    this->serialWrite(XModem::XMO_NACK);
    if (this->serialAvail(1500))
    {
      bool ok = receiveFrames(ChkSum);
      xserial.flushAlways();
      return ok;
    }
  }
}

unsigned short XModem::crc16_ccitt(char *buf, int size)
{
  unsigned short crc = 0;
  while (--size >= 0) {
    int i;
    crc ^= (unsigned short) *buf++ << 8;
    for (i = 0; i < 8; i++)
      if (crc & 0x8000)
        crc = crc << 1 ^ 0x1021;
      else
        crc <<= 1;
  }
  return crc;
}

unsigned char XModem::generateChkSum(void)
{
  //calculate chksum
  unsigned char chksum = 0;
  for(int i = 0; i< blockSize; i++) {
    chksum += this->buffer[i];
  }
  return chksum;
}

bool XModem::transmitFrame(transfer_t transfer, int blkSize)
{
  //send SOH
  if(blkSize == 1024)
    this->serialWrite(XModem::XMO_STX);
  else
    this->serialWrite(XModem::XMO_SOH);
  //send frame number
  this->serialWrite(this->blockNo);
  //send inv frame number
  this->serialWrite((unsigned char)(255-(this->blockNo)));
  //send data
  for(int i = 0; i <blkSize; i++)
    this->serialWrite(this->buffer[i]);
  //send checksum or crc
  if (transfer == ChkSum)
  {
    this->serialWrite(this->generateChkSum());
  }
  else
  {
    unsigned short crc;
    crc = this->crc16_ccitt(this->buffer, blkSize);
    this->serialWrite((unsigned char)(crc >> 8));
    this->serialWrite((unsigned char)(crc));
  }
  xserial.flush();
  return true;
}

bool XModem::transmitFrames(transfer_t transfer)
{
  this->blockNo = 1;
  this->blockNoExt = 1;
  // use this only in unit tetsing
  //memset(this->buffer, 'A', blockSize);
  //get data
  while(true) // keep going until all frames done
  {
    if (this->dataHandler != NULL)
    {
      if( false == this->dataHandler(xfile,this->blockNoExt, this->buffer, blockSize))
      {
        //end of transfer
        this->serialWrite(XModem::XMO_EOT);
        //wait ACK
        if (this->serialRead(XModem::receiveFrameDelay) == XModem::XMO_ACK)
          return true;
        else
          return false;
      }
    }
    else
    {
      //cancel transfer - send CAN twice
      this->serialWrite(XModem::XMO_CAN);
      this->serialWrite(XModem::XMO_CAN);
      //wait ACK
      if (this->serialRead(XModem::receiveFrameDelay) == XModem::XMO_ACK)
        return true;
      else
        return false;
    }
    int retries = 0;
    while(true)
    {
      transmitFrame(transfer, blockSize);
      int ret = this->serialRead(XModem::receiveFrameDelay);
      if(ret == XModem::XMO_ACK) //data is ok - go to next chunk
      {
          this->blockNo++;
          this->blockNoExt++;
          break;
      }
      else
      if(ret == XModem::XMO_NACK) //resend data
      {
      }
      else
      if(ret == XModem::XMO_CAN) //abort transmision
      {
        return false;
      }
      else
      {
      }
      if(++retries > 10)
      {
        //cancel transfer due to FAIL - send CAN twice
        this->serialWrite(XModem::XMO_CAN);
        this->serialWrite(XModem::XMO_CAN);
        return false;
      }
    }
  }
  return false;
}

bool XModem::transmit()
{
  int retry = 0;
  int sym;
  bool test = true;
  this->init();
  
  //wait for CRC transfer
  while(retry < 32)
  {
    if(this->serialAvail(1000))
    {
      sym = this->serialRead(1); //data is here - no delay
      if(sym == XModem::XMO_CRC)
      {
        if(send0block) // if YModem, send the header packet
        {
          strcpy((char *)this->buffer,this->xfile->name());
          int ct = strlen((char *)buffer);
          sprintf((char *)(buffer + ct + 1),"%u",(unsigned int)this->fileSize);
          ct = ct + strlen((char *)(buffer + ct + 1)) + 1;
          for(;ct<128;ct++)
            this->buffer[ct]=0;
          while(retry < 32)
          {
            this->blockNo = 0;
            this->transmitFrame(Crc, 128);
            sym = this->serialRead(XModem::receiveFrameDelay);
            if(sym == XModem::XMO_ACK) //data is ok - go to next chunk)
            {
              sym = this->serialRead(XModem::receiveByteDelay); // this should be the unnecc CRC
              if(sym == XModem::XMO_CRC)
                break;
              retry++;
            }
            else
            if((sym == XModem::XMO_CRC)&&(test))
            {
                test=false;
                retry++;
            }
            else
            if((sym != XModem::XMO_NACK) || (test))
            {
              retry=32;
              break;
            }
          }
        }
        if(retry >= 32)
          break;
        retry = 0;
        bool ok = this->transmitFrames(Crc);
        xserial.flushAlways();
        if(ok && send0block)
        {
          for(int i=0;i<128;i++)
            this->buffer[i]=0;
          this->blockNo = 0;
          this->transmitFrame(Crc, 128);
          //TODO: YModem: don't leave just yet, but move to next file
        }
        return ok;
      }
      else
      if(sym == XModem::XMO_NACK)
      {
        bool ok = this->transmitFrames(ChkSum);
        xserial.flushAlways();
        return ok;
      }
    }
    retry++;
  }
  this->serialWrite(XModem::XMO_CAN);
  this->serialWrite(XModem::XMO_CAN);
  return false;
}

#endif
