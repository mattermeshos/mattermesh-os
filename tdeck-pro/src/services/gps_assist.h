#pragma once
#include "clock_limits.h"
#include <stdint.h>
#include <stddef.h>
#include <math.h>
namespace device {
// u-blox M10 start-up aiding, sent once per GPS power-on at the detected baud.
// RAM/BBR only (the MIA-M10Q backup cell keeps BBR): no flash or OTP writes.
// AssistNow Autonomous extends broadcast ephemerides for days; MGA-INI time
// and the last saved position narrow the search after the backup data aged.
struct UbxFrame {uint8_t bytes[40]={};size_t size=0;};
constexpr uint32_t GPS_ASSIST_MIN_EPOCH=MM_EPOCH_MIN,GPS_ASSIST_MAX_EPOCH=MM_EPOCH_END;
constexpr uint16_t GPS_ASSIST_TIME_ACCURACY_S=30;
constexpr uint32_t GPS_ASSIST_POSITION_ACCURACY_CM=10000000; // 100 km: the device may have traveled
inline UbxFrame ubxFrame(uint8_t cls,uint8_t id,const uint8_t *payload,uint16_t size){
 UbxFrame f;if(size+8u>sizeof(f.bytes))return f;uint8_t *b=f.bytes;b[0]=0xb5;b[1]=0x62;b[2]=cls;b[3]=id;b[4]=uint8_t(size);b[5]=uint8_t(size>>8);
 for(uint16_t i=0;i<size;i++)b[6+i]=payload[i];uint8_t x=0,y=0;for(unsigned i=2;i<6u+size;i++){x+=b[i];y+=x;}b[6+size]=x;b[7+size]=y;f.size=8+size;return f;
}
inline void ubxPut32(uint8_t *p,uint32_t v){p[0]=uint8_t(v);p[1]=uint8_t(v>>8);p[2]=uint8_t(v>>16);p[3]=uint8_t(v>>24);}
// CFG-VALSET v0, layers RAM|BBR, CFG-ANA-USE_ANA (0x10230001) = 1.
inline UbxFrame ubxEnableAssistAutonomous(){const uint8_t p[]={0x00,0x03,0x00,0x00,0x01,0x00,0x23,0x10,0x01};return ubxFrame(0x06,0x8a,p,sizeof(p));}
// CFG-VALSET v0, layer RAM, CFG-NAVSPG-ACKAIDING (0x10110025) = 1: every MGA input is answered by
// MGA-ACK, the flow control the navigation database replay (gps_backup.h) relies on.
inline UbxFrame ubxEnableAckAiding(){const uint8_t p[]={0x00,0x01,0x00,0x00,0x25,0x00,0x11,0x10,0x01};return ubxFrame(0x06,0x8a,p,sizeof(p));}
// MGA-INI-TIME_UTC: no external reference, leap seconds unknown (-128).
inline UbxFrame ubxTimeUtc(uint32_t epoch,uint16_t accuracySeconds=GPS_ASSIST_TIME_ACCURACY_S){
 if(epoch<GPS_ASSIST_MIN_EPOCH||epoch>=GPS_ASSIST_MAX_EPOCH)return {};
 uint32_t days=epoch/86400,second=epoch%86400;int64_t z=int64_t(days)+719468;int64_t era=z/146097;unsigned doe=unsigned(z-era*146097);
 unsigned yoe=(doe-doe/1460+doe/36524-doe/146096)/365;unsigned doy=doe-(365*yoe+yoe/4-yoe/100),mp=(5*doy+2)/153;
 unsigned day=doy-(153*mp+2)/5+1,month=mp<10?mp+3:mp-9;int year=int(yoe)+int(era)*400+(month<=2);
 uint8_t p[24]={0x10,0x00,0x00,0x80};p[4]=uint8_t(year);p[5]=uint8_t(year>>8);p[6]=uint8_t(month);p[7]=uint8_t(day);
 p[8]=uint8_t(second/3600);p[9]=uint8_t(second/60%60);p[10]=uint8_t(second%60);p[16]=uint8_t(accuracySeconds);p[17]=uint8_t(accuracySeconds>>8);
 return ubxFrame(0x13,0x40,p,sizeof(p));
}
// MGA-INI-POS_LLH: degrees * 1e7, altitude unknown (0 cm), accuracy in cm.
inline UbxFrame ubxPositionLlh(double latitude,double longitude,uint32_t accuracyCm=GPS_ASSIST_POSITION_ACCURACY_CM){
 if(!isfinite(latitude)||!isfinite(longitude)||latitude<-90||latitude>90||longitude<-180||longitude>180||(latitude==0&&longitude==0))return {};
 uint8_t p[20]={0x01,0x00};ubxPut32(p+4,uint32_t(int32_t(lround(latitude*1e7))));ubxPut32(p+8,uint32_t(int32_t(lround(longitude*1e7))));ubxPut32(p+16,accuracyCm);
 return ubxFrame(0x13,0x40,p,sizeof(p));
}
}
