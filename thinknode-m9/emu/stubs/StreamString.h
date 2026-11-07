#pragma once
// Arduino-ESP32 StreamString: a String usable as a Stream (HTTPClient::getString writes the body into one).
#include <Arduino.h>
class StreamString:public Stream,public String {
public:
 size_t write(const uint8_t *data,size_t n)override{for(size_t i=0;i<n;i++)String::operator+=(char(data[i]));return n;}
 size_t write(uint8_t c)override{String::operator+=(char(c));return 1;}
 int available()override{return int(length());}
 int read()override{if(!length())return -1;char c=charAt(0);remove(0,1);return uint8_t(c);}
 int peek()override{return length()?uint8_t(charAt(0)):-1;}
 void flush()override{}
};
