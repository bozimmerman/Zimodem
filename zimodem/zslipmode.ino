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

/**
 * Basic design: The modem in SLIP mode behaves as a network card would for its
 * host computer.  Packets received over serial from the host are forwarded to the
 * network by the modem, with the same ip address as the modem.  Packets received
 * from the network FOR the modem's ip address are assumed to be for the host computer,
 * and are thus sent over serial to the computer to deal with using its own stack.
 */
static err_t slip_wifi_output_hook(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)
{
  if(slipMode.original_wifi_output != NULL)
    return slipMode.original_wifi_output(netif, p, ipaddr);
  return ERR_OK;
}

static err_t slip_wifi_input_hook(struct pbuf *p, struct netif *inp)
{
  if((p != NULL) && (p->len >= 34))
  {
    uint8_t *payload = (uint8_t *)p->payload;
    uint8_t *data = payload + 14;
    uint8_t ip_version = (data[0] >> 4) & 0x0F;

    if(ip_version == 4)
    {
      uint32_t dest_ip = (data[16] << 24) | (data[17] << 16) | (data[18] << 8) | data[19];

      IPAddress wifiIP = WiFi.localIP();
      uint32_t our_ip = ((uint32_t)wifiIP[0] << 24) | ((uint32_t)wifiIP[1] << 16) |
                        ((uint32_t)wifiIP[2] << 8) | (uint32_t)wifiIP[3];
      if(dest_ip == our_ip)
      {
        pbuf_remove_header(p, 14);
        if(baudRate >= 57600)
          slipMode.sendPacketToSerial(p);
        else
        if((dest_ip & 0xF0000000) != 0xE0000000 && dest_ip != 0xFFFFFFFF)
          slipMode.sendPacketToSerial(p);
        pbuf_free(p);
        return ERR_OK;
      }
    }
  }

  if(slipMode.original_wifi_input != NULL)
    return slipMode.original_wifi_input(p, inp);

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
  int total = 0;
  while(q != NULL)
  {
    uint8_t *payload = (uint8_t *)q->payload;
    int len = q->len;
    total += len;
    if(len >= 20)
    {
      uint8_t ihl = (payload[0] & 0x0F) * 4;
      uint8_t protocol = payload[9];

      if(protocol == 1 && len >= ihl + 2)
      {
        uint8_t icmp_type = payload[ihl];
        uint8_t icmp_code = payload[ihl + 1];
        debugPrintf("SLIP-RX IP packet: ver=%d proto=%d src=%d.%d.%d.%d dst=%d.%d.%d.%d ICMP: type=%d code=%d\n",
                    (payload[0] >> 4) & 0x0F,
                    protocol,
                    payload[12], payload[13], payload[14], payload[15],
                    payload[16], payload[17], payload[18], payload[19],
                    icmp_type, icmp_code);
      }
      else
      {
        debugPrintf("SLIP-RX IP packet: ver=%d proto=%d src=%d.%d.%d.%d dst=%d.%d.%d.%d\n",
                    (payload[0] >> 4) & 0x0F,
                    protocol,
                    payload[12], payload[13], payload[14], payload[15],
                    payload[16], payload[17], payload[18], payload[19]);
      }
    }
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

      if((i % 64) == 0)
      {
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
  if(logFileOpen)
    logPrintf("SLIP-RX %d bytes\n", total);
  debugPrintf("SLIP-RX: %d bytes\n", total);
}

void ZSLIPMode::injectPacketToNetwork(uint8_t *data, int len)
{
  if(len < 20)
    return;

  debugPrintf("SLIP-TX IP packet: ver=%d proto=%d src=%d.%d.%d.%d dst=%d.%d.%d.%d len=%d\n",
              (data[0] >> 4) & 0x0F,
              data[9],
              data[12], data[13], data[14], data[15],
              data[16], data[17], data[18], data[19],
              len);

  if(wifi_netif == NULL || original_wifi_output == NULL)
    return;

  struct pbuf *p = pbuf_alloc(PBUF_IP, len, PBUF_RAM);
  if(p == NULL)
    return;
  memcpy(p->payload, data, len);

  ip4_addr_t dest;
  IP4_ADDR(&dest, data[16], data[17], data[18], data[19]);
  err_t err = original_wifi_output(wifi_netif, p, &dest);

  if(err != ERR_OK)
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
  yield();
  delay(999);

  while(HWSerial.available() > 0)
    HWSerial.read();

  wifi_netif = netif_list;
  while(wifi_netif != NULL)
  {
    // Look for the WiFi station interface (usually "st" or "en")
    if((wifi_netif->name[0] == 's' && wifi_netif->name[1] == 't')
    || (wifi_netif->name[0] == 'e' && wifi_netif->name[1] == 'n'))
      break;
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
      if(this->buf == 0)
        return;
      this->maxBufSize = 4096;
      this->curBufLen = 0;
    }

    if(c == SLIP_END)
    {
      if(this->curBufLen > 0)
      {
        if(logFileOpen)
          logPrintf("SLIP-TX: %d bytes\n", this->curBufLen);
        debugPrintf("SLIP-TX: %d bytes\n", this->curBufLen);

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
      if(this->curBufLen < this->maxBufSize)
        this->buf[this->curBufLen++] = SLIP_END;
      this->escaped = false;
    }
    else
    if((c == SLIP_ESC_ESC) && (this->escaped))
    {
      if(this->curBufLen < this->maxBufSize)
        this->buf[this->curBufLen++] = SLIP_ESC;
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
      if(this->curBufLen < this->maxBufSize)
        this->buf[this->curBufLen++] = c;
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
