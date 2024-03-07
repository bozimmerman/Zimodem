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

static uint8_t slipmode_recv(const uint8_t *payload, const int plen)
{
  if((plen > 20)
  &&(payload[19]==255) // broadcast
  &&(payload[9]==17)) // udp
    return 0; // skip it

  debugPrintf("\r\nSLIP-in packet: %d bytes\r\n",plen);
  sserial.printb(ZSLIPMode::SLIP_END);
  if(sserial.isSerialOut())
    serialOutDeque();
  for(int p=0;p<plen;p++)
  {
    switch(payload[p])
    {
      case ZSLIPMode::SLIP_END:
      {
        sserial.printb(ZSLIPMode::SLIP_ESC);
        sserial.printb(ZSLIPMode::SLIP_END);
        break;
      }
      case ZSLIPMode::SLIP_ESC:
      {
        sserial.printb(ZSLIPMode::SLIP_ESC);
        sserial.printb(ZSLIPMode::SLIP_ESC_ESC);
        break;
      }
      default:
        sserial.printb(payload[p]);
        break;
    }
    yield();
    if(sserial.isSerialOut())
      serialOutDeque();
  }
  sserial.printb(ZSLIPMode::SLIP_END);
  if(sserial.isSerialOut())
    serialOutDeque();
  return 0;
}

err_t slip_input(struct pbuf *p, struct netif *inp)
{
  while(p != NULL)
  {
    int plen = p->len;
    if((plen > 0)&&(p->payload != 0))
      slipmode_recv((uint8_t *)p->payload, plen);
    p=p->next;
  }
  return false; // maybe this cancels?
}

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
  if(_pcb[0] == 0)
  {
    _pcb[0] = raw_new(IP_PROTO_TCP);
    _pcb[1] = raw_new(IP_PROTO_UDP);
    _pcb[2] = raw_new(IP_PROTO_ICMP);
    _pcb[3] = raw_new(IP_PROTO_IGMP);
    _pcb[4] = raw_new(IP_PROTO_UDPLITE);
  }
  for(int i=0;i<5;i++)
  {
    if(_pcb[i] != 0)
      raw_recv(_pcb[i], &_raw_recv, (void *)_pcb[i]);
  }
  esp_netif_t* esp_netif = get_esp_interface_netif(ESP_IF_WIFI_STA);
  struct netif *n = netif_list;
  while(n != 0)
  {
    //n->input = slip_input;
    n=n->next;
  }
  currMode=&slipMode;
  debugPrintf("Switched to SLIP mode\n\r");
}

static uint8_t _raw_recv(void *arg, raw_pcb *pcb, pbuf *pb, const ip_addr_t *addr)
{
  while(pb != NULL)
  {
    pbuf * this_pb = pb;
    int plen = this_pb->len;
    if(plen > 0)
    {
      uint8_t* payload = (uint8_t *)this_pb->payload;
      //pbuf_free(this_pb); // at this point, there are no refs, so errs out.
      slipmode_recv(payload, plen);
    }
    pb = pb->next;
    //this_pb->next = NULL; //TODO: i wonder if I need to free something here? check refs?
  }
  return false;// maybe this cancels?
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
        //TODO: this probably eats memory
        struct pbuf *p = pbuf_alloc(PBUF_RAW,this->curBufLen,PBUF_RAM);
        //struct pbuf *p = (struct pbuf *)malloc(sizeof(struct pbuf));
        p->next = NULL;
        memcpy(p->payload,this->buf,this->curBufLen);
        p->len = this->curBufLen;
        p->tot_len = this->curBufLen;
        //p->type_internal = PBUF_RAM;
        //p->ref = 1;
        //p->flags = 0;
        raw_send(_pcb[0], p); // not sure this is working
#ifdef ZIMODEM_ESP32
        debugPrintf("tot=%dk heap=%dk",(ESP.getFlashChipSize()/1024),(ESP.getFreeHeap()/1024));
        struct netif *n = ip_current_netif(); // keep this forever
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
