/*
   Copyright 2020-2026 Bo Zimmerman

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
  if(outStream != null)
  {
    int len=strlen(s);
    outStream->write((uint8_t *)s,len);
    if(logFileOpen)
    {
      for(int i=0;i<len;i++)
        logSocketOut(s[i]);
    }
    return len;
  }
  return 0;
}

char *ZPrint::getLastPrinterSpec()
{
  if(lastPrinterSpec==0)
    return "";
  return lastPrinterSpec;
}

void ZPrint::setLastPrinterSpec(const char *spec)
{
  if(lastPrinterSpec != 0)
    free(lastPrinterSpec);
  lastPrinterSpec = 0;
  if((spec != 0) && (*spec != 0))
  {
    int newLen = strlen(spec);
    lastPrinterSpec = (char *)malloc(newLen + 1);
    strcpy(lastPrinterSpec,spec);
  }
}


int ZPrint::getTimeoutDelayMs()
{
  return timeoutDelayMs;
}

void ZPrint::setTimeoutDelayMs(int ms)
{
  if(ms > 50)
    timeoutDelayMs = ms;
}

size_t ZPrint::writeChunk(char *s, int len)
{
  char buf[25];
  sprintf(buf,"%x\r\n",len);
  writeStr(buf);
  outStream->write((uint8_t *)s,len);
  if(logFileOpen)
  {
    for(int i=0;i<len;i++)
      logSocketOut(s[i]);
  }
  writeStr("\r\n");
  yield();
  return len+strlen(buf)+4;
}

void ZPrint::announcePrintJob(const char *hostIp, const int port, const char *req)
{
  logPrintfln("Print Request to host=%s, port=%d",hostIp,port);
  logPrintfln("Print Request is /%s",req);
}

ZResult ZPrint::switchToPostScript(char *prefix)
{
  if((lastPrinterSpec==0)
  ||(strlen(lastPrinterSpec)<=5)
  ||(lastPrinterSpec[1]!=':'))
    return ZERROR;
  char *workBuf = (char *)malloc(strlen(lastPrinterSpec) +1);
  strcpy(workBuf, lastPrinterSpec);
  char *hostIp;
  char *req;
  int port;
  bool doSSL;
  if(!parseWebUrl((uint8_t *)workBuf+2,&hostIp,&req,&port,&doSSL))
  {
    free(workBuf);
    return ZERROR;
  }
  payloadType = RAW;
  announcePrintJob(hostIp,port,req);
  ZResult result;
  if(pinSupport[pinRTS])
    s_pinWrite(pinRTS, rtsInactive);
  yield();
  wifiSock = new WiFiClientNode(hostIp,port,doSSL?FLAG_SECURE:0);
  wifiSock->setNoDelay(false); // we want a delay in this case
  outStream = wifiSock;
  result = finishSwitchTo(hostIp, req, port, doSSL, payloadType);
  if(result == ZERROR)
  {
    outStream = null;
    delete wifiSock;
    wifiSock = null;
  }
  yield();
  if(pinSupport[pinRTS])
    s_pinWrite(pinRTS, rtsActive);
  if((result != ZERROR)
  &&(prefix != 0)
  &&(strlen(prefix)>0))
  {
    writeChunk(prefix,strlen(prefix));
    serialIncoming();
  }
  free(workBuf);
  return result;
}

bool ZPrint::testPrinterSpec(const char *vbuf, int vlen, bool petscii)
{
  char *workBuf = (char *)malloc(vlen+1);
  strcpy(workBuf, vbuf);
  if(petscii)
  {
    for(int i=0;i<vlen;i++)
      workBuf[i] = petToAsc(workBuf[i]);
  }
  if((vlen <= 2)||(workBuf[1]!=':'))
  {
    free(workBuf);
    return false;
  }
  
  switch(workBuf[0])
  {
  case 'P': case 'p': 
  case 'A': case 'a': 
  case 'R': case 'r': 
    //yay
    break;
  default:
    free(workBuf);
    return false;
  }
  char *hostIp;
  char *req;
  int port;
  bool doSSL;
  if(!parseWebUrl((uint8_t *)workBuf+2,&hostIp,&req,&port,&doSSL))
  {
    free(workBuf);
    return false;
  }
  free(workBuf);
  return true;
}

ZResult ZPrint::switchTo(char *vbuf, int vlen, bool petscii)
{
  debugPrintf("\r\nMode:Print\r\n");
  char *workBuf = (char *)malloc(vlen+1);
  strcpy(workBuf, vbuf);
  if(petscii)
  {
    for(int i=0;i<vlen;i++)
      workBuf[i] = petToAsc(workBuf[i]);
  }
  bool newUrl = true;
  if((vlen <= 2)||(workBuf[1]!=':'))
  {
    if((lastPrinterSpec==0)
    ||(strlen(lastPrinterSpec)<=5)
    ||(lastPrinterSpec[1]!=':'))
    {
      free(workBuf);
      return ZERROR;
    }
    if((vlen == 1)
    &&(strchr("parPAR",workBuf[0])!=0))
      lastPrinterSpec[0]=workBuf[0];
    free(workBuf);
    workBuf = (char *)malloc(strlen(lastPrinterSpec) +1);
    strcpy(workBuf, lastPrinterSpec);
    vlen=strlen(workBuf);
    newUrl = false;
  }
  
  switch(workBuf[0])
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
    free(workBuf);
    return ZERROR;
  }
  char *hostIp;
  char *req;
  int port;
  bool doSSL;
  if(!parseWebUrl((uint8_t *)workBuf+2,&hostIp,&req,&port,&doSSL))
  {
    free(workBuf);
    return ZERROR;
  }
  if(newUrl)
    setLastPrinterSpec(vbuf);

  wifiSock = new WiFiClientNode(hostIp,port,doSSL?FLAG_SECURE:0);
  wifiSock->setNoDelay(false); // we want a delay in this case
  outStream = wifiSock;
  announcePrintJob(hostIp,port,req);
  ZResult result = finishSwitchTo(hostIp, req, port, doSSL, payloadType);
  free(workBuf);
  if(result == ZERROR)
    delete wifiSock;
  return result;
}

int ZPrint::buildIPPAttribute(char *buf, uint8_t tag, const char *name, const char *value, int valueLen)
{
  int pos = 0;
  int nameLen = strlen(name);
  buf[pos++] = tag;
  buf[pos++] = (nameLen >> 8) & 0xFF;
  buf[pos++] = nameLen & 0xFF;
  memcpy(buf + pos, name, nameLen);
  pos += nameLen;
  buf[pos++] = (valueLen >> 8) & 0xFF;
  buf[pos++] = valueLen & 0xFF;
  memcpy(buf + pos, value, valueLen);
  pos += valueLen;
  return pos;
}

ZResult ZPrint::finishSwitchTo(char *hostIp, char *req, int port, bool doSSL, PrintPayloadType pType)
{
  if((wifiSock != null) && (!wifiSock->isConnected()))
  {
    debugPrintf("Connection to printer lost.");
    return ZERROR;
  }
  char portStr[10];
  sprintf(portStr,"%d",port);
  // send the request and http headers:
  sprintf(pbuf,"POST /%s HTTP/1.1\r\n",req);
  writeStr(pbuf);
  writeStr("Transfer-Encoding: chunked\r\n");
  writeStr("Content-Type: application/ipp\r\n");
  sprintf(pbuf,"Host: %s:%d\r\n",hostIp,port);
  writeStr(pbuf);
  writeStr("Connection: Keep-Alive\r\n");
  writeStr("User-Agent: Zimodem\r\n");
  writeStr("Accept-Encoding: gzip,deflate\r\n");
  writeStr("\r\n");
  outStream->flush();
  // send the ipp header
  if((wifiSock != null)&&(!wifiSock->isConnected()))
  {
    debugPrintf("Connection to printer lost after request.");
    return ZERROR;
  }
  
  char jobChar1 = '0' + (jobNum / 10);
  char jobChar2 = '0' + (jobNum % 10);
  if(++jobNum>94)
    jobNum=0;

  // IPP version (1.1), operation-id (Print-Job = 0x0002), request-id (4 bytes)
  int requestId = jobNum + 1;
  sprintf(pbuf,"%c%c%c%c%c%c%c%c%c",
    0x01, 0x01,                           // version 1.1
    0x00, 0x02,                           // operation: Print-Job
    0x00, 0x00, 0x00, (char)requestId,   // request-id (big-endian)
    0x01);                                // operation-attributes-group tag
  writeChunk(pbuf,9);

  // attributes-charset
  int chunkLen = buildIPPAttribute(pbuf, 0x47, "attributes-charset", "utf-8", 5);
  writeChunk(pbuf, chunkLen);

  chunkLen = buildIPPAttribute(pbuf, 0x48, "attributes-natural-language", "en-us", 5);
  writeChunk(pbuf, chunkLen);

  char uriBuffer[256];
  sprintf(uriBuffer, "http://%s:%s/%s", hostIp, portStr, req);
  int urllen = strlen(uriBuffer);
  chunkLen = buildIPPAttribute(pbuf, 0x45, "printer-uri", uriBuffer, urllen);
  writeChunk(pbuf, chunkLen);

  chunkLen = buildIPPAttribute(pbuf, 0x42, "requesting-user-name", "zimodem", 7);
  writeChunk(pbuf, chunkLen);

  sprintf(pbuf,"%c", 0x02);
  writeChunk(pbuf,1);

  char jobName[12];
  sprintf(jobName, "zimodem-j%c%c", jobChar1, jobChar2);
  chunkLen = buildIPPAttribute(pbuf, 0x42, "job-name", jobName, 11);
  writeChunk(pbuf, chunkLen);

  char intValue[4] = {0x00, 0x00, 0x00, 0x01};
  chunkLen = buildIPPAttribute(pbuf, 0x21, "copies", intValue, 4);
  writeChunk(pbuf, chunkLen);

  char orientValue[4] = {0x00, 0x00, 0x00, 0x03};
  chunkLen = buildIPPAttribute(pbuf, 0x23, "orientation-requested", orientValue, 4);
  writeChunk(pbuf, chunkLen);

  chunkLen = buildIPPAttribute(pbuf, 0x44, "output-mode", "monochrome", 10);
  writeChunk(pbuf, chunkLen);

  const char* mimeType;
  int mimeTypeLen;
  switch(pType)
  {
    case PETSCII:
    case ASCII:
      mimeType = "text/plain";
      mimeTypeLen = 10;
      break;
    case RAW:
    default:
      mimeType = "application/octet-stream";
      mimeTypeLen = 24;
      break;
  }
  chunkLen = buildIPPAttribute(pbuf, 0x49, "document-format", mimeType, mimeTypeLen);
  writeChunk(pbuf, chunkLen);

  sprintf(pbuf,"%c", 0x03);
  writeChunk(pbuf,1);

  outStream->flush();
  if((wifiSock != null)&&(!wifiSock->isConnected()))
  {
    debugPrintf("Connection to printer lost after job.");
    return ZERROR;
  }
  checkOpenConnections();
  checkBaudChange();
  pdex=0;
  coldex=0;
  currentExpiresTimeMs = millis()+5000;
  currMode=&printMode;

  debugPrintf("Print Server ready for data.");
  return ZIGNORE;
}

void ZPrint::serialIncoming()
{
  if(HWSerial.available() > 0)
  {
    while(HWSerial.available() > 0)
    {
      uint8_t c=HWSerial.read();
      logSerialIn(c);
      int plusOut = processPlusPlusPlus(c);
      if(plusOut > 0)
      {
        for(int i=0;i<plusOut;i++)
          pbuf[pdex++]=commandMode.EC;
      }
      
      if(payloadType == PETSCII)
        c=petToAsc(c);
      if(payloadType != RAW)
      {
        if(c==0)
          continue;
        else
        if(c<32)
        {
          if((c=='\r')||(c=='\n'))
          {
            coldex=0;
          }
        }
        else
        {
          coldex++;
          if(coldex > 80)
          {
            pbuf[pdex++]='\n';
            pbuf[pdex++]='\r';
            coldex=1;
          }
          else
          if((lastC == '\n')&&(lastLastC!='\r'))
              pbuf[pdex++]='\r';
          else
          if((lastC == '\r')&&(lastLastC!='\n'))
              pbuf[pdex++]='\n';
        }
      }
      pbuf[pdex++]=(char)c;
      lastLastC=lastC;
      lastC=c;
      if(pdex>=250)
      {
        if(((wifiSock!=null)&&(wifiSock->isConnected()))
        ||((wifiSock==null)&&(outStream!=null)))
        {
          writeChunk(pbuf,pdex);
          //wifiSock->flush();
        }
        pdex=0;
      }
    }
    currentExpiresTimeMs = millis()+timeoutDelayMs;
  }
}

void ZPrint::switchBackToCommandMode(bool error)
{
  debugPrintf("\r\nMode:Command\r\n");
  if((wifiSock != null)||(outStream!=null))
  {
    if(error)
      commandMode.sendOfficialResponse(ZERROR);
    else
      commandMode.sendOfficialResponse(ZOK);
    if(wifiSock != null)
      delete wifiSock;
  }
  wifiSock = null;
  outStream = null;
  currMode = &commandMode;
}

void ZPrint::loop()
{
  if(((wifiSock==null)&&(outStream==null))
  || ((wifiSock!=null)&&(!wifiSock->isConnected())))
  {
    debugPrintf("No printer connection: %d %d %d\r\n",(wifiSock == null),(outStream==null),((wifiSock!=null)&&(!wifiSock->isConnected())));
    switchBackToCommandMode(true);
  }
  else
  if((checkPlusPlusPlusEscape())
  ||(millis()>currentExpiresTimeMs))
  {
    debugPrintf("Time-out in printing\r\n");
    if(pdex > 0)
      writeChunk(pbuf,pdex);
    writeStr("0\r\n\r\n");
    outStream->flush();
    switchBackToCommandMode(false);
  }
  checkBaudChange();
  logFileLoop();
}

