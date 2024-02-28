#ifndef ZIMODEM_PROTO_HTTP
#define ZIMODEM_PROTO_HTTP
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
bool parseWebUrl(uint8_t *vbuf, char **hostIp, char **req, int *port, bool *doSSL);
bool doWebGetBytes(const char *hostIp, int port, const char *req, const bool doSSL, uint8_t *buf, int *bufSize);
bool doGopherGetBytes(const char *hostIp, int port, const char *req, const bool doSSL, uint8_t *buf, int *bufSize);
WiFiClient *doWebGetStream(const char *hostIp, int port, const char *req, bool doSSL, uint32_t *responseSize);
WiFiClient *doGopherGetStream(const char *hostIp, int port, const char *req, bool doSSL, uint32_t *responseSize);
bool doWebGet(const char *hostIp, int port, FS *fs, const char *filename, const char *req, const bool doSSL);
bool doGopherGet(const char *hostIp, int port, FS *fs, const char *filename, const char *req, const bool doSSL);
#endif
