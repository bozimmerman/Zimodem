/*
 * zslipmode.h
 *
 *  Created on: May 17, 2022
 *      Author: Bo Zimmerman
 */

#ifdef INCLUDE_SLIP
#include "lwip/raw.h"

class ZSLIPMode: public ZMode
{
private:
  void switchBackToCommandMode();
  static const char SLIP_END = '\xc0';
  static const char SLIP_ESC = '\xdb';
  static const char SLIP_ESC_END = '\xdc';
  static const char SLIP_ESC_ESC = '\xdd';
  String encodeSLIP(uint8_t *ipPacket, int ipLen);
  String inPacket;
  bool started=false;
  bool escaped=false;

public:
  void switchTo();
  void serialIncoming();
  void loop();
};

#endif /* INCLUDE_SLIP_ */
