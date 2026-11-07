#include <cassert>
#include <string>
#include <cstdio>
#include <iostream>
#include <Arduino.h>
#include <WiFi.h>
#include "board/board.h"
#include "services/location.h"
#include "services/time_sync.h"
#include "services/time_sync_policy.h"
static time_t applied=0;
extern "C" int __wrap_settimeofday(const timeval *tv,const struct timezone*){applied=tv->tv_sec;return 0;}
std::string sentence(std::string text){unsigned crc=0;for(char c:text)crc^=uint8_t(c);char end[8];snprintf(end,sizeof(end),"*%02X\r\n",crc);return "$"+text+end;}
int main(){using namespace device;locationBegin();assert(locationEnabled(false));fakeWiFiStatus=0;timeSyncBegin();fakeMillis+=TIME_BOOT_GRACE_MS;timeSyncLoop();assert(locationState().enabled&&locationState().timeLease);
 SerialGPS.inject(sentence("GPGGA,014600.00,,,,,0,00,99.9,,,,,,")+sentence("GPGSV,1,1,07,01,40,100,30")+sentence("GPRMC,014600.00,V,0000.0000,N,00000.0000,E,0.0,0.0,161026,,,N"));locationLoop();
 assert(!locationState().fix&&locationState().timeValid&&locationState().satellitesVisible==7&&locationState().nmeaValid==3);assert(applied==gpsUtcEpoch(2026,10,16,1,46,0)&&timeSyncState().source==TimeSource::GPS);fakeMillis+=4999;timeSyncLoop();assert(locationState().enabled);fakeMillis++;timeSyncLoop();assert(!locationState().enabled&&!locationState().userEnabled);
 assert(locationEnabled(true));timeSyncRequest();timeSyncLoop();SerialGPS.inject(sentence("GPRMC,014610.00,V,0000.0000,N,00000.0000,E,0.0,0.0,161026,,,N"));locationLoop();assert(applied==gpsUtcEpoch(2026,10,16,1,46,10));fakeMillis+=5000;timeSyncLoop();assert(locationState().enabled&&locationState().userEnabled&&!locationState().timeLease);
 auto previous=applied;fakeMillis+=3000;auto bad=sentence("GPRMC,235959.00,V,0000.0000,N,00000.0000,E,0.0,0.0,311229,,,N");bad[4]='X';SerialGPS.inject(bad);locationLoop();assert(applied==previous&&!locationState().timeValid&&locationState().nmeaBad==1);
 SerialGPS.inject(sentence("GNRMC,014620.00,A,4424.0000,N,00854.0000,E,0,0,161026,,,A"));locationLoop();assert(locationState().fix);
 fakeMillis+=1000;SerialGPS.inject(sentence("GNRMC,014621.00,V,,,,,,,161026,,,N"));locationLoop();assert(!locationState().fix);
 std::cout<<"PASS: real TinyGPS++ parser + location + time service; checksummed RMC V clock before position, real GSV count, bad checksum rejected,5s power restore, originalON retained\n";
}
