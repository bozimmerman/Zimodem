/*
   Copyright 2018-2019 Bo Zimmerman

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

XModem::XModem(File &f,
               FlowControlType commandFlow, 
               int (*recvChar)(ZSerial *ser, int msDelay), 
               void (*sendChar)(ZSerial *ser, char sym), 
               bool (*dataHandler)(File *xfile, unsigned long number, char *buffer, int len))
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

bool XModem::dataAvail(int delay)
{
  if (this->byte != -1)
    return true;
  if ((this->byte = this->recvChar(&xserial,delay)) != -1)
    return true;
  else
    return false;    
}

int XModem::dataRead(int delay)
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

void XModem::dataWrite(char symbol)
{
  this->sendChar(&xserial,symbol);
}

bool XModem::receiveFrameNo()
{
  unsigned char num = (unsigned char)this->dataRead(XModem::receiveDelay);
  unsigned char invnum = (unsigned char)this-> dataRead(XModem::receiveDelay);
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

bool XModem::receiveData()
{
  for(int i = 0; i < 128; i++) {
    int byte = this->dataRead(XModem::receiveDelay);
    if(byte != -1)
      this->buffer[i] = (unsigned char)byte;
    else
      return false;
  }
  return true;  
}

bool XModem::checkCrc()
{
  unsigned short frame_crc = ((unsigned char)this->dataRead(XModem::receiveDelay)) << 8;
  
  frame_crc |= (unsigned char)this->dataRead(XModem::receiveDelay);
  //now calculate crc on data
  unsigned short crc = this->crc16_ccitt(this->buffer, 128);
  
  if(frame_crc != crc)
    return false;
  else
    return true;  
}

bool XModem::checkChkSum()
{
  unsigned char frame_chksum = (unsigned char)this->dataRead(XModem::receiveDelay);
  //calculate chksum
  unsigned char chksum = 0;
  for(int i = 0; i< 128; i++) {
    chksum += this->buffer[i];
  }
  if(frame_chksum == chksum)
    return true;
  else
    return false;
}

bool XModem::sendNack()
{
  this->dataWrite(XModem::XMO_NACK);  
  this->retries++;
  if(this->retries < XModem::rcvRetryLimit)
    return true;
  else
    return false;  
}

bool XModem::receiveFrames(transfer_t transfer)
{
  this->blockNo = 1;
  this->blockNoExt = 1;
  this->retries = 0;
  while (1) {
    char cmd = this->dataRead(1000);
    switch(cmd){
      case XModem::XMO_SOH:
        if (!this->receiveFrameNo()) {
          if (this->sendNack())
            break;
          else
            return false;
        }
        if (!this->receiveData()) { 
          if (this->sendNack())
            break;
          else
            return false;          
        };
        if (transfer == Crc) {
          if (!this->checkCrc()) {
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
        if(this->dataHandler != NULL && this->repeatedBlock == false)
          if(!this->dataHandler(xfile,this->blockNoExt, this->buffer, 128)) {
            return false;
          }
        //ack
        this->dataWrite(XModem::XMO_ACK);
        if(this->repeatedBlock == false)
        {
          this->blockNo++;
          this->blockNoExt++;
        }
        break;
      case XModem::XMO_EOT:
        this->dataWrite(XModem::XMO_ACK);
        return true;
      case XModem::XMO_CAN:
        //wait second CAN
        if(this->dataRead(XModem::receiveDelay) ==XModem::XMO_CAN) 
        {
          this->dataWrite(XModem::XMO_ACK);
          //this->flushInput();
          return false;
        }
        //something wrong
        this->dataWrite(XModem::XMO_CAN);
        this->dataWrite(XModem::XMO_CAN);
        this->dataWrite(XModem::XMO_CAN);
        return false;
      default:
        //something wrong
        this->dataWrite(XModem::XMO_CAN);
        this->dataWrite(XModem::XMO_CAN);
        this->dataWrite(XModem::XMO_CAN);
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
    this->dataWrite('C'); 
    if (this->dataAvail(1500)) 
    {
      bool ok = receiveFrames(Crc);
      xserial.flushAlways();
      return ok;
    }
  
  }
  for (int i =0; i <  16; i++)
  {
    this->dataWrite(XModem::XMO_NACK);  
    if (this->dataAvail(1500)) 
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
  for(int i = 0; i< 128; i++) {
    chksum += this->buffer[i];
  }
  return chksum;
}

bool XModem::transmitFrames(transfer_t transfer)
{
  this->blockNo = 1;
  this->blockNoExt = 1;
  // use this only in unit tetsing
  //memset(this->buffer, 'A', 128);
  while(1)
  {
    //get data
    if (this->dataHandler != NULL)
    {
      if( false == this->dataHandler(xfile,this->blockNoExt, this->buffer, 128))
      {
        //end of transfer
        this->sendChar(&xserial,XModem::XMO_EOT);
        //wait ACK
        if (this->dataRead(XModem::receiveDelay) == XModem::XMO_ACK)
          return true;
        else
          return false;
      }           
    }
    else
    {
      //cancel transfer - send CAN twice
      this->sendChar(&xserial,XModem::XMO_CAN);
      this->sendChar(&xserial,XModem::XMO_CAN);
      //wait ACK
      if (this->dataRead(XModem::receiveDelay) == XModem::XMO_ACK)
        return true;
      else
        return false;
    }
    //send SOH
    this->sendChar(&xserial,XModem::XMO_SOH);
    //send frame number 
    this->sendChar(&xserial,this->blockNo);
    //send inv frame number
    this->sendChar(&xserial,(unsigned char)(255-(this->blockNo)));
    //send data
    for(int i = 0; i <128; i++)
      this->sendChar(&xserial,this->buffer[i]);
    //send checksum or crc
    if (transfer == ChkSum) {
      this->sendChar(&xserial,this->generateChkSum());
    } else {
      unsigned short crc;
      crc = this->crc16_ccitt(this->buffer, 128);
      
      this->sendChar(&xserial,(unsigned char)(crc >> 8));
      this->sendChar(&xserial,(unsigned char)(crc));
       
    }
   //TO DO - wait NACK or CAN or ACK
    int ret = this->dataRead(XModem::receiveDelay);
    switch(ret)
    {
      case XModem::XMO_ACK: //data is ok - go to next chunk
        this->blockNo++;
        this->blockNoExt++;
        continue;
      case XModem::XMO_NACK: //resend data
        continue;
      case XModem::XMO_CAN: //abort transmision
        return false;
    }  
  }
  return false;
}

bool XModem::transmit()
{
  int retry = 0;
  int sym;
  this->init();
  
  //wait for CRC transfer
  while(retry < 32)
  {
    if(this->dataAvail(1000))
    {
      sym = this->dataRead(1); //data is here - no delay
      if(sym == 'C')
      {
        bool ok = this->transmitFrames(Crc);
        xserial.flushAlways();
        return ok;
      }
      if(sym == XModem::XMO_NACK)
      {
        bool ok = this->transmitFrames(ChkSum);
        xserial.flushAlways();
        return ok;
      }
    }
    retry++;
  } 
  return false;
}

#endif
