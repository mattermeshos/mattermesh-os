#pragma once
#include <stdint.h>
#include <string>
namespace device {
constexpr uint32_t FIRST_REPEATER_DISPLAY_MS=6000;
struct FirstRepeater {
 uint32_t token=0,shownAt=0;int chat=-1;bool observed=false;std::string hash;
 void sent(uint32_t id,int conversation){token=id;chat=conversation;observed=false;hash.clear();}
 bool observe(uint32_t id,const std::string &prefix,uint32_t now){if(id!=token||chat<0||observed||prefix.empty())return false;hash=prefix;observed=true;shownAt=now;return true;}
 bool visible(int conversation,uint32_t now)const{return observed&&conversation==chat&&uint32_t(now-shownAt)<FIRST_REPEATER_DISPLAY_MS;}
};
}
