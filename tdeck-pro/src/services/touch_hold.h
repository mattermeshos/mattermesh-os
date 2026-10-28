#pragma once
#include <stdint.h>
#include <stdlib.h>
namespace device {
constexpr uint32_t CHANNEL_HOLD_MS=700;
constexpr int CHANNEL_HOLD_SLOP=12;
struct TouchHold {
 int target=-1,x=0,y=0;uint32_t started=0;bool cancelled=false,consumed=false;
 void begin(int id,int px,int py,uint32_t now){target=id;x=px;y=py;started=now;cancelled=false;consumed=false;}
 int update(int px,int py,uint32_t now){
  if(abs(px-x)>CHANNEL_HOLD_SLOP||abs(py-y)>CHANNEL_HOLD_SLOP)cancelled=true;
  if(target<0||cancelled||consumed||uint32_t(now-started)<CHANNEL_HOLD_MS)return -1;
  consumed=true;return target;
 }
 void reset(){target=-1;cancelled=consumed=false;}
};
}
