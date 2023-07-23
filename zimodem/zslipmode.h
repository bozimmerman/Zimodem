/*
 * zslipmode.h
 *
 *  Created on: May 17, 2022
 *      Author: Bo Zimmerman
 */

#ifdef INCLUDE_SLIP
#include "lwip/raw.h"
#include "src/missing/slipif.h"

static ZSerial sserial;
class ZSLIPMode: public ZMode
{
private:
  void switchBackToCommandMode();
  String inPacket;
  bool started=false;
  bool escaped=false;
  raw_pcb *_pcb = 0;

public:
  static const char SLIP_END = '\xc0';
  static const char SLIP_ESC = '\xdb';
  static const char SLIP_ESC_END = '\xdc';
  static const char SLIP_ESC_ESC = '\xdd';
  void switchTo();
  void serialIncoming();
  void loop();
};

#endif /* INCLUDE_SLIP_ */
