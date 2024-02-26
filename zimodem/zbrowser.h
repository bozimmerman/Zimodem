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

#ifdef INCLUDE_SD_SHELL
class ZBrowser : public ZMode
{
#  ifdef INCLUDE_COMET64
     friend class Comet64;
#  endif

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
    int argNum(String argLetters, char argLetter, int def);
    String cleanOneArg(String line);
    String cleanFirstArg(String line);
    String cleanRemainArg(String line);
    bool isMask(String mask);
    bool matches(String fname, String mask);
    void makeFileList(String ***l, int *n, String p, String mask, bool recurse);
    bool deleteFile(String fname, String mask, bool recurse);
    void showDirectory(String path, String mask, String prefix, bool recurse);
    bool copyFiles(String source, String mask, String target, bool recurse, bool overwrite);
    
    bool doHelpCommand(String &line, bool showShellOutput);
    bool doDirCommand(String &line, bool showShellOutput);
    bool doMkDirCommand(String &line, bool showShellOutput);
    bool doRmDirCommand(String &line, bool showShellOutput);
    bool doCdDirCommand(String &line, bool showShellOutput);
    bool doCatCommand(String &line, bool showShellOutput);
    bool doRmCommand(String &line, bool showShellOutput);
    bool doCpCommand(String &line, bool showShellOutput);
    bool doRenCommand(String &line, bool showShellOutput);
    bool doMoveCommand(String &line, bool showShellOutput);
    bool doXGetCommand(String &line, bool showShellOutput);
    bool doXPutCommand(String &line, bool showShellOutput);
    bool doYGetCommand(String &line, bool showShellOutput);
    bool doYPutCommand(String &line, bool showShellOutput);
    bool doKGetCommand(String &line, bool showShellOutput);
    bool doKPutCommand(String &line, bool showShellOutput);
    bool doZGetCommand(String &line, bool showShellOutput);
    bool doZPutCommand(String &line, bool showShellOutput);
    bool doPGetCommand(String &line, bool showShellOutput);
    bool doPPutCommand(String &line, bool showShellOutput);
    bool doWGetCommand(String &line, bool showShellOutput);

#ifdef INCLUDE_FTP
    bool doFGetCommand(String &line, bool showShellOutput);
    bool doFPutCommand(String &line, bool showShellOutput);
    bool doFDirCommand(String &line, bool showShellOutput);
    FTPHost *ftpHost = 0;
#endif
    bool showMenu;
    bool savedEcho;
    String path="/";
    String EOLN;
    char EOLNC[5];
    unsigned long lastNumber;
    String lastString;
    bool quiet = false;

  public:
    ~ZBrowser();
    void switchTo();
    void serialIncoming();
    void loop();
    void init();
    bool doModeCommand(String &line, bool showShellOutput);
};
#endif
