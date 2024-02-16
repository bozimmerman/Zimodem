/* zmodem.c */

/* Synchronet ZMODEM Functions */

/* $Id: zmodem.c,v 1.120 2018/02/01 08:20:19 deuce Exp $ */

/******************************************************************************/
/* Project : Unite!       File : zmodem general        Version : 1.02         */
/*                                                                            */
/* (C) Mattheij Computer Service 1994                                         */
/*
 *  Date: Thu, 19 Nov 2015 10:10:02 +0100
 *  From: Jacques Mattheij
 *  Subject: Re: zmodem license
 *  To: Stephen Hurd, Fernando Toledo
 *  CC: Rob Swindell
 *
 *  Hello there to all of you,
 *
 *  So, this email will then signify as the transfer of any and all rights I
 *  held up to this point with relation to the copyright of the zmodem
 *  package as released by me many years ago and all associated files to
 *  Stephen Hurd. Fernando Toledo and Rob Swindell are named as
 *  witnesses to this transfer.
 *
 *  ...
 *
 *  best regards,
 *
 *  Jacques Mattheij
 ******************************************************************************/

#ifdef INCLUDE_SD_SHELL

/*
 * zmodem primitives and other code common to zmtx and zmrx
 */
#define ENDOFFRAME  2
#define FRAMEOK   1
#define TIMEOUT     -1  /* rx routine did not receive a character within timeout */
#define INVHDR      -2  /* invalid header received; but within timeout */
#define ABORTED     -3  /* Aborted *or* disconnected */
#define SUBPKTOVERFLOW  -4  /* Subpacket received more than block length */
#define CRCFAILED   -5  /* Failed CRC comparison */
#define INVALIDSUBPKT -6  /* Invalid Subpacket Type */
#define ZDLEESC 0x8000  /* one of ZCRCE; ZCRCG; ZCRCQ or ZCRCW was received; ZDLE escaped */

#define BADSUBPKT 0x80

#define HDRLEN     5  /* size of a zmodem header */

ZModem::~ZModem()
{
  if(zm != 0)
    free(zm);
  zm = 0;
}
ZModem::ZModem(FS *zfs, void* cbdata)
{
  zfileSystem=zfs;
  zm = (zmodem_t *)malloc(sizeof(zmodem_t));
  memset(zm,0,sizeof(zmodem_t));
  int log_level=LOG_DEBUG;
  zm->log_level=&log_level;
  zm->recv_bufsize     = (ulong)1024;
  zm->no_streaming     = ZFALSE;
  zm->want_fcs_16      =ZFALSE;
  zm->escape_telnet_iac  = ZTRUE;
  zm->escape_8th_bit   = ZFALSE;
  zm->escape_ctrl_chars  = ZFALSE;

  /* Use sane default values */
  zm->init_timeout=10;    /* seconds */
  zm->send_timeout=10;    /* seconds (reduced from 15) */
  zm->recv_timeout=10;    /* seconds (reduced from 20) */
  zm->crc_timeout=120;    /* seconds */
  zm->block_size=ZBLOCKLEN;
  zm->max_block_size=ZBLOCKLEN;
  zm->max_errors=9;

  zm->cbdata=cbdata;
}

#if 0
  int ZModem::lputs(void* unused, int level, const char* str)
  {
    debugPrintf("%s\r\n",str);
    return ZTRUE;
  }
  
  int ZModem::lprintf(int level, const char *fmt, ...)
  {
    char sbuf[1024];
    va_list argptr;
  
    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
    sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(lputs(NULL,level,sbuf));
  }
#else
  int ZModem::lputs(void *unused, int level, const char * str) {}
  int ZModem::lprintf(int level, const char *fmt, ...) {}

  #define lprintf //lprintf
#endif

char* ZModem::getfname(const char* path)
{
  const char* fname;
  const char* bslash;

  fname=strrchr(path,'/');
  bslash=strrchr(path,'\\');
  if(bslash>fname)
    fname=bslash;
  if(fname!=NULL)
    fname++;
  else
    fname=(char*)path;
  return((char*)fname);
}

BOOL ZModem::is_connected()
{
  return(ZTRUE);
}

BOOL ZModem::is_cancelled()
{
  return(zm->cancelled);
}

int ZModem::data_waiting(unsigned timeout)
{
  timeout *= 1000;
  unsigned long startTime = millis();
  while(zserial.available()==0)
  {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - startTime;
    if(elapsedTime >= timeout)
      return ZFALSE;
    delay(1);
  }
  return ZTRUE;
}

/*
 * show the progress of the transfer like this:
 * zmtx: sending file "garbage" 4096 bytes ( 20%)
 */
void ZModem::progress(void* cbdata, int64_t current_pos)
{
  //debugPrintf("POGRESS %lld\r\n",current_pos);
  // do nothing?
}

int ZModem::send_byte(void* unused, uchar ch, unsigned timeout)
{
  lprintf(LOG_DEBUG, "Send: %d", ch);
  zserial.printb(ch);
  //zserial.flush(); // safe flush
  return(0);
}

int ZModem::recv_byte(void* unused, unsigned timeout /* seconds */)
{
  unsigned long startTime = millis();
  timeout *= 1000;
  while(zserial.available()==0)
  {
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - startTime;
    if(elapsedTime >= timeout)
      return(NOINP);
    delay(1);
    yield();
  }
  int ch = zserial.read();
  lprintf(LOG_DEBUG, "Recvd: %d", ch);
  return ch;
}

static char *chr(int ch)
{
  static char str[25];

  switch(ch) {
    case TIMEOUT:     return("TIMEOUT");
    case ABORTED:     return("ABORTED");
    case SUBPKTOVERFLOW:  return "Subpacket Overflow";
    case CRCFAILED:     return "CRC Failure";
    case INVALIDSUBPKT:   return "Invalid Subpacket";
    case ZRQINIT:     return("ZRQINIT");
    case ZRINIT:      return("ZRINIT");
    case ZSINIT:      return("ZSINIT");
    case ZACK:        return("ZACK");
    case ZFILE:       return("ZFILE");
    case ZSKIP:       return("ZSKIP");
    case ZCRC:        return("ZCRC");
    case ZNAK:        return("ZNAK");
    case ZABORT:      return("ZABORT");
    case ZFIN:        return("ZFIN");
    case ZRPOS:       return("ZRPOS");
    case ZDATA:       return("ZDATA");
    case ZEOF:        return("ZEOF");
    case ZFERR:       return("ZFERR");
    case ZPAD:        return("ZPAD");
    case ZCAN:        return("ZCAN");
    case ZDLE:        return("ZDLE");
    case ZDLEE:       return("ZDLEE");
    case ZBIN:        return("ZBIN");
    case ZHEX:        return("ZHEX");
    case ZBIN32:      return("ZBIN32");
    case ZRESC:       return("ZRESC");
    case ZCRCE:       return("ZCRCE");
    case ZCRCG:       return("ZCRCG");
    case ZCRCQ:       return("ZCRCQ");
    case ZCRCW:       return("ZCRCW");

  }
  if(ch<0)
    sprintf(str,"%d",ch);
  else if(ch>=' ' && ch<='~')
    sprintf(str,"'%c' (%02Xh)",(uchar)ch,(uchar)ch);
  else
    sprintf(str,"%u (%02Xh)",(uchar)ch,(uchar)ch);
  return(str); 
}

static char* frame_desc(int frame)
{
  static char str[25];

  if(frame==TIMEOUT)
    return "TIMEOUT";

  if(frame==INVHDR)
    return "Invalid Header";

  if(frame==ABORTED)
    return "Aborted";

  if(frame >= 0 && (frame&BADSUBPKT)) {
    strcpy(str,"BAD ");
    switch(frame&~BADSUBPKT) {
      case ZRQINIT:   strcat(str,"ZRQINIT");    break;
      case ZRINIT:    strcat(str,"ZRINIT");   break;
      case ZSINIT:    strcat(str,"ZSINIT");   break;
      case ZACK:      strcat(str,"ZACK");     break;
      case ZFILE:     strcat(str,"ZFILE");    break;
      case ZSKIP:     strcat(str,"ZSKIP");    break;
      case ZNAK:      strcat(str,"ZNAK");     break;
      case ZABORT:    strcat(str,"ZABORT");   break;
      case ZFIN:      strcat(str,"ZFIN");     break;
      case ZRPOS:     strcat(str,"ZRPOS");    break;
      case ZDATA:     strcat(str,"ZDATA");    break;
      case ZEOF:      strcat(str,"ZEOF");     break;
      case ZFERR:     strcat(str,"ZFERR");    break;
      case ZCRC:      strcat(str,"ZCRC");     break;
      case ZCHALLENGE:  strcat(str,"ZCHALLENGE"); break;
      case ZCOMPL:    strcat(str,"ZCOMPL");   break;
      case ZCAN:      strcat(str,"ZCAN");     break;
      case ZFREECNT:    strcat(str,"ZFREECNT");   break;
      case ZCOMMAND:    strcat(str,"ZCOMMAND");   break;  
      case ZSTDERR:   strcat(str,"ZSTDERR");    break;    
      default: 
        sprintf(str,"Unknown (%08X)", frame);
        break;
    }
  } else
    sprintf(str,"%d",frame);
  return(str); 
}

ulong ZModem::frame_pos(int type)
{
  switch(type) {
    case ZRPOS:
    case ZACK:
    case ZEOF:
    case ZDATA:
      return(zm->rxd_header_pos);
  }

  return 0;
}

/*
 * read bytes as long as rdchk indicates that
 * more data is available.
 */

void ZModem::recv_purge()
{
  while(recv_byte(zm->cbdata,0)>=0);
}

/* 
 * Flush the output buffer
 */
void ZModem::flush()
{
  zserial.flush();
}

uint32_t ucrc32(unsigned char p, uint32_t crc)
{
  int c;
  crc ^= p;
  for(int c = 8; c-- > 0; )
    crc = (crc >> 1) ^ (0xEDB88320UL & ~((crc & 1l)-1l));
  return crc;
}

uint32_t ucrc32(char *buf, int size)
{
  uint32_t crc = 0xffffffffL;
  while (--size >= 0) {
    crc = ucrc32(*buf++, crc);
  }
  return crc;
}

uint16_t ucrc16(unsigned char p, uint16_t crc)
{
  int i;
  crc ^= (uint16_t) p << 8;
  for (i = 0; i < 8; i++)
    if (crc & 0x8000)
      crc = crc << 1 ^ 0x1021;
    else
      crc <<= 1;
  return crc;
}

uint16_t ucrc16(char *buf, int size)
{
  uint16_t crc = 0;
  while (--size >= 0) {
    crc = ucrc16(*buf++, crc);
  }
  return crc;
}

uint32_t fcrc32(File* fp, unsigned long len)
{
  int ch;
  uint32_t crc=0xffffffff;
  unsigned long l;

  fp->seek(0);
  for(l=0;(len==0 || l<len) && (fp->available()>0);l++) {
    if((ch=fp->read())<0)
      break;
    crc=ucrc32(ch,crc);
  }
  return(~crc);
}

/* 
 * transmit a character. 
 * this is the raw modem interface
 */
/* Returns 0 on success */
int ZModem::send_raw(unsigned char ch)
{
  int result;

  if((result=send_byte(zm->cbdata,ch,zm->send_timeout))!=0)
  {
    lprintf(LOG_ERR,"send_raw SEND ERROR: %d",result);
  }

  zm->last_sent = ch;

  return result;
}

/*
 * transmit a character ZDLE escaped
 */
int ZModem::send_esc(unsigned char c)
{
  int result;

  if((result=send_raw( ZDLE))!=0)
    return(result);
  /*
   * exclusive or; not an or so ZDLE becomes ZDLEE
   */
  return send_raw( (uchar)(c ^ 0x40));
}

/*
 * transmit a character; ZDLE escaping if appropriate
 */
int ZModem::tx(unsigned char c)
{
  int result;

  switch (c) {
    case ZMO_DLE:
    case ZMO_DLE|0x80:          /* even if high-bit set */
    case ZMO_XON:
    case ZMO_XON|0x80:
    case ZMO_XOFF:
    case ZMO_XOFF|0x80:
    case ZDLE:
      return send_esc( c);
    case 0x0d:
    case 0x0d|0x80:
      if(zm->escape_ctrl_chars && (zm->last_sent&0x7f) == '@')
        return send_esc( c);
      break;
    case TELNET_IAC:
      if(zm->escape_telnet_iac) {
        if((result=send_raw( ZDLE))!=0)
          return(result);
        return send_raw( ZRUB1);
      }
      break;
    default:
      if(zm->escape_ctrl_chars && (c&0x60)==0)
        return send_esc( c);
      break;
  }
  /*
   * anything that ends here is so normal we might as well transmit it.
   */
  return send_raw( c);
}

/**********************************************/
/* Output single byte as two hex ASCII digits */
/**********************************************/
int ZModem::send_hex(uchar val)
{
  char* xdigit="0123456789abcdef";
  int result;

  lprintf(LOG_DEBUG,"send_hex: %02X ",val);

  if((result=send_raw( xdigit[val>>4]))!=0)
    return result;
  return send_raw( xdigit[val&0xf]);
}

int ZModem::send_padded_zdle()
{
  int result;
  delay(100); //dammit, this might fix something.
  if((result=send_raw( ZPAD))!=0)
    return result;
  if((result=send_raw( ZPAD))!=0)
    return result;
  return send_raw( ZDLE);
}

/* 
 * transmit a hex header.
 * these routines use tx_raw because we're sure that all the
 * characters are not to be escaped.
 */
int ZModem::send_hex_header(unsigned char * p)
{
  int i;
  int result;
  uchar type=*p;
  unsigned short int crc;

  lprintf(LOG_DEBUG,"send_hex_header: %s", chr(type));

  if((result=send_padded_zdle())!=0)
    return result;

  if((result=send_raw( ZHEX))!=0)
    return result;

  /*
   * initialise the crc
   */

  crc = 0;

  /*
   * transmit the header
   */

  for(i=0;i<HDRLEN;i++) {
    if((result=send_hex( *p))!=0)
      return result;
    crc = ucrc16(*p, crc);
    p++;
  }

  /*
   * update the crc as though it were zero
   */

  /* 
   * transmit the crc
   */

  if((result=send_hex( (uchar)(crc>>8)))!=0)
    return result;
  if((result=send_hex( (uchar)(crc&0xff)))!=0)
    return result;

  /*
   * end of line sequence
   */

  if((result=send_raw( '\r'))!=0)
    return result;
  if((result=send_raw( '\n'))!=0) /* FDSZ sends 0x8a instead of 0x0a */
    return result;

  if(type!=ZACK && type!=ZFIN)
    result=send_raw( ZMO_XON); //TODO:BZ:SUSPICIOUS

  flush();

  return(result);
}

/*
 * Send ZMODEM binary header hdr
 */
int ZModem::send_bin32_header(unsigned char * p)
{
  int i;
  int result;
  uint32_t crc;

  lprintf(LOG_DEBUG,"send_bin32_header: %s", chr(*p));

  if((result=send_padded_zdle())!=0)
    return result;

  if((result=send_raw( ZBIN32))!=0)
    return result;

  crc = 0xffffffffL;

  for(i=0;i<HDRLEN;i++) {
    crc = ucrc32(*p,crc);
    if((result=tx( *p++))!=0)
      return result;
  }

  crc = ~crc;

  if((result= tx( (uchar)((crc      ) & 0xff)))!=0)
    return result;
  if((result= tx( (uchar)((crc >>  8) & 0xff)))!=0)
    return result;
  if((result= tx( (uchar)((crc >> 16) & 0xff)))!=0)
    return result;
  return    tx( (uchar)((crc >> 24) & 0xff));
}

int ZModem::send_bin16_header(unsigned char * p)
{
  int i;
  int result;
  unsigned int crc;

  lprintf(LOG_DEBUG,"send_bin16_header: %s", chr(*p));

  if((result=send_padded_zdle())!=0)
    return result;

  if((result=send_raw( ZBIN))!=0)
    return result;

  crc = 0;

  for(i=0;i<HDRLEN;i++) {
    crc = ucrc16(*p,crc);
    if((result=tx( *p++))!=0)
      return result;
  }

  if((result= tx( (uchar)(crc >> 8)))!=0)
    return result;
  return    tx( (uchar)(crc&0xff));
}


/* 
 * transmit a header using either hex 16 bit crc or binary 32 bit crc
 * depending on the receivers capabilities
 * we dont bother with variable length headers. I dont really see their
 * advantage and they would clutter the code unneccesarily
 */
int ZModem::send_bin_header(unsigned char * p)
{
  if(zm->can_fcs_32 && !zm->want_fcs_16)
    return send_bin32_header( p);
  return send_bin16_header( p);
}

/*
 * data subpacket transmission
 */
int ZModem::send_data32(uchar subpkt_type, unsigned char * p, size_t l)
{
  int result;
  uint32_t crc;

  lprintf(LOG_DEBUG,"send_data32: %s (%u bytes)", chr(subpkt_type), l);

  crc = 0xffffffffl;

  while(l > 0) {
    crc = ucrc32(*p,crc);
    if((result=tx( *p++))!=0)
      return result;
    l--;
  }

  crc = ucrc32(subpkt_type, crc);

  if((result=send_raw( ZDLE))!=0)
    return result;
  if((result=send_raw( subpkt_type))!=0)
    return result;

  crc = ~crc;

  if((result= tx( (uchar) ((crc      ) & 0xff)))!=0)
    return result;
  if((result= tx( (uchar) ((crc >> 8 ) & 0xff)))!=0)
    return result;
  if((result= tx( (uchar) ((crc >> 16) & 0xff)))!=0)
    return result;
  return    tx( (uchar) ((crc >> 24) & 0xff));
}

int ZModem::send_data16(uchar subpkt_type,unsigned char * p, size_t l)
{
  int result;
  unsigned short crc;

  lprintf(LOG_DEBUG,"send_data16: %s (%u bytes)", chr(subpkt_type), l);

  crc = 0;

  while(l > 0) {
    crc = ucrc16(*p,crc);
    if((result=tx( *p++))!=0)
      return result;
    l--;
  }

  crc = ucrc16(subpkt_type,crc);

  if((result=send_raw( ZDLE))!=0)
    return result;
  if((result=send_raw( subpkt_type))!=0)
    return result;
  
  if((result= tx( (uchar)(crc >> 8)))!=0)
    return result;
  return    tx( (uchar)(crc&0xff));
}

/*
 * send a data subpacket using crc 16 or crc 32 as desired by the receiver
 */
int ZModem::send_data_subpkt(uchar subpkt_type, unsigned char * p, size_t l)
{
  int result;

  if(subpkt_type == ZCRCW || subpkt_type == ZCRCE)  /* subpacket indicating 'end-of-frame' */
    zm->frame_in_transit=ZFALSE;
  else  /* other subpacket (mid-frame) */
    zm->frame_in_transit=ZTRUE;

  if(!zm->want_fcs_16 && zm->can_fcs_32) {
    if((result=send_data32( subpkt_type,p,l))!=0)
      return result;
  }
  else {  
    if((result=send_data16( subpkt_type,p,l))!=0)
      return result;
  }

  if(subpkt_type == ZCRCW)
    result=send_raw( 0x11/*XON*/);

  flush();

  return result;
}

int ZModem::send_data(uchar subpkt_type, unsigned char * p, size_t l)
{
  if(!zm->frame_in_transit) { /* Start of frame, include ZDATA header */
    lprintf(LOG_DEBUG,"send_data: start of frame, offset %u" ,zm->current_file_pos);
    send_pos_header( ZDATA, (uint32_t)zm->current_file_pos, /* Hex? */ ZFALSE);
  }

  return send_data_subpkt( subpkt_type, p, l);
}

int ZModem::send_pos_header(int type, int32_t pos, BOOL hex) 
{
  uchar header[5];

  header[0]   = type;
  header[ZP0] = (uchar) (pos        & 0xff);
  header[ZP1] = (uchar)((pos >>  8) & 0xff);
  header[ZP2] = (uchar)((pos >> 16) & 0xff);
  header[ZP3] = (uchar)((pos >> 24) & 0xff);

  if(hex)
    return send_hex_header( header);
  else
    return send_bin_header( header);
}

int ZModem::send_ack(int32_t pos)
{
  return send_pos_header( ZACK, pos, /* Hex? */ ZTRUE);
}

int ZModem::send_zfin()
{
  unsigned char zfin_header[] = { ZFIN, 0, 0, 0, 0 };

  lprintf(LOG_NOTICE,"Finishing Session (Sending ZFIN)");
  return send_hex_header(zfin_header);
}

int ZModem::send_zabort()
{
  lprintf(LOG_WARNING,"Aborting Transfer (Sending ZABORT)");
  return send_pos_header( ZABORT, 0, /* Hex? */ ZTRUE);
}

int ZModem::send_znak()
{
  lprintf(LOG_INFO,"Sending ZNAK");
  return send_pos_header( ZNAK, 0, /* Hex? */ ZTRUE);
}

int ZModem::send_zskip()
{
  lprintf(LOG_INFO,"Sending ZSKIP");
  return send_pos_header( ZSKIP, 0L, /* Hex? */ ZTRUE);
}

int ZModem::send_zeof(uint32_t pos)
{
  lprintf(LOG_INFO,"Sending End-of-File (ZEOF) frame (pos=%lu)", pos);
  return send_pos_header( ZEOF, pos, /* Hex? */ ZTRUE);
}


/*
 * rx_raw ; receive a single byte from the line.
 * reads as many are available and then processes them one at a time
 * check the data stream for 5 consecutive 0x18 CAN characters;
 * and if you see them abort. this saves a lot of clutter in
 * the rest of the code; even though it is a very strange place
 * for an exit. (but that was wat session abort was all about.)
 */

int ZModem::recv_raw()
{
  int c;
  unsigned attempt;

  for(attempt=0;attempt<=zm->recv_timeout;attempt++) {
    if((c=recv_byte(zm->cbdata,1)) >= 0) /* second timeout */
        break;
    if(is_cancelled())
      return(ZCAN);
    if(!is_connected())
      return(ABORTED);
  }
  if(attempt>zm->recv_timeout)
    return(TIMEOUT);

  if(c == ZMO_CAN) {
    zm->n_cans++;
    if(zm->n_cans == 5) {
      zm->cancelled=ZTRUE;
      lprintf(LOG_WARNING,"recv_raw: Cancelled remotely");
      /*return(TIMEOUT);  removed June-12-2005 */
    }
  }
  else {
    zm->n_cans = 0;
  }

  return c;
}

/*
 * rx; receive a single byte undoing any escaping at the
 * sending site. this bit looks like a mess. sorry for that
 * but there seems to be no other way without incurring a lot
 * of overhead. at least like this the path for a normal character
 * is relatively short.
 */

int ZModem::rx()
{
  int c;

  /*
   * outer loop for ever so for sure something valid
   * will come in; a timeout will occur or a session abort
   * will be received.
   */

  while(is_connected() && !is_cancelled()) {

    do {
      switch(c = recv_raw()) {
        case ZDLE:
          break;
        case ZMO_XON:
        case ZMO_XON|0x80:
        case ZMO_XOFF:
        case ZMO_XOFF|0x80:
          lprintf(LOG_WARNING,"rx: dropping flow ctrl char: %s" ,chr(c));
          continue;     
        default:
          /*
           * if all control characters should be escaped and 
           * this one wasnt then its spurious and should be dropped.
           */
          if(zm->escape_ctrl_chars && (c >= 0) && (c & 0x60) == 0) {
            lprintf(LOG_WARNING,"rx: dropping unescaped ctrl char: %s" ,chr(c));
            continue;
          }
          /*
           * normal character; return it.
           */
          return c;
      }
      break;
    } while(!is_cancelled());
  
    /*
     * ZDLE encoded sequence or session abort.
     * (or something illegal; then back to the top)
     */

    while(!is_cancelled()) {

      switch(c=recv_raw()) {
        case ZMO_XON:
        case ZMO_XON|0x80:
        case ZMO_XOFF:
        case ZMO_XOFF|0x80:
        case ZDLE:
          lprintf(LOG_WARNING,"rx: dropping escaped flow ctrl char: %s" ,chr(c));
          continue;     
        /*
         * these four are really nasty.
         * for convenience we just change them into 
         * special characters by setting a bit outside the
         * first 8. that way they can be recognized and still
         * be processed as characters by the rest of the code.
         */
        case ZCRCE:
        case ZCRCG:
        case ZCRCQ:
        case ZCRCW:
          lprintf(LOG_DEBUG,"rx: encoding data subpacket type: %s" ,chr(c));
          return (c | ZDLEESC);
        case ZRUB0:
          return 0x7f;
        case ZRUB1:
          return 0xff;
        default:
          if(c < 0)
            return c;

          if(zm->escape_ctrl_chars && (c & 0x60) == 0) {
            /*
             * a not escaped control character; probably
             * something from a network. just drop it.
             */
            lprintf(LOG_WARNING,"rx: dropping unescaped ctrl char: %s" ,chr(c));
            continue;
          }
          /*
           * legitimate escape sequence.
           * rebuild the orignal and return it.
           */
          if((c & 0x60) == 0x40) {
            return c ^ 0x40;
          }
          lprintf(LOG_WARNING,"rx: illegal sequence: ZDLE %s" ,chr(c));
          break;
      }
      break;
    } 
  }

  /*
   * not reached (unless cancelled).
   */

  return ABORTED;
}

/*
 * receive a data subpacket as dictated by the last received header.
 * return 2 with correct packet and end of frame
 * return 1 with correct packet frame continues
 * return 0 with incorrect frame.
 * return TIMEOUT with a timeout
 * if an acknowledgement is requested it is generated automatically
 * here. 
 */

/*
 * data subpacket reception
 */

int ZModem::recv_data32(unsigned char * p, unsigned maxlen, unsigned* l)
{
  int c;
  uint32_t rxd_crc;
  uint32_t crc;
  int subpkt_type;

  lprintf(LOG_DEBUG,"recv_data32");

  crc = 0xffffffffl;

  do {
    c = rx();

    if(c < 0)
      return c;

    if(c > 0xff)
      break;

    if(*l >= maxlen)
      return SUBPKTOVERFLOW;
    crc = ucrc32(c,crc);
    *p++ = c;
    (*l)++;
  } while(1);

  subpkt_type = c & 0xff;

  crc = ucrc32(subpkt_type, crc);

  crc = ~crc;

  rxd_crc  = rx();
  rxd_crc |= rx() << 8;
  rxd_crc |= rx() << 16;
  rxd_crc |= rx() << 24;

  if(rxd_crc != crc) {
    lprintf(LOG_WARNING,"CRC32 ERROR (%08lX, expected: %08lX) Bytes=%u, subpacket-type=%s" ,rxd_crc, crc, *l, chr(subpkt_type));
    return CRCFAILED;
  }
  lprintf(LOG_DEBUG,"GOOD CRC32: %08lX (Bytes=%u, subpacket-type=%s)" ,crc, *l, chr(subpkt_type));

  zm->ack_file_pos += *l;

  return subpkt_type;
}

int ZModem::recv_data16(register unsigned char* p, unsigned maxlen, unsigned* l)
{
  int c;
  int subpkt_type;
  unsigned short crc;
  unsigned short rxd_crc;

  lprintf(LOG_DEBUG,"recv_data16");

  crc = 0;

  do {
    c = rx();

    if(c < 0)
      return c;

    if(c > 0xff)
      break;

    if(*l >= maxlen)
      return SUBPKTOVERFLOW;
    crc = ucrc16(c,crc);
    *p++ = c;
    (*l)++;
  } while(1);

  subpkt_type = c & 0xff;

  crc = ucrc16(subpkt_type,crc);

  rxd_crc  = rx() << 8;
  rxd_crc |= rx();

  if(rxd_crc != crc) {
    lprintf(LOG_WARNING,"CRC16 ERROR (%04hX, expected: %04hX) Bytes=%d" ,rxd_crc, crc, *l);
    return CRCFAILED;
  }
  lprintf(LOG_DEBUG,"GOOD CRC16: %04hX (Bytes=%d)", crc, *l);

  zm->ack_file_pos += *l;

  return subpkt_type;
}

int ZModem::recv_data(unsigned char* p, size_t maxlen, unsigned* l, BOOL ack)
{
  int subpkt_type;
  unsigned n=0;

  if(l==NULL)
    l=&n;

  lprintf(LOG_DEBUG,"recv_data (%u-bit)", zm->receive_32bit_data ? 32:16);

  /*
   * receive the right type of frame
   */

  *l = 0;

  if(zm->receive_32bit_data) {
    subpkt_type = recv_data32( p, maxlen, l);
  }
  else {  
    subpkt_type = recv_data16( p, maxlen, l);
  }

  if(subpkt_type <= 0)  /* e.g. TIMEOUT, SUBPKTOVERFLOW, CRCFAILED */
    return(subpkt_type);

  lprintf(LOG_DEBUG,"recv_data received subpacket-type: %s" ,chr(subpkt_type));

  switch(subpkt_type)  {
    /*
     * frame continues non-stop
     */
    case ZCRCG:
      return FRAMEOK;
    /*
     * frame ends
     */
    case ZCRCE:
      return ENDOFFRAME;
    /*
     * frame continues; ZACK expected
     */
    case ZCRCQ:   
      if(ack)
        send_ack( zm->ack_file_pos);
      return FRAMEOK;
    /*
     * frame ends; ZACK expected
     */
    case ZCRCW:
      if(ack)
        send_ack( zm->ack_file_pos);
      return ENDOFFRAME;
  }

  lprintf(LOG_WARNING,"Received invalid subpacket-type: %s", chr(subpkt_type));

  return INVALIDSUBPKT;
}

BOOL ZModem::recv_subpacket(BOOL ack)
{
  int type;

  type=recv_data(zm->rx_data_subpacket,sizeof(zm->rx_data_subpacket),NULL,ack);
  if(type!=FRAMEOK && type!=ENDOFFRAME) {
    send_znak();
    return(ZFALSE);
  }

  return(ZTRUE);
}

int ZModem::recv_nibble() 
{
  int c;

  c = rx();

  if(c < 0)
    return c;

  if(c > '9') {
    if(c < 'a' || c > 'f') {
      /*
       * illegal hex; different than expected.
       * we might as well time out.
       */
      return -1;
    }

    c -= 'a' - 10;
  }
  else {
    if(c < '0') {
      /*
       * illegal hex; different than expected.
       * we might as well time out.
       */
      return -1;
    }
    c -= '0';
  }

  return c;
}

int ZModem::recv_hex()
{
  int n1;
  int n0;
  int ret;

  n1 = recv_nibble();

  if(n1 < 0)
    return n1;

  n0 = recv_nibble();

  if(n0 < 0)
    return n0;

  ret = (n1 << 4) | n0;

  lprintf(LOG_DEBUG,"recv_hex returning: 0x%02X", ret);

  return ret;
}

/*
 * receive routines for each of the six different styles of header.
 * each of these leaves zm->rxd_header_len set to 0 if the end result is
 * not a valid header.
 */
BOOL ZModem::recv_bin16_header()
{
  int c;
  int n;
  unsigned short int crc;
  unsigned short int rxd_crc;

  lprintf(LOG_DEBUG,"recv_bin16_header");

  crc = 0;

  for(n=0;n<HDRLEN;n++) {
    c = rx();
    if(c < 0) {
      lprintf(LOG_WARNING,"recv_bin16_header: %s", chr(c));
      return(ZFALSE);
    }
    crc = ucrc16(c,crc);
    zm->rxd_header[n] = c;
  }

  rxd_crc  = rx() << 8;
  rxd_crc |= rx();

  if(rxd_crc != crc) {
    lprintf(LOG_WARNING,"CRC16 ERROR: 0x%hX, expected: 0x%hX", rxd_crc, crc);
    return(ZFALSE);
  }
  lprintf(LOG_DEBUG,"GOOD CRC16: %04hX", crc);

  zm->rxd_header_len = 5;

  return(ZTRUE);
}

BOOL ZModem::recv_hex_header()
{
  int c;
  int i;
  unsigned short int crc = 0;
  unsigned short int rxd_crc;

  lprintf(LOG_DEBUG,"recv_hex_header");

  for(i=0;i<HDRLEN;i++) {
    c = recv_hex();
    if(c < 0 )
      return ZFALSE;
    crc = ucrc16(c,crc);

    zm->rxd_header[i] = c;
  }

  /*
   * receive the crc
   */

  c = recv_hex();

  if(c < 0)
    return ZFALSE;

  rxd_crc = c << 8;

  c = recv_hex();

  if(c < 0 )
    return ZFALSE;

  rxd_crc |= c;

  if(rxd_crc == crc) {
    lprintf(LOG_DEBUG,"GOOD CRC16: %04hX", crc);
    zm->rxd_header_len = 5;
  }
  else {
    lprintf(LOG_WARNING,"CRC16 ERROR: 0x%hX, expected: 0x%hX", rxd_crc, crc);
    return ZFALSE;
  }

  /*
   * drop the end of line sequence after a hex header
   */
  c = rx();
  if(c == '\r') {
    /*
     * both are expected with 0x0d
     */
    rx();  /* drop 0x0a */
  }

  return ZTRUE;
}

BOOL ZModem::recv_bin32_header()
{
  int c;
  int n;
  uint32_t crc;
  uint32_t rxd_crc;

  lprintf(LOG_DEBUG,"recv_bin32_header");

  crc = 0xffffffffL;

  for(n=0;n<HDRLEN;n++) {
    c = rx();
    if(c < 0)
      return(ZTRUE);
    crc = ucrc32(c,crc);
    zm->rxd_header[n] = c;
  }

  crc = ~crc;

  rxd_crc  = rx();
  rxd_crc |= rx() << 8;
  rxd_crc |= rx() << 16;
  rxd_crc |= rx() << 24;

  if(rxd_crc != crc) {
    lprintf(LOG_WARNING,"CRC32 ERROR (%08lX, expected: %08lX)" ,rxd_crc, crc);
    return(ZFALSE);
  }
  lprintf(LOG_DEBUG,"GOOD CRC32: %08lX", crc);

  zm->rxd_header_len = 5;
  return(ZTRUE);
}

/*
 * receive any style header
 * if the errors flag is set than whenever an invalid header packet is
 * received INVHDR will be returned. otherwise we wait for a good header
 * also; a flag (receive_32bit_data) will be set to indicate whether data
 * packets following this header will have 16 or 32 bit data attached.
 * variable headers are not implemented.
 */
int ZModem::recv_header_raw(int errors)
{
  int c;
  int frame_type;

  lprintf(LOG_DEBUG,"recv_header_raw");

  zm->rxd_header_len = 0;

  do {
    do {
      if((c = recv_raw()) < 0)
        return(c);
      if(is_cancelled())
        return(ZCAN);
    } while(c != ZPAD);
    
    if((c = recv_raw()) < 0)
      return(c);

    if(c == ZPAD) {
      if((c = recv_raw()) < 0)
        return(c);
    }

    /*
     * spurious ZPAD check
     */

    if(c != ZDLE) {
      lprintf(LOG_WARNING,"recv_header_raw: Expected ZDLE, received: %s" ,chr(c));
      continue;
    }

    /*
     * now read the header style
     */
    c = rx();
    switch (c) {
      case ZBIN:
        if(!recv_bin16_header())
          return INVHDR;
        zm->receive_32bit_data = ZFALSE;
        break;
      case ZHEX:
        if(!recv_hex_header())
          return INVHDR;
        zm->receive_32bit_data = ZFALSE;
        break;
      case ZBIN32:
        if(!recv_bin32_header())
          return INVHDR;
        zm->receive_32bit_data = ZTRUE;
        break;
      default:
        if(c < 0) {
          lprintf(LOG_WARNING,"recv_header_raw: %s", chr(c));
          return c;
        }
        /*
         * unrecognized header style
         */
        lprintf(LOG_ERR,"recv_header_raw: UNRECOGNIZED header style: %s" ,chr(c));
        if(errors) {
          return INVHDR;
        }

        continue;
    }
    if(errors && zm->rxd_header_len == 0) {
      return INVHDR;
    }

  } while(zm->rxd_header_len == 0 && !is_cancelled());

  if(is_cancelled())
    return(ZCAN);

  /*
   * this appears to have been a valid header.
   * return its type.
   */

  frame_type = zm->rxd_header[0];

  zm->rxd_header_pos = zm->rxd_header[ZP0] | (zm->rxd_header[ZP1] << 8) |
        (zm->rxd_header[ZP2] << 16) | (zm->rxd_header[ZP3] << 24);

  switch(frame_type) {
    case ZCRC:
      zm->crc_request = zm->rxd_header_pos;
      break;
    case ZDATA:
      zm->ack_file_pos = zm->rxd_header_pos;
      break;
    case ZFILE:
      zm->ack_file_pos = 0l;
      if(!recv_subpacket(/* ack? */ZFALSE))
        frame_type |= BADSUBPKT;
      break;
    case ZSINIT:
    case ZCOMMAND:
      if(!recv_subpacket(/* ack? */ZTRUE))
        frame_type |= BADSUBPKT;
      break;
    case ZFREECNT:
      send_pos_header( ZACK, 999999999L, /* Hex? */ ZTRUE);
      break;
  }

  lprintf(LOG_DEBUG,"recv_header_raw received header type: %s",frame_desc(frame_type));

  return frame_type;
}

int ZModem::recv_header()
{
  int ret;
  
  switch(ret = recv_header_raw( ZFALSE)) {
    case TIMEOUT:
      lprintf(LOG_WARNING,"recv_header TIMEOUT");
      break;
    case INVHDR:
      lprintf(LOG_WARNING,"recv_header detected an invalid header");
      break;
    default:
      lprintf(LOG_DEBUG,"recv_header returning: %s (pos=%lu)" ,frame_desc(ret), frame_pos( ret));

      if(ret==ZCAN)
        zm->cancelled=ZTRUE;
      else if(ret==ZRINIT)
        parse_zrinit();
      break;
  }

  return ret;
}

int ZModem::recv_header_and_check()
{
  int type=ABORTED;

  while(is_connected() && !is_cancelled()) {
    type = recv_header_raw(ZTRUE);   

    if(type == TIMEOUT)
      break;

    if(type != INVHDR && (type&BADSUBPKT) == 0)
      break;

    send_znak();
  }

  lprintf(LOG_DEBUG,"recv_header_and_check returning: %s (pos=%lu)",frame_desc(type), frame_pos( type));

  if(type==ZCAN)
    zm->cancelled=ZTRUE;

  return type;
}

BOOL ZModem::request_crc(int32_t length)
{
  recv_purge();
  send_pos_header(ZCRC,length,ZTRUE);
  return ZTRUE;
}

BOOL ZModem::recv_crc(uint32_t* crc)
{
  int type;

  if(!data_waiting(zm->crc_timeout)) {
    lprintf(LOG_ERR,"Timeout waiting for response (%u seconds)", zm->crc_timeout);
    return(ZFALSE);
  }
  if((type=recv_header())!=ZCRC) {
    lprintf(LOG_ERR,"Received %s instead of ZCRC", frame_desc(type));
    return(ZFALSE);
  }
  if(crc!=NULL)
    *crc = zm->crc_request;
  return ZTRUE;
}

BOOL ZModem::get_crc(int32_t length, uint32_t* crc)
{
  if(request_crc( length))
    return recv_crc( crc);
  return ZFALSE;
}

void ZModem::parse_zrinit()
{
  zm->can_full_duplex         = INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANFDX);
  zm->can_overlap_io          = INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANOVIO);
  zm->can_break           = INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANBRK);
  zm->can_fcs_32            = INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_CANFC32);
  zm->escape_ctrl_chars       = INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_ESCCTL);
  zm->escape_8th_bit          = INT_TO_BOOL(zm->rxd_header[ZF0] & ZF0_ESC8);

  /*
  lprintf(LOG_INFO,"Receiver requested mode (0x%02X):\r\n"
    "%s-duplex, %s overlap I/O, CRC-%u, Escape: %s"
    ,zm->rxd_header[ZF0]
    ,zm->can_full_duplex ? "Full" : "Half"
    ,zm->can_overlap_io ? "Can" : "Cannot"
    ,zm->can_fcs_32 ? 32 : 16
    ,zm->escape_ctrl_chars ? "ALL" : "Normal"
    );
  */

  if((zm->recv_bufsize = (zm->rxd_header[ZP0] | zm->rxd_header[ZP1]<<8)) != 0)
  {
    lprintf(LOG_INFO,"Receiver specified buffer size of: %u", zm->recv_bufsize);
  }
}

int ZModem::get_zrinit()
{
  unsigned char zrqinit_header[] = { ZRQINIT, /* ZF3: */0, 0, 0, /* ZF0: */0 };
  /* Note: sz/dsz/fdsz sends 0x80 in ZF3 because it supports var-length headers. */
  /* We do not, so we send 0x00, resulting in a CRC-16 value of 0x0000 as well. */

  send_raw('r');
  send_raw('z');
  send_raw('\r');
  send_hex_header(zrqinit_header);
  
  if(!data_waiting(zm->init_timeout))
    return(TIMEOUT);
  return recv_header();
}

int ZModem::send_zrinit()
{
  unsigned char zrinit_header[] = { ZRINIT, 0, 0, 0, 0 };
  
  zrinit_header[ZF0] = ZF0_CANFDX;

  if(!zm->no_streaming)
    zrinit_header[ZF0] |= ZF0_CANOVIO;

  if(zm->can_break)
    zrinit_header[ZF0] |= ZF0_CANBRK;

  if(!zm->want_fcs_16)
    zrinit_header[ZF0] |= ZF0_CANFC32;

  if(zm->escape_ctrl_chars)
    zrinit_header[ZF0] |= ZF0_ESCCTL;

  if(zm->escape_8th_bit)
    zrinit_header[ZF0] |= ZF0_ESC8;

  if(zm->no_streaming && zm->recv_bufsize==0)
    zm->recv_bufsize = sizeof(zm->rx_data_subpacket);

  zrinit_header[ZP0] = zm->recv_bufsize & 0xff;
  zrinit_header[ZP1] = zm->recv_bufsize >> 8;

  return send_hex_header( zrinit_header);
}

/* Returns ZFIN on success */
int ZModem::get_zfin()
{
  int result;
  int type=ZCAN;
  unsigned attempts;

  for(attempts=0; attempts<zm->max_errors && is_connected() && !is_cancelled(); attempts++) {
    if(attempts&1)  /* Alternate between ZABORT and ZFIN */
      result = send_zabort();
    else
      result = send_zfin();
    if(result != 0)
      return result;
    if((type = recv_header()) == ZFIN)
      break;
  }
  
  /*
   * these Os are formally required; but they don't do a thing
   * unfortunately many programs require them to exit 
   * (both programs already sent a ZFIN so why bother ?)
   */

  if(type == ZFIN) {
    send_raw('O');
    send_raw('O');
  }

  return type;
}

BOOL ZModem::handle_zrpos(uint64_t* pos)
{
  if(zm->rxd_header_pos <= zm->current_file_size) {
    if(*pos != zm->rxd_header_pos) {
      *pos = zm->rxd_header_pos;
      lprintf(LOG_INFO,"Resuming transfer from offset: %llu", *pos);
    }
    return ZTRUE;
  }
  lprintf(LOG_WARNING,"Invalid ZRPOS offset: %lu", zm->rxd_header_pos);
  return ZFALSE;
}

BOOL ZModem::handle_zack()
{
  if(zm->rxd_header_pos == zm->current_file_pos)
    return ZTRUE;
  lprintf(LOG_WARNING,"ZACK for incorrect offset (%lu vs %lu)" ,zm->rxd_header_pos, (ulong)zm->current_file_pos);
  return ZFALSE;
}

/*
 * send from the current position in the file
 * all the way to end of file or until something goes wrong.
 * (ZNAK or ZRPOS received)
 * returns ZRINIT on success.
 */
int ZModem::send_from(File* fp, uint64_t pos, uint64_t* sent)
{
  size_t n;
  uchar type;
  unsigned buf_sent=0;
  unsigned subpkts_sent=0;

  if(sent!=NULL)
    *sent=0;

  if(!fp->seek(pos)) {
    lprintf(LOG_ERR,"ERROR %d seeking to file offset %llu" ,errno, pos);
    send_pos_header( ZFERR, (uint32_t)pos, /* Hex? */ ZTRUE);
    return ZFERR;
  }
  zm->current_file_pos=pos;


  /*
   * send the data in the file
   */

  while(is_connected()) {

    /*
     * read a block from the file
     */

    n = fp->read(zm->tx_data_subpacket,zm->block_size);

    progress(zm->cbdata, fp->position());
    
    type = ZCRCW;

    /** ZMODEM.DOC:
      ZCRCW data subpackets expect a response before the next frame is sent.
      If the receiver does not indicate overlapped I/O capability with the
      CANOVIO bit, or sets a buffer size, the sender uses the ZCRCW to allow
      the receiver to write its buffer before sending more data.
    ***/
    /* Note: we always use ZCRCW for the first frame */
    if(subpkts_sent || n < zm->block_size) {
      /*  ZMODEM.DOC:
        In the absence of fatal error, the sender eventually encounters end of
        file.  If the end of file is encountered within a frame, the frame is
        closed with a ZCRCE data subpacket which does not elicit a response
        except in case of error.
      */
      if(n < zm->block_size)
        type = ZCRCE;
      else {
        if(zm->can_overlap_io && !zm->no_streaming && (zm->recv_bufsize==0 || buf_sent+n < zm->recv_bufsize))
          type = ZCRCG;
        else  /* Send a ZCRCW frame */
          buf_sent = 0; 
      }
    }

    /* Note: No support for sending ZCRCQ data sub-packets here */

    if(send_data( type, zm->tx_data_subpacket, n)!=0)
      return(TIMEOUT);

    zm->current_file_pos += n;
    if(zm->current_file_pos > zm->current_file_size)
      zm->current_file_size = zm->current_file_pos;
    subpkts_sent++;

    if(type == ZCRCW || type == ZCRCE) {  
      lprintf(LOG_DEBUG,"Sent end-of-frame (%s sub-packet)", chr(type));
      if(type==ZCRCW) { /* ZACK expected */
        lprintf(LOG_DEBUG,"Waiting for ZACK");
        while(is_connected()) {
          int ack;
          if((ack = recv_header()) != ZACK)
            return(ack);

          if(is_cancelled())
            return(ZCAN);

          if(handle_zack())
            break;
        } 
      }
    }

    if(sent!=NULL)
      *sent+=n;

    buf_sent+=n;

    if(n < zm->block_size) {
      lprintf(LOG_DEBUG,"send_from: end of file (or read error) reached at offset: %lld", zm->current_file_pos);
      send_zeof( (uint32_t)zm->current_file_pos);
      return recv_header();  /* If this is ZRINIT, Success */
    }

    /* 
     * characters from the other side
     * check out that header
     */

    while(data_waiting( zm->consecutive_errors ? 1:0) 
      && !is_cancelled() && is_connected()) {
      int rx_type;
      int c;
      lprintf(LOG_DEBUG,"Back-channel traffic detected:");
      if((c = recv_raw()) < 0)
        return(c);
      if(c == ZPAD) {
        /* ZMODEM.DOC: 
          FULL STREAMING WITH SAMPLING
          If one of these characters (0x18 CAN or ZPAD) is seen, an
          empty ZCRCE data subpacket is sent.
        */
        send_data( ZCRCE, NULL, 0);
        rx_type = recv_header();
        lprintf(LOG_DEBUG,"Received back-channel data: %s", chr(rx_type));
        if(rx_type >= 0) {
          return rx_type;
        }
      } 
      else
      {
        lprintf(LOG_DEBUG,"Received: %s",chr(c));
      }
    }
    if(is_cancelled())
      return(ZCAN);

    zm->consecutive_errors = 0;

    if(zm->block_size < zm->max_block_size) {
      zm->block_size*=2;
      if(zm->block_size > zm->max_block_size)
        zm->block_size = zm->max_block_size;
    }
  }

  lprintf(LOG_DEBUG,"send_from: returning unexpectedly!");

  /*
   * end of file reached.
   * should receive something... so fake ZACK
   */

  return ZACK;
}

/*
 * send a file; returns true when session is successful. (or file is skipped)
 */

BOOL ZModem::send_file(char* fname, File* fp, BOOL request_init, time_t* start, uint64_t* sent)
{
  uint64_t  pos=0;
  uint64_t  sent_bytes;
  unsigned char * p;
  uchar   zfile_frame[] = { ZFILE, 0, 0, 0, 0 };
  int     type;
  int     i;
  unsigned  attempts;

  if(zm->block_size == 0)
    zm->block_size = ZBLOCKLEN; 

  if(zm->block_size < 128)
    zm->block_size = 128; 

  if(zm->block_size > sizeof(zm->tx_data_subpacket))
    zm->block_size = sizeof(zm->tx_data_subpacket);

  if(zm->max_block_size < zm->block_size)
    zm->max_block_size = zm->block_size;

  if(zm->max_block_size > sizeof(zm->rx_data_subpacket))
    zm->max_block_size = sizeof(zm->rx_data_subpacket);

  if(sent!=NULL)  
    *sent=0;

  if(start!=NULL)   
    *start=time(NULL);

  zm->file_skipped=ZFALSE;

  if(zm->no_streaming)
  {
    lprintf(LOG_WARNING,"Streaming disabled");
  }

  if(request_init) {
    for(zm->errors=0; zm->errors<=zm->max_errors && !is_cancelled() && is_connected(); zm->errors++) {
      if(zm->errors)
      {
        lprintf(LOG_NOTICE,"Sending ZRQINIT (%u of %u)" ,zm->errors+1,zm->max_errors+1);
      }
      else
      {
        lprintf(LOG_INFO,"Sending ZRQINIT");
      }
      i = get_zrinit();
      if(i == ZRINIT)
        break;
      lprintf(LOG_WARNING,"send_file: received %s instead of ZRINIT" ,frame_desc(i));
    }
    if(zm->errors>=zm->max_errors || is_cancelled() || !is_connected())
      return(ZFALSE);
  }

  zm->current_file_size = fp->size();
  SAFECOPY(zm->current_file_name, getfname(fname));

  /*
   * the file exists. now build the ZFILE frame
   */

  /*
   * set conversion option
   * (not used; always binary)
   */

  zfile_frame[ZF0] = ZF0_ZCBIN;

  /*
   * management option
   */

  if(zm->management_protect) {
    zfile_frame[ZF1] = ZF1_ZMPROT;    
    lprintf(LOG_DEBUG,"send_file: protecting destination");
  }
  else if(zm->management_clobber) {
    zfile_frame[ZF1] = ZF1_ZMCLOB;
    lprintf(LOG_DEBUG,"send_file: overwriting destination");
  }
  else if(zm->management_newer) {
    zfile_frame[ZF1] = ZF1_ZMNEW;
    lprintf(LOG_DEBUG,"send_file: overwriting destination if newer");
  }
  else
    zfile_frame[ZF1] = ZF1_ZMCRC;

  /*
   * transport options
   * (just plain normal transfer)
   */

  zfile_frame[ZF2] = ZF2_ZTNOR;

  /*
   * extended options
   */

  zfile_frame[ZF3] = 0;

  /*
   * now build the data subpacket with the file name and lots of other
   * useful information.
   */

  /*
   * first enter the name and a 0
   */

  p = zm->tx_data_subpacket;

  strncpy((char *)zm->tx_data_subpacket,getfname(fname),sizeof(zm->tx_data_subpacket)-1);
  zm->tx_data_subpacket[sizeof(zm->tx_data_subpacket)-1]=0;

  p += strlen((char*)p) + 1;

  sprintf((char*)p,"%lld"" %llo 0 0 %u %lld"" 0"
    ,zm->current_file_size  /* use for estimating only, could be zero! */
    ,0
    ,zm->files_remaining
    ,zm->bytes_remaining
    );

  p += strlen((char*)p) + 1;

  for(attempts=0;;attempts++) {

    if(attempts > zm->max_errors)
      return(ZFALSE);

    /*
     * send the header and the data
     */

    lprintf(LOG_DEBUG,"Sending ZFILE frame: '%s'" ,zm->tx_data_subpacket+strlen((char*)zm->tx_data_subpacket)+1);

    if((i=send_bin_header(zfile_frame))!=0) {
      lprintf(LOG_DEBUG,"send_bin_header returned %d",i);
      continue;
    }
    if((i=send_data_subpkt(ZCRCW,zm->tx_data_subpacket,p - zm->tx_data_subpacket))!=0) {
      lprintf(LOG_DEBUG,"send_data_subpkt returned %d",i);
      continue;
    }
    /*
     * wait for anything but an ZACK packet
     */

    do {
      type = recv_header();
      if(is_cancelled())
        return(ZFALSE);
    } while(type == ZACK && is_connected());

    if(!is_connected())
      return(ZFALSE);

#if 0
    lprintf(LOG_INFO,"type : %d",type);
#endif

    if(type == ZSKIP) {
      zm->file_skipped=ZTRUE;
      lprintf(LOG_WARNING,"File skipped by receiver");
      return(ZTRUE);
    }

    if(type == ZCRC) {
      if(zm->crc_request==0)
      {
        lprintf(LOG_NOTICE,"Receiver requested CRC of entire file");
      }
      else
      {
        lprintf(LOG_NOTICE,"Receiver requested CRC of first %lu bytes" ,zm->crc_request);
      }
      send_pos_header(ZCRC,fcrc32(fp,zm->crc_request),ZTRUE);
      type = recv_header();
    }

    if(type == ZRPOS)
      break;
  }

  if(!handle_zrpos( &pos))
    return(ZFALSE);

  zm->transfer_start_pos = pos;
  zm->transfer_start_time = time(NULL);

  if(start!=NULL)   
    *start=zm->transfer_start_time;

  fp->seek(0);
  zm->errors = 0;
  zm->consecutive_errors = 0;

  lprintf(LOG_DEBUG,"Sending %s from offset %llu", fname, pos);
  do {
    /*
     * and start sending
     */

    type = send_from( fp, pos, &sent_bytes);

    if(!is_connected())
      return(ZFALSE);

    if(type == ZFERR || type == ZABORT || is_cancelled())
      break;

    if(type == ZSKIP) {
      zm->file_skipped=ZTRUE;
      lprintf(LOG_WARNING,"File skipped by receiver at offset: %llu", pos + sent_bytes);
      /* ZOC sends a ZRINIT after mid-file ZSKIP, so consume the ZRINIT here */
      recv_header();
      return(ZTRUE);
    }

    if(sent != NULL)
      *sent += sent_bytes;

    if(type == ZRINIT)
      return(ZTRUE); /* Success */

    if(type==ZACK && handle_zack()) {
      pos += sent_bytes;
      continue;
    }

    /* Error of some kind */

    lprintf(LOG_ERR,"Received %s at offset: %lld", chr(type), zm->current_file_pos);

    if(zm->block_size == zm->max_block_size && zm->max_block_size > ZBLOCKLEN)
      zm->max_block_size /= 2;

    if(zm->block_size > 128)
      zm->block_size /= 2; 

    zm->errors++;
    if(++zm->consecutive_errors > zm->max_errors)
      break;  /* failure */

    if(type==ZRPOS) {
      if(!handle_zrpos( &pos))
        break;
    }
  } while(ZTRUE);

  lprintf(LOG_WARNING,"Transfer failed on receipt of: %s", chr(type));

  return(ZFALSE);
}

int ZModem::recv_files(const char* download_dir, uint64_t* bytes_received)
{
  char    fpath[MAX_PATH+1];
  File*   fp;
  int64_t   l;
  BOOL    skip;
  BOOL    loop;
  uint64_t  b;
  uint32_t  crc;
  uint32_t  rcrc;
  int64_t   bytes;
  int64_t   kbytes;
  int64_t   start_bytes;
  unsigned  files_received=0;
  time_t    t;
  unsigned  cps;
  unsigned  timeout;
  unsigned  errors;

  if(bytes_received!=NULL)
    *bytes_received=0;
  zm->current_file_num=1;
  while(recv_init()==ZFILE) {
    bytes=zm->current_file_size;
    kbytes=bytes/1024;
    if(kbytes<1) 
      kbytes=0;
    lprintf(LOG_INFO,"Downloading %s (%lld"" KBytes) via Zmodem", zm->current_file_name, kbytes);

    do {  /* try */
      skip=ZTRUE;
      loop=ZFALSE;

      sprintf(fpath,"%s/%s",download_dir,zm->current_file_name);
      lprintf(LOG_DEBUG,"fpath=%s",fpath);
      File fileF=zfileSystem->open(fpath);
      if(fileF) {
        l=fileF.size();
        lprintf(LOG_WARNING,"%s already exists (%lld"" bytes)",fpath,l);
        if(l>=(int32_t)bytes) {
          lprintf(LOG_WARNING,"Local file size >= remote file size (%lld"")" ,bytes);
          break;
          /*
          if(zm->duplicate_filename==NULL)
            break;
          else {
            if(l > (int32_t)bytes) {
              if(zm->duplicate_filename(zm->cbdata, zm)) {
                loop=ZTRUE;
                continue;
              }
              break;
            }
          }
          */
        }
        fp=&fileF;
        if(!(*fp)) {
          lprintf(LOG_ERR,"Error %d opening %s", errno, fpath);
          break;
        }
        //setvbuf(fp,NULL,_IOFBF,0x10000);

        lprintf(LOG_NOTICE,"Requesting CRC of remote file: %s", zm->current_file_name);
        if(!request_crc( (uint32_t)l)) {
          fp->close();
          lprintf(LOG_ERR,"Failed to request CRC of remote file");
          break;
        }
        lprintf(LOG_NOTICE,"Calculating CRC of: %s", fpath);
        crc=fcrc32(fp,(uint32_t)l); /* Warning: 4GB limit! */
        fp->close();
        lprintf(LOG_INFO,"CRC of %s (%lu bytes): %08lX" ,getfname(fpath), (ulong)l, crc);
        lprintf(LOG_NOTICE,"Waiting for CRC of remote file: %s", zm->current_file_name);
        if(!recv_crc(&rcrc)) {
          lprintf(LOG_ERR,"Failed to get CRC of remote file");
          break;
        }
        if(crc!=rcrc) {
          lprintf(LOG_WARNING,"Remote file has different CRC value: %08lX", rcrc);
          /*
          if(zm->duplicate_filename) {
            if(zm->duplicate_filename(zm->cbdata, zm)) {
              loop=ZTRUE;
              continue;
            }
          }
          */
          break;
        }
        if(l == (int32_t)bytes) {
          lprintf(LOG_INFO,"CRC, length, and filename match.");
          break;
        }
        lprintf(LOG_INFO,"Resuming download of %s",fpath);
      }

      if(fileF)
        fileF.close();
      fileF=zfileSystem->open(fpath, FILE_APPEND);
      fp = &fileF;
      start_bytes=fp->size();

      skip=ZFALSE;
      errors=recv_file_data(fp,start_bytes);

      l=fp->size();
      fp->close();
      if(errors && l==0)  { /* aborted/failed download */
        if(zfileSystem->remove(fpath)) /* don't save 0-byte file */
        {
          lprintf(LOG_ERR,"Error %d removing %s",errno,fpath);
        }
        else
        {
          lprintf(LOG_INFO,"Deleted 0-byte file %s",fpath);
        }
      }
      else {
        if(l!=bytes) {
          lprintf(LOG_WARNING,"Incomplete download (%lld"" bytes received, expected %lld"")",l,bytes);
        } else {
          if((t=time(NULL)-zm->transfer_start_time)<=0)
            t=1;
          b=l-start_bytes;
          if((cps=(unsigned)(b/t))==0)
            cps=1;
          lprintf(LOG_INFO,"Received %llu"" bytes successfully (%u CPS)",b,cps);
          files_received++;
          if(bytes_received!=NULL)
            *bytes_received+=b;
        }
        //if(zm->current_file_time) //BZ: unsupported...
        //  setfdate(fpath,zm->current_file_time);
      }

    } while(loop);
    /* finally */

    if(skip) {
      lprintf(LOG_WARNING,"Skipping file");
      send_zskip();
    }
    zm->current_file_num++;
  }
  if(zm->local_abort)
    send_zabort();

  /* wait for "over-and-out" */
  timeout=zm->recv_timeout;
  zm->recv_timeout=2;
  if(rx()=='O')
    rx();
  zm->recv_timeout=timeout;

  return(files_received);
}

int ZModem::recv_init()
{
  int     type=0x18/*CAN*/;
  unsigned  errors;

  lprintf(LOG_DEBUG,"recv_init");

#if 0
  while(is_connected() && !is_cancelled() && (ch=recv_byte(zm->cbdata,0))!=NOINP)
  {
    lprintf(LOG_DEBUG,"Throwing out received: %s",chr((uchar)ch));
  }
#endif

  for(errors=0; errors<=zm->max_errors && !is_cancelled() && is_connected(); errors++) {
    if(errors)
    {
      lprintf(LOG_NOTICE,"Sending ZRINIT (%u of %u)",errors+1, zm->max_errors+1);
    }
    else
    {
      lprintf(LOG_INFO,"Sending ZRINIT");
    }
    send_zrinit();

    type = recv_header();

    if(zm->local_abort)
      break;

    if(type==TIMEOUT)
      continue;

    lprintf(LOG_DEBUG,"recv_init: Received %s",chr(type));

    if(type==ZFILE) {
      parse_zfile_subpacket();
      return(type);
    }

    if(type==ZFIN) {
      send_zfin(); /* 0x06 ACK */
      return(type);
    }

    lprintf(LOG_WARNING,"recv_init: Received %s instead of ZFILE or ZFIN" ,frame_desc(type));
    lprintf(LOG_DEBUG,"ZF0=%02X ZF1=%02X ZF2=%02X ZF3=%02X" ,zm->rxd_header[ZF0],zm->rxd_header[ZF1],zm->rxd_header[ZF2],zm->rxd_header[ZF3]);
  }

  return(type);
}

void ZModem::parse_zfile_subpacket()
{
  int     i;
  int     mode=0;
  long    serial=-1L;
  ulong   tmptime;

  SAFECOPY(zm->current_file_name,getfname((char*)zm->rx_data_subpacket));

  zm->current_file_size = 0;
  zm->current_file_time = 0;
  zm->files_remaining = 0;
  zm->bytes_remaining = 0;

  i=sscanf((char*)zm->rx_data_subpacket+strlen((char*)zm->rx_data_subpacket)+1,"%lld %lo %o %lo %u %lld"
    ,&zm->current_file_size /* file size (decimal) */
    ,&tmptime       /* file time (octal unix format) */
    ,&mode          /* file mode */
    ,&serial        /* program serial number */
    ,&zm->files_remaining /* remaining files to be sent */
    ,&zm->bytes_remaining /* remaining bytes to be sent */
    );
  zm->current_file_time=tmptime;

  lprintf(LOG_DEBUG,"Zmodem file (ZFILE) data (%u fields): %s",i, zm->rx_data_subpacket+strlen((char*)zm->rx_data_subpacket)+1);

  if(!zm->files_remaining)
    zm->files_remaining = 1;
  if(!zm->bytes_remaining)
    zm->bytes_remaining = zm->current_file_size;

  if(!zm->total_files)
    zm->total_files = zm->files_remaining;
  if(!zm->total_bytes)
    zm->total_bytes = zm->bytes_remaining;
}

/*
 * receive file data until the end of the file or until something goes wrong.
 * the name is only used to show progress
 */

unsigned ZModem::recv_file_data(File* fp, int64_t offset)
{
  int     type=0;
  unsigned  errors=0;
  off_t   pos;

  zm->transfer_start_pos=offset;
  zm->transfer_start_time=time(NULL);

  if(!(fp->seek(offset))) {
    lprintf(LOG_ERR,"ERROR %d seeking to file offset %lld" ,errno, offset);
    ZModem::send_pos_header( ZFERR, (uint32_t)offset, /* Hex? */ ZTRUE);
    return 1; /* errors */
  }

  /*  zmodem.doc:

    The zmodem receiver uses the file length [from ZFILE data] as an estimate only.
    It may be used to display an estimate of the transmission time,
    and may be compared with the amount of free disk space.  The
    actual length of the received file is determined by the data
    transfer. A file may grow after transmission commences, and
    all the data will be sent.
  */
  while(errors<=zm->max_errors && is_connected() && !is_cancelled()) {

    if((pos=fp->position()) > zm->current_file_size)
      zm->current_file_size = pos;

    if(zm->max_file_size!=0 && pos >= zm->max_file_size) {
      lprintf(LOG_WARNING,"Specified maximum file size (%lld"" bytes) reached at offset %lld" ,zm->max_file_size, pos);
      send_pos_header( ZFERR, (uint32_t)pos, /* Hex? */ ZTRUE);
      break;
    }

    if(type!=ENDOFFRAME)
      send_pos_header( ZRPOS, (uint32_t)pos, /* Hex? */ ZTRUE);

    type = recv_file_frame(fp);
    if(type == ZEOF || type == ZFIN)
      break;
    if(type==ENDOFFRAME)
    {
      lprintf(LOG_DEBUG,"Received complete frame at offset: %lu", (ulong)fp->position());
    }
    else {
      if(type>0 && !zm->local_abort)
      {
        lprintf(LOG_ERR,"Received %s at offset: %lu", chr(type), (ulong)fp->position());
      }
      errors++;
    }
  }

  /*
   * wait for the eof header
   */
  for(;errors<=zm->max_errors && !is_cancelled() && type!=ZEOF && type!=ZFIN; errors++)
    type = recv_header_and_check();

  return(errors);
}


int ZModem::recv_file_frame(File* fp)
{
  unsigned  n;
  int     type;
  unsigned  attempt;

  /*
   * wait for a ZDATA header with the right file offset
   * or a timeout or a ZFIN
   */

  for(attempt=0;;attempt++) {
    if(attempt>=zm->max_errors)
      return TIMEOUT;
    type = recv_header();
    switch(type) {
      case ZEOF:
        /* ZMODEM.DOC:
           If the receiver has not received all the bytes of the file, 
           the receiver ignores the ZEOF because a new ZDATA is coming.
        */
        if(zm->rxd_header_pos==(uint32_t)fp->position())  
          return type;
        lprintf(LOG_WARNING,"Ignoring ZEOF as all bytes (%lu) have not been received",zm->rxd_header_pos);
        continue;
      case ZFIN:
        return type;
      case TIMEOUT:
        return type;
    }
    if(is_cancelled() || !is_connected())
      return ZCAN;

    if(type==ZDATA)
      break;

    lprintf(LOG_WARNING,"Received %s instead of ZDATA frame", frame_desc(type));
  }

  if(zm->rxd_header_pos!=(uint32_t)fp->position()) {
    lprintf(LOG_WARNING,"Received wrong ZDATA frame (%lu vs %lu)" ,zm->rxd_header_pos, (ulong)fp->position());
    return ZFALSE;
  }
  
  do {
    type = recv_data(zm->rx_data_subpacket,sizeof(zm->rx_data_subpacket),&n,ZTRUE);

    lprintf(LOG_INFO,"packet len %d type %d\r\n",n,type);

    if (type == ENDOFFRAME || type == FRAMEOK) {
      
      if(fp->write(zm->rx_data_subpacket,n)!=n) {
        lprintf(LOG_ERR,"ERROR %d writing %u bytes at file offset %llu" ,errno, n,(uint64_t)fp->position());
        send_pos_header( ZFERR, (uint32_t)fp->position(), /* Hex? */ ZTRUE);
        return ZFALSE;
      }
    }

    if(type==FRAMEOK)
      zm->block_size = n;
    
    progress(zm->cbdata, fp->position());

    if(is_cancelled())
      return(ZCAN);

  } while(type == FRAMEOK);

  return type;
}

const char* ZModem::source(void)
{
  return (__FILE__);
}

char* ZModem::ver(char *buf)
{
  sscanf("$Revision: 1.120 $", "%*s %s", buf);

  return (buf);
}

#endif
