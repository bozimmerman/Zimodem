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

#define LAST_PACKET_BUF_SIZE 256
#define OVERFLOW_BUF_SIZE 256

class WiFiClientNode : public Stream
{
  private:
    void finishConnectionLink();
    WiFiClient client;
    WiFiClient *clientPtr;

  public:
    int id=0;
    char *host;
    int port;
    bool wasConnected=false;
    bool serverClient=false;
    int flagsBitmap = 0;
    char *delimiters = NULL;
    char *maskOuts = NULL;
    uint8_t one[1];
    uint8 lastPacketBuf[LAST_PACKET_BUF_SIZE];
    int lastPacketLen=0;
    uint8_t overflowBuf[OVERFLOW_BUF_SIZE];
    int overflowBufLen = 0;
    WiFiClientNode *next = null;

    WiFiClientNode(char *hostIp, int newport, int flagsBitmap);
    WiFiClientNode(WiFiClient newClient, int flagsBitmap);
    ~WiFiClientNode();
    bool isConnected();
    bool isPETSCII();
    bool isEcho();
    bool isXonXoff();
    bool isTelnet();
    bool isDisconnectedOnStreamExit();
    size_t write(uint8_t c);
    size_t write(const uint8_t *buf, size_t size);
    int read();
    int peek();
    void flush();
    int available();
    int read(uint8_t *buf, size_t size);
};

class PhoneBookEntry
{
  public:
    unsigned long number;
    const char *address;
    const char *modifiers;
    PhoneBookEntry *next = null;
    
    PhoneBookEntry(unsigned long phnum, const char *addr, const char *mod);
    ~PhoneBookEntry();

    static void loadPhonebook();
    static void clearPhonebook();
    static void savePhonebook();
};



