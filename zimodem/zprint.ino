/*
   Copyright 2020-2020 Bo Zimmerman

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

size_t ZPrint::writeStr(char *s)
{
  if(current != null)
  {
    int len=strlen(s);
    for(int i=0;i<len;i++)
    {
      current->write(s[i]);
      logSocketOut(s[i]);
    }
    return len;
  }
  return 0;
}

size_t ZPrint::writeChunk(char *s, int len)
{
  char buf[8];
  int tot=len;
  itoa(len,buf,16);
  tot += strlen(buf);
  writeStr(buf);
  writeStr("\r\n");
  for(int i=0;i<len;i++)
  {
    current->write(s[i]);
    logSocketOut(s[i]);
  }
  writeStr("\r\n");
  return tot+4;
}

ZResult ZPrint::switchTo(char *vbuf, int vlen, bool petscii)
{
  if((vlen <= 2)||(vbuf[1]!=':'))
    return ZERROR;
  if(petscii)
  {
    for(int i=0;i<vlen;i++)
      vbuf[i] = petToAsc(vbuf[i]);
  }
  switch(vbuf[0])
  {
  case 'P': case 'p': 
    payloadType = PETSCII;
    break;
  case 'A': case 'a': 
    payloadType = ASCII;
    break;
  case 'R': case 'r': 
    payloadType = RAW;
    break;
  default:
    return ZERROR;
  }
  char *hostIp;
  char *req;
  int port;
  bool doSSL;
  if(!parseWebUrl((uint8_t *)vbuf+2,&hostIp,&req,&port,&doSSL))
    return ZERROR;
  current = new WiFiClientNode(hostIp,port,0);
  if(!current->isConnected())
  {
    delete current;
    return ZERROR;
  }
  currMode=&printMode;
  currentExpiresTimeMs = millis()+5000;
  
  // send the request and http headers:
  sprintf(pbuf,"POST %s HTTP/1.1\r\n",req);
  writeStr(pbuf);
  writeStr("Transfer-Encoding: chunked\r\n");
  writeStr("Content-Type: application/ipp\r\n");
  sprintf(pbuf,"Host: %s:%d\r\n",hostIp,port);
  writeStr(pbuf);
  writeStr("Connection: Keep-Alive\r\n");
  writeStr("User-Agent: Zimodem\r\n");
  writeStr("Accept-Encoding: gzip,deflate\r\n");
  writeStr("\r\n");
  // send the ipp header
  current->flush();
  sprintf(pbuf,"%c%c%c%c%c%c%c%c%c",0x01,0x01,0x00,0x02,0x00,0x00,0x00,0x01,0x01);
  writeChunk(pbuf,9);
  sprintf(pbuf,"%c%c%cattributes-charset%c%cutf-8",0x47,0x00,0x12,0x00,0x05);
  writeChunk(pbuf,28);
  sprintf(pbuf,"%c%c%cattributes-natural-language%c%cen-us",0x48,0x00,0x1b,0x00,0x05);
  writeChunk(pbuf,37);
  //TODO:ippAttribs.add(i.add(new byte[]{0x45,0x00,0x0b}).add("printer-uri").add(new byte[]{0x00,0x3b}).add("http://192.168.1.10/printers/HP_ColorLaserJet_MFP_M278-M281").toBytes());
  sprintf(pbuf,"%c%c%crequesting-user-name%c%czimodem",0x42,0x00,0x14,0x00,0x07);
  writeChunk(pbuf,32);
  sprintf(pbuf,"%c%c%cjob-name%c%czimodem-job",0x42,0x00,0x08,0x00,0x0b);
  writeChunk(pbuf,24);
  sprintf(pbuf,"%c%c%c%ccopies%c%c%c%c%c%c",0x02,0x21,0x00,0x06,0x00,0x04,0x00,0x00,0x00,0x01);
  writeChunk(pbuf,16);
  sprintf(pbuf,"%c%c%corientation-requested%c%c%c%c%c%c",0x23,0x00,0x15,0x00,0x04,0x00,0x00,0x00,0x03);
  writeChunk(pbuf,30);
  sprintf(pbuf,"%c%c%coutput-mode%c%cmonochrome%c",0x44,0x00,0x0b,0x00,0x0a,     0x03);
  writeChunk(pbuf,27);
  current->flush();
  pdex=0;
  return ZOK;
}

void ZPrint::serialIncoming()
{
  if(HWSerial.available() > 0)
  {
    while(HWSerial.available() >= 0)
    {
      uint8_t c=HWSerial.read();
      logSerialIn(c);
      if(payloadType == PETSCII)
        c=petToAsc(c);
      if(c != 0)
      {
        if(payloadType != RAW)
        {
          if(c<32)
          {
            if((c=='\r')||(c=='\n'))
              coldex=0;
          }
          else
          {
            coldex++;
            if(coldex > 80)
            {
              pbuf[pdex++]='\n';
              coldex=1;
            }
          }
        }
        pbuf[pdex++]=(char)c;
        if(pdex>=254)
        {
          if((current!=null)&&(current->isConnected()))
          {
            writeChunk(pbuf,pdex);
            current->flush();
          }
          pdex=0;
        }
      }
    }
    currentExpiresTimeMs = millis()+1000;
  }
}

void ZPrint::switchBackToCommandMode(bool error)
{
  if(current != null)
  {
    serial.prints(commandMode.EOLN);
    if(error)
      serial.prints("ERROR");
    else
      serial.prints("OK");
    serial.prints(commandMode.EOLN);
    delete current;
  }
  current = null;
  currMode = &commandMode;
}

void ZPrint::loop()
{
  if((current==null) || (!current->isConnected()))
    switchBackToCommandMode(true);
  else
  if(millis()>currentExpiresTimeMs)
  {
    if(pdex > 0)
      writeChunk(pbuf,pdex);
    writeStr("0\r\n\r\n");
    current->flush();
    switchBackToCommandMode(false);
  }
  checkBaudChange();
}

