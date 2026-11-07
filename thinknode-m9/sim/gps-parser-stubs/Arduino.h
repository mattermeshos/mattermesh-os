#pragma once
#include "../location-stubs/Arduino.h"
#include <cmath>
using byte=uint8_t;
constexpr double TWO_PI=6.2831853071795864769;
inline double radians(double x){return x*0.017453292519943295;}
inline double degrees(double x){return x*57.29577951308232;}
inline double sq(double x){return x*x;}
