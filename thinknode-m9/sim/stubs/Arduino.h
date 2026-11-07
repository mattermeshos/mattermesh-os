#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <algorithm>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define pgm_read_dword(p) (*(const uint32_t*)(p))
#define pgm_read_ptr(p) (*(const void *const*)(p))
class __FlashStringHelper;
class String:public std::string { public:using std::string::string;String(const std::string &v):std::string(v){};unsigned length()const{return size();} };
using std::min;using std::max;
