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

#define ZSTREAM_ESC_BUF_MAX 10

enum HangupType
{
  HANGUP_NONE,
  HANGUP_PPPHARD,
  HANGUP_DTR,
  HANGUP_PDP
};

class ZStream : public ZMode
{
  private:
    WiFiClientNode *current = null;
    unsigned long lastNonPlusTimeMs = 0;
    unsigned long currentExpiresTimeMs = 0;
    unsigned long nextFlushMs = 0;
    int plussesInARow = 0;
    ZSerial serial;
    HangupType hangupType = HANGUP_NONE;
    int lastDTR = 0;
    bool defaultEcho=false;
    int lastPDP = 0;
    uint8_t escBuf[ZSTREAM_ESC_BUF_MAX];
    unsigned long switchAlarm = millis() + 5000;
    
    void switchBackToCommandMode(bool pppMode);
    void socketWrite(uint8_t c);
    void socketWrite(uint8_t *buf, uint8_t len);
    void baudDelay();

    bool isPETSCII();
    bool isEcho();
    FlowControlType getFlowControl();
    bool isTelnet();
    bool isDefaultEcho();
    bool isDisconnectedOnStreamExit();
    void doHangupChecks();

  public:
    void setDefaultEcho(bool tf);
    void switchTo(WiFiClientNode *conn);
    void setHangupType(HangupType type);
    HangupType getHangupType();
    
    void serialIncoming();
    void loop();
};

