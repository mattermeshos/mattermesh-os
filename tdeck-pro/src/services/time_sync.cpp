#include "clock_limits.h"
#include "time_sync.h"
#include "time_sync_policy.h"
#include "location.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <atomic>
#include <limits>
#include <time.h>
namespace device {
static TimeSyncState state;static uint32_t lastGPS=0,bootAt=0,modeAt=0,userRevision=0;
static bool started=false,lastWifi=false,gpsAttempted=false,releaseScheduled=false,clockKept=false;
static std::atomic<uint32_t> networkEpoch{0};static bool restorePending=false;static uint32_t restoreAt=0,releaseAt=0,leaseAt=0;
bool timeSyncNetwork(uint32_t epoch,uint32_t micros){
 if(epoch<MM_EPOCH_MIN||epoch>=MM_EPOCH_END||uint64_t(epoch)>uint64_t(std::numeric_limits<time_t>::max())||micros>=1000000)return false;
 timeval tv={time_t(epoch),suseconds_t(micros)};if(settimeofday(&tv,nullptr))return false;networkEpoch.store(epoch);return true;
}
// sntp_sync_time() below already set the clock; the notification only records the epoch (no second settimeofday).
static void networkTime(struct timeval *tv){if(tv&&tv->tv_sec>=MM_EPOCH_MIN&&tv->tv_sec<MM_EPOCH_END)networkEpoch.store(uint32_t(tv->tv_sec));}
static void releaseGPS(){
 releaseScheduled=false;
 if(state.gpsWaiting&&!locationTimeLease(false)){restorePending=true;restoreAt=millis();state.message="GPS power restore pending";state.revision++;return;}
 restorePending=false;state.gpsWaiting=state.gpsTemporary=false;
}
static void acquired(TimeSource source,uint32_t epoch){
 if(source==TimeSource::GPS&&state.gpsWaiting){releaseAt=millis();releaseScheduled=true;}
 else if(source==TimeSource::WiFi)releaseGPS();
 state.revision++;state.acquiring=false;state.timedOut=false;state.source=source;state.epoch=epoch;
 state.message=restorePending?"GPS power restore pending":source==TimeSource::WiFi?"Wi-Fi / NTP synchronized":source==TimeSource::Manual?"Set manually":"GPS synchronized";
}
void timeSyncBegin(uint32_t clock){state=TimeSyncState{};state.revision=1;lastGPS=0;networkEpoch=0;started=gpsAttempted=restorePending=releaseScheduled=false;
 clockKept=clock>=MM_EPOCH_MIN&&clock<MM_EPOCH_END;if(clockKept){state.acquiring=false;state.epoch=clock;state.message="Clock kept / no sync";}
bootAt=modeAt=millis();setenv("TZ",MM_TZ,1);tzset();sntp_set_time_sync_notification_cb(networkTime);}
void timeSyncRequest(){
#ifdef ESP32
 if(WiFi.status()==WL_CONNECTED)sntp_restart();
#endif
 releaseGPS();clockKept=false;state.acquiring=true;state.timedOut=false;state.message="Acquiring time";state.revision++;started=gpsAttempted=false;bootAt=millis()-TIME_BOOT_GRACE_MS;
}
void timeSyncLoop(){
 bool connected=WiFi.status()==WL_CONNECTED;uint32_t now=millis();
 if(restorePending){if(uint32_t(now-restoreAt)>=1000)releaseGPS();if(restorePending)return;}
 uint32_t epoch=networkEpoch.exchange(0);if(epoch)acquired(TimeSource::WiFi,epoch);
 if(!started){if(uint32_t(now-bootAt)<TIME_BOOT_GRACE_MS)return;started=true;lastWifi=connected;modeAt=now;}
 // Valid clock at boot: no automatic Wi-Fi/GPS acquisition, not even on link changes.
 if(clockKept){lastWifi=connected;return;}
 if(connected!=lastWifi){releaseGPS();lastWifi=connected;modeAt=now;gpsAttempted=false;state.acquiring=true;state.timedOut=false;state.revision++;if(epoch)acquired(TimeSource::WiFi,epoch);}
 if(connected){
  releaseGPS();if(!state.acquiring)return;
  if(uint32_t(now-modeAt)>=NETWORK_SYNC_TIMEOUT_MS){state.acquiring=false;state.timedOut=true;state.message="NTP unavailable / retry later";state.revision++;}else state.message="Acquiring time / Wi-Fi";
  return;
 }
 // A manual switch to OFF cancels both an acquisition lease and its5s tail.
 auto loc=locationState();
 if(state.gpsWaiting&&loc.userRevision!=userRevision){userRevision=loc.userRevision;state.gpsTemporary=!loc.userEnabled;
  if(!loc.userEnabled){releaseGPS();state.acquiring=false;state.message="Time sync canceled / GPS off";state.revision++;return;}
 }
 if(releaseScheduled&&uint32_t(now-releaseAt)>=GPS_AFTER_SYNC_MS){releaseGPS();state.revision++;}
 if(!state.acquiring)return;
 // Wi-Fi switched on: wait for NTP and leave GPS untouched; fall back to GPS only if no link appears.
 if(!gpsAttempted&&WiFi.getMode()!=WIFI_OFF&&uint32_t(now-modeAt)<NETWORK_SYNC_TIMEOUT_MS){state.message="Acquiring time / Wi-Fi";return;}
 if(!gpsAttempted){gpsAttempted=true;
  if(!loc.available||!locationTimeLease(true)){state.acquiring=false;state.message="GPS unavailable / clock kept";state.revision++;return;}
  state.gpsWaiting=true;state.gpsTemporary=!loc.userEnabled;userRevision=loc.userRevision;leaseAt=now;state.message="Acquiring time / GPS";state.revision++;
 }
 // A GPS the user keeps ON searches without deadline (nothing to switch off). A GPS powered on
 // only for the clock goes off again after GPS_LEASE_TIMEOUT_MS: indoors it would never fix.
 if(state.gpsWaiting&&state.gpsTemporary&&uint32_t(now-leaseAt)>=GPS_LEASE_TIMEOUT_MS){releaseGPS();state.acquiring=false;state.timedOut=true;state.message="No GPS time in 5 min / GPS off";state.revision++;}
}
bool timeSyncGPS(uint32_t epoch,bool validTime,bool fresh){
 if(!validTime||!fresh||uint64_t(epoch)>uint64_t(std::numeric_limits<time_t>::max())||epoch<MM_EPOCH_MIN||epoch>=MM_EPOCH_END||WiFi.status()==WL_CONNECTED||(!state.acquiring&&!gpsTimeDue(false,state.source==TimeSource::GPS,lastGPS,millis())))return false;
 timeval tv={time_t(epoch),0};if(settimeofday(&tv,nullptr))return false;lastGPS=millis();acquired(TimeSource::GPS,epoch);return true;
}
bool timeSyncManual(uint32_t epoch){
 if(epoch<MM_EPOCH_MIN||epoch>=MM_EPOCH_END)return false;
 timeval tv={time_t(epoch),0};if(settimeofday(&tv,nullptr))return false;
 releaseGPS();clockKept=true;started=true;acquired(TimeSource::Manual,epoch);return true;
}
TimeSyncState timeSyncState(){return state;}
}
#ifdef ESP32
extern "C" void sntp_sync_time(struct timeval *tv){
 if(tv&&tv->tv_sec>=MM_EPOCH_MIN&&tv->tv_sec<MM_EPOCH_END&&tv->tv_usec>=0&&device::timeSyncNetwork(uint32_t(tv->tv_sec),uint32_t(tv->tv_usec)))sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}
#endif
