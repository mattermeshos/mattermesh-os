#pragma once
#include <stdint.h>
namespace device {
constexpr uint32_t TIMEOUT_VALUES[]={15,30,60,120,300,0};
constexpr int BATTERY_FULL_MV=4200;
inline bool validFullCalibration(int millivolts){return millivolts>=4050&&millivolts<=4250;}
inline bool validTimeout(uint32_t seconds){for(auto v:TIMEOUT_VALUES)if(v==seconds)return true;return false;}
inline bool expired(uint32_t now,uint32_t activity,uint32_t seconds){return seconds&&uint32_t(now-activity)>=seconds*1000UL;}
// Percentage curve requested by the user (2026-10-19, battery charged): 100 % down to 4.05 V, then linear, 0 % at 3.40 V.
// The "full" calibration (fullMillivolts) no longer moves the curve; charge-done from the charger still reads 100 %.
constexpr int BATTERY_PERCENT_FULL_MV=4050,BATTERY_PERCENT_EMPTY_MV=3400;
inline int batteryPercent(int soc,bool valid,int millivolts,bool chargeDone,int fullMillivolts=BATTERY_FULL_MV){(void)soc;(void)valid;(void)fullMillivolts;if(chargeDone)return 100;if(millivolts<0)return -1;if(millivolts<=BATTERY_PERCENT_EMPTY_MV)return 0;if(millivolts>=BATTERY_PERCENT_FULL_MV)return 100;int span=BATTERY_PERCENT_FULL_MV-BATTERY_PERCENT_EMPTY_MV;return ((millivolts-BATTERY_PERCENT_EMPTY_MV)*100+span/2)/span;}
}
