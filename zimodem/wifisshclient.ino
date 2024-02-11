#ifdef INCLUDE_SSH
/*
   Copyright 2024-2023 Bo Zimmerman

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

WiFiSSHClient::WiFiSSHClient()
{
  libssh2_init(0);
}

WiFiSSHClient::~WiFiSSHClient()
{
  closeSSH();
  libssh2_exit();
}

void WiFiSSHClient::closeSSH()
{
  if(channel) {
    libssh2_channel_send_eof(channel);
    libssh2_channel_close(channel);
    libssh2_channel_free(channel);
    channel = null;
  }
  if(session) {
      libssh2_session_disconnect(session, "exit");
      libssh2_session_free(session);
      session = null;
  }
  if(sock) {
    close(sock);
    sock = null;
  }
}

WiFiSSHClient &WiFiSSHClient::operator=(const WiFiSSHClient &other)
{
  _username = other._username;
  _password = other._password;
  sock = other.sock;
  session = other.session;
  channel = other.channel;
  ibufSz = other.ibufSz;
  memcpy(ibuf, other.ibuf, INTERNAL_BUF_SIZE);
  return *this;
}

void WiFiSSHClient::stop()
{
  closeSSH();
}


int WiFiSSHClient::connect(IPAddress ip, uint16_t port)
{
  sock = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = ip;
  if(::connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0)
    return false;
  session = libssh2_session_init();
  if(libssh2_session_handshake(session, sock))
  {
    debugPrintf("wifisshclient: failed handshake\n\r");
    closeSSH();
    return false;
  }
  if(!finishLogin())
  {
    debugPrintf("wifisshclient: failed login\n\r");
    closeSSH();
    return false;
  }
  channel = libssh2_channel_open_session(session);
  if(!channel)
  {
    debugPrintf("wifisshclient: failed session\n\r");
    closeSSH();
    return false;
  }
  if(libssh2_channel_request_pty(channel, "vanilla")) 
  {
    debugPrintf("wifisshclient: failed pty\n\r");
    closeSSH();
    return false;
  }
  if(libssh2_channel_shell(channel)) {
    debugPrintf("wifisshclient: failed shell\n\r");
    closeSSH();
    return false;
  }
  ssize_t err = libssh2_channel_read(channel, ibuf, INTERNAL_BUF_SIZE);
  if((err > 0) && (err != LIBSSH2_ERROR_EAGAIN)) {
    ibufSz += err;
  }
}
int WiFiSSHClient::connect(IPAddress ip, uint16_t port, int32_t timeout_ms)
{
  return connect(ip, port);
}

int WiFiSSHClient::connect(const char *host, uint16_t port)
{
  if(host == null)
    return false;
  IPAddress hostIp((uint32_t)0);
  if(!WiFiGenericClass::hostByName(host, hostIp))
    return false;
  return connect(hostIp, port);
}

int WiFiSSHClient::connect(const char *host, uint16_t port, int32_t timeout_ms)
{
  return connect(host, port);
}


static char _secret[256];
static    void      kbd_callback(const char *name, int name_len, const char *instruction, int instruction_len,
                                 int num_prompts, const struct _LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts,
                                 struct _LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses, void **abstract)
{
  (void)name;
  (void)name_len;
  (void)instruction;
  (void)instruction_len;
  if(num_prompts == 1) {
      responses[0].text = strdup(_secret);
      responses[0].length = strlen(_secret);
  }
  (void)prompts;
  (void)abstract;
}

bool WiFiSSHClient::finishLogin()
{
  if(_username.length()==0)
    return true;
  char *userauthlist;
  /* check what authentication methods are available */
  userauthlist = libssh2_userauth_list(session, _username.c_str(), strlen(_username.c_str()));
  if(strstr(userauthlist, "password") != NULL)
  {
    if(libssh2_userauth_password(session, _username.c_str(), _password.c_str()))
      return false;
    return true;
  }
  else
  if(strstr(userauthlist, "keyboard-interactive") != NULL)
  {
    strcpy(_secret,_password.c_str());
    if(libssh2_userauth_keyboard_interactive(session, _username.c_str(), &kbd_callback) )
      return false;
    else
      return true;
  }
  else
  if(strstr(userauthlist, "publickey") != NULL)
  {
    /*
      size_t fn1sz, fn2sz;
      char *fn1, *fn2;
      char const *h = getenv("HOME");
      if(!h || !*h)
          h = ".";
      fn1sz = strlen(h) + strlen(keyfile1) + 2;
      fn2sz = strlen(h) + strlen(keyfile2) + 2;
      fn1 = (char *)malloc(fn1sz);
      fn2 = (char *)malloc(fn2sz);
      // Using asprintf() here would be much cleaner, but less portable
      snprintf(fn1, fn1sz, "%s/%s", h, keyfile1);
      snprintf(fn2, fn2sz, "%s/%s", h, keyfile2);

      if(libssh2_userauth_publickey_fromfile(session, username, fn1,
                                             fn2, password)) {
          free(fn2);
          free(fn1);
          return false;
          goto shutdown;
      }
      free(fn2);
      free(fn1);
      return true;
    */
    return false;
  }
  else
    return false;
}

void WiFiSSHClient::setLogin(String username, String password)
{
  _username = username;
  _password = password;
}

void WiFiSSHClient::intern_buffer_fill()
{
  if((session != null) && (channel != null))
  {
    const size_t bufRemain = INTERNAL_BUF_SIZE - ibufSz;
    if(bufRemain > 0)
    {
      libssh2_session_set_blocking(session, 0);
      ssize_t err = libssh2_channel_read(channel, ibuf + ibufSz, bufRemain);
      libssh2_session_set_blocking(session, 1);
      if((err > 0) && (err != LIBSSH2_ERROR_EAGAIN)) {
        ibufSz += err;
      }
    }
  }
}

int WiFiSSHClient::peek()
{
  intern_buffer_fill();
  if(ibufSz > 0)
    return ibuf[0];
  return -1;
}

size_t WiFiSSHClient::write(uint8_t data)
{
  return write(&data, 1);
}

int WiFiSSHClient::read()
{
  uint8_t b[1];
  size_t num = read(b, 1);
  if(num > 0)
    return b[0];
  return -1;
}

size_t WiFiSSHClient::write(const uint8_t *buf, size_t size)
{
  if(channel != null)
    return libssh2_channel_write(channel, (const char *)buf, size);
  return 0;
}

int WiFiSSHClient::read(uint8_t *buf, size_t size)
{
  size_t bytesRead = 0;
  while(bytesRead < size)
  {
    intern_buffer_fill();
    if(ibufSz == 0)
      break;
    size_t bytesToTake = size - bytesRead;
    if(bytesToTake > ibufSz)
      bytesToTake = ibufSz;
    memcpy(buf, ibuf, bytesToTake);
    if(ibufSz > bytesToTake) // if there are still buffer bytes remaining
      memcpy(ibuf, ibuf + bytesToTake, ibufSz - bytesToTake);
    ibufSz -= bytesToTake;
    bytesRead += bytesToTake;
  }
  return (int)bytesRead;
}

int WiFiSSHClient::available()
{
  intern_buffer_fill();
  return ibufSz;
}

uint8_t WiFiSSHClient::connected()
{
  if(ibufSz > 0)
    return true;
  intern_buffer_fill();
  if(ibufSz > 0)
    return true;
  if(channel == null)
    return false;
  return !libssh2_channel_eof(channel);
}

int WiFiSSHClient::fd() const
{
  return sock;
}

#endif
