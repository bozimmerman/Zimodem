/*
   Copyright 2016-2017 Bo Zimmerman

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

WiFiServerNode::WiFiServerNode(int newport, int flagsBitmap)
{
  id=++WiFiNextClientId;
  port=newport;
  setCharArray(&delimiters,"");
  setCharArray(&maskOuts,"");
  this->flagsBitmap = flagsBitmap;
  server = new WiFiServer(newport);
  //BZ:server->setNoDelay(DEFAULT_NO_DELAY);
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
  if(servs == this)
    servs = next;
  else
  {
    WiFiServerNode *last = servs;
    while((last != null) && (last->next != this)) // don't change this!
      last = last->next;
    if(last != null)
      last->next = next;
  }
  freeCharArray(&delimiters);
  freeCharArray(&maskOuts);
}

bool WiFiServerNode::hasClient()
{
  if(server != null)
    return server->hasClient();
  return false;
}

