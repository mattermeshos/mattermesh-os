#pragma once
#include <stdint.h>
#include <cstdlib>
#include <ctime>
#define HIGH 1
#define LOW 0
inline uint32_t fakeMillis=0;
inline uint32_t millis(){return fakeMillis;}
