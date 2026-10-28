#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include "services/hex_util.h"

// New-channel forms (Messages menu): create private / join private / join #hashtag.
// Keys are MeshCore channel secrets: 16 bytes (AES-128), shared as 32 hex chars.
namespace mm {
enum class ChannelForm { CreatePrivate=0,JoinPrivate=1,JoinHashtag=2 };
constexpr int ChannelFormCount=3;
constexpr size_t ChannelSecretBytes=16,ChannelSecretHex=ChannelSecretBytes*2,ChannelNameMax=31;

inline std::string channelSecretHex(const uint8_t *bytes){return hexString(bytes,ChannelSecretBytes);}
// "5a1e7c90b3d24f68..." -> "5a1e 7c90 b3d2 4f68" lines of 4 groups, for reading off the screen.
inline std::string channelSecretGroups(const std::string &hex,size_t from,size_t count){std::string out;for(size_t i=from;i<hex.size()&&i<from+count;i++){if(i>from&&(i-from)%4==0)out+=' ';out+=hex[i];}return out;}
// Typed key characters: hex digits only, lower case, at most 32.
inline bool acceptSecretChar(std::string &key,char c){if(key.size()>=ChannelSecretHex)return false;if(c>='A'&&c<='F')c=char(c-'A'+'a');if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;key+=c;return true;}
inline bool validSecret(const std::string &key){if(key.size()!=ChannelSecretHex)return false;for(char c:key)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
inline bool zeroSecret(const std::string &key){for(char c:key)if(c!='0')return false;return true;}
// Hashtag names: lower-case letters, digits and '-' (MeshCore app rule); upper case folds, space becomes '-'.
// The stored name carries the '#', so the typed part is at most 30 bytes.
inline bool acceptHashtagChar(std::string &name,char c){if(name.size()>=ChannelNameMax-1)return false;if(c>='A'&&c<='Z')c=char(c-'A'+'a');if(c==' ')c='-';if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;name+=c;return true;}
inline bool validHashtag(const std::string &name){if(name.empty()||name.size()>ChannelNameMax-1||name.front()=='-'||name.back()=='-')return false;for(char c:name)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;return true;}
// Private names: printable ASCII (no tab), trimmed, at most 31 bytes.
inline std::string cleanChannelName(const std::string &value){size_t a=0,b=value.size();while(a<b&&(unsigned char)value[a]<=32)a++;while(b>a&&(unsigned char)value[b-1]<=32)b--;std::string out;for(size_t i=a;i<b&&out.size()<ChannelNameMax;i++){unsigned char c=value[i];if(c>=32&&c<=126)out+=char(c);}return out;}
}
