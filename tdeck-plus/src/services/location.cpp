#include "location.h"
#include "time_sync.h"
#include "gps_nmea.h"
#include "gps_probe.h"
#include "gps_assist.h"
#include "gps_backup.h"
#include "time_sync_policy.h"
#include <Preferences.h>
#include <TinyGPS++.h>
#include "../board/board.h"
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
// T-Deck Plus: the MIA-M10Q has no power switch of its own (BOARD_POWERON feeds every peripheral) and the emulated
// GPS_EN pin reaches nothing, so "off" kept it tracking. Off now sends UBX-RXM-PMREQ: software backup until woken by
// UART RX, only the backup domain stays on (with the navigation data, so the next fix is a hot start). It is sent at
// 38400 and 9600 baud (the module may be on either after a cold start). On sends a few bytes first to wake it.
static uint32_t sleepEchoBytes=0;// bytes the module still sent in the 0.5 s after the sleep request (0 = asleep)
static void ubx(uint8_t cls,uint8_t id,const uint8_t *payload,uint16_t length){
 uint8_t head[6]={0xB5,0x62,cls,id,uint8_t(length),uint8_t(length>>8)},a=0,b=0;for(int i=2;i<6;i++){a+=head[i];b+=a;}for(int i=0;i<length;i++){a+=payload[i];b+=a;}
 uint8_t check[2]={a,b};SerialGPS.write(head,6);SerialGPS.write(payload,length);SerialGPS.write(check,2);SerialGPS.flush();}
static void gpsSleep(bool measure){
 static const uint8_t request[16]={0,0,0,0,0,0,0,0,2,0,0,0,8,0,0,0};// version 0, duration 0 (until woken), flags: backup, wake: UART RX
 for(uint32_t baud:{38400u,9600u}){SerialGPS.end();SerialGPS.begin(baud,SERIAL_8N1,BOARD_GPS_RXD,BOARD_GPS_TXD);delay(20);ubx(0x02,0x41,request,sizeof(request));}
 if(measure){delay(300);while(SerialGPS.available())SerialGPS.read();sleepEchoBytes=0;uint32_t until=millis()+500;while(int32_t(until-millis())>0){while(SerialGPS.available()){SerialGPS.read();sleepEchoBytes++;}delay(10);}
  Serial.printf("GPS_SLEEP bytes_after=%lu\n",(unsigned long)sleepEchoBytes);}
 SerialGPS.end();}
static bool applyEnabled(bool enabled){
 probe.cancel();
 if(!enabled)gpsSleep(state.enabled);
 state.enabled=enabled;state.fix=state.data=false;state.satellites=0;state.baud=enabled?38400:0;
 gps=TinyGPSPlus();nmea=GpsNmea{};state.nmeaFresh=state.timeValid=false;state.serialBytes=state.nmeaValid=state.nmeaBad=state.satellitesUsed=state.satellitesVisible=0;state.nmeaAgeMs=UINT32_MAX;alternate=assisted=false;start=baudAt=millis();
 backup=GpsBackup();fixSeen=offPending=false;fixFirstAt=0;
 if(enabled){SerialGPS.setRxBufferSize(2048);SerialGPS.setTxBufferSize(1024);SerialGPS.begin(state.baud,SERIAL_8N1,BOARD_GPS_RXD,BOARD_GPS_TXD);
  static const uint8_t wake[8]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};SerialGPS.write(wake,sizeof(wake));SerialGPS.flush();// RX edge: out of backup
 }
 state.revision++;return true;
}
void locationBegin(){
 preferences.begin("mmlocation",false);state.configured=preferences.getBool("valid",false);state.latitude=preferences.getDouble("latitude",0);state.longitude=preferences.getDouble("longitude",0);
 state.available=true;state.enabled=false;state.timeLease=false;state.userEnabled=preferences.getBool("gps_on",true);
 applyEnabled(state.userEnabled);
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
  state.latitude=lat;state.longitude=lon;state.configured=true;if(gps.altitude.isValid()){state.altitude=gps.altitude.meters();state.altitudeValid=true;}if(changed)state.revision++;
  // The saved position (map start, weather without a fix, GPS aiding) only takes a fix held for 30 s.
  // NVS keeps the last good position for the next boot: rewritten only after ~100 m (0.001 deg), not every minute.
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
uint32_t locationRevision(){return state.revision;}
static_assert(SkySatMax==GpsNmea::SkyMax,"sky table");
int locationHdop(){return state.enabled?nmea.hdopHundredths:-1;}
int locationSky(SkySat *out,int max){uint32_t now=millis();int n=0;if(!state.enabled)return 0;for(auto &v:nmea.sky)if(v.prn&&uint32_t(now-v.at)<GpsNmea::SatStaleMs&&n<max){auto &o=out[n++];o.azimuth=v.azimuth;o.elevation=v.elevation;o.used=nmea.inFix(v.system,v.prn,now);}return n;}
LocationState locationState(){auto view=state;view.fixAgeMs=state.enabled&&state.available&&gps.location.isValid()?gps.location.age():UINT32_MAX;view.fix=view.fix&&view.fixAgeMs<15000&&nmea.positionTrusted(millis());return view;}
}
