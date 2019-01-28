#ifdef INCLUDE_SD_SHELL
#ifndef ZIMODEM_PROTO_HTTP
#define ZIMODEM_PROTO_HTTP
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
bool parseFTPUrl(uint8_t *vbuf, char **hostIp, char **req, int *port, bool *doSSL, char **username, char **pw);
bool doFTPGet(FS *fs, const char *hostIp, int port, const char *filename, const char *req, const char *username, const char *pw, const bool doSSL);
bool doFTPPut(File &f, const char *hostIp, int port, const char *req, const char *username, const char *pw, const bool doSSL);
bool doFTPLS(ZSerial *serial, const char *hostIp, int port, const char *req, const char *username, const char *pw, const bool doSSL)
#endif
#endif
