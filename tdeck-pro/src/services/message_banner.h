#pragma once
#include <stdint.h>
#include <string>
namespace device {
constexpr uint32_t MESSAGE_BANNER_MS=4000;
struct MessageBanner {
 int chat=-1;uint32_t since=0;bool active=false;std::string sender,body;
 bool receive(int target,const std::string &name,const std::string &text,uint32_t now,bool locked,bool visible,bool muted){
  if(target<0||locked||visible||muted)return false;
  chat=target;sender=name;body=text;since=now;active=true;return true;
 }
 bool expire(uint32_t now){if(!active||uint32_t(now-since)<MESSAGE_BANNER_MS)return false;clear();return true;}
 void clear(){active=false;chat=-1;sender.clear();body.clear();}
};
}
