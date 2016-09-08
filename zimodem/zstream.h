/*
   Copyright 2016-2016 Bo Zimmerman

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

class ZStream : public ZMode
{
  private:
    WiFiClientNode *current = null;
    unsigned long lastNonPlusTimeMs = 0;
    unsigned long currentExpiresTimeMs = 0;
    int plussesInARow=0;
    bool disconnectOnExit=true;
    bool petscii=false;
    bool telnet=false;
    bool XON=true;
    long nextAlarm = millis() + 5000;
    int logFileCtrW=0;
    int logFileCtrR=0;

    void switchBackToCommandMode(bool logout);
    void serialWrite(uint8_t c);
    void serialDeque();

  public:
    bool doBBS=false;
    
    void switchTo(WiFiClientNode *conn);
    void switchTo(WiFiClientNode *conn, bool dodisconnect, bool doPETSCII, bool doTelnet);
    void switchTo(WiFiClientNode *conn, bool dodisconnect, bool doPETSCII, bool doTelnet, bool bbsMode);

    void serialIncoming();
    void loop();
};

