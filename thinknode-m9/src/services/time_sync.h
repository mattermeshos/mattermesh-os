#pragma once
#include <stdint.h>
namespace device {
enum class TimeSource {None,WiFi,GPS,Manual};
struct TimeSyncState {bool acquiring=true,gpsWaiting=false,gpsTemporary=false,timedOut=false;const char *message="Acquiring time";TimeSource source=TimeSource::None;uint32_t revision=0,epoch=0;};
// clock: system time at boot. A valid one (kept across a soft reset) skips the
// automatic Wi-Fi/GPS acquisition; timeSyncRequest() still forces one.
void timeSyncBegin(uint32_t clock=0);void timeSyncRequest();void timeSyncLoop();TimeSyncState timeSyncState();
bool timeSyncNetwork(uint32_t epoch,uint32_t micros=0);
bool timeSyncGPS(uint32_t epoch,bool validTime,bool fresh);
// The clock was set by hand (already applied): stop any acquisition and keep it.
void timeSyncManual(uint32_t epoch);
// Called with the UTC epoch each time the clock is set (Wi-Fi, GPS or by hand): the board keeps it (M9: PCF8563).
extern void (*timeSyncStore)(uint32_t epoch);
}
