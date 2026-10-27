#pragma once
#include "policy.h"
#include "battery_gauge.h"
namespace device {
struct BatteryState {int percent=-1,voltage=-1,soc=-1;bool gauge=false,charger=false,full=false,charging=false,estimated=false;};
int batteryFullMillivolts();bool batteryCalibrateFull();
enum class PowerSource {Unknown,Battery,USB};PowerSource batteryPowerSource();
void batteryLoop();BatteryState batteryState();
// Plus: last raw reading, filtered voltage, percent from the curve and the shown percent (battery log).
// Loads that sag the cell (set by the main loop each pass); multimeter calibration of the ADC gain.
void batteryLoad(const BatteryLoad &load);int batteryGainX1000();bool batterySetTrueMillivolts(int trueMv);
struct BatteryTrace {int rawMv=-1,filteredMv=-1,curvePercent=-1,shownPercent=-1,source=0;uint32_t readAt=0;};BatteryTrace batteryTrace();
}
