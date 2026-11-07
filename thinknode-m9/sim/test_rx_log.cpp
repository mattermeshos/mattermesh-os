#include <cassert>
#include <iostream>
#include "mesh/rx_log.h"
using namespace matter;
static RxPacket packet(uint8_t route,uint8_t type,int hops,int hashSize=2){RxPacket p;p.header=route|(type<<2);p.pathLen=((hashSize-1)<<6)|hops;for(int i=0;i<hops*hashSize&&i<RX_LOG_PATH_BYTES;i++)p.path[i]=uint8_t(i+1);return p;}
int main(){
 static RxPacket slots[4];RxLog log;assert(!log.ready()&&!log.newest(0));log.push(packet(1,2,0));assert(log.total()==1&&log.count()==0);
 log.attach(slots,4);for(int i=0;i<6;i++)log.push(packet(i%2?1:2,i%2?2:3,i));
 assert(log.count()==4&&log.total()==7&&log.newest(0)->sequence==7&&log.newest(3)->sequence==4&&!log.newest(4)&&log.find(5)&&!log.find(3));
 auto p=packet(1,2,7);assert(p.hops()==7&&p.hashSize()==2&&rxHop(p,0)=="0102"&&rxHop(p,6)=="0D0E");
 assert(rxPathSummary(p,3,3)=="0102 > 0304 > 0506 > ... > 090A > 0B0C > 0D0E");assert(rxPathSummary(packet(1,2,2),3,3)=="0102 > 0304"&&rxPathSummary(packet(1,2,0),3,3).empty());
 assert(packet(0,2,63,4).hops()==16);assert(std::string(rxRouteName(1))=="FLOOD"&&std::string(rxTypeName(2))=="TEXT_MSG"&&std::string(rxTypeName(3))=="ACK");
 log.clear();assert(log.count()==0&&log.total()==7);log.push(packet(1,2,0));assert(log.newest(0)->sequence==8);
 std::cout<<"PASS: RX log ring order, find, path summary, oversized path clamp, names\n";}
