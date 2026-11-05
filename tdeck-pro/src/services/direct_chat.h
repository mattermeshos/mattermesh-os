#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include "services/hex_util.h"

// New direct chat: contact picker search and the learned DM path line.
namespace mm {
// Case-insensitive ASCII substring match; an empty query matches everything.
inline bool contactMatches(const std::string &name,const std::string &query){
 if(query.empty())return true;auto lower=[](char c){return c>='A'&&c<='Z'?char(c-'A'+'a'):c;};
 for(size_t i=0;i+query.size()<=name.size();i++){size_t j=0;while(j<query.size()&&lower(name[i+j])==lower(query[j]))j++;if(j==query.size())return true;}return false;
}
// MeshCore out_path: length byte = hops in the low 6 bits, (hash bytes - 1) in the top 2; 0xFF = not learned (flood).
constexpr uint8_t DirectPathUnknown=0xFF;
inline int directPathHops(uint8_t encoded){return encoded==DirectPathUnknown?-1:encoded&63;}
inline std::string directPathHashes(uint8_t encoded,const uint8_t *bytes){
 if(encoded==DirectPathUnknown)return "";int hops=encoded&63,width=(encoded>>6)+1;
 return hexString(bytes,size_t(hops*width),false,'>',size_t(width));
}
// Line under a direct chat (where "First RPT" appears): the private route MeshCore learned and keeps.
inline std::string directPathLine(uint8_t encoded,const uint8_t *bytes){
 int hops=directPathHops(encoded);if(hops<0)return "DM path: flood (not learned yet)";if(hops==0)return "DM path: direct, 0 hops";
 return "DM path: "+std::to_string(hops)+(hops==1?" hop ":" hops ")+directPathHashes(encoded,bytes);
}
}
