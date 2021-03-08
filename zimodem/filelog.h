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

enum LogMode
{
  NADA=0,
  SocketIn=1,
  SocketOut=2,
  SerialIn=3,
  SerialOut=4
};

static unsigned long expectedSerialTime = 1000;

static bool logFileOpen = false;
static bool logFileDebug= false;
static File logFile; 

static void logSerialOut(const uint8_t c);
static void logSocketOut(const uint8_t c);
static void logSerialIn(const uint8_t c);
static void logSocketIn(const uint8_t c);
static void logPrint(const char* msg);
static void logPrintln(const char* msg);
static void logPrintf(const char* format, ...);
static void logPrintfln(const char* format, ...);
static bool *ISHEX(char *s);
static char *TOHEX(long a);
static char *TOHEX(int a);
static char *TOHEX(unsigned int a);
static char *TOHEX(unsigned long a);
static char *tohex(uint8_t a);
static char *TOHEX(uint8_t a);
static uint8_t FROMHEX(uint8_t a1, uint8_t a2);

