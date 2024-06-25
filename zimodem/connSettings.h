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
enum ConnFlag
{
  FLAG_DISCONNECT_ON_EXIT=1,
  FLAG_PETSCII=2,
  FLAG_TELNET=4,
  FLAG_ECHO=8,
  FLAG_XONXOFF=16,
  FLAG_SECURE=32,
  FLAG_RTSCTS=64
};

class ConnSettings
{
  public:
    bool petscii = false;
    bool telnet = false;
    bool echo = false;
    bool xonxoff = false;
    bool rtscts = false;
    bool secure = false;

    ConnSettings(int flagBitmap);
    ConnSettings(const char *dmodifiers);
    ConnSettings(String modifiers);
    int getBitmap();
    int getBitmap(FlowControlType forceCheck);
    void setFlag(ConnFlag flagMask, bool newVal);
    String getFlagString();

    static void IPtoStr(IPAddress *ip, String &str);
    static IPAddress *parseIP(const char *ipStr);
};
