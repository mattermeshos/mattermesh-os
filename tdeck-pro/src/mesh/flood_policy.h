#pragma once
#include <stdint.h>
namespace matter {
constexpr uint8_t OUTGOING_PATH_HASH_BYTES=2;
inline bool validPathHashBytes(int bytes){return bytes>=1&&bytes<=3;}
inline uint8_t &outgoingPathHashSetting(){static uint8_t bytes=OUTGOING_PATH_HASH_BYTES;return bytes;}
inline uint8_t outgoingPathHashBytes(){return outgoingPathHashSetting();}
inline bool setOutgoingPathHashBytes(int bytes){if(!validPathHashBytes(bytes))return false;outgoingPathHashSetting()=bytes;return true;}
// Keep encryption/payloads untouched. MeshCore encodes the native path width.
template<class Mesh,class Packet>
void sendMatterFlood(Mesh &mesh,Packet *packet,uint32_t delay=0,uint16_t *scopeCodes=nullptr){
 if(scopeCodes)mesh.sendFlood(packet,scopeCodes,delay,outgoingPathHashBytes());
 else mesh.sendFlood(packet,delay,outgoingPathHashBytes());
}
}
