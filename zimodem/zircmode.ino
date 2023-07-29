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
  if(irc_conn != null)
  {
    delete irc_conn;
    irc_conn = null;
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
  currentChannel="";
  joinReceived=false;
  debugRaw = false;

  if(nick.length() == 0)
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

  while((cmd.length()>0) && (cmd[0]==32) || (cmd[0]==7))
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
            irc_conn=c;
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
      if (c == '/') // Entering an IRC command
      {
        String lccmd = cmd;
        lccmd.toLowerCase();

        // JOIN Channel
        if (lccmd.startsWith("/join "))
        {
          int startOfChannel = cmd.indexOf(' ') + 1;
          int endOfChannel = cmd.indexOf(' ', startOfChannel);
          String chanToJoin = cmd.substring(startOfChannel, endOfChannel);
          
          if (chanToJoin.length()>0)
          {
            if (irc_conn != null)
            {
              irc_conn->print("JOIN :"+chanToJoin+"\r\n");
              currentChannel = chanToJoin;
              serial.println("* Now talking in: " + currentChannel);
            }
          }
          
          else
            serial.println("* A channel name is required *");
        }

        // PART (leave) Channel
        else
        if (lccmd.startsWith("/part "))
        {
          int startOfChannel = cmd.indexOf(' ') + 1;
          int endOfChannel = cmd.indexOf(' ', startOfChannel);
          String chanToPart = cmd.substring(startOfChannel, endOfChannel);
          String partReason = cmd.substring(endOfChannel + 1, cmd.length());
          
          if (chanToPart.length() > 0)
          {
            if (partReason.length() == 0)
              partReason = "Leaving...";
            if(irc_conn != null)
              irc_conn->print("PART " + chanToPart + " :" + partReason + "\r\n");
              
            serial.println("* You will need to /switch or /join a new channel to talk *");
          }
          else
            serial.println("* You must specify the channel to part/leave *");
        }

        // WHOIS - get user info
        else
        if (lccmd.startsWith("/whois "))
        {
          int startOfParameter = cmd.indexOf(' ') + 1;
          int endOfParameter = cmd.indexOf(' ', startOfParameter);
          String whoisUser = cmd.substring(startOfParameter, endOfParameter);
          
          if(irc_conn != null)
          {
            irc_conn->print("WHOIS " + whoisUser + "\r\n");
          }
        }

        // Switch to a new Channel to talk on
        else
        if (lccmd.startsWith("/switch "))
        {
          int startOfChannel = cmd.indexOf(' ') + 1;
          int endOfChannel = cmd.indexOf(' ', startOfChannel);
          String chanToSwitch = cmd.substring(startOfChannel, endOfChannel);
          
          if (chanToSwitch.length() > 0)
          {
            if(irc_conn != null)
              currentChannel = chanToSwitch;
              serial.println("* Now talking in: " + currentChannel);
          }
        }

        // Toggle debug mode to receive all RAW messages
        else
        if (lccmd.startsWith("/debug"))
        {
          if (debugRaw)
          {
            debugRaw = false;
            if(irc_conn != null)
              serial.println("* Debug OFF - no longer receive RAW IRC messages");
          }
          else
          {
            debugRaw = true;
            if(irc_conn != null)
              serial.println("* Debug ON - receiving RAW IRC messages");
          }
        }

        // Send "private message" to user
        else
        if(lccmd.startsWith("/msg "))
        {
          int cs=4;
          int startNick = cmd.indexOf(' ') + 1;
          int endNick = cmd.indexOf(' ', startNick);
          String sendToNick = cmd.substring(startNick, endNick);
          String msgToSend = cmd.substring(endNick + 1, cmd.length());
          //serial.println("* sendToNick: " + sendToNick);
          //serial.println("* msgToSend: " + msgToSend);

          if(cs < cmd.length())
          {
              if(irc_conn != null)
                irc_conn->print("PRIVMSG " + sendToNick + " :" + msgToSend + "\r\n");
                //irc_conn->printf("PRIVMSG %s :%s\r\n",sendToNick.c_str(),msgToSend.c_str());
          }
          else
            serial.println("* A nickname and message is required*");
        }
        else
        if(lccmd.startsWith("/quit"))
        {
          if(irc_conn != null)
          {
            irc_conn->print("QUIT Good bye!\r\n");
            irc_conn->flush();
            delay(1000);
            irc_conn->markForDisconnect();
            delete irc_conn;
            irc_conn = null;
          }
          switchBackToCommandMode();
        }
        else
        {
          serial.println("* Unknown command: "+lccmd);
          serial.println("* Try /?");
        }
      }
      // Otherwise send typed message to the active channel
      else
      if((irc_conn != null) && (joinReceived))
        irc_conn->printf("PRIVMSG %s :%s\r\n", currentChannel.c_str(), cmd.c_str());
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
        if ((irc_conn == null) || (!irc_conn->isConnected()))
          switchBackToCommandMode();
        else
        {
            String cmd;
            while ((irc_conn->available() > 0) && (irc_conn->isConnected()))
            {
              uint8_t c = irc_conn->read();
              if ((c == '\r') || (c == '\n') || (buf.length()>510))
              {
                  if ((c == '\r') || (c == '\n'))
                  {
                    cmd = buf;
                    buf = "";
                    break;
                  }
                  buf = "";
              }
              else
                  buf += (char)c;
            }

            if (cmd.length() > 0)
            {
              if (cmd.indexOf("PING :") == 0)
              {
                  int x = cmd.indexOf(':');
                  if (x > 0)
                    irc_conn->print("PONG " + cmd.substring(x + 1) + "\r\n");
              }
              else
              switch(ircState)
              {
                case ZIRCSTATE_WAIT:
                {
                  // 376 == End of MOTD
                  if (cmd.indexOf("376") >= 0) 
                  {
                      ircState = ZIRCSTATE_COMMAND;
                      //TODO: say something?
                  }

                  else
                  if(cmd.indexOf("No Ident response") >= 0)
                  {
                      irc_conn->print("NICK " + nick + "\r\n");
                      irc_conn->print("USER rewt 0 * :" + nick + "\r\n");
                  }

                  // 433 == Error, nickname in use
                  else
                  if(cmd.indexOf("433") >= 0) 
                  {
                      if(nick.indexOf("_____") == 0)
                      {
                          ircState = ZIRCSTATE_WAIT;
                          delete irc_conn;
                          irc_conn = null;
                          currState = ZIRCMENU_MAIN;
                      }
                      else
                      {
                        nick = "_" + nick;
                        irc_conn->print("NICK " + nick + "\r\n");
                        irc_conn->print("USER rewt 0 * :" + nick + "\r\n");
                        // above was user nick * * : nick
                      }
                  }

                  else
                  if(cmd.indexOf(":") == 0)
                  {
                    int x = cmd.indexOf(":",1);
                    if(x > 1)
                    {
                      serial.prints(cmd.substring(x + 1));
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

                  // raw IRC messages start with ':'
                  int rawMsgStart = cmd.indexOf(":"); 
                  // the first space occurs after the user details (which sometimes contain extra ':' chars)
                  int firstSpace = cmd.indexOf(" ");

                  // this is the 2nd colon, which divides the message meta data from the message contents
                  int msgDelimiter = cmd.indexOf(":", firstSpace);

                  String theMessage = cmd.substring(msgDelimiter + 1);
                  theMessage.trim();
                  String msgMetaData = cmd.substring(rawMsgStart + 1, msgDelimiter);
                  msgMetaData.trim();

                  // on JOIN & PART messages, there is no second colon, so the entire message is meta data
                  if (msgDelimiter <= 0)
                  {
                    msgMetaData = theMessage;
                  }

                  // PRIVMSG - either messages to channel we are in or private messages to us
                  // nickname before the '!' ex: :Wulfgar!~wulf459@user/wulf459 PRIVMSG Velma :that worked!
                  if (cmd.indexOf(" PRIVMSG ") > 0)
                  {                  
                    int endOfNick = msgMetaData.indexOf("!"); 
                    int startMsgType = cmd.indexOf("PRIVMSG");
                    int endMsgType = cmd.indexOf(' ', startMsgType);
                    int startSentTo = endMsgType + 1;
                    int endSentTo = cmd.indexOf(' ', startSentTo);
                    String sentTo = cmd.substring(startSentTo, endSentTo);
                    String fromNick = msgMetaData.substring(0,endOfNick);
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

                  // QUIT - message when a client quits IRC
                  else if (msgMetaData.indexOf(" QUIT ") > 0) 
                  {                  
                    int endOfNick = msgMetaData.indexOf("!"); 
                    String fromNick = msgMetaData.substring(0,endOfNick);
                    serial.print("[" + fromNick+ ": QUIT (" + theMessage + ")");
                  }

                  // JOIN - message when a client joins a channel we are in
                  else if (msgMetaData.indexOf(" JOIN ") > 0) 
                  {                  
                    int endOfNick = msgMetaData.indexOf("!");
                    int startOfChanName = msgMetaData.indexOf("#");
                    String fromNick = msgMetaData.substring(1,endOfNick);
                    String chanName = msgMetaData.substring(startOfChanName, msgMetaData.length());
                    serial.print("[" + fromNick+ " --> JOIN (" + chanName + ")");

                  }

                  // PART - message when a client leaves a channel we are in
                  else if (msgMetaData.indexOf(" PART ") > 0) 
                  {                  
                    int endOfNick = msgMetaData.indexOf("!"); 
                    int startOfChanName = msgMetaData.indexOf("#");
                    String fromNick = msgMetaData.substring(1,endOfNick);
                    String chanName = msgMetaData.substring(startOfChanName, msgMetaData.length());
                    serial.print("[" + fromNick+ " <-- PART (" + chanName + ")");

                  }

                  // 353 = Names (lists all nicknames on channel) -- ignore this to prevent nickname spam on channel join
                  else if (msgMetaData.indexOf(" 353 ") > 0) 
                  {
                    break;
                  }

                  // 332 = TOPIC
                  else if (msgMetaData.indexOf(" 332 ") > 0)
                  {
                    serial.prints("TOPIC: " + theMessage);
                  }

                  // 333 = Who set TOPIC & Time set
                  else if (msgMetaData.indexOf(" 333 ") > 0)
                  {
                    break;
                  }

                  // 366 = End of Names (channel joined)
                  else if(msgMetaData.indexOf(" 366 ") > 0)
                  {
                    joinReceived=true;
                    serial.println("Channel joined.  Enter a message to send to channel, /msg <nick> <message>, /part #channel, /switch #channel, or /quit.");
                    
                  }

                  // Handle message types from WHOIS lookups
                  // 319 = WHOIS (Channel)
                  else if (msgMetaData.indexOf(" 319 ") > 0)
                  {
                    serial.prints("W| " + cmd.substring(cmd.indexOf(" 319 ")));
                  }
                  // 312 = WHOIS (server)
                  else if (msgMetaData.indexOf(" 312 ") > 0)
                  {
                    serial.prints("W| " + cmd.substring(cmd.indexOf(" 312 ")));
                  }
                  // 311 = WHOIS (hostmask)
                  else if (msgMetaData.indexOf(" 311 ") > 0)
                  {
                    serial.prints("W| " + cmd.substring(cmd.indexOf(" 311 ")));
                  }
                  // 330 = WHOIS (hostmask)
                  else if (msgMetaData.indexOf(" 330 ") > 0)
                  {
                    serial.prints("W| " + cmd.substring(cmd.indexOf(" 330 ")));
                  }
                  // 671 = WHOIS (hostmask)
                  else if (msgMetaData.indexOf(" 671 ") > 0)
                  {
                    serial.prints("W| " + cmd.substring(cmd.indexOf(" 671 ")));
                  }

                  else
                    serial.prints(theMessage);
                    // Print copy of raw IRC messages for debugging. Comment in/out as needed.
                    //serial.prints("RAW> " + cmd);
                    serial.prints(EOLNC);

                  //serial.prints(EOLNC);
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
                irc_conn->print("NICK "+nick+"\r\n");
                irc_conn->print("USER rewt 0 * :"+nick+"\r\n");
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
