#include "battery.h"
#include <Arduino.h>
#include "../board/board.h"
#include <Preferences.h>
namespace device {
// Shared by both boards: last reading, NVS full-charge calibration.
static BatteryState state;static uint32_t last=0;static bool initialized=false;
static Preferences calibration;static int fullMillivolts=BATTERY_FULL_MV;
static void loadCalibration(){calibration.begin("mmbattery",false);int saved=calibration.getInt("full_mv",BATTERY_FULL_MV);if(validFullCalibration(saved))fullMillivolts=saved;}
int batteryFullMillivolts(){return fullMillivolts;}
BatteryState batteryState(){return state;}
}
// T-Deck Plus: no fuel gauge and no charger on I2C, only the battery voltage on GPIO4 (1:2 divider). The percentage
// comes from the same voltage curve; USB is recognised by the charger voltage, charging cannot be told apart.
namespace device {
// The voltage is read every 10 s; USB / battery is decided with hysteresis (enter >= 4400 mV, leave <= 4300 mV)
// confirmed by two readings in a row, then 30 s of settling; batteryPowerSource() returns the last decision.
// The percent itself: services/battery_gauge.h (gain, load sag, rest-voltage curve, shown-percent rules).
static PlusGauge gauge;static BatteryLoad load;static int gainX1000=PLUS_GAIN_DEFAULT_X1000;static int sourceVotes=0;static uint32_t settleUntil=0;
static PowerSource source=PowerSource::Unknown;static BatteryTrace trace;
void batteryLoad(const BatteryLoad &now){load=now;}
int batteryGainX1000(){return gainX1000;}
// Multimeter calibration: the true cell voltage now (mV) against the current raw reading.
bool batterySetTrueMillivolts(int trueMv){int raw=plus::batteryMillivolts();if(raw<2500||trueMv<3000||trueMv>4400)return false;int g=int(int64_t(trueMv)*1000/raw);
 if(g<PLUS_GAIN_MIN_X1000||g>PLUS_GAIN_MAX_X1000)return false;gainX1000=g;calibration.putInt("gain",g);gauge.reset();return true;}
void batteryLoop(){if(!initialized){loadCalibration();int g=calibration.getInt("gain",PLUS_GAIN_DEFAULT_X1000);if(g>=PLUS_GAIN_MIN_X1000&&g<=PLUS_GAIN_MAX_X1000)gainX1000=g;}
 if(initialized&&millis()-last<10000)return;initialized=true;last=millis();int v=plus::batteryMillivolts();trace.rawMv=v;trace.readAt=last;
 BatteryState next=BatteryState();next.percent=state.percent;next.voltage=state.voltage;next.estimated=state.estimated;
 if(v<2500||v>5000){state=BatteryState();source=PowerSource::Unknown;return;}
 // USB / battery with hysteresis (enter >= 4400 mV, leave <= 4300 mV), two readings in a row, then 30 s of settling.
 PowerSource seen=source==PowerSource::USB?(v<=4300?PowerSource::Battery:PowerSource::USB):(v>=4400?PowerSource::USB:PowerSource::Battery);
 if(source==PowerSource::Unknown){source=seen;sourceVotes=0;}
 else if(seen!=source){if(++sourceVotes>=2){source=seen;sourceVotes=0;gauge.reset();settleUntil=millis()+30000;}}
 else sourceVotes=0;
 if(source==PowerSource::USB){next.voltage=gauge.trueMv(v,gainX1000);next.percent=100;next.estimated=true;state=next;return;}// the charger rail, not the cell
 if(settleUntil){if(int32_t(settleUntil-millis())>0){state=next;return;}settleUntil=0;}// just unplugged: keep the last percent while the cell settles (never an empty "--%" gauge)
 next.percent=gauge.update(v,load,last,gainX1000);next.voltage=int(gauge.restMv+0.5f);trace.curvePercent=gauge.candidate;
 next.estimated=true;state=next;}
BatteryTrace batteryTrace(){trace.filteredMv=gauge.restMv>0?int(gauge.restMv+0.5f):-1;trace.shownPercent=state.percent;trace.source=int(source);return trace;}
PowerSource batteryPowerSource(){if(source==PowerSource::Unknown){int v=plus::batteryMillivolts();return v>=4400?PowerSource::USB:v>=2500?PowerSource::Battery:PowerSource::Unknown;}return source;}
bool batteryCalibrateFull(){return false;}
}
