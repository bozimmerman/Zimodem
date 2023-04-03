/*
 * zslipmode.ino
 *
 *  Created on: May 17, 2022
 *      Author: Bo Zimmerman
 */
#ifdef INCLUDE_SLIP


void ZSLIPMode::switchBackToCommandMode()
{
  currMode = &commandMode;
}

void ZSLIPMode::switchTo()
{
  currMode=&slipMode;
  debugPrintf("Switched to SLIP mode\n\r");
}

void ZSLIPMode::serialIncoming()
{
}

void ZSLIPMode::loop()
{
}

#endif /* INCLUDE_SLIP_ */
