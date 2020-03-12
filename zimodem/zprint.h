/*
   Copyright 2020-2020 Bo Zimmerman

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

enum PrintPayloadType
{
  PETSCII,
  ASCII,
  RAW
};

static unsigned int DEFAULT_DELAY_MS = 5000;

class ZPrint : public ZMode
{
  private:
    WiFiClientNode *current = null;
    unsigned int timeoutDelayMs = DEFAULT_DELAY_MS;
    char *lastPrinterSpec = 0;
    unsigned long currentExpiresTimeMs = 0;
    unsigned long nextFlushMs = 0;
    PrintPayloadType payloadType = PETSCII;
    size_t pdex=0;
    size_t coldex=0;
    char pbuf[256];
    ZSerial serial;
    char lastLastC = 0;
    char lastC = 0;

    size_t writeStr(char *s);
    size_t writeChunk(char *s, int len);
    void switchBackToCommandMode(bool error);

  public:

    ZResult switchTo(char *vbuf, int vlen, bool petscii);
    void setLastPrinterSpec(const char *spec);
    char *getLastPrinterSpec();
    void setTimeoutDelayMs(int ms);
    int getTimeoutDelayMs();

    void serialIncoming();
    void loop();
};

