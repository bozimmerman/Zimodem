/*
   Copyright 2016-2019 Bo Zimmerman

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

#ifdef INCLUDE_SD_SHELL
class ZBrowser : public ZMode
{
  private:
    enum ZBrowseState
    {
      ZBROW_MAIN=0,
    } currState;
    
    ZSerial serial;

    void switchBackToCommandMode();
    String makePath(String addendum);
    String fixPathNoSlash(String path);
    String stripDir(String path);
    String stripFilename(String path);
    String stripArgs(String line, String &argLetters);
    String cleanOneArg(String line);
    String cleanFirstArg(String line);
    String cleanRemainArg(String line);
    bool isMask(String mask);
    bool matches(String fname, String mask);
    void makeFileList(String ***l, int *n, String p, String mask, bool recurse);
    void deleteFile(String fname, String mask, bool recurse);
    void showDirectory(String path, String mask, String prefix, bool recurse);
    void copyFiles(String source, String mask, String target, bool recurse, bool overwrite);
    
    FTPHost *ftpHost = 0;
    bool showMenu;
    bool savedEcho;
    String path="/";
    String EOLN;
    char EOLNC[5];
    unsigned long lastNumber;
    String lastString;

  public:
    ~ZBrowser();
    void switchTo();
    void serialIncoming();
    void loop();
    void init();
    void doModeCommand(String &line);
};
#endif
