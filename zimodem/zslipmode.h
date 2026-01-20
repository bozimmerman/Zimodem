#if INCLUDE_SLIP
/*
   Copyright 2022-2026 Bo Zimmerman

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
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/ip.h"

static ZSerial sserial;
class ZSLIPMode: public ZMode
{
private:
  void switchBackToCommandMode();
  bool escaped=false;
  int curBufLen = 0;
  int maxBufSize = 4096;
  uint8_t *buf = 0;
  struct netif slip_netif;
  struct netif *wifi_netif;


public:
  static const char SLIP_END = '\xc0';
  static const char SLIP_ESC = '\xdb';
  static const char SLIP_ESC_END = '\xdc';
  static const char SLIP_ESC_ESC = '\xdd';
  netif_input_fn original_wifi_input;
  netif_output_fn original_wifi_output;

  void switchTo();
  void serialIncoming();
  void loop();

  void sendPacketToSerial(struct pbuf *p);
  void injectPacketToNetwork(uint8_t *data, int len);
};

#endif /* INCLUDE_SLIP */
