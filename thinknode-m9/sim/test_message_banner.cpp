#include <cassert>
#include <iostream>
#include "services/message_banner.h"
int main(){
 device::MessageBanner b;
 assert(!b.receive(1,"River","Visible",100,false,true,false));
 assert(!b.receive(1,"River","Locked",100,true,false,false));
 assert(!b.receive(1,"River","Muted",100,false,false,true));
 assert(!b.active);
 assert(b.receive(2,"River","New message",100,false,false,false));
 assert(b.chat==2&&b.active&&!b.expire(4099));
 assert(b.receive(3,"Scout","Latest",4000,false,false,false));
 assert(b.sender=="Scout"&&b.body=="Latest"&&!b.expire(7999));
 assert(b.expire(8000)&&!b.active);
 assert(b.receive(0,"River","Rollover",0xfffffff0,false,false,false));
 assert(!b.expire(20));assert(b.expire(5000));
 std::cout<<"PASS: nonmodal incoming banner, visible/muted/locked exclusion, latest replaces, timeout and rollover\n";
}
