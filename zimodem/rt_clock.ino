/*
   Copyright 2016-2017 Bo Zimmerman

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
#include <math.h>
#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )

const int NTP_PACKET_SIZE = 48;

uint8_t DAYS_IN_MONTH[13] PROGMEM = {
    31,28,31,30,31,30,31,31,30,31,30,31
};

char *uintToStr( const uint64_t num, char *str )
{
  uint8_t i = 0;
  uint64_t n = num;
  do
    i++;
  while ( n /= 10 );
  str[i] = '\0';
  n = num;
  do
    str[--i] = ( n % 10 ) + '0';
  while ( n /= 10 );
  return str;
}

DateTimeClock::DateTimeClock() : DateTimeClock(0)
{
}


DateTimeClock::DateTimeClock(uint32_t epochSecs)
{
  setByUnixEpoch(epochSecs);
}

DateTimeClock::DateTimeClock(int y, int m, int d, int h, int mn, int s, int mi)
{
  year=y;
  month=m;
  day=d;
  hour=h;
  min=mn;
  sec=s;
  milsec=mi;
}

RealTimeClock::RealTimeClock(uint32_t epochSecs) : DateTimeClock(epochSecs)
{
  lastMillis = millis();
  nextNTPMillis = millis();
}

RealTimeClock::RealTimeClock() : DateTimeClock()
{
  lastMillis = millis();
  nextNTPMillis = millis();
}

RealTimeClock::RealTimeClock(int y, int m, int d, int h, int mn, int s, int mi) :
    DateTimeClock(y,m,d,h,mn,s,mi)
{
  lastMillis = millis();
  nextNTPMillis = millis();
}

void RealTimeClock::startUdp()
{
  if(!udpStarted)
  {
    udpStarted=udp.begin(2390);
  }
}

void RealTimeClock::tick()
{
  if(udpStarted)
  {
    int cb = udp.parsePacket();
    if (cb) 
    {
      // adapted from code by  by Michael Margolis, Tom Igoe, and Ivan Grokhotkov
      //debugPrint("Packet received, length=%d\n\r",cb);
      byte packetBuffer[ NTP_PACKET_SIZE];
      udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      uint32_t secsSince1900 = htonl(*((uint32_t *)(packetBuffer + 40))); 
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const uint32_t seventyYears = 2208988800UL;
      // subtract seventy years:
      uint32_t epoch = secsSince1900 - seventyYears;
      lastMillis = millis();
      setByUnixEpoch(epoch);
      debugPrintf("Received NTP: %d/%d/%d %d:%d:%d\n\r",(int)getMonth(),(int)getDay(),(int)getYear(),(int)getHour(),(int)getMinute(),(int)getSecond());
      nextNTPMillis = millis() + (ntpPeriodMillis * 60); // one hour
    }
    else
    {
      int64_t diff = nextNTPMillis - millis();
      if((diff <= 0) && (diff > flipChk))
      {
        HWSerial.println("Forcing update. DELME!");
        forceUpdate();
      }
    }
  }
}

void RealTimeClock::forceUpdate()
{
  nextNTPMillis = millis() + ntpPeriodMillis;
  startUdp();
  sendTimeRequest();
}

bool RealTimeClock::isTimeSet()
{
  return (year > 1000);
}

bool RealTimeClock::resetTime()
{
  year=0;
  month=0;
  day=0;
  hour=0;
  min=0;
  sec=0;
  milsec=0;
  lastMillis = millis();
  nextNTPMillis = millis();
  return true;
}

int DateTimeClock::getYear()
{
  return year;
}

void DateTimeClock::setYear(int y)
{
  year=y;
}

void DateTimeClock::addYear(uint32_t y)
{
  year+=y;
}

int DateTimeClock::getMonth()
{
  return month + 1; // because 0 based
}

void DateTimeClock::setMonth(int m)
{
  month = m % 12;
}

void DateTimeClock::addMonth(uint32_t m)
{
  m = month + m;
  if(m > 11)
    addYear(floor(m / 12));
  setMonth(m);
}

int DateTimeClock::getDay()
{
  return day + 1;
}

void DateTimeClock::setDay(int d)
{
  day = d % getDaysInThisMonth();
}

void DateTimeClock::addDay(uint32_t d)
{
  d = day + d;
  if(d >= getDaysInThisMonth())
  {
    while(d > (isLeapYear()?366:365))
    {
      d=d-(isLeapYear()?366:365);
      addYear(1);
    }
    while(d >= getDaysInThisMonth())
    {
      d=d-getDaysInThisMonth();
      addMonth(1);
    }
  }
  setDay(d);
}

int DateTimeClock::getHour()
{
  return hour;
}

void DateTimeClock::setHour(int h)
{
  hour=h % 24;
}

void DateTimeClock::addHour(uint32_t h)
{
  h=hour + h;
  if(h > 23)
    addDay(floor(h / 24));
  setHour(h);
}

int DateTimeClock::getMinute()
{
  return min;
}

void DateTimeClock::setMinute(int mm)
{
  min=mm % 60;
}

void DateTimeClock::addMinute(uint32_t mm)
{
  mm = min+mm;
  if(mm > 59)
    addHour(floor(mm / 60));
  setMinute(mm);
}

int DateTimeClock::getSecond()
{
  return sec;
}

void DateTimeClock::setSecond(int s)
{
  sec=s % 60;
}

void DateTimeClock::addSecond(uint32_t s)
{
  s = sec + s;
  if(s > 59)
    addMinute(floor(s / 60));
  setSecond(s);
}

int DateTimeClock::getMillis()
{
  return milsec;
}

void DateTimeClock::setMillis(int s)
{
  milsec=s % 1000;
}

void DateTimeClock::addMillis(uint64_t s)
{
  s = milsec+s;
  if(s > 999)
    addSecond(floor(s / 1000));
  setMillis(s);
}

bool DateTimeClock::isLeapYear()
{
  if(year % 4 == 0)
  {
    if(year % 100 == 0)
    {
      if(year % 400 == 0)
        return true;
      return false;
    }
    return true;
  }
  return false;
}

uint8_t DateTimeClock::getDaysInThisMonth()
{
  if(month != 1) // feb exception
    return pgm_read_byte_near(DAYS_IN_MONTH + month);
  return (isLeapYear() ? 29 : 28);
}

void DateTimeClock::setTime(DateTimeClock &clock)
{
  year=clock.year;
  month=clock.month;
  day=clock.day;
  hour=clock.hour;
  min=clock.min;
  sec=clock.sec;
  milsec=clock.milsec;
}


DateTimeClock &RealTimeClock::getCurrentTime()
{
  adjClock.setTime(*this);
  uint64_t now=millis();
  if(lastMillis <= now)
    adjClock.addMillis(now - lastMillis);
  else
    adjClock.addMillis(now + (0xffffffffffffffff - lastMillis));
  return adjClock;
}


void DateTimeClock::setByUnixEpoch(uint32_t unisex)
{
  setYear(1970);
  setMonth(0);
  setDay(0);
  setHour(0);
  setMinute(0);
  setSecond(0);
  setMillis(0);
  uint64_t ms = unisex;
  ms *= 1000L;
  addMillis(ms);
}

uint32_t DateTimeClock::getUnixEpoch()
{
  if(year < 1970)
    return 0;
  uint32_t val = sec + (min * 60) + (hour * 60 * 60);
  //TODO:
  return val;
}

bool RealTimeClock::sendTimeRequest()
{
  if((WiFi.status() == WL_CONNECTED)&&(udpStarted))
  {
    // adapted from code by  by Michael Margolis, Tom Igoe, and Ivan Grokhotkov
    const char* ntpServerName = "time.nist.gov";
    debugPrintf("Sending NTP Packet...");
    byte packetBuffer[ NTP_PACKET_SIZE];
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;
    IPAddress timeServerIP;
    WiFi.hostByName(ntpServerName, timeServerIP);
    udp.beginPacket(timeServerIP, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
    return true;
  }
  return false;
}
