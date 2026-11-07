#include <cassert>
#include <iostream>
#include "services/direct_chat.h"
int main(){
 assert(mm::contactMatches("Aria T-Deck","")&&mm::contactMatches("Aria T-Deck","ari")&&mm::contactMatches("Aria T-Deck","T-DECK")&&mm::contactMatches("ab","ab"));
 assert(!mm::contactMatches("Aria","ariax")&&!mm::contactMatches("","a")&&!mm::contactMatches("Marco","rx"));
 const uint8_t one[]={0xa1,0xb2,0xc3,0xd4,0xe5,0xf6};
 assert(mm::directPathLine(0xFF,one)=="DM path: flood (not learned yet)"&&mm::directPathHops(0xFF)==-1);
 assert(mm::directPathLine(0,one)=="DM path: direct, 0 hops");
 assert(mm::directPathLine(1,one)=="DM path: 1 hop a1");
 assert(mm::directPathLine(3,one)=="DM path: 3 hops a1>b2>c3");
 assert(mm::directPathLine(0x40|3,one)=="DM path: 3 hops a1b2>c3d4>e5f6"&&mm::directPathHops(0x43)==3);
 std::cout<<"PASS: contact search (case-insensitive substring), DM path line flood/direct/1/3 hops/2-byte hashes\n";}
