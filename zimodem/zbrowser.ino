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

#ifdef INCLUDE_SD_SHELL

static void initSDShell()
{
  if(SD.begin())
  {
      debugPrintf("External SD card initialized.\r\n");
  }
}

ZBrowser::~ZBrowser()
{
#ifdef INCLUDE_FTP
  if(ftpHost != 0)
  {
    delete ftpHost;
    ftpHost = 0;
  }
#endif
}

void ZBrowser::switchTo()
{
  debugPrintf("\n\rMode:Browse\r\n");
  currMode=&browseMode;
  init();
}

void ZBrowser::init()
{
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
    if(commandMode.doEcho)
      serial.prints(EOLN);
    String line =commandMode.getNextSerialCommand();
    doModeCommand(line,true);
  }
}

void ZBrowser::switchBackToCommandMode()
{
  debugPrintf("\r\nMode:Command\r\n");

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
      }
      else
      if((i==0) && (i<p.length()-1))
        finalPath = "/";
      lastX=i+1;
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
      finalPath += "/"; // why this?! -- oh, so it can be removed below?
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

int ZBrowser::argNum(String argLetters, char argLetter, int def)
{
  int num = 0;
  int x = argLetters.indexOf(argLetter);
  if(x >= 0)
  {
    x++;
    String cnStr = "";
    while((x < argLetters.length())&&(strchr("0123456789",argLetters[x])!=null))
      cnStr += argLetters[x++];
    num = atoi(argLetters.c_str());
  }
  return num;
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

bool ZBrowser::copyFiles(String source, String mask, String target, bool recurse, bool overwrite)
{
  File root = SD.open(source);
  if(!root)
  {
    if(!quiet)
      serial.printf("Unknown path: %s%s",source.c_str(),EOLNC);
    return false;
  }
  
  if(root.isDirectory())
  {
    if(!SD.exists(target)) // cp d a
    {
      SD.mkdir(target);
    }
    else
    {
      File DD=SD.open(target); //cp d d2, cp d f 
      if(!DD.isDirectory())
      {
        if(!quiet)
          serial.printf("File exists: %s%s",DD.name(),EOLNC);
        DD.close();
        return false;
      }
    }
    for(File file = root.openNextFile(); file != null; file = root.openNextFile())
    {
      if(matches(file.name(), mask))
      {
        debugPrintf("file matched:%s\r\n",file.name());
        String tpath = target;
        if(file.isDirectory())
        {
          if(!recurse)
          {
            if(!quiet)
              serial.printf("Skipping: %s%s",file.name(),EOLNC);
          }
          else
          {
            if(!tpath.endsWith("/"))
              tpath += "/";
            tpath += file.name();
          }
        }
        copyFiles(source + "/" + file.name(),"",tpath,false,overwrite);
      }
    }
  }
  else
  {
    String tpath = target;
    if(SD.exists(tpath))
    {
      File DD=SD.open(tpath);
      if(DD.isDirectory()) // cp f d, cp f .
      {
        if(!tpath.endsWith("/"))
          tpath += "/";
        tpath += root.name();
        debugPrintf("file xform to file in dir:%s\r\n",tpath.c_str());
      }
      DD.close();
    }
    if(SD.exists(tpath))
    {
      File DD=SD.open(tpath);
      if(strcmp(DD.name(),root.name())==0)
      {
        if(!quiet)
          serial.printf("File exists: %s%s",DD.name(),EOLNC);
        DD.close();
        return false;
      }
      else
      if(!overwrite) // cp f a, cp f e
      {
        if(!quiet)
          serial.printf("File exists: %s%s",DD.name(),EOLNC);
        DD.close();
        return false;
      }
      else
      {
        DD.close();
        SD.remove(tpath);
      }
    }
    size_t len=root.size();
    File tfile = SD.open(tpath,FILE_WRITE);
    for(int i=0;i<len;i++)
    {
      uint8_t c=root.read();
      tfile.write(c);
    }
    tfile.close();
  }
  return true;
}

void ZBrowser::makeFileList(String ***l, int *n, String p, String mask, bool recurse)
{
  File root = SD.open(p);
  if(!root)
  {
    if(!quiet)
      serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
  }
  else
  if(root.isDirectory())
  {
    File file = root.openNextFile();
    while(file)
    {
      if(matches(file.name(), mask))
      {
        String fileName = file.name();
        if(file.isDirectory())
        {
          if(recurse)
          {
            file = root.openNextFile();
            makeFileList(l,n,p + "/" + fileName.c_str(),"*",recurse);
          } 
        }
        else
        {
          file = root.openNextFile();
          String **newList = (String **)malloc(sizeof(String *)*(*n+1));
          for(int i=0;i<*n;i++)
            newList[i]=(*l)[i];
          free(*l);
          newList[*n]=new String(fileName.c_str());
          (*n)++;
          *l=newList;
        }
      }
      else
        file = root.openNextFile();
    }
  }
}

bool ZBrowser::deleteFile(String p, String mask, bool recurse)
{
  bool success = true;
  File root = SD.open(p);
  if(!root)
  {
    if(!quiet)
      serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
    success = false;
  }
  else
  if(root.isDirectory())
  {
    File file = root.openNextFile();
    while(file)
    {
      if(matches(file.name(), mask))
      {
        String fileName = file.name();
        String delFile = p + "/" + fileName;
        if(file.isDirectory())
        {
          if(recurse)
          {
            file = root.openNextFile();
            deleteFile(delFile,"*",recurse);
            if(!SD.rmdir(delFile))
            {
              if(!quiet)
                serial.printf("Unable to delete: %s%s",delFile.c_str(),EOLNC);
              success = false;
            }
          } 
          else
          {
            if(!quiet)
              serial.printf("Skipping: %s%s",delFile.c_str(),EOLNC);
            file = root.openNextFile();
          }
        }
        else
        {
          file = root.openNextFile();
          if(!SD.remove(delFile))
          {
            if(!quiet)
              serial.printf("Unable to delete: %s%s",delFile.c_str(),EOLNC);
            success = false;
          }
        }
      }
      else
        file = root.openNextFile();
    }
  }
  return success;
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
  {
    if(!quiet)
      serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
  }
  else
  if(root.isDirectory())
  {
    File file = root.openNextFile();
    while(file)
    {
      if(matches(file.name(), mask))
      {
        debugPrintf("file matched:%s\r\n",file.name());
        if(file.isDirectory())
        {
          serial.printf("%sd %s%s",prefix.c_str(),file.name(),EOLNC);
          if(recurse)
          {
            String newPrefix = prefix + "  ";
            showDirectory(p + "/" + file.name(), mask, newPrefix, recurse);
          }
        }
        else
          serial.printf("%s  %s %lu%s",prefix.c_str(),file.name(),file.size(),EOLNC);
      }
      else
        debugPrintf("file unmatched:%s (%s)\r\n",file.name(),mask.c_str());
      file = root.openNextFile();
    }
  }
  else
    serial.printf("  %s %lu%s",root.name(),root.size(),EOLNC);
}

bool ZBrowser::doDirCommand(String &line, bool showShellOutput)
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
    mask=stripFilename(rawPath);
    if((mask.length()>0)&&(isMask(mask)))
      p=stripDir(rawPath);
    else
    {
      mask="";
      p=rawPath;
    }
  }
  showDirectory(p,mask,"",recurse);
  return true;
}

bool ZBrowser::doMkDirCommand(String &line, bool showShellOutput)
{
  String p = makePath(cleanOneArg(line));
  debugPrintf("md:%s\r\n",p.c_str());
  if((p.length() < 2) || isMask(p) || !SD.mkdir(p))
  {
    if(!quiet)
      serial.printf("Illegal path: %s%s",p.c_str(),EOLNC);
    return false;
  }
  return true;
}

bool ZBrowser::doCdDirCommand(String &line, bool showShellOutput)
{
  bool success = true;
  String p = makePath(cleanOneArg(line));
  debugPrintf("cd:%s\r\n",p.c_str());
  if(p.length()==0)
  {
    if(!quiet)
      serial.printf("Current path: %s%s",p.c_str(),EOLNC);
  }
  else
  if(p == "/")
    path = "/";
  else
  if(p.length()>1)
  {
    File root = SD.open(p);
    if(!root)
    {
      if(!quiet)
        serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
      success = false;
    }
    else
    if(!root.isDirectory())
    {
      if(!quiet)
        serial.printf("Illegal path: %s%s",p.c_str(),EOLNC);
      success = false;
    }
    else
      path = p + "/";
  }
  return success;
}

bool ZBrowser::doRmDirCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p = makePath(cleanOneArg(line));
  debugPrintf("rd:%s\r\n",p.c_str());
  File root = SD.open(p);
  if(!root)
  {
    if(!quiet)
      serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
    success = false;
  }
  else
  if(!root.isDirectory())
  {
    if(!quiet)
      serial.printf("Not a directory: %s%s",p.c_str(),EOLNC);
  }
  else
  if(!SD.rmdir(p))
  {
    if(!quiet)
      serial.printf("Failed to remove directory: %s%s",p.c_str(),EOLNC);
    success = false;
  }
  return success;
}

bool ZBrowser::doCatCommand(String &line, bool showShellOutput)
{
  bool success = true;
  String argLetters = "";
  line = stripArgs(line,argLetters);
  argLetters.toLowerCase();
  bool dohex=argLetters.indexOf('h')>=0;
  String p = makePath(cleanOneArg(line));
  debugPrintf("cat:%s\r\n",p.c_str());
  File root = SD.open(p);
  if(!root)
  {
    if(!quiet)
      serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
    success = false;
  }
  else
  if(root.isDirectory())
  {
    if(!quiet)
      serial.printf("Is a directory: %s%s",p.c_str(),EOLNC);
    success = false;
  }
  else
  {
    root.close();
    File f=SD.open(p, FILE_READ);
    int hexct = 0;
    for(int i=0;i<f.size();i++)
    {
      if(serial.available())
      {
        if(serial.read() == ' ')
          break;
      }
      if(dohex)
      {
        serial.print(TOHEX(f.read()));
        if(++hexct > 15)
        {
          serial.print(EOLNC);
          hexct=0;
        }
        else
          serial.write(' ');
      }
      else
        serial.write(f.read());
    }
    serial.print(EOLNC);
    f.close();
  }
  return success;
}

bool ZBrowser::doXGetCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p = makePath(cleanOneArg(line));
  debugPrintf("xget:%s\r\n",p.c_str());
  File root = SD.open(p);
  if(!root)
  {
    if(!quiet)
      serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
    success = false;
  }
  else
  if(root.isDirectory())
  {
    if(!quiet)
      serial.printf("Is a directory: %s%s",p.c_str(),EOLNC);
    root.close();
    success = false;
  }
  else
  {
    root.close();
    File rfile = SD.open(p, FILE_READ);
    String errors="";
    if(!quiet)
      serial.printf("Go to XModem download.%s",EOLNC);
    serial.flushAlways();
    if(xDownload(commandMode.getFlowControlType(), rfile,errors))
    {
      rfile.close();
      delay(2000);
      if(!quiet)
        serial.printf("Download completed successfully.%s",EOLNC);
    }
    else
    {
      rfile.close();
      delay(2000);
      if(!quiet)
        serial.printf("Download failed (%s).%s",errors.c_str(),EOLNC);
      success = false;
    }
  }
  return success;
}

bool ZBrowser::doXPutCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p = makePath(cleanOneArg(line));
  debugPrintf("xput:%s\r\n",p.c_str());
  File root = SD.open(p);
  if(root)
  {
    if(!quiet)
      serial.printf("File exists: %s%s",root.name(),EOLNC);
    root.close();
    success = false;
  }
  else
  {
    String dirNm=stripDir(p);
    File rootDir=SD.open(dirNm);
    if((!rootDir)||(!rootDir.isDirectory()))
    {
      if(!quiet)
        serial.printf("Path doesn't exist: %s%s",dirNm.c_str(),EOLNC);
      if(rootDir)
        rootDir.close();
      success = false;
    }
    else
    {
      File rfile = SD.open(p, FILE_WRITE);
      String errors="";
      if(!quiet)
        serial.printf("Go to XModem upload.%s",EOLNC);
      serial.flushAlways();
      if(xUpload(commandMode.getFlowControlType(), rfile,errors))
      {
        rfile.close();
        delay(2000);
        if(!quiet)
          serial.printf("Upload completed successfully.%s",EOLNC);
      }
      else
      {
        rfile.close();
        delay(2000);
        if(!quiet)
          serial.printf("Upload failed (%s).%s",errors.c_str(),EOLNC);
        success = false;
      }
    }
  }
  return success;
}

bool ZBrowser::doYGetCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p = makePath(cleanOneArg(line));
  debugPrintf("yget:%s\r\n",p.c_str());
  File root = SD.open(p);
  if(!root)
  {
    if(!quiet)
      serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
    success = false;
  }
  else
  if(root.isDirectory())
  {
    if(!quiet)
      serial.printf("Is not a directory: %s%s",p.c_str(),EOLNC);
    root.close();
    success = false;
  }
  else
  {
    String errors="";
    if(!quiet)
      serial.printf("Go to YModem download.%s",EOLNC);
    serial.flushAlways();
    if(yDownload(&SD, commandMode.getFlowControlType(), root,errors))
    {
      delay(2000);
      if(!quiet)
        serial.printf("Download completed successfully.%s",EOLNC);
    }
    else
    {
      delay(2000);
      if(!quiet)
        serial.printf("Download failed (%s).%s",errors.c_str(),EOLNC);
      success = false;
    }
  }
  return success;
}

bool ZBrowser::doYPutCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p = makePath(cleanOneArg(line));
  debugPrintf("yput:%s\r\n",p.c_str());
  File rootDir=SD.open(p);
  if((!rootDir)||(!rootDir.isDirectory()))
  {
    if(!quiet)
      serial.printf("Path doesn't exist: %s%s",p.c_str(),EOLNC);
    if(rootDir)
      rootDir.close();
    success = false;
  }
  else
  {
    String errors="";
    if(!quiet)
      serial.printf("Go to YModem upload.%s",EOLNC);
    serial.flushAlways();
    if(yUpload(&SD, commandMode.getFlowControlType(), rootDir, errors))
    {
      rootDir.close();
      delay(2000);
      if(!quiet)
        serial.printf("Upload completed successfully.%s",EOLNC);
    }
    else
    {
      rootDir.close();
      delay(2000);
      if(!quiet)
        serial.printf("Upload failed (%s).%s",errors.c_str(),EOLNC);
      success = false;
    }
  }
  return success;
}

bool ZBrowser::doKGetCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String rawPath = makePath(cleanOneArg(line));
  String p=stripDir(rawPath);
  String mask=stripFilename(rawPath);
  String errors="";
  String **fileList=(String**)malloc(sizeof(String *));
  int numFiles=0;
  makeFileList(&fileList,&numFiles,p,mask,true);
  if(!quiet)
    serial.printf("Go to Kermit download.%s",EOLNC);
  serial.flushAlways();
  if(kDownload(commandMode.getFlowControlType(),SD,fileList,numFiles,errors))
  {
    delay(2000);
    if(!quiet)
      serial.printf("Download completed successfully.%s",EOLNC);
    serial.flushAlways();
  }
  else
  {
    delay(2000);
    if(!quiet)
      serial.printf("Download failed (%s).%s",errors.c_str(),EOLNC);
    serial.flushAlways();
    success = false;
  }
  for(int i=0;i<numFiles;i++)
    delete(fileList[i]);
  delete(fileList);
  return success;
}

bool ZBrowser::doKPutCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p = makePath(cleanOneArg(line));
  debugPrintf("kput:%s\r\n",p.c_str());
  String dirNm=p;
  File rootDir=SD.open(dirNm);
  if((!rootDir)||(!rootDir.isDirectory()))
  {
    if(!quiet)
      serial.printf("Path doesn't exist: %s%s",dirNm.c_str(),EOLNC);
    if(rootDir)
      rootDir.close();
  }
  else
  {
    String rootDirNm = rootDir.name();
    rootDir.close();
    String errors="";
    if(!quiet)
      serial.printf("Go to Kermit upload.%s",EOLNC);
    serial.flushAlways();
    if(kUpload(commandMode.getFlowControlType(),SD,rootDirNm,errors))
    {
      delay(2000);
      if(!quiet)
        serial.printf("Upload completed successfully.%s",EOLNC);
      serial.flushAlways();
    }
    else
    {
      delay(2000);
      if(!quiet)
        serial.printf("Upload failed (%s).%s",errors.c_str(),EOLNC);
      serial.flushAlways();
      success = false;
    }
  }
  return success;
}

bool ZBrowser::doZGetCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p = makePath(cleanOneArg(line));
  debugPrintf("zget:%s\r\n",p.c_str());
  File root = SD.open(p);
  if(!root)
  {
    if(!quiet)
      serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
    success = false;
  }
  else
  if(root.isDirectory())
  {
    if(!quiet)
      serial.printf("Is a directory: %s%s",p.c_str(),EOLNC);
    root.close();
    success = false;
  }
  else
  {
    root.close();
    String errors="";
    if(zDownload(commandMode.getFlowControlType(),SD,p,errors))
    {
      delay(2000);
      if(!quiet)
        serial.printf("Download completed successfully.%s",EOLNC);
    }
    else
    {
      delay(2000);
      if(!quiet)
        serial.printf("Download failed (%s).%s",errors.c_str(),EOLNC);
      success = false;
    }
  }
  return success;
}

bool ZBrowser::doZPutCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p = makePath(cleanOneArg(line));
  debugPrintf("zput:%s\r\n",p.c_str());
  String dirNm=p;
  File rootDir=SD.open(dirNm);
  if((!rootDir)||(!rootDir.isDirectory()))
  {
    if(!quiet)
      serial.printf("Path doesn't exist: %s%s",dirNm.c_str(),EOLNC);
    if(rootDir)
      rootDir.close();
    success = false;
  }
  else
  {
    String rootDirNm = rootDir.name();
    rootDir.close();
    String errors="";
    if(!quiet)
      serial.printf("Go to ZModem upload.%s",EOLNC);
    serial.flushAlways();
    if(zUpload(commandMode.getFlowControlType(),SD,rootDirNm,errors))
    {
      delay(2000);
      if(!quiet)
        serial.printf("Upload completed successfully.%s",EOLNC);
    }
    else
    {
      delay(2000);
      if(!quiet)
        serial.printf("Upload failed (%s).%s",errors.c_str(),EOLNC);
      success = false;
    }
  }
  return success;
}

bool ZBrowser::doPGetCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p = makePath(cleanOneArg(line));
  debugPrintf("pget:%s\r\n",p.c_str());
  File root = SD.open(p);
  if(!root)
  {
    if(!quiet)
      serial.printf("Unknown path: %s%s",p.c_str(),EOLNC);
    success = false;
  }
  else
  if(root.isDirectory())
  {
    if(!quiet)
      serial.printf("Is a directory: %s%s",p.c_str(),EOLNC);
    root.close();
    success = false;
  }
  else
  {
    root.close();
    File rfile = SD.open(p, FILE_READ);
    String errors="";
    if(!quiet)
      serial.printf("Go to Punter download.%s",EOLNC);
    serial.flushAlways();
    if(pDownload(commandMode.getFlowControlType(), rfile,errors))
    {
      rfile.close();
      delay(2000);
      if(!quiet)
        serial.printf("Download completed successfully.%s",EOLNC);
    }
    else
    {
      rfile.close();
      delay(2000);
      if(!quiet)
        serial.printf("Download failed (%s).%s",errors.c_str(),EOLNC);
      success = false;
    }
  }
  return success;
}

bool ZBrowser::doPPutCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p = makePath(cleanOneArg(line));
  debugPrintf("pput:%s\r\n",p.c_str());
  File root = SD.open(p);
  if(root)
  {
    if(!quiet)
      serial.printf("File exists: %s%s",root.name(),EOLNC);
    root.close();
    success = false;
  }
  else
  {
    String dirNm=stripDir(p);
    File rootDir=SD.open(dirNm);
    if((!rootDir)||(!rootDir.isDirectory()))
    {
      if(!quiet)
        serial.printf("Path doesn't exist: %s%s",dirNm.c_str(),EOLNC);
      if(rootDir)
        rootDir.close();
      success = false;
    }
    else
    {
      root.close();
      rootDir.close();
      File rfile = SD.open(p, FILE_WRITE);
      String errors="";
      if(!quiet)
        serial.printf("Go to Punter upload.%s",EOLNC);
      serial.flushAlways();
      if(pUpload(commandMode.getFlowControlType(), rfile,errors))
      {
        rfile.close();
        delay(2000);
        if(!quiet)
          serial.printf("Upload completed successfully.%s",EOLNC);
      }
      else
      {
        rfile.close();
        delay(2000);
        if(!quiet)
          serial.printf("Upload failed (%s).%s",errors.c_str(),EOLNC);
        success = false;
      }
    }
  }
  return success;
}

bool ZBrowser::doRmCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String argLetters = "";
  line = stripArgs(line,argLetters);
  argLetters.toLowerCase();
  bool recurse=argLetters.indexOf('r')>=0;
  String rawPath = makePath(cleanOneArg(line));
  String p=stripDir(rawPath);
  String mask=stripFilename(rawPath);
  debugPrintf("rm:%s (%s)\r\n",p.c_str(),mask.c_str());
  deleteFile(p,mask,recurse);
  return success;
}

bool ZBrowser::doCpCommand(String &line, bool showShellOutput)
{
  bool success=true;
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
    if(!isMask(mask))
      mask="";
    else
      p1=stripDir(p1);
  }
  debugPrintf("cp:%s (%s) -> %s\r\n",p1.c_str(),mask.c_str(), p2.c_str());
  success = copyFiles(p1,mask,p2,recurse,overwrite);
  return success;
}

bool ZBrowser::doRenCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p1=makePath(cleanFirstArg(line));
  String p2=makePath(cleanRemainArg(line));
  debugPrintf("ren:%s -> %s\r\n",p1.c_str(), p2.c_str());
  if(p1 == p2)
  {
    if(!quiet)
      serial.printf("File exists: %s%s",p1.c_str(),EOLNC);
    success = false;
  }
  else
  if(SD.exists(p2))
  {
    if(!quiet)
      serial.printf("File exists: %s%s",p2.c_str(),EOLNC);
    success = false;
  }
  else
  {
    if(!SD.rename(p1,p2))
    {
      if(!quiet)
        serial.printf("Failed to rename: %s%s",p1.c_str(),EOLNC);
      success = false;
    }
  }
  return success;
}

bool ZBrowser::doWGetCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p1=cleanFirstArg(line);
  String p2=makePath(cleanRemainArg(line));
  debugPrintf("wget:%s -> %s\r\n",p1.c_str(), p2.c_str());
  if((p1.length()<8)
  || ((strcmp(p1.substring(0,7).c_str(),"http://") != 0)
    && (strcmp(p1.substring(0,9).c_str(),"https://") != 0)
    && (strcmp(p1.substring(0,11).c_str(),"gophers://") != 0)
    && (strcmp(p1.substring(0,10).c_str(),"gopher://") != 0)))
  {
    if(!quiet)
      serial.printf("Not a url: %s%s",p1.c_str(),EOLNC);
    success = false;
  }
  else
  if(SD.exists(p2))
  {
    if(!quiet)
      serial.printf("File exists: %s%s",p2.c_str(),EOLNC);
    success = false;
  }
  else
  {
    char buf[p1.length()+1];
    strcpy(buf,p1.c_str());
    bool gopher = p1.startsWith("gopher");
    char *hostIp;
    char *req;
    int port;
    bool doSSL;
    if(!parseWebUrl((uint8_t *)buf,&hostIp,&req,&port,&doSSL))
    {
      if(!quiet)
        serial.printf("Invalid url: %s",p1.c_str());
      success = false;
    }
    else
    if(gopher)
    {
      if(!doGopherGet(hostIp, port, &SD, p2.c_str(), req, doSSL))
      {
        if(!quiet)
          serial.printf("Wget failed: %s to file %s",p1.c_str(),p2.c_str());
        success = false;
      }
    }
    else
    if(!doWebGet(hostIp, port, &SD, p2.c_str(), req, doSSL))
    {
      if(!quiet)
        serial.printf("Wget failed: %s to file %s",p1.c_str(),p2.c_str());
      success = false;
    }
  }
  return success;
}

#ifdef INCLUDE_FTP

bool ZBrowser::doFGetCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p1=cleanFirstArg(line);
  String p2=cleanRemainArg(line);
  if(p2.length() == 0)
  {
    int slash = p1.lastIndexOf('/');
    if(slash < p1.length()-1)
      p2 = makePath(p1.substring(slash+1));
    else
      p2 = makePath(p1);
  }
  else
    p2=makePath(p2);
  debugPrintf("fget:%s -> %s\r\n",p1.c_str(), p2.c_str());
  char *tmp=0;
  bool isUrl = ((p1.length()>=11)
                && ((strcmp(p1.substring(0,6).c_str(),"ftp://") == 0)
                   || (strcmp(p1.substring(0,7).c_str(),"ftps://") == 0)));
  if((ftpHost==0)&&(!isUrl))
  {
    if(!quiet)
      serial.printf("Url required: %s%s",p1.c_str(),EOLNC);
    success = false;
  }
  else
  if(SD.exists(p2))
  {
    if(!quiet)
      serial.printf("File exists: %s%s",p2.c_str(),EOLNC);
    success = false;
  }
  else
  {
    uint8_t buf[p1.length()+1];
    strcpy((char *)buf,p1.c_str());
    char *req;
    ftpHost = makeFTPHost(isUrl,ftpHost,buf,&req);
    if(req == 0)
    {
      if(!quiet)
        serial.printf("Invalid url: %s",p1.c_str());
      success = false;
    }
    else
    if(!ftpHost->doGet(&SD, p2.c_str(), req))
    {
      if(!quiet)
        serial.printf("Fget failed: %s to file %s",p1.c_str(),p2.c_str());
      success = false;
    }
  }
  return success;
}

bool ZBrowser::doFPutCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p1=makePath(cleanFirstArg(line));
  String p2=cleanRemainArg(line);
  debugPrintf("fput:%s -> %s\r\n",p1.c_str(), p2.c_str());
  char *tmp=0;
  bool isUrl = ((p2.length()>=11)
                && ((strcmp(p2.substring(0,6).c_str(),"ftp://") == 0)
                   || (strcmp(p2.substring(0,7).c_str(),"ftps://") == 0)));
  if((ftpHost==0)&&(!isUrl))
  {
    if(!quiet)
      serial.printf("Url required: %s%s",p2.c_str(),EOLNC);
    success = false;
  }
  else
  if(!SD.exists(p1))
  {
    if(!quiet)
      serial.printf("File not found: %s%s",p1.c_str(),EOLNC);
    success = false;
  }
  else
  {
    uint8_t buf[p2.length()+1];
    strcpy((char *)buf,p2.c_str());
    File file = SD.open(p1);
    char *req;
    ftpHost = makeFTPHost(isUrl,ftpHost,buf,&req);
    if(req == 0)
    {
      if(!quiet)
        serial.printf("Invalid url: %s",p2.c_str());
      success = false;
    }
    else
    if(!file)
    {
      if(!quiet)
        serial.printf("File not found: %s%s",p1.c_str(),EOLNC);
      success = false;
    }
    else
    {
      if(!ftpHost->doPut(file, req))
      {
        if(!quiet)
          serial.printf("Fput failed: %s from file %s",p2.c_str(),p1.c_str());
        success = false;
      }
      file.close();
    }
  }
  return success;
}

bool ZBrowser::doFDirCommand(String &line, bool showShellOutput)
{
  bool success=true;
  String p1=cleanOneArg(line);
  debugPrintf("fls:%s\r\n",p1.c_str());
  char *tmp=0;
  bool isUrl = ((p1.length()>=11)
                && ((strcmp(p1.substring(0,6).c_str(),"ftp://") == 0)
                   || (strcmp(p1.substring(0,7).c_str(),"ftps://") == 0)));
  if((ftpHost==0)&&(!isUrl))
  {
    if(!quiet)
      serial.printf("Url required: %s%s",p1.c_str(),EOLNC);
    success = false;
  }
  else
  {
    uint8_t buf[p1.length()+1];
    strcpy((char *)buf,p1.c_str());
    char *req;
    ftpHost = makeFTPHost(isUrl,ftpHost,buf,&req);
    if(req == 0)
    {
      if(!quiet)
        serial.printf("Invalid url: %s",p1.c_str());
      success = false;
    }
    else
    if(!ftpHost->doLS(&serial, req))
    {
      if(!quiet)
        serial.printf("Fls failed: %s",p1.c_str());
      success = false;
    }
  }
  return success;
}

#endif

bool ZBrowser::doMoveCommand(String &line, bool showShellOutput)
{
  bool success=true;
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
    if((mask.length()>0)&&(isMask(mask)))
      p1=stripDir(p1);
    else
      mask = "";
  }
  debugPrintf("mv:%s(%s) -> %s\r\n",p1.c_str(),mask.c_str(),p2.c_str());
  if((mask.length()==0)||(!isMask(mask)))
  {
    File root = SD.open(p2);
    if(root && root.isDirectory())
    {
      if (!p2.endsWith("/"))
        p2 += "/";
      p2 += stripFilename(p1);
      debugPrintf("mv:%s -> %s\r\n",p1.c_str(),p2.c_str());
    }
    root.close();
    if(p1 == p2)
    {
      if(!quiet)
        serial.printf("File exists: %s%s",p1.c_str(),EOLNC);
      success = false;
    }
    else
    if(SD.exists(p2) && (!overwrite))
    {
      if(!quiet)
        serial.printf("File exists: %s%s",p2.c_str(),EOLNC);
      success = false;
    }
    else
    {
      if(SD.exists(p2))
        SD.remove(p2);
      if(!SD.rename(p1,p2))
      {
        if(!quiet)
          serial.printf("Failed to move: %s%s",p1.c_str(),EOLNC);
        success = false;
      }
    }
  }
  else
  {
    success = copyFiles(p1,mask,p2,false,overwrite);
    if(success)
      success = deleteFile(p1,mask,false);
  }
  return success;
}

bool ZBrowser::doHelpCommand(String &line, bool showShellOutput)
{
  bool success=true;
  if(!quiet)
  {
    serial.printf("Commands:%s",EOLNC);
    serial.printf("ls/dir/list/$ [-r] [/][path]                   - List files%s",EOLNC);
    serial.printf("cd [/][path][..]                               - Change to new directory%s",EOLNC);
    serial.printf("md/mkdir/makedir [/][path]                     - Create a new directory%s",EOLNC);
    serial.printf("rd/rmdir/deletedir [/][path]                   - Delete a directory%s",EOLNC);
    serial.printf("rm/del/delete [-r] [/][path]filename           - Delete a file%s",EOLNC);
    serial.printf("cp/copy [-r] [-f] [/][path]file [/][path]file  - Copy file(s)%s",EOLNC);
    serial.printf("ren/rename [/][path]file [/][path]file         - Rename a file%s",EOLNC);
    serial.printf("mv/move [-f] [/][path]file [/][path]file       - Move file(s)%s",EOLNC);
    serial.printf("cat/type [-h] [/][path]filename                - View a file(s)%s",EOLNC);
    serial.printf("df/free/info                                   - Show space remaining%s",EOLNC);
    serial.printf("xget/zget/kget/pget [/][path]filename          - Download a file%s",EOLNC);
    serial.printf("xput/zput/kput/pput [/][path]filename          - Upload a file%s",EOLNC);
    serial.printf("wget [http://url] [/][path]filename            - Download url to file%s",EOLNC);
#ifdef INCLUDE_FTP
    serial.printf("fget [ftp://user:pass@url/file] [/][path]file  - FTP get file%s",EOLNC);
    serial.printf("fput [/][path]file [ftp://user:pass@url/file]  - FTP put file%s",EOLNC);
    serial.printf("fdir [ftp://user:pass@url/path]                - ftp url dir%s",EOLNC);
#endif
    serial.printf("exit/quit/x/endshell                           - Quit to command mode%s",EOLNC);
    serial.printf("%s",EOLNC);
  }
  return success;
}

bool ZBrowser::doModeCommand(String &line, bool showShellOutput)
{
  char c='?';
  bool success = true;
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
  quiet = !showShellOutput;
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
        quiet = false;
        return success;
      }
      else
      if(cmd.equalsIgnoreCase("ls")||cmd.equalsIgnoreCase("dir")||cmd.equalsIgnoreCase("$")||cmd.equalsIgnoreCase("list"))
        success = doDirCommand(line,showShellOutput);
      else
      if(cmd.equals("?")||cmd.equals("help"))
        success = doHelpCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("mv")||cmd.equalsIgnoreCase("move"))
        success = doMoveCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("rm")||cmd.equalsIgnoreCase("del")||cmd.equalsIgnoreCase("delete"))
        success = doRmCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("cp")||cmd.equalsIgnoreCase("copy"))
        success = doCpCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("df")||cmd.equalsIgnoreCase("free")||cmd.equalsIgnoreCase("info"))
      {
        if(!quiet)
          serial.printf("%llu free of %llu total%s",(SD.totalBytes()-SD.usedBytes()),SD.totalBytes(),EOLNC);
      }
      else
      if(cmd.equalsIgnoreCase("ren")||cmd.equalsIgnoreCase("rename"))
        success = doRenCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("md")||cmd.equalsIgnoreCase("mkdir")||cmd.equalsIgnoreCase("makedir"))
        success = doMkDirCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("cd"))
        success = doCdDirCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("rd")||cmd.equalsIgnoreCase("rmdir")||cmd.equalsIgnoreCase("deletedir"))
        success = doRmDirCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("cat")||cmd.equalsIgnoreCase("type"))
        success = doCatCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("xget"))
        success = doXGetCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("xput"))
        success = doXPutCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("yget"))
        success = doYGetCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("yput"))
        success = doYPutCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("zget")||cmd.equalsIgnoreCase("rz")||cmd.equalsIgnoreCase("rz.exe"))
        success = doZGetCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("zput")||cmd.equalsIgnoreCase("sz"))
        success = doYPutCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("kget")||cmd.equalsIgnoreCase("rk"))
        success = doKGetCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("kput")||cmd.equalsIgnoreCase("sk"))
        success = doKPutCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("pget"))
        success = doPGetCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("pput"))
        success = doPPutCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("wget"))
        success = doWGetCommand(line,showShellOutput);
      else
#ifdef INCLUDE_FTP
      if(cmd.equalsIgnoreCase("fget"))
        success = doFGetCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("fput"))
        success = doFPutCommand(line,showShellOutput);
      else
      if(cmd.equalsIgnoreCase("fls") || cmd.equalsIgnoreCase("fdir"))
        success = doFDirCommand(line,showShellOutput);
      else
#endif
      if(!quiet)
        serial.printf("Unknown command: '%s'.  Try '?'.%s",cmd.c_str(),EOLNC);
      showMenu=true;
      break;
    }
  }
  quiet = !showShellOutput;
  return success;
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
        if(!quiet)
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
  logFileLoop();
}
#else
static void initSDShell()
{}
#endif
