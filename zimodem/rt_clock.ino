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
#include <WiFiUdp.h>

WiFiUDP udp;
const int NTP_PACKET_SIZE = 48;
static bool udpStarted = false;

uint8_t DAYS_IN_MONTH[13] PROGMEM = {
    31,28,31,30,31,30,31,31,30,31,30,31
};

RealTimeClock::RealTimeClock(uint64_t epochMillis)
{
  setYear(1970);
  setMonth(0);
  setDay(0);
  addMillis(epochMillis);
  lastMicros = micros();
}

RealTimeClock::RealTimeClock(int y, int m, int d, int h, int mn, int s, int mi)
{
  year=y;
  month=m;
  day=d;
  hour=h;
  min=mn;
  sec=s;
  milsec=mi;
  lastMicros = micros();
}

void RealTimeClock::startUdp()
{
  if(!udpStarted)
  {
    udpStarted=true;
    udp.begin(2390);
  }
}

void RealTimeClock::tick()
{
  int64_t diff = micros()-lastMicros;
  if(diff < 0)
    diff = -diff;
  addMillis(floor(diff/1000));
  lastMicros = micros()-(diff % 1000); 
  if(udpStarted)
  {
    int cb = udp.parsePacket();
    if (cb) 
    {
      // adapted from code by  by Michael Margolis, Tom Igoe, and Ivan Grokhotkov
      debugPrintf("Packet received, length=%d\n",cb);
      byte packetBuffer[ NTP_PACKET_SIZE];
      udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
      //the timestamp starts at byte 40 of the received packet and is four bytes,
      // or two words, long. First, esxtract the two words:
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const unsigned long seventyYears = 2208988800UL;
      // subtract seventy years:
      unsigned long epoch = secsSince1900 - seventyYears;
      setMillisSinceEpoch(epoch * 1000);
    }
    else
    {
      //TODO: check when the next time a request needs to be made... then DO
    }
  }
}

bool RealTimeClock::isTimeSet()
{
  return (year > 0);
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
  lastMicros = micros();
  //TODO: check time servers?
  return true;
}

int RealTimeClock::getYear()
{
  return year;
}

void RealTimeClock::setYear(int y)
{
  year=y;
}

void RealTimeClock::addYear(int y)
{
  year+=y;
}

int RealTimeClock::getMonth()
{
  return month + 1; // because 0 based
}

void RealTimeClock::setMonth(int m)
{
  month = m % 12;
}

void RealTimeClock::addMonth(int m)
{
  m = month + m;
  if(m > 11)
    addYear(floor(m / 12));
  setMonth(m);
}

int RealTimeClock::getDay()
{
  return day;
}

void RealTimeClock::setDay(int d)
{
  day = d % getDaysInThisMonth();
}

void RealTimeClock::addDay(int d)
{
  d = day + d;
  if(d >= getDaysInThisMonth())
  {
    while(d > isLeapYear()?366:365)
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

int RealTimeClock::getHour()
{
  return hour;
}

void RealTimeClock::setHour(int h)
{
  hour=h % 24;
}

void RealTimeClock::addHour(int h)
{
  h=hour + h;
  if(h > 23)
    addDay(floor(h / 24));
  setHour(h);
}

int RealTimeClock::getMinute()
{
  return min;
}

void RealTimeClock::setMinute(int mm)
{
  min=mm % 60;
}

void RealTimeClock::addMinute(int mm)
{
  mm = min+mm;
  if(mm > 59)
    addHour(floor(mm / 60));
  setMinute(mm);
}

int RealTimeClock::getSecond()
{
  return sec;
}

void RealTimeClock::setSecond(int s)
{
  sec=s % 60;
}

void RealTimeClock::addSecond(int s)
{
  s = sec + s;
  if(s > 59)
    addMinute(floor(s / 60));
  setSecond(s);
}

int RealTimeClock::getMillis()
{
  return milsec;
}

void RealTimeClock::setMillis(int s)
{
  milsec=s % 1000;
}

void RealTimeClock::addMillis(int s)
{
  s = milsec+s;
  if(s > 999)
    addSecond(floor(s / 1000));
  setMillis(s);
}

bool RealTimeClock::isLeapYear()
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

uint8_t RealTimeClock::getDaysInThisMonth()
{
  if(month != 1) // feb exception
    return pgm_read_byte_near(DAYS_IN_MONTH + month);
  return isLeapYear() ? 29 : 28;
}


void RealTimeClock::setMillisSinceEpoch(uint64_t milsec)
{
  setYear(1970);
  setMonth(0);
  setDay(0);
  setHour(0);
  setMinute(0);
  setSecond(0);
  setMillis(0);
  addMillis(milsec);
  lastMicros = micros();
}

uint64_t RealTimeClock::getMillisSinceEpoch()
{
  if(year < 1970)
    return 0;
  uint64_t val = milsec + (sec * 1000) + (min * 60000) + (hour * 60 * 60000);
  //TODO:
  return val;
}

bool RealTimeClock::sendTimeRequest()
{
  if(WiFi.status() == WL_CONNECTED)
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
    //TODO: finish this part
    //udp.beginPacket(address, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
  }
}
