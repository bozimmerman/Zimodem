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

enum LogOutputState
{
  LOS_NADA=0,
  LOS_SocketIn=1,
  LOS_SocketOut=2,
  LOS_SerialIn=3,
  LOS_SerialOut=4
};

static unsigned long expectedSerialTime = 1000;

static bool logFileOpen = false;
static bool logFile2Uart= false;
static File logFile; 

static void logSerialOut(const uint8_t c);
static void logSocketOut(const uint8_t c);
static void logSerialIn(const uint8_t c);
static void logSocketIn(const uint8_t c);
static void logPrint(const char* msg);
static void logPrintln(const char* msg);
static void logPrintf(const char* format, ...);
static void logPrintfln(const char* format, ...);
static void logFileLoop();
static char *TOHEX(const char *s, char *hex, const size_t len);
static char *TOHEX(long a);
static char *TOHEX(int a);
static char *TOHEX(unsigned int a);
static char *TOHEX(unsigned long a);
static char *tohex(uint8_t a);
static char *TOHEX(uint8_t a);
static uint8_t FROMHEX(uint8_t a1, uint8_t a2);
static char *FROMHEX(const char *hex, char *s, const size_t len);
