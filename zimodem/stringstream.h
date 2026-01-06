#ifndef _STRING_STREAM_H_INCLUDED_
#define _STRING_STREAM_H_INCLUDED_
/*
   Copyright 2016-2026 Bo Zimmerman

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

class StringStream : public Stream
{
public:
    StringStream(const String &s)
    {
      str = s;
      position = 0;
    }

    // Stream methods
    virtual int available() { return str.length() - position; }
    virtual int read() { return position < str.length() ? str[position++] : -1; }
    virtual int peek() { return position < str.length() ? str[position] : -1; }
    virtual void flush() { };
    // Print methods
    virtual size_t write(uint8_t c) { str += (char)c; return 1;};

private:
    String str;
    int length;
    int position;
};

#endif // _STRING_STREAM_H_INCLUDED_
