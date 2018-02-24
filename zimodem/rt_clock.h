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

#include <WiFiUdp.h>

class DateTimeClock
{
public:
  DateTimeClock(uint32_t epochSecs);
  DateTimeClock();
  DateTimeClock(int year, int month, int day, int hour, int min, int sec, int millis);
protected:
  uint16_t year=0;
  uint8_t month=0;
  uint8_t day=0;
  uint8_t hour=0;
  uint8_t min=0;
  uint8_t sec=0;
  uint16_t milsec=0;
  char str[55];

  bool isLeapYear();
  uint8_t getDaysInThisMonth();
public:
  int getYear();
  void setYear(int y);
  void addYear(uint32_t y);
  int getMonth();
  void setMonth(int m);
  void addMonth(uint32_t m);
  int getDay();
  void setDay(int d);
  void addDay(uint32_t d);
  int getHour();
  void setHour(int h);
  void addHour(uint32_t h);
  int getMinute();
  void setMinute(int mm);
  void addMinute(uint32_t mm);
  int getSecond();
  void setSecond(int s);
  void addSecond(uint32_t s);
  int getMillis();;
  void setMillis(int s);
  void addMillis(uint64_t s);

  void setByUnixEpoch(uint32_t unisex);
  uint32_t getUnixEpoch();

  void setTime(DateTimeClock &clock);
  //void startUdp();
  //bool sendTimeRequest();
};

class RealTimeClock : DateTimeClock
{
public:
  RealTimeClock(uint32_t epochSecs);
  RealTimeClock();
  RealTimeClock(int year, int month, int day, int hour, int min, int sec, int millis);

  void tick();

  bool isTimeSet();
  bool resetTime();

  void forceUpdate();

  DateTimeClock &getCurrentTime();

  // should be private
private:
  DateTimeClock adjClock;
  WiFiUDP udp;
  bool udpStarted = false;
  uint64_t lastMillis = 0;
  uint64_t nextNTPMillis = 0;
  int64_t ntpPeriodMillis = 60 * 1000; // every minute

  const int64_t flipChk = -(0x6fffffffffffffff);


  void startUdp();
  bool sendTimeRequest();
};
