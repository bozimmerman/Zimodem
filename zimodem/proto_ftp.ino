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
#ifdef INCLUDE_FTP

FTPHost::~FTPHost()
{
  freeCharArray(&hostIp);
  freeCharArray(&username);
  freeCharArray(&pw);
  freeCharArray(&path);
  freeCharArray(&file);
  if(streams != 0)
  {
    delete streams;
    streams=0;
  }
}

bool FTPHost::doGet(FS *fs, const char *localpath, const char *remotepath)
{
  fixPath(remotepath);
  return doFTPGet(fs,hostIp,port,localpath,file,username,pw,doSSL);
}

bool FTPHost::doPut(File &f, const char *remotepath)
{
  fixPath(remotepath);
  return doFTPPut(f,hostIp,port,file,username,pw,doSSL);
}

bool FTPHost::doLS(ZSerial *serial, const char *remotepath)
{
  fixPath(remotepath);
  bool kaplah = doFTPLS(serial,hostIp,port,file,username,pw,doSSL);
  if((kaplah) 
  && (strcmp(file,path) != 0)
  && (strlen(file)>0))
  {
    if(file[strlen(file)-1]=='/')
      setCharArray(&path, file);
    else
    {
      char buf[strlen(file)+2];
      sprintf(buf,"%s/",file);
      setCharArray(&path, buf);
    }
  }
  return kaplah;
}

void FTPHost::fixPath(const char *remotepath)
{
  if(remotepath == 0)
    return;
  char *end = strrchr(remotepath, '/');
  int buflen = ((path == 0) ? 0 : strlen(path)) + strlen(remotepath) + 3;
  char fbuf[buflen];
  char pbuf[buflen];
  if(remotepath[0] == '/')
  {
    strcpy(fbuf, remotepath);
    if(end != 0)
    {
      *end = 0;
      sprintf(pbuf,"%s/",remotepath);
    }
    else
      strcpy(pbuf, "/");
  }
  else
  {
    sprintf(fbuf,"%s%s",path,remotepath);
    if(end != 0)
    {
      *end = 0;
      sprintf(pbuf,"%s%s/",path,remotepath);
    }
    else
      strcpy(pbuf, path);
  }
  setCharArray(&path, pbuf);
  setCharArray(&file, fbuf);
}

bool FTPHost::parseUrl(uint8_t *vbuf, char **req)
{
  char *newHostIp;
  int newPort;
  bool newDoSSL;
  char *newUsername;
  char *newPassword;
  if(parseFTPUrl(vbuf,&newHostIp,req,&newPort,&newDoSSL,&newUsername,&newPassword))
  {
    setCharArray(&hostIp,newHostIp);
    port = newPort;
    doSSL = newDoSSL;
    setCharArray(&username,newUsername);
    setCharArray(&pw,newPassword);
    setCharArray(&path,"/");
    setCharArray(&file,"");
    return true;
  }
  return false;
}

FTPHost *makeFTPHost(bool isUrl, FTPHost *host, uint8_t *buf, char **req)
{
  if(isUrl)
  {
    if(host != 0)
      delete host;
    host = new FTPHost();
    if(!(host->parseUrl(buf,req)))
    {
      delete host;
      host=0;
      *req=0;
    }
  }
  else
    *req=(char *)buf;
  return host;
}

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
  int len=strlen((char *)vbuf);
  *req = strchr((char *)vbuf,'/');
  if(*req != NULL)
  {
    *(*req)=0;
    if((*req) != (char *)(vbuf+len-1))
      (*req)++;
  }
  else
    *req = (char *)(vbuf + len);
  if(portB != NULL)
  {
     *portB = 0;
     portB++;
     *port = atoi(portB);
     if(*port <= 0)
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
  unsigned long now=millis();
  String line = "";
  while(((millis()-now < timeout) || (c->available()>0)) 
  && (c->connected()|| (c->available()>0)))
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
  if(buf != NULL)
  {
    int start = 4;
    if(resp.length()>=132)
      start=4+(resp.length()-132);
    strcpy(buf,resp.substring(start).c_str());
  }
  return atoi(resp.substring(0,3).c_str());
}

void readBytesToSilence(WiFiClient *cc)
{
  unsigned long now = millis(); // just eat the intro for 1 second of silence
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

FTPClientPair::~FTPClientPair()
{
  if(cmdClient != 0)
  {
    doFTPQuit(&cmdClient);
    cmdClient = 0;
  }
  if(dataClient != 0)
  {
    dataClient->stop();
    delete dataClient;
    dataClient = 0;
  }
}

static bool doFTPLogin(WiFiClient *cc, const char *hostIp, int port, const char *username, const char *pw)
{
  if(WiFi.status() != WL_CONNECTED)
    return false;
  if(!cc->connect(hostIp, port))
    return doFTPQuit(&cc);
  cc->setNoDelay(DEFAULT_NO_DELAY);
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
  return true;
}

static WiFiClient *doFTPPassive(WiFiClient *cc, bool doSSL)
{
  char ipbuf[129];
  cc->printf("PASV\r\n");
  int respCode = getFTPResponseCode(cc, ipbuf);
  if(respCode != 227)
  {
    doFTPQuit(&cc);
    return 0;
  }
  // now parse the pasv result in .* (ipv4,ipv4,ipv4,ipv4,
  char *ipptr = strchr(ipbuf,'(');
  while((ipptr != NULL) && (strchr(ipptr+1,'(')!= NULL))
    ipptr=strchr(ipptr+1,'(');
  if(ipptr == NULL)
  {
    doFTPQuit(&cc);
    return 0;
  }
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
  {
    doFTPQuit(&cc);
    return 0;
  }
  sprintf(ipbuf,"%d.%d.%d.%d",digits[0],digits[1],digits[2],digits[3]);
  int dataPort = (256 * digits[4]) + digits[5];
  // ok, now we are ready for DATA!
  if(WiFi.status() != WL_CONNECTED)
  {
    doFTPQuit(&cc);
    return 0;
  }
  WiFiClient *c = createWiFiClient(doSSL);
  if(!c->connect(ipbuf, dataPort))
  {
    doFTPQuit(&c);
    doFTPQuit(&cc);
    return 0;
  }
  c->setNoDelay(DEFAULT_NO_DELAY);
  return c;
}

FTPClientPair *doFTPGetStream(const char *hostIp, int port, const char *remotepath, const char *username, const char *pw, bool doSSL, uint32_t *responseSize)
{
  *responseSize = 0;
  WiFiClient *cc = createWiFiClient(doSSL);
  if(!doFTPLogin(cc, hostIp, port, username, pw))
    return 0;
  cc->printf("TYPE I\r\n");
  int respCode = getFTPResponseCode(cc, NULL);
  if(respCode < 0)
  {
    doFTPQuit(&cc);
    return 0;
  }
  WiFiClient *c = doFTPPassive(cc, doSSL);
  if(c == 0)
    return 0;
  cc->printf("RETR %s\r\n",remotepath);
  char lbuf[133];
  respCode = getFTPResponseCode(cc, lbuf);
  if((respCode < 0)||(respCode > 400))
  {
    c->stop();
    delete c;
    doFTPQuit(&cc);
    return 0;
  }
  if(respCode == 150)
  {
    char *eob=strstr(lbuf," bytes");
    if(eob)
    {
      *eob=0;
      char *sob=eob-1;
      while(strchr("0123456789",*sob) && (sob > lbuf))
        sob--;
      if((sob<(eob-1))&&(sob>lbuf))
        *responseSize=atoi(sob+1);
    }
  }
  FTPClientPair *pair = (FTPClientPair *)malloc(sizeof(FTPClientPair));
  pair->cmdClient = cc;
  pair->dataClient = c;
  return pair;
}

WiFiClient *FTPHost::doGetStream(const char *remotepath, uint32_t *responseSize)
{
  streams = doFTPGetStream(hostIp,port,remotepath,username,pw,doSSL,responseSize);
  if(streams != 0)
    return streams->dataClient;
  return 0;
}

bool doFTPGet(FS *fs, const char *hostIp, int port, const char *localpath, const char *remotepath, const char *username, const char *pw, const bool doSSL)
{
  WiFiClient *cc = createWiFiClient(doSSL);
  if(!doFTPLogin(cc, hostIp, port, username, pw))
    return false;
  cc->printf("TYPE I\r\n");
  int respCode = getFTPResponseCode(cc, NULL);
  if(respCode < 0)
    return doFTPQuit(&cc);
  WiFiClient *c = doFTPPassive(cc, doSSL);
  if(c == 0)
    return false;
  cc->printf("RETR %s\r\n",remotepath);
  respCode = getFTPResponseCode(cc, NULL);
  if((respCode < 0)||(respCode > 400))
  {
    c->stop();
    delete c;
    return doFTPQuit(&cc);
  }
  File f = fs->open(localpath, "w");
  unsigned long now=millis();
  while((c->connected()||(c->available()>0)) 
  && ((millis()-now) < 30000)) // loop for data, with nice long timeout
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

bool doFTPPut(File &f, const char *hostIp, int port, const char *remotepath, const char *username, const char *pw, const bool doSSL)
{
  WiFiClient *cc = createWiFiClient(doSSL);
  if(!doFTPLogin(cc, hostIp, port, username, pw))
    return false;
  cc->printf("TYPE I\r\n");
  int respCode = getFTPResponseCode(cc, NULL);
  if(respCode < 0)
    return doFTPQuit(&cc);
  WiFiClient *c = doFTPPassive(cc, doSSL);
  if(c == 0)
    return false;
  cc->printf("STOR %s\r\n",remotepath);
  respCode = getFTPResponseCode(cc, NULL);
  if((respCode < 0)||(respCode > 400))
  {
    c->stop();
    delete c;
    return doFTPQuit(&cc);
  }
  unsigned long now=millis();
  while((c->connected()) 
  && (f.available()>0) && ((millis()-now) < 30000)) // loop for data, with nice long timeout
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
  c->flush();
  c->stop();
  delete c;
  doFTPQuit(&cc);
  return true;
}

bool doFTPLS(ZSerial *serial, const char *hostIp, int port, const char *remotepath, const char *username, const char *pw, const bool doSSL)
{
  WiFiClient *cc = createWiFiClient(doSSL);
  if(!doFTPLogin(cc, hostIp, port, username, pw))
    return false;
  cc->printf("TYPE A\r\n");
  int respCode = getFTPResponseCode(cc, NULL);
  if(respCode < 0)
    return doFTPQuit(&cc);
  if((remotepath != NULL)&& (*remotepath != NULL))
  {
    cc->printf("CWD %s\r\n",remotepath);
    respCode = getFTPResponseCode(cc, NULL);
    if((respCode < 0)||(respCode > 400))
      return doFTPQuit(&cc);
    readBytesToSilence(cc);
  }
  WiFiClient *c = doFTPPassive(cc, doSSL);
  if(c == 0)
    return false;
  cc->printf("LIST\r\n",remotepath);
  respCode = getFTPResponseCode(cc, NULL);
  if((respCode < 0)||(respCode > 400))
  {
    c->stop();
    delete c;
    return doFTPQuit(&cc);
  }
  unsigned long now=millis();
  while((c->connected()||(c->available()>0)) 
  && ((millis()-now) < 30000)) // loop for data, with nice long timeout
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
