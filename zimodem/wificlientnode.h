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

#define PACKET_BUF_SIZE 256

#ifdef ZIMODEM_ESP32
# include <WiFiClientSecure.h>
#endif
#ifdef INCLUDE_SSH
# include "wifisshclient.h"
#endif

static WiFiClient *createWiFiClient(bool SSL)
{
#ifdef ZIMODEM_ESP32
  if(SSL)
  {
    WiFiClientSecure *c = new WiFiClientSecure();
    c->setInsecure();
    return c;
  }
  else
#else
  //WiFiClientSecure *c = new WiFiClientSecure();
  //c->setInsecure();
#endif
  return new WiFiClient();
}

typedef struct Packet
{
  uint8_t num = 0;
  uint16_t len = 0;
  uint8_t buf[PACKET_BUF_SIZE] = {0};
};

class WiFiClientNode : public Stream
{
  private:
    void finishConnectionLink();
    int flushOverflowBuffer();
    void fillUnderflowBuf();
    WiFiClient client;
    WiFiClient *clientPtr;
    bool answered=true;
    int ringsRemain=0;
    unsigned long nextRingMillis = 0;
    unsigned long nextDisconnect = 0;
    void constructNode();
    void constructNode(char *hostIp, int newport, int flagsBitmap, int ringDelay);
    void constructNode(char *hostIp, int newport, char *username, char *password, int flagsBitmap, int ringDelay);

  public:
    int id=0;
    char *host;
    int port;
    bool wasConnected=false;
    bool serverClient=false;
    int flagsBitmap = 0;
    char *delimiters = NULL;
    char *maskOuts = NULL;
    char *stateMachine = NULL;
    char *machineState = NULL;
    String machineQue = "";

    uint8_t nextPacketNum=1;
    uint8_t blankPackets=0;
    struct Packet lastPacket[3]; // 0 = current buf, 1&2 are back-cache bufs
    //struct Packet underflowBuf; // underflows no longer handled this way
    WiFiClientNode *next = null;

    WiFiClientNode(char *hostIp, int newport, int flagsBitmap);
    WiFiClientNode(char *hostIp, int newport, char *username, char *password, int flagsBitmap);
    WiFiClientNode(WiFiClient newClient, int flagsBitmap, int ringDelay);
    ~WiFiClientNode();
    bool isConnected();

    FlowControlType getFlowControl();
    bool isPETSCII();
    bool isEcho();
    bool isTelnet();

    bool isAnswered();
    void answer();
    int ringsRemaining(int delta);
    unsigned long nextRingTime(long delta);
    void markForDisconnect();
    bool isMarkedForDisconnect();

    bool isDisconnectedOnStreamExit();
    void setDisconnectOnStreamExit(bool tf);

    void setNoDelay(bool tf);

    size_t write(uint8_t c);
    size_t write(const uint8_t *buf, size_t size);
    void print(String s);
    int read();
    int peek();
    void flush();
    void flushAlways();
    int available();
    int read(uint8_t *buf, size_t size);
    String readLine(unsigned int timeout);

    static int getNumOpenWiFiConnections();
    static void checkForAutoDisconnections();
};


