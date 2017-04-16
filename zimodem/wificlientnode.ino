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

void WiFiClientNode::finishConnectionLink()
{
  wasConnected=true;
  if(conns == null)
    conns = this;
  else
  {
    WiFiClientNode *last = conns;
    while(last->next != null)
      last = last->next;
    last->next = this;
  }
  checkOpenConnections();
}

WiFiClientNode::WiFiClientNode(char *hostIp, int newport, int flagsBitmap)
{
  port=newport;
  host=new char[strlen(hostIp)+1];
  strcpy(host,hostIp);
  id=++WiFiNextClientId;
  this->flagsBitmap = flagsBitmap;
  clientPtr = new WiFiClient();
  client = *clientPtr;
  client.setNoDelay(true);
  setCharArray(&delimiters,"");
  setCharArray(&maskOuts,"");
  if(!client.connect(hostIp, port))
  {
    // deleted when it returns and is deleted
  }
  else
  {
    finishConnectionLink();
  }
}
    
WiFiClientNode::WiFiClientNode(WiFiClient newClient, int flagsBitmap)
{
  this->flagsBitmap = flagsBitmap;
  clientPtr=null;
  newClient.setNoDelay(true);
  port=newClient.localPort();
  setCharArray(&delimiters,"");
  setCharArray(&maskOuts,"");
  String remoteIPStr = newClient.remoteIP().toString();
  const char *remoteIP=remoteIPStr.c_str();
  host=new char[remoteIPStr.length()+1];
  strcpy(host,remoteIP);
  id=++WiFiNextClientId;
  client = newClient;
  finishConnectionLink();
  serverClient=true;
}
    
WiFiClientNode::~WiFiClientNode()
{
  if(host!=null)
  {
    client.stop();
    delete host;
    host=null;
  }
  if(clientPtr != null)
  {
    delete clientPtr;
    clientPtr = null;
  }
  if(conns == this)
    conns = next;
  else
  {
    WiFiClientNode *last = conns;
    while((last != null) && (last->next != this)) // don't change this!
      last = last->next;
    if(last != null)
      last->next = next;
  }
  if(commandMode.current == this)
    commandMode.current = conns;
  if(commandMode.nextConn == this)
    commandMode.nextConn = conns;
  freeCharArray(&delimiters);
  freeCharArray(&maskOuts);
  next=null;
  checkOpenConnections();
}

bool WiFiClientNode::isConnected()
{
  return (host != null) && client.connected();
}

bool WiFiClientNode::isPETSCII()
{
  return (flagsBitmap & FLAG_PETSCII) == FLAG_PETSCII;
}

bool WiFiClientNode::isEcho()
{
  return (flagsBitmap & FLAG_ECHO) == FLAG_ECHO;
}

bool WiFiClientNode::isXonXoff()
{
  return (flagsBitmap & FLAG_XONXOFF) == FLAG_XONXOFF;
}

bool WiFiClientNode::isTelnet()
{
  return (flagsBitmap & FLAG_TELNET) == FLAG_TELNET;
}

bool WiFiClientNode::isDisconnectedOnStreamExit()
{
  return (flagsBitmap & FLAG_DISCONNECT_ON_EXIT) == FLAG_DISCONNECT_ON_EXIT;
}

size_t WiFiClientNode::write(uint8_t c)
{
  if(host == null)
    return 0;
  return client.write(c);
}

int WiFiClientNode::read()
{
  if(host == null)
    return 0;
  return client.read();
}

int WiFiClientNode::peek()
{
  if(host == null)
    return 0;
  return client.peek();
}

void WiFiClientNode::flush()
{
  if((host != null)&&(client.available()==0))
    client.flush();
}

int WiFiClientNode::available()
{
  if(host == null)
    return 0;
  return client.available();
}

int WiFiClientNode::read(uint8_t *buf, size_t size)
{
  if(host == null)
    return 0;
  return client.read(buf,size);
}

size_t WiFiClientNode::write(const uint8_t *buf, size_t size)
{
  if(host == null)
    return 0;
  return client.write(buf,size);
}

PhoneBookEntry::PhoneBookEntry(unsigned long phnum, const char *addr, const char *mod)
{
  number=phnum;
  address = new char[strlen(addr)+1];
  strcpy((char *)address,addr);
  modifiers = new char[strlen(mod)+1];
  strcpy((char *)modifiers,mod);
  
  if(phonebook == null)
    phonebook = this;
  else
  {
    PhoneBookEntry *last = phonebook;
    while(last->next != null)
      last = last->next;
    last->next = this;
  }
}

PhoneBookEntry::~PhoneBookEntry()
{
  if(phonebook == this)
    phonebook = next;
  else
  {
    PhoneBookEntry *last = phonebook;
    while((last != null) && (last->next != this)) // don't change this!
      last = last->next;
    if(last != null)
      last->next = next;
  }
  freeCharArray((char **)&address);
  freeCharArray((char **)&modifiers);
  next=null;
}

void PhoneBookEntry::loadPhonebook()
{
  clearPhonebook();
  if(SPIFFS.exists("/zphonebook.txt"))
  {
    File f = SPIFFS.open("/zphonebook.txt", "r");
    while(f.available()>0)
    {
      String str="";
      char c=f.read();
      while((c != '\n') && (f.available()>0))
      {
        str += c;
        c=f.read();
      }
      int argn=0;
      String configArguments[3];
      for(int i=0;i<3;i++)
        configArguments[i]="";
      for(int i=0;i<str.length();i++)
      {
        if((str[i]==',')&&(argn<=2))
          argn++;
        else
          configArguments[argn] += str[i];
      }
      PhoneBookEntry *phb = new PhoneBookEntry(atol(configArguments[0].c_str()),configArguments[1].c_str(),configArguments[2].c_str());
    }
    f.close();
  }
}

void PhoneBookEntry::clearPhonebook()
{
  PhoneBookEntry *phb = phonebook;
  while(phonebook != null)
    delete phonebook;
}

void PhoneBookEntry::savePhonebook()
{
  SPIFFS.remove("/zphonebook.txt");
  delay(500);
  File f = SPIFFS.open("/zphonebook.txt", "w");
  int ct=0;
  PhoneBookEntry *phb=phonebook;
  while(phb != null)
  {
    f.printf("%ul,%s,%s\n",phb->number,phb->address,phb->modifiers);
    phb = phb->next;
    ct++;
  }
  f.close();
  delay(500);
  if(SPIFFS.exists("/zphonebook.txt"))
  {
    File f = SPIFFS.open("/zphonebook.txt", "r");
    while(f.available()>0)
    {
      String str="";
      char c=f.read();
      while((c != '\n') && (f.available()>0))
      {
        str += c;
        c=f.read();
      }
      int argn=0;
      if(str.length()>0)
      {
        for(int i=0;i<str.length();i++)
        {
          if((str[i]==',')&&(argn<=2))
            argn++;
        }
      }
      if(argn!=2)
      {
        delay(100);
        savePhonebook();
      }
    }
    f.close();
  }
}


