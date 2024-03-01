#ifdef INCLUDE_SD_SHELL
#ifdef INCLUDE_COMET64
/*
   Copyright 2024-2024 Bo Zimmerman

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
/* Reverse engineered by Bo Zimmerman */
Comet64::Comet64(SDFS *fs, FlowControlType fcType)
{
  cFS = fs;
  cserial.setPetsciiMode(false);
  if(fcType == FCT_RTSCTS)
    cserial.setFlowControlType(FCT_RTSCTS);
  else
    cserial.setFlowControlType(FCT_DISABLED);
  browseMode.init();
  browsePetscii = browseMode.serial.isPetsciiMode();
  browseMode.serial.setPetsciiMode(true);
}

Comet64::~Comet64()
{
  browseMode.serial.setPetsciiMode(browsePetscii);
  //closeAllFiles();
}

void Comet64::checkDoPlusPlusPlus(const int c, const unsigned long tm)
{
  if(c == '+')
  {
      if((plussesInARow>0)||((tm-lastNonPlusTm)>800))
      {
        plussesInARow++;
        if(plussesInARow > 2)
          plusTimeExpire = tm + 800;
      }
  }
  else
  {
    plusTimeExpire = 0;
    lastNonPlusTm = tm;
    plussesInARow = 0;
  }
}

bool Comet64::checkPlusPlusPlusExpire(const unsigned long tm)
{
  if(aborted)
    return true;
  if((plusTimeExpire>0)&&(tm>plusTimeExpire)&&(plussesInARow>2))
  {
    aborted = true;
    plusTimeExpire = 0;
    lastNonPlusTm = tm;
    plussesInARow = 0;
    return true;
  }
  return false;
}

bool Comet64::isAborted()
{
  return aborted;
}

void Comet64::printDiskHeader(const char* name)
{
  int i=0;
  cserial.print("\"");
  for(;i<16 && name[i] != 0;i++)
    cserial.printf("%c",ascToPetcii(name[i]));
  for(;i<16;i++)
    cserial.print(" ");
  cserial.print("\"");
}

void Comet64::printFilename(const char* name)
{
  int i=0;
  cserial.print("\"");
  for(;i<16 && name[i] != 0;i++)
    cserial.printf("%c",ascToPetcii(name[i]));
  cserial.print("\"");
  for(;i<16;i++)
    cserial.print(" ");
}

String Comet64::getNormalExistingFilename(String name)
{
  File f = cFS->open(name,FILE_READ);
  if(f)
  {
    bool isDir = f.isDirectory();
    f.close();
    if(!isDir)
      return name;
  }
  if((!name.endsWith(".prg"))
  &&(!name.endsWith(".PRG"))
  &&(!name.endsWith(".seq"))
  &&(!name.endsWith(".SEQ")))
  {
    String chk;
    chk = getNormalExistingFilename(name + ".prg");
    if(chk.length() > 0)
      return chk;
    chk = getNormalExistingFilename(name + ".PRG");
    if(chk.length() > 0)
      return chk;
    chk = getNormalExistingFilename(name + ".seq");
    if(chk.length() > 0)
      return chk;
    chk = getNormalExistingFilename(name + ".SEQ");
    if(chk.length() > 0)
      return chk;
  }
  return "";
}

void Comet64::printPetscii(const char* name)
{
  int i=0;
  for(int i=0;name[i] != 0;i++)
    cserial.printf("%c",ascToPetcii(name[i]));
}

void Comet64::receiveLoop()
{
  serialOutDeque();
  unsigned long tm = millis();
  if(checkPlusPlusPlusExpire(tm))
    return;
  int c;
  while(cserial.available() > 0)
  {
    c=cserial.read();
    if(idex<COM64_BUFSIZ)
    {
      if((idex>0)||(c!=' '))
        inbuf[idex++]=c;
    }
    checkDoPlusPlusPlus(c, tm);
    if(checkPlusPlusPlusExpire(tm))
      return;
    yield();
    if(c==13)
    {
      inbuf[idex-1]=c; // we do this to signal the rest of the method
      break;
    }
  }

  if((idex==0)
  ||(inbuf[idex-1]!=13))
  {
    serialOutDeque();
    return;
  }
  inbuf[--idex]=0;
  while((idex>0)&&(inbuf[idex-1]==' '))
    idex--;
  if(idex==0)
  {
    serialOutDeque();
    return;
  }

  //since de-petsciify breaks save, and we needs lb,hb in the args. :(
  char *sp = strchr((char *)inbuf,' ');
  char *args = (sp+1);
  char *cmd = (char *)inbuf;
  if(sp == 0)
    args=(char *)(inbuf-1);
  else
    *sp=0;
  uint8_t lbhb[2];
  char *lbhbc = strchr((char *)args,',');
  if(lbhbc > args)
  {
    lbhb[0] = lbhbc[1];
    lbhb[1] = lbhbc[2];
  }


  // ok, for most commands, petscii translation helps...
  for(int i=0;i<idex;i++)
    if(inbuf[i]!=0)
      inbuf[i] = (uint8_t)petToAsc((char)inbuf[i]);
  for(int i=0;cmd[i]!=0;i++)
    cmd[i]=(uint8_t)toupper((char)cmd[i]);
  debugPrintf("COMET64 received: '%s' '%s'\r\n",cmd,args);
  if((strcmp(cmd,"LOGIN")==0)
  ||(strcmp(cmd,"LOGOUT")==0)
  ||(strcmp(cmd,"QUIT")==0))
  {
    // ignore login commands?
      printPetscii("00 - ok\r\n");
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"HELP")==0)
  {
      printPetscii("00 - ok\r\n");
      printPetscii("There is no help for you.\r\n");
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"DISKS")==0)
  {
      printPetscii("00 - ok\r\n");
      //TODO: return list of disks
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"INSERT")==0)
  {
      //TODO: insert a disk image
      printPetscii("00 - ok\r\n");
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"NEW")==0)
  {
      //TODO: create a disk image
      printPetscii("00 - ok\r\n");
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"REMOVE")==0)
  {
      //TODO: un-insert a disk image
      printPetscii("00 - ok\r\n");
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"NAME")==0)
  {
      printPetscii("00 - ok\r\n");
      //TODO: name of a disk image
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"BLOCKS")==0)
  {
      printPetscii("00 - ok\r\n");
      //TODO: blocks of a disk image
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"GETSECTOR")==0)
  {
      //TODO: blocks of a disk image
      printPetscii("00 - ok\r\n");
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"PUTSECTOR")==0)
  {
      //TODO: blocks of a disk image
      printPetscii("00 - ok\r\n");
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"CF")==0)
  {
      String cmd = "cd ";
      cmd += args;
      if(browseMode.doModeCommand(cmd,false))
        printPetscii("00 - ok\r\n");
      else
        printPetscii("?500 - operation failed\r\n");
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"MF")==0)
  {
      String cmd = "md ";
      cmd += args;
      if(browseMode.doModeCommand(cmd,false))
        printPetscii("00 - ok\r\n");
      else
        printPetscii("?500 - operation failed\r\n");
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"SCRATCH")==0)
  {
      String cmd = "rm ";
      String p = args;
      String chk = getNormalExistingFilename(p);
      if(p.length()==0)
        printPetscii("?62 - file not found\r\n");
      else
      if(browseMode.doModeCommand(cmd+p,false))
        printPetscii("00 - ok\r\n");
      else
        printPetscii("?500 - operation failed\r\n");
      cserial.write(4);
  }
  else
  if(strcmp(cmd,"LOAD")==0)
  {
      String p = browseMode.makePath(browseMode.cleanOneArg(args));
      p = getNormalExistingFilename(p);
      if(p.length()>0)
      {
        File f=SD.open(p, FILE_READ);
        unsigned char hb = round(floor(f.size()/256));
        unsigned char lb = (f.size()-(hb*256));
        cserial.write(lb);
        cserial.write(hb);
        for(int i=0;i<f.size();i++)
          cserial.write(f.read());
        f.close();
      }
      else
      {
        printPetscii("?62 - file not found\r\n");
        cserial.write(4);
      }
  }
  else
  if(strcmp(cmd,"SAVE")==0)
  {
    char *cdex = strchr(args,',');
    if(cdex != 0)
    {
      *cdex=0;
      size_t bytes = lbhb[0] + (256 * lbhb[1]);
      String p = browseMode.makePath(browseMode.cleanOneArg(args));
      File root = SD.open(p);
      if(!root || !root.isDirectory())
      {
        if(root)
         root.close();
        printPetscii("00 - send file "); // authorize the transfer
        cserial.printf("%d ",bytes);
        printPetscii("bytes\r\n"); // authorize the transfer
        cserial.write(4);
        cserial.flush();
        yield();
        File f=SD.open(p, FILE_WRITE);
        unsigned long last=millis();
        bool fail=false;
        for(size_t i=0;i<bytes;)
        {
          yield();
          if(cserial.available()>0)
          {
            int c = cserial.read();
            f.write(c);
            last=millis();
            i++;
          }
          else
          if((millis()-last)>3000)
          {
            fail=true;
            break;
          }
        }
        f.close();
        if(fail)
        {
          cFS->remove(p);
          printPetscii("?25 - read error\r\n");
        }
        else
          printPetscii("00 - ok\r\n");
      }
      else
        printPetscii("?63 - file exists\r\n");
    }
    else
      printPetscii("?500 - internal error\r\n");
    cserial.write(4);
  }
  else
  if((strcmp(cmd,"$")==0)
  ||(strcmp(cmd,"FIND")==0))
  {
    // header line
    bool find = (strcmp(cmd,"FIND")==0);
    if(find)
    {
      char *comma = strchr(args,',');
      if(comma != 0)
        *comma = 0;
    }
    cserial.printf("0 %c",18); // rvs on
    char *lastSlash = strrchr(browseMode.path.c_str(),'/');
    if(lastSlash == 0)
      lastSlash = (char *)browseMode.path.c_str();
    else
      lastSlash++;
    printDiskHeader((const char *)lastSlash); // prints the dir name in petscii
    cserial.printf("    2%c%c\r\n",193,146); //2A[rvs off]
    File root = cFS->open(browseMode.path.c_str());
    if(root && root.isDirectory())
    {
      File file = root.openNextFile();
      while(file)
      {
        if((!find)||(browseMode.matches(file.name(),args)))
        {
          String fname = file.name();
          String ext = " prg";
          String blks;
          if(file.isDirectory())
          {
            ext = " dir";
            blks="0   ";
          }
          else
          {
            if(fname.endsWith(".prg")||fname.endsWith(".PRG"))
              fname = fname.substring(0,fname.length()-4);
            else
            if(fname.endsWith(".seq")||fname.endsWith(".SEQ"))
            {
              fname = fname.substring(0,fname.length()-4);
              ext = " seq";
            }
            int numBlocks = round(ceil(file.size()/254.0));
            blks = numBlocks;
            while(blks.length()<4)
              blks += " ";
          }
          cserial.printf(blks.c_str());
          printFilename(fname.c_str());
          printPetscii(ext.c_str());
          cserial.printf("\r\n");
        }
        file = root.openNextFile();
      }
    }
    uint64_t free = cFS->totalBytes() - cFS->usedBytes();
    uint64_t adjFree= round(ceil(free/254.0));
    int blksFree = (adjFree < 65535) ? adjFree : 65535;
    cserial.printf("%u",blksFree);
    printPetscii(" blocks free.\r\n");
    cserial.write(4);
  }
  else
  {
    printPetscii("?500 - unknown command\r\n");
    cserial.write(4);
  }
  idex=0; // we are ready for the next packet!
  serialOutDeque();
}
#endif
#endif
