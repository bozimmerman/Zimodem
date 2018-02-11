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
#include <FS.h>

class ZModem
{
private:
  Stream *mdmIO = null;

  enum ZStatus
  {
    ZSTATUS_CONTINUE,
    ZSTATUS_TIMEOUT,
    ZSTATUS_FINISH,
    ZSTATUS_CANCEL
  } lastStatus = ZSTATUS_CONTINUE;
  enum ZExpect
  {
    ZEXPECT_FILENAME,
    ZEXPECT_DATA,
    ZEXPECT_NOTHING
  };
  enum ZAction {
    ZACTION_ESCAPE,
    ZACTION_DATA,
    ZACTION_HEADER,
    ZACTION_CANCEL,
    ZACTION_FINISH
  };
  uint8_t readZModemByte(long timeout);

public:
  ZModem(Stream &modemIO);

  bool receive(FS &fs, String dirPath);
  bool transmit(File &rfile);
};
