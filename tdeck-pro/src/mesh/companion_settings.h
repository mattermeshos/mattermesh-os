#pragma once
#include <string>
#include "flood_policy.h"
namespace matter {
constexpr unsigned COMPANION_NAME_MAX_BYTES=31;
inline bool validCompanionName(const std::string &name){
 if(name.empty()||name.size()>COMPANION_NAME_MAX_BYTES)return false;
 bool visible=false;for(unsigned char c:name){if(c<32||c==127)return false;if(c!=' ')visible=true;}return visible;
}
// Six-column radio.conf files predate this preference and retain the 2-byte default.
inline bool parseSavedPathBytes(const char *field,int &bytes){
 if(!field||!*field){bytes=OUTGOING_PATH_HASH_BYTES;return true;}
 if(field[0]<'1'||field[0]>'3'||field[1])return false;bytes=field[0]-'0';return true;
}
}
