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

#include <WiFiUdp.h>

static const char *TimeZones[243][2] PROGMEM = { {"UTC","0"},
    {"A","1"},{"CDT","10:30"},{"ACST","9:30"},{"ACT","-5"},
    {"ACT","9:30/10:30"},{"ACWST","8:45"},{"ADT","3"},{"ADT","-3"},
    {"AEDT","11"},{"AEST","10"},{"AET","11"},{"AFT","4:30"},
    {"AKDT","-8"},{"AKST","-9"},{"ALMT","6"},{"AMST","-3"},{"AMST","5"},
    {"AMT","-4"},{"AMT","4"},{"ANAST","12"},{"ANAT","12"},{"AQTT","5"},
    {"ART","-3"},{"AST","2"},{"AST","-4"},{"AT","-4/-3"},{"AWDT","9"},
    {"AWST","8"},{"AZOST","0"},{"AZOT","-1"},{"AZST","5"},{"AZT","4"},
    {"AOE","-12"},{"B","2"},{"BNT","8"},{"BOT","-4"},{"BRST","-2"},
    {"BRT","-3"},{"BST","6"},{"BST","11"},{"BST","1"},{"BTT","6"},
    {"C","3"},{"CAST","8"},{"CAT","2"},{"CCT","6:30"},{"CDT","-5"},
    {"CDT","-4"},{"CEST","2"},{"CET","1"},{"CHADT","13:45"},
    {"CHAST","12:45"},{"CHOST","9"},{"CHOT","8"},{"CHUT","10"},
    {"CIDST","-4"},{"CIST","-5"},{"CKT","-10"},{"CLST","-3"},
    {"CLT","-4"},{"COT","-5"},{"CST","-6"},{"CST","8"},{"CST","-5"},
    {"CT","-6/-5"},{"CVT","-1"},{"CXT","7"},{"ChST","10"},{"D","4"},
    {"DAVT","7"},{"DDUT","10"},{"E","5"},{"EASST","-5"},{"EAST","-6"},
    {"EAT","3"},{"ECT","-5"},{"EDT","-4"},{"EEST","3"},{"EET","2"},
    {"EGST","0"},{"EGT","-1"},{"EST","-5"},{"ET","-5/-4"},{"F","6"},
    {"FET","3"},{"FJST","13"},{"FJT","12"},{"FKST","-3"},{"FKT","-4"},
    {"FNT","-2"},{"G","7"},{"GALT","-6"},{"GAMT","-9"},{"GET","4"},
    {"GFT","-3"},{"GILT","12"},{"GMT","0"},{"GST","4"},{"GST","-2"},
    {"GYT","-4"},{"H","8"},{"HADT","-9"},{"HAST","-10"},{"HKT","8"},
    {"HOVST","8"},{"HOVT","7"},{"I","9"},{"ICT","7"},{"IDT","3"},
    {"IOT","6"},{"IRDT","4:30"},{"IRKST","9"},{"IRKT","8"},
    {"IRST","3:30"},{"IST","5:30"},{"IST","1"},{"IST","2"},{"JST","9"},
    {"K","10"},{"KGT","6"},{"KOST","11"},{"KRAST","8"},{"KRAT","7"},
    {"KST","9"},{"KUYT","4"},{"L","11"},{"LHDT","11"},{"LHST","10:30"},
    {"LINT","14"},{"M","12"},{"MAGST","12"},{"MAGT","11"},{"MART","-9:30"},
    {"MAWT","5"},{"MDT","-6"},{"MHT","12"},{"MMT","6:30"},{"MSD","4"},
    {"MSK","3"},{"MST","-7"},{"MT","-7/-6"},{"MUT","4"},{"MVT","5"},
    {"MYT","8"},{"N","-1"},{"NCT","11"},{"NDT","-2:30"},{"NFT","11"},
    {"NOVST","7"},{"NOVT","6"},{"NPT","5:45"},{"NRT","12"},{"NST","-3:30"},
    {"NUT","-11"},{"NZDT","13"},{"NZST","12"},{"O","-2"},{"OMSST","7"},
    {"OMST","6"},{"ORAT","5"},{"P","-3"},{"PDT","-7"},{"PET","-5"},
    {"PETST","12"},{"PETT","12"},{"PGT","10"},{"PHOT","13"},{"PHT","8"},
    {"PKT","5"},{"PMDT","-2"},{"PMST","-3"},{"PONT","11"},{"PST","-8"},
    {"PST","-8"},{"PT","-8/-7"},{"PWT","9"},{"PYST","-3"},{"PYT","-4"},
    {"PYT","8:30"},{"Q","-4"},{"QYZT","6"},{"R","-5"},{"RET","4"},
    {"ROTT","-3"},{"S","-6"},{"SAKT","11"},{"SAMT","4"},{"SAST","2"},
    {"SBT","11"},{"SCT","4"},{"SGT","8"},{"SRET","11"},{"SRT","-3"},
    {"SST","-11"},{"SYOT","3"},{"T","-7"},{"TAHT","-10"},{"TFT","5"},
    {"TJT","5"},{"TKT","13"},{"TLT","9"},{"TMT","5"},{"TOST","14"},
    {"TOT","13"},{"TRT","3"},{"TVT","12"},{"U","-8"},{"ULAST","9"},
    {"ULAT","8"},{"UYST","-2"},{"UYT","-3"},{"UZT","5"},
    {"V","-9"},{"VET","-4"},{"VLAST","11"},{"VLAT","10"},{"VOST","6"},
    {"VUT","11"},{"W","-10"},{"WAKT","12"},{"WARST","-3"},{"WAST","2"},
    {"WAT","1"},{"WEST","1"},{"WET","0"},{"WFT","12"},{"WGST","-2"},
    {"WGT","-3"},{"WIB","7"},{"WIT","9"},{"WITA","8"},{"WST","14"},
    {"WST","1"},{"WT","0"},{"X","-11"},{"Y","-12"},{"YAKST","10"},
    {"YAKT","9"},{"YAPT","10"},{"YEKST","6"},{"YEKT","5"},{"Z","0"}
};

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

  int getDoWNumber();
  const char *getDoW();

  bool isInStandardTime();
  bool isInDaylightSavingsTime();

  void setTime(DateTimeClock &clock);
};

class RealTimeClock : DateTimeClock
{
public:
  RealTimeClock(uint32_t epochSecs);
  RealTimeClock();
  RealTimeClock(int year, int month, int day, int hour, int min, int sec, int millis);

  void tick();

  bool isTimeSet();

  bool reset();

  int getTimeZoneCode();
  void setTimeZoneCode(int val);
  bool setTimeZone(String str);

  String getFormat();
  void setFormat(String fmt);

  String getNtpServerHost();
  void setNtpServerHost(String newHost);

  bool isDisabled();
  void setDisabled(bool tf);

  void forceUpdate();

  DateTimeClock &getCurrentTime();
  String getCurrentTimeFormatted();

  // should be private
private:
  bool disabled = false;
  DateTimeClock adjClock;
  WiFiUDP udp;
  bool udpStarted = false;
  uint32_t lastMillis = 0;
  uint32_t nextNTPMillis = 0;
  int32_t ntpPeriodMillis = 60 * 1000; // every minute
  int32_t ntpPeriodLongMillis = 5 * 60 * 60 * 1000; // every 5 hours
  uint8_t tzCode = 0;
  String format="%M/%d/%yyyy %h:%mm:%ss%aa %z";
  String ntpServerName = "time.nist.gov";

  void startUdp();
  bool sendTimeRequest();
};


