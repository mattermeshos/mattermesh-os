#pragma once
#include <stdint.h>
inline bool fakeGpsValid=false,fakeGpsUpdated=false;
inline uint32_t fakeGpsAge=0;inline double fakeGpsLatitude=0,fakeGpsLongitude=0;
struct TinyGPSPlus {
 struct Location {bool isValid(){return fakeGpsValid;}uint32_t age(){return fakeGpsAge;}bool isUpdated(){return fakeGpsUpdated;}double lat(){return fakeGpsLatitude;}double lng(){fakeGpsUpdated=false;return fakeGpsLongitude;}} location;
 struct Altitude {double meters(){return 0;}bool isValid(){return true;}} altitude;
 struct Satellites {uint32_t value(){return 0;}} satellites;
 struct Date {bool isValid(){return false;}uint32_t age(){return 0;}int year(){return 2026;}int month(){return 10;}int day(){return 14;}} date;
 struct Time {bool isValid(){return false;}uint32_t age(){return 0;}int hour(){return 0;}int minute(){return 0;}int second(){return 0;}} time;
 unsigned parsed=0;
 void encode(int){parsed++;}
 unsigned charsProcessed(){return parsed;}
};
