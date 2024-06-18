#ifndef _STRING_STREAM_H_INCLUDED_
#define _STRING_STREAM_H_INCLUDED_

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
