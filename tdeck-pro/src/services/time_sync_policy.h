#pragma once
#include <stdint.h>
namespace device {
constexpr uint32_t GPS_TIME_RESYNC_MS=60000,GPS_AFTER_SYNC_MS=5000,NETWORK_SYNC_TIMEOUT_MS=30000,TIME_BOOT_GRACE_MS=3000;
// A GPS powered on only for the clock (user setting OFF) is switched off again after this long
// without a valid time: indoors it would otherwise stay on until the battery is flat. A GPS the
// user keeps ON is never touched.
constexpr uint32_t GPS_LEASE_TIMEOUT_MS=300000;
inline bool gpsTimeDue(bool wifiConnected,bool gpsWasSource,uint32_t last,uint32_t now){return !wifiConnected&&(!gpsWasSource||uint32_t(now-last)>=GPS_TIME_RESYNC_MS);}
// Gregorian UTC conversion; never change the process-global TZ while networking runs.
inline uint32_t gpsUtcEpoch(int year,int month,int day,int hour,int minute,int second){
 if(year<2024||year>2099||month<1||month>12||hour<0||hour>23||minute<0||minute>59||second<0||second>59)return 0;
 const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};bool leap=year%4==0&&(year%100!=0||year%400==0);
 if(day<1||day>days[month-1]+(month==2&&leap))return 0;
 int y=year-(month<=2),era=y/400;unsigned yoe=unsigned(y-era*400),mp=unsigned(month+(month>2?-3:9));
 unsigned doy=(153*mp+2)/5+unsigned(day)-1,doe=yoe*365+yoe/4-yoe/100+doy;
 int64_t civilDays=int64_t(era)*146097+doe-719468;
 return uint32_t(civilDays*86400+hour*3600+minute*60+second);
}
}
