#ifdef INCLUDE_SLIP
/*
   Copyright 2022-2024 Bo Zimmerman

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


void ZSLIPMode::switchBackToCommandMode()
{
  debugPrintf("\r\nMode:Command\r\n");
  currMode = &commandMode;
  if(this->buf != 0)
    free(this->buf);
  this->buf = 0;
  //TODO: UNDO THIS:   raw_recv(_pcb, &_raw_recv, (void *) _pcb);
}

esp_netif_t* get_esp_interface_netif(esp_interface_t interface);

esp_err_t esp_netif_transmit_new(void *esp_netif, void *data, size_t len)
{
  debugPrintf("\r\n**1:%d\r\n",len);
  return 0;
}
esp_err_t esp_netif_transmit_wrap_new(void *esp_netif, void *data, size_t len, void *netstack_buf)
{
  debugPrintf("\r\n**2:%d\r\n",len);
  return 0;
}

void esp_netif_free_rx_buffer_new(void *esp_netif, void *buffer)
{
  debugPrintf("\r\n**3:%d\r\n",buffer);
}

esp_netif_driver_ifconfig_t driver_ifconfig =
{
    .handle = &slipMode,
    .transmit = esp_netif_transmit_new,
    .transmit_wrap = esp_netif_transmit_wrap_new,
    .driver_free_rx_buffer = esp_netif_free_rx_buffer_new
};

void ZSLIPMode::switchTo()
{
  debugPrintf("\r\nMode:SLIP\r\n");

  sserial.setFlowControlType(FCT_DISABLED);
  if(commandMode.getFlowControlType()==FCT_RTSCTS)
    sserial.setFlowControlType(FCT_RTSCTS);
  sserial.setPetsciiMode(false);
  sserial.setXON(true);
  this->curBufLen = 0;
  this->escaped=false;

  //WiFi.disconnect(false,false);  disconnects too much. :(
  // this is the 'raw' way, that appears to rx all packets, but
  // so does the existing wifi, so Crap.
  if(_pcb == 0)
  {
    _pcb = raw_new(IP_PROTO_TCP);
    if(!_pcb){
        return;
    }
  }
  raw_recv(_pcb, &_raw_recv, (void *) _pcb);
  esp_netif_t* esp_netif = get_esp_interface_netif(ESP_IF_WIFI_STA);
  driver_ifconfig.handle = &driver_ifconfig;
  debugPrintf("\r\nHere we go: \r\n");
  esp_err_t err = esp_netif_attach(esp_netif, &driver_ifconfig);
  debugPrintf("\r\nHere we went: \r\n");
  /*
   *  Figure out a way to temporarily disable the driver handlers
   *  for the existing wifi connection, as 'raw' doesn't seem
   *  to give a shit.
      esp_err_t esp_netif_set_driver_config(esp_netif_t *esp_netif, const esp_netif_driver_ifconfig_t *driver_config)
      {
          if (esp_netif == NULL || driver_config == NULL) {
              return ESP_ERR_ESP_NETIF_INVALID_PARAMS;
          }
          esp_netif->driver_handle = driver_config->handle;
          esp_netif->driver_transmit = driver_config->transmit;
          esp_netif->driver_transmit_wrap = driver_config->transmit_wrap;
          esp_netif->driver_free_rx_buffer = driver_config->driver_free_rx_buffer;
          return ESP_OK;
      }
      Where the driver config is: CODE: SELECT ALL

      const esp_netif_driver_ifconfig_t driver_ifconfig =
      {
          .driver_free_rx_buffer = NULL,
          .transmit = esp_modem_dte_transmit,
          .handle = dte
      };
      esp_err_t err = esp_netif_attach(esp_netif, driver);
   */


  currMode=&slipMode;
  debugPrintf("Switched to SLIP mode\n\r");
}


static uint8_t _raw_recv(void *arg, raw_pcb *pcb, pbuf *pb, const ip_addr_t *addr)
{
  while(pb != NULL)
  {
    pbuf * this_pb = pb;
    pb = pb->next;
    this_pb->next = NULL; //TODO: i wonder if I need to free something here? check refs?
    int plen = this_pb->len;
    if(plen > 0)
    {
      uint8_t* payload = (uint8_t *)this_pb->payload;
      if(logFileOpen)
        logPrintln("SLIP-in packet:");
      sserial.printb(ZSLIPMode::SLIP_END);
      if(sserial.isSerialOut())
        serialOutDeque();
      for(int p=0;p<plen;p++)
      {
        //sserial.printb(buf[p]); //TODO:RESTORE ME
        if(payload[p] == ZSLIPMode::SLIP_END)
        {
          sserial.printb(ZSLIPMode::SLIP_ESC);
          sserial.printb(ZSLIPMode::SLIP_END);
        }
        else
        if(payload[p] == ZSLIPMode::SLIP_ESC)
        {
          sserial.printb(ZSLIPMode::SLIP_ESC);
          sserial.printb(ZSLIPMode::SLIP_ESC_ESC);
        }
        else
          sserial.printb(payload[p]);
        if(sserial.isSerialOut())
          serialOutDeque();
      }
      sserial.printb(ZSLIPMode::SLIP_END);
      if(sserial.isSerialOut())
        serialOutDeque();
    }
    //pbuf_free(this_pb); // at this point, there are no refs, so errs out.
  }
  return 0;
}

void ZSLIPMode::serialIncoming()
{
  while(HWSerial.available()>0)
  {
    uint8_t c = HWSerial.read();
    if(logFileOpen)
      logSerialIn(c);
    if(this->buf == 0)
    {
      this->buf = (uint8_t *)malloc(4096);
      this->maxBufSize = 4096;
      this->curBufLen = 0;
    }
    if (c == ZSLIPMode::SLIP_END)
    {
      if(this->curBufLen > 0)
      {
        if(logFileOpen)
          logPrintln("SLIP-out packet.");
        struct pbuf *p = (struct pbuf *)malloc(sizeof(struct pbuf));
        p->next = NULL;
        p->payload = (void *)this->buf;
        p->len = this->curBufLen;
        p->tot_len = this->curBufLen;
        p->type_internal = PBUF_RAM;
        p->ref = 1;
        p->flags = 0;
        raw_send(_pcb, p);
#ifdef ZIMODEM_ESP32
        debugPrintf("tot=%dk heap=%dk",(ESP.getFlashChipSize()/1024),(ESP.getFreeHeap()/1024));
#endif
        this->curBufLen = 0;
        //free(this->buf); // this might crash
        //this->buf = 0;
        this->escaped=false;
      }
    }
    else
    if(c == ZSLIPMode::SLIP_ESC)
      this->escaped=true;
    else
    if((c == ZSLIPMode::SLIP_ESC_END)
    &&(this->escaped))
    {
      this->buf[this->curBufLen++] = ZSLIPMode::SLIP_END;
      this->escaped = false;
    }
    else
    if((c == ZSLIPMode::SLIP_ESC_ESC)
    &&(this->escaped))
    {
      this->buf[this->curBufLen++] = ZSLIPMode::SLIP_ESC;
      this->escaped=false;
    }
    else
    if(this->escaped)
    {
      debugPrintf("SLIP Protocol Error\n");
      if(logFileOpen)
        logPrintln("SLIP error.");
      this->curBufLen = 0;
      this->escaped=false;
    }
    else
      this->buf[this->curBufLen++] = c;
    if(this->curBufLen >= this->maxBufSize)
    {
      uint8_t *newBuf = (uint8_t *)malloc(this->maxBufSize*2);
      memcpy(newBuf,this->buf,this->curBufLen);
      maxBufSize *= 2;
      free(this->buf);
      this->buf = newBuf;
    }
  }
}

void ZSLIPMode::loop()
{
  if(sserial.isSerialOut())
    serialOutDeque();
  logFileLoop();
}

#endif /* INCLUDE_SLIP_ */
