#include <cassert>
#include <string>
#include <cstdio>
#include <iostream>
#include "services/gps_nmea.h"
std::string sentence(std::string text){unsigned checksum=0;for(auto c:text)checksum^=uint8_t(c);char end[8];snprintf(end,sizeof(end),"*%02X\r\n",checksum);return "$"+text+end;}
void feed(device::GpsNmea &gps,const std::string &text,uint32_t now){for(auto c:text)gps.feed(c,now);}
int main(){device::GpsNmea gps;assert(!gps.valid&&!gps.satellitesUsed(0)&&!gps.satellitesVisible(0));
 feed(gps,sentence("GPGGA,013800,,,,,0,03,99.9,,,,,,"),100);assert(gps.valid==1&&gps.satellitesUsed(100)==3&&gps.fresh(100));
 feed(gps,sentence("GPGSV,2,1,08,01,40,100,30"),200);feed(gps,sentence("GPGSV,2,2,08,02,40,100,30"),201);assert(gps.satellitesVisible(201)==8);
 feed(gps,sentence("GLGSV,1,1,04,01,40,100,30"),300);assert(gps.satellitesVisible(300)==12);
 feed(gps,sentence("GNGSV,3,1,18,01,40,100,30"),400);assert(gps.satellitesVisible(400)==18); // real counts above12 retained
 auto bad=sentence("GPGGA,013800,,,,,0,10,99.9,,,,,,");bad[5]='X';feed(gps,bad,500);assert(gps.bad==1&&gps.satellitesUsed(500)==3);
 feed(gps,sentence("GPGGA,013800,,,,,0,,99.9,,,,,,"),600);assert(gps.satellitesUsed(600)==3);assert(!gps.satellitesVisible(16000)&&!gps.satellitesUsed(16000)&&!gps.fresh(16000));
 gps=device::GpsNmea{};feed(gps,sentence("GPGSV,1,1,05"),0xfffffff0);assert(gps.satellitesVisible(100)==5&&gps.fresh(100));feed(gps,"$"+std::string(160,'a')+"\r\n",100);assert(gps.bad);
 // A checksum-valid loss of navigation validity must invalidate a cached fix
 // immediately; TinyGPS++ retains the last position for up to15s otherwise.
 gps=device::GpsNmea{};
 feed(gps,sentence("GNRMC,120000.00,A,4424.0000,N,00854.0000,E,0,0,161026,,,A"),100);
 assert(gps.positionTrusted(100));
 feed(gps,sentence("GNRMC,120001.00,V,,,,,,,161026,,,N"),1100);
 assert(!gps.positionTrusted(1100));
 feed(gps,sentence("GNGGA,120002.00,4424.0000,N,00854.0000,E,1,08,1.20,10.0,M,0,M,,"),2100);
 assert(gps.positionTrusted(2100)&&gps.quality==1&&gps.hdopHundredths==120);
 auto invalid=sentence("GNGGA,120003.00,,,,,0,00,99.99,,,,,,");invalid[8]='X';feed(gps,invalid,3100);assert(gps.positionTrusted(3100));
 feed(gps,sentence("GNGGA,120003.00,,,,,0,00,99.99,,,,,,"),3100);assert(!gps.positionTrusted(3100));
 feed(gps,sentence("GPGSV,2,1,05,01,40,100,32,02,30,110,18,03,20,120,,04,10,130,0,1"),4000);
 feed(gps,sentence("GPGSV,2,2,05,05,05,140,25,1"),4001);
 assert(gps.views[0].peakCno==32&&gps.views[0].trackedEntries==3);
 assert(gps.views[0].raw[0]=='G');
 // Solution quality: estimated (6) or no fix (0) never counts; too few satellites or a poor HDOP neither.
 {device::GpsNmea q;q.quality=1;q.hdopHundredths=120;q.used=7;q.usedValid=true;q.usedAt=5000;assert(q.solutionGood(6000));
  q.quality=6;assert(!q.solutionGood(6000));q.quality=0;assert(!q.solutionGood(6000));q.quality=2;assert(q.solutionGood(6000));
  q.used=3;assert(!q.solutionGood(6000));q.used=5;q.hdopHundredths=640;assert(!q.solutionGood(6000));q.hdopHundredths=-1;assert(q.solutionGood(6000));}
 std::cout<<"PASS: real checksummed NMEA before fix; GGA used/GSV visible; multi-constellation dedup; missing/invalid/stale/overflow/rollover, no12sat cap\n";
}
