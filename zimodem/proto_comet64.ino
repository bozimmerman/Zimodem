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
Comet64::Comet64(FS *fs, FlowControlType fcType)
{
  cFS = fs;
  cserial.setPetsciiMode(false);
  if(fcType == FCT_RTSCTS)
    cserial.setFlowControlType(FCT_RTSCTS);
  else
    cserial.setFlowControlType(FCT_DISABLED);
}

Comet64::~Comet64()
{
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
  for(int i=0;i<idex;i++)
    inbuf[i] = (uint8_t)petToAsc((char)inbuf[i]);
  char *sp = strchr((char *)inbuf,' ');
  char *args = (sp+1);
  char *cmd = (char *)inbuf;
  if(sp == 0)
    args=(char *)(inbuf-1);
  else
    *sp=0;
  debugPrintf("COMET64 received: '%s' '%s'\n",cmd,args);
  if(strcmp(cmd,"login")==0)
  {
      // ignore login commands?
  }
  else
  if(strcmp(cmd,"$")==0)
  {
      cserial.printf("hi %s\n",args);
      cserial.println("hi");
      cserial.println("hi");
  }
  idex=0; // we are ready for the next packet!
  serialOutDeque();
}
#endif
#endif
