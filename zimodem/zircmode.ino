#ifdef INCLUDE_IRCC
/*
   Copyright 2022-2024 Bo Zimmerman, Steve Gibson

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
//https://github.com/bl4de/irc-client/blob/master/irc_client.py
void ZIRCMode::switchBackToCommandMode()
{
  debugPrintf("\r\nMode:Command\r\n");
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
  debugPrintf("\r\nMode:IRC\r\n");
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
  debugRaw = false;
  joinReceived=false;
  if(nick.length()==0)
  {
    if(SPIFFS.exists(NICK_FILE))
    {
      File f = SPIFFS.open(NICK_FILE, "r");
      while(f.available()>0)
      {
        char c=f.read();
        if((c != '\n') && (c>0))
          nick += c;
      }
    }
    if(nick.length()==0)
    {
      randomSeed(millis());
      char tempNick[50];
      sprintf(tempNick,"ChangeMe#%ld",random(1,999));
      nick = tempNick;
    }
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
          int port=6667;
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
        bool fail = cmd.indexOf(',') >= 0;
        int colonDex=cmd.indexOf(':');
        fail = fail || (colonDex <= 0) || (colonDex == cmd.length()-1);
        fail = fail || (colonDex != cmd.lastIndexOf(':'));
        if(!fail)
        {
          for(int i=colonDex+1;i<cmd.length();i++)
          {
            if(strchr("0123456789",cmd[i]) == 0)
            {
              fail=true;
              break;
            }
          }
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
      {
        nick=cmd;
        File f = SPIFFS.open(NICK_FILE, "w");
        f.printf("%s",nick.c_str());
        f.close();
      }
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
        // index the first and second arguments, space/tab delimited
        int firstArgStart=0;
        while((cmd.length()>firstArgStart)&&(cmd[firstArgStart]!=' ')&&(cmd[firstArgStart]!=9))
          firstArgStart++;
        while((cmd.length()>firstArgStart)
        &&((cmd[firstArgStart]==' ')||(cmd[firstArgStart]==9)))
          firstArgStart++;
        int nextArgStart = firstArgStart;
        while((cmd.length()>nextArgStart)&&(cmd[nextArgStart]!=' ')&&(cmd[nextArgStart]!=9))
          nextArgStart++;
        int firstArgEnd=nextArgStart;
        while((cmd.length()>nextArgStart)
        &&((cmd[nextArgStart]==' ')||(cmd[nextArgStart]==9)))
          nextArgStart++;
        if(lccmd.startsWith("/join "))
        {
          if(firstArgStart < cmd.length())
          {
            channelName = cmd.substring(firstArgStart);
            if(current != null)
              current->print("JOIN :"+channelName+"\r\n");
            serial.println("* Now talking in: " + channelName);
          }
          else
            serial.println("* A channel name is required *");
        }
        else
        if(lccmd.startsWith("/list"))
        {
          if(firstArgStart < cmd.length())
          {
            listFilter = cmd.substring(firstArgStart);
            listFilter.toLowerCase();
            if(current != null)
              current->print("LIST\r\n");
          }
          else
            serial.println("* A channel name filter is required.  Trust me. *");
        }
        else
        if (lccmd.startsWith("/part "))
        {
          String reasonToLeave = "Leaving...";
          String channelToLeave = "";
          if(nextArgStart < cmd.length())
          {
            channelToLeave = cmd.substring(firstArgStart,firstArgEnd);
            reasonToLeave = cmd.substring(nextArgStart);
          }
          else
          if(firstArgStart < cmd.length())
            channelToLeave = cmd.substring(firstArgStart);
          if(firstArgStart < cmd.length())
          {
            if(current != null)
              current->print("PART " + channelToLeave + " :" + reasonToLeave + "\r\n");
            if(channelToLeave == channelName)
            {
                serial.println("* You will need to /switch or /join a new channel to talk *");
                channelName = "";
            }
          }
          else
            serial.println("* You must specify the channel to part/leave *");
        }
        else
        if (lccmd.startsWith("/whois ")) // WHOIS - get user info
        {
          if((current != null)
          &&(nextArgStart < cmd.length()))
          {
            current->print("WHOIS " + cmd.substring(firstArgStart) + "\r\n");
          }
          else
            serial.println("* You must specify who to whois *");
        }
        else
        if (lccmd.startsWith("/switch ")) // Switch to a new Channel to talk on
        {
          if(firstArgStart < cmd.length())
          {
            if(current != null)
              channelName = cmd.substring(firstArgStart);
            serial.println("* Now talking in: " + channelName);
          }
          else
            serial.println("* A channel name is required *");
        }
        else
        if (lccmd.startsWith("/debug")) // Toggle debug mode to receive all RAW messages
        {
          if (debugRaw)
          {
            debugRaw = false;
            if(current != null)
              serial.println("* Debug OFF - no longer receive RAW IRC messages");
          }
          else
          {
            debugRaw = true;
            if(current != null)
              serial.println("* Debug ON - receiving RAW IRC messages");
          }
        }
        else
        if(lccmd.startsWith("/msg ")) // Send "private message" to user
        {
          if((firstArgStart < cmd.length())
          &&(nextArgStart < cmd.length()))
          {
              String sendToNick = cmd.substring(firstArgStart,firstArgEnd);
              String msgToSend = cmd.substring(nextArgStart);
              if(current != null)
                current->print("PRIVMSG " + sendToNick + " :" + msgToSend + "\r\n");
                //current->printf("PRIVMSG %s :%s\r\n",sendToNick.c_str(),msgToSend.c_str());
          }
          else
            serial.println("* A nickname and message is required*");
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
      {
        if(channelName.length() == 0)
          serial.println("* You must /switch or /join a channel to talk. *");
        else
          current->printf("PRIVMSG %s :%s\r\n",channelName.c_str(),cmd.c_str());
      }
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
                      serial.prints("* Commands: /join #<channel>, /list <filter>, or /quit.\r\n");
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
                  // Print copy of raw IRC messages for debugging. Comment in/out as needed.
                  if (debugRaw)
                  {
                    serial.prints("RAW> " + cmd);
                    serial.prints(EOLNC);
                  }
                  int rawMsgStart = cmd.indexOf(":"); // raw IRC messages start with ':'
                  // the first space occurs after the user details (which sometimes contain extra ':' chars)
                  int firstSpace = cmd.indexOf(" ");
                  // this is the 2nd colon, after which is the message
                  int secondColon = (firstSpace >= 0) ? cmd.indexOf(":", firstSpace) : -1;
                  String theMessage = (secondColon >= 0) ? cmd.substring(secondColon + 1) : "";
                  theMessage.trim();
                  String msgMetaData = (secondColon >= 0) ? cmd.substring(rawMsgStart + 1, secondColon) : theMessage;
                  msgMetaData.trim();
                  int startMsgType = cmd.indexOf("PRIVMSG ");
                  if((startMsgType >= 0)
                  &&(secondColon > 0))
                  {
                    int endOfNick = msgMetaData.indexOf("!");
                    if (endOfNick >= 0)
                    {
                      String fromNick = msgMetaData.substring(0,endOfNick);
                      int endMsgType = cmd.indexOf(' ', startMsgType);
                      int startSentTo = endMsgType + 1;
                      int endSentTo = cmd.indexOf(' ', startSentTo);
                      String sentTo = cmd.substring(startSentTo, endSentTo);
                      //serial.println("* sentTo: " + sentTo);
                      if (sentTo.startsWith("#")) // contains channel name in msg, so it is a channel message
                      {
                        serial.print("[" + sentTo + "]: <" + fromNick+ "> " + theMessage);
                      }
                      else
                      {
                        serial.print("*PRIV* <" + fromNick+ "> " + theMessage);
                      }
                    }
                  }
                  else // QUIT - message when a client quits IRC
                  if (msgMetaData.indexOf(" QUIT") > 0)
                  {
                    int endOfNick = msgMetaData.indexOf("!");
                    String fromNick = msgMetaData.substring(0,endOfNick);
                    serial.print("[" + fromNick+ ": QUIT (" + theMessage + ")");
                  }
                  else // JOIN - message when a client joins a channel we are in
                  if (msgMetaData.indexOf(" JOIN") > 0)
                  {
                    int endOfNick = msgMetaData.indexOf("!");
                    int startOfChanName = msgMetaData.indexOf("#");
                    String chanName="";
                    if(startOfChanName >0)
                      chanName = msgMetaData.substring(startOfChanName, msgMetaData.length());
                    else
                    {
                      startOfChanName = theMessage.indexOf("#");
                      if(startOfChanName >= 0)
                        chanName = theMessage.substring(startOfChanName, theMessage.length());
                    }
                    if((chanName.length()>0)&&(endOfNick>0))
                    {
                      String fromNick = msgMetaData.substring(0,endOfNick);
                      serial.print("[" + fromNick+ " --> JOIN (" + chanName + ")");
                    }
                  }
                  else // PART - message when a client leaves a channel we are in
                  if (msgMetaData.indexOf(" PART ") > 0)
                  {
                    int endOfNick = msgMetaData.indexOf("!");
                    String chanName="";
                    int startOfChanName = msgMetaData.indexOf("#");
                    if(startOfChanName >0)
                      chanName = msgMetaData.substring(startOfChanName, msgMetaData.length());
                    else
                    {
                      startOfChanName = theMessage.indexOf("#");
                      if(startOfChanName >= 0)
                        chanName = theMessage.substring(startOfChanName, theMessage.length());
                    }
                    if((chanName.length()>0)&&(endOfNick>0))
                    {
                      String fromNick = msgMetaData.substring(0,endOfNick);
                      serial.print("[" + fromNick+ " <-- PART (" + chanName + ")");
                    }
                  }
                  else // 353 = Names (lists all nicknames on channel) -- ignore this to prevent nickname spam on channel join
                  if (msgMetaData.indexOf(" 353 ") >= 0)
                  {
                      break;
                  }
                  else // 332 = TOPIC
                  if (cmd.indexOf("332") >= 0)
                  {
                    serial.prints("TOPIC: " + theMessage);
                  }
                  else // 322 = LIST
                  if (cmd.indexOf("322") >= 0)
                  {
                    int channelStart = cmd.indexOf("#",cmd.indexOf("322"));
                    int channelEnd = channelStart>0?cmd.indexOf(" ",channelStart):-1;
                    if(channelEnd > 0)
                    {
                      String channelName = cmd.substring(channelStart,channelEnd);
                      channelName.toLowerCase();
                      if((listFilter.length()==0)||(channelName.indexOf(listFilter)>=0))
                        serial.prints(cmd.substring(channelStart));
                      else
                        break; // no EOLN PLZ
                    }
                    else
                      break; // no EOLN PLZ
                  }
                  else
                  if (cmd.indexOf("333") >= 0) // 333 = Who set TOPIC & Time set
                  {
                      break;
                  }
                  else // 366 = End of Names (channel joined)
                  if(msgMetaData.indexOf(" 366 ")>=0)
                  {
                    joinReceived=true;
                    serial.prints("Channel joined.  Enter a message to send to channel.");
                    serial.prints(EOLNC);
                    serial.prints("/msg <nick> <message>, /part #<channel>, /switch #<channel>, /list, or /quit.");
                    serial.prints(EOLNC);
                  }
                  else // Handle message types from WHOIS lookups 319 = WHOIS (Channel)
                  if (msgMetaData.indexOf(" 319 ") > 0)
                  {
                    serial.prints("W| " + cmd.substring(cmd.indexOf(" 319 ")));
                  }
                  else // 312 = WHOIS (server)
                  if (msgMetaData.indexOf(" 312 ") > 0)
                  {
                    serial.prints("W| " + cmd.substring(cmd.indexOf(" 312 ")));
                  }
                  else // 311 = WHOIS (hostmask)
                  if (msgMetaData.indexOf(" 311 ") > 0)
                  {
                    serial.prints("W| " + cmd.substring(cmd.indexOf(" 311 ")));
                  }
                  else // 330 = WHOIS (hostmask)
                  if (msgMetaData.indexOf(" 330 ") > 0)
                  {
                    serial.prints("W| " + cmd.substring(cmd.indexOf(" 330 ")));
                  }
                  else // 671 = WHOIS (hostmask)
                  if (msgMetaData.indexOf(" 671 ") > 0)
                  {
                    serial.prints("W| " + cmd.substring(cmd.indexOf(" 671 ")));
                  }
                  else
                    serial.prints(theMessage);
                  serial.prints(EOLNC);
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
  logFileLoop();
}

#endif /* INCLUDE_IRCC */
