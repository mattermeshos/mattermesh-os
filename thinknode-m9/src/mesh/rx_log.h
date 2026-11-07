#pragma once
#include "services/hex_util.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
// RX packets: ring buffer of the last received LoRa packets (header, path,
// signal, first payload bytes). Pure C++ so the host tests can use it; the
// firmware places the slots in PSRAM (service.cpp).
namespace matter {
constexpr int RX_LOG_CAPACITY=500,RX_LOG_PATH_BYTES=64,RX_LOG_PAYLOAD_HEAD=16;
struct RxPacket {
 uint32_t sequence=0,epoch=0,uptimeMs=0;
 uint8_t header=0,pathLen=0,path[RX_LOG_PATH_BYTES]={},payload[RX_LOG_PAYLOAD_HEAD]={},hash[4]={};
 uint16_t length=0,payloadLength=0,transport[2]={};
 int16_t rssiTenths=0,snrTenths=0;
 uint8_t route()const{return header&3;}
 uint8_t type()const{return (header>>2)&15;}
 uint8_t version()const{return header>>6;}
 int hashSize()const{return (pathLen>>6)+1;}
 int hops()const{int n=pathLen&63;return n*hashSize()<=RX_LOG_PATH_BYTES?n:RX_LOG_PATH_BYTES/hashSize();}
 bool flood()const{return route()<=1;}
 bool transportCodes()const{return route()==0||route()==3;}
 float rssi()const{return rssiTenths/10.0f;}
 float snr()const{return snrTenths/10.0f;}
};
class RxLog {
 RxPacket *slots=nullptr;int capacity=0,head=0,size=0;uint32_t next=1;
public:
 void attach(RxPacket *storage,int count){slots=storage;capacity=storage?count:0;head=size=0;}
 bool ready()const{return capacity>0;}
 void push(RxPacket p){p.sequence=next++;if(!capacity)return;slots[head]=p;head=(head+1)%capacity;if(size<capacity)size++;}
 int count()const{return size;}
 uint32_t total()const{return next-1;}
 // 0 = newest.
 const RxPacket *newest(int index)const{if(index<0||index>=size)return nullptr;return &slots[(head-1-index+capacity*2)%capacity];}
 const RxPacket *find(uint32_t sequence)const{for(int i=0;i<size;i++){auto p=newest(i);if(p->sequence==sequence)return p;if(p->sequence<sequence)break;}return nullptr;}
 void clear(){head=size=0;}
};
inline const char *rxRouteName(uint8_t route){static const char *n[]={"T-FLOOD","FLOOD","DIRECT","T-DIRECT"};return n[route&3];}
// MeshCore payload type names (Packet.h PAYLOAD_TYPE_*).
inline const char *rxTypeName(uint8_t type){static const char *n[]={"REQ","RESPONSE","TEXT_MSG","ACK","ADVERT","GRP_TXT","GRP_DATA","ANON_REQ","PATH","TRACE","MULTIPART","CONTROL","TYPE_0C","TYPE_0D","TYPE_0E","RAW_CUSTOM"};return n[type&15];}
inline std::string rxHex(const uint8_t *data,int count){return count>0?mm::hexString(data,size_t(count),true):std::string();}
inline std::string rxHop(const RxPacket &p,int index){return rxHex(p.path+index*p.hashSize(),p.hashSize());}
// "E8D1 > 5027 > ... > 8859": the first `head` and last `tail` hops around an ellipsis.
inline std::string rxPathSummary(const RxPacket &p,int head,int tail){
 int n=p.hops();std::string out;
 auto add=[&](const std::string &part){if(!out.empty())out+=" > ";out+=part;};
 if(head+tail>=n){for(int i=0;i<n;i++)add(rxHop(p,i));return out;}
 for(int i=0;i<head;i++)add(rxHop(p,i));add("...");for(int i=n-tail;i<n;i++)add(rxHop(p,i));return out;
}
}
