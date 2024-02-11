/*
   Copyright 2016-2024 Bo Zimmerman

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
  if(disabled)
    return;
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
      // now to apply the timezone.  Ugh;
      setByUnixEpoch(epoch);
      String tz="";
      {
        char s=0;
        char c=pgm_read_byte_near(&(TimeZones[tzCode][1][s]));
        while(c != 0)
        {
          tz += c;
          c=pgm_read_byte_near(&(TimeZones[tzCode][1][++s]));
        }
        String otz=tz;
        int x=tz.indexOf("/");
        if(x > 0)
        {
          if(isInStandardTime())
            tz = tz.substring(0,x);
          else
            tz = tz.substring(x+1);
        }
        x=tz.indexOf(":");
        int mm=0;
        if(x > 0)
        {
          mm = atoi(tz.substring(x+1).c_str());
          tz = tz.substring(0,x);
        }
        uint32_t echg = (atoi(tz.c_str()) * 3600);
        echg += ((echg < 0)?(-(mm * 60)):(mm * 60));
        setByUnixEpoch(epoch + echg);
        debugPrintf("Received NTP: %d/%d/%d %d:%d:%d\n\r",(int)getMonth(),(int)getDay(),(int)getYear(),(int)getHour(),(int)getMinute(),(int)getSecond());
      }
      nextNTPMillis = millis() + ntpPeriodLongMillis; // one hour
    }
    else
    {
      uint32_t now=millis();
      if(nextNTPMillis >= now)
      {
        if(((nextNTPMillis - now) > ntpPeriodLongMillis)
        ||(nextNTPMillis == now))
          forceUpdate();
      }
      else
      if((now - nextNTPMillis) > ntpPeriodLongMillis)
        forceUpdate();
    }
  }
}

void RealTimeClock::forceUpdate()
{
  if(!disabled)
  {
    nextNTPMillis = millis() + ntpPeriodMillis;
    startUdp();
    sendTimeRequest();
  }
}

bool RealTimeClock::isTimeSet()
{
  return (year > 1000);
}

bool RealTimeClock::reset()
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
  tzCode = 0;
  format="%M/%d/%yyyy %h:%mm:%ss%aa %z";
  ntpServerName = "time.nist.gov";
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
  uint32_t now=millis();
  if(lastMillis <= now)
    adjClock.addMillis(now - lastMillis);
  else
    adjClock.addMillis(now + (0xffffffff - lastMillis));
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
    String host = ntpServerName;
    int port=123;
    int pi=host.indexOf(':');
    if(pi>0)
    {
      port=atoi(host.substring(pi+1).c_str());
      host = host.substring(0,pi);
    }
    WiFi.hostByName(ntpServerName.c_str(), timeServerIP);
    udp.beginPacket(timeServerIP, port); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
    return true;
  }
  return false;
}

int DateTimeClock::getDoWNumber()
{
  uint16_t x= (getMonth() + 9) % 12;
  uint16_t y = getYear() - x/10;
  uint32_t z= 365*y + y/4 - y/100 + y/400 + (x*306 + 5)/10 + (getDay() - 1);
  z=z%7;
  if(z>3)
    return z-4;
  else
    return z+3;
}

const char *DateTimeClock::getDoW()
{
  int num=getDoWNumber();
  switch(num)
  {
  case 0:return "Sunday";
  case 1:return "Monday";
  case 2:return "Tuesday";
  case 3:return "Wednesday";
  case 4:return "Thursday";
  case 5:return "Friday";
  case 6:return "Saturday";
  default: return "Broken";
  }
}

bool DateTimeClock::isInStandardTime()
{
  uint8_t m=getMonth();
  if(m<3)
    return true;
  if(m==3)
  {
    uint8_t d=getDay();
    uint8_t dow=getDoWNumber();
    while(dow-- > 0)
      d--;
    if(d<14)
      return true;
    return false;
  }
  if((m>3)&&(m<11))
    return false;
  if(m==11)
  {
    uint8_t d=getDay();
    uint8_t dow=getDoWNumber();
    while(dow-- > 0)
      d--;
    if(d<7)
      return false;
    return true;
  }
  return true;
}

bool DateTimeClock::isInDaylightSavingsTime()
{
  return !isInStandardTime();
}

int RealTimeClock::getTimeZoneCode()
{
  return tzCode;
}

void RealTimeClock::setTimeZoneCode(int val)
{
  if((tzCode >= 0)&&(tzCode < 243))
  {
    tzCode = val;
    forceUpdate();
  }
}

bool RealTimeClock::setTimeZone(String str)
{
  str.toUpperCase();
  if(str.length()==0)
    return false;
  for(int i=0;i<243;i++)
  {
    for(int s=0;s<=str.length();s++)
    {
      char c=pgm_read_byte_near(&(TimeZones[i][0][s]));
      if(s==str.length())
      {
        if(c==0)
        {
          tzCode = i;
          forceUpdate();
          return true;
        }
      }
      else
      if((c==0)||(c != str[s]))
        break;
    }
  }
  return false;
}

String RealTimeClock::getFormat()
{
  return format;
}

void RealTimeClock::setFormat(String fmt)
{
  fmt.replace(',','.');
  format = fmt;
}

bool RealTimeClock::isDisabled()
{
  return disabled;
}

void RealTimeClock::setDisabled(bool tf)
{
  disabled=tf;
}

String RealTimeClock::getCurrentTimeFormatted()
{
  //String format="%M/%D/%YYYY %h:%mm:%ss%a"
  DateTimeClock c = getCurrentTime();
  String f=format;
  if(f.indexOf("%yyyy")>=0)
  {
    sprintf(str,"%04d",(int)c.getYear());
    f.replace("%yyyy",str);
  }
  if(f.indexOf("%yy")>=0)
  {
    int y=c.getYear();
    y -= (floor(y/100)*100);
    sprintf(str,"%02d",y);
    f.replace("%yy",str);
  }
  if(f.indexOf("%y")>=0)
  {
    sprintf(str,"%d",(int)c.getYear());
    f.replace("%y",str);
  }
  if(f.indexOf("%MM")>=0)
  {
    sprintf(str,"%02d",(int)c.getMonth());
    f.replace("%MM",str);
  }
  if(f.indexOf("%M")>=0)
  {
    sprintf(str,"%d",(int)c.getMonth());
    f.replace("%M",str);
  }
  if(f.indexOf("%dd")>=0)
  {
    sprintf(str,"%02d",(int)c.getDay());
    f.replace("%dd",str);
  }
  if(f.indexOf("%d")>=0)
  {
    sprintf(str,"%d",(int)c.getDay());
    f.replace("%d",str);
  }
  if(f.indexOf("%ee")>=0)
  {
    f.replace("%ee",c.getDoW());
  }
  if(f.indexOf("%e")>=0)
  {
    String dow=c.getDoW();
    dow = dow.substring(0,3);
    sprintf(str,"%d",dow.c_str());
    f.replace("%e",str);
  }
  if(f.indexOf("%HH")>=0)
  {
    sprintf(str,"%02d",(int)c.getHour());
    f.replace("%HH",str);
  }
  if(f.indexOf("%H")>=0)
  {
    sprintf(str,"%d",(int)c.getHour());
    f.replace("%H",str);
  }
  if(f.indexOf("%hh")>=0)
  {
    if((c.getHour()%12)==0)
      strcpy(str,"12");
    else
      sprintf(str,"%02d",c.getHour()%12);
    f.replace("%hh",str);
  }
  if(f.indexOf("%h")>=0)
  {
    if((c.getHour()%12)==0)
      strcpy(str,"12");
    else
      sprintf(str,"%d",(int)(c.getHour() % 12));
    f.replace("%h",str);
  }
  if(f.indexOf("%mm")>=0)
  {
    sprintf(str,"%02d",(int)c.getMinute());
    f.replace("%mm",str);
  }
  if(f.indexOf("%m")>=0)
  {
    sprintf(str,"%d",(int)c.getMinute());
    f.replace("%m",str);
  }
  if(f.indexOf("%ss")>=0)
  {
    sprintf(str,"%02d",(int)c.getSecond());
    f.replace("%ss",str);
  }
  if(f.indexOf("%s")>=0)
  {
    sprintf(str,"%d",(int)c.getSecond());
    f.replace("%s",str);
  }
  if(f.indexOf("%AA")>=0)
    f.replace("%AA",(c.getHour()<12)?"AM":"PM");
  if(f.indexOf("%aa")>=0)
    f.replace("%aa",(c.getHour()<12)?"am":"pm");
  if(f.indexOf("%A")>=0)
    f.replace("%A",(c.getHour()<12)?"A":"P");
  if(f.indexOf("%a")>=0)
    f.replace("%a",(c.getHour()<12)?"a":"p");
  if(f.indexOf("%z")>=0)
  {
    String z="";
    char s=0;
    char c=pgm_read_byte_near(&(TimeZones[tzCode][0][s]));
    while(c != 0)
    {
      z += c;
      c=pgm_read_byte_near(&(TimeZones[tzCode][0][++s]));
    }
    z.toLowerCase();
    f.replace("%z",z.c_str());
  }
  if(f.indexOf("%Z")>=0)
  {
    String z="";
    char s=0;
    char c=pgm_read_byte_near(&(TimeZones[tzCode][0][s]));
    while(c != 0)
    {
      z += c;
      c=pgm_read_byte_near(&(TimeZones[tzCode][0][++s]));
    }
    f.replace("%Z",z.c_str());
  }
  return f;
}

String RealTimeClock::getNtpServerHost()
{
  return ntpServerName;
}

void RealTimeClock::setNtpServerHost(String newHost)
{
  newHost.replace(',','.');
  ntpServerName = newHost;
}

