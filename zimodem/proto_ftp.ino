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
bool parseFTPUrl(uint8_t *vbuf, char **hostIp, char **req, int *port, bool *doSSL, char **username, char **pw)
{
  *doSSL = false;
  if(strstr((char *)vbuf,"ftp:")==(char *)vbuf)
    vbuf = vbuf + 4;
  else
  if(strstr((char *)vbuf,"ftps:")==(char *)vbuf)
  {
    vbuf = vbuf + 5;
    *doSSL = true;
  }
  while(*vbuf == '/')
    vbuf++;

  *port= 21;
  *hostIp = (char *)vbuf;
  char *atSign=strchr((char *)vbuf,'@');
  *username=NULL;
  *pw = NULL;
  if(atSign != NULL)
  {
    *hostIp = atSign + 1;
    *atSign = 0;
    *username = (char *)vbuf;
    vbuf = (uint8_t *)(atSign + 1);
    char *pwB = strchr(*username, ':');
    if(pwB != NULL)
    {
      *pw = pwB+1;
      *pwB=0;
    }
  }
  char *portB=strchr((char *)vbuf,':');
  *req = strchr((char *)vbuf,'/');
  if(*req != NULL)
  {
    *(*req)=0;
    (*req)++;
  }
  else
  {
    int len=strlen((char *)vbuf);
    *req = (char *)(vbuf + len);
  }
  if(portB != NULL)
  {
     *portB = 0;
     portB++;
     *port = atoi(portB);
     if(port <= 0)
       return ZERROR;
  }
  return true;
}

static bool doFTPQuit(WiFiClient **c)
{
  if((*c)->connected())
  {
    (*c)->printf("QUIT\r\n");
    delay(500);
  }
  (*c)->stop();
  delete (*c);
  return false;
}

static String readLine(WiFiClient *c, int timeout)
{
  long now=millis();
  String line = "";
  while(((millis()-now < timeout) || (c->available()>0)) && (c->connected()))
  {
    yield();
    if(c->available()>0)
    {
      char ch=c->read();
      if((ch=='\n')||(ch=='\r'))
      {
          if(line.length()>0)
            return line;
      }
      else
      if((ch >= 32 ) && (ch <= 127))
        line += ch;
      now=millis();
    }
  }
  return line;
}

static int getFTPResponseCode(WiFiClient *c, char *buf)
{
  String resp = readLine(c,5000);
  if(resp.length() == 0)
    return -1; // timeout total
  while((resp.length()<3)
      ||(resp[0] < '0')||(resp[0] > '9')
      ||(resp[1] < '0')||(resp[1] > '9')
      ||(resp[2] < '0')||(resp[2] > '9'))
  {
      yield();
      resp = readLine(c,1000);
      if(resp.length()==0)
        return -1;
  }
  if((buf != NULL)&&(resp.length()<132))
    strcpy(buf,resp.substring(4).c_str());
  return atoi(resp.substring(0,3).c_str());
}

void readBytesToSilence(WiFiClient *cc)
{
  long now = millis(); // just eat the intro for 1 second of silence
  while(millis()-now < 1000)
  {
    yield();
    if(cc->available()>0)
    {
      cc->read();
      now=millis();
    }
  }
}

bool doFTPGet(FS *fs, const char *hostIp, int port, const char *filename, const char *req, const char *username, const char *pw, const bool doSSL)
{
  WiFiClient *cc = createWiFiClient(doSSL);
  if(WiFi.status() != WL_CONNECTED)
    return false;
  cc->setNoDelay(DEFAULT_NO_DELAY);
  if(!cc->connect(hostIp, port))
    return doFTPQuit(&cc);
  readBytesToSilence(cc);
  if(username == NULL)
    cc->printf("USER anonymous\r\n");
  else
    cc->printf("USER %s\r\n",username);
  int respCode = getFTPResponseCode(cc, NULL);
  if(respCode != 331)
    return doFTPQuit(&cc);
  if(pw == NULL)
    cc->printf("PASS zimodem@zimtime.net\r\n");
  else
    cc->printf("PASS %s\r\n",pw);
  respCode = getFTPResponseCode(cc, NULL);
  if(respCode != 230)
    return doFTPQuit(&cc);
  readBytesToSilence(cc);
  cc->printf("TYPE I\r\n");
  respCode = getFTPResponseCode(cc, NULL);
  if(respCode < 0)
    return doFTPQuit(&cc);
  char ipbuf[129];
  cc->printf("PASV\r\n");
  respCode = getFTPResponseCode(cc, ipbuf);
  if(respCode != 227)
    return doFTPQuit(&cc);
  // now parse the pasv result in .* (ipv4,ipv4,ipv4,ipv4,
  char *ipptr = strchr(ipbuf,'(');
  while((ipptr != NULL) && (strchr(ipptr+1,'(')!= NULL))
    ipptr=strchr(ipptr+1,'(');
  if(ipptr == NULL)
    return doFTPQuit(&cc);
  int digitCount=0;
  int digits[10];
  char *commaPtr=strchr(ipptr+1,',');
  while((commaPtr != NULL)&&(digitCount < 10))
  {
    *commaPtr = 0;
    digits[digitCount++] = atoi(ipptr+1);
    ipptr=commaPtr;
    commaPtr=strchr(ipptr+1,',');
    if(commaPtr == NULL)
      commaPtr=strchr(ipptr+1,')');
  }
  if(digitCount < 6)
    return doFTPQuit(&cc);
  sprintf(ipbuf,"%d.%d.%d.%d",digits[0],digits[1],digits[2],digits[3]);
  int dataPort = (256 * digits[4]) + digits[5];
  // ok, now we are ready for DATA!
  if(WiFi.status() != WL_CONNECTED)
    return doFTPQuit(&cc);
  WiFiClient *c = createWiFiClient(doSSL);
  c->setNoDelay(DEFAULT_NO_DELAY);
  if(!c->connect(ipbuf, dataPort))
  {
    doFTPQuit(&c);
    return doFTPQuit(&cc);
  }
  cc->printf("RETR %s\r\n",req);
  respCode = getFTPResponseCode(cc, NULL);
  if((respCode < 0)||(respCode > 400))
    return doFTPQuit(&cc);
  File f = fs->open(filename, "w");
  long now=millis();
  while((c->connected()) && ((millis()-now) < 30000)) // loop for data, with nice long timeout
  {
    if(c->available()>=0)
    {
      now=millis();
      uint8_t ch=c->read();
      //logSocketIn(ch); // this is ALSO not socket input!
      f.write(ch);
    }
    else
      yield();
  }
  f.flush();
  f.close();
  c->stop();
  delete c;
  doFTPQuit(&cc);
  return true;
}

bool doFTPPut(File &f, const char *hostIp, int port, const char *req, const char *username, const char *pw, const bool doSSL)
{
  WiFiClient *cc = createWiFiClient(doSSL);
  if(WiFi.status() != WL_CONNECTED)
    return false;
  cc->setNoDelay(DEFAULT_NO_DELAY);
  if(!cc->connect(hostIp, port))
    return doFTPQuit(&cc);
  readBytesToSilence(cc);
  if(username == NULL)
    cc->printf("USER anonymous\r\n");
  else
    cc->printf("USER %s\r\n",username);
  int respCode = getFTPResponseCode(cc, NULL);
  if(respCode != 331)
    return doFTPQuit(&cc);
  if(pw == NULL)
    cc->printf("PASS zimodem@zimtime.net\r\n");
  else
    cc->printf("PASS %s\r\n",pw);
  respCode = getFTPResponseCode(cc, NULL);
  if(respCode != 230)
    return doFTPQuit(&cc);
  readBytesToSilence(cc);
  cc->printf("TYPE I\r\n");
  respCode = getFTPResponseCode(cc, NULL);
  if(respCode < 0)
    return doFTPQuit(&cc);
  char ipbuf[129];
  cc->printf("PASV\r\n");
  debugPrintf("PASV\r\n");
  respCode = getFTPResponseCode(cc, ipbuf);
  if(respCode != 227)
    return doFTPQuit(&cc);
  // now parse the pasv result in .* (ipv4,ipv4,ipv4,ipv4,
  char *ipptr = strchr(ipbuf,'(');
  while((ipptr != NULL) && (strchr(ipptr+1,'(')!= NULL))
    ipptr=strchr(ipptr+1,'(');
  if(ipptr == NULL)
    return doFTPQuit(&cc);
  int digitCount=0;
  int digits[10];
  char *commaPtr=strchr(ipptr+1,',');
  while((commaPtr != NULL)&&(digitCount < 10))
  {
    *commaPtr = 0;
    digits[digitCount++] = atoi(ipptr+1);
    ipptr=commaPtr;
    commaPtr=strchr(ipptr+1,',');
    if(commaPtr == NULL)
      commaPtr=strchr(ipptr+1,')');
  }
  if(digitCount < 6)
    return doFTPQuit(&cc);
  sprintf(ipbuf,"%d.%d.%d.%d",digits[0],digits[1],digits[2],digits[3]);
  debugPrintf(ipbuf,"%d.%d.%d.%d",digits[0],digits[1],digits[2],digits[3]);
  int dataPort = (256 * digits[4]) + digits[5];
  // ok, now we are ready for DATA!
  if(WiFi.status() != WL_CONNECTED)
    return doFTPQuit(&cc);
  WiFiClient *c = createWiFiClient(doSSL);
  c->setNoDelay(DEFAULT_NO_DELAY);
  if(!c->connect(ipbuf, dataPort))
  {
    doFTPQuit(&c);
    return doFTPQuit(&cc);
  }
  debugPrintf(" STOR %s\r\n",req);
  cc->printf("STOR %s\r\n",req);
  respCode = getFTPResponseCode(cc, NULL);
  if((respCode < 0)||(respCode > 400))
    return doFTPQuit(&cc);
  long now=millis();
  debugPrintf(" Storing... %d\r\n",f.available());
  while((c->connected()) && (f.available()>0) && ((millis()-now) < 30000)) // loop for data, with nice long timeout
  {
    if(f.available()>=0)
    {
      now=millis();
      uint8_t ch=f.read();
      //logSocketIn(ch); // this is ALSO not socket input!
      c->write(ch);
    }
    else
      yield();
  }
  debugPrintf("FPUT: Done\r\n");
  c->flush();
  c->stop();
  delete c;
  doFTPQuit(&cc);
  return true;
}

bool doFTPLS(ZSerial *serial, const char *hostIp, int port, const char *req, const char *username, const char *pw, const bool doSSL)
{
  WiFiClient *cc = createWiFiClient(doSSL);
  if(WiFi.status() != WL_CONNECTED)
    return false;
  cc->setNoDelay(DEFAULT_NO_DELAY);
  if(!cc->connect(hostIp, port))
    return doFTPQuit(&cc);
  readBytesToSilence(cc);
  if(username == NULL)
    cc->printf("USER anonymous\r\n");
  else
    cc->printf("USER %s\r\n",username);
  int respCode = getFTPResponseCode(cc, NULL);
  if(respCode != 331)
    return doFTPQuit(&cc);
  if(pw == NULL)
    cc->printf("PASS zimodem@zimtime.net\r\n");
  else
    cc->printf("PASS %s\r\n",pw);
  respCode = getFTPResponseCode(cc, NULL);
  if(respCode != 230)
    return doFTPQuit(&cc);
  readBytesToSilence(cc);
  cc->printf("TYPE A\r\n");
  respCode = getFTPResponseCode(cc, NULL);
  if(respCode < 0)
    return doFTPQuit(&cc);
  if((req != NULL)&& (*req != NULL))
  {
    cc->printf("CWD %s\r\n",req);
    respCode = getFTPResponseCode(cc, NULL);
    if((respCode < 0)||(respCode > 400))
      return doFTPQuit(&cc);
    readBytesToSilence(cc);
  }
  char ipbuf[129];
  cc->printf("PASV\r\n");
  respCode = getFTPResponseCode(cc, ipbuf);
  if(respCode != 227)
    return doFTPQuit(&cc);
  // now parse the pasv result in .* (ipv4,ipv4,ipv4,ipv4,
  char *ipptr = strchr(ipbuf,'(');
  while((ipptr != NULL) && (strchr(ipptr+1,'(')!= NULL))
    ipptr=strchr(ipptr+1,'(');
  if(ipptr == NULL)
    return doFTPQuit(&cc);
  int digitCount=0;
  int digits[10];
  char *commaPtr=strchr(ipptr+1,',');
  while((commaPtr != NULL)&&(digitCount < 10))
  {
    *commaPtr = 0;
    digits[digitCount++] = atoi(ipptr+1);
    ipptr=commaPtr;
    commaPtr=strchr(ipptr+1,',');
    if(commaPtr == NULL)
      commaPtr=strchr(ipptr+1,')');
  }
  if(digitCount < 6)
    return doFTPQuit(&cc);
  sprintf(ipbuf,"%d.%d.%d.%d",digits[0],digits[1],digits[2],digits[3]);
  int dataPort = (256 * digits[4]) + digits[5];
  // ok, now we are ready for DATA!
  if(WiFi.status() != WL_CONNECTED)
    return doFTPQuit(&cc);
  WiFiClient *c = createWiFiClient(doSSL);
  c->setNoDelay(DEFAULT_NO_DELAY);
  if(!c->connect(ipbuf, dataPort))
  {
    doFTPQuit(&c);
    return doFTPQuit(&cc);
  }
  cc->printf("LIST\r\n",req);
  respCode = getFTPResponseCode(cc, NULL);
  if((respCode < 0)||(respCode > 400))
    return doFTPQuit(&cc);
  long now=millis();
  while((c->connected()) && ((millis()-now) < 30000)) // loop for data, with nice long timeout
  {
    if(c->available()>=0)
    {
      now=millis();
      uint8_t ch=c->read();
      //logSocketIn(ch); // this is ALSO not socket input!
      serial->print((char)ch);
    }
    else
      yield();
  }
  c->stop();
  delete c;
  doFTPQuit(&cc);
  return true;
}
#endif