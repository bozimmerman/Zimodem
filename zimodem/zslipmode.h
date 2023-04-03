/*
 * zslipmode.h
 *
 *  Created on: May 17, 2022
 *      Author: Bo Zimmerman
 */

#ifdef INCLUDE_SLIP

class ZSLIPMode: public ZMode
{
private:
  void switchBackToCommandMode();

public:
  void switchTo();
  void serialIncoming();
  void loop();
};

#endif /* INCLUDE_SLIP_ */
