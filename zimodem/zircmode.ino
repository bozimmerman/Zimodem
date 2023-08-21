/*
 * zircmode.ino
 *
 *  Created on: May 18, 2022
 *      Author: Bo Zimmerman
 */
#ifdef INCLUDE_IRCC
//https://github.com/bl4de/irc-client/blob/master/irc_client.py
void ZIRCMode::switchBackToCommandMode()
{
  serial.println("Back in command mode.");
  if(current != null)
  {
    delete current;
    current = null;
  }
  currMode = &commandMode;
}

void ZIRCMode::switchTo()
{
  currMode=&ircMode;
  savedEcho=commandMode.doEcho;
  commandMode.doEcho=true;
  serial.setFlowControlType(commandMode.serial.getFlowControlType());
  serial.setPetsciiMode(commandMode.serial.isPetsciiMode());
  showMenu=true;
  EOLN=commandMode.EOLN;
  EOLNC=EOLN.c_str();
  currState=ZIRCMENU_MAIN;
  lastNumber=0;
  lastAddress="";
  lastOptions="";
  channelName="";
  joinReceived=false;
  if(nick.length()==0)
  {
    randomSeed(millis());
    char tempNick[50];
    sprintf(tempNick,"ChangeMe#%ld",random(1,999));
    nick = tempNick;
  }
}

void ZIRCMode::doIRCCommand()
{
  String cmd = commandMode.getNextSerialCommand();
  char c='?';
  while((cmd.length()>0)
  &&(cmd[0]==32)||(cmd[0]==7))
    cmd = cmd.substring(1);
  if(cmd.length()>0)
    c=lc(cmd[0]);
  switch(currState)
  {
    case ZIRCMENU_MAIN:
    {
      if((c=='q')||(cmd.length()==0))
      {
        commandMode.showInitMessage();
        switchBackToCommandMode();
        return;
      }
      else
      if(c=='a') // add to phonebook
      {
        currState=ZIRCMENU_NUM;
        showMenu=true;
      }
      else
      if(c=='n') // change nick
      {
        currState=ZIRCMENU_NICK;
        showMenu=true;
      }
      else
      if(c>47 && c<58) // its a phonebook entry!
      {
        PhoneBookEntry *pb=PhoneBookEntry::findPhonebookEntry(cmd);
        if(pb == null)
        {
          serial.printf("%s%sPhone number not found: '%s'.%s%s",EOLNC,EOLNC,cmd.c_str(),EOLNC,EOLNC);
          currState=ZIRCMENU_MAIN;
          showMenu=true;
        }
        else
        {
          serial.printf("%s%sConnecting to %s...%s%s",EOLNC,EOLNC,pb->address,EOLNC,EOLNC);
          String address = pb->address;
          char vbuf[address.length()+1];
          strcpy(vbuf,pb->address);
          char *colon=strstr((char *)vbuf,":");
          int port=6666;
          if(colon != null)
          {
            (*colon)=0;
            port=atoi((char *)(++colon));
          }
          int flagsBitmap=0;
          {
            ConnSettings flags("");
            flagsBitmap = flags.getBitmap(serial.getFlowControlType());
          }
          logPrintfln("Connnecting: %s %d %d",(char *)vbuf,port,flagsBitmap);
          WiFiClientNode *c = new WiFiClientNode((char *)vbuf,port,flagsBitmap);
          if(!c->isConnected())
          {
            logPrintln("Connnect: FAIL");
            serial.prints("Connection failed.\n\r"); //TODO: maybe get rid of?
            currState=ZIRCMENU_MAIN;
            showMenu=true;
          }
          else
          {
            logPrintfln("Connnect: SUCCESS: %d",c->id);
            serial.prints("Connected.\n\r"); //TODO: maybe get rid of?
            current=c;
            buf = "";
            currState=ZIRCMENU_COMMAND;
            ircState=ZIRCSTATE_WAIT;
            timeout=millis()+6000;
            showMenu=true; // only wait to get it to act
          }
        }
      }
      else
      {
        showMenu=true; // re-show the menu
      }
      break;
    }
    case ZIRCMENU_NUM:
    {
      PhoneBookEntry *pb=PhoneBookEntry::findPhonebookEntry(cmd);
      if(pb != null)
      {
        serial.printf("%s%sNumber already exists '%s'.%s%s",EOLNC,EOLNC,cmd.c_str(),EOLNC,EOLNC);
        currState=ZIRCMENU_MAIN;
        showMenu=true;
      }
      else
      if(cmd.length()==0)
      {
        currState=ZIRCMENU_MAIN;
        showMenu=true;
      }
      else
      {
        lastNumber = atol((char *)cmd.c_str());
        lastAddress = "";
        ConnSettings flags(commandMode.getConfigFlagBitmap());
        lastOptions = flags.getFlagString();
        lastNotes = "";
        currState=ZIRCMENU_ADDRESS;
        showMenu=true;
      }
      break;
    }
    case ZIRCMENU_ADDRESS:
    {
      PhoneBookEntry *entry = PhoneBookEntry::findPhonebookEntry(lastNumber);
      if(cmd.equalsIgnoreCase("delete") && (entry != null))
      {
        delete entry;
        currState=ZIRCMENU_MAIN;
        serial.printf("%sPhonebook entry deleted.%s%s",EOLNC,EOLNC,EOLNC);
      }
      else
      if((cmd.length()==0) && (entry != null))
          currState=ZIRCMENU_NOTES; // just keep old values
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
          currState=ZIRCMENU_NOTES;
        }
      }
      showMenu=true; // re-show the menu
      break;
    }
    case ZIRCMENU_NOTES:
    {
      if(cmd.length()>0)
        lastNotes=cmd;
      currState=ZIRCMENU_OPTIONS;
      showMenu=true; // re-show the menu
      break;
    }
    case ZIRCMENU_NICK:
    {
      if(cmd.length()>0)
        nick=cmd;
      currState=ZIRCMENU_MAIN;
      showMenu=true; // re-show the menu
      break;
    }
    case ZIRCMENU_OPTIONS:
    {
      if(cmd.length()==0)
      {
        serial.printf("%sPhonebook entry added.%s%s",EOLNC,EOLNC,EOLNC);
        PhoneBookEntry *entry = new PhoneBookEntry(lastNumber,lastAddress.c_str(),lastOptions.c_str(),lastNotes.c_str());
        PhoneBookEntry::savePhonebook();
        currState=ZIRCMENU_MAIN;
      }
      else
      {
        ConnSettings flags(lastOptions.c_str());
        switch(c)
        {
          case 'p':
            flags.petscii=!flags.petscii;
            break;
          case 't':
            flags.telnet=!flags.telnet;
            break;
          case 'e':
            flags.echo=!flags.echo;
            break;
          case 'f':
            if(flags.xonxoff)
            {
              flags.xonxoff=false;
              flags.rtscts=true;
            }
            else
            if(flags.rtscts)
              flags.rtscts=false;
            else
              flags.xonxoff=true;
            break;
          case 's':
            flags.secure=!flags.secure;
            break;
          default:
            serial.printf("%sInvalid toggle option '%s'.%s%s",EOLNC,cmd.c_str(),EOLNC,EOLNC);
            break;
        }
        lastOptions = flags.getFlagString();
      }
      showMenu=true; // re-show the menu
      break;
    }
    case ZIRCMENU_COMMAND:
    {
      if(c=='/')
      {
        String lccmd = cmd;
        lccmd.toLowerCase();
        if(lccmd.startsWith("/join "))
        {
          int cs=5;
          while((cmd.length()>cs)
          &&((cmd[cs]==' ')||(cmd[cs]==7)))
            cs++;
          if(cs < cmd.length())
          {
            if(channelName.length()>0 && joinReceived)
            {
              serial.println("* Already in "+channelName+": Not Yet Implemented");
              // we are already joined somewhere
            }
            else
            {
              channelName = cmd.substring(cs);
              if(current != null)
                current->print("JOIN :"+channelName+"\r\n");
            }
          }
          else
            serial.println("* A channel name is required *");
        }
        else
        if(lccmd.startsWith("/quit"))
        {
          if(current != null)
          {
            current->print("QUIT Good bye!\r\n");
            current->flush();
            delay(1000);
            current->markForDisconnect();
            delete current;
            current = null;
          }
          switchBackToCommandMode();
        }
        else
        {
          serial.println("* Unknown command: "+lccmd);
          serial.println("* Try /?");
        }
      }
      else
      if((current != null)
      &&(joinReceived))
        current->printf("PRIVMSG %s :%s\r\n",channelName.c_str(),cmd.c_str());
      break;
    }
  }
}

void ZIRCMode::loopMenuMode()
{
  if(showMenu)
  {
    showMenu=false;
    switch(currState)
    {
      case ZIRCMENU_MAIN:
      {
        if(nick.length()==0)
          nick = hostname;
        serial.printf("%sInternet Relay Chat (IRC) Main Menu%s",EOLNC,EOLNC);
        serial.printf("[NICK] name: %s%s",nick.c_str(),EOLNC);
        serial.printf("[ADD] new phonebook entry%s",EOLNC);
        PhoneBookEntry *p = phonebook;
        if(p != null)
        {
          serial.printf("Phonebook entries:%s",EOLNC);
          while(p != null)
          {
            if(strlen(p->notes)>0)
              serial.printf("  [%lu] %s (%s)%s",p->number, p->address, p->notes, EOLNC);
            else
              serial.printf("  [%lu] %s%s",p->number, p->address, EOLNC);
            p=p->next;
          }
        }
        serial.printf("%sEnter command, number to connect to, or ENTER to exit: ",EOLNC,EOLNC);
        break;
      }
      case ZIRCMENU_NUM:
        serial.printf("%sEnter a new fake phone number (digits ONLY)%s: ",EOLNC,EOLNC);
        break;
      case ZIRCMENU_ADDRESS:
      {
        serial.printf("%sEnter a new hostname:port%s: ",EOLNC,EOLNC);
        break;
      }
      case ZIRCMENU_NICK:
      {
        serial.printf("%sEnter a new nick-name%s: ",EOLNC,EOLNC);
        break;
      }
      case ZIRCMENU_OPTIONS:
      {
        ConnSettings flags(lastOptions.c_str());
        serial.printf("%sConnection Options:%s",EOLNC,EOLNC);
        serial.printf("[PETSCII] Translation: %s%s",flags.petscii?"ON":"OFF",EOLNC);
        serial.printf("[TELNET] Translation: %s%s",flags.telnet?"ON":"OFF",EOLNC);
        serial.printf("[ECHO]: %s%s",flags.echo?"ON":"OFF",EOLNC);
        serial.printf("[FLOW] Control: %s%s",flags.xonxoff?"XON/XOFF":flags.rtscts?"RTS/CTS":"DISABLED",EOLNC);
        serial.printf("%sEnter option to toggle or ENTER to exit%s: ",EOLNC,EOLNC);
        break;
      }
      case ZIRCMENU_NOTES:
      {
        serial.printf("%sEnter some notes for this entry (%s)%s: ",EOLNC,lastNotes.c_str(),EOLNC);
        break;
      }
      case ZIRCMENU_COMMAND:
      {
        showMenu=true; // keep coming back here, over and over and over
        if((current==null)||(!current->isConnected()))
          switchBackToCommandMode();
        else
        {
            String cmd;
            while((current->available()>0) && (current->isConnected()))
            {
              uint8_t c = current->read();
              if((c == '\r')||(c == '\n')||(buf.length()>510))
              {
                  if((c=='\r')||(c=='\n'))
                  {
                    cmd=buf;
                    buf="";
                    break;
                  }
                  buf="";
              }
              else
                  buf += (char)c;
            }
            if(cmd.length()>0)
            {
              if(cmd.indexOf("PING :")==0)
              {
                  int x = cmd.indexOf(':');
                  if(x>0)
                    current->print("PONG "+cmd.substring(x+1)+"\r\n");
              }
              else
              switch(ircState)
              {
                case ZIRCSTATE_WAIT:
                {
                  if(cmd.indexOf("376")>=0)
                  {
                      ircState = ZIRCSTATE_COMMAND;
                      //TODO: say something?
                  }
                  else
                  if(cmd.indexOf("No Ident response")>=0)
                  {
                      current->print("NICK "+nick+"\r\n");
                      current->print("USER guest 0 * :"+nick+"\r\n");
                  }
                  else
                  if(cmd.indexOf("433")>=0)
                  {
                      if(nick.indexOf("_____")==0)
                      {
                          ircState = ZIRCSTATE_WAIT;
                          delete current;
                          current = null;
                          currState = ZIRCMENU_MAIN;
                      }
                      else
                      {
                        nick = "_" + nick;
                        current->print("NICK "+nick+"\r\n");
                        current->print("USER guest 0 * :"+nick+"\r\n");
                        // above was user nick * * : nick
                      }
                  }
                  else
                  if(cmd.indexOf(":")==0)
                  {
                    int x = cmd.indexOf(":",1);
                    if(x>1)
                    {
                      serial.prints(cmd.substring(x+1));
                      serial.prints(EOLNC);
                    }
                  }
                  break;
                }
                case ZIRCSTATE_COMMAND:
                {
                  if((!joinReceived) 
                  && (channelName.length()>0) 
                  && (cmd.indexOf("366")>=0))
                  {
                    joinReceived=true;
                    serial.prints("Channel joined.  Enter a message to send, or /quit.");
                    serial.prints(EOLNC);
                  }
                  int x0 = cmd.indexOf(":");
                  int x1 = (x0>=0)?cmd.indexOf(" PRIVMSG ", x0+1):-1;
                  x1 = (x1>0)?cmd.indexOf(":", x1+1):(x0>=0)?cmd.indexOf(":", x0+1):-1;
                  if(x1>0)
                  {
                    String msg2=cmd.substring(x1+1);
                    msg2.trim();
                    String msg1=cmd.substring(x0+1,x1);
                    msg1.trim();
                    int x2=msg1.indexOf("!");
                    if(x2>=0)
                      serial.print("< "+msg1.substring(0,x2)+"> "+msg2);
                    else
                      serial.prints(msg2);
                    serial.prints(EOLNC);
                  }
                  break;
                }
                default:
                  serial.prints("unknown state\n\r");
                  switchBackToCommandMode();
                  break;
              }
            }
            else
            if(ircState == ZIRCSTATE_WAIT)
            {
              if(millis()>timeout)
              {
                timeout = millis()+60000;
                current->print("NICK "+nick+"\r\n");
                current->print("USER guest 0 * :"+nick+"\r\n");
              }
            }
        }
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

void ZIRCMode::serialIncoming()
{
  bool crReceived=commandMode.readSerialStream();
  commandMode.clearPlusProgress(); // re-check the plus-escape mode
  if(crReceived)
  {
    doIRCCommand();
  }
}

void ZIRCMode::loop()
{
  loopMenuMode();
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

#endif /* INCLUDE_IRCC */
