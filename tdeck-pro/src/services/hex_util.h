#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
// Shared hex helpers: lower case unless asked, an optional separator every `group` bytes; either case accepted on input.
namespace mm {
inline char hexDigit(uint8_t v,bool upper=false){return (upper?"0123456789ABCDEF":"0123456789abcdef")[v&15];}
inline int hexNibble(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
inline void appendHex(std::string &out,const uint8_t *bytes,size_t n,bool upper=false,char separator=0,size_t group=1){
 out.reserve(out.size()+n*3);
 for(size_t i=0;i<n;i++){if(separator&&i&&i%group==0)out+=separator;out+=hexDigit(bytes[i]>>4,upper);out+=hexDigit(bytes[i],upper);}
}
inline std::string hexString(const uint8_t *bytes,size_t n,bool upper=false,char separator=0,size_t group=1){std::string out;appendHex(out,bytes,n,upper,separator,group);return out;}
// Exactly n bytes from exactly 2n hex digits.
inline bool parseHex(const char *text,size_t length,uint8_t *out,size_t n){
 if(length!=n*2)return false;
 for(size_t i=0;i<n;i++){int hi=hexNibble(text[2*i]),lo=hexNibble(text[2*i+1]);if(hi<0||lo<0)return false;out[i]=uint8_t(hi<<4|lo);}
 return true;
}
}
