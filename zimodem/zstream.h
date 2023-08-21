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

#define ZSTREAM_ESC_BUF_MAX 10

class ZStream : public ZMode
{
  private:
    WiFiClientNode *current = null;
    unsigned long lastNonPlusTimeMs = 0;
    unsigned long currentExpiresTimeMs = 0;
    unsigned long nextFlushMs = 0;
    int plussesInARow = 0;
    ZSerial serial;
    int lastDTR = 0;
    uint8_t escBuf[ZSTREAM_ESC_BUF_MAX];
    unsigned long nextAlarm = millis() + 5000;
    
    void switchBackToCommandMode(bool logout);
    void socketWrite(uint8_t c);
    void socketWrite(uint8_t *buf, uint8_t len);
    void baudDelay();

    bool isPETSCII();
    bool isEcho();
    FlowControlType getFlowControl();
    bool isTelnet();
    bool isDisconnectedOnStreamExit();

 
  public:
    
    void switchTo(WiFiClientNode *conn);
    
    void serialIncoming();
    void loop();
};

