/*
 * zslipmode.ino
 *
 *  Created on: May 17, 2022
 *      Author: Bo Zimmerman
 */
#ifdef INCLUDE_SLIP

#include "zslipmode.h"
#include "lwip/ip.h"
#include "lwip/netif.h"
#include "netif/slipif.h"

void ZSLIPMode::switchBackToCommandMode()
{
  currMode = &commandMode;
}

void ZSLIPMode::switchTo()
{
  struct netif sl_netif;
  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;
  char int_no = 2;

  IP4_ADDR(&ipaddr, WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  IP4_ADDR(&netmask, WiFi.subnetMask()[0], WiFi.subnetMask()[1], WiFi.subnetMask()[2], WiFi.subnetMask()[3]);
  IP4_ADDR(&gw, WiFi.gatewayIP()[0], WiFi.gatewayIP()[1], WiFi.gatewayIP()[2], WiFi.gatewayIP()[3]);
  netif_add (&sl_netif, &ipaddr, &netmask, &gw, &int_no, slipif_init, ip_input);
  netif_set_up(&sl_netif);
  //ip_napt_enable(ipaddr.addr, 1);
  currMode=&slipMode;
}

void ZSLIPMode::serialIncoming()
{
}

void ZSLIPMode::loop()
{
  serialOutDeque();
  //switchBackToCommandMode();
}
#endif /* INCLUDE_SLIP_ */
