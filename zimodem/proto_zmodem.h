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

#ifdef REMOVEME

#ifndef __ZMODEM_H__
#define __ZMODEM_H__

#include <FS.h>
#ifndef FILE_READ
# define FILE_READ "r"
#endif
#ifndef FILE_WRITE
# define FILE_WRITE "w"
#endif
#ifndef FILE_APPEND
# define FILE_APPEND "a"
#endif

/* Defines ---------------------------------------------------------------- */

/**
 * The flavors of Zmodem that are supported.
 */
typedef enum
{
  ZMODEM_FLAVOR_CRC16,                    /* Zmodem 16-bit CRC */
  ZMODEM_FLAVOR_CRC32                     /* Zmodem 32-bit CRC */
} ZMODEM_FLAVOR;


/*
 * Technically, Zmodem maxes at 1024 bytes, but each byte might be
 * CRC-escaped to twice its size. Then we've got the CRC escape itself to
 * include.
 */
#define ZMODEM_BLOCK_SIZE       1024
#define ZMODEM_MAX_BLOCK_SIZE   (2 * (ZMODEM_BLOCK_SIZE + 4 + 1))

/*
 * Require an ACK every 32 frames on reliable links.
 */
#define ZMODEM_WINDOW_SIZE_RELIABLE 32
/*
 * Require an ACK every 4 frames on unreliable links.
 */
#define ZMODEM_WINDOW_SIZE_UNRELIABLE 4

/* Data types ----------------------------------------------- */

/* Used to note the start of a packet */
#define ZMODEM_DT_ZPAD                    '*'
/* CRC next, frame ends, header packet follows */
#define ZMODEM_DT_ZCRCE                   'h'
/* CRC next, frame continues nonstop */
#define ZMODEM_DT_ZCRCG                   'i'
/* CRC next, frame continues, ZACK expected */
#define ZMODEM_DT_ZCRCQ                   'j'
/* CRC next, ZACK expected, end of frame */
#define ZMODEM_DT_ZCRCW                   'k'
/* Packet types */
#define ZMODEM_PKT_ZRQINIT               0
#define ZMODEM_PKT_ZRINIT                1
#define ZMODEM_PKT_ZSINIT                2
#define ZMODEM_PKT_ZACK                  3
#define ZMODEM_PKT_ZFILE                 4
#define ZMODEM_PKT_ZSKIP                 5
#define ZMODEM_PKT_ZNAK                  6
#define ZMODEM_PKT_ZABORT                7
#define ZMODEM_PKT_ZFIN                  8
#define ZMODEM_PKT_ZRPOS                 9
#define ZMODEM_PKT_ZDATA                 10
#define ZMODEM_PKT_ZEOF                  11
#define ZMODEM_PKT_ZFERR                 12
#define ZMODEM_PKT_ZCRC                  13
#define ZMODEM_PKT_ZCHALLENGE            14
#define ZMODEM_PKT_ZCOMPL                15
#define ZMODEM_PKT_ZCAN                  16
#define ZMODEM_PKT_ZFREECNT              17
#define ZMODEM_PKT_ZCOMMAND              18

#define ZMODEM_CRC32 0xedb88320        /* CRC polynomial */

/* Globals ---------------------------------------------------------------- */
/* Transfer capabilities sent in ZRInit packet */
/* Rx can send and receive true FDX */
#define ZMODEM_TX_CAN_FULL_DUPLEX      0x00000001
/* Rx can receive data during disk I/O */
#define ZMODEM_TX_CAN_OVERLAP_IO       0x00000002
/* Rx can send a break signal */
#define ZMODEM_TX_CAN_BREAK            0x00000004
/* Receiver can decrypt */
#define ZMODEM_TX_CAN_DECRYPT          0x00000008
/* Receiver can uncompress */
#define ZMODEM_TX_CAN_LZW              0x00000010
/* Receiver can use 32 bit Frame Check */
#define ZMODEM_TX_CAN_CRC32            0x00000020
/* Receiver expects ctl chars to be escaped */
#define ZMODEM_TX_ESCAPE_CTRL          0x00000040
/* Receiver expects 8th bit to be escaped */
#define ZMODEM_TX_ESCAPE_8BIT          0x00000080

/**
 * The Zmodem protocol state that can encompass multiple file transfers.
 */
typedef enum
{
  /* Before the first byte is sent */
  ZMODEM_STATE_INIT,
  /* Transfer complete */
  ZMODEM_STATE_COMPLETE,
  /* Transfer was aborted due to excessive timeouts or ZCAN */
  ZMODEM_STATE_ABORT,
  /* Collecting data for a ZFILE, ZSINIT, ZDATA, and ZCOMMAND packet. */
  ZMODEM_STATE_ZDATA,
  /*
   * Receiver side
   */
  /* Send ZRINIT */
  ZMODEM_STATE_ZRINIT,
  /* Waiting for ZFILE or ZSINIT */
  ZMODEM_STATE_ZRINIT_WAIT,
  /* Send ZCHALLENGE */
  ZMODEM_STATE_ZCHALLENGE,
  /* Waiting for ZACK */
  ZMODEM_STATE_ZCHALLENGE_WAIT,
  /* Send ZRPOS */
  ZMODEM_STATE_ZRPOS,
  /* Waiting for ZDATA */
  ZMODEM_STATE_ZRPOS_WAIT,
  /* Send ZSKIP */
  ZMODEM_STATE_ZSKIP,
  /* Send ZCRC */
  ZMODEM_STATE_ZCRC,
  /* Waiting for ZCRC */
  ZMODEM_STATE_ZCRC_WAIT,
  /*
   * Sender side
   */
  /* Send ZRQINIT */
  ZMODEM_STATE_ZRQINIT,
  /* Waiting for ZRINIT or ZCHALLENGE */
  ZMODEM_STATE_ZRQINIT_WAIT,
  /* Send ZSINIT */
  ZMODEM_STATE_ZSINIT,
  /* Waiting for ZACK */
  ZMODEM_STATE_ZSINIT_WAIT,
  /* Send ZFILE */
  ZMODEM_STATE_ZFILE,
  /* Waiting for ZSKIP, ZCRC, or ZRPOS */
  ZMODEM_STATE_ZFILE_WAIT,
  /* Send ZEOF */
  ZMODEM_STATE_ZEOF,
  /* Waiting for ZRPOS */
  ZMODEM_STATE_ZEOF_WAIT,
  /* Send ZFIN */
  ZMODEM_STATE_ZFIN,
  /* Waiting for ZFIN */
  ZMODEM_STATE_ZFIN_WAIT
} ZMODEM_STATE;

/* Every bit of Zmodem data goes out as packets */
struct zmodem_packet
{
  int type;
  uint32_t argument;
  bool use_crc32;
  int crc16;
  uint32_t crc32;
  unsigned char data[ZMODEM_MAX_BLOCK_SIZE];
  unsigned int data_n;

  /*
   * Performance tweak for decode_zdata_bytes to allow it to quickly bail
   * out during CRC check.
   */
  unsigned char crc_buffer[5];
};

/* Return codes from parse_packet() */
typedef enum
{
  ZM_PP_INVALID,
  ZM_PP_NODATA,
  ZM_PP_CRCERROR,
  ZM_PP_OK
} ZMODEM_PARSE_PACKET;

#define ZMODEM_HEX_PACKET_LENGTH       20

#define big_to_little_endian(X) (((X >> 24) & 0xFF) | \
                                ((X >> 8) & 0xFF00) | \
                                ((X << 8) & 0xFF0000) | \
                                ((X << 24) & 0xFF000000))

#endif /* __ZMODEM_H__ */

class ZModem
{
private:
  FS *fs = null;
  Stream *mdmIn = null;
  ZSerial *mdmOt = null;

  /* Set this to true to enable debug log. */
  bool debugOn = true;

  /* ZMODEM_STATE_INIT, COMPLETE, ABORT, etc. */
  ZMODEM_STATE state = ZMODEM_STATE_INIT;
  /* State before entering DATA state */
  ZMODEM_STATE prior_state = ZMODEM_STATE_INIT;
  /* Send/receive flags */
  unsigned long flags =0;
  /* If true, use 32-bit CRC */
  bool use_crc32 = true;
  /* If true, we are the sender */
  bool sending = false;
  /* Current filename being sent/received */
  char * file_name = null;
  /* Size of file in bytes */
  unsigned int file_size = 0;
  /* Modification time of file */
  time_t file_modtime = 0;
  /* Current position */
  off_t file_position = 0;
  /* Stream pointer to current file */
  File file_stream;
  /* File CRC32 */
  uint32_t file_crc32 = -1;
  /* Block size */
  int block_size = ZMODEM_BLOCK_SIZE;
  /* If true, sent block will ask for ZACK */
  bool ack_required = false;
  /* If true, we are waiting to hear ZACK */
  bool waiting_for_ack = false;
  /*
   * If true, we are continuously streaming the ZDATA "data subpacket" and
   * will not need to generate a new packet header.
   */
  bool streaming_zdata = false;
  /* Timeout normally lasts 10 seconds */
  int timeout_length = 10;
  /* The beginning time for the most recent timeout cycle */
  time_t timeout_begin = 0;
  /* Total number of timeouts before aborting is 5 */
  int timeout_max = 5;
  /* Total number of timeouts so far */
  int timeout_count = 0;
  /* Number of bytes confirmed from the receiver */
  int confirmed_bytes = 0;
  /*
   * Number of bytes confirmed from the receiver when we dropped the block
   * size.
   */
  int last_confirmed_bytes = 0;
  /* True means TCP/IP or error-correcting modem */
  bool reliable_link = true;
  /* File position when the block size was last reduced */
  off_t file_position_downgrade = 0;
  /* When 0, require a ZACK, controls window size */
  unsigned blocks_ack_count = 0;
  /* Number of error blocks */
  int consecutive_errors = 0;
  /* Full pathname to file */
  char file_fullname[FILENAME_SIZE];

  /* The list of files to upload */
  File upload_file_list[];
  /* The current entry in upload_file_list being sent */
  int upload_file_list_i;
  /*
   * The path to download to.  Note download_path is Xstrdup'd TWICE: once HERE
   * and once more on the progress dialog.  The q_program_state transition to
   * Q_STATE_CONSOLE is what Xfree's the copy in the progress dialog.  This
   * copy is Xfree'd in zmodem_stop().
   */
  char *download_path = null;

  /* Needs to persist across calls to zmodem() */
  struct zmodem_packet packet;

  /* Internal buffer used to collect a complete packet before processing it */
  unsigned char packet_buffer[ZMODEM_MAX_BLOCK_SIZE];
  unsigned int packet_buffer_n;

  /*
   * Internal buffer used to queue a complete outbound packet so that the
   * top-level code can saturate the link.
   */
  unsigned char outbound_packet[ZMODEM_MAX_BLOCK_SIZE];
  unsigned int outbound_packet_n;

  /* The ZMODEM_STATE_ZCHALLENGE value we asked for */
  uint32_t zchallenge_value;

  /**
   * encode_byte is a simple lookup into this map.
   */
  unsigned char encode_byte_map[256];

  uint32_t crc_32_tab[256];

public:
  ZModem(FS &fs, Stream &modemIn, ZSerial &modemOut);

  /* Functions -------------------------------------------------------------- */

  /**
   * Process raw bytes from the remote side through the transfer protocol.  See
   * also protocol_process_data().
   *
   * @param input the bytes from the remote side
   * @param input_n the number of bytes in input_n
   * @param output a buffer to contain the bytes to send to the remote side
   * @param output_n the number of bytes that this function wrote to output
   * @param output_max the maximum number of bytes this function may write to
   * output
   */
  void zmodem_process(unsigned char * input, const unsigned int input_n,
                   unsigned char * output, unsigned int * output_n,
                   const unsigned int output_max);

  /**
   * Setup for a new file transfer session.
   *
   * @param file_list list of files to upload, or NULL if this will be a
   * download.
   * @param pathname the path to save downloaded files to
   * @param send if true, this is an upload: file_list must be valid and
   * pathname is ignored.  If false, this is a download: file_list must be NULL
   * and pathname will be used.
   * @param in_flavor the type of Zmodem transfer to perform
   * @return true if successful
   */
  bool zmodem_start(File file_list[], const char * pathname,
                     const bool send, const ZMODEM_FLAVOR in_flavor);

  /**
   * Stop the file transfer.  Note that this function is only called in
   * stop_file_transfer() and save_partial is always true.  However it is left
   * in for API completeness.
   *
   * @param save_partial if true, save any partially-downloaded files.
   */
  void zmodem_stop(const bool save_partial);

  String getLastErrors();
};



#endif

static ZSerial zserial;

static boolean zDownload(FS &fs, String filePath, String &errors)
{
  bool result=false;
  /*
  ZModem zmo(fs,HWSerial,zserial);
  File files[2];
  files[0] = fs->open(filePath);
  files[1] = null;
  result = zmo.zmodem_start(files,"/",true,ZMODEM_FLAVOR_CRC16);
  zserial.flushAlways();
  if(!result)
    errors = zmo.getLastErrors();
  */
  return result;
}

static boolean zUpload(FS &fs, String dirPath, String &errors)
{
  bool result=false;
  /*
  ZModem zmo(fs,HWSerial,zserial);
  result = zmo.zmodem_start(null,dirPath,false,ZMODEM_FLAVOR_CRC16);
  zserial.flushAlways();
  if(!result)
    errors = zmo.getLastErrors();
  */
  return result;
}

static void initZSerial(FlowControlType commandFlow)
{
  zserial.setFlowControlType(FCT_DISABLED);
  if(commandFlow==FCT_RTSCTS)
    zserial.setFlowControlType(FCT_RTSCTS);
  else
    zserial.setFlowControlType(FCT_NORMAL);
  zserial.setPetsciiMode(false);
  zserial.setXON(true);
}

