#include "location.h"
#include "time_sync.h"
#include "gps_nmea.h"
#include "time_sync_policy.h"
#include <Preferences.h>
#include <TinyGPS++.h>
#include "../board/board.h"
#include <math.h>
#include <time.h>
namespace device {
static GpsNmea nmea;static uint32_t baudAt=0;static Preferences preferences;static TinyGPSPlus gps;static LocationState state;static uint32_t lastSave=0,start=0;static bool alternate=false;
static uint32_t goodSince=0;// start of the current run of good fixes
// GPS enable level: active low on the M9 boards checked in the lab, active high on others (a module that stays silent).
// With no byte at all for GpsSilentMs the other level is tried; the level that brings data is kept in NVS.
static bool enHigh=false,enHighSaved=false;static uint32_t enAt=0;constexpr uint32_t GpsSilentMs=20000;
// ThinkNode M9: a plain NMEA module on Serial2 at 115200 baud, with its own enable line; other modules may come at 9600.
static bool applyEnabled(bool enabled){
 if(!enabled)SerialGPS.end();
 hw::gpsPower(enabled,enHigh);enAt=millis();
 state.enabled=enabled;state.fix=state.data=false;state.satellites=0;state.baud=enabled?BOARD_GPS_BAUD:0;
 gps=TinyGPSPlus();nmea=GpsNmea{};state.nmeaFresh=state.timeValid=false;state.serialBytes=state.nmeaValid=state.nmeaBad=state.satellitesUsed=state.satellitesVisible=0;state.nmeaAgeMs=UINT32_MAX;alternate=false;start=baudAt=millis();
 if(enabled){SerialGPS.setRxBufferSize(2048);SerialGPS.begin(state.baud,SERIAL_8N1,BOARD_GPS_RXD,BOARD_GPS_TXD);}
 state.revision++;return true;
}
void locationBegin(){
 hw::gpsPinsBegin();preferences.begin("mmlocation",false);state.configured=preferences.getBool("valid",false);state.latitude=preferences.getDouble("latitude",0);state.longitude=preferences.getDouble("longitude",0);
 enHigh=enHighSaved=preferences.getBool("gps_en_high",false);
 state.available=true;state.enabled=false;state.timeLease=false;state.userEnabled=preferences.getBool("gps_on",true);
 applyEnabled(state.userEnabled);
}
bool locationTimeLease(bool acquire){
 if(!state.available)return false;bool wanted=state.userEnabled||acquire;
 if(wanted!=state.enabled&&!applyEnabled(wanted)){applyEnabled(state.enabled);return false;}
 state.timeLease=acquire;state.revision++;return true;
}
bool locationEnabled(bool enabled){
 if(!state.available)return false;
 bool old=state.enabled;if(enabled!=old&&!applyEnabled(enabled)){applyEnabled(old);return false;}
 if(preferences.putBool("gps_on",enabled)!=1){applyEnabled(old);return false;}
 state.userEnabled=enabled;state.timeLease=false;state.userRevision++;state.revision++;return true;
}
bool locationToggleUser(){return locationEnabled(!state.userEnabled);}
bool locationSet(double latitude,double longitude){if(!isfinite(latitude)||!isfinite(longitude)||latitude<-90||latitude>90||longitude<-180||longitude>180)return false;if(!state.fix){state.latitude=latitude;state.longitude=longitude;}state.configured=true;preferences.putDouble("latitude",latitude);preferences.putDouble("longitude",longitude);preferences.putBool("valid",true);state.revision++;return true;}
void locationLoop(){
 if(!state.enabled)return;uint32_t now=millis();int budget=1024;
 while(SerialGPS.available()&&budget--){int ch=SerialGPS.read();if(ch<0)break;nmea.feed(char(ch),now);gps.encode(ch);}
 bool oldData=state.data,oldFresh=state.nmeaFresh,oldTime=state.timeValid;auto oldUsed=state.satellitesUsed,oldVisible=state.satellitesVisible;
 state.data=nmea.bytes>0;state.serialBytes=nmea.bytes;state.nmeaValid=nmea.valid;state.nmeaBad=nmea.bad;state.nmeaFresh=nmea.fresh(now);state.nmeaAgeMs=nmea.valid?uint32_t(now-nmea.lastValid):UINT32_MAX;
 state.satellitesUsed=nmea.satellitesUsed(now);state.satellitesVisible=nmea.satellitesVisible(now);state.satellites=state.satellitesVisible?state.satellitesVisible:state.satellitesUsed;
 // Not a single byte for GpsSilentMs: the module may be unpowered, so the enable line takes the other level.
 if(nmea.bytes==0&&uint32_t(now-enAt)>GpsSilentMs){enHigh=!enHigh;hw::gpsPower(true,enHigh);enAt=now;Serial.printf("GPS_EN try active_high=%d\n",enHigh);state.revision++;}
 if(nmea.bytes>0&&enHigh!=enHighSaved&&preferences.putBool("gps_en_high",enHigh)==1){enHighSaved=enHigh;Serial.printf("GPS_EN saved active_high=%d\n",enHigh);}
 // Retry the two rates already supported by this board when no valid NMEA
 // has ever arrived. Noise bytes alone must not freeze baud discovery.
 if(!nmea.valid&&uint32_t(now-baudAt)>5000){SerialGPS.end();alternate=!alternate;state.baud=alternate?9600:BOARD_GPS_BAUD;SerialGPS.begin(state.baud,SERIAL_8N1,BOARD_GPS_RXD,BOARD_GPS_TXD);baudAt=now;state.revision++;}
 bool fix=gps.location.isValid()&&gps.location.age()<15000&&nmea.positionTrusted(now)&&nmea.solutionGood(now);
 if(!fix)goodSince=0;else if(!goodSince)goodSince=now?now:1;if(fix!=state.fix){state.fix=fix;state.revision++;}
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
uint32_t locationRevision(){return state.revision;}
static_assert(SkySatMax==GpsNmea::SkyMax,"sky table");
int locationHdop(){return state.enabled?nmea.hdopHundredths:-1;}
int locationSky(SkySat *out,int max){uint32_t now=millis();int n=0;if(!state.enabled)return 0;for(auto &v:nmea.sky)if(v.prn&&uint32_t(now-v.at)<GpsNmea::SatStaleMs&&n<max){auto &o=out[n++];o.azimuth=v.azimuth;o.elevation=v.elevation;o.used=nmea.inFix(v.system,v.prn,now);}return n;}
LocationState locationState(){auto view=state;view.fixAgeMs=state.enabled&&state.available&&gps.location.isValid()?gps.location.age():UINT32_MAX;view.fix=view.fix&&view.fixAgeMs<15000&&nmea.positionTrusted(millis());return view;}
}
