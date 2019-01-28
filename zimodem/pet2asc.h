/*
   Copyright 2016-2019 Bo Zimmerman

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
#ifdef ZIMODEM_ESP32
# include <WiFi.h>
# define ENC_TYPE_NONE WIFI_AUTH_OPEN
# include <HardwareSerial.h>
# include <SPIFFS.h>
# include <Update.h>
# include "SD.h"
# include "SPI.h"
# include "driver/uart.h"
  static HardwareSerial HWSerial(UART_NUM_2);
#else
# include "ESP8266WiFi.h"
# define HWSerial Serial
#endif

#include <FS.h>

char petToAsc(char c);
bool ascToPet(char *c, Stream *stream);
char ascToPetcii(char c);
bool handleAsciiIAC(char *c, Stream *stream);

static void setCharArray(char **target, const char *src)
{
  if(src == NULL)
    return;
  if(*target != NULL)
    free(*target);
  *target = (char *)malloc(strlen(src)+1);
  strcpy(*target,src);
}

static void freeCharArray(char **arr)
{
  if(*arr == NULL)
    return;
  free(*arr);
  *arr = NULL;
}

static int modifierCompare(const char *match1, const char *match2)
{
  if(strlen(match1) != strlen(match2))
    return -1;
    
  for(int i1=0;i1<strlen(match1);i1++)
  {
    char c1=tolower(match1[i1]);
    bool found=false;
    for(int i2=0;i2<strlen(match2);i2++)
    {
      char c2=tolower(match2[i2]);
      found = found || (c1==c2);
    }
    if(!found)
      return -1;
  }
  return 0;
}

