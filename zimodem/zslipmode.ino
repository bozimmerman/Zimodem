/*
 * zslipmode.ino
 *
 *  Created on: May 17, 2022
 *      Author: Bo Zimmerman
 */
#ifdef INCLUDE_SLIP

static uint8_t _raw_recv(void *arg, raw_pcb *pcb, pbuf *pb, const ip_addr_t *addr)
{
  debugPrintf("->\n\r");
  while(pb != NULL) 
  {
    pbuf * this_pb = pb;
    pb = pb->next;
    this_pb->next = NULL;
    for(int i=0;i<this_pb->len;i+=4)
    {
      for(int l=i;l<i+4;l++)
      {
        if(l<this_pb->len)
        {
          debugPrintf(tohex(((uint8_t *)this_pb->payload)[l]));
          debugPrintf(" ");
        }
      }
      debugPrintf("\n\r");
    }
    //pbuf_free(this_pb); // at this point, there are no refs, so errs out.
  }
  debugPrintf("<\n\r");
  return 0;
}

void ZSLIPMode::switchBackToCommandMode()
{
  currMode = &commandMode;
}

void ZSLIPMode::switchTo()
{
  inPacket="";
  started=false;
  escaped=false;
  raw_pcb *_pcb = raw_new(IP_PROTO_TCP);
  if(!_pcb){
      return;
  }
  //_lock = xSemaphoreCreateMutex();
  raw_recv(_pcb, &_raw_recv, (void *) _pcb);
  currMode=&slipMode;
  debugPrintf("Switched to SLIP mode\n\r");
}

String ZSLIPMode::encodeSLIP(uint8_t *ipPacket, int ipLen)
{
  String slip;
  slip += SLIP_END;
  for(int i=0;i<ipLen;i++)
  {
    if(ipPacket[i] == SLIP_END)
        slip += SLIP_ESC + SLIP_ESC_END;
    else
    if(ipPacket[i] == SLIP_ESC)
        slip += SLIP_ESC + SLIP_ESC_ESC;
    else
      slip += ipPacket[i];
  }
  slip += SLIP_END;
  return slip;
}

void ZSLIPMode::serialIncoming()
{
  while(HWSerial.available()>0)
  {
    uint8_t c = HWSerial.read();
    if (c == SLIP_END)
    {
      if(started)
      {
        if(inPacket.length()>0)
        {
          //TODO: send the packet!
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
}

#endif /* INCLUDE_SLIP_ */
