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

// Custom output hook for WiFi interface to capture outgoing packets
static err_t slip_wifi_output_hook(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)
{
  // Send a copy to serial via SLIP encoding
  slipMode.sendPacketToSerial(p);  // Use existing slipMode instance

  // Also send via original output (so WiFi still works normally)
  // Comment this line if you want SLIP-only mode
  if (slipMode.original_wifi_output != NULL) {
    return slipMode.original_wifi_output(netif, p, ipaddr);
  }

  return ERR_OK;
}

// Custom input hook for WiFi interface to capture incoming packets
static err_t slip_wifi_input_hook(struct pbuf *p, struct netif *inp)
{
  // Send a copy to serial via SLIP encoding
  slipMode.sendPacketToSerial(p);  // Use existing slipMode instance

  // Also process via original input (so WiFi still works normally)
  // Comment this line if you want SLIP-only mode
  if (slipMode.original_wifi_input != NULL) {
    return slipMode.original_wifi_input(p, inp);
  }

  return ERR_OK;
}

// Dummy netif init for SLIP interface
static err_t slip_netif_init(struct netif *netif)
{
  netif->name[0] = 's';
  netif->name[1] = 'l';
  netif->mtu = 1500;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_LINK_UP;
  // SLIP is point-to-point, no ARP needed, output handled separately
  return ERR_OK;
}

void ZSLIPMode::sendPacketToSerial(struct pbuf *p)
{
  if (p == NULL) return;

  // Send SLIP_END to mark start of packet
  sserial.printb(SLIP_END);
  if(sserial.isSerialOut())
    serialOutDeque();

  // Iterate through pbuf chain
  struct pbuf *q = p;
  while (q != NULL) {
    uint8_t *payload = (uint8_t *)q->payload;
    int len = q->len;

    // Encode each byte with SLIP escaping
    for (int i = 0; i < len; i++) {
      uint8_t c = payload[i];

      if (c == SLIP_END) {
        sserial.printb(SLIP_ESC);
        sserial.printb(SLIP_ESC_END);  // FIX: Was SLIP_END, should be SLIP_ESC_END
      } else if (c == SLIP_ESC) {
        sserial.printb(SLIP_ESC);
        sserial.printb(SLIP_ESC_ESC);
      } else {
        sserial.printb(c);
      }

      // Yield periodically to prevent watchdog timeout
      if ((i % 64) == 0) {
        yield();
        if(sserial.isSerialOut())
          serialOutDeque();
      }
    }

    q = q->next;
  }

  // Send SLIP_END to mark end of packet
  sserial.printb(SLIP_END);
  if(sserial.isSerialOut())
    serialOutDeque();
}

void ZSLIPMode::injectPacketToNetwork(uint8_t *data, int len)
{
  if (len < 20) {
    // Too small to be a valid IP packet
    debugPrintf("SLIP: packet too small (%d bytes)\n", len);
    return;
  }

  // Allocate pbuf for the packet
  struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
  if (p == NULL) {
    debugPrintf("SLIP: pbuf_alloc failed\n");
    return;
  }

  // Copy data into pbuf
  memcpy(p->payload, data, len);
  p->len = len;
  p->tot_len = len;

  // Determine IP version
  uint8_t ip_version = (data[0] >> 4) & 0x0F;

  if (ip_version == 4) {
    // IPv4 packet - inject into IP stack
    err_t err = ip4_input(p, &slip_netif);
    if (err != ERR_OK) {
      debugPrintf("SLIP: ip4_input failed: %d\n", err);
      pbuf_free(p);  // FIX: Free pbuf on error
    }
    // Note: ip4_input will free the pbuf on success
  }
#if LWIP_IPV6
  else if (ip_version == 6) {
    // IPv6 packet - inject into IP stack
    err_t err = ip6_input(p, &slip_netif);
    if (err != ERR_OK) {
      debugPrintf("SLIP: ip6_input failed: %d\n", err);
      pbuf_free(p);  // FIX: Free pbuf on error
    }
    // Note: ip6_input will free the pbuf on success
  }
#endif
  else {
    debugPrintf("SLIP: unknown IP version %d\n", ip_version);
    pbuf_free(p);
  }
}

void ZSLIPMode::switchBackToCommandMode()
{
  debugPrintf("\r\nMode:Command\r\n");

  // Restore original WiFi netif hooks
  if (wifi_netif != NULL && original_wifi_output != NULL) {
    wifi_netif->output = original_wifi_output;
  }
  if (wifi_netif != NULL && original_wifi_input != NULL) {
    wifi_netif->input = original_wifi_input;
  }

  // Remove SLIP netif
  netif_remove(&slip_netif);

  // Clean up
  currMode = &commandMode;

  if(this->buf != 0) {
    free(this->buf);
    this->buf = 0;
  }
}

esp_netif_t* get_esp_interface_netif(esp_interface_t interface);

void ZSLIPMode::switchTo()
{
  debugPrintf("\r\nMode:SLIP\r\n");

  // Set up serial parameters
  sserial.setFlowControlType(FCT_DISABLED);
  if(commandMode.getFlowControlType()==FCT_RTSCTS)
    sserial.setFlowControlType(FCT_RTSCTS);
  sserial.setPetsciiMode(false);
  sserial.setXON(true);

  this->curBufLen = 0;
  this->escaped = false;

  // Find the WiFi netif
  wifi_netif = netif_list;
  while (wifi_netif != NULL) {
    // Look for the WiFi station interface (usually "st" or "en")
    if ((wifi_netif->name[0] == 's' && wifi_netif->name[1] == 't') ||
        (wifi_netif->name[0] == 'e' && wifi_netif->name[1] == 'n')) {
      break;
    }
    wifi_netif = wifi_netif->next;
  }

  if (wifi_netif == NULL) {
    debugPrintf("SLIP: Could not find WiFi interface\n");
    wifi_netif = netif_default;  // Fall back to default interface
  }

  if (wifi_netif != NULL) {
    debugPrintf("SLIP: Using interface %c%c%d\n",
                wifi_netif->name[0], wifi_netif->name[1], wifi_netif->num);

    // Hook the output function to intercept outgoing packets
    original_wifi_output = wifi_netif->output;
    wifi_netif->output = slip_wifi_output_hook;

    // Hook the input function to intercept incoming packets
    original_wifi_input = wifi_netif->input;
    wifi_netif->input = slip_wifi_input_hook;
  }

  // Initialize SLIP netif
  ip4_addr_t ipaddr, netmask, gw;
  IP4_ADDR(&ipaddr, 192, 168, 255, 1);   // SLIP side IP
  IP4_ADDR(&netmask, 255, 255, 255, 0);
  IP4_ADDR(&gw, 192, 168, 255, 1);

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

    // Allocate buffer on first use
    if(this->buf == 0)
    {
      this->buf = (uint8_t *)malloc(4096);
      if (this->buf == 0) {
        debugPrintf("SLIP: malloc failed\n");
        return;
      }
      this->maxBufSize = 4096;
      this->curBufLen = 0;
    }

    // SLIP_END marks packet boundary
    if (c == SLIP_END)
    {
      if(this->curBufLen > 0)
      {
        if(logFileOpen)
          logPrintln("SLIP-RX packet.");

        debugPrintf("SLIP-RX: %d bytes\n", this->curBufLen);

        // Inject packet into network stack
        injectPacketToNetwork(this->buf, this->curBufLen);

        this->curBufLen = 0;
        this->escaped = false;
      }
      // else: empty packet or start marker, ignore
    }
    // SLIP_ESC starts an escape sequence
    else if(c == SLIP_ESC)
    {
      this->escaped = true;
    }
    // SLIP_ESC_END after ESC means literal END character
    else if((c == SLIP_ESC_END) && (this->escaped))
    {
      if (this->curBufLen < this->maxBufSize) {
        this->buf[this->curBufLen++] = SLIP_END;
      }
      this->escaped = false;
    }
    // SLIP_ESC_ESC after ESC means literal ESC character
    else if((c == SLIP_ESC_ESC) && (this->escaped))
    {
      if (this->curBufLen < this->maxBufSize) {
        this->buf[this->curBufLen++] = SLIP_ESC;
      }
      this->escaped = false;
    }
    // Invalid escape sequence
    else if(this->escaped)
    {
      debugPrintf("SLIP Protocol Error: unexpected 0x%02X after ESC\n", c);
      if(logFileOpen)
        logPrintln("SLIP error.");
      this->curBufLen = 0;
      this->escaped = false;
    }
    // Normal data byte
    else
    {
      if (this->curBufLen < this->maxBufSize) {
        this->buf[this->curBufLen++] = c;
      }
    }

    // Grow buffer if needed
    if(this->curBufLen >= this->maxBufSize)
    {
      int newSize = this->maxBufSize * 2;
      if (newSize > 65536) {
        // Packet too large, discard
        debugPrintf("SLIP: packet too large, discarding\n");
        this->curBufLen = 0;
        this->escaped = false;
      } else {
        uint8_t *newBuf = (uint8_t *)malloc(newSize);
        if (newBuf != NULL) {
          memcpy(newBuf, this->buf, this->curBufLen);
          maxBufSize = newSize;
          free(this->buf);
          this->buf = newBuf;
        } else {
          debugPrintf("SLIP: buffer realloc failed\n");
          this->curBufLen = 0;
        }
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
