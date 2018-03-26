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

ZModem::ZModem(FS &filesys, Stream &modemIn, ZSerial &modemOut)
{
  fs = &filesys;
  mdmIn = &modemIn;
  mdmOt = &modemOut;
}

String ZModem::getLastErrors()
{
  return "";
}

/* CRC16 CODE ------------------------------------------------------------- */

/*
 * KAL - This CRC16 routine was taken verbatim from XYMODEM.DOC.
 *
 * This function calculates the CRC used by the XMODEM/CRC Protocol
 * The first argument is a pointer to the message block.
 * The second argument is the number of bytes in the message block.
 * The function returns an integer which contains the CRC.
 * The low order 16 bits are the coefficients of the CRC.
 */
int ZModem::compute_crc16(int crc, const unsigned char *ptr, int count) 
{
  int i;

  while (--count >= 0) 
  {
    crc = crc ^ ((int) *ptr++ << 8);
    for (i = 0; i < 8; ++i)
    {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc = crc << 1;
    }
  }
  return (crc & 0xFFFF);
}

/* CRC32 CODE ------------------------------------------------------------- */

/*
 * The following CRC32 code was posted by Colin Plumb in
 * comp.os.linux.development.system.  Google link:
 * http://groups.google.com/groups?hl=en&lr=&ie=UTF-8&selm=4dr0ab%24o1k%40nyx10.cs.du.edu
 */

/*
 * This uses the CRC-32 from IEEE 802 and the FDDI MAC,
 * x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.
 *
 * This is, for a slight efficiency win, used in a little-endian bit
 * order, where the least significant bit of the accumulator (and each
 * byte of input) corresponds to the highest exponent of x.
 * E.g. the byte 0x23 is interpreted as x^7+x^6+x^2.  For the
 * most rational output, the computed 32-bit CRC word should be
 * sent with the low byte (which are the most significant coefficients
 * from the polynomial) first.  If you do this, the CRC of a buffer
 * that includes a trailing CRC will always be zero.  Or, if you
 * use a trailing invert variation, some fixed value.  For this
 * polynomial, that fixed value is 0x2144df1c.  (Leading presets,
 * be they to 0, -1, or something else, don't matter.)
 *
 * Thus, the little-endian hex constant is as follows:
 *           11111111112222222222333
 * 012345678901234567890123456789012
 * 111011011011100010000011001000001
 * \  /\  /\  /\  /\  /\  /\  /\  /\
 *   E   D   B   8   8   3   2   0
 *
 * This technique, while a bit confusing, is widely used in e.g. Zmodem.
 */


/*
 * This computes the CRC table quite efficiently, using the fact that
 * crc_32_tab[i^j] = crc_32_tab[i] ^ crc_32_tab[j].  We start out with
 * crc_32_tab[0] = 0, j = 128, and then set h to the desired value of
 * crc_32_tab[j].  Then for each crc_32_tab[i] which is already set (which
 * includes i = 0, so crc_32_tab[j] will get set), set crc_32_tab[i^j] =
 * crc_32_tab[i] ^ h.
 * Then divide j by 2 and repeat until everything is filled in.
 * The first pass sets crc_32_tab[128].  The second sets crc_32_tab[64]
 * and crc_32_tab[192].  The third sets entries 32, 96, 160 and 224.
 * The eighth and last pass sets all the odd-numbered entries.
 */
void ZModem::makecrc(void) 
{
  int i, j = 128;
  uint32_t h = 1;

  crc_32_tab[0] = 0;
  do 
  {
    if (h & 1)
      h = (h >> 1) ^ CRC32;
    else
      h >>= 1;
    for (i = 0; i < 256; i += j + j)
      crc_32_tab[i + j] = crc_32_tab[i] ^ h;
  } 
  while (j >>= 1);
}

/*
 * Compute a CRC on the given buffer and length using a static CRC
 * accumulator.  If buf is NULL this initializes the accumulator,
 * otherwise it updates it to include the additional data and
 * returns the CRC of the data so far.
 *
 * The CRC is computed using preset to -1 and invert.
 */
uint32_t ZModem::compute_crc32(const uint32_t old_crc, const unsigned char *buf,
                              unsigned len) {
    uint32_t crc;

    if (buf) 
    {
      crc = old_crc;
      while (len--)
        crc = (crc >> 8) ^ crc_32_tab[(crc ^ *buf++) & 0xff];
      return crc ^ 0xffffffff;        /* Invert */
    } 
    else
      return 0xffffffff;      /* Preset to -1 */
}

/* CRC32 CODE ------------------------------------------------------------- */

/* ------------------------------------------------------------------------ */
/* Block size adjustment logic -------------------------------------------- */
/* ------------------------------------------------------------------------ */

/**
 * Move up to a larger block size if things are going better.
 */
void ZModem::block_size_up() 
{
# ifdef ZMODEM_DEBUG
    debugPrintf("block_size_up(): block_size = %d\n", block_size);
# endif

  /*
   * After getting a clean bill of health for 8k, move block size up
   */
  if ((confirmed_bytes - file_position_downgrade) > 8196) 
  {
    block_size *= 2;
    if (block_size > ZMODEM_BLOCK_SIZE)
        block_size = ZMODEM_BLOCK_SIZE;
  }
  last_confirmed_bytes = confirmed_bytes;

# ifdef ZMODEM_DEBUG
    debugPrintf("block_size_up(): NEW block size = %d\n", block_size);
# endif
}

/**
 * Move down to a smaller block size if things are going badly.
 */
void ZModem::block_size_down() 
{
  int outstanding_packets =
    (confirmed_bytes - last_confirmed_bytes) /
        block_size;

# ifdef ZMODEM_DEBUG
  debugPrintf("block_size_down(): block_size = %d outstanding_packets = %d\n",
      block_size, outstanding_packets);
# endif

  if (outstanding_packets >= 3) 
  {
    if (block_size > 32) 
    {
      block_size /= 2;
      file_position_downgrade = confirmed_bytes;
    }
  }
  if (outstanding_packets >= 10) 
  {
    if (block_size == 32) 
    {
      /*
       * Too much line noise, give up
       */
      state = ZMODEM_STATE_ABORT;
    }
  }
  blocks_ack_count = ZMODEM_WINDOW_SIZE_UNRELIABLE;
  last_confirmed_bytes = confirmed_bytes;

# ifdef ZMODEM_DEBUG
    debugPrintf("block_size_down(): NEW block size = %d\n", block_size);
# endif

}

/* ------------------------------------------------------------------------ */
/* Progress dialog -------------------------------------------------------- */
/* ------------------------------------------------------------------------ */

/**
 * Statistics: reset for a new file.
 *
 * @param filename the file being transferred
 * @param filesize the size of the file (known from either file attributes or
 * because this is an upload)
 */
void ZModem::stats_new_file(const char * filename, const int filesize) 
{
  confirmed_bytes = 0;
  last_confirmed_bytes = 0;
}

/**
 * Statistics: increment the block count.
 */
void ZModem::stats_increment_blocks() 
{
  consecutive_errors = 0;

# ifdef ZMODEM_DEBUG
    debugPrintf("stats_increment_blocks(): waiting_for_ack = %s ack_required = %s reliable_link = %s confirmed_bytes = %u last_confirmed_bytes = %u block_size = %d blocks_ack_count = %d\n",
        (waiting_for_ack == true ? "true" : "false"),
        (ack_required == true ? "true" : "false"),
        (reliable_link == true ? "true" : "false"),
        confirmed_bytes, last_confirmed_bytes,
        block_size, blocks_ack_count);
# endif

}

/**
 * Statistics: an error was encountered.
 */
void ZModem::stats_increment_errors() 
{
  consecutive_errors++;

  /*
   * Unreliable link is a one-way ticket until the next call to
   * zmodem_start() .
   */
  reliable_link = false;

# ifdef ZMODEM_DEBUG
    debugPrintf("stats_increment_errors(): waiting_for_ack = %s ack_required = %s reliable_link = %s confirmed_bytes = %u last_confirmed_bytes = %u block_size = %d blocks_ack_count = %d\n",
        (waiting_for_ack == true ? "true" : "false"),
        (ack_required == true ? "true" : "false"),
        (reliable_link == true ? "true" : "false"),
        confirmed_bytes, last_confirmed_bytes,
        block_size, blocks_ack_count);
# endif
    
    /*
     * If too many errors when not in ZDATA, the other end is probably not
     * even running zmodem.  Bail out.
     */
  if ((consecutive_errors >= 15) && (state != ZMODEM_STATE_ZDATA))
    state = ZMODEM_STATE_ABORT;
}

/**
 * Initialize a new file to upload.
 *
 * @return true if OK, false if the file could not be opened
 */
bool ZModem::setup_for_next_file() 
{
  char * basename_arg;

  /*
   * Reset our dynamic variables
   */
  if (file_stream != NULL) {
      file_stream.close();
  }
  file_stream = NULL;
  if (file_name != NULL) {
      free(file_name);
  }
  file_name = NULL;

  if (upload_file_list[upload_file_list_i] == NULL) 
  {
      /*
       * Special case: the termination/finish packet
       */
#   ifdef ZMODEM_DEBUG
      debugPrintf("ZMODEM: No more files (name='%s')\n",
            upload_file_list[upload_file_list_i].name().c_str());
#   endif
      /*
       * We're done
       */
      state = ZMODEM_STATE_ZFIN;
      return true;
  }

  /*
   * Get the file's modification time
   */
  file_modtime = 0;//upload_file_list[upload_file_list_i].fstats.st_mtime;
  file_size = upload_file_list[upload_file_list_i].size();

  /*
   * Open the file
   */
  if (!(file_stream = upload_file_list[upload_file_list_i]))
  {
#   ifdef ZMODEM_DEBUG
      debugPrintf("ERROR: Unable to open file %s: %s (%d)\n",
            upload_file_list[upload_file_list_i].name().c_str(), strerror(errno),
            errno);
#   endif

    state = ZMODEM_STATE_ABORT;
    return false;
  }

  /*
   * Note that basename and dirname modify the arguments
   */
  basename_arg = strdup(upload_file_list[upload_file_list_i].name().c_str());
  if (file_name != NULL) {
    free(file_name);
  }
  file_name = strdup(basename(basename_arg));

  /*
   * Update the stats
   */
  stats_new_file(upload_file_list[upload_file_list_i].name().c_str(),
                 upload_file_list[upload_file_list_i].size());

  /*
   * Free the copies passed to basename() and dirname()
   */
  free(basename_arg);

# ifdef ZMODEM_DEBUG
    debugPrintf("UPLOAD set up for new file %s (%lu bytes)...\n",
        upload_file_list[upload_file_list_i].name().c_str(),
        (long int) upload_file_list[upload_file_list_i].size());
# endif

  /*
   * Update stuff if this is the second file
   */
  if (state != ZMODEM_STATE_ABORT) 
  {
    /*
     * We need to send ZMODEM_STATE_ZFILE now
     */
    state = ZMODEM_STATE_ZFILE;
  }
  return true;
}

/**
 * Reset the timeout counter.
 */
void ZModem::reset_timer() 
{
  time(&timeout_begin);
}

/**
 * Check for a timeout.
 *
 * @return true if a timeout has occurred
 */
bool ZModem::check_timeout() 
{
  time_t now;
  time(&now);

# ifdef ZMODEM_DEBUG
    debugPrintf("check_timeout()\n");
# endif
  if (now - timeout_begin >= timeout_length) 
  {
    /*
     * Timeout
     */
    timeout_count++;
#   ifdef ZMODEM_DEBUG
      debugPrintf("ZMODEM: Timeout #%d\n", timeout_count);
#   endif

    if (timeout_count >= timeout_max) 
    {
      stats_increment_errors();
      state = ZMODEM_STATE_ABORT;
    } 
    else 
    {
      stats_increment_errors();
    }

    /*
     * Reset timeout
     */
    reset_timer();
    return true;
  }

  /*
   * No timeout yet
   */
  return false;
}

/* ------------------------------------------------------------------------ */
/* Encoding layer --------------------------------------------------------- */
/* ------------------------------------------------------------------------ */

/**
 * Turn a string into hex.
 *
 * @param input a buffer with 8-bit bytes
 * @param input_n the number of bytes in input
 * @param output the output string of hexademical ASCII characters
 * @param output_max the maximum number of bytes that can be written to
 * output
 */
void ZModem::hexify_string(const unsigned char * input,
                          const unsigned int input_n,
                          unsigned char * output,
                          const unsigned int output_max) 
{
  char digits[] = "0123456789abcdefg";
  unsigned int i;
  assert(output_max >= input_n * 2);
  for (i = 0; i < input_n; i++) 
  {
    output[2 * i] = digits[(input[i] & 0xF0) >> 4];
    output[2 * i + 1] = digits[input[i] & 0x0F];
  }
}

/**
 * Turn a hex string into binary.
 *
 * @param input a buffer with hexadecimal ASCII characters
 * @param input_n the number of bytes in input
 * @param output the output buffer of 8-bit bytes
 * @param output_max the maximum number of bytes that can be written to
 * output
 * @return true if conversion was successful, false if input is not a valid
 * hex string
 */
bool ZModem::dehexify_string(const unsigned char * input,
                              const unsigned int input_n,
                              unsigned char * output,
                              const unsigned int output_max) {

  unsigned int i;

  assert(output_max >= input_n / 2);

  for (i = 0; i < input_n; i++) 
  {
    int ch = tolower(input[i]);
    int j = i / 2;

    if ((ch >= '0') && (ch <= '9'))
      output[j] = ch - '0';
    else 
    if ((ch >= 'a') && (ch <= 'f'))
      output[j] = ch - 'a' + 0x0A;
    else 
    {
      /*
       * Invalid hex string
       */
      return false;
    }
    output[j] = output[j] << 4;

    i++;
    ch = tolower(input[i]);

    if ((ch >= '0') && (ch <= '9'))
        output[j] |= ch - '0';
    else 
    if ((ch >= 'a') && (ch <= 'f'))
        output[j] |= ch - 'a' + 0x0A;
    else 
    {
      /*
       * Invalid hex string
       */
      return false;
    }
  }

  /*
   * All OK
   */
  return true;
}

/* ------------------------------------------------------------------------ */
/* Bytes layer ------------------------------------------------------------ */
/* ------------------------------------------------------------------------ */

/**
 * Turn escaped ZDATA bytes into regular bytes, copying to output.
 *
 * The CRC escape sequence bytes are copied to crc_buffer which must be at
 * least 5 bytes long:  1 byte for CRC escape character, 4 bytes for CRC
 * data.
 *
 * The input buffer will be shifted down to handle multiple packets streaming
 * together.
 *
 * @param input the encoded bytes from the remote side
 * @param input_n the number of bytes in input_n
 * @param output a buffer to contain the decoded bytes
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @param crc_buffer a buffer to contain the CRC bytes
 */
bool ZModem::decode_zdata_bytes(unsigned char * input,
                                 unsigned int * input_n,
                                 unsigned char * output,
                                 unsigned int * output_n,
                                 const unsigned int output_max,
                                 unsigned char * crc_buffer) 
{

  int i;                      /* input iterator */
  int j;                      /* for doing_crc case */
  bool doing_crc = false;
  bool done = false;
  unsigned char crc_type = 0;

# ifdef ZMODEM_DEBUG
  {
    debugPrintf("decode_zdata_bytes(): input_n = %d output_n = %d output_max = %d data: ",
        *input_n, *output_n, output_max);
    for (i = 0; i < *input_n; i++) {
      debugPrintf("%02x ", (input[i] & 0xFF));
    }
    debugPrintf("\n");
  }
# endif

  /*
   * Worst-case scenario:  input is twice the output size
   */
  assert((output_max * 2) >= (*input_n));

  /*
   * We need to quickly scan the input and bail out if it too short.
   *
   * The first check is to look for a CRC escape of some kind, if that is
   * missing we are done.
   */
  for (i = 0; (i < *input_n) && (done == false); i++) 
  {
    if (input[i] == C_CAN) 
    {
      /*
       * Point past the CAN
       */
      i++;
      if (i == *input_n) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("decode_zdata_bytes: incomplete (C_CAN)\n");
#       endif
        return false;
      }
      switch (input[i]) 
      {
      case ZMODEM_DT_ZCRCE:
      case ZMODEM_DT_ZCRCG:
      case ZMODEM_DT_ZCRCQ:
      case ZMODEM_DT_ZCRCW:
          /*
           * The CRC escape is here, we can run the big loop
           */
          goto decode_zdata_bytes_big_loop;
      }
    }
  }

  /*
   * The CRC escape is missing, so we need to bail out now.
   */
# ifdef ZMODEM_DEBUG
   debugPrintf("decode_zdata_bytes: incomplete (no CRC escape)\n");
# endif
  return false;

decode_zdata_bytes_big_loop:

  *output_n = 0;
  j = 0;

  for (i = 0; (i < *input_n) && (done == false); i++) 
  {
    if (input[i] == C_CAN) 
    {
      /*
       * Point past the CAN
       */
      i++;

      if (i == *input_n) 
      {
        /*
         * Uh-oh, missing last byte
         */
#       ifdef ZMODEM_DEBUG
          debugPrintf("decode_zdata_bytes: incomplete (C_CAN)\n");
#       endif
        return false;
      }
      if (input[i] == ZMODEM_DT_ZCRCE) 
      {
        if (doing_crc == true) 
        {
          /*
           * WOAH! CRC escape within a CRC escape
           */
          return false;
        }
        /*
         * CRC escape, switch to crc collection
         */
        doing_crc = true;
        crc_type = input[i];
        i--;
      } 
      else
      if (input[i] == ZMODEM_DT_ZCRCG) 
      {
        if (doing_crc == true) 
        {
          /*
           * WOAH! CRC escape within a CRC escape
           */
          return false;
        }

        /*
         * CRC escape, switch to crc collection
         */
        doing_crc = true;
        crc_type = input[i];
        i--;
      }
      else 
      if (input[i] == ZMODEM_DT_ZCRCQ) 
      {
        if (doing_crc == true) 
        {
          /*
           * WOAH! CRC escape within a CRC escape
           */
          return false;
        }
        /*
         * CRC escape, switch to crc collection
         */
        doing_crc = true;
        crc_type = input[i];
        i--;
      } 
      else 
      if (input[i] == ZMODEM_DT_ZCRCW) 
      {
        if (doing_crc == true) 
        {
          /*
           * WOAH! CRC escape within a CRC escape
           */
          return false;
        }
        /*
         * CRC escape, switch to crc collection
         */
        doing_crc = true;
        crc_type = input[i];
        i--;
      } 
      else 
      if (input[i] == 'l') 
      {
        /*
         * Escaped control character: 0x7f
         */
        if (doing_crc == true) 
        {
          crc_buffer[j] = 0x7F;
          j++;
        } 
        else 
        {
          output[*output_n] = 0x7F;
          *output_n = *output_n + 1;
        }
      } 
      else
      if (input[i] == 'm') 
      {
        /*
         * Escaped control character: 0xff
         */
        if (doing_crc == true) 
        {
          crc_buffer[j] = 0xFF;
          j++;
        } 
        else 
        {
          output[*output_n] = 0xFF;
          *output_n = *output_n + 1;
        }
      } 
      else
      if ((input[i] & 0x40) != 0) 
      {
        /*
         * Escaped control character: CAN m OR 0x40
         */
        if (doing_crc == true) 
        {
          crc_buffer[j] = input[i] & 0xBF;
          j++;
        } 
        else 
        {
          output[*output_n] = input[i] & 0xBF;
          *output_n = *output_n + 1;
        }
      } 
      else 
      if (input[i] == C_CAN) 
      {
        /*
         * Real CAN, cancel the transfer
         */
        state = ZMODEM_STATE_ABORT;
        return false;
      } 
      else 
      {
        /*
         * Should never get here
         */
      }
    } 
    else 
    {
      /*
       * If we're doing the CRC part, put the data elsewhere
       */
      if (doing_crc == true) 
      {
        crc_buffer[j] = input[i];
        j++;
      } 
      else 
      {
        /*
         * I ought to ignore any unencoded control characters when
         * encoding was requested at this point here.  However,
         * encoding control characters is broken anyway in lrzsz so I
         * won't bother with a further check.  If you want actually
         * reliable transfer over not-8-bit-clean links, use Kermit
         * instead.
         */
        output[*output_n] = input[i];
        *output_n = *output_n + 1;
      }
    }

    if (doing_crc == true) 
    {
      if ((packet.use_crc32 == true) && (j == 5)) 
      {
        /*
         * Done
         */
        done = true;
      }
      if ((packet.use_crc32 == false) && (j == 3)) 
      {
        /*
         * Done
         */
        done = true;
      }
    }
  } /* for (i=0; i<input_n; i++) */

# ifdef ZMODEM_DEBUG
    debugPrintf("decode_zdata_bytes(): i = %d j = %d done = %s\n", i, j,
        (done == true ? "true" : "false"));
# endif
  if (done == true) 
  {
    switch (crc_type) 
    {
    case ZMODEM_DT_ZCRCE:
      /*
       * CRC next, frame ends, header packet follows
       */
#     ifdef ZMODEM_DEBUG
        debugPrintf("decode_zdata_bytes(): ZMODEM_DT_ZCRCE\n");
#     endif
      break;
    case ZMODEM_DT_ZCRCG:
      /*
       * CRC next, frame continues nonstop
       */
#     ifdef ZMODEM_DEBUG
        debugPrintf("decode_zdata_bytes(): ZMODEM_DT_ZCRCG\n");
#     endif
      break;
    case ZMODEM_DT_ZCRCQ:
      /*
       * CRC next, frame continues, ZACK expected
       */
#     ifdef ZMODEM_DEBUG
        debugPrintf("decode_zdata_bytes(): ZMODEM_DT_ZCRCQ\n");
#     endif
      break;
    case ZMODEM_DT_ZCRCW:
      /*
       * CRC next, ZACK expected, end of frame
       */
#     ifdef ZMODEM_DEBUG
        debugPrintf("decode_zdata_bytes(): ZMODEM_DT_ZCRCW\n");
#     endif
      break;
    default:
#     ifdef ZMODEM_DEBUG
        debugPrintf("!!!! decode_zdata_bytes(): UNKNOWN CRC ESCAPE TYPE !!!!\n");
#     endif
      break;
    }
  }

  if (done == true) 
  {
    if (crc_type == ZMODEM_DT_ZCRCW) 
    {
      /*
       * ZMODEM_DT_ZCRCW is always followed by XON, so kill it
       */
      if (input[i] == C_XON)
          i++;
    }
    /*
     * Got a packet and it's CRC, shift input down
     */
    memmove(input, input + i, *input_n - i);
    *input_n -= i;

    /*
     * Return OK
     */
    return true;
  }

  /*
   * Reached the end of a packet before we got to the CRC value
   */
  return false;
}

//-------------------------------------------------------------------------------------------------DELME-----------------------------------
/**
 * Turn one byte into up to two escaped bytes, copying to output.
 *
 * The output buffer must be big enough to contain all the data.
 */
void ZModem::setup_encode_byte_map() 
{
  int ch;
  for (ch = 0; ch < 256; ch++) 
  {
    bool encode_char = false;
    /*
     * Oh boy, do we have another design flaw...  lrzsz does not allow
     * any regular characters to be encoded, so we cannot protect against
     * telnet, ssh, and rlogin sequences from breaking the link.
     */
    switch (ch) 
    {
      case C_CAN:
      case C_XON:
      case C_XOFF:
      case (C_XON | 0x80):
      case (C_XOFF | 0x80):
#if 0
        /*
         * lrzsz breaks if we try to escape extra characters
         */
      case 0x1D:             /* For telnet */
      case '~':              /* For ssh, rlogin */
#endif
        encode_char = true;
        break;
      default:
        if ((ch < 0x20) && (flags & ZMODEM_TX_ESCAPE_CTRL)) 
        {
          /*
           * 7bit control char, encode only if requested
           */
          encode_char = true;
        } 
        else
        if ((ch >= 0x80) && (ch < 0xA0)) 
        {
          /*
           * 8bit control char, always encode
           */
          encode_char = true;
        } 
        else 
        if (((ch & 0x80) != 0) && (flags & ZMODEM_TX_ESCAPE_8BIT)) 
        {
          /*
           * 8bit char, encode only if requested
           */
          encode_char = true;
        }
        break;
    }
    if (encode_char == true)
    {
      /*
       * Encode
       */
      encode_byte_map[ch] = ch | 0x40;
    } 
    else 
    if (ch == 0x7F)
    {
      /*
       * Escaped control character: 0x7f
       */
      encode_byte_map[ch] = 'l';
    } 
    else
    if (ch == 0xFF) 
    {
      /*
       * Escaped control character: 0xff
       */
      encode_byte_map[ch] = 'm';
    } 
    else 
    {
      /*
       * Regular character
       */
      encode_byte_map[ch] = ch;
    }
  }
# ifdef ZMODEM_DEBUG
  {
    debugPrintf("setup_encode_byte_map():\n");
    debugPrintf("---- \n");
    for (ch = 0; ch < 256; ch++)
        debugPrintf("From %02x --> To %02x\n", (int) ch, (int) encode_byte_map[ch]);
    debugPrintf("---- \n");
  }
# endif
}

/**
 * Turn one byte into up to two escaped bytes, copying to output.  The output
 * buffer must be big enough to contain all the data.
 *
 * @param ch the byte to convert
 * @param output a buffer to contain the encoded byte
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 */
void ZModem::encode_byte(const unsigned char ch, unsigned char * output,
                        unsigned int * output_n,
                        const unsigned int output_max) 
{
  unsigned char new_ch = encode_byte_map[ch];
  /*
   * Check for space
   */
  assert(*output_n + 2 <= output_max);
  if (new_ch != ch) 
  {
    /*
     * Encode
     */
    output[*output_n] = C_CAN;
    *output_n = *output_n + 1;
    output[*output_n] = new_ch;
    *output_n = *output_n + 1;
  } 
  else 
  {
    /*
     * Regular character
     */
    output[*output_n] = ch;
    *output_n = *output_n + 1;
  }
}

/**
 * Turn regular bytes into escaped bytes, copying to output.  The output
 * buffer must be big enough to contain all the data.
 *
 * @param output a buffer to contain the encoded byte
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @param crc_type ZMODEM_DT_ZCRCE, ZMODEM_DT_ZCRCG, ZMODEM_DT_ZCRCQ, or ZMODEM_DT_ZCRCW
 */
void ZModem::encode_zdata_bytes(unsigned char * output,
                               unsigned int * output_n,
                               const unsigned int output_max,
                               const unsigned char crc_type) 
{
  unsigned int i;             /* input iterator */
  unsigned int j;             /* CRC32 iterator */
  int crc_16;
  uint32_t crc_32;
  bool doing_crc = false;
  unsigned int crc_length = 0;
  unsigned char ch;
  unsigned char crc_buffer[4];
# ifdef ZMODEM_DEBUG
  {
    debugPrintf("encode_zdata_bytes(): packet.type = %d packet.use_crc32 = %s packet.data_n = %d output_n = %d output_max = %d data: ",
        packet.type, (packet.use_crc32 == true ? "true" : "false"),
        packet.data_n, *output_n, output_max);
    for (i = 0; i < packet.data_n; i++)
      debugPrintf("%02x ", (packet.data[i] & 0xFF));
    debugPrintf("\n");
  }
# endif
  for (i = 0; ; i++) 
  {
    if (doing_crc == false) 
    {
      if (i == packet.data_n) 
      {
        /*
         * Add the link escape sequence
         */
        output[*output_n] = C_CAN;
        *output_n = *output_n + 1;
        output[*output_n] = crc_type;
        *output_n = *output_n + 1;
        /*
         * Compute the CRC
         */
        if ((packet.use_crc32 == true) && (packet.type != ZMODEM_PKT_ZSINIT)) 
        {
          crc_length = 4;
          crc_32 = compute_crc32(0, NULL, 0);
          /*
           * Another case of *strange* CRC behavior...
           */
          for (j = 0; j < packet.data_n; j++) 
          {
              crc_32 = ~compute_crc32(crc_32, packet.data + j, 1);
          }
          crc_32 = ~compute_crc32(crc_32, &crc_type, 1);
          crc_32 = ~crc_32;
#         ifdef ZMODEM_DEBUG
            debugPrintf("encode_zdata_bytes(): DATA CRC32: %08x\n", crc_32);
#         endif
          /*
           * Little-endian
           */
          crc_buffer[0] = (unsigned char) ( crc_32        & 0xFF);
          crc_buffer[1] = (unsigned char) ((crc_32 >>  8) & 0xFF);
          crc_buffer[2] = (unsigned char) ((crc_32 >> 16) & 0xFF);
          crc_buffer[3] = (unsigned char) ((crc_32 >> 24) & 0xFF);
        } 
        else 
        {
          /*
           * 16-bit CRC
           */
          crc_length = 2;
          crc_16 = 0;
          crc_16 = compute_crc16(crc_16, packet.data, packet.data_n);
          crc_16 = compute_crc16(crc_16, &crc_type, 1);
#         ifdef ZMODEM_DEBUG
            debugPrintf("encode_zdata_bytes(): DATA CRC16: %04x\n", crc_16);
#         endif
          /*
           * Big-endian
           */
          crc_buffer[0] = (unsigned char) ((crc_16 >> 8) & 0xFF);
          crc_buffer[1] = (unsigned char) ( crc_16       & 0xFF);
        }
        doing_crc = true;
        i = -1;
        continue;
      } 
      else 
        ch = packet.data[i];
    } 
    else 
    {
      if (i >= crc_length)
        break;
      ch = crc_buffer[i];
    }
    /*
     * Encode the byte
     */
    encode_byte(ch, output, output_n, output_max);
  } /* for (i = 0; i < packet.data_n; i++) */
  /*
   * One type of packet is terminated "special"
   */
  if (crc_type == ZMODEM_DT_ZCRCW) 
  {
    output[*output_n] = C_XON;
    *output_n = *output_n + 1;
  }
# ifdef ZMODEM_DEBUG
  {
    debugPrintf("encode_zdata_bytes(): i = %d *output_n = %d data: ", i, *output_n);
    for (i = 0; i < *output_n; i++)
        debugPrintf("%02x ", (output[i] & 0xFF));
    debugPrintf("\n");
  }
# endif
}

/* ------------------------------------------------------------------------ */
/* Packet layer ----------------------------------------------------------- */
/* ------------------------------------------------------------------------ */
/**
 * Build a Zmodem packet.
 *
 * @param type the packet type
 * @param argument a 32-bit argument
 * @param data_packet the buffer to write the packet to
 * @param data_packet_n the size of data_packet
 * @param data_packet_max the maximum size of data_packet
 */
void ZModem::build_packet(const int type, const long argument,
                         unsigned char * data_packet,
                         unsigned int * data_packet_n,
                         const int data_packet_max) 
{
  int crc_16;
  unsigned char crc_16_hex[4];
  uint32_t crc_32;
  unsigned char header[10];
  bool do_hex;
  int i;
  
# ifdef ZMODEM_DEBUG
  char * type_string;
  switch (type) 
  {
    case ZMODEM_PKT_ZRQINIT:
      type_string = "ZRQINIT";
      break;
    case ZMODEM_PKT_ZRINIT:
      type_string = "ZRINIT";
      break;
    case ZMODEM_PKT_ZSINIT:
      type_string = "ZSINIT";
      break;
    case ZMODEM_PKT_ZACK:
      type_string = "ZACK";
      break;
    case ZMODEM_PKT_ZFILE:
      type_string = "ZFILE";
      break;
    case ZMODEM_PKT_ZSKIP:
      type_string = "ZSKIP";
      break;
    case ZMODEM_PKT_ZNAK:
      type_string = "ZNAK";
      break;
    case ZMODEM_PKT_ZABORT:
      type_string = "ZABORT";
      break;
    case ZMODEM_PKT_ZFIN:
      type_string = "ZFIN";
      break;
    case ZMODEM_PKT_ZRPOS:
      type_string = "ZRPOS";
      break;
    case ZMODEM_PKT_ZDATA:
      type_string = "ZDATA";
      break;
    case ZMODEM_PKT_ZEOF:
      type_string = "ZEOF";
      break;
    case ZMODEM_PKT_ZFERR:
      type_string = "ZFERR";
      break;
    case ZMODEM_PKT_ZCRC:
      type_string = "ZCRC";
      break;
    case ZMODEM_PKT_ZCHALLENGE:
      type_string = "ZCHALLENGE";
      break;
    case ZMODEM_PKT_ZCOMPL:
      type_string = "ZCOMPL";
      break;
    case ZMODEM_PKT_ZCAN:
      type_string = "ZCAN";
      break;
    case ZMODEM_PKT_ZFREECNT:
      type_string = "ZFREECNT";
      break;
    case ZMODEM_PKT_ZCOMMAND:
      type_string = "ZCOMMAND";
      break;
    default:
      type_string = "Invalid type";
  }
  debugPrintf("build_packet(): type = %s (%d) argument = %08lx\n", type_string,
      type, argument);
# endif
  /*
   * Initialize the packet
   */
  packet.type = type;
  packet.use_crc32 = use_crc32;
  packet.data_n = 0;
  /*
   * Copy type to first header byte
   */
  header[0] = type;
  switch (type) 
  {
    case ZMODEM_PKT_ZRPOS:
    case ZMODEM_PKT_ZEOF:
    case ZMODEM_PKT_ZCRC:
    case ZMODEM_PKT_ZCOMPL:
    case ZMODEM_PKT_ZFREECNT:
    case ZMODEM_PKT_ZSINIT:
      /*
       * Little endian order
       */
      header[4] = (argument >> 24) & 0xFF;
      header[3] = (argument >> 16) & 0xFF;
      header[2] = (argument >> 8) & 0xFF;
      header[1] = argument & 0xFF;
      break;
    default:
      /*
       * Everything else is in big endian order
       */
      header[1] = (argument >> 24) & 0xFF;
      header[2] = (argument >> 16) & 0xFF;
      header[3] = (argument >> 8) & 0xFF;
      header[4] = argument & 0xFF;
      break;
  }
  switch (type) 
  {
    case ZMODEM_PKT_ZRQINIT:
    case ZMODEM_PKT_ZRINIT:
    case ZMODEM_PKT_ZSINIT:
    case ZMODEM_PKT_ZCHALLENGE:
      /*
       * ZCHALLENGE comes before the CRC32 negotiation, so it must use hex
       * packets.  The other packets are defined by the standard to be hex.
       */
    case ZMODEM_PKT_ZRPOS:
      do_hex = true;
      break;
    default:
      if ((flags & ZMODEM_TX_ESCAPE_CTRL) || (flags & ZMODEM_TX_ESCAPE_8BIT))
        do_hex = true;
      else
        do_hex = false;
      break;
  }
  /*
   * OK, so we can get seriously out of sync with rz -- it doesn't bother
   * checking to see if ZSINIT is CRC32 or not.  So we have to see what it
   * expects and encode appropriately.
   */
  if ((type == ZMODEM_PKT_ZSINIT) && (sending == true) && (use_crc32 == true) ) 
    do_hex = false;
  /*
   * A bug in sz: it sometimes loses the ZCRC even though it reads the
   * bytes.
   */
  if ((type == ZMODEM_PKT_ZCRC) && (sending == false))
    do_hex = true;
  if (do_hex == true) 
  {
    /*
     * Hex must be 16-bit CRC, override the default setting
     */
    packet.use_crc32 = false;
    /*
     * Hex packets
     */
    data_packet[0] = ZMODEM_DT_ZPAD;
    data_packet[1] = ZMODEM_DT_ZPAD;
    data_packet[2] = C_CAN;
    data_packet[3] = 'B';
    hexify_string(header, 5, &data_packet[4], ZMODEM_HEX_PACKET_LENGTH - 10);
    *data_packet_n = *data_packet_n + ZMODEM_HEX_PACKET_LENGTH;
    /*
     * Hex packets always use 16-bit CRC
     */
    crc_16 = compute_crc16(0, header, 5);
    crc_16_hex[0] = (crc_16 >> 8) & 0xFF;
    crc_16_hex[1] = crc_16 & 0xFF;
    hexify_string(crc_16_hex, 2, &data_packet[14], ZMODEM_HEX_PACKET_LENGTH - 14);
    data_packet[18] = C_CR;
    /*
     * lrzsz flips the high bit here.  Why??
     */
    /* data_packet[19] = C_LF; */
    data_packet[19] = C_LF | 0x80;
    switch (type) 
    {
      case ZMODEM_PKT_ZFIN:
      case ZMODEM_PKT_ZACK:
          break;
      default:
          /*
           * Append XON to most hex packets
           */
          data_packet[(*data_packet_n)] = C_XON;
          *data_packet_n = *data_packet_n + 1;
          break;
    }
  } 
  else 
  {
    bool altered_encode_byte_map = false;
    uint32_t old_flags = flags;
    if (type == ZMODEM_PKT_ZSINIT) 
    {
      /*
       * Special case: lrzsz needs control characters escaped in the
       * ZMODEM_STATE_ZSINIT.
       */
      if (!(flags & ZMODEM_TX_ESCAPE_CTRL)) 
      {
        altered_encode_byte_map = true;
        /*
         * Update the encode map
         */
        flags |= ZMODEM_TX_ESCAPE_CTRL;
        setup_encode_byte_map();
      }
    }
    /*
     * Binary packets
     */
    data_packet[0] = ZMODEM_DT_ZPAD;
    data_packet[1] = C_CAN;
    if (use_crc32 == true)
      data_packet[2] = 'C';
    else
      data_packet[2] = 'A';
    /*
     * Set initial length
     */
    *data_packet_n = *data_packet_n + 3;
    /*
     * Encode the argument field
     */
    for (i = 0; i < 5; i++) 
      encode_byte((unsigned char) (header[i] & 0xFF), data_packet,
                  data_packet_n, data_packet_max);
    if (packet.use_crc32 == true) 
    {
      crc_32 = compute_crc32(0, NULL, 0);
      crc_32 = compute_crc32(crc_32, header, 5);
      /*
       * Little-endian
       */
      encode_byte((unsigned char) ( crc_32        & 0xFF),
                  data_packet, data_packet_n, data_packet_max);
      encode_byte((unsigned char) ((crc_32 >>  8) & 0xFF),
                  data_packet, data_packet_n, data_packet_max);
      encode_byte((unsigned char) ((crc_32 >> 16) & 0xFF),
                  data_packet, data_packet_n, data_packet_max);
      encode_byte((unsigned char) ((crc_32 >> 24) & 0xFF),
                  data_packet, data_packet_n, data_packet_max);
    } 
    else 
    {
      crc_16 = compute_crc16(0, header, 5);
      encode_byte((unsigned char) ((crc_16 >> 8) & 0xFF),
                  data_packet, data_packet_n, data_packet_max);
      encode_byte((unsigned char) ( crc_16       & 0xFF),
                  data_packet, data_packet_n, data_packet_max);
    }
    if (altered_encode_byte_map == true) 
    {
      /*
       * Restore encode_byte_map and flags
       */
      flags = old_flags;
      setup_encode_byte_map();
    }
  }
  /*
   * Make sure we're still OK
   */
  assert(*data_packet_n <= data_packet_max);
}


/**
 * Parse a Zmodem packet.
 *
 * @param input the bytes from the remote side
 * @param input_n the number of bytes in input_n
 * @param discard the number of bytes that were consumed by the input
 * @return a parse packet return code (OK, CRC error, etc.)
 */
ZMODEM_PARSE_PACKET ZModem::parse_packet(const unsigned char * input,
                                    const int input_n, int * discard) 
{
  char * type_string;
  int begin = 0;
  unsigned char crc_header[5];
  uint32_t crc_32;
  int crc_16;
  bool has_data = false;
  int i;
  bool got_can = false;
  unsigned char ch;
  unsigned char hex_buffer[4];
# ifdef ZMODEM_DEBUG
    debugPrintf("parse_packet()\n");
# endif
  /*
   * Clear packet
   */
  memset(&packet, 0, sizeof(struct zmodem_packet));
  /*
   * Find the start of the packet
   */
  while (input[begin] != ZMODEM_DT_ZPAD) 
  {
    /*
     * Strip non-ZMODEM_DT_ZPAD characters
     */
    begin++;
    if (begin >= input_n) 
    {
      /*
       * Throw away what's here, we're still looking for a packet
       * beginning.
       */
      *discard = begin;
      return ZM_PP_NODATA;
    }
  }
  /*
   * Throw away up to the packet beginning
   */
  *discard = begin;
  while (input[begin] == ZMODEM_DT_ZPAD) 
  {
    /*
     * Strip ZMODEM_DT_ZPAD characters
     */
    begin++;
    if (begin >= input_n)
      return ZM_PP_NODATA;
  }
  /*
   * Pull into fields
   */
  if (input[begin] != C_CAN) 
  {
    *discard = *discard + 1;
    return ZM_PP_INVALID;
  }
  begin++;
  if (begin >= input_n)
    return ZM_PP_NODATA;
  if (input[begin] == 'A') 
  {
    /*
     * CRC-16
     */
    if (input_n - begin < 8)
        return ZM_PP_NODATA;
    packet.use_crc32 = false;
    packet.argument = 0;
    packet.crc16 = 0;
    begin += 1;
    for (i = 0; i < 7; i++, begin++) 
    {
      if (begin >= input_n)
        return ZM_PP_NODATA;
      if (input[begin] == C_CAN) 
      {
        /*
         * Escape control char
         */
        got_can = true;
        i--;
        continue;
      }
      if (got_can == true)
      {
        got_can = false;
        if (input[begin] == 'l') 
        {
          /*
           * Escaped control character: 0x7f
           */
          ch = 0x7F;
        } 
        else 
        if (input[begin] == 'm') 
        {
          /*
           * Escaped control character: 0xff
           */
          ch = 0xFF;
        } 
        else 
        if ((input[begin] & 0x40) != 0) 
        {
          /*
           * Escaped control character: CAN m OR 0x40
           */
          ch = input[begin] & 0xBF;
        } 
        else 
        {
          /*
           * Should never get here
           */
          return ZM_PP_INVALID;
        }
      } 
      else
        ch = input[begin];
      if (i == 0) 
      {
        /*
         * Type
         */
        packet.type = ch;
        crc_header[0] = packet.type;
      } 
      else 
      if (i < 5) 
      {
        /*
         * Argument
         */
        packet.argument |= (ch << (32 - (8 * i)));
        crc_header[i] = ch;
      } 
      else 
      {
        /*
         * CRC
         */
        packet.crc16 |= (ch << (16 - (8 * (i - 4))));
      }
    }
  } 
  else 
  if (input[begin] == 'B') 
  {
    /*
     * CRC-16 HEX
     */
    begin++;
    if (input_n - begin < 14 + 2)
      return ZM_PP_NODATA;
    packet.use_crc32 = false;
    /*
     * Dehexify
     */
    memset(hex_buffer, 0, sizeof(hex_buffer));
    if (dehexify_string(&input[begin], 2, hex_buffer,
            sizeof(hex_buffer)) == false)
      return ZM_PP_INVALID;
    packet.type = hex_buffer[0];
    memset(hex_buffer, 0, sizeof(hex_buffer));
    if (dehexify_string(&input[begin + 2], 8, hex_buffer, sizeof(hex_buffer)) == false)
        return ZM_PP_INVALID;
    packet.argument = ((hex_buffer[0] & 0xFF) << 24) |
                      ((hex_buffer[1] & 0xFF) << 16) |
                      ((hex_buffer[2] & 0xFF) <<  8) |
                       (hex_buffer[3] & 0xFF);
    memset(hex_buffer, 0, sizeof(hex_buffer));
    if (dehexify_string(&input[begin + 10], 4, hex_buffer, sizeof(hex_buffer)) == false)
      return ZM_PP_INVALID;
    packet.crc16 = ((hex_buffer[0] & 0xFF) << 8) | (hex_buffer[1] & 0xFF);
    /*
     * Point to end
     */
    begin += 14;
    /*
     * Copy header to crc_header
     */
    crc_header[0] = packet.type;
    crc_header[1] = (unsigned char) ((packet.argument >> 24) & 0xFF);
    crc_header[2] = (unsigned char) ((packet.argument >> 16) & 0xFF);
    crc_header[3] = (unsigned char) ((packet.argument >>  8) & 0xFF);
    crc_header[4] = (unsigned char) ( packet.argument        & 0xFF);
    /*
     * More special-case junk: sz sends 0d 8a at the end of each hex
     * header.
     */
    begin += 2;
    /*
     * sz also sends XON at the end of each hex header except ZFIN and
     * ZACK.
     */
    switch (packet.type) 
    {
      case ZMODEM_PKT_ZFIN:
      case ZMODEM_PKT_ZACK:
          break;
      default:
        if (input_n - begin < 1)
            return ZM_PP_NODATA;
        begin++;
        break;
    }
  } 
  else 
  if (input[begin] == 'C') 
  {
    /*
     * CRC-32
     */
    if (input_n - begin < 10)
        return ZM_PP_NODATA;
    packet.use_crc32 = true;
    packet.argument = 0;
    packet.crc32 = 0;
    /*
     * Loop through the type, argument, and crc values, unescaping
     * control characters along the way
     */
    begin += 1;
    for (i = 0; i < 9; i++, begin++) 
    {
      if (begin >= input_n)
        return ZM_PP_NODATA;
      if (input[begin] == C_CAN) 
      {
        /*
         * Escape control char
         */
        got_can = true;
        i--;
        continue;
      }
      if (got_can == true) 
      {
        got_can = false;
        if (input[begin] == 'l') 
        {
          /*
           * Escaped control character: 0x7f
           */
          ch = 0x7F;
        } 
        else
        if (input[begin] == 'm') 
        {
          /*
           * Escaped control character: 0xff
           */
          ch = 0xFF;
        } 
        else
        if ((input[begin] & 0x40) != 0) 
        {
          /*
           * Escaped control character: CAN m OR 0x40
           */
          ch = input[begin] & 0xBF;
        } 
        else 
        {
          /*
           * Should never get here
           */
          return ZM_PP_NODATA;
        }
      } 
      else
        ch = input[begin];
      if (i == 0) 
      {
        /*
         * Type
         */
        packet.type = ch;
        crc_header[0] = packet.type;
      } 
      else
      if (i < 5) 
      {
        /*
         * Argument
         */
        packet.argument |= (ch << (32 - (8 * i)));
        crc_header[i] = ch;
      } 
      else 
      {
        /*
         * CRC - in little-endian form
         */
        packet.crc32 |= (ch << (8 * (i - 5)));
      }
    }
  } 
  else 
  {
    /*
     * Invalid packet type
     */
    *discard = *discard + 1;
    return ZM_PP_INVALID;
  }
  /*
   * Type
   */
  switch (packet.type) 
  {
    case ZMODEM_PKT_ZRQINIT:
      type_string = "ZRQINIT";
      break;
    case ZMODEM_PKT_ZRINIT:
      type_string = "ZRINIT";
      break;
    case ZMODEM_PKT_ZSINIT:
      type_string = "ZSINIT";
      break;
    case ZMODEM_PKT_ZACK:
      type_string = "ZACK";
      break;
    case ZMODEM_PKT_ZFILE:
      type_string = "ZFILE";
      break;
    case ZMODEM_PKT_ZSKIP:
      type_string = "ZSKIP";
      break;
    case ZMODEM_PKT_ZNAK:
      type_string = "ZNAK";
      break;
    case ZMODEM_PKT_ZABORT:
      type_string = "ZABORT";
      break;
    case ZMODEM_PKT_ZFIN:
      type_string = "ZFIN";
      break;
    case ZMODEM_PKT_ZRPOS:
      type_string = "ZRPOS";
      break;
    case ZMODEM_PKT_ZDATA:
      type_string = "ZDATA";
      break;
    case ZMODEM_PKT_ZEOF:
      type_string = "ZEOF";
      break;
    case ZMODEM_PKT_ZFERR:
      type_string = "ZFERR";
      break;
    case ZMODEM_PKT_ZCRC:
      type_string = "ZCRC";
      break;
    case ZMODEM_PKT_ZCHALLENGE:
      type_string = "ZCHALLENGE";
      break;
    case ZMODEM_PKT_ZCOMPL:
      type_string = "ZCOMPL";
      break;
    case ZMODEM_PKT_ZCAN:
      type_string = "ZCAN";
      break;
    case ZMODEM_PKT_ZFREECNT:
      type_string = "ZFREECNT";
      break;
    case ZMODEM_PKT_ZCOMMAND:
      type_string = "ZCOMMAND";
      break;
    default:
#     ifdef ZMODEM_DEBUG
        debugPrintf("parse_packet(): INVALID PACKET TYPE %d\n", packet.type);
#     endif
      return ZM_PP_INVALID;
  }
  /*
   * Figure out if the argument is supposed to be flipped
   */
  switch (packet.type) 
  {
    case ZMODEM_PKT_ZRPOS:
    case ZMODEM_PKT_ZEOF:
    case ZMODEM_PKT_ZCRC:
    case ZMODEM_PKT_ZCOMPL:
    case ZMODEM_PKT_ZFREECNT:
      /*
       * Swap the packet argument around
       */
      packet.argument = big_to_little_endian(packet.argument);
      break;
    default:
      break;
  }
# ifdef ZMODEM_DEBUG
  if (packet.use_crc32 == true) {
    debugPrintf("parse_packet(): CRC32 type = %s (%d) argument=%08x crc=%08x\n",
          type_string, packet.type, packet.argument, packet.crc32);
  } else {
    debugPrintf("parse_packet(): CRC16 type = %s (%d) argument=%08x crc=%04x\n",
          type_string, packet.type, packet.argument, packet.crc16);
  }
# endif
  /*
   * Check CRC
   */
  if (packet.use_crc32 == true) 
  {
    crc_32 = compute_crc32(0, NULL, 0);
    crc_32 = compute_crc32(crc_32, crc_header, 5);
    if (crc_32 != packet.crc32) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("parse_packet(): CRC ERROR (given=%08x computed=%08x)\n",
              packet.crc32, crc_32);
#     endif
      stats_increment_errors();
      return ZM_PP_CRCERROR;
    }
  } 
  else 
  {
    crc_16 = compute_crc16(0, crc_header, 5);
    if (crc_16 != packet.crc16) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("parse_packet(): CRC ERROR (given=%04x computed=%04x)\n",
              packet.crc16, crc_16);
#     endif
      stats_increment_errors();
      return ZM_PP_CRCERROR;
    }
  }
# ifdef ZMODEM_DEBUG
    debugPrintf("parse_packet(): CRC OK\n");
# endif
  /*
   * Pull data for certain packet types
   */
  switch (packet.type) 
  {
    case ZMODEM_PKT_ZSINIT:
    case ZMODEM_PKT_ZFILE:
    case ZMODEM_PKT_ZDATA:
    case ZMODEM_PKT_ZCOMMAND:
      /*
       * Packet data will follow
       */
      has_data = true;
      break;
    default:
        break;
  }

  /*
   * Discard what's been processed
   */
  *discard = begin;

  if (has_data == true) 
  {
    prior_state = state;
    state = ZMODEM_STATE_ZDATA;
    packet.data_n = 0;
    packet.crc16 = 0;
    packet.crc32 = compute_crc32(0, NULL, 0);
  }

  /*
   * All OK
   */
  return ZM_PP_OK;
}

/* ------------------------------------------------------------------------ */
/* Top-level states ------------------------------------------------------- */
/* ------------------------------------------------------------------------ */

/**
 * Receive:  ZMODEM_STATE_ZCHALLENGE
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
bool ZModem::receive_zchallenge(unsigned char * output,
                                 unsigned int * output_n,
                                 const unsigned int output_max) 
{
  uint32_t options;
# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zchallenge()\n");
# endif
  zchallenge_value = random();
# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zchallenge() VALUE = %08x\n", zchallenge_value);
# endif
  options = zchallenge_value;
  build_packet(ZMODEM_PKT_ZCHALLENGE, options, output, output_n, output_max);
  state = ZMODEM_STATE_ZCHALLENGE_WAIT;
  /*
   * Discard input bytes
   */
  packet_buffer_n = 0;
  /*
   * Process through the new state
   */
  return false;
}

/**
 * Receive:  ZMODEM_STATE_ZCHALLENGE_WAIT
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
bool ZModem::receive_zchallenge_wait(unsigned char * output,
                                      unsigned int * output_n,
                                      const unsigned int output_max) 
{
  ZMODEM_PARSE_PACKET rc_pp;
  uint32_t options = 0;
  int discard;

# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zchallenge_wait()\n");
# endif
  if (packet_buffer_n > 0) 
  {
    rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);
    /*
     * Take the bytes off the stream
     */
    if (discard > 0) 
    {
      memmove(packet_buffer, packet_buffer + discard,
              packet_buffer_n - discard);
      packet_buffer_n -= discard;
    }
    if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("receive_zchallenge_wait(): ERROR garbled header\n");
#     endif
      stats_increment_errors();
      packet_buffer_n = 0;
      build_packet(ZMODEM_PKT_ZNAK, options, output, output_n, output_max);
      return true;
    }
    if (rc_pp == ZM_PP_NODATA)
      return true;
    if (rc_pp == ZM_PP_OK) 
    {
      if (packet.type == ZMODEM_PKT_ZACK) 
      {
        /*
         * Verify the value returned
         */
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zchallenge_wait() argument = %08x correct = %08x\n",
                packet.argument, zchallenge_value);
#       endif
        if (packet.argument == zchallenge_value) 
        {
          /*
           * I'd love to wait a second here so the user can see the
           * successful ZCHALLENGE response on the transfer
           * screen...
           */
          /*
           * Send the ZRINIT
           */
          state = ZMODEM_STATE_ZRINIT;
          packet.crc16 = 0;
          packet.crc32 = compute_crc32(0, NULL, 0);
          return false;
        } 
        else 
        {
#         ifdef ZMODEM_DEBUG
            debugPrintf("receive_zchallenge_wait(): ERROR zchallenge error\n");
#         endif
          stats_increment_errors();
          state = ZMODEM_STATE_ABORT;
          return true;
        }
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZNAK) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zchallenge_wait(): ERROR ZNAK\n");
#       endif
        /*
         * Re-send
         */
        stats_increment_errors();
        state = ZMODEM_STATE_ZCHALLENGE;
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZRQINIT) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zchallenge_wait(): ERROR ZRQINIT\n");
#       endif
        /*
         * Re-send, but don't count as an error
         */
        state = ZMODEM_STATE_ZCHALLENGE;
      } 
      else 
      {
        /*
         * Sender isn't Zmodem compliant, abort.
         */
        state = ZMODEM_STATE_ABORT;
        return true;
      }
    }
    /*
     * Process through the new state
     */
    return false;
  }
  if (check_timeout() == true) 
  {
    state = ZMODEM_STATE_ZCHALLENGE;
    return false;
  }
  /*
   * No data, done
   */
  return true;
}

/**
 * Receive:  ZMODEM_STATE_ZCRC
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
bool ZModem::receive_zcrc(unsigned char * output,
                           unsigned int * output_n,
                           const unsigned int output_max) 
{
  /*
   * Buffer for reading the file
   */
  unsigned char file_buffer[1];
  size_t file_buffer_n;
  /*
   * Save the original file position
   */
  off_t original_position = file_position;
  int total_bytes = 0;
# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zcrc() ENTER\n");
# endif

  /*
   * Reset crc32
   */
  file_crc32 = compute_crc32(0, NULL, 0);
  /*
   * Seek to beginning of file
   */
  file_stream.seek(0);
  while (file_stream.available()>0) 
  {
    file_buffer_n = file_stream.read(file_buffer,sizeof(file_buffer));
    total_bytes += file_buffer_n;
    /*
     * I think I have a different CRC function from lrzsz...  I have to
     * negate both here and below to get the same value.
     */
    file_crc32 =
        ~compute_crc32(file_crc32, file_buffer, file_buffer_n);
  }
  /*
   * Seek back to the original location
   */
  file_stream.seek(original_position);
  file_crc32 = ~file_crc32;
# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zcrc() total_bytes = %d on-disk CRC32 = %08lx\n",
        total_bytes, (unsigned long) file_crc32);
#  endif
  build_packet(ZMODEM_PKT_ZCRC, total_bytes, output, output_n, output_max);
  state = ZMODEM_STATE_ZCRC_WAIT;
  packet_buffer_n = 0;
  return false;
}

/**
 * Receive:  ZMODEM_STATE_ZCRC_WAIT
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
bool ZModem::receive_zcrc_wait(unsigned char * output,
                                unsigned int * output_n,
                                const unsigned int output_max) 
{
  ZMODEM_PARSE_PACKET rc_pp;
  struct stat fstats;
  int i;
  int rc;
  uint32_t options = 0;
  int discard;
# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zcrc_wait()\n");
# endif
  if (packet_buffer_n > 0) 
  {
    rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);
    /*
     * Take the bytes off the stream
     */
    if (discard > 0) 
    {
      memmove(packet_buffer, packet_buffer + discard,
              packet_buffer_n - discard);
      packet_buffer_n -= discard;
    }
    if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("receive_zcrc_wait(): ERROR garbled header\n");
#     endif
      stats_increment_errors();
      packet_buffer_n = 0;
      build_packet(ZMODEM_PKT_ZNAK, options, output, output_n, output_max);
      return true;
    }
    if (rc_pp == ZM_PP_NODATA)
        return true;
    if (rc_pp == ZM_PP_OK) 
    {
      if (packet.type == ZMODEM_PKT_ZCRC) 
      {
        /*
         * Verify the value returned
         */
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zcrc_wait() argument = %08x correct = %08lx\n",
                packet.argument, (unsigned long) file_crc32);
#       endif
        if (packet.argument == file_crc32) 
        {
          /*
           * We're working on the same file, check its length to
           * see if we should send ZRPOS or ZSKIP.
           */
          if (file_size == file_position) 
          {
#           ifdef ZMODEM_DEBUG
              debugPrintf("receive_zcrc_wait(): got file, switch to ZSKIP\n");
#           endif
            state = ZMODEM_STATE_ZSKIP;
          } 
          else 
          {
#           ifdef ZMODEM_DEBUG
              debugPrintf("receive_zcrc_wait(): crash recovery, switch to ZRPOS\n");                        state = ZMODEM_STATE_ZRPOS;
#           endif
          }
        } 
        else 
        {
          /*
           * This is a different file, rename it
           */
          for (i = 0; ; i++) 
          {
            /*
             * Add a numeric suffix and see if the file doesn't
             * exist.  If so, we can use that filename.
             */
            sprintf(file_fullname, "%s/%s.%04d", download_path, file_name, i);
            rc = stat(file_fullname, &fstats);
            if (rc < 0) 
            {
              if (errno == ENOENT) 
              {
                /*
                 * We found a free filename, break out and
                 * try to create it.
                 */
#               ifdef ZMODEM_DEBUG
                  debugPrintf("receive_zcrc_wait(): prevent overwrite, switch to ZRPOS, new filename = %s\n",
                        file_fullname);
#               endif
                break;
              } 
              else 
              {
                state = ZMODEM_STATE_ABORT;
                return true;
              }
            }
          } /* for (i = 0; ; i++) */
          file_position = 0;
          file_stream = fs->open(file_fullname,FILE_WRITE);
          if (!file_stream) 
          {
            state = ZMODEM_STATE_ABORT;
            return true;
          }
          /*
           * Seek to the end
           */
          file_stream.seek(file_stream.size()-1);
          //fseek(file_stream, 0, SEEK_END);
          /*
           * Update progress display
           */
          stats_new_file(file_fullname, file_size);
          /*
           * Ready for ZRPOS now
           */
          state = ZMODEM_STATE_ZRPOS;
        }
        /*
         * At this point we have switched to ZRPOS or ZSKIP, either
         * with the original filename (crash recovery) or a new one
         * (different file).
         */
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZNAK) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zcrc_wait(): ERROR ZNAK\n");
#       endif
        stats_increment_errors();
        state = ZMODEM_STATE_ZCRC;
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZFILE) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zcrc_wait(): ZFILE sender does not understand ZCRC, move to crash recovery (even though this may corrupt the file)\n");
#       endif
        stats_increment_errors();
        state = ZMODEM_STATE_ZRPOS;
      } 
      else 
      {
        /*
         * Sender isn't Zmodem compliant, abort.
         */
        state = ZMODEM_STATE_ABORT;
        return true;
      }
    }
    /*
     * Process through the new state
     */
    return false;
  }
  if (check_timeout() == true) 
  {
    state = ZMODEM_STATE_ZCRC;
    return false;
  }
  /*
   * No data, done
   */
  return true;
}

/**
 * Receive:  ZRINIT
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
bool ZModem::receive_zrinit(unsigned char * output,
                             unsigned int * output_n,
                             const unsigned int output_max) 
{
  uint32_t options;
# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zrinit()\n");
# endif
  options = ZMODEM_TX_CAN_FULL_DUPLEX | ZMODEM_TX_CAN_OVERLAP_IO;
  if (use_crc32 == true)
    options |= ZMODEM_TX_CAN_CRC32;
  options |= ZMODEM_TX_ESCAPE_CTRL;
  flags = options;
  build_packet(ZMODEM_PKT_ZRINIT, options, output, output_n, output_max);
  state = ZMODEM_STATE_ZRINIT_WAIT;
  packet_buffer_n = 0;
  return false;
}

/**
 * Receive:  ZMODEM_STATE_ZRINIT_WAIT
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
bool ZModem::receive_zrinit_wait(unsigned char * output,
                                  unsigned int * output_n,
                                  const unsigned int output_max) 
{
  ZMODEM_PARSE_PACKET rc_pp;
  int discard;
  uint32_t options = 0;
# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zrinit_wait()\n");
# endif
  if (packet_buffer_n > 0) 
  {
    rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);
    /*
     * Take the bytes off the stream
     */
    if (discard > 0) 
    {
      memmove(packet_buffer, packet_buffer + discard,
              packet_buffer_n - discard);
      packet_buffer_n -= discard;
    }
    if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("receive_zrinit_wait(): ERROR garbled header\n");
#     endif
      stats_increment_errors();
      packet_buffer_n = 0;
      build_packet(ZMODEM_PKT_ZNAK, options, output, output_n, output_max);
      return true;
    }
    if (rc_pp == ZM_PP_NODATA) 
      return true;
    if (rc_pp == ZM_PP_OK) 
    {
      if (packet.type == ZMODEM_PKT_ZFIN) 
      {
        /*
         * All done.
         */
        options = 0;
        build_packet(ZMODEM_PKT_ZFIN, options, output, output_n, output_max);
        /*
         * Waiting for the Over-and-Out
         */
        state = ZMODEM_STATE_ZFIN_WAIT;
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZRQINIT) 
      {
        /*
         * Sender has repeated its ZRQINIT, re-send the ZRINIT
         * response.
         */
        state = ZMODEM_STATE_ZRINIT;
        packet.crc16 = 0;
        packet.crc32 = compute_crc32(0, NULL, 0);
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZSINIT) 
      {
        /*
         * See what options were specified
         */
        if (packet.argument & ZMODEM_TX_ESCAPE_CTRL) 
        {
          flags |= ZMODEM_TX_ESCAPE_CTRL;
#         ifdef ZMODEM_DEBUG
              debugPrintf("receive_zrinit_wait() ZSINIT ZMODEM_TX_ESCAPE_CTRL\n");
#         endif
        }
        if (packet.argument & ZMODEM_TX_ESCAPE_8BIT) 
        {
          flags |= ZMODEM_TX_ESCAPE_8BIT;
#         ifdef ZMODEM_DEBUG
            debugPrintf("receive_zrinit_wait() ZSINIT ZMODEM_TX_ESCAPE_8BIT\n");
#         endif
        }
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zrinit_wait() ZSINIT state = %d\n",
              state);
#       endif
        /*
         * Update the encode map
         */
        setup_encode_byte_map();
        /*
         * ZACK the ZMODEM_STATE_ZSINIT
         */
        options = 0;
        build_packet(ZMODEM_PKT_ZACK, options, output, output_n, output_max);
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZCOMMAND) 
      {
        // do nothing
      } 
      else
      if (packet.type == ZMODEM_PKT_ZFILE) 
      {
        /*
         * Record the prior state and switch to data processing
         */
        prior_state = ZMODEM_STATE_ZRINIT_WAIT;
        state = ZMODEM_STATE_ZDATA;
        packet.data_n = 0;
        packet.crc16 = 0;
        packet.crc32 = compute_crc32(0, NULL, 0);
      } 
      else
      if (packet.type == ZMODEM_PKT_ZNAK) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zrinit_wait(): ERROR ZNAK\n");
#       endif
        stats_increment_errors();
        if (prior_state == ZMODEM_STATE_ZSKIP) 
        {
          /*
           * Special case: send a ZMODEM_STATE_ZSKIP back instead of a ZRINIT if
           * we asked for a ZMODEM_STATE_ZSKIP the last time.
           */
          state = ZMODEM_STATE_ZSKIP;
        } 
        else 
        {
          state = ZMODEM_STATE_ZRINIT;
        }
      } 
      else 
      {
        /*
         * Sender isn't Zmodem compliant, abort.
         */
        state = ZMODEM_STATE_ABORT;
        return true;
      }
    }
    /*
     * Process through the new state
     */
    return false;
  }
  if (check_timeout() == true) 
  {
    if (prior_state == ZMODEM_STATE_ZSKIP) 
    {
      /*
       * Special case: send a ZMODEM_STATE_ZSKIP back instead of a ZRINIT if we
       * asked for a ZMODEM_STATE_ZSKIP the last time.
       */
      state = ZMODEM_STATE_ZSKIP;
    } 
    else
      state = ZMODEM_STATE_ZRINIT;
    return false;
  }
  /*
   * No data, done
   */
  return true;
}


/**
 * Receive:  ZMODEM_STATE_ZRPOS
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
bool ZModem::receive_zrpos(unsigned char * output,
                            unsigned int * output_n,
                            const unsigned int output_max) 
{
  uint32_t options;
# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zrpos()\n");
# endif
  options = file_position;
  build_packet(ZMODEM_PKT_ZRPOS, options, output, output_n, output_max);
  state = ZMODEM_STATE_ZRPOS_WAIT;
# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zrpos(): ZRPOS file_position = %ld\n",
        file_position);
# endif
  packet_buffer_n = 0;
  return false;
}

/**
 * Receive:  ZMODEM_STATE_ZRPOS_WAIT
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
bool ZModem::receive_zrpos_wait(unsigned char * output,
                                 unsigned int * output_n,
                                 const unsigned int output_max) 
{
  ZMODEM_PARSE_PACKET rc_pp;
  int discard;
  uint32_t options = 0;
  struct utimbuf utime_buffer;
# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zrpos_wait()\n");
# endif
  if (packet_buffer_n > 0) 
  {
    rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);
    /*
     * Take the bytes off the stream
     */
    if (discard > 0) 
    {
      memmove(packet_buffer, packet_buffer + discard,
              packet_buffer_n - discard);
      packet_buffer_n -= discard;
    }
    if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) 
    {
      if (prior_state != ZMODEM_STATE_ZRPOS_WAIT) 
      {
        /*
         * Only send ZNAK when we aren't in ZDATA mode
         */
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zrpos_wait(): ERROR garbled header\n");
#       endif
        stats_increment_errors();
        packet_buffer_n = 0;
        build_packet(ZMODEM_PKT_ZNAK, options, output, output_n, output_max);
        return true;
      } 
      else 
      {
        /*
         * Keep processing the buffer until we get ZM_PP_NODATA
         */
        return false;
      }
    }
    if (rc_pp == ZM_PP_NODATA)
        return true;

    if (rc_pp == ZM_PP_OK) 
    {
      if (packet.type == ZMODEM_PKT_ZEOF) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zrpos_wait() ZEOF bytes = %ud file size = %ld\n",
                packet.argument, file_position);
#       endif
        /*
         * Check file length and ack
         */
        if (file_position == packet.argument) 
        {
          /*
           * All ok
           */
          file_stream.close();
          /*
           * Set access and modification times
           */
          utime_buffer.actime = file_modtime;
          utime_buffer.modtime = file_modtime;
          utime(file_fullname, &utime_buffer);
          assert(file_name != NULL);
          free(file_name);
          file_name = NULL;
          file_stream = NULL;
          options = 0;
          build_packet(ZMODEM_PKT_ZRINIT, options, output, output_n,
                       output_max);
          /*
           * ZEOF will be followed by ZFIN or ZFILE, let
           * receive_zrinit_wait() figure it out.
           */
          state = ZMODEM_STATE_ZRINIT_WAIT;
        } 
        else 
        {
          /*
           * The sender claims that the file size is different from
           * the actual size they sent.  We will send an error and
           * try to recover.
           */
#         ifdef ZMODEM_DEBUG
            debugPrintf("receive_zrpos_wait(): ERROR BAD FILE POSITION FROM SENDER\n");
#         endif
          stats_increment_errors();
          state = ZMODEM_STATE_ZRPOS;
        }
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZDATA) 
      {
        /*
         * Record the prior state and switch to data processing
         */
        prior_state = ZMODEM_STATE_ZRPOS_WAIT;
        state = ZMODEM_STATE_ZDATA;
        packet.data_n = 0;
        packet.crc16 = 0;
        packet.crc32 = compute_crc32(0, NULL, 0);
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZNAK) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zrpos_wait(): ERROR ZNAK\n");
#       endif
        stats_increment_errors();
        state = ZMODEM_STATE_ZRPOS;
      } 
      else 
      {
        /*
         * Sender isn't Zmodem compliant, abort.
         */
        state = ZMODEM_STATE_ABORT;
        return true;
      }
    }
    /*
     * Process through the new state
     */
    return false;
  }
  if (check_timeout() == true) 
  {
    state = ZMODEM_STATE_ZRPOS;
    return false;
  }
  /*
   * No data, done
   */
  return true;
}

/**
 * Receive:  ZMODEM_STATE_ZFILE
 *
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
bool ZModem::receive_zfile() 
{
  int filesleft;
  long totalbytesleft;
  mode_t permissions;
  struct stat fstats;
  bool need_new_file = false;
  int i;
  int rc;
  bool file_exists = false;
# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zfile()\n");
# endif
  /*
   * Break out ZMODEM_STATE_ZFILE data
   */
  /*
   * filename
   */
  file_name = strdup((char *) packet.data);
  /*
   * size, mtime, umask, files left, total left
   */
  sscanf((char *) packet.data + strlen((char *) packet.data) + 1,
         "%u %lo %o 0 %d %ld", (unsigned int *) &file_size,
         &file_modtime, (int *) &permissions, &filesleft,
         &totalbytesleft);
  /*
   * It so happens we can't use the permissions mask.  Forsberg didn't
   * encode it in a standard way.
   */
# ifdef ZMODEM_DEBUG
  {
    debugPrintf("receive_zfile(): ZFILE name: %s\n", file_name);
    debugPrintf("receive_zfile(): ZFILE size: %u mtime: %s umask: %04o\n",
            file_size, ctime(&file_modtime), permissions);
    debugPrintf("receive_zfile(): ZFILE filesleft: %d totalbytesleft: %ld\n",
            filesleft, totalbytesleft);
  }
# endif
  /*
   * Open the file
   */
  sprintf(file_fullname, "%s/%s", download_path, file_name);
  rc = stat(file_fullname, &fstats);
  if (rc < 0) 
  {
    if (errno == ENOENT) 
    {
      /*
       * Creating the file, so go straight to ZRPOS
       */
      file_position = 0;
#     ifdef ZMODEM_DEBUG
        debugPrintf("receive_zfile(): new file, switch to ZRPOS\n");
#     endif
      state = ZMODEM_STATE_ZRPOS;
    } 
    else 
    {
      state = ZMODEM_STATE_ABORT;
      return true;
    }
  } 
  else 
  {
    file_exists = true;
    file_position = fstats.st_size;
    /*
     * Check if we need to ZMODEM_STATE_ZSKIP or ZCRC this file
     */
    if (file_size < file_position) 
    {
      /*
       * Uh-oh, this is obviously a new file because it is smaller than
       * the file on disk.
       */
      need_new_file = true;
    } 
    else 
    if (file_size == file_position) 
    {
      /*
       * Hmm, we have a file on disk already.  We'll open the file, but
       * switch to ZCRC and see if we should ZMODEM_STATE_ZSKIP the file based on
       * its CRC value.
       */
#     ifdef ZMODEM_DEBUG
        debugPrintf("receive_zfile(): existing file, switch to ZCRC\n");
#     endif
      state = ZMODEM_STATE_ZCRC;
    } 
    else 
    if (file_size > 0) 
    {
      /*
       * Looks like a crash recovery case
       */
#     ifdef ZMODEM_DEBUG
        debugPrintf("receive_zfile(): filename exists, might need crash recovery, switch to ZCRC\n");
#     endif
      state = ZMODEM_STATE_ZCRC;
    } 
    else 
    {
      /*
       * 0-length file, so we can switch directly to ZMODEM_STATE_ZRPOS.
       */
#     ifdef ZMODEM_DEBUG
        debugPrintf("receive_zfile(): 0-length file, switch to ZRPOS\n");
#     endif
      state = ZMODEM_STATE_ZRPOS;
    }
  }
  if (need_new_file == true) 
  {
    /*
     * Guarantee we get a new file
     */
    file_exists = false;
    for (i = 0; ; i++) 
    {
      /*
       * Change the filename
       */
      sprintf(file_fullname, "%s/%s.%04d", download_path,
              file_name, i);
      rc = stat(file_fullname, &fstats);
      if (rc < 0) 
      {
        if (errno == ENOENT) 
        {
          /*
           * Creating the file, so go straight to ZRPOS
           */
          file_position = 0;
#         ifdef ZMODEM_DEBUG
            debugPrintf("receive_zfile(): prevent overwrite, switch to ZRPOS, new filename = %s\n",
                file_fullname);
#         endif
          state = ZMODEM_STATE_ZRPOS;
          break;
        } 
        else 
        {
          state = ZMODEM_STATE_ABORT;
          return true;
        }
      }
    } /* for (i = 0 ; ; i++) */
  } /* if (need_new_file == true) */
  if (file_exists == true)
    file_stream = fs->open(file_fullname);
  else
    file_stream = fs->open(file_fullname, FILE_WRITE);
  if (file_stream == NULL) 
  {
    state = ZMODEM_STATE_ABORT;
    return true;
  }
  /*
   * Seek to the end
   */
  file_stream.seek(file_stream.size()-1);
  /*
   * Update progress display
   */
  stats_new_file(file_fullname, file_size);
  /*
   * Process through the new state
   */
  return false;
}

/**
 * Receive:  ZDATA
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
bool ZModem::receive_zdata(unsigned char * output,
                            unsigned int * output_n,
                            const unsigned int output_max) 
{
  bool end_of_packet;
  bool acknowledge;
  bool crc_ok = false;
  uint32_t options;
  int crc16;
  uint32_t crc32;
  unsigned int i;

# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zdata(): DATA state=%d prior_state=%d packet.data_n=%d\n",
        state, prior_state, packet.data_n);
# endif
  /*
   * First, decode the bytes for escaped control characters
   */
  if (decode_zdata_bytes
      (packet_buffer, &packet_buffer_n, packet.data + packet.data_n,
       &packet.data_n, sizeof(packet.data) - packet.data_n,
       packet.crc_buffer) == false) 
  {
    /*
     * Not enough data available, wait for more
     */
    /*
     * Trash the partial data in packet.data.
     */
    if (packet_buffer_n > 0)
        packet.data_n = 0;
    return true;
  }
  /*
   * See what kind of CRC escape was requested
   */
  if (packet.crc_buffer[0] == ZMODEM_DT_ZCRCG) 
  {
    /*
     * CRC escape:  not end of packet, no acknowledgement required
     */
    end_of_packet = false;
    acknowledge = false;
  } 
  else 
  if (packet.crc_buffer[0] == ZMODEM_DT_ZCRCE) 
  {
    /*
     * CRC escape:  end of packet, no acknowledgement required
     */
    end_of_packet = true;
    acknowledge = false;
  } 
  else 
  if (packet.crc_buffer[0] == ZMODEM_DT_ZCRCW) 
  {
    /*
     * CRC escape:  end of packet, acknowledgement required
     */
    end_of_packet = true;
    acknowledge = true;
  } 
  else 
  if (packet.crc_buffer[0] == ZMODEM_DT_ZCRCQ) 
  {
    /*
     * CRC escape:  not end of packet, acknowledgement required
     */
    end_of_packet = false;
    acknowledge = true;
  } 
  else 
  {
    /*
     * Sender isn't Zmodem compliant, abort.
     */
    state = ZMODEM_STATE_ABORT;
    return true;
  }
  /*
   * Check the crc
   */
  /*
   * Copy the CRC escape byte onto the end of packet.data
   */
  packet.data[packet.data_n] = packet.crc_buffer[0];
  if (packet.use_crc32 == true) 
  {
    /*
     * 32-bit CRC
     */
    packet.crc32 = compute_crc32(0, NULL, 0);
    packet.crc32 =
        compute_crc32(packet.crc32, packet.data, packet.data_n + 1);
    /*
     * Little-endian
     */
    crc32 = ((packet.crc_buffer[4] & 0xFF) << 24) |
            ((packet.crc_buffer[3] & 0xFF) << 16) |
            ((packet.crc_buffer[2] & 0xFF) << 8) |
             (packet.crc_buffer[1] & 0xFF);
#   ifdef ZMODEM_DEBUG
    {
      debugPrintf("receive_zdata(): DATA CRC32: given    %08x\n", crc32);
      debugPrintf("receive_zdata(): DATA CRC32: computed %08x\n", packet.crc32);
    }
#   endif
    if (crc32 == packet.crc32) 
    {
      /*
       * CRC OK
       */
      crc_ok = true;
    }
  } 
  else 
  {
    /*
     * 16-bit CRC
     */
    packet.crc16 = compute_crc16(packet.crc16, packet.data, packet.data_n + 1);
    crc16 = ((packet.crc_buffer[1] & 0xFF) << 8) |
             (packet.crc_buffer[2] & 0xFF);
#   ifdef ZMODEM_DEBUG
    {
      debugPrintf("receive_zdata(): DATA CRC16: given    %04x\n", crc16);
      debugPrintf("receive_zdata(): DATA CRC16: computed %04x\n", packet.crc16);
    }
#   endif
    if (crc16 == packet.crc16) 
    {
      /*
       * CRC OK
       */
      crc_ok = true;
    }
  }
  if (crc_ok == true) 
  {
#   ifdef ZMODEM_DEBUG
      debugPrintf("receive_zdata(): DATA CRC: OK\n");
#   endif
    if (prior_state == ZMODEM_STATE_ZRPOS_WAIT) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("receive_zdata(): WRITE TO FILE %d BYTES\n", packet.data_n);
#     endif
      /*
       * Write the packet to file
       */
      file_stream.write(packet.data,packet.data_n);
      file_stream.flush();
      /*
       * Increment count
       */
      file_position += packet.data_n;
      block_size = packet.data_n;
      stats_increment_blocks();
      packet.data_n = 0;
      packet.crc16 = 0;
      packet.crc32 = compute_crc32(0, NULL, 0);
      if (acknowledge == true) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zdata(): ZACK required\n");
#       endif
        options = big_to_little_endian(file_position);
        build_packet(ZMODEM_PKT_ZACK, options, output, output_n, output_max);
      }
      if (end_of_packet == true) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("receive_zdata(): PACKET EOF\n");
#       endif
        state = ZMODEM_STATE_ZRPOS_WAIT;
        return false;
      }
    }
  } 
  else 
  {
    /*
     * CRC error
     */
#   ifdef ZMODEM_DEBUG
      debugPrintf("receive_zdata(): CRC ERROR\n");
#   endif
    if (prior_state == ZMODEM_STATE_ZRPOS_WAIT) 
    {
      /*
       * CRC error in the packet
       */
      stats_increment_errors();
      /*
       * Send ZMODEM_STATE_ZRPOS
       */
      packet_buffer_n = 0;
      options = file_position;
      build_packet(ZMODEM_PKT_ZRPOS, options, output, output_n, output_max);
      state = ZMODEM_STATE_ZRPOS_WAIT;
#     ifdef ZMODEM_DEBUG
        debugPrintf("receive_zdata(): send ZRPOS file_position = %ld\n",
            file_position);
#     endif
      return true;
    } 
    else 
    if (prior_state == ZMODEM_STATE_ZRINIT_WAIT) 
    {
      /*
       * CRC error in the packet
       */
      stats_increment_errors();
      /*
       * Send ZNAK
       */
      packet_buffer_n = 0;
      options = 0;
      build_packet(ZMODEM_PKT_ZNAK, options, output, output_n, output_max);
      state = ZMODEM_STATE_ZRINIT_WAIT;
#     ifdef ZMODEM_DEBUG
        debugPrintf("receive_zdata(): send ZNAK, back to zrinit to get filename\n");
#     endif
      return true;
    } 
    else 
    {
      /*
       * Some other state.  This is a coding error.  Save a log message,
       */
#     ifdef ZMODEM_DEBUG
        debugPrintf("receive_zdata(): CRC error, but in incorrect protocol state!\n");
#     endif
    }
  }
# ifdef ZMODEM_DEBUG
  {
    debugPrintf("receive_zdata(): DATA (post-processed): ");
    for (i = 0; i < packet.data_n; i++)
        debugPrintf("%02x ", (packet.data[i] & 0xFF));
    debugPrintf("\n");
  }
# endif
  /*
   * Figure out the next state to transition to
   */
  if (prior_state == ZMODEM_STATE_ZRINIT_WAIT) 
  {
    switch (packet.type) 
    {
      case ZMODEM_PKT_ZFILE:
        state = ZMODEM_STATE_ZFILE;
        break;
      case ZMODEM_PKT_ZSINIT:
        state = ZMODEM_STATE_ZRINIT_WAIT;
        /*
         * Send ZACK
         */
        options = 0;
        build_packet(ZMODEM_PKT_ZACK, options, output, output_n, output_max);
        return true;
      case ZMODEM_PKT_ZCOMMAND:
        state = ZMODEM_STATE_ZRINIT_WAIT;
        /*
         * Send ZCOMPL, assume it failed
         */
        options = 1;
        build_packet(ZMODEM_PKT_ZCOMPL, options, output, output_n, output_max);
        return true;
      default:
        state = ZMODEM_STATE_ZDATA;
        break;
    }
  } 
  else 
  {
    /*
     * We came here from ZMODEM_STATE_ZRPOS_WAIT
     */
    state = ZMODEM_STATE_ZDATA;
  }
  /*
   * Process through the new state
   */
  return false;
}

/**
 * Receive:  ZMODEM_STATE_ZSKIP
 *
 * @param output a buffer to write bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum size of the output buffer
 * @return true if we are done reading input and are ready to send bytes to
 * the remote side
 */
bool ZModem::receive_zskip(unsigned char * output,
                            unsigned int * output_n,
                            const unsigned int output_max) 
{
  uint32_t options = 0;
  struct utimbuf utime_buffer;

# ifdef ZMODEM_DEBUG
    debugPrintf("receive_zskip()\n");
# endif
  /*
   * Close existing file handle, reset file fields...
   */
  file_stream.close();
  /*
   * Set access and modification times
   */
  utime_buffer.actime = file_modtime;
  utime_buffer.modtime = file_modtime;
  utime(file_fullname, &utime_buffer);

  assert(file_name != NULL);
  free(file_name);
  file_name = NULL;
  file_stream = NULL;
  /*
   * Send out ZMODEM_STATE_ZSKIP packet
   */
  build_packet(ZMODEM_PKT_ZSKIP, options, output, output_n, output_max);
  /*
   * ZMODEM_STATE_ZSKIP will be followed immediately by another ZFILE, which is handled
   * in receive_zrinit_wait().
   */
  prior_state = ZMODEM_STATE_ZSKIP;
  state = ZMODEM_STATE_ZRINIT_WAIT;
  packet_buffer_n = 0;
  return false;
}

/**
 * Receive a file via the Zmodem protocol.
 *
 * @param input the bytes from the remote side
 * @param input_n the number of bytes in input_n
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
void ZModem::zmodem_receive(unsigned char * input, unsigned int input_n,
                           unsigned char * output, unsigned int * output_n,
                           const unsigned int output_max) 
{
  bool done;
  /*
  #ifdef ZMODEM_DEBUG
      debugPrintf("zmodem_receive() NOISE ON\n");
  #endif
   * int noise = 0;
   * noise++;
   * if (noise > 30) {
   *     noise = 0;
   *     input[0] = 0xaa;
   * }
   */
  unsigned int i;
# ifdef ZMODEM_DEBUG
  {
    debugPrintf("zmodem_receive() START packet_buffer_n = %d packet_buffer = ",
        packet_buffer_n);
    for (i = 0; i < packet_buffer_n; i++)
      debugPrintf("%02x ", (packet_buffer[i] & 0xFF));
    debugPrintf("\n");
  }
# endif
  done = false;
  while (done == false) 
  {
    /*
     * Add input_n to packet_buffer
     */
    if (input_n > sizeof(packet_buffer) - packet_buffer_n) 
    {
      memcpy(packet_buffer + packet_buffer_n, input,
             sizeof(packet_buffer) - packet_buffer_n);
      memmove(input, input + sizeof(packet_buffer) - packet_buffer_n,
              input_n - (sizeof(packet_buffer) - packet_buffer_n));
      input_n -= (sizeof(packet_buffer) - packet_buffer_n);
      packet_buffer_n = sizeof(packet_buffer);
    } 
    else 
    {
      memcpy(packet_buffer + packet_buffer_n, input, input_n);
      packet_buffer_n += input_n;
      input_n = 0;
    }
    switch (state) 
    {
      case ZMODEM_STATE_INIT:
        /*
         * This state is where everyone begins.  Start with ZCHALLENGE or
         * ZRINIT.
         */
        state = ZMODEM_STATE_ZRINIT;
        packet.crc16 = 0;
        packet.crc32 = compute_crc32(0, NULL, 0);
        break;
      case ZMODEM_STATE_ZCHALLENGE:
        done = receive_zchallenge(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZCHALLENGE_WAIT:
        done = receive_zchallenge_wait(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZCRC:
        done = receive_zcrc(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZCRC_WAIT:
        done = receive_zcrc_wait(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZRINIT:
        done = receive_zrinit(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZRINIT_WAIT:
        done = receive_zrinit_wait(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZRPOS:
        done = receive_zrpos(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZRPOS_WAIT:
        done = receive_zrpos_wait(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZFILE:
        done = receive_zfile();
        break;
      case ZMODEM_STATE_ZSKIP:
        done = receive_zskip(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZDATA:
        done = receive_zdata(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZFIN_WAIT:
#       ifdef ZMODEM_DEBUG
          debugPrintf("zmodem_receive(): ZFIN\n");
#       endif
        /*
         * NOP
         */
        done = true;
        state = ZMODEM_STATE_COMPLETE;
        break;
      case ZMODEM_STATE_ABORT:
      case ZMODEM_STATE_COMPLETE:
        /*
         * NOP
         */
        done = true;
        break;
      case ZMODEM_STATE_ZFILE_WAIT:
      case ZMODEM_STATE_ZSINIT:
      case ZMODEM_STATE_ZSINIT_WAIT:
      case ZMODEM_STATE_ZRQINIT:
      case ZMODEM_STATE_ZRQINIT_WAIT:
      case ZMODEM_STATE_ZFIN:
      case ZMODEM_STATE_ZEOF:
      case ZMODEM_STATE_ZEOF_WAIT:
        /*
         * Receive should NEVER see these states
         */
        abort();
        break;
    } /* switch (state) */

#   ifdef ZMODEM_DEBUG
      debugPrintf("zmodem_receive(): done = %s\n",
          (done == true ? "true" : "false"));
#   endif
  } /* while (done == false) */
# ifdef ZMODEM_DEBUG
  {
    debugPrintf("zmodem_receive() END packet_buffer_n = %d packet_buffer = ",
        packet_buffer_n);
    for (i = 0; i < packet_buffer_n; i++)
        debugPrintf("%02x ", (packet_buffer[i] & 0xFF));
    debugPrintf("\n");
  }
# endif
}

/**
 * Send:  ZMODEM_STATE_ZRQINIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
bool ZModem::send_zrqinit(unsigned char * output,
                           unsigned int * output_n,
                           const unsigned int output_max) 
{
  uint32_t options;
# ifdef ZMODEM_DEBUG
    debugPrintf("send_zrqinit()\n");
# endif
  options = 0;
  build_packet(ZMODEM_PKT_ZRQINIT, options, output, output_n, output_max);
  state = ZMODEM_STATE_ZRQINIT_WAIT;
  packet_buffer_n = 0;
  return false;
}

/**
 * Send:  ZMODEM_STATE_ZRQINIT_WAIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
bool ZModem::send_zrqinit_wait(unsigned char * output,
                                unsigned int * output_n,
                                const unsigned int output_max) 
{
  ZMODEM_PARSE_PACKET rc_pp;
  int discard;
  uint32_t options = 0;

# ifdef ZMODEM_DEBUG
    debugPrintf("send_zrqinit_wait()\n");
# endif
  if (packet_buffer_n > 0) 
  {
    rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);
    /*
     * Take the bytes off the stream
     */
    if (discard > 0) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("send_zrqinit_wait(): packet_buffer_n = %d discard = %d\n",
              packet_buffer_n, discard);
#     endif
      memmove(packet_buffer, packet_buffer + discard,
              packet_buffer_n - discard);
      packet_buffer_n -= discard;
    }
    if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("send_zrqinit_wait(): ERROR garbled header\n");
#     endif
      stats_increment_errors();
      packet_buffer_n = 0;
      build_packet(ZMODEM_PKT_ZNAK, options, output, output_n, output_max);
      return true;
    }
    if (rc_pp == ZM_PP_NODATA)
      return true;

    if (rc_pp == ZM_PP_OK)
    {
      if (packet.type == ZMODEM_PKT_ZRINIT) 
      {
        /*
         * See what options were specified
         */
        if (packet.argument & ZMODEM_TX_ESCAPE_CTRL) 
        {
          flags |= ZMODEM_TX_ESCAPE_CTRL;
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zrqinit_wait() ZRINIT ZMODEM_TX_ESCAPE_CTRL\n");
#          endif
        }
        if (packet.argument & ZMODEM_TX_ESCAPE_8BIT) 
        {
          flags |= ZMODEM_TX_ESCAPE_8BIT;
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zrqinit_wait() ZRINIT ZMODEM_TX_ESCAPE_8BIT\n");
#         endif
        }
        if (packet.argument & ZMODEM_TX_CAN_FULL_DUPLEX) 
        {
          flags |= ZMODEM_TX_CAN_FULL_DUPLEX;
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zrqinit_wait() ZRINIT ZMODEM_TX_CAN_FULL_DUPLEX\n");
#         endif
        }
        if (packet.argument & ZMODEM_TX_CAN_OVERLAP_IO) 
        {
          flags |= ZMODEM_TX_CAN_OVERLAP_IO;
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zrqinit_wait() ZRINIT ZMODEM_TX_CAN_OVERLAP_IO\n");
#         endif
        }
        if (packet.argument & ZMODEM_TX_CAN_BREAK) 
        {
          flags |= ZMODEM_TX_CAN_BREAK;
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zrqinit_wait() ZRINIT ZMODEM_TX_CAN_BREAK\n");
#         endif
        }
        if (packet.argument & ZMODEM_TX_CAN_DECRYPT) 
        {
          flags |= ZMODEM_TX_CAN_DECRYPT;
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zrqinit_wait() ZRINIT ZMODEM_TX_CAN_DECRYPT\n");
#         endif
        }
        if (packet.argument & ZMODEM_TX_CAN_LZW) 
        {
          flags |= ZMODEM_TX_CAN_LZW;
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zrqinit_wait() ZRINIT ZMODEM_TX_CAN_LZW\n");
#         endif
        }
        if (packet.argument & ZMODEM_TX_CAN_CRC32) 
        {
          flags |= ZMODEM_TX_CAN_CRC32;
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zrqinit_wait() ZRINIT ZMODEM_TX_CAN_CRC32\n");
#         endif
          use_crc32 = true;
        }
        /*
         * Update the encode map
         */
        setup_encode_byte_map();
        /*
         * Now switch to ZMODEM_STATE_ZSINIT
         */
        state = ZMODEM_STATE_ZSINIT;
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZCHALLENGE) 
      {
        /*
         * Respond to ZMODEM_STATE_ZCHALLENGE, remain in ZMODEM_STATE_ZRINIT_WAIT
         */
        options = packet.argument;
        build_packet(ZMODEM_PKT_ZACK, options, output, output_n, output_max);
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZNAK) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zrqinit_wait(): ERROR ZNAK\n");
#       endif
        stats_increment_errors();
        state = ZMODEM_STATE_ZRQINIT;
      } 
      else 
      {
        /*
         * Sender isn't Zmodem compliant, abort.
         */
        state = ZMODEM_STATE_ABORT;
        return true;
      }
    }
    /*
     * Process through the new state
     */
    return false;
  }
  if (check_timeout() == true) 
  {
      state = ZMODEM_STATE_ZRQINIT;
      return false;
  }
  /*
   * No data, done
   */
  return true;
}

/**
 * Send:  ZMODEM_STATE_ZSINIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
bool ZModem::send_zsinit(unsigned char * output,
                          unsigned int * output_n,
                          const unsigned int output_max) 
{
  uint32_t options;
# ifdef ZMODEM_DEBUG
    debugPrintf("send_zsinit()\n");
# endif
  /*
   * Escape ctrl characters by default, but not 8bit characters
   */
  if ((flags & ZMODEM_TX_ESCAPE_CTRL) == 0) 
  {
    options = ZMODEM_TX_ESCAPE_CTRL;
    build_packet(ZMODEM_PKT_ZSINIT, options, output, output_n, output_max);
    state = ZMODEM_STATE_ZSINIT_WAIT;
    /*
     * This is where I could put an attention string
     */
    /*
     * snprintf(packet.data, sizeof(packet.data) - 1, "SOMETHING HERE");
     */
    packet.data[0] = 0x0;
    packet.data_n = strlen((char *) packet.data) + 1;
    /*
     * Make sure we continue to use the right CRC
     */
    packet.use_crc32 = false;
    /*
     * Now encode
     */
    encode_zdata_bytes(output, output_n, output_max, ZMODEM_DT_ZCRCW);
  } 
  else 
  {
    /*
     * Head straight into file upload
     */
    state = ZMODEM_STATE_ZFILE;
  }
  packet_buffer_n = 0;
  return false;
}

/**
 * Send:  ZMODEM_STATE_ZSINIT_WAIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
bool ZModem::send_zsinit_wait(unsigned char * output,
                               unsigned int * output_n,
                               const unsigned int output_max) {

  ZMODEM_PARSE_PACKET rc_pp;
  int discard;
  uint32_t options = 0;
# ifdef ZMODEM_DEBUG
    debugPrintf("send_zsinit_wait()\n");
# endif
  if (packet_buffer_n > 0) 
  {
    rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

    /*
     * Take the bytes off the stream
     */
    if (discard > 0) 
    {
      memmove(packet_buffer, packet_buffer + discard,
              packet_buffer_n - discard);
      packet_buffer_n -= discard;
    }
    if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("send_zsinit_wait(): ERROR garbled header\n");
#     endif
      stats_increment_errors();
      packet_buffer_n = 0;
      build_packet(ZMODEM_PKT_ZNAK, options, output, output_n, output_max);
      return true;
    }
    if (rc_pp == ZM_PP_NODATA)
        return true;
    if (rc_pp == ZM_PP_OK) 
    {
      if (packet.type == ZMODEM_PKT_ZACK) 
      {
        /*
         * I'd love to wait a full second here...
         */
        /*
         * Switch to ZMODEM_STATE_ZFILE
         */
        state = ZMODEM_STATE_ZFILE;
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZNAK) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zsinit_wait(): ERROR ZNAK\n");
#       endif
        stats_increment_errors();
        state = ZMODEM_STATE_ZSINIT;
      } 
      else 
      {
        /*
         * Sender isn't Zmodem compliant, abort.
         */
        state = ZMODEM_STATE_ABORT;
        return true;
      }
    }
    /*
     * Process through the new state
     */
    return false;
  }    
  if (check_timeout() == true) 
  {
    state = ZMODEM_STATE_ZSINIT;
    return false;
  }
  /*
   * No data, done
   */
  return true;
}

/**
 * Send:  ZMODEM_STATE_ZFILE
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
bool ZModem::send_zfile(unsigned char * output,
                         unsigned int * output_n,
                         const unsigned int output_max) 
{
    uint32_t options;
#   ifdef ZMODEM_DEBUG
        debugPrintf("send_zfile()\n");
#   endif
  /*
   * Send header for the next file
   */
  options = 0;
  build_packet(ZMODEM_PKT_ZFILE, options, output, output_n, output_max);
  state = ZMODEM_STATE_ZFILE_WAIT;
  /*
   * Put together the filename info
   */
  snprintf((char *) packet.data, sizeof(packet.data) - 1,
           "%s %d %lo 0 0 1 %d", file_name, file_size,
           file_modtime, file_size);
  /*
   * Include the NUL terminator
   */
  packet.data_n = strlen((char *) packet.data) + 1;
  packet.data[strlen(file_name)] = 0x00;
  /*
   * Make sure we continue to use the right CRC
   */
  packet.use_crc32 = use_crc32;
  /*
   * Now encode the filename
   */
  encode_zdata_bytes(output, output_n, output_max, ZMODEM_DT_ZCRCW);
  packet_buffer_n = 0;
  return false;
}

/**
 * Send:  ZMODEM_STATE_ZFILE_WAIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
bool ZModem::send_zfile_wait(unsigned char * output,
                              unsigned int * output_n,
                              const unsigned int output_max) {

  ZMODEM_PARSE_PACKET rc_pp;

  int discard;
  uint32_t options = 0;

# ifdef ZMODEM_DEBUG
      debugPrintf("send_zfile_wait()\n");
# endif

  if (packet_buffer_n > 0) 
  {
    rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);
    /*
     * Take the bytes off the stream
     */
    if (discard > 0) 
    {
      memmove(packet_buffer, packet_buffer + discard,
              packet_buffer_n - discard);
      packet_buffer_n -= discard;
    }

    if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) 
    {
#     ifdef ZMODEM_DEBUG
      debugPrintf("send_zfile_wait(): ERROR garbled header\n");
#     endif
      stats_increment_errors();
      packet_buffer_n = 0;
      build_packet(ZMODEM_PKT_ZNAK, options, output, output_n, output_max);
      return true;
    }
    if (rc_pp == ZM_PP_NODATA)
      return true;
    if (rc_pp == ZM_PP_OK) 
    {
      if (packet.type == ZMODEM_PKT_ZRPOS) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zfile_wait() ZRPOS packet.argument = %u file_size = %u\n",
              (uint32_t) packet.argument,
              (uint32_t) file_size);
#       endif
        /*
         * Sanity check the file size
         */
        if (packet.argument > file_size) 
        {
          /*
           * The receiver lied to me, so screw them.
           */
          state = ZMODEM_STATE_ABORT;
          return true;
        }
        /*
         * Seek to the desired location
         */
        file_position = packet.argument;
        file_stream.seek(file_position);
        /*
         * Send the ZDATA start
         */
        options = big_to_little_endian(file_position);
        build_packet(ZMODEM_PKT_ZDATA, options, output, output_n, output_max);
        prior_state = ZMODEM_STATE_ZFILE_WAIT;
        state = ZMODEM_STATE_ZDATA;
        ack_required = false;
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZNAK) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zfile_wait(): ERROR ZNAK\n");
#       endif
        stats_increment_errors();
        state = ZMODEM_STATE_ZFILE;
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZCRC) 
      {
        int total_bytes = 0;
        /*
         * Buffer for reading the file
         */
        unsigned char file_buffer[1];
        size_t file_buffer_n;
        /*
         * Save the original file position
         */
        off_t original_position = file_position;
        /*
         * Reset crc32
         */
        file_crc32 = compute_crc32(0, NULL, 0);
        /*
         * Seek to beginning of file
         */
        file_stream.seek(0);
        while ((file_stream.available()>0) && (total_bytes < packet.argument))
        {
          file_buffer_n = file_stream.read(file_buffer, sizeof(file_buffer));
          total_bytes += file_buffer_n;
          /*
           * I think I have a different CRC function from lrzsz...
           * I have to negate both here and below to get the same
           * value.
           */
          file_crc32 =
              ~compute_crc32(file_crc32, file_buffer,
                             file_buffer_n);
        }
        file_crc32 = ~file_crc32;
        /*
         * Seek back to the original location
         */
        file_stream.seek(original_position);
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zfile_wait() respond to ZCRC total_bytes = %d on-disk CRC32 = %08lx\n",
              total_bytes, (unsigned long) file_crc32);
#       endif
        /*
         * Send it as a ZCRC
         */
        options = file_crc32;
        build_packet(ZMODEM_PKT_ZCRC, options, output, output_n, output_max);
      } 
      else
      if (packet.type == ZMODEM_PKT_ZSKIP) 
      {
        file_stream.close();
        assert(file_name != NULL);
        free(file_name);
        file_name = NULL;
        file_stream = NULL;
        /*
         * Setup for the next file.
         */
        upload_file_list_i++;
        setup_for_next_file();
      } 
      else 
      {
        /*
         * Sender isn't Zmodem compliant, abort.
         */
        state = ZMODEM_STATE_ABORT;
        return true;
      }
    }
    /*
     * Process through the new state
     */
    return false;
  }
  if (check_timeout() == true) 
  {
    state = ZMODEM_STATE_ZFILE;
    return false;
  }
  /*
   * No data, done
   */
  return true;
}

/**
 * Send:  ZDATA
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
bool ZModem::send_zdata(unsigned char * output,
                         unsigned int * output_n,
                         const unsigned int output_max) {

  ZMODEM_PARSE_PACKET rc_pp;
  uint32_t options = 0;
  int discard;
  int rc;
  bool last_block = false;
  bool use_spare_packet = false;
  bool got_error = false;

# ifdef ZMODEM_DEBUG
  {
    debugPrintf("send_zdata(): DATA state=%d prior_state=%d packet.data_n=%d\n",
        state, prior_state, packet.data_n);
    debugPrintf("send_zdata(): waiting_for_ack = %s ack_required = %s reliable_link = %s confirmed_bytes = %u last_confirmed_bytes = %u block_size = %d blocks_ack_count = %d\n",
            (waiting_for_ack == true ? "true" : "false"),
            (ack_required == true ? "true" : "false"),
            (reliable_link == true ? "true" : "false"),
            confirmed_bytes, last_confirmed_bytes,
            block_size, blocks_ack_count);
  }
# endif

  /*
   * Check the input buffer first
   */
  if (packet_buffer_n > 0) 
  {
    rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);

    /*
     * Take the bytes off the stream
     */
    if (discard > 0) 
    {
      memmove(packet_buffer, packet_buffer + discard,
              packet_buffer_n - discard);
      packet_buffer_n -= discard;
    }

    if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("send_zdata(): ERROR garbled header\n");
#     endif
      stats_increment_errors();
      packet_buffer_n = 0;
      build_packet(ZMODEM_PKT_ZNAK, options, output, output_n, output_max);
      return true;
    }

    if (rc_pp == ZM_PP_NODATA)
        return true;

    if (rc_pp == ZM_PP_OK) 
    {
      if (packet.type == ZMODEM_PKT_ZSKIP) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zdata() ZMODEM_STATE_ZSKIP\n");
#       endif

        /*
         * This is the proper way to skip a file - head to ZEOF
         */

        /*
         * Send an empty ZMODEM_DT_ZCRCW on recovery
         */
        outbound_packet_n = 0;
        *output_n = 0;

        /*
         * Make sure we continue to use the right CRC
         */
        packet.use_crc32 = use_crc32;

        /*
         * Encode directly to output
         */
        encode_zdata_bytes(output, output_n, output_max, ZMODEM_DT_ZCRCW);
        state = ZMODEM_STATE_ZEOF;
        /*
         * Process through the new state
         */
        return false;
      }
      if (packet.type == ZMODEM_PKT_ZRPOS) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zdata() ZMODEM_STATE_ZRPOS\n");
#       endif
        if (ack_required == false) 
        {
          /*
           * This is the first ZRPOS that indicates an error.
           */
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zdata(): ERROR ZRPOS\n");
#         endif
          stats_increment_errors();
          /*
           * Send a ZMODEM_DT_ZCRCW.  That part is handled below.
           */
          ack_required = true;
          waiting_for_ack = false;
          /*
           * Throw away everything that is still in the buffer so
           * we can start with the empty ZMODEM_DT_ZCRCW packet.
           */
          *output_n = 0;
          outbound_packet_n = 0;
          streaming_zdata = false;  
          packet_buffer_n = 0;
          got_error = true;
        } 
        else 
        {
          /*
           * lrz will send a second ZRPOS, but Hyperterm does not
           * when the user hits 'Skip file'.  I'm not sure which is
           * really correct, so handle both cases gracefully.
           */
          ack_required = false;
          waiting_for_ack = false;
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zdata(): 2nd ZRPOS in reponse to ZMODEM_DT_ZCRCW\n");
#         endif
        }
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zdata(): ZRPOS reposition to %u, file size is %u\n",
              (uint32_t) packet.argument,
              (uint32_t) file_size);
#       endif
        if (packet.argument <= file_size) 
        {
          /*
           * Record the confirmed bytes and use them to change
           * block size as needed.
           */
          confirmed_bytes = packet.argument;
          if (got_error == true) 
          {
            block_size_down();
            if (state == ZMODEM_STATE_ABORT) 
            {
#             ifdef ZMODEM_DEBUG
                debugPrintf("Transfer was cancelled, bye!\n");
#             endif
              return true;
            }
          }
          /*
           * Seek to the desired location
           */
          file_position = packet.argument;
          file_stream.seek(file_position);
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zdata() ZRPOS new file position: %lu\n",
                file_position);
#         endif
          /*
           * Send the ZDATA start.
           */
          options = big_to_little_endian(file_position);
          build_packet(ZMODEM_PKT_ZDATA, options, output, output_n,
                       output_max);
        } 
        else 
        if (packet.argument > file_size) 
        {
          /*
           * The receiver lied to me, so screw them.
           */
          state = ZMODEM_STATE_ABORT;
          return true;
        }
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZACK) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zdata() ZACK\n");
#       endif
        /*
         * See how much they acked
         */
        ack_required = false;
        waiting_for_ack = false;
        /*
         * Seek to the desired location
         */
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zdata() ZACK original file position: %lu\n",
              file_position);
#        endif
        /*
         * Hyperterm lies to me when the user clicks 'Skip file'
         */
        if (big_to_little_endian(packet.argument) > file_size) 
        {
#         ifdef ZMODEM_DEBUG
            debugPrintf("send_zdata() ZACK RECEIVER LIED ABOUT FILE POSITION\n");
#         endif
          /*
           * Treat this as ZEOF
           */
          state = ZMODEM_STATE_ZEOF;
          return false;
        }
        file_position = big_to_little_endian(packet.argument);
        /*
         * Normal case: file position is somewhere within the file
         */
        file_stream.seek(file_position);
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zdata() ZACK new file position: %lu\n",
              file_position);
#       endif
        /*
         * Record the confirmed bytes and use them to change block
         * size as needed.
         */
        confirmed_bytes = file_position;
        block_size_up();
        if (file_position == file_size) 
        {
          /*
           * Yippee, done
           */
          /*
           * Send ZEOF
           */
          state = ZMODEM_STATE_ZEOF;
          return false;
        } 
        else 
        {
          /*
           * Check to see if we need to begin a new frame or just
           * keep running with this one.
           */
          if (streaming_zdata == false) 
          {
#           ifdef ZMODEM_DEBUG
              debugPrintf("send_zdata() Send new ZDATA start at file position %lu\n",
                    file_position);
#           endif
            /*
             * Send the ZDATA start
             */
            options = big_to_little_endian(file_position);
            build_packet(ZMODEM_PKT_ZDATA, options, output, output_n,
                         output_max);
            streaming_zdata = true;
          }
        }
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZNAK) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zdata(): ERROR ZNAK\n");
#       endif
        stats_increment_errors();
        /*
         * Technically ZNAK on ZDATA is a protocol error.  I am
         * choosing to re-send starting from the last ack'd position
         * hoping for a recovery, but it would be just as valid to
         * cancel the transfer here.
         */
        state = ZMODEM_STATE_ZRPOS;
      } 
      else 
      {
        /*
         * Sender isn't Zmodem compliant, abort.
         */
        state = ZMODEM_STATE_ABORT;
        return true;
      }
    }
  } 
  else 
  {
    /*
     * No input data, see if we are waiting on the other side
     */
    if (waiting_for_ack == true) 
    {
      /*
       * We are waiting for a new ZRPOS, check timeout
       */
      if (check_timeout() == true) 
      {
        /*
         * Resend the ZMODEM_DT_ZCRCW for recovery
         */
        ack_required = true;
        waiting_for_ack = false;
      } 
      else 
      {
        /*
         * No timeout, exit out
         */
        return true;
      }
    }
  } /* if (packet_buffer_n > 0) */

  if ((waiting_for_ack == false) && (ack_required == false)) 
  {
    /*
     * Send more data if it's available (or we are right at the end) AND
     * there is room in the output buffer.
     */
    if (((file_stream.available()>0) || (file_stream.position() == file_size)) 
    && (outbound_packet_n == 0)) 
    {
      if (output_max - *output_n < (2 * block_size)) 
      {
        /*
         * There isn't enough space in output, instead put the data
         * in outbound_packet where it will be queued for later.
         */
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zdata(): switch to outbound_packet\n");
#       endif
        use_spare_packet = true;
        assert(outbound_packet_n == 0);
      }
#     ifdef ZMODEM_DEBUG
        debugPrintf("send_zdata(): read %d bytes from file\n",
            block_size);
#     endif
      rc = file_stream.read(packet.data, block_size);
      if (rc < 0) 
      {
        state = ZMODEM_STATE_ABORT;
        return true;
      } 
      else 
      if ((rc < block_size) || (rc == 0)) 
      {
        /*
         * Last packet, woo!
         */
        last_block = true;
        file_position = file_size;
      } 
      else 
        file_position += block_size;
      packet.data_n = rc;
      /*
       * Increment count
       */
      stats_increment_blocks();
      if (use_spare_packet == true) 
      {
        assert(outbound_packet_n == 0);
        if (last_block == true) 
        {
          /*
           * ZMODEM_DT_ZCRCW on last block
           */

          /*
           * Make sure we continue to use the right CRC
           */
          packet.use_crc32 = use_crc32;
          encode_zdata_bytes(outbound_packet, &outbound_packet_n,
              sizeof(outbound_packet), ZMODEM_DT_ZCRCW);
          waiting_for_ack = true;
        } 
        else 
        {
          /*
           * Check window size
           */
          blocks_ack_count--;
          if (blocks_ack_count == 0) 
          {
#           ifdef ZMODEM_DEBUG
              debugPrintf("send_zdata(): Require a ZACK via ZMODEM_DT_ZCRCQ \n");
#           endif
            /*
             * Require a ZACK via ZMODEM_DT_ZCRCQ
             */
            if (reliable_link == true)
              blocks_ack_count = ZMODEM_WINDOW_SIZE_RELIABLE;
            else
                blocks_ack_count = ZMODEM_WINDOW_SIZE_UNRELIABLE;
            waiting_for_ack = true;
            streaming_zdata = true;
            /*
             * Make sure we continue to use the right CRC
             */
            packet.use_crc32 = use_crc32;
            encode_zdata_bytes(outbound_packet, &outbound_packet_n,
                sizeof(outbound_packet), ZMODEM_DT_ZCRCQ);
          } 
          else 
          {
#           ifdef ZMODEM_DEBUG
              debugPrintf("send_zdata(): Keep streaming with ZMODEM_DT_ZCRCG \n");
#           endif
            /*
             * ZMODEM_DT_ZCRCG otherwise
             */

            /*
             * Make sure we continue to use the right CRC
             */
            packet.use_crc32 = use_crc32;

            encode_zdata_bytes(outbound_packet, &outbound_packet_n,
                sizeof(outbound_packet), ZMODEM_DT_ZCRCG);
          }
        }
      } 
      else 
      {
        if (last_block == true) 
        {
          /*
           * ZMODEM_DT_ZCRCW on last block
           */
          /*
           * Make sure we continue to use the right CRC
           */
          packet.use_crc32 = use_crc32;
          encode_zdata_bytes(output, output_n, output_max, ZMODEM_DT_ZCRCW);
          waiting_for_ack = true;
        } 
        else 
        {
          /*
           * Check window size
           */
          blocks_ack_count--;
          if (blocks_ack_count == 0) 
          {
#           ifdef ZMODEM_DEBUG
              debugPrintf("send_zdata(): Require a ZACK via ZMODEM_DT_ZCRCQ \n");
#           endif
            /*
             * Require a ZACK via ZMODEM_DT_ZCRCQ
             */
            if (reliable_link == true) 
                blocks_ack_count = ZMODEM_WINDOW_SIZE_RELIABLE;
            else 
                blocks_ack_count = ZMODEM_WINDOW_SIZE_UNRELIABLE;
            waiting_for_ack = true;
            streaming_zdata = true;
            /*
             * Make sure we continue to use the right CRC
             */
            packet.use_crc32 = use_crc32;
            encode_zdata_bytes(output, output_n, output_max, ZMODEM_DT_ZCRCQ);
          } 
          else 
          {
#           ifdef ZMODEM_DEBUG
              debugPrintf("send_zdata(): Keep streaming with ZMODEM_DT_ZCRCG \n");
#           endif
            /*
             * ZMODEM_DT_ZCRCG otherwise
             */

            /*
             * Make sure we continue to use the right CRC
             */
            packet.use_crc32 = use_crc32;

            encode_zdata_bytes(output, output_n, output_max, ZMODEM_DT_ZCRCG);
          }
        }
      }
    } /* if ((!feof(file_stream)) && (outbound_packet_n == 0)) */
  } 
  else 
  if ((ack_required == true) && (waiting_for_ack == false)) 
  {
#   ifdef ZMODEM_DEBUG
      debugPrintf("send_zdata(): Send empty ZMODEM_DT_ZCRCW on recovery \n");
#   endif
    /*
     * Send empty ZMODEM_DT_ZCRCW on recovery
     */
    packet.data_n = 0;
    if ((outbound_packet_n > 0) && (sizeof(outbound_packet) - outbound_packet_n > 32))
    {
      /*
       * Encode to the other buffer
       */
      /*
       * Make sure we continue to use the right CRC
       */
      packet.use_crc32 = use_crc32;
      encode_zdata_bytes(outbound_packet, &outbound_packet_n,
          sizeof(outbound_packet), ZMODEM_DT_ZCRCW);
      waiting_for_ack = true;
    } 
    else 
    if (output_max - *output_n > 32) 
    {
      /*
       * Encode directly to output
       */

      /*
       * Make sure we continue to use the right CRC
       */
      packet.use_crc32 = use_crc32;

      encode_zdata_bytes(output, output_n, output_max, ZMODEM_DT_ZCRCW);

      waiting_for_ack = true;
    }
  }
  /*
   * Force the queue to fill up on this call.
   */
  if (use_spare_packet == true)
      return false;
  /*
   * I either sent some data out, or I can't do anything else
   */
  return true;
}

/**
 * Send:  ZMODEM_STATE_ZEOF
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
bool ZModem::send_zeof(unsigned char * output,
                        unsigned int * output_n,
                        const unsigned int output_max) {
  uint32_t options;

# ifdef ZMODEM_DEBUG
    debugPrintf("send_zeof()\n");
# endif

  options = file_size;
  build_packet(ZMODEM_PKT_ZEOF, options, output, output_n, output_max);
  state = ZMODEM_STATE_ZEOF_WAIT;
  packet_buffer_n = 0;
  return false;
}

/**
 * Send:  ZMODEM_STATE_ZEOF_WAIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
bool ZModem::send_zeof_wait(unsigned char * output,
                             unsigned int * output_n,
                             const unsigned int output_max) 
{
  ZMODEM_PARSE_PACKET rc_pp;
  uint32_t options = 0;
  int discard;
# ifdef ZMODEM_DEBUG
    debugPrintf("send_zeof_wait()\n");
# endif
  /*
   * Check the input buffer first
   */
  if (packet_buffer_n > 0) 
  {
    rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);
    /*
     * Take the bytes off the stream
     */
    if (discard > 0) 
    {
      memmove(packet_buffer, packet_buffer + discard,
              packet_buffer_n - discard);
      packet_buffer_n -= discard;
    }
    if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("send_zeof_wait(): ERROR garbled header\n");
#     endif
      stats_increment_errors();
      packet_buffer_n = 0;
      build_packet(ZMODEM_PKT_ZNAK, options, output, output_n, output_max);
      return true;
    }
    if (rc_pp == ZM_PP_NODATA)
      return true;
    if (rc_pp == ZM_PP_OK) 
    {
      if (packet.type == ZMODEM_PKT_ZRINIT) 
      {
        /*
         * Yippee, done
         */
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zeof_wait() ZRINIT\n");
#       endif
        file_stream.close();
        assert(file_name != NULL);
        free(file_name);
        file_name = NULL;
        file_stream = NULL;
        /*
         * Setup for the next file.
         */
        upload_file_list_i++;
        setup_for_next_file();
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZNAK) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zeof_wait(): ERROR ZNAK\n");
#       endif
        stats_increment_errors();
        state = ZMODEM_STATE_ZEOF;

      } 
      else 
      {
        /*
         * Sender isn't Zmodem compliant, abort.
         */
        state = ZMODEM_STATE_ABORT;
        return true;
      }
    }

    /*
     * Process through the new state
     */
    return false;
  }

  if (check_timeout() == true) 
  {
    state = ZMODEM_STATE_ZEOF;
    return false;
  }

  /*
   * No data, done
   */
  return true;
}

/**
 * Send:  ZMODEM_STATE_ZFIN
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
bool ZModem::send_zfin(unsigned char * output,
                        unsigned int * output_n,
                        const unsigned int output_max) {

  uint32_t options;

# ifdef ZMODEM_DEBUG
    debugPrintf("send_zfin()\n");
# endif

  options = 0;
  build_packet(ZMODEM_PKT_ZFIN, options, output, output_n, output_max);
  state = ZMODEM_STATE_ZFIN_WAIT;
  packet_buffer_n = 0;
  return false;
}

/**
 * Send:  ZMODEM_STATE_ZFIN_WAIT
 *
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
bool ZModem::send_zfin_wait(unsigned char * output,
                             unsigned int * output_n,
                             const unsigned int output_max) 
{

  ZMODEM_PARSE_PACKET rc_pp;

  int discard;
  uint32_t options = 0;

# ifdef ZMODEM_DEBUG
    debugPrintf("send_zfin_wait()\n");
# endif
    
  if (packet_buffer_n > 0) 
  {
    rc_pp = parse_packet(packet_buffer, packet_buffer_n, &discard);
    /*
     * Take the bytes off the stream
     */
    if (discard > 0) 
    {
      memmove(packet_buffer, packet_buffer + discard,
              packet_buffer_n - discard);
      packet_buffer_n -= discard;
    }
    if ((rc_pp == ZM_PP_CRCERROR) || (rc_pp == ZM_PP_INVALID)) 
    {
#     ifdef ZMODEM_DEBUG
        debugPrintf("send_zfin_wait(): ERROR garbled header\n");
#     endif
      stats_increment_errors();
      packet_buffer_n = 0;
      build_packet(ZMODEM_PKT_ZNAK, options, output, output_n, output_max);
      return true;
    }

    if (rc_pp == ZM_PP_NODATA)
      return true;

    if (rc_pp == ZM_PP_OK) 
    {
      if (packet.type == ZMODEM_PKT_ZFIN) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zfin_wait(): ZFIN, sending Over-and-Out\n");
#       endif
        /*
         * Send Over-and-Out
         */
        output[0] = 'O';
        output[1] = 'O';
        *output_n = 2;
        /*
         * Now switch to COMPLETE
         */
        state = ZMODEM_STATE_COMPLETE;
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZNAK) 
      {
#       ifdef ZMODEM_DEBUG
          debugPrintf("send_zfin_wait(): ERROR ZNAK\n");
#       endif
        stats_increment_errors();
        state = ZMODEM_STATE_ZFIN;
      } 
      else 
      if (packet.type == ZMODEM_PKT_ZRINIT) 
      {
#       ifdef ZMODEM_DEBUG
            debugPrintf("send_zfin_wait(): ERROR ZRINIT\n");
#       endif
        stats_increment_errors();
        state = ZMODEM_STATE_ZFIN;
      } 
      else 
      {
        /*
         * Sender isn't Zmodem compliant, abort.
         */
        state = ZMODEM_STATE_ABORT;
        return true;
      }
    }
    /*
     * Process through the new state
     */
    return false;
  }

  if (check_timeout() == true) 
  {
    state = ZMODEM_STATE_ZFIN;
    return false;
  }

  /*
   * No data, done
   */
  return true;
}

/**
 * Send a file via the Zmodem protocol.
 *
 * @param input the bytes from the remote side
 * @param input_n the number of bytes in input_n
 * @param output a buffer to contain the bytes to send to the remote side
 * @param output_n the number of bytes that this function wrote to output
 * @param output_max the maximum number of bytes this function may write to
 * output
 */
void ZModem::zmodem_send(unsigned char * input, unsigned int input_n,
                        unsigned char * output, unsigned int * output_n,
                        const unsigned int output_max) {
  unsigned int i;
  bool done;
  int can_count = 0;

  done = false;
  while (done == false) 
  {
#   ifdef ZMODEM_DEBUG
    {
      debugPrintf("zmodem_send() START input_n = %d output_n = %d\n", input_n,
            *output_n);
      debugPrintf("zmodem_send() START packet_buffer_n = %d packet_buffer = ",
              packet_buffer_n);
      for (i = 0; i < packet_buffer_n; i++)
          debugPrintf("%02x ", (packet_buffer[i] & 0xFF));
      debugPrintf("\n");
    }
#   endif

    /*
     * Add input_n to packet_buffer
     */
    if (input_n > sizeof(packet_buffer) - packet_buffer_n) 
    {
      memcpy(packet_buffer + packet_buffer_n, input,
             sizeof(packet_buffer) - packet_buffer_n);
      memmove(input, input + sizeof(packet_buffer) - packet_buffer_n,
              input_n - (sizeof(packet_buffer) - packet_buffer_n));
      input_n -= (sizeof(packet_buffer) - packet_buffer_n);
      packet_buffer_n = sizeof(packet_buffer);
    }
    else 
    {
      memcpy(packet_buffer + packet_buffer_n, input, input_n);
      packet_buffer_n += input_n;
      input_n = 0;
    }

    /*
     * Scan for 4 consecutive C_CANs
     */
    for (i = 0; i < packet_buffer_n; i++) 
    {
      if (packet_buffer[i] != C_CAN)
        can_count = 0;
      else
        can_count++;
      if (can_count >= 4) 
      {
        /*
         * Receiver has killed the transfer
         */
        state = ZMODEM_STATE_ABORT;
      }
    }

    if (outbound_packet_n > 0) 
    {
      /*
       * Dispatch whatever is in outbound_packet
       */
      int n = output_max - *output_n;
      if (n > outbound_packet_n)
        n = outbound_packet_n;

#     ifdef ZMODEM_DEBUG
      {
        debugPrintf("zmodem_send() dispatch only %d bytes outbound_packet\n", n);
        debugPrintf("zmodem_send() output_max %d output_n %d n %d\n",
                output_max, *output_n, n);
        debugPrintf("zmodem_send() outbound_packet: %d bytes: ",
                outbound_packet_n);
        for (i = 0; i < outbound_packet_n; i++)
            debugPrintf("%02x ", (outbound_packet[i] & 0xFF));
        debugPrintf("\n");
      }
#     endif

      if (n > 0) 
      {
        memcpy(output + *output_n, outbound_packet, n);
        memmove(outbound_packet, outbound_packet + n,
                outbound_packet_n - n);
        outbound_packet_n -= n;
        *output_n += n;
      }

      /*
       * Do nothing else
       */
      done = true;
    } 
    else 
    {
      switch (state) 
      {
      case ZMODEM_STATE_INIT:
        /*
         * This state is where everyone begins.  Start with ZMODEM_STATE_ZRQINIT
         */
        state = ZMODEM_STATE_ZRQINIT;
        break;
      case ZMODEM_STATE_ZSINIT:
        done = send_zsinit(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZSINIT_WAIT:
        done = send_zsinit_wait(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZRQINIT:
        done = send_zrqinit(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZRQINIT_WAIT:
        done = send_zrqinit_wait(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZFILE:
        done = send_zfile(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZFILE_WAIT:
        done = send_zfile_wait(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZDATA:
        done = send_zdata(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZEOF:
        done = send_zeof(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZEOF_WAIT:
        done = send_zeof_wait(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZFIN:
        done = send_zfin(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ZFIN_WAIT:
        done = send_zfin_wait(output, output_n, output_max);
        break;
      case ZMODEM_STATE_ABORT:
      case ZMODEM_STATE_COMPLETE:
        /*
         * NOP
         */
        done = true;
        break;
      case ZMODEM_STATE_ZCRC:
      case ZMODEM_STATE_ZCRC_WAIT:
      case ZMODEM_STATE_ZRINIT:
      case ZMODEM_STATE_ZRINIT_WAIT:
      case ZMODEM_STATE_ZRPOS:
      case ZMODEM_STATE_ZRPOS_WAIT:
      case ZMODEM_STATE_ZCHALLENGE:
      case ZMODEM_STATE_ZCHALLENGE_WAIT:
      case ZMODEM_STATE_ZSKIP:
        /*
         * Send should NEVER see these states
         */
        abort();
        break;
      } /* switch (state) */
    }

#   ifdef ZMODEM_DEBUG
      debugPrintf("zmodem_send(): done = %s\n",
          (done == true ? "true" : "false"));
#   endif

  } /* while (done == false) */


  /*
  #ifdef ZMODEM_DEBUG
      debugPrintf("zmodem_send() NOISE\n");
  #endif
   * int noise = 0;
   * noise++;
   * if (noise > 30) {
   *     noise = 0;
   *     output[0] = 0xaa;
   * }
   */

# ifdef ZMODEM_DEBUG
  {
    debugPrintf("zmodem_send() END packet_buffer_n = %d packet_buffer = ",
        packet_buffer_n);
    for (i = 0; i < packet_buffer_n; i++)
      debugPrintf("%02x ", (packet_buffer[i] & 0xFF));
    debugPrintf("\n");
  }
# endif

}

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
void ZModem::zmodem_process(unsigned char * input, const unsigned int input_n,
                            unsigned char * output, unsigned int * output_n,
                            const unsigned int output_max) {

  unsigned int i;

  /*
   * Check my input arguments
   */
  assert(input_n >= 0);
  assert(input != NULL);
  assert(output != NULL);
  assert(*output_n >= 0);
  assert(output_max > ZMODEM_MAX_BLOCK_SIZE);

  if ((state == ZMODEM_STATE_ABORT) || (state == ZMODEM_STATE_COMPLETE))
    return;

# ifdef ZMODEM_DEBUG
  {
    debugPrintf("ZMODEM: state = %d input_n = %d output_n = %d\n", state,
        input_n, *output_n);
    debugPrintf("ZMODEM: %d input bytes:  ", input_n);
    for (i = 0; i < input_n; i++)
      debugPrintf("%02x ", (input[i] & 0xFF));
    debugPrintf("\n");
  }
# endif

  if (input_n > 0) 
  {
    /*
     * Something was sent to me, so reset timeout
     */
    reset_timer();
  }

  if (sending == false)
    zmodem_receive(input, input_n, output, output_n, output_max);
  else
    zmodem_send(input, input_n, output, output_n, output_max);

# ifdef ZMODEM_DEBUG
  {
    debugPrintf("ZMODEM: %d output bytes: ", *output_n);
    for (i = 0; i < *output_n; i++)
      debugPrintf("%02x ", (output[i] & 0xFF));
    debugPrintf("\n");
  }
# endif

  /*
   * Reset the timer if we sent something
   */
  if (*output_n > 0)
    reset_timer();
}

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
bool ZModem::zmodem_start(File file_list[], const char * pathname,
                    const bool send, const ZMODEM_FLAVOR in_flavor) 
{

  int i;

  /*
   * If I got here, then I know that all the files in file_list exist.
   * forms.c ensures the files are all readable by me.
   */

  /*
   * Verify that file_list is set when send is true
   */
  if (send == true)
    assert(file_list != NULL);
  else
    assert(file_list == NULL);

  /*
   * Assume we don't start up successfully
   */
  state = ZMODEM_STATE_ABORT;

  upload_file_list = file_list;
  upload_file_list_i = 0;

# ifdef ZMODEM_DEBUG
  {
    debugPrintf("ZMODEM: START flavor = %d pathname = \'%s\'\n", in_flavor,
        pathname);
    if (upload_file_list != NULL) {
      for (i = 0; upload_file_list[i] != NULL; i++) {
        debugPrintf("upload_file_list[%d] = '%s'\n", i,
                upload_file_list[i].name().c_str());
      }
    }
  }
# endif

  sending = send;

  if (send == true) 
  {
    /*
     * Set up for first file
     */
    if (setup_for_next_file() == false)
      return false;
  }
  else 
  {
    /*
     * Save download path
     */
    download_path = Xstrdup(pathname, __FILE__, __LINE__);
    set_transfer_stats_filename("");
    set_transfer_stats_pathname(pathname);
  }

  if (in_flavor == ZMODEM_FLAVOR_CRC32) {
    makecrc();
    if (send != true) 
    {
      /*
       * We aren't allowed to send in CRC32 unless the receiver asks
       * for it.
       */
      use_crc32 = true;
    }
  } 
  else
    use_crc32 = false;

  state = ZMODEM_STATE_INIT;

  confirmed_bytes = 0;
  last_confirmed_bytes = 0;
  consecutive_errors = 0;

  /*
   * Set the window size
   */
  reliable_link = true;
  blocks_ack_count = ZMODEM_WINDOW_SIZE_RELIABLE;
  streaming_zdata = false;

  /*
   * Clear the packet buffer
   */
  packet_buffer_n = 0;
  outbound_packet_n = 0;

  /*
   * Setup timer
   */
  reset_timer();
  timeout_count = 0;

  /*
   * Initialize the encode map
   */
  setup_encode_byte_map();

# ifdef ZMODEM_DEBUG
    debugPrintf("ZMODEM: START OK\n");
# endif
  return true;
}

/**
 * Stop the file transfer.  Note that this function is only called in
 * stop_file_transfer() and save_partial is always true.  However it is left
 * in for API completeness.
 *
 * @param save_partial if true, save any partially-downloaded files.
 */
void ZModem::zmodem_stop(const bool save_partial) 
{
  char notify_message[256];
# ifdef ZMODEM_DEBUG
    debugPrintf("ZMODEM: STOP\n");
# endif
  if ((save_partial == true) || (sending == true)) 
  {
    if (file_stream != NULL) 
    {
      file_stream.flush();
      file_stream.close();
    }
  } 
  else 
  {
    if (file_stream != NULL) 
    {
      file_stream.close();
      fs->remove(file_stream);
    }
  }
  file_stream = NULL;
  if (file_name != NULL)
    free(file_name);
  file_name = NULL;
  if (download_path != NULL)
    free(download_path);
  download_path = NULL;
}

#endif