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

PhoneBookEntry::PhoneBookEntry(unsigned long phnum, const char *addr, const char *mod, const char *note)
{
  number=phnum;
  address = new char[strlen(addr)+1];
  strcpy((char *)address,addr);
  modifiers = new char[strlen(mod)+1];
  strcpy((char *)modifiers,mod);
  notes = new char[strlen(note)+1];
  strcpy((char *)notes,note);
  
  if(phonebook == null)
  {
    phonebook = this;
    return;
  }
  PhoneBookEntry *last = phonebook;
  if(last->number > number)
  {
    phonebook = this;
    next = last;
    return;
  }
  while(last->next != null)
  {
    if(last->next->number > number)
    {
      next = last->next;
      last->next = this;
      return;
    }
    last = last->next;
  }
  last->next = this;
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
  freeCharArray((char **)&notes);
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
      String configArguments[4];
      for(int i=0;i<4;i++)
        configArguments[i]="";
      for(int i=0;i<str.length();i++)
      {
        if((str[i]==',')&&(argn<=2))
          argn++;
        else
          configArguments[argn] += str[i];
      }
      PhoneBookEntry *phb = new PhoneBookEntry(atol(configArguments[0].c_str()),configArguments[1].c_str(),configArguments[2].c_str(),configArguments[3].c_str());
    }
    f.close();
  }
}

bool PhoneBookEntry::checkPhonebookEntry(String cmd)
{
    const char *vbuf=(char *)cmd.c_str();
    bool error = false;
    for(char *cptr=(char *)vbuf;*cptr!=0;cptr++)
    {
      if(strchr("0123456789",*cptr) == 0)
      {
        error = true;
        break;
      }
    }
    if(error || (strlen((char *)vbuf)>9))
      return false;
    return true;
}

PhoneBookEntry *PhoneBookEntry::findPhonebookEntry(long number)
{
  PhoneBookEntry *p = phonebook;
  while(p != null)
  {
    if(p->number == number)
      return p;
    p=p->next;
  }
  return null;
}

PhoneBookEntry *PhoneBookEntry::findPhonebookEntry(String number)
{
  if(!checkPhonebookEntry(number))
    return null;
  return findPhonebookEntry(atol(number.c_str()));
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
    f.printf("%ul,%s,%s,%s\n",phb->number,phb->address,phb->modifiers,phb->notes);
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
      if(argn!=3)
      {
        delay(100);
        f.close();
        savePhonebook();
        return;
      }
    }
    f.close();
  }
}
