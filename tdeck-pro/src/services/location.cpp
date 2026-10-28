#include "location.h"
#include "time_sync.h"
#include "gps_nmea.h"
#include "gps_probe.h"
#include "gps_assist.h"
#include "gps_backup.h"
#include "time_sync_policy.h"
#include <Preferences.h>
#include <TinyGPS++.h>
#include "board/board.h"
#include <math.h>
#include <time.h>
namespace device {
static GpsNmea nmea;static GpsProbe probe;static uint32_t baudAt=0;static Preferences preferences;static TinyGPSPlus gps;static LocationState state;static uint32_t lastSave=0,start=0;static bool alternate=false,assisted=false;static std::string assistSent="none";
static GpsBackup backup;static LocationStore store;static uint32_t goodSince=0;// start of the current run of good fixes
static uint32_t fixFirstAt=0,lastDumpAt=0,offDeadline=0,saves=0,saveFailures=0;static bool fixSeen=false,dumped=false,offPending=false;
static void sendUbx(const uint8_t *data,size_t size){SerialGPS.write(data,size);}
static uint32_t clockEpoch(){uint32_t t=uint32_t(time(nullptr));return t>=GPS_ASSIST_MIN_EPOCH&&t<GPS_ASSIST_MAX_EPOCH?t:0;}
static bool dumpWanted(uint32_t now){return fixSeen&&store.save&&backup.idle()&&!probe.busy();}
static void saveBackup(uint32_t now){auto data=backup.take();if(store.save&&store.save(data)){saves++;dumped=true;lastDumpAt=now;}else saveFailures++;state.revision++;}
static bool available_=false;
static bool applyEnabled(bool enabled){
 if(!available_)return false;
 probe.cancel();
 if(!enabled)SerialGPS.end();
 digitalWrite(BOARD_GPS_EN,enabled?HIGH:LOW);
 if(digitalRead(BOARD_GPS_EN)!=(enabled?HIGH:LOW))return false;
 state.enabled=enabled;state.fix=state.data=false;state.satellites=0;state.baud=enabled?38400:0;
 gps=TinyGPSPlus();nmea=GpsNmea{};state.nmeaFresh=state.timeValid=false;state.serialBytes=state.nmeaValid=state.nmeaBad=state.satellitesUsed=state.satellitesVisible=0;state.nmeaAgeMs=UINT32_MAX;alternate=assisted=false;start=baudAt=millis();
 backup=GpsBackup();fixSeen=offPending=false;fixFirstAt=0;
 if(enabled){SerialGPS.setRxBufferSize(2048);SerialGPS.setTxBufferSize(1024);SerialGPS.begin(state.baud,SERIAL_8N1,BOARD_GPS_RXD,BOARD_GPS_TXD);}
 state.revision++;return true;
}
void locationBegin(bool ready){
 preferences.begin("mmlocation",false);state.configured=preferences.getBool("valid",false);state.latitude=preferences.getDouble("latitude",0);state.longitude=preferences.getDouble("longitude",0);
 state.available=ready;state.enabled=false;state.timeLease=false;state.userEnabled=preferences.getBool("gps_on",true);available_=ready;
 if(ready){pinMode(BOARD_GPS_EN,OUTPUT);if(!applyEnabled(state.userEnabled)){applyEnabled(false);state.available=false;}}
}
bool locationTimeLease(bool acquire){
 if(!state.available)return false;bool wanted=state.userEnabled||acquire;uint32_t now=millis();
 if(wanted)offPending=false;
 // Release with a fix seen: save the navigation database first (a few seconds at most), then power off from locationLoop.
 if(!wanted&&state.enabled&&!offPending&&dumpWanted(now)&&(!dumped||uint32_t(now-lastDumpAt)>=GPS_BACKUP_BEFORE_OFF_MS)&&backup.startDump(now,clockEpoch(),sendUbx)){offPending=true;offDeadline=now+GPS_BACKUP_OFF_DEADLINE_MS;state.revision++;return true;}
 if(wanted!=state.enabled&&!applyEnabled(wanted)){applyEnabled(state.enabled);return false;}
 state.timeLease=acquire;state.revision++;return true;
}
bool locationEnabled(bool enabled){
 if(!state.available)return false;offPending=false;
 bool old=state.enabled;if(enabled!=old&&!applyEnabled(enabled)){applyEnabled(old);return false;}
 if(preferences.putBool("gps_on",enabled)!=1){applyEnabled(old);return false;}
 state.userEnabled=enabled;state.timeLease=false;state.userRevision++;state.revision++;return true;
}
bool locationToggleUser(){return locationEnabled(!state.userEnabled);}
int locationPowerLevel(){return available_?digitalRead(BOARD_GPS_EN):-1;}
bool locationSet(double latitude,double longitude){if(!isfinite(latitude)||!isfinite(longitude)||latitude<-90||latitude>90||longitude<-180||longitude>180)return false;if(!state.fix){state.latitude=latitude;state.longitude=longitude;}state.configured=true;preferences.putDouble("latitude",latitude);preferences.putDouble("longitude",longitude);preferences.putBool("valid",true);state.revision++;return true;}
void locationLoop(){
 if(!state.enabled)return;uint32_t now=millis();int budget=1024;
 while(SerialGPS.available()&&budget--){int ch=SerialGPS.read();if(ch<0)break;if(!probe.feed(uint8_t(ch),now)){nmea.feed(char(ch),now);gps.encode(ch);}else if(probe.frameReady){probe.frameReady=false;backup.onFrame(probe.cls,probe.id,probe.payload,probe.length,now);}}
 probe.tick(now,[](const uint8_t *data,size_t size){SerialGPS.write(data,size);});
 backup.tick(now,sendUbx);if(backup.ready)saveBackup(now);
 // Lease released: the GPS goes off once the database is saved or the deadline passes.
 if(offPending&&(backup.idle()||int32_t(now-offDeadline)>=0)){offPending=false;if(!state.userEnabled){applyEnabled(false);state.timeLease=false;state.revision++;return;}}
 bool oldData=state.data,oldFresh=state.nmeaFresh,oldTime=state.timeValid;auto oldUsed=state.satellitesUsed,oldVisible=state.satellitesVisible;
 state.data=nmea.bytes>0;state.serialBytes=nmea.bytes;state.nmeaValid=nmea.valid;state.nmeaBad=nmea.bad;state.nmeaFresh=nmea.fresh(now);state.nmeaAgeMs=nmea.valid?uint32_t(now-nmea.lastValid):UINT32_MAX;
 state.satellitesUsed=nmea.satellitesUsed(now);state.satellitesVisible=nmea.satellitesVisible(now);state.satellites=state.satellitesVisible?state.satellitesVisible:state.satellitesUsed;
 // Retry the two rates already supported by this board when no valid NMEA
 // has ever arrived. Noise bytes alone must not freeze baud discovery.
 if(!nmea.valid&&uint32_t(now-baudAt)>5000){SerialGPS.end();alternate=!alternate;state.baud=alternate?9600:38400;SerialGPS.begin(state.baud,SERIAL_8N1,BOARD_GPS_RXD,BOARD_GPS_TXD);baudAt=now;state.revision++;}
 // Once per power-on, as soon as the baud is known: aiding frames (gps_assist.h).
 if(nmea.valid&&!assisted){assisted=true;assistSent="ana";auto send=[](const UbxFrame &f){if(f.size)SerialGPS.write(f.bytes,f.size);return f.size>0;};
  send(ubxEnableAssistAutonomous());send(ubxEnableAckAiding());if(send(ubxTimeUtc(uint32_t(time(nullptr)))))assistSent+="+time";if(state.configured&&send(ubxPositionLlh(state.latitude,state.longitude)))assistSent+="+position";
  // Navigation database saved by the previous power-on (SD): replayed entry by entry, MGA-ACK paced.
  if(store.load){std::vector<uint8_t> saved;if(store.load(saved)&&backup.startRestore(saved,now,sendUbx))assistSent+="+database";}}
 bool fix=gps.location.isValid()&&gps.location.age()<15000&&nmea.positionTrusted(now)&&nmea.solutionGood(now);
 if(!fix)goodSince=0;else if(!goodSince)goodSince=now?now:1;if(fix!=state.fix){state.fix=fix;state.revision++;}
 if(fix&&!fixSeen){fixSeen=true;fixFirstAt=now;}
 // Database dump: 30 s after the first fix, then every 10 min while powered (gps_backup.h).
 if(!offPending&&dumpWanted(now)&&uint32_t(now-fixFirstAt)>=GPS_BACKUP_AFTER_FIX_MS&&(!dumped||uint32_t(now-lastDumpAt)>=GPS_BACKUP_INTERVAL_MS))backup.startDump(now,clockEpoch(),sendUbx);
 if(fix&&gps.location.isUpdated()){
  double lat=gps.location.lat(),lon=gps.location.lng();bool changed=fabs(lat-state.latitude)>0.001||fabs(lon-state.longitude)>0.001;
  state.latitude=lat;state.longitude=lon;state.configured=true;state.altitude=gps.altitude.meters();if(changed)state.revision++;
  // NVS keeps the last position (map start, weather without a fix, GPS aiding): only a fix held for 30 s, rewritten
  // after ~100 m of movement (GPS jitter alone used to write it every minute).
  static double savedLat=NAN,savedLon=NAN;
  if(uint32_t(now-goodSince)>=30000&&millis()-lastSave>60000&&!(fabs(lat-savedLat)<=0.001&&fabs(lon-savedLon)<=0.001)){lastSave=millis();savedLat=lat;savedLon=lon;preferences.putDouble("latitude",lat);preferences.putDouble("longitude",lon);if(!preferences.getBool("valid",false))preferences.putBool("valid",true);}
 }
 // TinyGPS++ commits checksum-valid RMC time/date even when its position
 // validity flag is V. A fresh clock does not require a position fix.
 state.timeValid=false;
 if(gps.date.isValid()&&gps.time.isValid()&&gps.date.age()<2000&&gps.time.age()<2000&&state.nmeaFresh){
  uint32_t epoch=gpsUtcEpoch(gps.date.year(),gps.date.month(),gps.date.day(),gps.time.hour(),gps.time.minute(),gps.time.second());
  state.timeValid=epoch!=0;if(state.timeValid)timeSyncGPS(epoch,true,true);
 }
 if(oldData!=state.data||oldFresh!=state.nmeaFresh||oldTime!=state.timeValid||oldUsed!=state.satellitesUsed||oldVisible!=state.satellitesVisible)state.revision++;
}

std::string locationNmeaDiagnostic(){std::string out="quality="+std::to_string(nmea.quality)+" hdop_x100="+std::to_string(nmea.hdopHundredths)+" nav_valid="+std::to_string(nmea.fixKnown&&nmea.positionTrusted(millis()));for(auto &v:nmea.views)if(v.valid){out+="\n"+std::string(v.talker)+": "+std::to_string(v.count)+" age_ms="+std::to_string(uint32_t(millis()-v.at))+" peak_cn0="+std::to_string(v.peakCno)+" tracked_entries="+std::to_string(v.trackedEntries);out+="\n"+std::string(v.raw);}return out;}
bool locationUbxPoll(uint8_t cls,uint8_t id,const uint8_t *payload,uint16_t size){return state.enabled&&state.available&&probe.poll(cls,id,payload,size);}
std::string locationUbxResult(){return probe.rawDiagnostic(millis());}
bool locationProbe(){return state.enabled&&state.available&&probe.start();}
std::string locationReceiverDiagnostic(){return probe.diagnostic(millis())+"\nassist="+(assisted?assistSent:std::string("pending"));}
void locationStore(LocationStore s){store=s;}
bool locationBackupNow(){if(!state.enabled||!state.available||!store.save||!backup.idle()||probe.busy())return false;return backup.startDump(millis(),clockEpoch(),sendUbx);}
std::string locationBackupDiagnostic(){return "store="+std::to_string(store.save!=nullptr)+" "+backup.diagnostic()+" saves="+std::to_string(saves)+" save_failures="+std::to_string(saveFailures)+" saved_age_ms="+(dumped?std::to_string(uint32_t(millis()-lastDumpAt)):std::string("none"))+" fix_seen="+std::to_string(fixSeen)+" off_pending="+std::to_string(offPending);}
bool gpsActive(){return state.enabled&&state.available;}
LocationState locationState(){auto view=state;view.fixAgeMs=state.enabled&&state.available&&gps.location.isValid()?gps.location.age():UINT32_MAX;view.fix=view.fix&&view.fixAgeMs<15000&&nmea.positionTrusted(millis());return view;}
}
