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

static int WiFiNextClientId = 0;

class WiFiServerSpec
{
  public:
    int port;
    int id;
    int flagsBitmap = 0;
    char *delimiters = NULL;
    char *maskOuts = NULL;
    char *stateMachine = NULL;

    WiFiServerSpec();
    WiFiServerSpec(WiFiServerSpec &copy);
    ~WiFiServerSpec();

    WiFiServerSpec& operator=(const WiFiServerSpec&);
};

class WiFiServerNode : public WiFiServerSpec
{
  public:
    WiFiServer *server;
    WiFiServerNode *next = null;

    WiFiServerNode(int port, int flagsBitmap);
    bool hasClient();
    ~WiFiServerNode();

    static WiFiServerNode *FindServer(int port);
    static void DestroyAllServers();
    static bool ReadWiFiServer(File &f, WiFiServerSpec &node);
    static void SaveWiFiServers();
    static void RestoreWiFiServers();
};

