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

#ifdef INCLUDE_SD_SHELL

static void initSDShell()
{
  if(SD.begin())
  {
    if(SD.cardType() != CARD_NONE)
      browseEnabled = true;
  }
}

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
  strcpy(EOLNC,commandMode.EOLN.c_str());
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
  String finalPath="";
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
        if(sub.equals(".."))
        {
          if(backX > 1)
            finalPath = finalPath.substring(0,backStack[--backX]);
        }
        else
        if(sub.length()>0)
        {
          finalPath += sub;
          finalPath += "/";
          backStack[++backX]=finalPath.length();
        }
        lastX=i+1;
      }
    }
  }

  if(lastX<p.length())
  {
    String sub=p.substring(lastX);
    if(sub.equals("."))
    {
      // do nothing
    }
    else
    if(sub.equals(".."))
    {
      if(backX > 1)
        finalPath = finalPath.substring(0,backStack[--backX]);
    }
    else
    {
      finalPath += sub;
      finalPath += "/";
    }
  }
  if(finalPath.length()==0)
    return "/";
  if(finalPath.length()>1)
    finalPath.remove(finalPath.length()-1);
  return finalPath;
}

String ZBrowser::stripDir(String p)
{
  int x=p.lastIndexOf("/");
  if(x<=0)
    return "/";
  return p.substring(0,x);
}

String ZBrowser::stripFilename(String p)
{
  int x=p.lastIndexOf("/");
  if((x<0)||(x==p.length()-1))
    return "";
  return p.substring(x+1);
}

String ZBrowser::cleanFirstArg(String line)
{
  int state=0;
  String arg="";
  for(int i=0;i<line.length();i++)
  {
    if((line[i]=='\\')&&(i<line.length()-1))
    {
      i++;
      arg += line[i];
    }
    else
    if(line[i]=='\"')
    {
      if((state == 0)&&(arg.length()==0))
        state=1;
      else
      if(state==1)
        break;
      else
        arg += line[i];
    }
    else
    if(line[i] == ' ')
    {
      if(state == 0)
        break;
      arg += line[i];
    }
    else
    {
      arg += line[i];
    }
  }
  return arg;
}

String ZBrowser::cleanRemainArg(String line)
{
  int state=0;
  int ct=0;
  for(int i=0;i<line.length();i++)
  {
    if((line[i]=='\\')&&(i<line.length()-1))
    {
      i++;
      ct++;
    }
    else
    if(line[i]=='\"')
    {
      if((state == 0)&&(ct==0))
        state=1;
      else
      if(state==1)
      {
        String remain=line.substring(i+1);
        remain.trim();
        return cleanOneArg(remain);
      }
      else
        ct++;
    }
    else
    if(line[i] == ' ')
    {
      if(state == 0)
      {
        String remain=line.substring(i+1);
        remain.trim();
        return cleanOneArg(remain);
      }
      ct++;
    }
    else
      ct++;
  }
  return "";
}

String ZBrowser::cleanOneArg(String line)
{
  int state=0;
  String arg="";
  for(int i=0;i<line.length();i++)
  {
    if((line[i]=='\\')&&(i<line.length()-1))
    {
      i++;
      arg += line[i];
    }
    else
    if(line[i]=='\"')
    {
      if((state == 0)&&(arg.length()==0))
        state=1;
      else
      if(state==1)
        break;
      else
        arg += line[i];
    }
    else
      arg += line[i];
  }
  return arg;
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
    {
      argLetters = line.substring(1);
      return "";
    }
    argLetters += line.substring(1,x);
    line = line.substring(x+1);
    line.trim();
  }
  return line;
}

void ZBrowser::copyFiles(String source, String mask, String target, bool recurse, bool overwrite)
{
  int maskFilterLen = source.length();
  if(!source.endsWith("/"))
    maskFilterLen++;

  File root = SD.open(source);
  if(!root)
    serial.printf("Unknown path: %s%s",source.c_str(),EOLNC);
  else
  if(root.isDirectory())
  {
    for(File file = root.openNextFile(); file != null; file = root.openNextFile())
    {
      if(matches(file.name()+maskFilterLen, mask))
      {
        if(file.isDirectory())
        {
          if((mask.length()>0)&&(!recurse))
          {
            if(isMask(mask))
            {
              serial.printf("Skipping: %s%s",file.name(),EOLNC);
              continue;
            }
            if(!SD.exists(target))
              SD.mkdir(target);
            else
            {
              File tstTarg = SD.open(target);
              if(!tstTarg.isDirectory())
              {
                if(!overwrite)
                {
                  serial.printf("Skipping: %s%s",file.name(),EOLNC);
                  continue;
                }
                tstTarg.close();
                SD.remove(target);
                SD.mkdir(target);
              }
              else
                tstTarg.close();
              if(strcmp(tstTarg.name(),file.name())==0)
              {
                  serial.printf("Skipping: %s%s",file.name(),EOLNC);
                  continue;
              }
            }
            copyFiles(file.name(),"*",target,false,overwrite);
            continue;
          }
          String tpath=target;
          tpath += "/";
          tpath += stripFilename(file.name());
          if(!SD.exists(tpath))
          {
            SD.mkdir(tpath);
          }
          else
          {
            File tdir = SD.open(tpath);
            String tnamestr=tdir.name();
            if(!tdir.isDirectory())
            {
              tdir.close();
              if(overwrite)
              {
                SD.remove(tnamestr);
                SD.mkdir(tnamestr);
              }
              else
              {
                  serial.printf("Skipping: %s%s",file.name(),EOLNC);
                  continue;
              }
            }
            else
              tdir.close();
            if(strcmp(tnamestr.c_str(),file.name())==0)
            {
              serial.printf("Skipping: %s%s",file.name(),EOLNC);
              continue;
            }
          }
          if(recurse)
          {
            copyFiles(file.name(),"",tpath,recurse,overwrite);
          } 
        }
        else 
        {
          String tpath=target;
          File tchk = SD.open(target);
          if(tchk)
          {
            if(tchk.isDirectory())
            {
              tpath += "/";
              tpath += stripFilename(file.name());
              tchk.close();
            }
            else
            {
              if(!overwrite)
              {
                serial.printf("File exists: %s%s",tchk.name(),EOLNC);
                tchk.close();
                continue;
              }
              else
              {
                SD.remove(tchk.name());
                tchk.close();
              }
            }
          }
          String sname=file.name();
          size_t len=file.size();
          File tfile = SD.open(tpath,FILE_WRITE);
          for(int i=0;i<len;i++)
          {
            uint8_t c=file.read();
            tfile.write(c);
          }
          tfile.close();
        }
      }
    }
  }
}


void ZBrowser::deleteFile(String p, String mask, bool recurse)
{
  int maskFilterLen = p.length();
  if(!p.endsWith("/"))
    maskFilterLen++;

  File root = SD.open(p);
  if(!root)
    serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
  else
  if(root.isDirectory())
  {
    File file = root.openNextFile();
    while(file)
    {
      if(matches(file.name()+maskFilterLen, mask))
      {
        if(file.isDirectory())
        {
          if(recurse)
          {
            deleteFile(file.name(),mask,recurse);
            if(!SD.rmdir(file.name()))
              serial.printf("Unable to delete: %s%s",file.name()+maskFilterLen,EOLNC);
          } 
        }
        else 
        if(!SD.remove(file.name()))
          serial.printf("Unable to delete: %s%s",file.name()+maskFilterLen,EOLNC);
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
  int maskFilterLen = p.length();
  if(!p.endsWith("/"))
    maskFilterLen++;
    
  File root = SD.open(p);
  if(!root)
    serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
  else
  if(root.isDirectory())
  {
    File file = root.openNextFile();
    while(file)
    {
      if(matches(file.name()+maskFilterLen, mask))
      {
        debugPrintf("file matched:%s\n",file.name());
        if(file.isDirectory())
        {
          serial.printf("%sd %s%s",prefix.c_str(),file.name()+maskFilterLen,EOLNC);
          if(recurse)
          {
            String newPrefix = prefix + "  ";
            showDirectory(file.name(), mask, newPrefix, recurse);
          }
        }
        else
          serial.printf("%s  %s %lu%s",prefix.c_str(),file.name()+maskFilterLen,file.size(),EOLNC);
      }
      else
        debugPrintf("file unmatched:%s (%s)\n",file.name(),mask.c_str());
      file = root.openNextFile();
    }
  }
  else
    serial.printf("  %s %lu%s",root.name(),root.size(),EOLNC);
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
  if(sp > 0)
  {
    cmd=line.substring(0,sp);
    cmd.trim();
    line = line.substring(sp+1);
    line.trim();
  }
  else
    line = "";
  switch(currState)
  {
    case ZBROW_MAIN:
    {
      if(cmd.length()==0)
        showMenu=true;
      else
      if(cmd.equalsIgnoreCase("exit")||cmd.equalsIgnoreCase("quit")||cmd.equalsIgnoreCase("x")||cmd.equalsIgnoreCase("endshell"))
      {
        serial.prints("OK");
        serial.prints(EOLN);
        //commandMode.showInitMessage();
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
        String rawPath = makePath(cleanOneArg(line));
        String p;
        String mask;
        if((line.length()==0)||(line.endsWith("/")))
        {
          p=rawPath;
          mask="";
        }
        else
        {
          p=stripDir(rawPath);
          mask=stripFilename(rawPath);
        }
        
        debugPrintf("ls:%s (%s)\n",p.c_str(),mask.c_str());
        showDirectory(p,mask,"",recurse);
      }
      else
      if(cmd.equalsIgnoreCase("md")||cmd.equalsIgnoreCase("mkdir")||cmd.equalsIgnoreCase("makedir"))
      {
        String p = makePath(line);
        debugPrintf("md:%s\n",p.c_str());
        if((p.length() < 2) || isMask(p) || !SD.mkdir(p))
          serial.printf("Illegal path: %s%s",p.c_str(),EOLNC);
      }
      else
      if(cmd.equalsIgnoreCase("cd"))
      {
        String p = makePath(line);
        debugPrintf("cd:%s\n",p.c_str());
        if(p.length()==0)
          serial.printf("Current path: %s%s",p.c_str(),EOLNC);
        else
        if(p == "/")
          path = "/";
        else
        if(p.length()>1)
        {
          File root = SD.open(p);
          if(!root)
            serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
          else
          if(!root.isDirectory())
            serial.printf("Illegal path: %s%s",p.c_str(),EOLNC);
          else
            path = p + "/";
        }
      }
      else
      if(cmd.equalsIgnoreCase("rd")||cmd.equalsIgnoreCase("rmdir")||cmd.equalsIgnoreCase("deletedir"))
      {
        String p = makePath(line);
        debugPrintf("rd:%s\n",p.c_str());
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
      if(cmd.equalsIgnoreCase("cat")||cmd.equalsIgnoreCase("type"))
      {
        String p = makePath(line);
        debugPrintf("cat:%s\n",p.c_str());
        File root = SD.open(p);
        if(!root)
          serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
        else
        if(root.isDirectory())
          serial.printf("Is a directory: %s%s",p.c_str(),EOLNC);
        else
        {
          root.close();
          File f=SD.open(p, FILE_READ);
          for(int i=0;i<f.size();i++)
            serial.write(f.read());
          f.close();
        }
      }
      else
      if(cmd.equalsIgnoreCase("xget"))
      {
        String p = makePath(line);
        debugPrintf("xget:%s\n",p.c_str());
        File root = SD.open(p);
        if(!root)
          serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
        else
        if(root.isDirectory())
        {
          serial.printf("Is a directory: %s%s",p.c_str(),EOLNC);
          root.close();
        }
        else
        {
          root.close();
          File rfile = SD.open(p, FILE_READ);
          String errors="";
          serial.printf("Go to XModem download.%s",EOLNC);
          serial.flushAlways();
          initXSerial(commandMode.getFlowControlType());
          if(xDownload(rfile,errors))
          {
            rfile.close();
            delay(1000);
            serial.printf("Download completed successfully.%s",EOLNC);
          }
          else
          {
            rfile.close();
            delay(1000);
            serial.printf("Download failed (%s).%s",errors.c_str(),EOLNC);
          }
        }
      }
      else
      if(cmd.equalsIgnoreCase("xput"))
      {
        String p = makePath(line);
        debugPrintf("xput:%s\n",p.c_str());
        File root = SD.open(p);
        if(root)
        {
          serial.printf("File exists: %s%s",root.name(),EOLNC);
          root.close();
        }
        else
        {
          String dirNm=stripDir(p);
          File rootDir=SD.open(dirNm);
          if((!rootDir)||(!rootDir.isDirectory()))
          {
            serial.printf("Path doesn't exist: %s%s",dirNm.c_str(),EOLNC);
            if(rootDir)
              rootDir.close();
          }
          else
          {
            File rfile = SD.open(p, FILE_WRITE);
            String errors="";
            serial.printf("Go to XModem upload.%s",EOLNC);
            serial.flushAlways();
            initXSerial(commandMode.getFlowControlType());
            if(xUpload(rfile,errors))
            {
              rfile.close();
              delay(1000);
              serial.printf("Upload completed successfully.%s",EOLNC);
            }
            else
            {
              rfile.close();
              delay(1000);
              serial.printf("Upload failed (%s).%s",errors.c_str(),EOLNC);
            }
          }
        }
      }
      else
      if(cmd.equalsIgnoreCase("zget")||cmd.equalsIgnoreCase("rz")||cmd.equalsIgnoreCase("rz.exe"))
      {
        String p = makePath(line);
        debugPrintf("zget:%s\n",p.c_str());
        File root = SD.open(p);
        if(!root)
          serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
        else
        if(root.isDirectory())
        {
          serial.printf("Is a directory: %s%s",p.c_str(),EOLNC);
          root.close();
        }
        else
        {
          root.close();
          String errors="";
          initZSerial(commandMode.getFlowControlType());
          if(zDownload(SD,p,errors))
          {
            delay(1000);
            serial.printf("Download completed successfully.%s",EOLNC);
          }
          else
          {
            delay(1000);
            serial.printf("Download failed (%s).%s",errors.c_str(),EOLNC);
          }
        }
      }
      else
      if(cmd.equalsIgnoreCase("zput")||cmd.equalsIgnoreCase("sz"))
      {
        String p = makePath(line);
        debugPrintf("zput:%s\n",p.c_str());
        String dirNm=p;
        File rootDir=SD.open(dirNm);
        if((!rootDir)||(!rootDir.isDirectory()))
        {
          serial.printf("Path doesn't exist: %s%s",dirNm.c_str(),EOLNC);
          if(rootDir)
            rootDir.close();
        }
        else
        {
          String rootDirNm = rootDir.name();
          rootDir.close();
          String errors="";
          serial.printf("Go to ZModem upload.%s",EOLNC);
          serial.flushAlways();
          initZSerial(commandMode.getFlowControlType());
          if(zUpload(SD,rootDirNm,errors))
          {
            delay(1000);
            serial.printf("Upload completed successfully.%s",EOLNC);
          }
          else
          {
            delay(1000);
            serial.printf("Upload failed (%s).%s",errors.c_str(),EOLNC);
          }
        }
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
        debugPrintf("rm:%s (%s)\n",p.c_str(),mask.c_str());
        deleteFile(p,mask,recurse);
      }
      else
      if(cmd.equalsIgnoreCase("cp")||cmd.equalsIgnoreCase("copy"))
      {
        String argLetters = "";
        line = stripArgs(line,argLetters);
        argLetters.toLowerCase();
        bool recurse=argLetters.indexOf('r')>=0;
        bool overwrite=argLetters.indexOf('f')>=0;
        String p1=makePath(cleanFirstArg(line));
        String p2=makePath(cleanRemainArg(line));
        String mask;
        if((line.length()==0)||(line.endsWith("/")))
          mask="";
        else
        {
          mask=stripFilename(p1);
          p1=stripDir(p1);
        }
        
        debugPrintf("cp:%s (%s) -> %s\n",p1.c_str(),mask.c_str(), p2.c_str());
        copyFiles(p1,mask,p2,recurse,overwrite);
      }
      else
      if(cmd.equalsIgnoreCase("df")||cmd.equalsIgnoreCase("free")||cmd.equalsIgnoreCase("info"))
      {
        serial.printf("%llu free of %llu total%s",(SD.totalBytes()-SD.usedBytes()),SD.totalBytes(),EOLNC);
      }
      else
      if(cmd.equalsIgnoreCase("ren")||cmd.equalsIgnoreCase("rename"))
      {
        String p1=makePath(cleanFirstArg(line));
        String p2=makePath(cleanRemainArg(line));
        debugPrintf("ren:%s -> %s\n",p1.c_str(), p2.c_str());
        if(p1 == p2)
          serial.printf("File exists: %s%s",p1.c_str(),EOLNC);
        else
        if(SD.exists(p2))
          serial.printf("File exists: %s%s",p2.c_str(),EOLNC);
        else
        {
          if(!SD.rename(p1,p2))
            serial.printf("Failed to rename: %s%s",p1.c_str(),EOLNC);
        }
      }
      else
      if(cmd.equalsIgnoreCase("wget"))
      {
        String p1=cleanFirstArg(line);
        String p2=makePath(cleanRemainArg(line));
        debugPrintf("wget:%s -> %s\n",p1.c_str(), p2.c_str());
        if((p1.length()<8)
        || ((strcmp(p1.substring(0,7).c_str(),"http://") != 0)
           && (strcmp(p1.substring(0,9).c_str(),"https://") != 0)))
          serial.printf("Not a url: %s%s",p1.c_str(),EOLNC);
        else
        if(SD.exists(p2))
          serial.printf("File exists: %s%s",p2.c_str(),EOLNC);
        else
        {
          char buf[p1.length()+1];
          strcpy(buf,p1.c_str());
          char *hostIp;
          char *req;
          int port;
          bool doSSL;
          if(!parseWebUrl((uint8_t *)buf,&hostIp,&req,&port,&doSSL))
            serial.printf("Invalid url: %s",p1.c_str());
          else
          if(!doWebGet(hostIp, port, &SD, p2.c_str(), req, doSSL))
            serial.printf("Wget failed: %s to file %s",p1.c_str(),p2.c_str());
        }
      }
      else
      if(cmd.equalsIgnoreCase("fget"))
      {
        String p1=cleanFirstArg(line);
        String p2=makePath(cleanRemainArg(line));
        debugPrintf("fget:%s -> %s\n",p1.c_str(), p2.c_str());
        char *tmp=0;
        if((p1.length()<11)
        || ((strcmp(p1.substring(0,6).c_str(),"ftp://") != 0)
           && (strcmp(p1.substring(0,8).c_str(),"ftps://") != 0)))
          serial.printf("Not a url: %s%s",p1.c_str(),EOLNC);
        /*
        else
        if(((tmp=strchr(p1.c_str(),'@'))==0)
        ||(strchr(p1.c_str()+6,':')>tmp)
        ||(strchr(p1.c_str()+6,':')==0))
            serial.printf("Missing username:password@ syntax: %s%s",p1.c_str(),EOLNC);
        */
        else
        if(SD.exists(p2))
          serial.printf("File exists: %s%s",p2.c_str(),EOLNC);
        else
        {
          uint8_t buf[p1.length()+1];
          strcpy((char *)buf,p1.c_str());
          char *hostIp;
          char *req;
          int port;
          bool doSSL;
          char *username;
          char *password;
          if(!parseFTPUrl(buf,&hostIp,&req,&port,&doSSL,&username,&password))
            serial.printf("Invalid url: %s",p1.c_str());
          else
          if(!doFTPGet(&SD, hostIp, port, p2.c_str(), req, username, password, doSSL))
            serial.printf("Fget failed: %s to file %s",p1.c_str(),p2.c_str());
        }
      }
      else
      if(cmd.equalsIgnoreCase("fls") || cmd.equalsIgnoreCase("fdir"))
      {
        String p1=line;
        debugPrintf("fls:%s\n",p1.c_str());
        char *tmp=0;
        if((p1.length()<11)
        || ((strcmp(p1.substring(0,6).c_str(),"ftp://") != 0)
           && (strcmp(p1.substring(0,8).c_str(),"ftps://") != 0)))
          serial.printf("Not a url: %s%s",p1.c_str(),EOLNC);
        /*
        else
        if(((tmp=strchr(p1.c_str(),'@'))==0)
        ||(strchr(p1.c_str()+6,':')>tmp)
        ||(strchr(p1.c_str()+6,':')==0))
            serial.printf("Missing username:password@ syntax: %s%s",p1.c_str(),EOLNC);
        */
        else
        {
          uint8_t buf[p1.length()+1];
          strcpy((char *)buf,p1.c_str());
          char *hostIp;
          char *req;
          int port;
          bool doSSL;
          char *username;
          char *password;
          if(!parseFTPUrl(buf,&hostIp,&req,&port,&doSSL,&username,&password))
            serial.printf("Invalid url: %s",p1.c_str());
          else
          if(!doFTPLS(&serial, hostIp, port, req, username, password, doSSL))
            serial.printf("Fls failed: %s",p1.c_str());
        }
      }
      else
      if(cmd.equalsIgnoreCase("mv")||cmd.equalsIgnoreCase("move"))
      {
        String argLetters = "";
        line = stripArgs(line,argLetters);
        argLetters.toLowerCase();
        bool overwrite=argLetters.indexOf('f')>=0;
        String p1=makePath(cleanFirstArg(line));
        String p2=makePath(cleanRemainArg(line));
        String mask;
        if((line.length()==0)||(line.endsWith("/")))
          mask="";
        else
        {
          mask=stripFilename(p1);
          p1=stripDir(p1);
        }
        debugPrintf("mv:%s -> %s\n",p1.c_str(),p2.c_str());
        if((mask.length()==0)||(!isMask(mask)))
        {
          if(p1 == p2)
            serial.printf("File exists: %s%s",p1.c_str(),EOLNC);
          else
          if(SD.exists(p2) && (!overwrite))
            serial.printf("File exists: %s%s",p2.c_str(),EOLNC);
          else
          {
            if(SD.exists(p2))
              SD.remove(p2);
            if(!SD.rename(p1,p2))
              serial.printf("Failed to rename: %s%s",p1.c_str(),EOLNC);
          }
        }
        else
        {
          copyFiles(p1,mask,p2,false,overwrite);
          deleteFile(p1,mask,false);
        }
      }
      else
      if(cmd.equals("?")||cmd.equals("help"))
      {
        serial.printf("Commands:%s",EOLNC);
        serial.printf("ls/dir/list/$ (-r) ([path])  - List files%s",EOLNC);
        serial.printf("cd [path]  - Change to new directory%s",EOLNC);
        serial.printf("md/mkdir/makedir [path]  - Create a new directory%s",EOLNC);
        serial.printf("rd/rmdir/deletedir [path]  - Delete a directory%s",EOLNC);
        serial.printf("rm/del/delete (-r) [path]  - Delete a file%s",EOLNC);
        serial.printf("cp/copy (-r) (-f) [path] [path]  - Copy file(s)%s",EOLNC);
        serial.printf("ren/rename [path] [path]  - Rename a file%s",EOLNC);
        serial.printf("mv/move (-f) [path] [path]  - Move file(s)%s",EOLNC);
        serial.printf("cat/type [path]  - View a file(s)%s",EOLNC);
        serial.printf("df/free/info - Show space remaining%s",EOLNC);
        serial.printf("xget/zget [path]  - Download a file%s",EOLNC);
        serial.printf("xput/zput [path]  - Upload a file%s",EOLNC);
        serial.printf("wget [http://url] [path]  - Download url to file%s",EOLNC);
        serial.printf("fget [ftp://user:pass@url] [path]  - FTP to file%s",EOLNC);
        serial.printf("fdir [ftp://user:pass@url]  - ftp url dir%s",EOLNC);
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
#else
static void initSDShell()
{}
#endif
