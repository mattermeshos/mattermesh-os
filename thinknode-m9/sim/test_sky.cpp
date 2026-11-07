#include "services/sky.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
// Reference values: NOAA fractional-year sunrise method and published 2026 moon phases, within a few minutes / 1.5 days.
static time_t utc(int y,int mo,int d,int h,int mi){struct tm t={};t.tm_year=y-1900;t.tm_mon=mo-1;t.tm_mday=d;t.tm_hour=h;t.tm_min=mi;return timegm(&t);}
static void near(time_t got,time_t want,int seconds){if(std::llabs((long long)(got-want))>seconds){std::printf("FAIL: %lld vs %lld\n",(long long)got,(long long)want);std::exit(1);}}
int main(){
 // Cervinia 45.93 N 7.63 E, 2026-10-27: sunrise 06:01 UTC, sunset 16:25 UTC (NOAA fractional-year method, CET 07:01 / 17:25).
 auto s=mm_sky::sunTimes(utc(2026,10,27,9,0),45.93,7.63);assert(s.valid&&!s.polar);near(s.rise,utc(2026,10,27,6,1),240);near(s.set,utc(2026,10,27,16,25),240);
 // Same day asked late in the evening and early in the morning: still that solar day.
 near(mm_sky::sunTimes(utc(2026,10,27,22,30),45.93,7.63).set,s.set,120);near(mm_sky::sunTimes(utc(2026,10,27,0,30),45.93,7.63).rise,s.rise,120);
 // Quito on the equator ~12 h of light; Tromso midsummer midnight sun, midwinter polar night.
 auto q=mm_sky::sunTimes(utc(2026,3,20,12,0),-0.18,-78.47);assert(q.valid&&!q.polar&&std::llabs((long long)(q.set-q.rise)-12*3600)<15*60);
 assert(mm_sky::sunTimes(utc(2026,6,21,12,0),69.65,18.96).polar==1);assert(mm_sky::sunTimes(utc(2026,12,21,12,0),69.65,18.96).polar==-1);
 assert(!mm_sky::sunTimes(0,NAN,7).valid&&!mm_sky::sunTimes(0,91,7).valid&&!mm_sky::sunTimes(0,45,181).valid);
 // Moon: full 2026-10-26 04:11 UTC, new 2026-11-09 07:02 UTC, first quarter 2026-10-18 16:12 UTC.
 auto full=mm_sky::moonPhase(utc(2026,10,26,4,11));assert(full.illumination>=98);
 auto fresh=mm_sky::moonPhase(utc(2026,11,9,7,2));assert(fresh.illumination<=2&&(fresh.age<1||fresh.age>28.5));
 auto quarter=mm_sky::moonPhase(utc(2026,10,18,16,12));assert(quarter.waxing&&quarter.illumination>=40&&quarter.illumination<=60);
 near(mm_sky::moonPhase(utc(2026,10,20,0,0)).nextFull,utc(2026,10,26,4,11),36*3600);
 // ISO weeks: 2026-10-27 week 44; 2027-01-01 (Fri) week 53 of 2026; 2024-12-30 (Mon) week 1; 2026-01-01 (Thu) week 1; 2021-01-03 (Sun) week 53.
 auto week=[](int y,int mo,int d){time_t t=utc(y,mo,d,12,0);struct tm tm;gmtime_r(&t,&tm);return mm_sky::isoWeek(tm);};
 assert(week(2026,10,27)==44&&week(2027,1,1)==53&&week(2024,12,30)==1&&week(2026,1,1)==1&&week(2021,1,3)==53);
 std::printf("PASS: sky - sunrise/sunset (mid latitude, equator, midnight sun, polar night, bad input), moon phase and next full moon, ISO weeks\n");
}
