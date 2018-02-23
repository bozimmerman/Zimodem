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

class RealTimeClock
{
public:
  RealTimeClock(uint64_t epochMillis);
  RealTimeClock(int year, int month, int day, int hour, int min, int sec, int millis);

  void tick();

  bool isTimeSet();
  bool resetTime();

  int getYear();
  void setYear(int y);
  void addYear(int y);
  int getMonth();
  void setMonth(int m);
  void addMonth(int m);
  int getDay();
  void setDay(int d);
  void addDay(int d);
  int getHour();
  void setHour(int h);
  void addHour(int h);
  int getMinute();
  void setMinute(int mm);
  void addMinute(int mm);
  int getSecond();
  void setSecond(int s);
  void addSecond(int s);
  int getMillis();;
  void setMillis(int s);
  void addMillis(int s);

  void setByUnixEpoch(uint32_t unisex);
  uint32_t getUnixEpoch();

  // should be private
  void startUdp();
  bool sendTimeRequest();
private:
  bool isLeapYear();
  uint8_t getDaysInThisMonth();

  uint64_t lastMicros = 0;
  uint16_t year=0;
  uint8_t month=0;
  uint8_t day=0;
  uint8_t hour=0;
  uint8_t min=0;
  uint8_t sec=0;
  uint16_t milsec=0;

  //void startUdp();
  //bool sendTimeRequest();
};
