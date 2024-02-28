#ifdef INCLUDE_IRCC
/*
   Copyright 2022-2024 Bo Zimmerman

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

static const char *NICK_FILE       = "/znick.txt";

class ZIRCMode: public ZMode
{
private:
  ZSerial serial; // storage for serial settings only
  bool showMenu;
  bool savedEcho;
  String EOLN;
  const char *EOLNC;
  WiFiClientNode *current = null;
  bool debugRaw;
  unsigned long lastNumber;
  unsigned long timeout=0;
  String buf;
  String nick;
  String listFilter = "";
  String lastAddress;
  String lastOptions;
  String lastNotes;
  String channelName;
  bool joinReceived;
  enum ZIRCMenu
  {
    ZIRCMENU_MAIN=0,
    ZIRCMENU_NICK=1,
    ZIRCMENU_ADDRESS=2,
    ZIRCMENU_NUM=3,
    ZIRCMENU_NOTES=4,
    ZIRCMENU_OPTIONS=5,
    ZIRCMENU_COMMAND=6
  } currState;
  enum ZIRCState
  {
    ZIRCSTATE_WAIT=0,
    ZIRCSTATE_COMMAND=1
  } ircState;

  void switchBackToCommandMode();
  void doIRCCommand();
  void loopMenuMode();
  void loopCommandMode();

public:
  void switchTo();
  void serialIncoming();
  void loop();
};

#endif /* INCLUDE_IRCC */
