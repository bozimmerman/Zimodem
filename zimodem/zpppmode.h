#ifndef ZIMODEM_ZPPPMODE_H
#define ZIMODEM_ZPPPMODE_H

#if INCLUDE_PPP

#define PPP_FLAG     0x7E
#define PPP_ESCAPE   0x7D
#define PPP_TRANS    0x20

#define PPP_PROTOCOL_LCP    0xC021
#define PPP_PROTOCOL_IPCP   0x8021
#define PPP_PROTOCOL_IP     0x0021

#define LCP_CONF_REQ    1
#define LCP_CONF_ACK    2
#define LCP_CONF_NAK    3
#define LCP_CONF_REJ    4
#define LCP_TERM_REQ    5
#define LCP_TERM_ACK    6
#define LCP_CODE_REJ    7
#define LCP_ECHO_REQ    8
#define LCP_ECHO_REPLY  9

enum PPPState
{
  PPP_DEAD,
  PPP_ESTABLISH,
  PPP_OPENED,
  PPP_TERMINATE
};

class ZPPPMode : public ZMode
{
private:
  uint8_t *buf = 0;
  int maxBufSize = 0;
  int curBufLen = 0;
  bool escaped = false;
  uint16_t fcs = 0xFFFF;

  PPPState state = PPP_DEAD;
  uint8_t lcpId = 0;
  uint8_t ipcpId = 0;
  bool lcpOpened = false;
  bool ipcpOpened = false;

  struct netif ppp_netif;
  struct netif *wifi_netif = NULL;

  void sendLCPPacket(uint8_t code, uint8_t id, uint8_t *data, int len);
  void sendIPCPPacket(uint8_t code, uint8_t id, uint8_t *data, int len);
  void sendPPPFrame(uint16_t protocol, uint8_t *data, int len);
  void handleLCP(uint8_t *data, int len);
  void handleIPCP(uint8_t *data, int len);
  void handleIPPacket(uint8_t *data, int len);
  void processPPPFrame(uint8_t *data, int len);
  uint16_t calculateFCS(uint8_t *data, int len);

public:
  netif_output_fn original_wifi_output = NULL;
  netif_input_fn original_wifi_input = NULL;

  void switchTo();
  void switchBackToCommandMode();
  void serialIncoming();
  void loop();
  void sendPacketToSerial(struct pbuf *p);
  void injectPacketToNetwork(uint8_t *data, int len);
};

#endif
#endif
