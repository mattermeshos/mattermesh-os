#include <cassert>
#include <iostream>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include "services/time_sync.h"
#include "services/location.h"
namespace device {LocationState locationState(){return LocationState{};}bool locationTimeLease(bool){return true;}}
static time_t applied=0;
extern "C" int __wrap_settimeofday(const timeval *tv,const struct timezone*){applied=tv->tv_sec;return 0;}
int main(){
 using namespace device;constexpr uint32_t epoch=1791981296;
 timeSyncBegin();assert(timeSyncState().acquiring);
 fakeMillis=100;fakeWiFiStatus=WL_CONNECTED;
 assert(!timeSyncGPS(epoch,true,true));assert(timeSyncState().acquiring);
 timeval ntp={epoch,0};fakeNTPCallback(&ntp);timeSyncLoop();assert(!timeSyncState().acquiring&&timeSyncState().source==TimeSource::WiFi);
 timeSyncBegin();fakeWiFiStatus=0;
 assert(!timeSyncGPS(epoch,false,true));assert(!timeSyncGPS(epoch,true,false));assert(!timeSyncGPS(0,true,true));
 assert(timeSyncGPS(epoch,true,true));assert(applied==epoch&&!timeSyncState().acquiring&&timeSyncState().source==TimeSource::GPS);
 assert(!timeSyncGPS(epoch+1,true,true));fakeMillis=60100;assert(timeSyncGPS(epoch+60,true,true));
 fakeWiFiStatus=WL_CONNECTED;assert(!timeSyncGPS(epoch+120,true,true));fakeNTPCallback(&ntp);timeSyncLoop();assert(timeSyncState().source==TimeSource::WiFi&&!timeSyncState().acquiring);
 std::cout<<"PASS: real time service with mocked hardware, Wi-Fi skips GPS, NTP completion, fresh GPS fix clears banner immediately, stale/no-fix rejection, resync interval\n";
}
