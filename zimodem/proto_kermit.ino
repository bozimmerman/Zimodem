#ifdef INCLUDE_SD_SHELL
/*
 *  K e r m i t  File Transfer Utility
 *
 *  UNIX Kermit, Columbia University, 1981, 1982, 1983
 *  Bill Catchings, Bob Cattani, Chris Maio, Frank da Cruz, Alan Crosswell
 *
 *  Also:   Jim Guyton, Rand Corporation
 *      Walter Underwood, Ford Aerospace
 *      Lauren Weinstein
 *      
 *  Adapted for Zimodem by Bo Zimmerman
 */

KModem::KModem(FlowControlType commandFlow,
               int (*recvChar)(ZSerial *ser, int msDelay),
               void (*sendChar)(ZSerial *ser, char sym),
               bool (*dataHandler)(File *kfp, unsigned long number, char *buffer, int len),
               String &errors)
{
  this->errStr = &errors;
  this->sendChar = sendChar;
  this->recvChar = recvChar;
  this->dataHandler = dataHandler;  
  this->kserial.setFlowControlType(FCT_DISABLED);
  if(commandFlow==FCT_RTSCTS)
    this->kserial.setFlowControlType(FCT_RTSCTS);
  this->kserial.setPetsciiMode(false);
  this->kserial.setXON(true);
}

void KModem::flushinput()
{
  while(kserial.available()>0)
    logSerialIn(kserial.read());
}

bool KModem::receive()
{
  state = 'R';      /* Receive-Init is the start state */
  n = 0;        /* Initialize message number */
  numtry = 0;       /* Say no tries yet */

  while(TRUE)
  {
    if (debug)
      debugPrintf(" recsw state: %c\r\n",state);
    switch(state)     /* Do until done */
    {
    case 'R': 
      state = rinit(); 
      break; /* Receive-Init */
    case 'F': 
      state = rfile(); 
      break; /* Receive-File */
    case 'D': 
      state = rdata(); 
      break; /* Receive-Data */
    case 'C': 
      kserial.flushAlways();
      return true;   /* Complete state */
    case 'A': 
      kserial.flushAlways();
      return false;    /* "Abort" state */
    }
  }
}


bool KModem::transmit()
{
  if (gnxtfl() == FALSE)  /* No more files go? */
  {
    kserial.flushAlways();
    return false;    /* if not, break, EOT, all done */
  }
  state = 'S';      /* Send initiate is the start state */
  n = 0;        /* Initialize message number */
  numtry = 0;       /* Say no tries yet */
  while(ZTRUE)       /* Do this as long as necessary */
  {
    if (debug) 
      debugPrintf("sendsw state: %c\r\n",state);
    switch(state)
    {
    case 'S': 
      state = sinit();  
      break; /* Send-Init */
    case 'F': 
      state = sfile();  
      break; /* Send-File */
    case 'D': 
      state = sdata();  
      break; /* Send-Data */
    case 'Z': 
      state = seof();   
      break; /* Send-End-of-File */
    case 'B': 
      state = sbreak(); 
      break; /* Send-Break */
    case 'C': 
      kserial.flushAlways();
      return true;     /* Complete */
    case 'A': 
      kserial.flushAlways();
      return false;    /* "Abort" */
    default:  
      kserial.flushAlways();
      return false;    /* Unknown, fail */
    }
  }
}

/*
 *  s i n i t
 *
 *  Send Initiate: send this host's parameters and get other side's back.
 */

char KModem::sinit()
{
  int num, len;     /* Packet number, length */

  if (numtry++ > MAXTRY) 
    return('A'); /* If too many tries, give up */
  spar(packet);     /* Fill up init info packet */

  flushinput();     /* Flush pending input */

  spack('S',n,6,packet);    /* Send an S packet */
  switch(rpack(&len,&num,recpkt)) /* What was the reply? */
  {
  case 'N':  
    return(state); /* NAK, try it again */

  case 'Y':     /* ACK */
    if (n != num)   /* If wrong ACK, stay in S state */
      return(state);    /* and try again */
    rpar(recpkt);   /* Get other side's init info */

    if (eol == 0) 
      eol = '\n'; /* Check and set defaults */
    if (quote == 0) 
      quote = '#';

    numtry = 0;     /* Reset try counter */
    n = (n+1)%64;   /* Bump packet count */
    return('F');    /* OK, switch state to F */

  case 'E':     /* Error packet received */
    prerrpkt(recpkt);   /* Print it out and */
    return('A');    /* abort */

  case FALSE: 
    return(state);  /* Receive failure, try again */

  default: 
    return('A');   /* Anything else, just "abort" */
  }
}


/*
 *  s f i l e
 *
 *  Send File Header.
 */

char KModem::sfile()
{
  int num, len;    /* Packet number, length */
  char filnam1[MAX_PATH],/* Converted file name */
       *newfilnam, /* Pointer to file name to send */
       *cp;        /* char pointer */
  if (numtry++ > MAXTRY) 
    return('A'); /* If too many tries, give up */

  if (kfpClosed)     /* If not already open, */
  { 
    if (debug) 
      debugPrintf("   Opening %s for sending.\r\n",filnam);
    kfp = kfileSystem->open(filnam,"r");   /* open the file to be sent */
    if (!kfp)     /* If bad file pointer, give up */
    {
        debugPrintf("Cannot open file %s",filnam);
        return('A');
    }
    kfpClosed=false;
  }

  strcpy(filnam1, filnamo);   /* Copy file name */
  newfilnam = cp = filnam1;
  if (!xflg)
    while (*cp != '\0')   /* Strip off all leading directory */
      if (*cp++ == '/')   /* names (ie. up to the last /). */
        newfilnam = cp;

  len = strlen(newfilnam);    /* Compute length of new filename */

  debugPrintf("Sending %s as %s",filnam,newfilnam);

  spack('F',n,len,newfilnam);   /* Send an F packet */
  switch(rpack(&len,&num,recpkt)) /* What was the reply? */
  {     
  case 'N':     /* NAK, just stay in this state, */
    num = (--num<0 ? 63:num); /* unless it's NAK for next packet */
    if (n != num)   /* which is just like an ACK for */ 
      return(state);    /* this packet so fall thru to... */
  case 'Y':     /* ACK */
    if (n != num) 
      return(state); /* If wrong ACK, stay in F state */
    numtry = 0;     /* Reset try counter */
    n = (n+1)%64;   /* Bump packet count */
    size = bufill(packet);  /* Get first data from file */
    return('D');    /* Switch state to D */
  case 'E':     /* Error packet received */
    prerrpkt(recpkt);   /* Print it out and */
    return('A');    /* abort */
  case FALSE: 
    return(state);  /* Receive failure, stay in F state */
  default:    
    return('A');  /* Something else, just "abort" */
  }
}


/*
 *  s d a t a
 *
 *  Send File Data
 */

char KModem::sdata()
{
  int num, len;     /* Packet number, length */

  if (numtry++ > MAXTRY) 
    return('A'); /* If too many tries, give up */
  spack('D',n,size,packet);   /* Send a D packet */
  switch(rpack(&len,&num,recpkt)) /* What was the reply? */
  {       
  case 'N':     /* NAK, just stay in this state, */
    num = (--num<0 ? 63:num); /* unless it's NAK for next packet */
    if (n != num)   /* which is just like an ACK for */
      return(state);    /* this packet so fall thru to... */
  case 'Y':     /* ACK */
    if (n != num) 
      return(state); /* If wrong ACK, fail */
    numtry = 0;     /* Reset try counter */
    n = (n+1)%64;   /* Bump packet count */
    if ((size = bufill(packet)) == EOF) /* Get data from file */
      return('Z');    /* If EOF set state to that */
    return('D');    /* Got data, stay in state D */
  case 'E':     /* Error packet received */
    prerrpkt(recpkt);   /* Print it out and */
    return('A');    /* abort */
  case FALSE: 
    return(state);  /* Receive failure, stay in D */
  default:    
    return('A');  /* Anything else, "abort" */
  }
}


/*
 *  s e o f
 *
 *  Send End-Of-File.
 */

char KModem::seof()
{
  int num, len;     /* Packet number, length */
  if (numtry++ > MAXTRY) 
    return('A'); /* If too many tries, "abort" */

  spack('Z',n,0,packet);    /* Send a 'Z' packet */
  switch(rpack(&len,&num,recpkt)) /* What was the reply? */
  {
  case 'N':     /* NAK, just stay in this state, */
    num = (--num<0 ? 63:num); /* unless it's NAK for next packet, */
    if (n != num)   /* which is just like an ACK for */
      return(state);    /* this packet so fall thru to... */
  case 'Y':     /* ACK */
    if (n != num) 
      return(state); /* If wrong ACK, hold out */
    numtry = 0;     /* Reset try counter */
    n = (n+1)%64;   /* and bump packet count */
    if (debug) 
      debugPrintf("   Closing input file %s, ",filnam);
    kfp.close();     /* Close the input file */
    kfpClosed = true;      /* Set flag indicating no file open */ 
    if (debug) 
      debugPrintf("looking for next file...\r\n");
    if (gnxtfl() == FALSE)  /* No more files go? */
      return('B');    /* if not, break, EOT, all done */
    if (debug) 
      debugPrintf("   New file is %s\r\n",filnam);
    return('F');    /* More files, switch state to F */
  case 'E':     /* Error packet received */
    prerrpkt(recpkt);   /* Print it out and */
    return('A');    /* abort */
  case FALSE: 
    return(state);  /* Receive failure, stay in Z */
  default:    
    return('A');  /* Something else, "abort" */
  }
}


/*
 *  s b r e a k
 *
 *  Send Break (EOT)
 */

char KModem::sbreak()
{
  int num, len;     /* Packet number, length */
  if (numtry++ > MAXTRY) 
    return('A'); /* If too many tries "abort" */

  spack('B',n,0,packet);    /* Send a B packet */
  switch (rpack(&len,&num,recpkt))  /* What was the reply? */
  {
  case 'N':     /* NAK, just stay in this state, */
    num = (--num<0 ? 63:num); /* unless NAK for previous packet, */
    if (n != num)   /* which is just like an ACK for */
      return(state);    /* this packet so fall thru to... */
  case 'Y':     /* ACK */
    if (n != num) 
      return(state); /* If wrong ACK, fail */
    numtry = 0;     /* Reset try counter */
    n = (n+1)%64;   /* and bump packet count */
    return('C');    /* Switch state to Complete */
  case 'E':     /* Error packet received */
    prerrpkt(recpkt);   /* Print it out and */
    return('A');    /* abort */
  case FALSE: 
    return(state);  /* Receive failure, stay in B */
  default:    
    return ('A'); /* Other, "abort" */
  }
}

/*
 *  r i n i t
 *
 *  Receive Initialization
 */
  
char KModem::rinit()
{
  int len, num;     /* Packet length, number */

  if (numtry++ > MAXTRY) 
    return('A'); /* If too many tries, "abort" */

  char rs=rpack(&len,&num,packet);
  if (debug) 
    debugPrintf(" recsw-rinit state: %c\r\n",rs);
  switch(rs) /* Get a packet */
  {
  case 'S':     /* Send-Init */
    rpar(packet);   /* Get the other side's init data */
    spar(packet);   /* Fill up packet with my init info */
    spack('Y',n,6,packet);  /* ACK with my parameters */
    oldtry = numtry;    /* Save old try count */
    numtry = 0;     /* Start a new counter */
    n = (n+1)%64;   /* Bump packet number, mod 64 */
    return('F');    /* Enter File-Receive state */
  case 'E':     /* Error packet received */
    prerrpkt(recpkt);   /* Print it out and */
    return('A');    /* abort */
  case FALSE:     /* Didn't get packet */
    spack('N',n,0,0);   /* Return a NAK */
    return(state);    /* Keep trying */
  default:
    return('A'); /* Some other packet type, "abort" */
  }
}

/*
 *  r f i l e
 *
 *  Receive File Header
 */

char KModem::rfile()
{
  int num, len;     /* Packet number, length */

  if (numtry++ > MAXTRY) 
    return('A'); /* "abort" if too many tries */

  char rs = rpack(&len,&num,packet); 
  if (debug) 
    debugPrintf(" recsw-rfile state: %c\r\n",rs);
  switch(rs) /* Get a packet */
  {
  case 'S':     /* Send-Init, maybe our ACK lost */
    if (oldtry++ > MAXTRY) 
      return('A'); /* If too many tries abort */
    if (num == ((n==0) ? 63:n-1)) /* Previous packet, mod 64? */
    {       /* Yes, ACK it again with  */
      spar(packet);   /* our Send-Init parameters */
      spack('Y',num,6,packet);
      numtry = 0;   /* Reset try counter */
      return(state);    /* Stay in this state */
    }
    else 
      return('A');   /* Not previous packet, "abort" */
  case 'Z':     /* End-Of-File */
    if (oldtry++ > MAXTRY) 
      return('A');
    if (num == ((n==0) ? 63:n-1)) /* Previous packet, mod 64? */
    {       /* Yes, ACK it again. */
      spack('Y',num,0,0);
      numtry = 0;
      return(state);    /* Stay in this state */
    }
    else 
      return('A');   /* Not previous packet, "abort" */
  case 'F':     /* File Header (just what we want) */
    if (num != n) 
      return('A');  /* The packet number must be right */
    {
      char filnam1[MAX_PATH];     /* Holds the converted file name */
      char *subNam=filnam1;
      if(rootpath.length()>0)
      {
        strcpy(filnam1, rootpath.c_str());
        subNam += rootpath.length();
        if(filnam1[strlen(filnam1)-1]!='/')
        {
          filnam1[strlen(filnam1)]='/';
          filnam1[strlen(filnam1)+1]=0;
          subNam++;
        }
      }
      strcpy(subNam, packet);  /* Copy the file name */
      kfp = kfileSystem->open(filnam1,FILE_WRITE);
      if (!kfp) /* Try to open a new file */
      {
        if(errStr != 0)
          (*errStr) += ("Cannot create %s\n",filnam1); /* Give up if can't */
        return('A');
      }
      else      /* OK, give message */
      {
        debugPrintf("Receiving %s as %s\r\n",packet,filnam1);
        kfpClosed=false;
      }
    }

    spack('Y',n,0,0);   /* Acknowledge the file header */
    oldtry = numtry;    /* Reset try counters */
    numtry = 0;     /* ... */
    n = (n+1)%64;   /* Bump packet number, mod 64 */
    return('D');    /* Switch to Data state */
  case 'B':     /* Break transmission (EOT) */
    if (num != n) 
      return ('A'); /* Need right packet number here */
    spack('Y',n,0,0);   /* Say OK */
    return('C');    /* Go to complete state */
  case 'E':     /* Error packet received */
    prerrpkt(recpkt);   /* Print it out and */
    return('A');    /* abort */
  case FALSE:     /* Didn't get packet */
    spack('N',n,0,0);   /* Return a NAK */
    return(state);    /* Keep trying */
  default:    
    return ('A'); /* Some other packet, "abort" */
  }
}

/*
 *  r d a t a
 *
 *  Receive Data
 */

char KModem::rdata()
{
  int num, len;     /* Packet number, length */
  if (numtry++ > MAXTRY) 
    return('A'); /* "abort" if too many tries */

  char rs=rpack(&len,&num,packet);
  if (debug) 
    debugPrintf(" recsw-rdata state: %c\r\n",rs);
  switch(rs) /* Get packet */
  {
  case 'D':     /* Got Data packet */
    if (num != n)   /* Right packet? */
    {       /* No */
      if (oldtry++ > MAXTRY)
        return('A'); /* If too many tries, abort */
      if (num == ((n==0) ? 63:n-1)) /* Else check packet number */
      {     /* Previous packet again? */
        spack('Y',num,6,packet); /* Yes, re-ACK it */
        numtry = 0;   /* Reset try counter */
        return(state);  /* Don't write out data! */
      }
      else
         return('A'); /* sorry, wrong number */
    }
    /* Got data with right packet number */
    bufemp(packet,len);   /* Write the data to the file */
    spack('Y',n,0,0);   /* Acknowledge the packet */
    oldtry = numtry;    /* Reset the try counters */
    numtry = 0;     /* ... */
    n = (n+1)%64;   /* Bump packet number, mod 64 */
    return('D');    /* Remain in data state */
  case 'F':     /* Got a File Header */
    if (oldtry++ > MAXTRY)
      return('A');    /* If too many tries, "abort" */
    if (num == ((n==0) ? 63:n-1)) /* Else check packet number */
    {       /* It was the previous one */
      spack('Y',num,0,0); /* ACK it again */
      numtry = 0;   /* Reset try counter */
      return(state);    /* Stay in Data state */
    }
    else
      return('A');    /* Not previous packet, "abort" */
  case 'Z':     /* End-Of-File */
    if (num != n) 
      return('A');  /* Must have right packet number */
    spack('Y',n,0,0);   /* OK, ACK it. */
    kfp.close();     /* Close the file */
    kfpClosed=true;
    n = (n+1)%64;   /* Bump packet number */
    return('F');    /* Go back to Receive File state */
  case 'E':     /* Error packet received */
    prerrpkt(recpkt);   /* Print it out and */
    return('A');    /* abort */
  case FALSE:     /* Didn't get packet */
    spack('N',n,0,0);   /* Return a NAK */
    return(state);    /* Keep trying */
  default:
    return('A');    /* Some other packet, "abort" */
  }
}

/*
 *  s p a c k
 *
 *  Send a Packet
 */

int KModem::spack(char type, int num, int len, char *data)
{
  int i;        /* Character loop counter */
  char chksum, buffer[100];   /* Checksum, packet buffer */
  char *bufp;    /* Buffer pointer */

  if (debug>1)      /* Display outgoing packet */
  {
    if (data != NULL)
      data[len] = '\0';   /* Null-terminate data to print it */
    debugPrintf("\n  spack type: %c\r\n",type);
    debugPrintf("   num:  %d\r\n",num);
    debugPrintf("   len:  %d\r\n",len);
    if (data != NULL)
      debugPrintf("      data: \"%s\"\r\n",data);
  }
  
  bufp = buffer;      /* Set up buffer pointer */
  for (i=1; i<=pad; i++) 
    sendChar(&kserial,padchar); /* Issue any padding */

  *bufp++ = SOH;      /* Packet marker, ASCII 1 (SOH) */
  *bufp++ = (len+3+' ');    /* Send the character count */
  chksum  = (len+3+' ');    /* Initialize the checksum */
  *bufp++ = (num+' ');    /* Packet number */
  chksum += (num+' ');    /* Update checksum */
  *bufp++ = type;     /* Packet type */
  chksum += type;     /* Update checksum */

  for (i=0; i<len; i++)   /* Loop for all data characters */
  {
    *bufp++ = data[i];    /* Get a character */
    chksum += data[i];    /* Update checksum */
  }
  chksum = (((chksum&0300) >> 6)+chksum)&077; /* Compute final checksum */
  *bufp++ = (chksum+' ');   /* Put it in the packet */
  if ( mflg ) 
    *bufp++ = eol;    /* MacKermit needs this */
  *bufp = eol;      /* Extra-packet line terminator */
  for(i=0;i<bufp-buffer+1;i++) /* Send the packet */
    sendChar(&kserial,buffer[i]);
}

/*
 *  r p a c k
 *
 *  Read a Packet
 */

int KModem::rpack(int *len, int *num, char *data)
{
  int i, done;  /* Data character number, loop exit */
  char t,       /* Current input character */
       type,    /* Packet type */
       cchksum, /* Our (computed) checksum */
       rchksum; /* Checksum received from other host */

  if ((timint > MAXTIM) || (timint < MINTIM)) 
    timint = MYTIME;
  
  while (t != SOH)      /* Wait for packet header */
  {
    if((t=recvChar(&kserial,timint))<0)
      return('A');
    t &= 0177;      /* Handle parity */
  }

  done = FALSE;     /* Got SOH, init loop */
  while (!done)     /* Loop to get a packet */
  {
    if((t=recvChar(&kserial,timint))<0)
      return('A');
    if (!image) 
      t &= 0177;    /* Handle parity */
    if (t == SOH) 
      continue;   /* Resynchronize if SOH */
    cchksum = t;      /* Start the checksum */
    *len = (t-' ')-3;   /* Character count */

    if((t=recvChar(&kserial,timint))<0)
      return('A');
    if (!image) 
      t &= 0177;    /* Handle parity */
    if (t == SOH) 
      continue;   /* Resynchronize if SOH */
    cchksum = cchksum + t;    /* Update checksum */
    *num = (t-' ');   /* Packet number */

    if((t=recvChar(&kserial,timint))<0)
      return('A');
    if (!image) 
      t &= 0177;    /* Handle parity */
    if (t == SOH) 
      continue;   /* Resynchronize if SOH */
    cchksum = cchksum + t;    /* Update checksum */
    type = t;     /* Packet type */

    for (i=0; i<*len; i++)    /* The data itself, if any */
    {       /* Loop for character count */
      if((t=recvChar(&kserial,timint))<0)
        return('A');
      if (!image) 
        t &= 0177;  /* Handle parity */
      if (t == SOH) 
        continue; /* Resynch if SOH */
      cchksum = cchksum + t;  /* Update checksum */
      data[i] = t;    /* Put it in the data buffer */
    }
    data[*len] = 0;     /* Mark the end of the data */

    if((t=recvChar(&kserial,timint))<0)
      return('A');
    rchksum = (t-' ');    /* Convert to numeric */
    if((t=recvChar(&kserial,timint))<0)
      return('A');
    if (!image) 
      t &= 0177;    /* Handle parity */
    if (t == SOH) 
      continue;   /* Resynchronize if SOH */
    done = TRUE;      /* Got checksum, done */
  }

  if (debug>1)      /* Display incoming packet */
  {
    if (data != NULL)
      data[*len] = '\0';    /* Null-terminate data to print it */
    debugPrintf("\n  rpack type: %c\r\n",type);
    debugPrintf("   num:  %d\r\n",*num);
    debugPrintf("   len:  %d\r\n",*len);
    if (data != NULL)
      debugPrintf("      data: \"%s\"\r\n",data);
  }
          /* Fold in bits 7,8 to compute */
  cchksum = (((cchksum&0300) >> 6)+cchksum)&077; /* final checksum */

  if (cchksum != rchksum) 
    return(FALSE);

  return(type);     /* All OK, return packet type */
}

/*
 *  b u f i l l
 *
 *  Get a bufferful of data from the file that's being sent.
 *  Only control-quoting is done; 8-bit & repeat count prefixes are
 *  not handled.
 */

int KModem::bufill(char buffer[])
{
  int i;        /* Loop index */
  char t;        /* Char read from file */
  char t7;      /* 7-bit version of above */

  i = 0;        /* Init data buffer pointer */
  while(dataHandler(&kfp,0,&t,1))  /* Get the next character */
  {
    t7 = t & 0177;      /* Get low order 7 bits */
    if (t7 < SP || t7==DEL || t7==quote) /* Does this char require */
    {           /* special handling? */
      if (t=='\n' && !image)
      {       /* Do LF->CRLF mapping if !image */
        buffer[i++] = quote;
        buffer[i++] = '\r' ^ 64;
      }
      buffer[i++] = quote;  /* Quote the character */
      if (t7 != quote)
      {
        t = (t ^ 64);   /* and uncontrolify */
        t7 = (t7 ^ 64);
      }
    }
    if (image)
      buffer[i++] = t;    /* Deposit the character itself */
    else
      buffer[i++] = t7;
    if (i >= spsiz-8) 
      return(i);  /* Check length */
  }
  if (i==0) 
    return(EOF);    /* Wind up here only on EOF */
  return(i);        /* Handle partial buffer */
}

void KModem::bufemp(char buffer[], int len)
{
  int i;        /* Counter */
  char t;       /* Character holder */

  for (i=0; i<len; i++)   /* Loop thru the data field */
  {
    t = buffer[i];      /* Get character */
    if (t == MYQUOTE)   /* Control quote? */
    {       /* Yes */
      t = buffer[++i];    /* Get the quoted character */
      if ((t & 0177) != MYQUOTE)  /* Low order bits match quote char? */
        t = t ^ 64;   /* No, uncontrollify it */
    }
    if (t==CR && !image)    /* Don't pass CR if in image mode */
      continue;
    dataHandler(&kfp,0,&t,1);
  }
}

int KModem::gnxtfl()
{
  if (filecount-- == 0) 
    return FALSE; /* If no more, fail */
  filnam = (char *)filelist[filenum++]->c_str();
  filnamo = filnam;
  if (debug) 
    debugPrintf("   gnxtfl: filelist = \"%s\"\r\n",filnam);
  return TRUE;      /* else succeed */
}

void KModem::spar(char data[])
{
    data[0] = (MAXPACKSIZ + ' ');    /* Biggest packet I can receive */
    data[1] = (MYTIME + ' ');   /* When I want to be timed out */
    data[2] = (MYPAD + ' ');    /* How much padding I need */
    data[3] = (MYPCHAR  ^ 64);   /* Padding character I want */
    data[4] = (MYEOL + ' ');    /* End-Of-Line character I want */
    data[5] = MYQUOTE;      /* Control-Quote character I send */
}


void KModem::setTransmitList(String **fileList, int numFiles)
{
  filelist = fileList;
  filecount = numFiles;
  filenum = 0;
}


/*  r p a r
 *
 *  Get the other host's send-init parameters
 *
 */

void KModem::rpar(char data[])
{
    spsiz = (data[0]- ' ');    /* Maximum send packet size */
    timint = (data[1]- ' ');   /* When I should time out */
    pad = (data[2]- ' ');    /* Number of pads to send */
    padchar = (data[3] ^ 64);   /* Padding character to send */
    eol = (data[4]- ' ');    /* EOL character I must send */
    quote = data[5];      /* Incoming data quote character */
}

/*
 *  p r e r r p k t
 *
 *  Print contents of error packet received from remote host.
 */
void KModem::prerrpkt(char *msg)
{
  if(errStr != 0)
  {
    (*errStr)+=msg;
    debugPrintf("kermit: %s\r\n",msg);
  }
}


#endif
