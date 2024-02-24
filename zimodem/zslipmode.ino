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

void ZSLIPMode::switchTo()
{
  debugPrintf("\r\nMode:SLIP\r\n");
  uint8_t num_slip1 = 0;
  struct netif slipif1;
  ip4_addr_t ipaddr_slip1, netmask_slip1, gw_slip1;
  //LWIP_PORT_INIT_SLIP1_IPADDR(&ipaddr_slip1);
  //LWIP_PORT_INIT_SLIP1_GW(&gw_slip1);
  //LWIP_PORT_INIT_SLIP1_NETMASK(&netmask_slip1);
  //printf("Starting lwIP slipif, local interface IP is %s\r\n", ip4addr_ntoa(&ipaddr_slip1));

  //netif_add(&slipif1, &ipaddr_slip1, &netmask_slip1, &gw_slip1, &num_slip1, slipif_init, ip_input);

  sserial.setFlowControlType(FCT_DISABLED);
  if(commandMode.getFlowControlType()==FCT_RTSCTS)
    sserial.setFlowControlType(FCT_RTSCTS);
  sserial.setPetsciiMode(false);
  sserial.setXON(true);

  this->curBufLen = 0;
  this->escaped=false;
  if(_pcb == 0)
  {
    _pcb = raw_new(IP_PROTO_TCP);
    if(!_pcb){
        return;
    }
  }
  //_lock = xSemaphoreCreateMutex();
  raw_recv(_pcb, &_raw_recv, (void *) _pcb);
  currMode=&slipMode;
  debugPrintf("Switched to SLIP mode\n\r");
}


static uint8_t _raw_recv(void *arg, raw_pcb *pcb, pbuf *pb, const ip_addr_t *addr)
{
  while(pb != NULL)
  {
    pbuf * this_pb = pb;
    pb = pb->next;
    this_pb->next = NULL;
    int plen = this_pb->len;
    if(plen > 0)
    {
      uint8_t* payload = (uint8_t *)this_pb->payload;
      //if(logFileOpen)
      //  logPrintln("SLIP-in packet:");
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
    if(c == ZSLIPMode::SLIP_ESC_END)
    {
      if(this->escaped)
      {
        this->buf[this->curBufLen++] = ZSLIPMode::SLIP_END;
        this->escaped = false;
      }
      else
        this->buf[this->curBufLen++] = c;
    }
    else
    if(c == ZSLIPMode::SLIP_ESC_ESC)
    {
      if(this->escaped)
      {
        this->buf[this->curBufLen++] = ZSLIPMode::SLIP_ESC;
        this->escaped=false;
      }
      else
        this->buf[this->curBufLen++] = c;
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
