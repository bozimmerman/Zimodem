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

class ZConfig : public ZMode
{
  private:
    enum ZConfigMenu
    {
      ZCFGMENU_MAIN=0,
      ZCFGMENU_NUM=1,
      ZCFGMENU_ADDRESS=2,
      ZCFGMENU_OPTIONS=3,
      ZCFGMENU_WIMENU=4,
      ZCFGMENU_WIFIPW=5,
      ZCFGMENU_WICONFIRM=6,
      ZCFGMENU_FLOW=7,
      ZCFGMENU_BBSMENU=8,
      ZCFGMENU_NEWPORT=9,
      ZCFGMENU_NEWHOST=10,
      ZCFGMENU_NOTES=11,
      ZCFGMENU_NETMENU=12,
      ZCFGMENU_SUBNET=13,
      ZCFGMENU_NEWPRINT=14
    } currState;
    
    ZSerial serial; // storage for serial settings only

    void switchBackToCommandMode();
    void doModeCommand();
    bool showMenu;
    bool savedEcho;
    String EOLN;
    const char *EOLNC;
    unsigned long lastNumber;
    String lastAddress;
    String lastOptions;
    String lastNotes;
    WiFiServerSpec serverSpec;
    bool newListen;
    bool useDHCP;
    bool settingsChanged=false;
    char netOpt = ' ';
    int lastNumNetworks=0;
    IPAddress lastIP;
    IPAddress lastDNS;
    IPAddress lastGW;
    IPAddress lastSN;

  public:
    void switchTo();
    void serialIncoming();
    void loop();
};

