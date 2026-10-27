#pragma once
#include <cmath>
#include <ctime>
// Home widgets: sunrise / sunset (Julian-day sunrise equation, within a few minutes) and the moon phase, offline from the
// clock and a position. Times are UTC epoch seconds; the caller formats them in the local time zone.
namespace mm_sky {
constexpr double Rad=M_PI/180,SynodicDays=29.530588853;
constexpr time_t NewMoonRef=947182440;// 2000-01-06 18:14 UTC
// polar: 0 normal day, 1 the sun never sets, -1 it never rises (no rise / set then).
struct SunTimes {bool valid=false;int polar=0;time_t rise=0,set=0;};
// The solar day nearest `when` at that longitude (east positive).
inline SunTimes sunTimes(time_t when,double lat,double lon){
 SunTimes out;if(!std::isfinite(lat)||!std::isfinite(lon)||lat<-90||lat>90||lon<-180||lon>180)return out;
 double jd=when/86400.0+2440587.5,n=std::round(jd-2451545.0009+lon/360),mean=n+0.0009-lon/360;
 double m=std::fmod(357.5291+0.98560028*mean,360),c=1.9148*std::sin(m*Rad)+0.02*std::sin(2*m*Rad)+0.0003*std::sin(3*m*Rad);
 double lambda=std::fmod(m+c+180+102.9372,360),transit=2451545.0+mean+0.0053*std::sin(m*Rad)-0.0069*std::sin(2*lambda*Rad);
 double sinDecl=std::sin(lambda*Rad)*std::sin(23.44*Rad),cosDecl=std::cos(std::asin(sinDecl));
 double cosHour=(std::sin(-0.833*Rad)-std::sin(lat*Rad)*sinDecl)/(std::cos(lat*Rad)*cosDecl);
 auto epoch=[](double j){return time_t(std::llround((j-2440587.5)*86400));};
 out.valid=true;if(cosHour>1||cosHour<-1){out.polar=cosHour>1?-1:1;return out;}
 double hour=std::acos(cosHour)/Rad;out.rise=epoch(transit-hour/360);out.set=epoch(transit+hour/360);return out;
}
// age in days since the new moon; illumination 0..100; nextFull: the next full moon (mean month, within ~1.5 days).
struct MoonPhase {double age=0;int illumination=0;bool waxing=true;const char *shortName="New";time_t nextFull=0;};
inline MoonPhase moonPhase(time_t when){
 MoonPhase out;double age=std::fmod((double(when)-NewMoonRef)/86400,SynodicDays);if(age<0)age+=SynodicDays;
 out.age=age;out.waxing=age<SynodicDays/2;out.illumination=int(std::lround((1-std::cos(2*M_PI*age/SynodicDays))/2*100));
 static const char *shorts[]={"New","Waxing","First qtr","Waxing","Full","Waning","Last qtr","Waning"};
 int octant=int(std::floor(age/SynodicDays*8+0.5))%8;out.shortName=shorts[octant];
 double toFull=std::fmod(SynodicDays/2-age+SynodicDays,SynodicDays);if(toFull<0.5)toFull+=SynodicDays;// "next" once today's is passing
 out.nextFull=when+time_t(toFull*86400);return out;
}
// ISO 8601 week number of a local date (Monday first, week 1 holds the first Thursday).
inline int isoWeek(const struct tm &t){
 int week=(t.tm_yday+1-((t.tm_wday+6)%7+1)+10)/7,year=t.tm_year+1900;
 auto p=[](int y){return (y+y/4-y/100+y/400)%7;};auto weeks=[&](int y){return p(y)==4||p(y-1)==3?53:52;};
 if(week<1)return weeks(year-1);if(week>weeks(year))return 1;return week;
}
}
