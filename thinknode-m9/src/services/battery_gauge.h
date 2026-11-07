#pragma once
// Battery gauge (no fuel gauge chip: only the cell voltage through the 1:2 divider on GPIO13).
// The sag model below comes from the T-Deck Plus and is a starting point for the M9.
// Redesigned 2026-10-21 from a 6.8 h discharge log (backups/plus-battery-20261021/battery-log.csv):
// - gain: the ADC reads about 3 % low (a freshly charged cell showed 4.04-4.05 V instead of ~4.17 V); default x1.030,
//   adjustable from a multimeter reading (serial "battery gain <true mV>", NVS mmbattery/gain);
// - load: the cell sags under use, so the reading is lifted by the measured sag of each load (unlocked screen and
//   keyboard light ~85 mV, GPS ~38 mV, Wi-Fi ~26 mV, CPU at 240 MHz while locked ~10 mV) to estimate the rest voltage;
// - curve: rest voltage to percent, 0 % at 3.30 V; above 3.77 V fitted to the log (about 9 %/h with that day's use),
//   below it the usual LiPo shape (no data yet under 3.77 V: refine after a full discharge);
// - shown percent: goes down one point after three readings at least 2 points lower; goes back UP one point per
//   reading only after 2 minutes at rest (locked, screen off, no Wi-Fi / GPS) with three readings at least 3 points
//   higher, so a load sag no longer costs percent for good, and normal use does not see-saw.
#include <stdint.h>
namespace device {
struct BatteryLoad {bool active=false,wifi=false,gps=false,cpuHigh=false;};// active: unlocked (screen + keyboard light on)
constexpr int PLUS_GAIN_DEFAULT_X1000=1000,PLUS_GAIN_MIN_X1000=950,PLUS_GAIN_MAX_X1000=1100;
inline int plusLoadSagMv(const BatteryLoad &l){return (l.active?85:0)+(l.gps?38:0)+(l.wifi?26:0)+(l.cpuHigh&&!l.active?10:0);}
inline bool plusAtRest(const BatteryLoad &l){return !l.active&&!l.wifi&&!l.gps;}
// Rest voltage (true mV) -> percent.
inline int plusBatteryCurve(int mv){
 // M9: a 4.35 V cell (LGS4056HDA-4.35). Generic LiHV shape, provisional until a full discharge is measured on a real M9.
 static const int16_t v[]={3300,3600,3670,3720,3760,3800,3850,3930,4020,4120,4220,4320};
 static const int8_t p[]={0,5,10,20,30,40,50,60,70,80,90,100};
 constexpr int n=sizeof(v)/sizeof(v[0]);
 if(mv<0)return -1;if(mv<=v[0])return 0;if(mv>=v[n-1])return 100;
 for(int i=1;i<n;i++)if(mv<=v[i])return p[i-1]+((mv-v[i-1])*(p[i]-p[i-1])+(v[i]-v[i-1])/2)/(v[i]-v[i-1]);
 return 100;}
// One reading every ~10 s on battery. Returns the percent to show (-1 until the first reading).
struct PlusGauge {
 float restMv=0;int shown=-1,low=0,high=0,candidate=-1;uint32_t restSince=0;bool resting=false;
 void reset(){restMv=0;shown=-1;low=high=0;candidate=-1;restSince=0;resting=false;}
 int trueMv(int rawMv,int gainX1000)const{return int((int64_t)rawMv*gainX1000/1000);}
 int update(int rawMv,const BatteryLoad &load,uint32_t nowMs,int gainX1000){
  float rest=float(trueMv(rawMv,gainX1000)+plusLoadSagMv(load));
  restMv=restMv<=0?rest:restMv+(rest-restMv)*0.15f;// EMA, about a 1 min time constant at 10 s
  bool atRest=plusAtRest(load);if(atRest&&!resting)restSince=nowMs;resting=atRest;
  candidate=plusBatteryCurve(int(restMv+0.5f));
  if(shown<0){shown=candidate;low=high=0;return shown;}
  if(candidate<=shown-2){high=0;if(++low>=3){shown--;low=0;}}
  else if(candidate>=shown+3&&resting&&uint32_t(nowMs-restSince)>=120000){low=0;if(++high>=3){shown++;high=2;}}// keep climbing one point per reading
  else{low=0;high=0;}
  return shown;}
};
}
