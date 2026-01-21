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

static err_t slip_wifi_output_hook(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)
{
  slipMode.sendPacketToSerial(p);
  if(slipMode.original_wifi_output != NULL) {
    return slipMode.original_wifi_output(netif, p, ipaddr);
  }

  return ERR_OK;
}

static err_t slip_wifi_input_hook(struct pbuf *p, struct netif *inp)
{
  if(baudRate >= 57600)
    slipMode.sendPacketToSerial(p);
  else
  if((p != NULL) && (p->len >= 20))
  {
    uint8_t *data = (uint8_t *)p->payload;
    uint8_t ip_version = (data[0] >> 4) & 0x0F;

    if(ip_version == 4)
    {
      uint32_t dest_ip = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];

      // Skip multicast (224.0.0.0/4) and broadcast
      if((dest_ip & 0xF0000000) != 0xE0000000 && dest_ip != 0xFFFFFFFF)
        slipMode.sendPacketToSerial(p);
    }
  }
  else
    slipMode.sendPacketToSerial(p);

  return ERR_OK;
}

// Dummy netif init for SLIP interface
static err_t slip_netif_init(struct netif *netif)
{
  netif->name[0] = 's';
  netif->name[1] = 'l';
  netif->mtu = 1500;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_LINK_UP;
  return ERR_OK;
}

void ZSLIPMode::sendPacketToSerial(struct pbuf *p)
{
  if(p == NULL) return;

  sserial.printb(SLIP_END);
  if(sserial.isSerialOut())
    serialOutDeque();

  struct pbuf *q = p;
  while(q != NULL)
  {
    uint8_t *payload = (uint8_t *)q->payload;
    int len = q->len;

    for(int i = 0; i < len; i++)
    {
      uint8_t c = payload[i];

      if(c == SLIP_END)
      {
        sserial.printb(SLIP_ESC);
        sserial.printb(SLIP_ESC_END);
      }
      else
      if(c == SLIP_ESC)
      {
        sserial.printb(SLIP_ESC);
        sserial.printb(SLIP_ESC_ESC);
      }
      else
        sserial.printb(c);

      if((i % 64) == 0) {
        yield();
        if(sserial.isSerialOut())
          serialOutDeque();
      }
    }

    q = q->next;
  }

  sserial.printb(SLIP_END);
  if(sserial.isSerialOut())
    serialOutDeque();
}

void ZSLIPMode::injectPacketToNetwork(uint8_t *data, int len)
{
  if(len < 20)
    return;

  struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
  if(p == NULL)
    return;

  memcpy(p->payload, data, len);
  p->len = len;
  p->tot_len = len;

  uint8_t ip_version = (data[0] >> 4) & 0x0F;
  if(ip_version == 4)
  {
    err_t err = ip4_input(p, &slip_netif);
    if(err != ERR_OK)
      pbuf_free(p);
  }
#if LWIP_IPV6
  else
  if(ip_version == 6)
  {
    err_t err = ip6_input(p, &slip_netif);
    if(err != ERR_OK)
      pbuf_free(p);
  }
#endif
  else
    pbuf_free(p);
}

void ZSLIPMode::switchBackToCommandMode()
{
  debugPrintf("\r\nMode:Command\r\n");

  if(wifi_netif != NULL && original_wifi_output != NULL)
    wifi_netif->output = original_wifi_output;
  if(wifi_netif != NULL && original_wifi_input != NULL)
    wifi_netif->input = original_wifi_input;

  netif_remove(&slip_netif);
  currMode = &commandMode;

  if(this->buf != 0)
  {
    free(this->buf);
    this->buf = 0;
  }
}

esp_netif_t* get_esp_interface_netif(esp_interface_t interface);

void ZSLIPMode::switchTo()
{
  debugPrintf("\r\nMode:SLIP\r\n");
  sserial.setFlowControlType(FCT_DISABLED);
  if(commandMode.getFlowControlType()==FCT_RTSCTS)
    sserial.setFlowControlType(FCT_RTSCTS);
  sserial.setPetsciiMode(false);
  sserial.setXON(true);

  this->curBufLen = 0;
  this->escaped = false;

  wifi_netif = netif_list;
  while(wifi_netif != NULL)
  {
    // Look for the WiFi station interface (usually "st" or "en")
    if((wifi_netif->name[0] == 's' && wifi_netif->name[1] == 't') ||
        (wifi_netif->name[0] == 'e' && wifi_netif->name[1] == 'n')) {
      break;
    }
    wifi_netif = wifi_netif->next;
  }

  if(wifi_netif == NULL)
    wifi_netif = netif_default;

  if(wifi_netif != NULL)
  {
    debugPrintf("SLIP: Using interface %c%c%d\n",
                wifi_netif->name[0], wifi_netif->name[1], wifi_netif->num);

    original_wifi_output = wifi_netif->output;
    wifi_netif->output = slip_wifi_output_hook;

    original_wifi_input = wifi_netif->input;
    wifi_netif->input = slip_wifi_input_hook;
  }

  ip4_addr_t ipaddr, netmask, gw;
  IPAddress wifiIP = WiFi.localIP();
  IPAddress wifiGW = WiFi.gatewayIP();
  IPAddress wifiMask = WiFi.subnetMask();

  if(wifiIP[0] != 0)
  {
    IP4_ADDR(&ipaddr, wifiIP[0], wifiIP[1], wifiIP[2], wifiIP[3]);
    IP4_ADDR(&gw, wifiGW[0], wifiGW[1], wifiGW[2], wifiGW[3]);
    IP4_ADDR(&netmask, wifiMask[0], wifiMask[1], wifiMask[2], wifiMask[3]);
  }
  else
  {
    IP4_ADDR(&ipaddr, 192, 168, 1, 1);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 1, 1);
  }
  debugPrintf("SLIP: Using WiFi IP: %s\n", ip4addr_ntoa(&ipaddr));
  debugPrintf("SLIP: Using WiFi Gateway: %s\n", ip4addr_ntoa(&gw));
  debugPrintf("SLIP: Using WiFi Netmask: %s\n", ip4addr_ntoa(&netmask));

  netif_add(&slip_netif, &ipaddr, &netmask, &gw, NULL, slip_netif_init, ip_input);
  netif_set_up(&slip_netif);
  netif_set_link_up(&slip_netif);

  currMode = &slipMode;
  debugPrintf("SLIP mode active\n\r");
}

void ZSLIPMode::serialIncoming()
{
  while(HWSerial.available() > 0)
  {
    uint8_t c = HWSerial.read();

    if(logFileOpen)
      logSerialIn(c);

    if(this->buf == 0)
    {
      this->buf = (uint8_t *)malloc(4096);
      if(this->buf == 0) {
        debugPrintf("SLIP: malloc failed\n");
        return;
      }
      this->maxBufSize = 4096;
      this->curBufLen = 0;
    }

    if(c == SLIP_END)
    {
      if(this->curBufLen > 0)
      {
        if(logFileOpen)
          logPrintln("SLIP-RX packet.");

        debugPrintf("SLIP-RX: %d bytes\n", this->curBufLen);

        injectPacketToNetwork(this->buf, this->curBufLen);

        this->curBufLen = 0;
        this->escaped = false;
      }
    }
    else
    if(c == SLIP_ESC)
    {
      this->escaped = true;
    }
    else
    if((c == SLIP_ESC_END) && (this->escaped))
    {
      if(this->curBufLen < this->maxBufSize) {
        this->buf[this->curBufLen++] = SLIP_END;
      }
      this->escaped = false;
    }
    else
    if((c == SLIP_ESC_ESC) && (this->escaped))
    {
      if(this->curBufLen < this->maxBufSize) {
        this->buf[this->curBufLen++] = SLIP_ESC;
      }
      this->escaped = false;
    }
    else
    if(this->escaped)
    {
      debugPrintf("SLIP Protocol Error: unexpected 0x%02X after ESC\n", c);
      if(logFileOpen)
        logPrintln("SLIP error.");
      this->curBufLen = 0;
      this->escaped = false;
    }
    else
    {
      if(this->curBufLen < this->maxBufSize) {
        this->buf[this->curBufLen++] = c;
      }
    }

    if(this->curBufLen >= this->maxBufSize)
    {
      int newSize = this->maxBufSize * 2;
      if(newSize > 65536)
      {
        debugPrintf("SLIP: packet too large, discarding\n");
        this->curBufLen = 0;
        this->escaped = false;
      }
      else
      {
        uint8_t *newBuf = (uint8_t *)malloc(newSize);
        if(newBuf != NULL)
        {
          memcpy(newBuf, this->buf, this->curBufLen);
          maxBufSize = newSize;
          free(this->buf);
          this->buf = newBuf;
        }
        else
          this->curBufLen = 0;
      }
    }
  }
}

void ZSLIPMode::loop()
{
  if(sserial.isSerialOut())
    serialOutDeque();
  logFileLoop();
}

#endif /* INCLUDE_SLIP */
