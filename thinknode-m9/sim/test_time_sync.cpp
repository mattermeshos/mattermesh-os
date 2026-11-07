#include <cassert>
#include <iostream>
#include "services/time_sync_policy.h"
int main(){
 using namespace device;
 assert(gpsTimeDue(false,false,0,100)); // First fix has no initial 60-second delay.
 assert(!gpsTimeDue(true,false,0,100)); // Connected Wi-Fi skips GPS time.
 assert(!gpsTimeDue(false,true,100,59999));
 assert(gpsTimeDue(false,true,100,60100));
 assert(gpsTimeDue(false,true,0xffffff00u,60000));
 assert(gpsUtcEpoch(2024,1,1,0,0,0)==1704067200);
 assert(gpsUtcEpoch(2024,2,29,0,0,0)==1709164800);
 assert(gpsUtcEpoch(2026,2,29,0,0,0)==0);
 assert(gpsUtcEpoch(2026,0,1,0,0,0)==0);
 assert(gpsUtcEpoch(2026,10,14,24,0,0)==0);
 assert(gpsUtcEpoch(2026,10,14,12,34,56)==1791981296);
 std::cout<<"PASS: immediate first GPS fix, Wi-Fi priority, rate limiting/wrap, Gregorian UTC conversion and invalid dates\n";
}
