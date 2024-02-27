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

// courtesy Craig Bruce, 1995

unsigned char petToAscTable[256] PROGMEM = {
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x14,0x09,0x0d,0x11,0x93,0x0a,0x0e,0x0f,
0x10,0x0b,0x12,0x13,0x08,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x5b,0x5c,0x5d,0x5e,0x5f,
0x60,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x7b,0x7c,0x7d,0x7e,0x7f,
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
0x90,0x91,0x92,0x0c,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
0x60,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf
};

unsigned char ascToPetTable[256] PROGMEM = {
0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x14,0xa0,0x0a,0x11,0x93,0x0d,0x0e,0x0f,
0x10,0x0b,0x12,0x13,0x08,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
0x40,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0x5b,0x5c,0x5d,0x5e,0x5f,
0xc0,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0xdb,0xdc,0xdd,0xde,0xdf,
0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
0x90,0x91,0x92,0x0c,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};

#define TELNET_BINARY 0 
  /** TELNET CODE: echo */
#define TELNET_ECHO 1 
  /** TELNET CODE: echo */
#define TELNET_LOGOUT 18 
  /** TELNET CODE: supress go ahead*/
#define TELNET_SUPRESS_GO_AHEAD 3 
  /** TELNET CODE: sending terminal type*/
#define TELNET_TERMTYPE 24 
  /** TELNET CODE: Negotiate About Window Size.*/
#define TELNET_NAWS 31 
  /** TELNET CODE: Remote Flow Control.*/
#define TELNET_TOGGLE_FLOW_CONTROL 33 
  /** TELNET CODE: Linemode*/
#define TELNET_LINEMODE 34 
  /** TELNET CODE: MSDP protocol*/
#define TELNET_MSDP 69 
  /** TELNET CODE: MSSP Server Status protocol*/
#define TELNET_MSSP 70 
  /** TELNET CODE: text compression, protocol 1*/
#define TELNET_COMPRESS 85 
  /** TELNET CODE: text compression, protocol 2*/
#define TELNET_COMPRESS2 86 
  /** TELNET CODE: MSP SOund protocol*/
#define TELNET_MSP 90 
  /** TELNET CODE: MXP Extended protocol*/
#define TELNET_MXP 91 
  /** TELNET CODE: AARD protocol*/
#define TELNET_AARD 102 
  /** TELNET CODE: End of subnegotiation parameters*/
#define TELNET_SE 240 
  /** TELNET CODE: Are You There*/
#define TELNET_AYT 246 
  /** TELNET CODE: Erase character*/
#define TELNET_EC 247 
  /** TELNET CODE: ATCP protocol*/
#define TELNET_ATCP 200 
  /** TELNET CODE: GMCP protocol*/
#define TELNET_GMCP 201 
  /** TELNET CODE: Indicates that what follows is subnegotiation of the indicated option*/
#define TELNET_SB 250 
  /** TELNET CODE: Indicates the desire to begin performing, or confirmation that you are now performing, the indicated option*/
#define TELNET_WILL 251 
  /** TELNET CODE: Indicates the refusal to perform, or continue performing, the indicated option*/
#define TELNET_WONT 252 
  /** TELNET CODE: Indicates the request that the other party perform, or confirmation that you are expecting the other party to perform, the indicated option*/
#define TELNET_DO 253 
  /** TELNET CODE: 253 doubles as fake ansi telnet code*/
#define TELNET_ANSI 253 
  /** TELNET CODE: Indicates the demand that the other party stop performing, or confirmation that you are no longer expecting the other party to perform, the indicated option.*/
#define TELNET_DONT 254 
  /** TELNET CODE: Indicates that the other party can go ahead and transmit -- I'm done.*/
#define TELNET_GA 249 
  /** TELNET CODE: Indicates that there is nothing to do?*/
#define TELNET_NOP 241 
  /** TELNET CODE: IAC*/
#define TELNET_IAC 255 


uint8_t streamAvailRead(Stream *stream)
{
  int ct=0;
  while((stream->available()==0)
  &&(ct++)<250)
    delay(1);
  return stream->read();
}

bool handleAsciiIAC(char *c, Stream *stream)
{
  if(*c == 255)
  {
    *c=streamAvailRead(stream);
    logSocketIn(*c);
    if(*c==TELNET_IAC)
    {
      *c = 255;
      return true;
    }
    if(*c==TELNET_WILL)
    {
      char what=streamAvailRead(stream);
      logSocketIn(what);
      uint8_t iacDont[] = {TELNET_IAC, TELNET_DONT, what};
      if(what == TELNET_TERMTYPE)
        iacDont[1] = TELNET_DO;
      for(int i=0;i<3;i++)
        logSocketOut(iacDont[i]);
      stream->write(iacDont,3);
      return false;
    }
    if(*c==TELNET_DONT)
    {
      char what=streamAvailRead(stream);
      logSocketIn(what);
      uint8_t iacWont[] = {TELNET_IAC, TELNET_WONT, what};
      for(int i=0;i<3;i++)
        logSocketOut(iacWont[i]);
      stream->write(iacWont,3);
      return false;
    }
    if(*c==TELNET_WONT)
    {
      char what=streamAvailRead(stream);
      logSocketIn(what);
      return false;
    }
    if(*c==TELNET_DO)
    {
      char what=streamAvailRead(stream);
      logSocketIn(what);
      uint8_t iacWont[] = {TELNET_IAC, TELNET_WONT, what};
      if(what == TELNET_TERMTYPE)
        iacWont[1] = TELNET_WILL;
      for(int i=0;i<3;i++)
        logSocketOut(iacWont[i]);
      stream->write(iacWont,3);
      return false;
    }
    if(*c==TELNET_SB)
    {
      char what=streamAvailRead(stream);
      logSocketIn(what);
      char lastC=*c;
      while(((lastC!=TELNET_IAC)||(*c!=TELNET_SE))&&(*c>=0))
      {
        lastC=*c;
        *c=streamAvailRead(stream);
        logSocketIn(*c);
      }
      if(what == TELNET_TERMTYPE)
      {
        int respLen = termType.length() + 6;
        uint8_t buf[respLen];
        buf[0]=TELNET_IAC;
        buf[1]=TELNET_SB;
        buf[2]=TELNET_TERMTYPE;
        buf[3]=(uint8_t)0;
        sprintf((char *)buf+4,termType.c_str());
        buf[respLen-2]=TELNET_IAC;
        buf[respLen-1]=TELNET_SE;
        for(int i=0;i<respLen;i++)
          logSocketOut(buf[i]);
        stream->write(buf,respLen);
        return false;
      }
    }
    return false;
  }
  return true;
}

bool ansiColorToPetsciiColor(char *c, Stream *stream)
{
  if(*c == 27)
  {
    *c=streamAvailRead(stream);
    logSocketIn(*c);
    if(*c=='[')
    {
      int code1=0;
      int code2=-1;
      *c=streamAvailRead(stream);
      logSocketIn(*c);
      while((*c>='0')&&(*c<='9'))
      {
        code1=(code1*10) + (*c-'0');
        *c=streamAvailRead(stream);
        logSocketIn(*c);
      }
      while(*c==';')
      {
        *c=streamAvailRead(stream);
        logSocketIn(*c);
        if((*c>='0')&&(*c<='9'))
          code2=0;
        while((*c>='0')&&(*c<='9'))
        {
          code2=(code2*10) + (*c-'0');
          *c=streamAvailRead(stream);
          logSocketIn(*c);
        }
      }
      switch(code1)
      {
      case 0:
        // dark...
        switch(code2)
        {
        case -1:
        case 0:
          *c=  146; // rvs off
          return true;
        case 30: // black
          *c=  155;//144;
          return true;
        case 31: // red
          *c=  28;
          return true;
        case 32: // green
          *c=  30;
          return true;
        case 33: // yellow
          *c=  129;
          return true;
        case 34: // blue
          *c=  31;
          return true;
        case 35: // purple
          *c=  149;
          return true;
        case 36: // cyan
          *c=  152;
          return true;
        case 37: // white/grey
        default: 
          *c= 155;
          return true;
        }
        break;
      case 1:
        // light..
        switch(code2)
        {
        case -1:/* bold */ 
          *c=  0;
          return true;
        case 0:
          *c=  146; // rvs off
          return true;
        case 30: // black
          *c=  151;
          return true;
        case 31: // red
          *c=  150;
          return true;
        case 32: // green
          *c=  153;
          return true;
        case 33: // yellow
          *c=  158;
          return true;
        case 34: // blue
          *c=  154;
          return true;
        case 35: // purple
          *c=  156;
          return true;
        case 36: // cyan
          *c=  159;
          return true;
        case 37: // white/grey
        default: 
          *c=  5;
          return true;
        }
        break;
      case 2: /*?*/
      case 3: /*?*/
      case 4: /*underline*/
      case 5: /*blink*/
      case 6: /*italics*/
        *c=0;
        return true;
      case 40: case 41: case 42: case 43: case 44: 
      case 45: case 46: case 47: case 48: case 49:
        *c=18; // rvs on
        return true;
      }
    }
    *c = 0;
    return true;
  }
  return false;
}

char petToAsc(char c)
{
  return pgm_read_byte_near(petToAscTable + (int)c);
}

bool ascToPet(char *c, Stream *stream)
{
  if(!ansiColorToPetsciiColor(c,stream))
    *c = pgm_read_byte_near(ascToPetTable + (int)(*c));
  return true;
}

char ascToPetcii(char c)
{
  return pgm_read_byte_near(ascToPetTable + (int)c);
}

