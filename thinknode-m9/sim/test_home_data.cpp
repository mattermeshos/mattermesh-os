#include "services/home_data.h"
#include "services/gps_nmea.h"
#include <cassert>
#include <cstring>
#include <cstdio>
static void feed(device::GpsNmea &n,const char *body,uint32_t now){unsigned x=0;for(const char *p=body;*p;p++)x^=uint8_t(*p);char line[160];std::snprintf(line,sizeof line,"$%s*%02X\r\n",body,x);for(const char *p=line;*p;p++)n.feed(*p,now);}
int main(){
 // Altitude: first sample at once, then one per step; climb counts rises of ClimbStep (25 m) or more from the last low,
 // over the stored samples only, and ignores +-10..20 m noise.
 mm_home::AltitudeTrack t;assert(t.add(0,1000,0)&&!t.add(1000,1500,0));
 int path[]={1004,1030,1015,1045,1030,1100};uint32_t now=0;for(int a:path){now+=mm_home::AltitudeStepMs;assert(t.add(now,a,0));}
 assert(t.count==7&&t.at(0)==1000&&t.at(6)==1100&&t.climb()==30+30+70);
 {mm_home::AltitudeTrack noise;uint32_t at=0;for(int i=0;i<72;i++){noise.add(at,i%2?1010:990,0);at+=mm_home::AltitudeStepMs;}assert(noise.climb()==0);}
 for(int i=0;i<100;i++){now+=mm_home::AltitudeStepMs;t.add(now,2000+i,0);}assert(t.count==mm_home::AltitudeSamples&&t.at(mm_home::AltitudeSamples-1)==2099);
 // Packets: deltas of the running total land in the current quarter; old quarters scroll out; a radio restart is 0.
 mm_home::PacketHistory p;p.update(0,50);p.update(1000,57);assert(p.at(23)==7);
 p.update(mm_home::PacketBucketMs+5,60);assert(p.at(22)==7&&p.at(23)==3&&p.lastHour()==10);
 p.update(mm_home::PacketBucketMs*40,61);assert(p.at(23)==1&&p.lastHour()==1);p.update(mm_home::PacketBucketMs*40+1,2);assert(p.at(23)==1);p.update(mm_home::PacketBucketMs*40+2,5);assert(p.at(23)==4);// counting resumes from the new total
 mm_home::PacketHistory r;r.update(0xFFFFFF00u,10);r.update(0xFFFFFF00u+mm_home::PacketBucketMs+100,15);assert(r.at(22)==0&&r.at(23)==5);// millis rollover keeps the history
 // UTM: Cervinia 45.9213 N 7.6312 E is 32T 393xxx 5086xxx; Sydney 33.8688 S 151.2093 E is 56H 334368 6250948 (+-1 m rounding).
 auto u=mm_home::utm(45.9213,7.6312);assert(u.rfind("32T 3",0)==0&&u.find(" 5086")!=std::string::npos);
 assert(mm_home::utm(-33.8688,151.2093)=="56H 334369 6250948"||mm_home::utm(-33.8688,151.2093)=="56H 334368 6250948");
 assert(mm_home::utm(85,0).empty()&&mm_home::utm(NAN,0).empty());
 assert(mm_home::spoken(45.9213,true)=="four five point nine two one three north"&&mm_home::spoken(-7.05,false)=="seven point zero five zero zero west");
 // GSV satellites and the GSA fix set: by constellation (GPS 5 and BeiDou 5 are different satellites); a GSA burst
 // (one per constellation) adds up, the next epoch starts again; entries expire after SatStaleMs.
 device::GpsNmea n;feed(n,"GPGSV,2,1,05,05,70,030,40,12,40,095,35,07,20,150,,19,55,200,38",1000);feed(n,"GPGSV,2,2,05,02,35,250,30",1000);feed(n,"GBGSV,1,1,01,05,30,300,33",1000);
 feed(n,"GNGSA,A,3,05,12,19,,,,,,,,,,1.8,1.2,1.3,1",1000);feed(n,"GNGSA,A,3,,,,,,,,,,,,,1.8,1.2,1.3,4",1040);
 auto used=[&](uint32_t now){int seen=0,in=0;for(auto &s:n.sky)if(s.prn&&uint32_t(now-s.at)<device::GpsNmea::SatStaleMs){seen++;if(n.inFix(s.system,s.prn,now))in++;}return seen*100+in;};
 assert(used(1100)==603);// 6 seen, GPS 5 / 12 / 19 used, BeiDou 5 not
 bool found=false;for(auto &s:n.sky)if(s.prn==12)found=s.system==1&&s.elevation==40&&s.azimuth==95;assert(found);
 feed(n,"GNGGA,120000.00,4555.000,N,00737.000,E,1,07,1.2,1842.0,M,47.0,M,,",1990);feed(n,"GNGSA,A,3,07,,,,,,,,,,,,1.8,1.2,1.3,1",2000);assert(n.inFix(1,7,2000)&&!n.inFix(1,5,2000));// a new epoch (GGA) replaces the set
 feed(n,"GPGSV,1,1,01,07,25,150,30",20000);assert(used(20000)==100);// the rest of the sky expired
 std::printf("PASS: home data - altitude track and climb, packets per quarter hour (scroll, restart, millis rollover), UTM, spoken coordinates, GSV satellites by constellation, GSA epochs, expiry\n");
}
