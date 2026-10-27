#pragma once
#include <stdint.h>
#include <cmath>
#include <cstdio>
#include <string>
namespace device {
constexpr uint32_t GPS_POSITION_MAX_AGE_MS=15000;
// A position in range; a fresh fix is also from a running receiver (Home widgets add that 0,0 means "none").
inline bool gpsPositionValid(double latitude,double longitude){return std::isfinite(latitude)&&std::isfinite(longitude)&&latitude>=-90&&latitude<=90&&longitude>=-180&&longitude<=180;}
inline bool gpsFixFresh(bool available,bool enabled,bool fix,uint32_t age,double latitude,double longitude){return available&&enabled&&fix&&age<GPS_POSITION_MAX_AGE_MS&&gpsPositionValid(latitude,longitude);}
inline std::string gpsCoordinateMessage(bool available,bool enabled,bool fix,uint32_t age,double latitude,double longitude){
 if(!gpsFixFresh(available,enabled,fix,age,latitude,longitude))return "";
 char text[64];std::snprintf(text,sizeof(text),"GPS: %.6f, %.6f",latitude,longitude);return text;
}
}
