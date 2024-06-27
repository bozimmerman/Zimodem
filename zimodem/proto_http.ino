/*
   Copyright 2016-2024 Bo Zimmerman

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
bool parseWebUrl(uint8_t *vbuf, char **hostIp, char **req, int *port, bool *doSSL)
{
  *doSSL = false;
  if(strstr((char *)vbuf,"http:")==(char *)vbuf)
    vbuf = vbuf + 5;
  else
  if(strstr((char *)vbuf,"gopher:")==(char *)vbuf)
    vbuf = vbuf + 7;
  else
  if(strstr((char *)vbuf,"gophers:")==(char *)vbuf)
  {
    vbuf = vbuf + 8;
    *doSSL = true;
  }
  else
  if(strstr((char *)vbuf,"https:")==(char *)vbuf)
  {
    vbuf = vbuf + 6;
    *doSSL = true;
  }
  while(*vbuf == '/')
    vbuf++;

  *port= (*doSSL) ? 443 : 80;
  *hostIp = (char *)vbuf;
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
     if(*port <= 0)
       return false;
  }
  return true;
}

# ifdef INCLUDE_SD_SHELL
class ChunkedStream : public WiFiClient
{
private:
  WiFiClient *wifi = null;
  int chunkCount = 0;
  int chunkSize = 0;
  uint8_t state = 0; //0
  bool eof = false;

public:

  ChunkedStream(WiFiClient *s)
  {
    wifi = s;
  }

  ~ChunkedStream()
  {
    if(wifi != null)
    {
      wifi->stop();
      delete wifi;
    }
  }

  virtual int read()
  {
    if(available()==0)
      return -1;
    int returnC = -1;
    char c=wifi->read();
    bool gotC = false;
    int errors = 0;
    while((!gotC) && (errors < 5000))
    {
      switch(state)
      {
        case 0:
          if(c=='0')
          {
            eof=true;
            return -1;
          }
          if((c>='0')&&(c<='9'))
          {
            chunkSize = (c - '0');
            state=1;
          }
          else
          if((c>='a')&&(c<='f'))
          {
            chunkSize = 10 + (c-'a');
            state=1;
          }
          break;
        case 1:
        {
          if((c>='0')&&(c<='9'))
            chunkSize = (chunkSize * 16) + (c - '0');
          else
          if((c>='a')&&(c<='f'))
            chunkSize = (chunkSize * 16) + (c-'a');
          else
          if(c=='\r')
            state=2;
          break;
        }
        case 2:
          if(c == '\n')
          {
            state = 3;
            errors = 0;
            chunkCount=0;
          }
          else
            state = 0;
          break;
        case 3:
          if(chunkCount < chunkSize)
          {
            gotC = true;
            chunkCount++;
            returnC = c;
          }
          else
          if(c == '\r')
            state = 4;
          else
            state = 0;
          break;
        case 4:
          if(c == '\n')
            state = 0;
          else
            state = 0; // what else is there to do?!
          break;
      }
      while((!gotC)
      && (errors < 5000))
      {
        if(available()>0)
        {
          c=wifi->read();
          break;
        }
        else
        if(++errors > 5000)
          break;
        else
          delay(1);
      }
    }
    return returnC;
  }
  virtual int peek()
  {
    return wifi->peek();
  }

  virtual int read(uint8_t *buf, size_t size)
  {
    if(size == 0)
      return 0;
    int num = available();
    if(num > size)
      num=size;
    for(int i=0;i<num;i++)
      buf[i]=read();
    return num;
  }

  bool getNoDelay()
  {
    return wifi->getNoDelay();
  }
  void setNoDelay(bool nodelay)
  {
    wifi->setNoDelay(nodelay);
  }

  virtual int available()
  {
    return wifi->available();
  }

  virtual void stop()
  {
    wifi->stop();
  }
  virtual uint8_t connected()
  {
    return wifi->connected() || (!eof);
  }
};

class FileWiFiStream : public WiFiClient
{
private:
  File f;
  uint8_t state = 0; //0

  void closeFile()
  {
    if(state == 0)
    {
      String name = "/";
      name += f.name();
      f.close();
      SD.remove(name.c_str());
      state=1;
    }
  }

public:

  FileWiFiStream(const char *file)
  {
    f = SD.open(file,FILE_READ);
  }

  ~FileWiFiStream()
  {
    closeFile();
  }

  virtual int read()
  {
    return f.read();
  }

  virtual int peek()
  {
    return f.peek();
  }

  virtual int read(uint8_t *buf, size_t size)
  {
    return f.read(buf,size);
  }

  bool getNoDelay()
  {
    return true;
  }
  void setNoDelay(bool nodelay)
  {
  }

  virtual int available()
  {
    return f.available();
  }

  virtual void stop()
  {
    closeFile();
  }
  virtual uint8_t connected()
  {
    return (state == 0);
  }
};
#endif

WiFiClient *doGopherGetStream(const char *hostIp, int port, const char *req, bool doSSL, uint32_t *responseSize)
{
  *responseSize = 0;
  if(WiFi.status() != WL_CONNECTED)
    return null;

  WiFiClient *c = createWiFiClient(doSSL);
  if(port == 0)
    port = 70;
  if(!c->connect(hostIp, port))
  {
    c->stop();
    delete c;
    return null;
  }
  c->setNoDelay(DEFAULT_NO_DELAY);
  const char *root = "";
  if(req == NULL)
    req=root;
  c->printf("%s\r\n",req);
  c->flush();
  *responseSize = 0;
# ifdef INCLUDE_SD_SHELL
  if(SD.cardType() != CARD_NONE)
  {
    char tempWebName[20];
    sprintf(tempWebName,"/.tmp_web_%u",random(9999));
    if(SD.exists(tempWebName))
      SD.remove(tempWebName);
    File tempF = SD.open(tempWebName,FILE_WRITE);
    if(!tempF)
    {
      *responseSize = 0;
      return c;
    }
    size_t written = 0;
    int b = c->read();
    int errors = 0;
    while((b < 0)&&(++errors<2000))
    {
      delay(1);
      b = c->read();
    }
    while((b >= 0)&&(c->connected()||(c->available()>0)))
    {
      written++;
      tempF.write(b);
      b = c->read();
      while((b < 0)&&(++errors<2000)&&(c->connected()||(c->available()>0)))
      {
        delay(1);
        b = c->read();
      }
    }
    delete c;
    tempF.close();
    *responseSize = written;
    return new FileWiFiStream(tempWebName);
  }
# endif
  return c;
}

WiFiClient *doWebGetStream(const char *hostIp, int port, const char *req, bool doSSL, uint32_t *responseSize)
{
  *responseSize = 0;
  if(WiFi.status() != WL_CONNECTED)
    return null;
  
  WiFiClient *c = createWiFiClient(doSSL);
  if(port == 0)
    port = 80;
  if(!c->connect(hostIp, port))
  {
    c->stop();
    delete c;
    return null;
  }
  c->setNoDelay(DEFAULT_NO_DELAY);

  const char *root = "";
  if(req == NULL)
    req=root;
  if(*req == '/')
    req++;
  
  c->printf("GET /%s HTTP/1.1\r\n",req);
  c->printf("User-Agent: Zimodem Firmware\r\n");
  c->printf("Host: %s\r\n",hostIp);
  c->printf("Connection: close\r\n\r\n");
  
  String ln = "";
  String reUrl = "";
  uint32_t respLength = 0;
  int respCode = -1;
  bool chunked = false;
  while(c->connected() || (c->available()>0))
  {
    yield();
    if(c->available()==0)
      continue;

    char ch = (char)c->read();
    logSocketIn(ch); // this is very much socket input!
    if(ch == '\r')
      continue;
    else
    if(ch == '\n')
    {
      if(ln.length()==0)
        break;
      if(respCode < 0)
      {
        int sp = ln.indexOf(' ');
        if(sp<=0)
          break;
        ln.remove(0,sp+1);
        sp = ln.indexOf(' ');
        if(sp<=0)
          break;
        ln.remove(sp);
        respCode = atoi(ln.c_str());
      }
      else
      {
        int x=ln.indexOf(':');
        if(x>0)
        {
          String header = ln.substring(0,x);
          header.toLowerCase();
          if(header == "content-length")
          {
            ln.remove(0,16);
            respLength = atoi(ln.c_str());
          }
          else
          if(header == "location")
          {
            reUrl = ln;
            reUrl.remove(0,10);
          }
          else
          if(header == "transfer-encoding")
          {
              ln.toLowerCase();
              chunked = ln.indexOf("chunked") > 0;  
          }
        }
      }
      ln = "";
    }
    else
      ln.concat(ch);
  }
  
  if((respCode >= 300) 
  && (respCode <= 399) 
  && (reUrl.length() > 0)
  && (reUrl.length() < 1024))
  {
    char newUrlBuf[reUrl.length()+1];
    strcpy(newUrlBuf,reUrl.c_str());
    char *hostIp2;
    char *req2;
    int port2;
    bool doSSL2;
    if(parseWebUrl((uint8_t *)newUrlBuf, &hostIp2,&req2,&port2,&doSSL2))
    {
        c->stop();
        delete c;
        return doWebGetStream(hostIp2,port2,req2,doSSL2,responseSize);
      
    }
  }
  
# ifdef INCLUDE_SD_SHELL
  if((chunked)
  &&(SD.cardType() != CARD_NONE))
    respLength = 1;
#endif
  *responseSize = respLength;
  if(((!c->connected())&&(c->available()==0))
  ||(respCode != 200)
  ||(respLength <= 0))
  {
    c->stop();
    delete c;
    return null;
  }
# ifdef INCLUDE_SD_SHELL
  if((chunked)
  &&(SD.cardType() != CARD_NONE))
  {
    ChunkedStream *ch = new ChunkedStream(c);
    char tempWebName[20];
    sprintf(tempWebName,"/.tmp_web_%u",random(9999));
    if(SD.exists(tempWebName))
      SD.remove(tempWebName);
    File tempF = SD.open(tempWebName,FILE_WRITE);
    if(!tempF)
    {
      delete ch;
      *responseSize = 0;
      return c;
    }
    size_t written = 0;
    int b = ch->read();
    int errors = 0;
    while((b < 0)&&(++errors<2000))
    {
      delay(1);
      b = ch->read();
    }
    while((b >= 0)&&(ch->connected()))
    {
      written++;
      tempF.write(b);
      b = ch->read();
      while((b < 0)&&(++errors<2000)&&(ch->connected()))
      {
        delay(1);
        b = ch->read();
      }
    }
    delete ch;
    tempF.close();
    *responseSize = written;
    return new FileWiFiStream(tempWebName);
  }
# endif
  //if(chunked) // technically, if a length was returned, chunked would be ok, but that's not in the cards.
  //  return new ChunkedStream(c);
  return c;
}

bool doStreamGet(WiFiClient *c, uint32_t respLength, FS *fs, const char *filename)
{
  uint32_t bytesRead = 0;
  File f = fs->open(filename, "w");
  unsigned long now = millis();
  while(((bytesRead < respLength) || (respLength==0))
  && (c->connected()||(c->available()>0)) 
  && ((millis()-now)<10000))
  {
    if(c->available()>0)
    {
      now=millis();
      uint8_t ch=c->read();
      logSocketIn(ch); // this is very much socket input!
      f.write(ch);
      bytesRead++;
    }
    else
      yield();
  }
  f.flush();
  f.close();
  c->stop();
  delete c;
  return (respLength == 0) || (bytesRead == respLength);
}

bool doWebGet(const char *hostIp, int port, FS *fs, const char *filename, const char *req, const bool doSSL)
{
  uint32_t respLength = 0;
  WiFiClient *c = doWebGetStream(hostIp, port, req, doSSL, &respLength);
  if(c == NULL)
    return false;
  return doStreamGet(c,respLength,fs,filename);
}

bool doGopherGet(const char *hostIp, int port, FS *fs, const char *filename, const char *req, const bool doSSL)
{
  uint32_t respLength = 0;
  WiFiClient *c = doGopherGetStream(hostIp, port, req, doSSL, &respLength);
  if(c == NULL)
    return false;
  return doStreamGet(c,respLength,fs,filename);
}

bool doStreamGetBytes(WiFiClient *c, uint32_t respLength, uint8_t *buf, int *bufSize)
{
  *bufSize = -1;
  if(((!c->connected())&&(c->available()==0))
  ||(respLength > *bufSize))
  {
    c->stop();
    delete c;
    return false;
  }
  *bufSize = (int)respLength;
  int index=0;
  unsigned long now = millis();
  while((index < respLength) // this can be removed for chunked encoding support
  &&(c->connected()||(c->available()>0)) 
  && ((millis()-now)<10000))
  {
    if(c->available()>0)
    {
      uint8_t ch=c->read();
      now = millis();
      logSocketIn(ch); // how is this not socket input -- it's coming from a WiFiClient -- that's THE SOCKET!
      buf[index++] = ch;
    }
    else
      yield();
  }
  *bufSize = index;
  c->stop();
  delete c;
  return (respLength == 0) || (index == respLength);
}

bool doWebGetBytes(const char *hostIp, int port, const char *req, const bool doSSL, uint8_t *buf, int *bufSize)
{
  *bufSize = -1;
  uint32_t respLength=0;
  WiFiClient *c = doWebGetStream(hostIp, port, req, doSSL, &respLength);
  if(c==null)
    return false;
  return doStreamGetBytes(c,respLength,buf,bufSize);
}

bool doGopherGetBytes(const char *hostIp, int port, const char *req, const bool doSSL, uint8_t *buf, int *bufSize)
{
  *bufSize = -1;
  uint32_t respLength=0;
  WiFiClient *c = doGopherGetStream(hostIp, port, req, doSSL, &respLength);
  if(c==null)
    return false;
  return doStreamGetBytes(c,respLength,buf,bufSize);
}
