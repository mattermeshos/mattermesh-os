#include "services/home_data.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
 // Altitude: first sample at once, then one per step; climb counts rises of 10 m or more from the last low.
 mm_home::AltitudeTrack t;assert(t.add(0,1000,0)&&!t.add(1000,1500,0));
 int path[]={1004,1020,1015,1040,1030,1100};uint32_t now=0;for(int a:path){now+=mm_home::AltitudeStepMs;assert(t.add(now,a,0));}
 assert(t.count==7&&t.at(0)==1000&&t.at(6)==1100&&t.climb==20+25+70);
 for(int i=0;i<100;i++){now+=mm_home::AltitudeStepMs;t.add(now,2000+i,0);}assert(t.count==mm_home::AltitudeSamples&&t.at(mm_home::AltitudeSamples-1)==2099);
 // The climb follows the window: 12 h on flat ground after a climb read 0.
 for(int i=0;i<mm_home::AltitudeSamples;i++){now+=mm_home::AltitudeStepMs;t.add(now,2500,0);}assert(t.climb==0);
 // Packets: deltas of the running total land in the current quarter; old quarters scroll out; a radio restart is 0.
 mm_home::PacketHistory p;p.update(0,50);p.update(1000,57);assert(p.at(23)==7);
 p.update(mm_home::PacketBucketMs+5,60);assert(p.at(22)==7&&p.at(23)==3&&p.lastHour()==10);
 p.update(mm_home::PacketBucketMs*40,61);assert(p.at(23)==1&&p.lastHour()==1);p.update(mm_home::PacketBucketMs*40+1,2);assert(p.at(23)==3);// restart: the 2 packets since it count
 // UTM: Cervinia 45.9213 N 7.6312 E is 32T 393xxx 5086xxx; Sydney 33.8688 S 151.2093 E is 56H 334368 6250948.
 auto u=mm_home::utm(45.9213,7.6312);assert(u.rfind("32T 3",0)==0&&u.find(" 5086")!=std::string::npos);
 assert(mm_home::utm(-33.8688,151.2093)=="56H 334369 6250948"||mm_home::utm(-33.8688,151.2093)=="56H 334368 6250948");
 assert(mm_home::utm(85,0).empty()&&mm_home::utm(NAN,0).empty());
 assert(mm_home::spoken(45.9213,true)=="four five point nine two one three north"&&mm_home::spoken(-7.05,false)=="seven point zero five zero zero west");
 std::printf("PASS: home data - altitude track and climb, packets per quarter hour (scroll, restart), UTM, spoken coordinates\n");
}
