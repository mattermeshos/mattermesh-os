#include "time_zone.h"
#include <Preferences.h>
#include <stdlib.h>
#include <time.h>
namespace device {
static int zone=-1;
static int indexOf(int id){for(int i=0;i<TIME_ZONE_COUNT;i++)if(TIME_ZONES[i].id==id)return i;return TIME_ZONE_DEFAULT;}
int timeZone(){
 if(zone<0){int id=TIME_ZONES[TIME_ZONE_DEFAULT].id;Preferences p;if(p.begin("mmtime",true)){id=p.getInt("tz",id);p.end();}zone=indexOf(id);}
 return zone;}
const char *timeZonePosix(){return TIME_ZONES[timeZone()].posix;}
void setTimeZone(int index){
 zone=index<0?0:index>=TIME_ZONE_COUNT?TIME_ZONE_COUNT-1:index;
 Preferences p;if(p.begin("mmtime",false)){p.putInt("tz",TIME_ZONES[zone].id);p.end();}
 setenv("TZ",timeZonePosix(),1);tzset();}
}
