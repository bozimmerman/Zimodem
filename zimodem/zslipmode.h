/*
 * zslipmode.h
 *
 *  Created on: May 17, 2022
 *      Author: Bo Zimmerman
 */

#ifdef INCLUDE_SLIP
#ifndef ZIMODEM_ZSLIPMODE_H_
#define ZIMODEM_ZSLIPMODE_H_

class ZSLIPMode: public ZMode
{
private:
  void switchBackToCommandMode();

public:
  void switchTo();
  void serialIncoming();
  void loop();
};

#endif /* ZIMODEM_ZSLIPMODE_H_ */
#endif /* INCLUDE_SLIP_ */
