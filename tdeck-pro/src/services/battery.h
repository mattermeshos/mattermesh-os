#pragma once
#include "policy.h"
namespace device {
struct BatteryState {int percent=-1,voltage=-1,soc=-1;bool gauge=false,charger=false,full=false,charging=false,estimated=false;};
int batteryFullMillivolts();bool batteryCalibrateFull();
enum class PowerSource {Unknown,Battery,USB};PowerSource batteryPowerSource();bool batteryShutdown();
void batteryLoop();BatteryState batteryState();
}
