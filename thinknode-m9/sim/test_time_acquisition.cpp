#include <cassert>
#include <iostream>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include "services/time_sync.h"
#include "services/time_sync_policy.h"
#include "services/location.h"
namespace device {
static LocationState location;static int leases=0;static bool failLease=false;
LocationState locationState(){return location;}
bool locationTimeLease(bool take){leases++;if(failLease)return false;location.timeLease=take;location.enabled=location.userEnabled||take;return true;}
}
static time_t applied=0;
extern "C" int __wrap_settimeofday(const timeval *tv,const struct timezone*){applied=tv->tv_sec;return 0;}
int main(){using namespace device;constexpr uint32_t epoch=1792061296;
 auto boot=[](bool wifi,bool gps){fakeMillis=100;fakeWiFiStatus=wifi?WL_CONNECTED:0;fakeWiFiMode=wifi?WIFI_STA:WIFI_OFF;location=LocationState{};location.available=true;location.userEnabled=location.enabled=gps;leases=0;failLease=false;timeSyncBegin();fakeMillis+=TIME_BOOT_GRACE_MS;timeSyncLoop();};
 applied=epoch;assert(!timeSyncNetwork(0)&&applied==epoch);assert(!timeSyncNetwork(epoch,1000000)&&applied==epoch);assert(timeSyncNetwork(epoch)&&applied==epoch);
 boot(true,false);assert(!location.enabled&&leases==0&&!timeSyncState().gpsWaiting);assert(!timeSyncGPS(epoch,true,true));timeval ntp={epoch,0};fakeNTPCallback(&ntp);timeSyncLoop();assert(timeSyncState().source==TimeSource::WiFi&&!timeSyncState().acquiring&&leases==0);
 boot(true,true);fakeNTPCallback(&ntp);timeSyncLoop();assert(location.enabled&&leases==0);
 boot(false,false);assert(location.enabled&&location.timeLease&&timeSyncState().gpsTemporary);assert(!timeSyncGPS(0,true,true)&&!timeSyncGPS(epoch,false,true)&&!timeSyncGPS(epoch,true,false));assert(timeSyncGPS(epoch,true,true));assert(applied==epoch&&location.enabled&&location.timeLease&&!timeSyncState().acquiring);fakeMillis+=GPS_AFTER_SYNC_MS-1;timeSyncLoop();assert(location.enabled);fakeMillis++;timeSyncLoop();assert(!location.enabled&&!location.timeLease);
 boot(false,true);assert(timeSyncGPS(epoch,true,true)&&location.enabled&&location.userEnabled);fakeMillis+=GPS_AFTER_SYNC_MS;timeSyncLoop();assert(location.enabled&&!location.timeLease);
 boot(false,false);fakeMillis+=GPS_LEASE_TIMEOUT_MS-1;timeSyncLoop();assert(!timeSyncState().timedOut&&location.enabled&&timeSyncState().acquiring);fakeMillis++;timeSyncLoop();assert(timeSyncState().timedOut&&!location.enabled&&!location.timeLease&&!timeSyncState().acquiring&&!timeSyncState().gpsWaiting);// GPS powered on for the clock only: off after 5 min indoors
 fakeMillis+=600000;timeSyncLoop();assert(!location.enabled&&leases==2);// no retry loop
 boot(false,true);fakeMillis+=600000;timeSyncLoop();assert(!timeSyncState().timedOut&&location.enabled&&timeSyncState().acquiring);// user GPS ON: never touched, no deadline
 boot(false,false);location.userEnabled=true;location.userRevision++;fakeMillis+=1000;timeSyncLoop();fakeMillis+=GPS_LEASE_TIMEOUT_MS;timeSyncLoop();assert(location.enabled&&!timeSyncState().timedOut&&timeSyncState().acquiring);// switched ON by the user during the lease: deadline dropped
 boot(false,false);fakeWiFiStatus=WL_CONNECTED;timeSyncLoop();assert(!location.enabled&&!timeSyncState().gpsWaiting);fakeNTPCallback(&ntp);timeSyncLoop();assert(timeSyncState().source==TimeSource::WiFi);
 boot(false,false);location.userEnabled=true;location.userRevision++;timeSyncLoop();assert(timeSyncGPS(epoch,true,true)&&location.enabled);
 boot(false,false);location.enabled=location.timeLease=false;location.userRevision++;timeSyncLoop();assert(!location.enabled&&!timeSyncState().acquiring);
 boot(true,false);fakeMillis+=NETWORK_SYNC_TIMEOUT_MS;timeSyncLoop();assert(timeSyncState().timedOut&&!location.enabled&&leases==0);
 boot(false,false);timeSyncRequest();timeSyncLoop();assert(location.enabled&&timeSyncState().gpsWaiting);
 boot(false,false);failLease=true;assert(timeSyncGPS(epoch,true,true));fakeMillis+=GPS_AFTER_SYNC_MS;timeSyncLoop();assert(timeSyncState().gpsWaiting&&location.enabled);fakeWiFiStatus=WL_CONNECTED;fakeNTPCallback(&ntp);timeSyncLoop();assert(timeSyncState().gpsWaiting);failLease=false;fakeMillis+=1000;timeSyncLoop();assert(!location.enabled&&timeSyncState().source==TimeSource::WiFi);
 // Wi-Fi switched on but still joining: NTP first, GPS untouched; GPS only after the network window.
 auto bootJoining=[&](bool gps){fakeMillis=100;fakeWiFiStatus=0;fakeWiFiMode=WIFI_STA;location=LocationState{};location.available=true;location.userEnabled=location.enabled=gps;leases=0;failLease=false;timeSyncBegin();fakeMillis+=TIME_BOOT_GRACE_MS;timeSyncLoop();};
 bootJoining(false);assert(!location.enabled&&leases==0&&timeSyncState().acquiring);fakeMillis+=5000;timeSyncLoop();assert(leases==0);fakeWiFiStatus=WL_CONNECTED;fakeNTPCallback(&ntp);timeSyncLoop();assert(timeSyncState().source==TimeSource::WiFi&&!location.enabled&&leases==0);
 bootJoining(true);fakeMillis+=5000;timeSyncLoop();fakeWiFiStatus=WL_CONNECTED;fakeNTPCallback(&ntp);timeSyncLoop();assert(timeSyncState().source==TimeSource::WiFi&&location.enabled&&location.userEnabled&&leases==0);
 bootJoining(false);fakeMillis+=NETWORK_SYNC_TIMEOUT_MS-1;timeSyncLoop();assert(leases==0&&!location.enabled);fakeMillis++;timeSyncLoop();assert(location.enabled&&location.timeLease&&timeSyncState().gpsTemporary);
 assert(timeSyncGPS(epoch,true,true));fakeMillis+=GPS_AFTER_SYNC_MS;timeSyncLoop();assert(!location.enabled&&timeSyncState().source==TimeSource::GPS);
 bootJoining(false);fakeMillis+=2000;fakeWiFiMode=WIFI_OFF;timeSyncLoop();assert(location.enabled&&location.timeLease);// switched off while waiting
 // Clock kept across a soft reset: no Wi-Fi wait, no GPS lease, even on link changes; manual request still works.
 auto bootKept=[&](bool wifiOn){fakeMillis=100;fakeWiFiStatus=0;fakeWiFiMode=wifiOn?WIFI_STA:WIFI_OFF;location=LocationState{};location.available=true;leases=0;failLease=false;timeSyncBegin(epoch);fakeMillis+=TIME_BOOT_GRACE_MS;timeSyncLoop();};
 bootKept(false);assert(!timeSyncState().acquiring&&leases==0&&!location.enabled);fakeMillis+=600000;timeSyncLoop();assert(leases==0&&!location.enabled);
 bootKept(true);fakeMillis+=NETWORK_SYNC_TIMEOUT_MS*2;timeSyncLoop();assert(leases==0&&!timeSyncState().acquiring);fakeWiFiStatus=WL_CONNECTED;timeSyncLoop();fakeWiFiStatus=0;timeSyncLoop();assert(leases==0&&!location.enabled&&!timeSyncState().acquiring);
 bootKept(false);fakeWiFiStatus=WL_CONNECTED;fakeNTPCallback(&ntp);timeSyncLoop();assert(timeSyncState().source==TimeSource::WiFi&&leases==0);// passive NTP still accepted
 bootKept(false);timeSyncRequest();timeSyncLoop();assert(location.enabled&&location.timeLease&&timeSyncState().acquiring);
 bootKept(false);timeSyncBegin(1000);fakeMillis+=TIME_BOOT_GRACE_MS;timeSyncLoop();assert(location.enabled&&timeSyncState().acquiring);// 1970 clock after power loss
#ifdef ESP32
 applied=epoch;timeval bad={1,0};sntp_sync_time(&bad);assert(applied==epoch);timeval valid={epoch+1,0};sntp_sync_time(&valid);assert(applied==epoch+1&&fakeSNTPStatus==SNTP_SYNC_STATUS_COMPLETED);
#endif
 boot(false,false);location.fix=false;assert(timeSyncGPS(epoch,true,true));fakeMillis+=GPS_AFTER_SYNC_MS;timeSyncLoop();assert(!location.enabled); // fresh valid UTC before position fix
 boot(false,false);assert(timeSyncGPS(epoch,true,true));location.userEnabled=true;location.userRevision++;fakeMillis+=GPS_AFTER_SYNC_MS;timeSyncLoop();assert(location.enabled&&location.userEnabled&&!location.timeLease);
 boot(false,false);fakeMillis=0xfffffff0;assert(timeSyncGPS(epoch,true,true));fakeMillis+=GPS_AFTER_SYNC_MS;timeSyncLoop();assert(!location.enabled);
 std::cout<<"PASS: valid boot clock skips Wi-Fi/GPS acquisition (manual request/1970 clock still acquire), Wi-Fi ON waits for NTP with GPS untouched then GPS fallback, temporary GPS lease off after GPS_LEASE_TIMEOUT_MS (user GPS ON has no deadline), fresh UTC before position fix,5s lease tail/rollover, manual override; real time service, mocked GPS/NTP hardware: Wi-Fi priority/no GPS accesses, GPS lease success/timeout/restoration, original ON retained, fresh valid fix only, network transition, user override, bounded acquisition/retry\n";
}
