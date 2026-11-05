#pragma once
#include <string>
#include <stdint.h>
namespace mm {
inline bool utf8Continuation(unsigned char c){return (c&0xc0)==0x80;}
// Start of the code point that ends just before `at` (at>0); works on std::string, Arduino String or char*.
template<class S> inline size_t utf8PrevStart(const S &s,size_t at){size_t from=at-1;while(from>0&&utf8Continuation(static_cast<unsigned char>(s[from])))from--;return from;}
// Largest cut <= room that does not split a code point of s.
template<class S> inline size_t utf8Floor(const S &s,size_t room){while(room&&utf8Continuation(static_cast<unsigned char>(s[room])))room--;return room;}
inline size_t utf8Next(const std::string &s,size_t at,uint32_t &cp){
 if(at>=s.size()){cp=0;return at;}unsigned char c=s[at];if(c<128){cp=c;return at+1;}
 int n=c>=0xf0&&c<=0xf4?4:c>=0xe0&&c<0xf0?3:c>=0xc2&&c<0xe0?2:0;
 if(!n||at+n>s.size()){cp='?';return at+1;}cp=c&((1u<<(7-n))-1);
 for(int i=1;i<n;i++){unsigned char b=s[at+i];if((b&0xc0)!=0x80){cp='?';return at+1;}cp=(cp<<6)|(b&63);}
 if((n==2&&cp<128)||(n==3&&cp<2048)||(n==4&&cp<65536)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)){cp='?';return at+1;}return at+n;
}
inline bool invisibleCodepoint(uint32_t cp){return cp==0x200d||cp==0xfe0e||cp==0xfe0f||(cp>=0x1f3fb&&cp<=0x1f3ff);}
// Regional-indicator flags currently render as separate oversized letter glyphs.
// Omit them only from sender labels; protocol, journal and message bodies stay intact.
inline std::string senderDisplayName(const std::string &value){
 std::string out;uint32_t cp;
 for(size_t at=0;at<value.size();){size_t next=utf8Next(value,at,cp);if(cp<0x1f1e6||cp>0x1f1ff)out+=value.substr(at,next-at);at=next;}
 while(!out.empty()&&out.back()==' ')out.pop_back();while(!out.empty()&&out.front()==' ')out.erase(out.begin());return out;
}
inline std::string mentionDisplayText(const std::string &value){
 std::string out;size_t at=0;
 while(at<value.size()){size_t start=value.find("@[",at);if(start==std::string::npos){out+=value.substr(at);break;}out+=value.substr(at,start-at)+"@[";size_t end=value.find(']',start+2);if(end==std::string::npos){out+=senderDisplayName(value.substr(start+2));break;}out+=senderDisplayName(value.substr(start+2,end-start-2))+']';at=end+1;}
 return out;
}
}
