#include <cassert>
#include <iostream>
#include "mesh/flood_policy.h"
#include "mesh/companion_settings.h"
struct Packet {};
struct Mesh {
 int calls=0;bool scoped=false;uint32_t delay=0;uint8_t width=0;
 void sendFlood(Packet*,uint32_t d,uint8_t w){calls++;delay=d;width=w;scoped=false;}
 void sendFlood(Packet*,uint16_t*,uint32_t d,uint8_t w){calls++;delay=d;width=w;scoped=true;}
};
int main(){Mesh mesh;Packet packet;uint16_t scope[2]={123,0};assert(matter::outgoingPathHashBytes()==2);
 for(int bytes=1;bytes<=3;bytes++){assert(matter::setOutgoingPathHashBytes(bytes));matter::sendMatterFlood(mesh,&packet,42);assert(!mesh.scoped&&mesh.delay==42&&mesh.width==bytes);matter::sendMatterFlood(mesh,&packet,17,scope);assert(mesh.scoped&&mesh.delay==17&&mesh.width==bytes);}
 assert(!matter::setOutgoingPathHashBytes(0)&&!matter::setOutgoingPathHashBytes(4)&&matter::outgoingPathHashBytes()==3);matter::setOutgoingPathHashBytes(2);
 int bytes;assert(matter::parseSavedPathBytes("",bytes)&&bytes==2);for(auto text:{"1","2","3"})assert(matter::parseSavedPathBytes(text,bytes)&&bytes==text[0]-'0');for(auto text:{"0","4","2junk","-1","1.0"})assert(!matter::parseSavedPathBytes(text,bytes));
 assert(matter::validCompanionName("MatterMesh_Ridge")&&matter::validCompanionName(std::string(31,'A')));for(auto name:{"","   ","Name\trow","Name\nrow"})assert(!matter::validCompanionName(name));assert(!matter::validCompanionName(std::string(32,'A')));
 std::cout<<"PASS: 1/2/3-byte native flood overloads preserve scope/delay; invalid widths rejected; legacy file defaults to 2; companion name bounds/control validation\n";
}
