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
  //TODO: UNDO THIS:   raw_recv(_pcb, &_raw_recv, (void *) _pcb);
}

void ZSLIPMode::switchTo()
{
  uint8_t num_slip1 = 0;
  struct netif slipif1;
  ip4_addr_t ipaddr_slip1, netmask_slip1, gw_slip1;
  //LWIP_PORT_INIT_SLIP1_IPADDR(&ipaddr_slip1);
  //LWIP_PORT_INIT_SLIP1_GW(&gw_slip1);
  //LWIP_PORT_INIT_SLIP1_NETMASK(&netmask_slip1);
  //printf("Starting lwIP slipif, local interface IP is %s\n", ip4addr_ntoa(&ipaddr_slip1));

  netif_add(&slipif1, &ipaddr_slip1, &netmask_slip1, &gw_slip1, &num_slip1, slipif_init, ip_input);

  sserial.setFlowControlType(FCT_DISABLED);
  if(commandMode.getFlowControlType()==FCT_RTSCTS)
    sserial.setFlowControlType(FCT_RTSCTS);
  sserial.setPetsciiMode(false);
  sserial.setXON(true);

  inPacket="";
  started=false;
  escaped=false;
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

static String encodeSLIP(uint8_t *ipPacket, int ipLen)
{
  String slip;
  slip += ZSLIPMode::SLIP_END;
  for(int i=0;i<ipLen;i++)
  {
    if(ipPacket[i] == ZSLIPMode::SLIP_END)
        slip += ZSLIPMode::SLIP_ESC + ZSLIPMode::SLIP_ESC_END;
    else
    if(ipPacket[i] == ZSLIPMode::SLIP_ESC)
        slip += ZSLIPMode::SLIP_ESC + ZSLIPMode::SLIP_ESC_ESC;
    else
      slip += ipPacket[i];
  }
  slip += ZSLIPMode::SLIP_END;
  return slip;
}

static uint8_t _raw_recv(void *arg, raw_pcb *pcb, pbuf *pb, const ip_addr_t *addr)
{
  while(pb != NULL)
  {
    pbuf * this_pb = pb;
    pb = pb->next;
    this_pb->next = NULL;
    for(int i=0;i<this_pb->len;i+=4)
    {
      String pkt = encodeSLIP((uint8_t *)this_pb->payload, this_pb->len);
      int plen = pkt.length();
      if(plen > 0)
      {
        if(logFileOpen)
          logPrintln("SLIP-in packet:");
        uint8_t *buf = (uint8_t *)pkt.c_str();
        for(int p=0;p<plen;p++)
        {
          if(logFileOpen)
            logSocketIn(buf[p]);
          sserial.printb(buf[p]);
          if(sserial.isSerialOut())
            serialOutDeque();
        }
        if(sserial.isSerialOut())
          serialOutDeque();
      }
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
    if (c == SLIP_END)
    {
      if(started)
      {
        if(inPacket.length()>0)
        {
          if(logFileOpen)
            logPrintln("SLIP-out packet.");
          struct pbuf p = { 0 };
          p.next = NULL;
          p.payload = (void *)inPacket.c_str();
          p.len = inPacket.length();
          p.tot_len = inPacket.length();
          raw_send(_pcb, &p);
        }
      }
      else
        started=true;
      inPacket="";
    }
    else
    if(c == SLIP_ESC)
      escaped=true;
    else
    if(c == SLIP_ESC_END)
    {
      if(escaped)
      {
        inPacket += SLIP_END;
        escaped = false;
      }
      else
        inPacket += c;
    }
    else
    if(c == SLIP_ESC_ESC)
    {
      if(escaped)
      {
        inPacket += SLIP_ESC;
        escaped=false;
      }
      else
        inPacket += c;
    }
    else
    if(escaped)
    {
      debugPrintf("SLIP Protocol Error\n");
      if(logFileOpen)
        logPrintln("SLIP error.");
      inPacket="";
      escaped=false;
    }
    else
    {
      inPacket += c;
      started=true;
    }
  }
}

void ZSLIPMode::loop()
{
  if(sserial.isSerialOut())
    serialOutDeque();
}

#endif /* INCLUDE_SLIP_ */
