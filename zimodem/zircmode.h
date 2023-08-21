/*
 * zircmode.h
 *
 *  Created on: May 18, 2022
 *      Author: Bo Zimmerman
 */

#ifdef INCLUDE_IRCC
class ZIRCMode: public ZMode
{
private:
  ZSerial serial; // storage for serial settings only
  bool showMenu;
  bool savedEcho;
  String EOLN;
  const char *EOLNC;
  WiFiClientNode *current = null;
  unsigned long lastNumber;
  unsigned long timeout=0;
  String buf;
  String nick;
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
