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

void ZConfig::switchTo()
{
  currMode=&configMode;
  serial.setFlowControlType(commandMode.serial.getFlowControlType());
  serial.setPetsciiMode(commandMode.serial.isPetsciiMode());
  savedEcho=commandMode.doEcho;
  commandMode.doEcho=true;
  serial.setXON(true);
  showMenu=true;
  EOLN=commandMode.EOLN;
  EOLNC=EOLN.c_str();
  currState = ZCFGMENU_MAIN;
  lastNumber=0;
  lastAddress="";
  lastOptions="";
  settingsChanged=false;
  lastNumNetworks=0;
}

void ZConfig::serialIncoming()
{
  bool crReceived=commandMode.readSerialStream();
  commandMode.clearPlusProgress(); // re-check the plus-escape mode
  if(crReceived)
  {
    doModeCommand();
  }
}

void ZConfig::switchBackToCommandMode()
{
  commandMode.doEcho=savedEcho;
  currMode = &commandMode;
}

void ZConfig::doModeCommand()
{
  String cmd = commandMode.getNextSerialCommand();
  char c='?';
  for(int i=0;i<cmd.length();i++)
  {
    if(cmd[i]>32)
    {
      c=lc(cmd[i]);
      break;
    }
  }
  switch(currState)
  {
    case ZCFGMENU_MAIN:
    {
      if((c=='q')||(cmd.length()==0))
      {
        if(settingsChanged)
        {
          currState=ZCFGMENU_WICONFIRM;
          showMenu=true;
        }
        else
        {
          commandMode.showInitMessage();
          switchBackToCommandMode();
          return;
        }
      }
      else
      if(c=='a') // add to phonebook
      {
        currState=ZCFGMENU_NUM;
        showMenu=true;
      }
      else
      if(c=='w') // wifi
      {
        currState=ZCFGMENU_WIMENU;
        showMenu=true;
      }
      else
      if(c=='f') // flow control
      {
        currState=ZCFGMENU_FLOW;
        showMenu=true;
      }
      else
      if(c=='p') // petscii translation toggle
      {
        commandMode.serial.setPetsciiMode(!commandMode.serial.isPetsciiMode());
        serial.setPetsciiMode(commandMode.serial.isPetsciiMode());
        settingsChanged=true;
        showMenu=true;
      }
      else
      if(c=='e') // echo
      {
        savedEcho = !savedEcho;
        settingsChanged=true;
        showMenu=true;
      }
      else
      if(c>47 && c<58) // its a phonebook entry!
      {
        PhoneBookEntry *pb=PhoneBookEntry::findPhonebookEntry(cmd);
        if(pb == null)
        {
          serial.printf("%s%sPhone number not found: '%s'.%s%s",EOLNC,EOLNC,cmd.c_str(),EOLNC,EOLNC);
          currState=ZCFGMENU_MAIN;
          showMenu=true;
        }
        else
        {
          lastNumber = pb->number;
          lastAddress = pb->address;
          lastOptions = pb->modifiers;
          currState=ZCFGMENU_ADDRESS;
          showMenu=true;
        }
      }
      else
      {
        showMenu=true; // re-show the menu
      }
      break;
    }
    case ZCFGMENU_WICONFIRM:
    {
      if((cmd.length()==0)||(c=='n'))
      {
        commandMode.showInitMessage();
        switchBackToCommandMode();
        return;
      }
      else
      if(c=='y')
      {
        commandMode.reSaveConfig();
        serial.printf("%sSettings saved.%s",EOLNC,EOLNC);
        commandMode.showInitMessage();
        switchBackToCommandMode();
        return;
      }
      else
        showMenu=true;
    }
    case ZCFGMENU_NUM:
    {
      PhoneBookEntry *pb=PhoneBookEntry::findPhonebookEntry(cmd);
      if(pb != null)
      {
        serial.printf("%s%sNumber already exists '%s'.%s%s",EOLNC,EOLNC,cmd.c_str(),EOLNC,EOLNC);
        currState=ZCFGMENU_MAIN;
        showMenu=true;
      }
      else
      {
        lastNumber = atol((char *)cmd.c_str());
        lastAddress = "";
        lastOptions = "";
        currState=ZCFGMENU_ADDRESS;
        showMenu=true;
      }
      break;
    }
    case ZCFGMENU_ADDRESS:
    {
      PhoneBookEntry *entry = PhoneBookEntry::findPhonebookEntry(lastNumber);
      if(cmd.equalsIgnoreCase("delete") && (entry != null))
      {
        delete entry;
        currState=ZCFGMENU_MAIN;
        serial.printf("%sPhonebook entry deleted.%s%s",EOLNC,EOLNC,EOLNC);
      }
      else
      if((cmd.length()==0) && (entry != null))
          currState=ZCFGMENU_OPTIONS; // just keep old values
      else
      {
        boolean fail = cmd.indexOf(',') >= 0;
        int colonDex=cmd.indexOf(':');
        fail = fail || (colonDex <= 0) || (colonDex == cmd.length()-1);
        fail = fail || (colonDex != cmd.lastIndexOf(':'));
        if(!fail)
        {
          for(int i=colonDex+1;i<cmd.length();i++)
            if(strchr("0123456789",cmd[i])<0)
              fail=true;
        }
        if(fail)
        {
          serial.printf("%sInvalid address format (hostname:port) for '%s'.%s%s",EOLNC,cmd.c_str(),EOLNC,EOLNC);
        }
        else
        {
          lastAddress = cmd;
          currState=ZCFGMENU_OPTIONS;
        }
      }
      showMenu=true; // re-show the menu
      break;
    }
    case ZCFGMENU_OPTIONS:
    {
      if(cmd.length()==0)
      {
        PhoneBookEntry *entry = PhoneBookEntry::findPhonebookEntry(lastNumber);
        if(entry != null)
        {
          serial.printf("%sPhonebook entry updated.%s%s",EOLNC,EOLNC,EOLNC);
          delete entry;
        }
        else
          serial.printf("%sPhonebook entry added.%s%s",EOLNC,EOLNC,EOLNC);
        entry = new PhoneBookEntry(lastNumber,lastAddress.c_str(),lastOptions.c_str());
        PhoneBookEntry::savePhonebook();
        currState=ZCFGMENU_MAIN;
      }
      else
      {
        int flagBitmap = commandMode.makeStreamFlagsBitmap(lastOptions.c_str(), false);
        boolean petscii = (flagBitmap & FLAG_PETSCII) > 0;
        boolean telnet = (flagBitmap & FLAG_TELNET) > 0;
        boolean echo = (flagBitmap & FLAG_ECHO) > 0;
        boolean xonxoff = (flagBitmap & FLAG_XONXOFF) > 0;
        boolean rtscts = (flagBitmap & FLAG_RTSCTS) > 0;
        boolean secure = (flagBitmap & FLAG_SECURE) > 0;
        switch(c)
        {
          case 'p': 
            petscii=!petscii;
            break;
          case 't': 
            telnet=!telnet;
            break;
          case 'e': 
            echo=!echo;
            break;
          case 'f':
            if(xonxoff)
            {
              xonxoff=false;
              rtscts=true; 
            }
            else
            if(rtscts)
              rtscts=false;
            else
              xonxoff=true;
            break;
          case 's': 
            secure=!secure;
            break;
          default:
            serial.printf("%sInvalid toggle option '%s'.%s%s",EOLNC,cmd.c_str(),EOLNC,EOLNC);
            break;
        }
        lastOptions =(petscii?"p":"");
        lastOptions += (petscii?"p":"");
        lastOptions += (telnet?"t":"");
        lastOptions += (echo?"e":"");
        lastOptions += (xonxoff?"x":"");
        lastOptions += (rtscts?"r":"");
        lastOptions += (secure?"s":"");
      }
      showMenu=true; // re-show the menu
      break;
    }
    case ZCFGMENU_WIMENU:
    {
      if(cmd.length()==0)
      {
        currState=ZCFGMENU_MAIN;
        showMenu=true;
      }
      else
      {
        int num=atoi(cmd.c_str());
        if((num<=0)||(num>lastNumNetworks))
            serial.printf("%sInvalid number.  Try again.%s",EOLNC,EOLNC);
        else
        if(WiFi.encryptionType(num-1) == ENC_TYPE_NONE)
        {
          if(!connectWifi(WiFi.SSID(num-1).c_str(),""))
          {
            serial.printf("%sUnable to connect to %s. :(%s",EOLNC,WiFi.SSID(num-1).c_str(),EOLNC);
          }
          else
          {
            wifiSSI=WiFi.SSID(num-1);
            wifiPW="";
            settingsChanged=true;
            serial.printf("%sConnected!%s",EOLNC,EOLNC);
            currState=ZCFGMENU_MAIN;
          }
          showMenu=true;
        }
        else
        {
          lastNumber=num-1;
          currState=ZCFGMENU_WIFIPW;
          showMenu=true;
        }
      }
      break;
    }
    case ZCFGMENU_WIFIPW:
      if(cmd.length()==0)
      {
        currState=ZCFGMENU_WIMENU;
        showMenu=true;
      }
      else
      {
          if(!connectWifi(WiFi.SSID(lastNumber).c_str(),cmd.c_str()))
          {
            serial.printf("%sUnable to connect to %s. :(%s",EOLNC,WiFi.SSID(lastNumber).c_str(),EOLNC);
          }
          else
          {
            wifiSSI=WiFi.SSID(lastNumber);
            wifiPW=cmd;
            settingsChanged=true;
            serial.printf("%sConnected!%s",EOLNC,EOLNC);
            currState=ZCFGMENU_MAIN;
          }
          showMenu=true;
      }
      break;
    case ZCFGMENU_FLOW:
      if(cmd.length()==0)
      {
        currState=ZCFGMENU_WIMENU;
        showMenu=true;
      }
      else
      {
        currState=ZCFGMENU_MAIN;
        showMenu=true;
        if(c=='x')
          commandMode.serial.setFlowControlType(FCT_NORMAL);
        else
        if(c=='r')
          commandMode.serial.setFlowControlType(FCT_RTSCTS);
        else
        if(c=='d')
          commandMode.serial.setFlowControlType(FCT_DISABLED);
        else
        {
          serial.printf("%sUnknown flow control type '%s'.  Try again.%s",EOLNC,cmd.c_str(),EOLNC);
          currState=ZCFGMENU_FLOW;
        }
        settingsChanged = settingsChanged || (currState ==ZCFGMENU_MAIN);
        serial.setFlowControlType(commandMode.serial.getFlowControlType());
        serial.setXON(true);
      }
      break;
  }
}

void ZConfig::loop()
{
  if(showMenu)
  {
    showMenu=false;
    switch(currState)
    {
      case ZCFGMENU_MAIN:
      {
        serial.printf("%sMain Menu%s",EOLNC,EOLNC);
        serial.printf("[WIFI] connection: %s%s",(WiFi.status() == WL_CONNECTED)?wifiSSI.c_str():"Not connected",EOLNC);
        String flowName;
        switch(commandMode.serial.getFlowControlType())
        {
          case FCT_NORMAL:
            flowName = "XON/XOFF";
            break;
          case FCT_RTSCTS:
            flowName = "RTS/CTS";
            break;
          case FCT_DISABLED:
            flowName = "DISABLED";
            break;
          default:
            flowName = "OTHER";
            break;
        }
        serial.printf("[FLOW] control: %s%s",flowName.c_str(),EOLNC);
        serial.printf("[ECHO] keystrokes: %s%s",savedEcho?"ON":"OFF",EOLNC);
        serial.printf("[PETSCII] translation: %s%s",commandMode.serial.isPetsciiMode()?"ON":"OFF",EOLNC);
        serial.printf("[ADD] new phonebook entry%s",EOLNC);
        PhoneBookEntry *p = phonebook;
        if(p != null)
        {
          serial.printf("Phonebook entries:%s",EOLNC);
          while(p != null)
          {
            serial.printf("  [%lu] %s%s",p->number, p->address, EOLNC);
            p=p->next;
          }
        }
        serial.printf("%sEnter command or entry or ENTER to exit: ",EOLNC,EOLNC);
        break;
      }
      case ZCFGMENU_NUM:
        serial.printf("%sEnter a new fake phone number (digits ONLY)%s: ",EOLNC,EOLNC);
        break;
      case ZCFGMENU_ADDRESS:
      {
        PhoneBookEntry *lastEntry = PhoneBookEntry::findPhonebookEntry(lastNumber);
        if(lastEntry == null)
          serial.printf("%sEnter a new hostname:port%s: ",EOLNC,EOLNC);
        else
          serial.printf("%sModify hostname:port, or enter DELETE (%s)%s: ",EOLNC,lastAddress.c_str(),EOLNC);
        break;
      }
      case ZCFGMENU_OPTIONS:
      {
        int flagBitmap = commandMode.makeStreamFlagsBitmap(lastOptions.c_str(), false);
        boolean petscii = (flagBitmap & FLAG_PETSCII) > 0;
        boolean telnet = (flagBitmap & FLAG_TELNET) > 0;
        boolean echo = (flagBitmap & FLAG_ECHO) > 0;
        boolean xonxoff = (flagBitmap & FLAG_XONXOFF) > 0;
        boolean rtscts = (flagBitmap & FLAG_RTSCTS) > 0;
        boolean secure = (flagBitmap & FLAG_SECURE) > 0;
        serial.printf("%sConnection Options:%s",EOLNC,EOLNC);
        serial.printf("[PETSCII] Translation: %s%s",petscii?"ON":"OFF",EOLNC);
        serial.printf("[TELNET] Translation: %s%s",telnet?"ON":"OFF",EOLNC);
        serial.printf("[ECHO]: %s%s",echo?"ON":"OFF",EOLNC);
        serial.printf("[FLOW] Control: %s%s",xonxoff?"XON/XOFF":rtscts?"RTS/CTS":"DISABLED",EOLNC);
        serial.printf("%sEnter option to toggle or ENTER to exit%s: ",EOLNC,EOLNC);
        break;
      }
      case ZCFGMENU_WIMENU:
      {
        int n = WiFi.scanNetworks();
        if(n>20)
          n=20;
        serial.printf("%sWiFi Networks:%s",EOLNC,EOLNC);
        lastNumNetworks=n;
        for (int i = 0; i < n; ++i)
        {
          serial.printf("[%d] ",(i+1));
          serial.prints(WiFi.SSID(i).c_str());
          serial.prints(" (");
          serial.printi(WiFi.RSSI(i));
          serial.prints(")");
          serial.prints((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
          serial.prints(EOLN.c_str());
          serial.flush();
          delay(10);
        }
        serial.printf("%sEnter number to connect, or ENTER:%s",EOLNC,EOLNC);
        break;
      }
      case ZCFGMENU_WIFIPW:
      {
        serial.printf("%sEnter your WiFi Password: ",EOLNC);
        break;
      }
      case ZCFGMENU_FLOW:
      {
        serial.printf("%sEnter RTS/CTS, XON/XOFF, or DISABLE flow control%s: ",EOLNC,EOLNC);
        break;
      }
      case ZCFGMENU_WICONFIRM:
      {
        serial.printf("%sYour setting changed.  Save them (y/N)?",EOLNC);
        break;
      }
    }
  }
  if(commandMode.checkPlusEscape())
  {
    switchBackToCommandMode();
  }
  else
  if(serial.isSerialOut())
  {
    serialOutDeque();
  }
}

