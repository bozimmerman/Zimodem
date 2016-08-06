#include <ESP8266WiFi.h>

char petToAsc(char c, Stream *stream);
char ascToPet(char c, Stream *stream);
char handleAsciiIAC(char c, Stream *stream);

