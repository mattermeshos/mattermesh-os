#include <cassert>
#include <iostream>
#include "services/first_repeater.h"
int main(){
 device::FirstRepeater rpt;rpt.sent(42,1);assert(!rpt.visible(1,0));assert(!rpt.observe(41,"abcd",100));assert(!rpt.observe(42,"",100));assert(rpt.observe(42,"abcd",100));assert(rpt.visible(1,6099)&&!rpt.visible(1,6100)&&!rpt.visible(0,100));assert(!rpt.observe(42,"1122",200)&&rpt.hash=="abcd");
 rpt.sent(43,2);assert(!rpt.visible(1,101)&&!rpt.observed);assert(rpt.observe(43,"ab",0xfffffff0));assert(rpt.visible(2,0xfffffff0u+5999u)&&!rpt.visible(2,0xfffffff0u+6000u));
 std::cout<<"PASS: first observed repeater matched to latest send/chat, no invented route, exact 6s expiry\n";
}
