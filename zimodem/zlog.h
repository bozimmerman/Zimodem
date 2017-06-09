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

static int logFileCtrR=0;
static int logFileCtrW=0;
static unsigned long expectedSerialTime = 1000;
static unsigned long lastSerialWrite = millis();
static unsigned long lastSerialRead = millis();
static unsigned long streamStartTime = millis();
static bool logFileOpen = false;
static File logFile; 

static void logSerialOut(uint8_t c);
static void logSocketOut(uint8_t c);
static char *TOHEX(long a);
static char *TOHEX(int a);
static char *TOHEX(unsigned int a);
static char *TOHEX(unsigned long a);
static char *TOHEX(uint8_t a);

