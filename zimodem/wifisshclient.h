#ifdef INCLUDE_SSH
/*
   Copyright 2023-2024 Bo Zimmerman

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
#include "Arduino.h"
#include "IPAddress.h"
#include <WiFi.h>
#include "src/libssh2/libssh2_config.h"
#include "src/libssh2/libssh2.h"

class WiFiSSHClient : public WiFiClient
{
protected:
  String _username = "";
  String _password = "";
  libssh2_socket_t sock = null;
  LIBSSH2_SESSION *session = null;
  LIBSSH2_CHANNEL *channel = null;
  static const size_t INTERNAL_BUF_SIZE = 1024;
  size_t ibufSz = 0;
  char ibuf[INTERNAL_BUF_SIZE];

  bool finishLogin();
  void closeSSH();
  void intern_buffer_fill();

public:
    WiFiSSHClient();
    ~WiFiSSHClient();
    int connect(IPAddress ip, uint16_t port) override;
    int connect(IPAddress ip, uint16_t port, int32_t timeout_ms) override;
    int connect(const char *host, uint16_t port) override;
    int connect(const char *host, uint16_t port, int32_t timeout_ms) override;
    void setLogin(String username, String password);
    int peek() override;
    size_t write(uint8_t data) override;
    size_t write(const uint8_t *buf, size_t size) override;
    int available() override;
    int read() override;
    int read(uint8_t *buf, size_t size) override;
    void flush() {}
    void stop() override;
    uint8_t connected() override;
    int fd() const override;

    operator bool()
    {
        return connected();
    }
    WiFiSSHClient &operator=(const WiFiSSHClient &other);
    bool operator==(const bool value)
    {
        return bool() == value;
    }
    bool operator!=(const bool value)
    {
        return bool() != value;
    }
    bool operator==(const WiFiSSHClient &);
    bool operator!=(const WiFiSSHClient &rhs)
    {
        return !this->operator==(rhs);
    };

private:
};
#endif
