/*
 * zmodem.h
 * zmodem constants
 * (C) Mattheij Computer Service 1994
 *
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
 */

/* $Id: zmodem.h,v 1.55 2018/02/01 08:20:19 deuce Exp $ */

#ifndef _ZMODEM_H
#define _ZMODEM_H

#define ZMODEM_FILE_SIZE_MAX  0xffffffff  /* 32-bits, blame Chuck */

/*
 * ascii constants
 */

#define BOOL bool
#define BYTE uint8_t
#define uchar uint8_t
#define MAX_PATH 253
#define NOINP   -1      /* input buffer empty (incom only) */
#define LOG_EMERG       0       /* system is unusable */
#define LOG_ALERT       1       /* action must be taken immediately */
#define LOG_CRIT        2       /* critical conditions */
#define LOG_ERR         3       /* error conditions */
#define LOG_WARNING     4       /* warning conditions */
#define LOG_NOTICE      5       /* normal but significant condition */
#define LOG_INFO        6       /* informational */
#define LOG_DEBUG       7       /* debug-level messages */

#define  ZMO_DLE      0x10
#define  ZMO_XON      0x11
#define  ZMO_XOFF     0x13
#define  ZMO_CAN      0x18

#ifndef INT_TO_BOOL
#define INT_TO_BOOL(x)  ((x)?ZTRUE:ZFALSE)
#endif

#define ZFALSE 0
#define ZTRUE 1

#define TERMINATE(str)                      str[sizeof(str)-1]=0

/* This is a bound-safe version of strcpy basically - only works with fixed-length arrays */
#ifdef SAFECOPY_USES_SPRINTF
#define SAFECOPY(dst,src)                   sprintf(dst,"%.*s",(int)sizeof(dst)-1,src)
#else   /* strncpy is faster */
#define SAFECOPY(dst,src)                   (strncpy(dst,src,sizeof(dst)), TERMINATE(dst))
#endif

/*
 * zmodem constants
 */

#define ZBLOCKLEN  1024    /* "true" Zmodem max subpacket length */

#define ZMAXHLEN    0x10    /* maximum header information length */
#define ZMAXSPLEN   0x400    /* maximum subpacket length */


#define  ZPAD     0x2a    /* pad character; begins frames */
#define  ZDLE     0x18    /* ctrl-x zmodem escape */
#define  ZDLEE    0x58    /* escaped ZDLE */

#define  ZBIN     0x41    /* binary frame indicator (CRC16) */
#define  ZHEX     0x42    /* hex frame indicator */
#define  ZBIN32   0x43    /* binary frame indicator (CRC32) */
#define  ZBINR32  0x44    /* run length encoded binary frame (CRC32) */

#define  ZVBIN    0x61    /* binary frame indicator (CRC16) */
#define  ZVHEX    0x62    /* hex frame indicator */
#define  ZVBIN32  0x63    /* binary frame indicator (CRC32) */
#define  ZVBINR32 0x64    /* run length encoded binary frame (CRC32) */

#define  ZRESC    0x7e    /* run length encoding flag / escape character */

/*
 * zmodem frame types
 */

#define  ZRQINIT     0x00    /* request receive init (s->r) */
#define  ZRINIT      0x01    /* receive init (r->s) */
#define  ZSINIT      0x02    /* send init sequence (optional) (s->r) */
#define  ZACK        0x03    /* ack to ZRQINIT ZRINIT or ZSINIT (s<->r) */
#define  ZFILE       0x04    /* file name (s->r) */
#define  ZSKIP       0x05    /* skip this file (r->s) */
#define  ZNAK        0x06    /* last packet was corrupted (?) */
#define  ZABORT      0x07    /* abort batch transfers (?) */
#define  ZFIN        0x08    /* finish session (s<->r) */
#define  ZRPOS       0x09    /* resume data transmission here (r->s) */
#define  ZDATA       0x0a    /* data packet(s) follow (s->r) */
#define  ZEOF        0x0b    /* end of file reached (s->r) */
#define  ZFERR       0x0c    /* fatal read or write error detected (?) */
#define  ZCRC        0x0d    /* request for file CRC and response (?) */
#define  ZCHALLENGE  0x0e    /* security challenge (r->s) */
#define  ZCOMPL      0x0f    /* request is complete (?) */
#define  ZCAN        0x10    /* pseudo frame;
                   other end cancelled session with 5* CAN */
#define  ZFREECNT    0x11    /* request free bytes on file system (s->r) */
#define  ZCOMMAND    0x12    /* issue command (s->r) */
#define  ZSTDERR     0x13    /* output data to stderr (??) */

/*
 * ZDLE sequences
 */

#define  ZCRCE    0x68    /* CRC next, frame ends, header packet follows */
#define  ZCRCG    0x69    /* CRC next, frame continues nonstop */
#define  ZCRCQ    0x6a    /* CRC next, frame continuous, ZACK expected */
#define  ZCRCW    0x6b    /* CRC next, frame ends,       ZACK expected */
#define  ZRUB0    0x6c    /* translate to rubout 0x7f */
#define  ZRUB1    0x6d    /* translate to rubout 0xff */

/*
 * frame specific data.
 * entries are prefixed with their location in the header array.
 */

/*
 * Byte positions within header array
 */

#define FTYPE 0          /* frame type */

#define ZF0  4          /* First flags byte */
#define ZF1  3
#define ZF2  2
#define ZF3  1

#define ZP0  1          /* Low order 8 bits of position */
#define ZP1  2
#define ZP2  3
#define ZP3  4          /* High order 8 bits of file position */

/*
 * ZRINIT frame
 * zmodem receiver capability flags
 */

#define  ZF0_CANFDX    0x01  /* Receiver can send and receive true full duplex */
#define  ZF0_CANOVIO   0x02  /* receiver can receive data during disk I/O */
#define  ZF0_CANBRK    0x04  /* receiver can send a break signal */
#define  ZF0_CANCRY    0x08  /* Receiver can decrypt DONT USE */
#define  ZF0_CANLZW    0x10  /* Receiver can uncompress DONT USE */
#define  ZF0_CANFC32   0x20  /* Receiver can use 32 bit Frame Check */
#define  ZF0_ESCCTL    0x40  /* Receiver expects ctl chars to be escaped */
#define  ZF0_ESC8      0x80  /* Receiver expects 8th bit to be escaped */

#define ZF1_CANVHDR    0x01  /* Variable headers OK */

/*
 * ZSINIT frame
 * zmodem sender capability
 */

#define ZF0_TESCCTL   0x40  /* Transmitter expects ctl chars to be escaped */
#define ZF0_TESC8     0x80  /* Transmitter expects 8th bit to be escaped */

#define ZATTNLEN      0x20  /* Max length of attention string */
#define ALTCOFF       ZF1    /* Offset to alternate canit string, 0 if not used */

/*
 * ZFILE frame
 */

/*
 * Conversion options one of these in ZF0
 */

#define ZF0_ZCBIN   1    /* Binary transfer - inhibit conversion */
#define ZF0_ZCNL    2    /* Convert NL to local end of line convention */
#define ZF0_ZCRESUM 3    /* Resume interrupted file transfer */

/*
 * Management include options, one of these ored in ZF1
 */

#define ZF1_ZMSKNOLOC  0x80  /* Skip file if not present at rx */
#define ZF1_ZMMASK     0x1f  /* Mask for the choices below */
#define ZF1_ZMNEWL      1    /* Transfer if source newer or longer */
#define ZF1_ZMCRC       2    /* Transfer if different file CRC or length */
#define ZF1_ZMAPND      3    /* Append contents to existing file (if any) */
#define ZF1_ZMCLOB      4    /* Replace existing file */
#define ZF1_ZMNEW       5    /* Transfer if source newer */
#define ZF1_ZMDIFF      6    /* Transfer if dates or lengths different */
#define ZF1_ZMPROT      7    /* Protect destination file */
#define ZF1_ZMCHNG      8    /* Change filename if destination exists */

/*
 * Transport options, one of these in ZF2
 */

#define ZF2_ZTNOR    0    /* no compression */
#define ZF2_ZTLZW    1    /* Lempel-Ziv compression */
#define ZF2_ZTRLE    3    /* Run Length encoding */

/*
 * Extended options for ZF3, bit encoded
 */

#define ZF3_ZCANVHDR   0x01  /* Variable headers OK */
                             /* Receiver window size override */
#define ZF3_ZRWOVR     0x04  /* byte position for receive window override/256 */
#define ZF3_ZXSPARS    0x40  /* encoding for sparse file operations */

/*
 * ZCOMMAND frame
 */

#define ZF0_ZCACK1     0x01  /* Acknowledge, then do command */

typedef struct {

  BYTE      rxd_header[ZMAXHLEN];              /* last received header */
  int       rxd_header_len;                  /* last received header size */
  uint32_t  rxd_header_pos;                  /* last received header position value */

  /*
   * receiver capability flags
   * extracted from the ZRINIT frame as received
   */

  BOOL can_full_duplex;
  BOOL can_overlap_io;
  BOOL can_break;
  BOOL can_fcs_32;
  BOOL want_fcs_16;
  BOOL escape_ctrl_chars;
  BOOL escape_8th_bit;

  /*
   * file management options.
   * only one should be on
   */

  int management_newer;
  int management_clobber;
  int management_protect;

  /* from zmtx.c */

  BYTE tx_data_subpacket[8192];
  BYTE rx_data_subpacket[8192];              /* zzap = 8192 */

  char       current_file_name[MAX_PATH+1];
  int64_t    current_file_size;
  int64_t    current_file_pos;
  time_t     current_file_time;
  unsigned   current_file_num;
  unsigned   total_files;
  int64_t    total_bytes;
  unsigned   files_remaining;
  int64_t    bytes_remaining;
  int64_t    transfer_start_pos;
  time_t     transfer_start_time;

  int      receive_32bit_data;
  int      use_crc16;
  int32_t  ack_file_pos;        /* file position used in acknowledgement of correctly */
                    /* received data subpackets */

  int last_sent;

  int n_cans;

  /* Stuff added by RRS */

  /* Status */
  BOOL      cancelled;
  BOOL      local_abort;
  BOOL      file_skipped;
  BOOL      no_streaming;
  BOOL      frame_in_transit;
  unsigned  recv_bufsize;  /* Receiver specified buffer size */
  int32_t   crc_request;
  unsigned  errors;
  unsigned  consecutive_errors;

  /* Configuration */
  BOOL      escape_telnet_iac;
  unsigned  init_timeout;
  unsigned  send_timeout;
  unsigned  recv_timeout;
  unsigned  crc_timeout;
  unsigned  max_errors;
  unsigned  block_size;
  unsigned  max_block_size;
  int64_t   max_file_size;    /* 0 = unlimited */
  int       *log_level;
  /* error C2520: conversion from unsigned __int64 to double not implemented, use signed __int64 */
  void*    cbdata;
} zmodem_t;

class ZModem
{
public:
  ZModem(FS *zfs, void* cbdata);
  ~ZModem();
  BOOL send_file( char* name, File* fp, BOOL request_init, time_t* start, uint64_t* bytes_sent);
  int get_zfin();
  int recv_init();
  int lputs(void* unused, int level, const char* str);
  int lprintf(int level, const char *fmt, ...);
  int send_zabort();
  int send_zfin();
  int recv_files(const char* download_dir, uint64_t* bytes_received);
  unsigned recv_file_data( File*, int64_t offset);
  zmodem_t *zm=0;
  ZSerial zserial;
  FS *zfileSystem=0;
private:
  char* ver(char *buf);
  const char* source(void);
  int rx();
  int tx(BYTE ch);
  int send_ack( int32_t pos);
  int send_nak();
  int send_zskip();
  int send_zrinit();
  int send_pos_header(int type, int32_t pos, BOOL hex);
  int get_zrinit();
  BOOL get_crc( int32_t length, uint32_t* crc);
  void parse_zrinit();
  void parse_zfile_subpacket();
  int recv_file_frame(File* fp);
  int recv_header_and_check();
  int send_hex(uchar val);
  int send_padded_zdle();
  int send_hex_header(unsigned char * p);
  int send_bin32_header(unsigned char * p);
  int send_bin16_header(unsigned char * p);
  int send_bin_header(unsigned char * p);
  int send_data32(uchar subpkt_type, unsigned char * p, size_t l);
  int send_data16(uchar subpkt_type,unsigned char * p, size_t l);
  int send_data(uchar subpkt_type, unsigned char * p, size_t l);
  int send_data_subpkt(uchar subpkt_type, unsigned char * p, size_t l);
  int data_waiting(unsigned timeout);
  void recv_purge();
  void flush();
  int send_raw(unsigned char ch);
  int send_esc(unsigned char c);
  int recv_data32(unsigned char * p, unsigned maxlen, unsigned* l);
  int recv_data16(register unsigned char* p, unsigned maxlen, unsigned* l);
  int recv_data(unsigned char* p, size_t maxlen, unsigned* l, BOOL ack);
  BOOL recv_subpacket(BOOL ack);
  int recv_nibble();
  int recv_hex();
  int recv_raw();
  BOOL recv_bin16_header();
  BOOL recv_hex_header();
  BOOL recv_bin32_header();
  int recv_header_raw(int errors);
  int recv_header();
  BOOL request_crc(int32_t length);
  BOOL recv_crc(uint32_t* crc);
  BOOL handle_zrpos(uint64_t* pos);
  BOOL handle_zack();
  BOOL is_connected();
  BOOL is_cancelled();
  int send_from(File* fp, uint64_t pos, uint64_t* sent);
  int send_znak();
  int send_zeof(uint32_t pos);
  void progress(void* cbdata, int64_t current_pos);
  int send_byte(void* unused, uchar ch, unsigned timeout);
  int recv_byte(void* unused, unsigned timeout); /* seconds */
  ulong frame_pos(int type);
  char* getfname(const char* path);
};

static ZModem *initZSerial(FS &fs, FlowControlType commandFlow)
{
  ZModem *modem = new ZModem(&SD, NULL);
  modem->zserial.setFlowControlType(FCT_DISABLED);
  if(commandFlow==FCT_RTSCTS)
    modem->zserial.setFlowControlType(FCT_RTSCTS);
  else
    modem->zserial.setFlowControlType(FCT_NORMAL);
  modem->zserial.setPetsciiMode(false);
  modem->zserial.setXON(true);
  return modem;
}

static bool zDownload(FlowControlType flow, FS &fs, String filePath, String &errors)
{
  time_t starttime = 0;
  uint64_t bytes_sent=0;
  BOOL success=ZFALSE;
  char filePathC[MAX_PATH];
  File F;

  ZModem *modem = initZSerial(fs, flow);

  //static int send_files(char** fname, uint fnames)
  F=modem->zfileSystem->open(filePath);
  modem->zm->files_remaining = 1;
  modem->zm->bytes_remaining = F.size();
  strcpy(filePathC,filePath.c_str());
  success=modem->send_file(filePathC, &F, ZTRUE, &starttime, &bytes_sent);
  if(success)
    modem->get_zfin();
  F.close();

  modem->zserial.flushAlways();
  delete modem;
  return (success==ZTRUE) && (modem->zm->cancelled==ZFALSE);
}

static bool zUpload(FlowControlType flow, FS &fs, String dirPath, String &errors)
{
  BOOL success=ZFALSE;
  int   i;
  char str[MAX_PATH];
  File fp;
  int err;

  ZModem *modem = initZSerial(fs,flow);

  //static int receive_files(char** fname_list, int fnames)
  //TODO: loop might be necc around here, for multiple files?
  i=modem->recv_init();
  if(modem->zm->cancelled || (i<0))
  {
    delete modem;
    return ZFALSE;
  }
  switch(i) {
    case ZFILE:
      //SAFECOPY(fname,zm.current_file_name);
      //file_bytes = zm.current_file_size;
      //ftime = zm.current_file_time;
      //total_files = zm.files_remaining;
      //total_bytes = zm.bytes_remaining;
      break;
    case ZFIN:
    case ZCOMPL:
      delete modem;
      return ZTRUE; // was (!success)
    default:
      delete modem;
      return ZFALSE;
  }

  strcpy(str,dirPath.c_str());
  if(str[strlen(str)-1]!='/')
  {
    str[strlen(str)]='/';
    str[strlen(str)+1]=0;
  }
  strcpy(str+strlen(str),modem->zm->current_file_name);

  fp = modem->zfileSystem->open(str,FILE_WRITE);
  if(!fp)
  {
    modem->lprintf(LOG_ERR,"Error %d creating %s",errno,str);
    modem->send_zabort();
    //zmodem_send_zskip(); //TODO: for when we move to multiple files
    //continue;
    delete modem;
    return ZFALSE;
  }
  err=modem->recv_file_data(&fp,0);

  if(err<=modem->zm->max_errors && !modem->zm->cancelled)
    success=ZTRUE;

  if(success)
    modem->send_zfin();

  fp.close();
  if(modem->zm->local_abort)
  {
    modem->lprintf(LOG_ERR,"Locally aborted, sending cancel to remote");
    modem->send_zabort();
    delete modem;
    return ZFALSE;
  }

  modem->zserial.flushAlways();
  delete modem;
  return (success == ZTRUE);
}
#endif
