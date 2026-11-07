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
// ThinkNode M9: no fuel gauge, the cell voltage on GPIO13 (1:2 divider, unreadable while Wi-Fi is on: the last percent stays)
// and USB power on GPIO1. Charging cannot be told from full.
namespace device {
// The voltage is read every 10 s; after USB is unplugged 30 s of settling.
// The percent itself: services/battery_gauge.h (gain, load sag, rest-voltage curve, shown-percent rules).
static PlusGauge gauge;static BatteryLoad load;static int gainX1000=PLUS_GAIN_DEFAULT_X1000;static uint32_t settleUntil=0;
static PowerSource source=PowerSource::Unknown;static BatteryTrace trace;
void batteryLoad(const BatteryLoad &now){load=now;}
int batteryGainX1000(){return gainX1000;}
// Multimeter calibration: the true cell voltage now (mV) against the current raw reading.
bool batterySetTrueMillivolts(int trueMv){int raw=hw::batteryMillivolts();if(raw<2500||trueMv<3000||trueMv>4400)return false;int g=int(int64_t(trueMv)*1000/raw);
 if(g<PLUS_GAIN_MIN_X1000||g>PLUS_GAIN_MAX_X1000)return false;gainX1000=g;calibration.putInt("gain",g);gauge.reset();return true;}
void batteryLoop(){if(!initialized){loadCalibration();int g=calibration.getInt("gain",PLUS_GAIN_DEFAULT_X1000);if(g>=PLUS_GAIN_MIN_X1000&&g<=PLUS_GAIN_MAX_X1000)gainX1000=g;}
 if(initialized&&millis()-last<10000)return;initialized=true;last=millis();
 PowerSource seen=hw::usbPowered()?PowerSource::USB:PowerSource::Battery;
 if(seen!=source){if(source!=PowerSource::Unknown){gauge.reset();settleUntil=millis()+30000;}source=seen;}
 if(!hw::batteryReadable())return;// the last percent and voltage stay on screen
 int v=hw::batteryMillivolts();trace.rawMv=v;trace.readAt=last;
 BatteryState next=BatteryState();next.percent=state.percent;next.voltage=state.voltage;next.estimated=state.estimated;
 if(v<2500||v>5000){state=BatteryState();return;}
 if(source==PowerSource::USB){next.voltage=gauge.trueMv(v,gainX1000);next.percent=100;next.estimated=true;state=next;return;}// the charger holds the cell up
 if(settleUntil){if(int32_t(settleUntil-millis())>0){state=next;return;}settleUntil=0;}// just unplugged: keep the last percent while the cell settles (never an empty "--%" gauge)
 next.percent=gauge.update(v,load,last,gainX1000);next.voltage=int(gauge.restMv+0.5f);trace.curvePercent=gauge.candidate;
 next.estimated=true;state=next;}
BatteryTrace batteryTrace(){trace.filteredMv=gauge.restMv>0?int(gauge.restMv+0.5f):-1;trace.shownPercent=state.percent;trace.source=int(source);return trace;}
PowerSource batteryPowerSource(){return source==PowerSource::Unknown?(hw::usbPowered()?PowerSource::USB:PowerSource::Battery):source;}
bool batteryCalibrateFull(){return false;}
}
