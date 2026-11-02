// Time zones: unique ids, every POSIX string gives the offsets its label promises, the choice survives a restart by id.
#include <cassert>
#include <cstring>
#include <iostream>
#include <set>
#include <string>
#include <time.h>
#include <Preferences.h>
#include "services/time_zone.h"
using namespace device;
static int offsetMinutes(time_t utc){struct tm l,u;localtime_r(&utc,&l);gmtime_r(&utc,&u);l.tm_isdst=u.tm_isdst=0;return int((timegm(&l)-timegm(&u))/60);}
static int labelMinutes(const char *&p){int sign=*p=='-'?-1:1;p++;int h=0,m=0;while(*p>='0'&&*p<='9')h=h*10+(*p++-'0');if(*p==':'){p++;while(*p>='0'&&*p<='9')m=m*10+(*p++-'0');}return sign*(h*60+m);}
int main(){
 std::set<int> ids;for(auto &z:TIME_ZONES)assert(ids.insert(z.id).second);
 assert(std::string(TIME_ZONES[TIME_ZONE_DEFAULT].posix)=="CET-1CEST,M3.5.0,M10.5.0/3");// Italy keeps its old clock
 const time_t january=1767614400,july=1783339200;// 2026-01-05 and 2026-07-05, 12:00 UTC
 for(int i=0;i<TIME_ZONE_COUNT;i++){setTimeZone(i);assert(timeZone()==i);
  const char *p=TIME_ZONES[i].offset;assert(!strncmp(p,"UTC",3));p+=3;int standard=labelMinutes(p),summer=standard;
  if(!strncmp(p,", summer ",9)){p+=9;summer=labelMinutes(p);}assert(*p==0);
  bool south=std::string(TIME_ZONES[i].posix).find("M10.1.0")!=std::string::npos||std::string(TIME_ZONES[i].posix).find("M9.5.0")!=std::string::npos;
  int jan=offsetMinutes(january),jul=offsetMinutes(july);
  if(south)assert(jan==summer&&jul==standard);else assert(jan==standard&&jul==summer);}
 setTimeZone(17);assert(fakePrefs["tz"]==TIME_ZONES[17].id);
 setTimeZone(-5);assert(timeZone()==0);setTimeZone(999);assert(timeZone()==TIME_ZONE_COUNT-1);
 std::cout<<"PASS: "<<TIME_ZONE_COUNT<<" time zones, unique ids, offsets and summer time as labelled, default Central Europe, saved by id\n";}
