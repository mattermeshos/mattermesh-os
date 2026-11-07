#pragma once
#include <stdint.h>
namespace device {
constexpr uint32_t TIMEOUT_VALUES[]={15,30,60,120,300,0};
constexpr int BATTERY_FULL_MV=4350;
inline bool validFullCalibration(int millivolts){return millivolts>=4200&&millivolts<=4400;}
inline bool validTimeout(uint32_t seconds){for(auto v:TIMEOUT_VALUES)if(v==seconds)return true;return false;}
inline bool expired(uint32_t now,uint32_t activity,uint32_t seconds){return seconds&&uint32_t(now-activity)>=seconds*1000UL;}
// Battery percentage: services/battery_gauge.h.
}
