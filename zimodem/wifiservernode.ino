/*
   Copyright 2016-2016 Bo Zimmerman

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

WiFiServerNode::WiFiServerNode(int newport)
{
  id=++WiFiNextClientId;
  port=newport;
  server = new WiFiServer(newport);
  server->setNoDelay(true);
  server->begin();
  if(servs==null)
    servs=this;
  else
  {
    WiFiServerNode *s=servs;
    while(s->next != null)
      s=s->next;
    s->next=this;
  }
}

WiFiServerNode::~WiFiServerNode()
{
    if(server != null)
    {
      server->stop();
      server->close();
      delete server;
    }
    if(servs == null)
    {
    }
    else
    if(servs == this)
      servs = next;
    else
    {
      WiFiServerNode *last = servs;
      while(last->next != this)
        last = last->next;
      if(last != null)
        last->next = next;
    }
}

