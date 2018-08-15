/*
   Copyright 2016-2017 Bo Zimmerman

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
     if(port <= 0)
       return false;
  }
  return true;
}

bool doWebGetStream(const char *hostIp, int port, const char *req, WiFiClient *c, uint32_t *responseSize)
{
  *responseSize = 0;
  if(WiFi.status() != WL_CONNECTED)
    return false;
  if(port == 0)
    port = 80;
  c->setNoDelay(DEFAULT_NO_DELAY);
  if(!c->connect(hostIp, port))
  {
    c->stop();
    return false;
  }
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
  uint32_t respLength = 0;
  int respCode = -1;
  while(c->connected())
  {
    yield();
    if(c->available()<=0)
      continue;
      
    char ch = (char)c->read();
    //logSocketIn(ch); // this is NOT socket input!!
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
      if(ln.startsWith("Content-length: ")
      ||ln.startsWith("Content-Length: "))
      {
        ln.remove(0,16);
        respLength = atoi(ln.c_str());
      }
      ln = "";
    }
    else
      ln.concat(ch);
  }
  *responseSize = respLength;
  if((!c->connected())
  ||(respCode != 200)
  ||(respLength <= 0))
  {
    c->stop();
    return false;
  }
  return true;
}

bool doWebGet(const char *hostIp, int port, FS *fs, const char *filename, const char *req, const bool doSSL)
{
  uint32_t respLength=0;
  WiFiClient *c = createWiFiClient(doSSL);
  if((!doWebGetStream(hostIp, port, req, c, &respLength))
  ||(respLength == 0))
  {
    c->stop();
    delete c;
    return false;
  }
  
  File f = fs->open(filename, "w");
  long now = millis();
  while((respLength>0) && (c->connected()) && ((millis()-now)<10000))
  {
    if(c->available()>=0)
    {
      now=millis();
      uint8_t ch=c->read();
      //logSocketIn(ch); // this is ALSO not socket input!
      f.write(ch);
      respLength--;
    }
    else
      yield();
  }
  f.flush();
  f.close();
  c->stop();
  delete c;
  return (respLength == 0);
}

bool doWebGetBytes(const char *hostIp, int port, const char *req, const bool doSSL, uint8_t *buf, int *bufSize)
{
  WiFiClient *c = createWiFiClient(doSSL);
  uint32_t respLength=0;
  if(!doWebGetStream(hostIp, port, req, c, &respLength))
  {
    c->stop();
    delete c;
    return false;
  }
  if((!c->connected())
  ||(respLength > *bufSize))
  {
    c->stop();
    delete c;
    return false;
  }
  *bufSize = (int)respLength;
  int index=0;
  while((respLength>0) && (c->connected()))
  {
    if(c->available()>=0)
    {
      uint8_t ch=c->read();
      //logSocketIn(ch); // again, NOT SOCKET INPUT!
      buf[index++] = ch;
      respLength--;
    }
    else
      yield();
  }
  c->stop();
  delete c;
  return (respLength == 0);
}
