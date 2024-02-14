#ifdef INCLUDE_SD_SHELL
#ifdef INCLUDE_COMET64
/*
   Copyright 2024-2024 Bo Zimmerman

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
/* Reverse engineered by Bo Zimmerman */
#include <FS.h>

# define COM64_BUFSIZ  204

class Comet64
{
private:
  ZSerial cserial;
  uint8_t inbuf[COM64_BUFSIZ+1];
  int idex = 0;

  bool aborted = false;
  unsigned long lastNonPlusTm = 0;
  unsigned int plussesInARow = 0;
  unsigned long plusTimeExpire = 0;
  SDFS *cFS = &SD;
  bool browsePetscii = false;

  void checkDoPlusPlusPlus(const int c, const unsigned long tm);
  bool checkPlusPlusPlusExpire(const unsigned long tm);
  void printFilename(const char* name);
  void printDiskHeader(const char* name);
  void printPetscii(const char* name);
  String getNormalExistingFilename(String name);

public:
  void receiveLoop();
  bool isAborted();
  Comet64(SDFS *fs, FlowControlType fcType);
  ~Comet64();
};
#endif
#endif
