/*
   Copyright 2024-2026 Bo Zimmerman

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

#if INCLUDE_SD_SHELL
#if INCLUDE_FTP
#include "proto_comet64.h"

class ZComet64Mode : public ZMode
{
  private:
    void switchBackToCommandMode();
    Comet64 *proto = 0;

  public:
    void switchTo();
    void serialIncoming();
    void loop();
};

#endif
#endif
