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

static int ping(char *host)
{
  IPAddress hostIp((uint32_t)0);
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
}
#endif
