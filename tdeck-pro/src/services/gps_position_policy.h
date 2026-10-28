#pragma once
#include <stdint.h>
#include <cmath>
#include <cstdio>
#include <string>
namespace device {
constexpr uint32_t GPS_POSITION_MAX_AGE_MS=15000;
inline std::string gpsCoordinateMessage(bool available,bool enabled,bool fix,uint32_t age,double latitude,double longitude){
 if(!available||!enabled||!fix||age>=GPS_POSITION_MAX_AGE_MS||!std::isfinite(latitude)||!std::isfinite(longitude)||latitude<-90||latitude>90||longitude<-180||longitude>180)return "";
 char text[64];std::snprintf(text,sizeof(text),"GPS: %.6f, %.6f",latitude,longitude);return text;
}
}
