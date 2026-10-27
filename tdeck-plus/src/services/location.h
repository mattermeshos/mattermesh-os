#pragma once
#include <Arduino.h>
#include <string>
#include <vector>
namespace device {
struct LocationState {bool nmeaFresh=false,timeValid=false;uint32_t serialBytes=0,nmeaValid=0,nmeaBad=0,nmeaAgeMs=UINT32_MAX,satellitesUsed=0,satellitesVisible=0;bool configured=false,fix=false,data=false,enabled=false,available=false,userEnabled=false,timeLease=false;uint32_t userRevision=0;double latitude=0,longitude=0,altitude=0;bool altitudeValid=false;/* a GGA altitude came with the fix */uint32_t satellites=0,revision=0,baud=0,fixAgeMs=UINT32_MAX;};
std::string locationNmeaDiagnostic();
bool locationProbe();bool locationUbxPoll(uint8_t cls,uint8_t id,const uint8_t *payload,uint16_t size);std::string locationUbxResult();std::string locationReceiverDiagnostic();
// Home GPS sky: satellites seen within GpsNmea::SatStaleMs; used = in the fix (GSA).
constexpr int SkySatMax=48;// = GpsNmea::SkyMax
struct SkySat {uint16_t azimuth=0;uint8_t elevation=0;bool used=false;};int locationSky(SkySat *out,int max);int locationHdop();// GGA HDOP in hundredths, -1 unknown
void locationBegin();void locationLoop();LocationState locationState();uint32_t locationRevision();bool locationSet(double latitude,double longitude);
bool locationEnabled(bool enabled);bool locationToggleUser();bool locationTimeLease(bool acquire);
// Host-side navigation database store (gps_backup.h): load/save the whole blob, e.g. on the SD card.
struct LocationStore {bool (*load)(std::vector<uint8_t>&)=nullptr;bool (*save)(const std::vector<uint8_t>&)=nullptr;};
void locationStore(LocationStore store);bool locationBackupNow();std::string locationBackupDiagnostic();
}
