#pragma once
#include <stdint.h>
#include <string>
namespace device {
constexpr uint32_t LOCK_RX_REPEATER_MS=15000,CHAT_RX_REPEATER_MS=6000;
inline std::string incomingRepeater(const std::string &path){
 if(path.compare(0,6,"flood:"))return "";auto pos=path.find_last_of(',');auto hash=path.substr(pos==std::string::npos?6:pos+1);
 if(hash.size()<2||hash.size()>6||hash.size()%2)return "";
 for(char &c:hash){if(c>='A'&&c<='F')c+=32;if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return "";}return hash;
}
// Flood hashes are source -> receiver: the last hop is the local RX repeater.
struct ReceivedRepeater {std::string hash;uint32_t at=0,token=0;int chat=-1;
 void receive(uint32_t id,int channel,const std::string &path,uint32_t now){token=id;chat=channel;hash=incomingRepeater(path);at=now;}
 bool visible(uint32_t now,uint32_t duration)const{return !hash.empty()&&uint32_t(now-at)<duration;}
};
}
