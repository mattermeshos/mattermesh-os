#pragma once
#include "Arduino.h"
class Print {public:virtual ~Print(){};virtual size_t write(uint8_t)=0;
 virtual size_t write(const uint8_t *p,size_t n){size_t v=0;for(size_t i=0;i<n;i++)v+=write(p[i]);return v;}
 size_t print(const char *s){return write((const uint8_t*)s,strlen(s));}
 size_t print(const String &s){return print(s.c_str());}
 size_t println(const char *s){return print(s)+write('\n');}
};
