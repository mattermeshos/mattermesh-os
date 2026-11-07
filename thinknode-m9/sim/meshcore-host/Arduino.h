#pragma once
// Minimal Arduino surface for building the MeshCore core on the PC (sim/companion_emulator.cpp).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define pgm_read_dword(p) (*(const uint32_t*)(p))
using std::min;using std::max;
unsigned long millis();
inline unsigned long micros(){return millis()*1000;}
inline void delay(unsigned long){}
inline char *ltoa(long v,char *buf,int base){if(base==16)snprintf(buf,24,"%lx",v);else snprintf(buf,24,"%ld",v);return buf;}
inline long random(long n){return n>0?std::rand()%n:0;}
inline long random(long a,long b){return b>a?a+std::rand()%(b-a):a;}
#include "Stream.h"
