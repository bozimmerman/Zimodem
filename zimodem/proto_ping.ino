#ifdef INCLUDE_PING
/*
   Copyright 2023-2023 Bo Zimmerman

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
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/ip.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/inet.h"


#ifndef ZIMODEM_ESP32
# include "lwip/raw.h"
  static uint8_t ping_recv(void *pingtm, struct raw_pcb *pcb, pbuf *packet, const ip_addr_t *addr)
  {
    unsigned long *tm = (unsigned long *)pingtm;
    *tm = millis();
    return false;
  }
#endif

static int ping(char *host)
{
  IPAddress hostIp((uint32_t)0);
#ifdef ZIMODEM_ESP32
  if(!WiFiGenericClass::hostByName(host, hostIp)){
    return 1;
  }
  const int socketfd = socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP);
  if(socketfd < 0)
    return socketfd;

  const size_t pingpktLen = 10 + sizeof(struct icmp_echo_hdr);
  struct icmp_echo_hdr *pingpkt = (struct icmp_echo_hdr *)malloc(pingpktLen);
  ICMPH_TYPE_SET(pingpkt, ICMP_ECHO);
  ICMPH_CODE_SET(pingpkt, 0);
  pingpkt->id = 65535;
  pingpkt->seqno = htons(1);
  pingpkt->chksum = 0;
  pingpkt->chksum = inet_chksum(pingpkt, pingpktLen);

  ip4_addr_t outaddr;
  outaddr.addr = hostIp;

  struct sockaddr_in sockout;
  sockout.sin_len = sizeof(sockout);
  sockout.sin_family = AF_INET;
  inet_addr_from_ip4addr(&sockout.sin_addr, &outaddr);
  int ok = sendto(socketfd, pingpkt, pingpktLen, 0, (struct sockaddr*)&sockout, sizeof(sockout));
  free(pingpkt);
  if (ok == 0)
  {
    closesocket(socketfd);
    return -1;
  }

  struct timeval timev;
  timev.tv_sec = 5;
  timev.tv_usec = 0;
  if(setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, &timev, sizeof(timev)) < 0)
  {
    closesocket(socketfd);
    return -1;
  }

  uint8_t recvBuf[256];
  struct sockaddr_in inaddr;
  socklen_t inlen = 0;
  unsigned long time = millis();

  if(recvfrom(socketfd, recvBuf, 256, 0, (struct sockaddr*)&inaddr, &inlen) > 0)
  {
    unsigned long now = millis();
    if(now > time)
      time = now - time;
    else
      time = now;
    if(time > 65536)
      time = 65536;
  } else {
    closesocket(socketfd);
    return -1;
  }
  closesocket(socketfd);
  return (int)time;
#else
  if(!WiFi.hostByName(host, hostIp))
    return -1;
  int icmp_len = sizeof(struct icmp_echo_hdr);
  struct pbuf * packet = pbuf_alloc(PBUF_IP, 32 + icmp_len, PBUF_RAM);
  if(packet == nullptr)
    return 1;
  struct icmp_echo_hdr * pingRequest = (struct icmp_echo_hdr *)packet->payload;
  ICMPH_TYPE_SET(pingRequest, ICMP_ECHO);
  ICMPH_CODE_SET(pingRequest, 0);
  pingRequest->chksum = 0;
  pingRequest->id = 0x0100;
  pingRequest->seqno = htons(0);
  char dataByte = 'a';
  for(size_t i=0; i<32; i++)
  {
    ((char*)pingRequest)[icmp_len + i] = dataByte;
    if(++dataByte > 'w')
      dataByte = 'a';
  }
  pingRequest->chksum = inet_chksum(pingRequest,32+icmp_len);
  ip_addr_t dest_addr;
  dest_addr.addr = hostIp;
  struct raw_pcb *ping_pcb = raw_new(IP_PROTO_ICMP);

  unsigned long startTm = millis();
  unsigned long tm = 0;
  raw_recv(ping_pcb, ping_recv, (void *)&tm);
  raw_bind(ping_pcb, IP_ADDR_ANY);

  raw_sendto(ping_pcb, packet, &dest_addr);
  while((millis()-startTm < 1500) && (tm == 0))
    delay(1);
  pbuf_free(packet);
  raw_remove(ping_pcb);
  return (tm > 0)? (int)(millis()-tm) : -1;
#endif
}
#endif
