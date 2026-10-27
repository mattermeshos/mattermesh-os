#pragma once
#include <Arduino.h>
#include <string>
#include <vector>
#include <ExtensionIOXL9555.hpp>
namespace device {
struct LocationState {bool nmeaFresh=false,timeValid=false;uint32_t serialBytes=0,nmeaValid=0,nmeaBad=0,nmeaAgeMs=UINT32_MAX,satellitesUsed=0,satellitesVisible=0;bool configured=false,fix=false,data=false,enabled=false,available=false,userEnabled=false,timeLease=false;uint32_t userRevision=0;double latitude=0,longitude=0,altitude=0;uint32_t satellites=0,revision=0,baud=0,fixAgeMs=UINT32_MAX;bool injected=false;/* "gps inject" test position */};
std::string locationNmeaDiagnostic();
bool locationProbe();bool locationUbxPoll(uint8_t cls,uint8_t id,const uint8_t *payload,uint16_t size);std::string locationUbxResult();std::string locationReceiverDiagnostic();
void locationBegin(ExtensionIOXL9555 &io,bool ready);void locationLoop();LocationState locationState();int locationHdop();/* GGA HDOP in hundredths, -1 unknown */bool gpsActive();/* enabled and module answering, no state copy */bool locationSet(double latitude,double longitude);
// Tests ("gps inject"): NMEA lines from USB instead of the receiver (GPS on).
bool locationInject(bool on);bool locationInjectLine(const char *line);
bool locationEnabled(bool enabled);bool locationToggleUser();bool locationTimeLease(bool acquire);int locationPowerLevel();
// Host-side navigation database store (gps_backup.h): load/save the whole blob, e.g. on the SD card.
struct LocationStore {bool (*load)(std::vector<uint8_t>&)=nullptr;bool (*save)(const std::vector<uint8_t>&)=nullptr;};
void locationStore(LocationStore store);bool locationBackupNow();std::string locationBackupDiagnostic();
}
