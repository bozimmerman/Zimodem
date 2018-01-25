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

void ZBrowser::switchTo()
{
  currMode=&browseMode;
  serial.setFlowControlType(commandMode.serial.getFlowControlType());
  serial.setPetsciiMode(commandMode.serial.isPetsciiMode());
  savedEcho=commandMode.doEcho;
  commandMode.doEcho=true;
  serial.setXON(true);
  showMenu=true;
  EOLN=commandMode.EOLN;
  EOLNC=EOLN.c_str();
  currState = ZBROW_MAIN;
  lastNumber=0;
  lastString="";
}

void ZBrowser::serialIncoming()
{
  bool crReceived=commandMode.readSerialStream();
  commandMode.clearPlusProgress(); // re-check the plus-escape mode
  if(crReceived)
  {
    doModeCommand();
  }
}

void ZBrowser::switchBackToCommandMode()
{
  commandMode.doEcho=savedEcho;
  currMode = &commandMode;
}

String ZBrowser::fixPathNoSlash(String p)
{
  String finalPath="/";
  int lastX=0;
  uint16_t backStack[256] = {0};
  backStack[0]=1;
  int backX=1;
  for(int i=0;i<p.length() && i<512;i++)
  {
    if(p[i]=='/')
    {
      if(i>lastX)
      {
        String sub=p.substring(lastX,i);
        if(sub.equals("."))
        {
          // do nothing
        }
        else
        if(!sub.equals(".."))
        {
          if(backX > 1)
            finalPath = finalPath.substring(0,backStack[--backX]);
        }
        else
        {
          finalPath = sub + "/";
          backStack[++backX]=finalPath.length();
        }
        lastX=i+1;
      }
    }
  }
  if(finalPath.length()>1)
    finalPath.remove(finalPath.length()-1);
  return finalPath;
}

String ZBrowser::stripDir(String p)
{
  int x=p.lastIndexOf("/");
  if(x<=0)
    return p;
  return p.substring(0,x);
}

String ZBrowser::stripFilename(String p)
{
  int x=p.lastIndexOf("/");
  if((x<0)||(x==p.length()-1))
    return "";
  return p.substring(x+1);
}

String ZBrowser::makePath(String addendum)
{
  if(addendum.length()>0)
  {
    if(addendum.startsWith("/"))
      return fixPathNoSlash(addendum);
    else
      return fixPathNoSlash(path + addendum);
  }
  return fixPathNoSlash(path);
}

bool ZBrowser::isMask(String mask)
{
  return (mask.indexOf("*")>=0) || (mask.indexOf("?")>=0);
}

String ZBrowser::stripArgs(String line, String &argLetters)
{
  while(line.startsWith("-"))
  {
    int x=line.indexOf(' ');
    if(x<0)
      break;
    argLetters += line.substring(1,x);
    line = line.substring(x+1);
    line.trim();
  }
  return line;
}

void ZBrowser::deleteFile(String p, String mask, bool recurse)
{
  File root = SD.open(p);
  if(!root)
    serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
  else
  if(root.isDirectory())
  {
    File file = root.openNextFile();
    while(file)
    {
      if(matches(file.name(), mask))
      {
        if(file.isDirectory())
        {
          if(recurse)
          {
            deleteFile(p+"/"+file.name(),mask,recurse);
            if(!SD.rmdir(p+"/"+file.name()))
              serial.printf("Unable to delete: %s%s",p+"/"+file.name(),EOLNC);
          } 
        }
        else 
        if(!SD.remove(p+"/"+file.name()))
          serial.printf("Unable to delete: %s%s",p+"/"+file.name(),EOLNC);
      }
      file = root.openNextFile();
    }
  }
}

bool ZBrowser::matches(String fname, String mask)
{
  if((mask.length()==0)||(mask.equals("*")))
    return true;
  if(!isMask(mask))
    return mask.equals(fname);
  int f=0;
  for(int i=0;i<mask.length();i++)
  {
    if(f>=fname.length())
      return false;
    if(mask[i]=='?') 
      f++;
    else
    if(mask[i]=='*')
    {
      if(i==mask.length()-1)
        return true;
      int remain=mask.length()-i-1;
      f=fname.length()-remain;
    }
    else
    if(mask[i]!=fname[f++])
      return false;
  }
  return true;
}

void ZBrowser::showDirectory(String p, String mask, String prefix, bool recurse)
{
  File root = SD.open(p);
  if(!root)
    serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
  else
  if(root.isDirectory())
  {
    File file = root.openNextFile();
    while(file)
    {
      if(matches(file.name(), mask))
      {
        if(file.isDirectory())
        {
          serial.printf("d %s%s",file.name(),EOLNC);
          if(recurse)
            showDirectory(p + "/" + file.name(), mask, "  ", recurse);
        }
        else 
          serial.printf("  %s %llu%s",file.name(),file.size(),EOLNC);
      }
      file = root.openNextFile();
    }
    serial.printf("%llu free of %llu total%s",(SD.totalBytes()-SD.usedBytes()),SD.totalBytes(),EOLNC);
  }
  else
    serial.printf("  %s %llu%s",root.name(),root.size(),EOLNC);
}

void ZBrowser::doModeCommand()
{
  String line = commandMode.getNextSerialCommand();
  char c='?';
  for(int i=0;i<line.length();i++)
  {
    if(line[i]<32)
      line.remove(i--);
  }
  int sp=line.indexOf(' ');
  String cmd=line;
  if(sp < 0)
  {
    cmd=line.substring(0,sp);
    cmd.trim();
    line = line.substring(sp+1);
    line.trim();
  }
  switch(currState)
  {
    case ZBROW_MAIN:
    {
      if(cmd.length()==0)
        showMenu=true;
      else
      if(cmd.equalsIgnoreCase("exit")||cmd.equalsIgnoreCase("quit")||cmd.equalsIgnoreCase("x")||cmd.equalsIgnoreCase("endshell"))
      {
        commandMode.showInitMessage();
        switchBackToCommandMode();
        return;
      }
      else
      if(cmd.equalsIgnoreCase("ls")||cmd.equalsIgnoreCase("dir")||cmd.equalsIgnoreCase("$")||cmd.equalsIgnoreCase("list"))
      {
        String argLetters = "";
        line = stripArgs(line,argLetters);
        argLetters.toLowerCase();
        bool recurse=argLetters.indexOf('r')>=0;
        String rawPath = makePath(line);
        String p=stripDir(rawPath);
        String mask=stripFilename(rawPath);
        showDirectory(p,mask,"",recurse);
      }
      else
      if(cmd.equalsIgnoreCase("md")||cmd.equalsIgnoreCase("mkdir")||cmd.equalsIgnoreCase("makedir"))
      {
        String p = makePath(line);
        if(isMask(p) || SD.mkdir(p))
          serial.printf("Illegal path: %s%s",p.c_str(),EOLNC);
      }
      else
      if(cmd.equalsIgnoreCase("rd")||cmd.equalsIgnoreCase("rmdir")||cmd.equalsIgnoreCase("deletedir"))
      {
        String p = makePath(line);
        File root = SD.open(p);
        if(!root)
          serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
        else
        if(!root.isDirectory())
          serial.printf("Not a directory: %s%s",p.c_str(),EOLNC);
        else
        if(!SD.rmdir(p))
          serial.printf("Failed to remove directory: %s%s",p.c_str(),EOLNC);
      }
      else
      if(cmd.equalsIgnoreCase("rm")||cmd.equalsIgnoreCase("del")||cmd.equalsIgnoreCase("delete"))
      {
        String argLetters = "";
        line = stripArgs(line,argLetters);
        argLetters.toLowerCase();
        bool recurse=argLetters.indexOf('r')>=0;
        String rawPath = makePath(line);
        String p=stripDir(rawPath);
        String mask=stripFilename(rawPath);
        deleteFile(p,mask,recurse);
      }
      else
      if(cmd.equals("?")||cmd.equals("help"))
      {
        serial.printf("Commands:%s",EOLNC);
        serial.printf("ls/dir/list/$ (-r) ([path])  - List files%s",EOLNC);
        serial.printf("md/mkdir/makedir [path]  - Create a new directory%s",EOLNC);
        serial.printf("rd/rmdir/deletedir [path]  - Delete a directory%s",EOLNC);
        serial.printf("rm/del/delete (-r) [path]  - Delete a file%s",EOLNC);
        serial.printf("exit/quit/x/endshell  - Quit to command mode%s",EOLNC);
        serial.printf("%s",EOLNC);
      }
      else
        serial.printf("Unknown command: '%s'.  Try '?'.%s",cmd.c_str(),EOLNC);
      showMenu=true;
      break;
    }
  }
}

void ZBrowser::loop()
{
  if(showMenu)
  {
    showMenu=false;
    switch(currState)
    {
      case ZBROW_MAIN:
      {
        serial.printf("%s%s> ",EOLNC,path.c_str());
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

