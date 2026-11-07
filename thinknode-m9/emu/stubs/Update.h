#pragma once
// Emulator OTA writer: there is no flash slot to update, so begin() refuses (the firmware reports it).
#include <Arduino.h>
#define U_FLASH 0
class UpdateClass {
public:
 bool begin(size_t,int=U_FLASH){return false;}
 size_t write(const uint8_t*,size_t){return 0;}
 bool end(bool=false){return false;}
 void abort(){}
 const char *errorString(){return "no OTA slot in the emulator";}
};
inline UpdateClass Update;
