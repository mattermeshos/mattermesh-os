#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include "touch_hold.h"
namespace device {
constexpr uint32_t TOUCH_RELEASE_MS=24,TOUCH_POLL_MS=8;
// Drag: a release after a mostly horizontal move (>35 px); only the map uses it.
enum class TouchActionKind { Tap,Swipe,Hold,Drag };
struct TouchAction {TouchActionKind kind=TouchActionKind::Tap;int x=0,y=0,dx=0,dy=0,target=-1;uint32_t scene=0,releasedAt=0;std::string key;};
// Acquisition never dispatches UI actions: safe to feed from the EPD BUSY callback.
class TouchPipeline {
 static constexpr int Capacity=8;TouchAction queue[Capacity];int head=0,count=0;
 bool active=false,up=false,consumed=false,blocked=false,moved=false;int sx=0,sy=0,px=0,py=0,target=-1;
 uint32_t began=0,released=0,scene=0;std::string key;
 void emit(TouchActionKind kind,uint32_t at){TouchAction action;action.kind=kind;action.x=px;action.y=py;action.dx=px-sx;action.dy=py-sy;action.target=target;action.scene=scene;action.releasedAt=at;action.key=key;if(count==Capacity){dropped++;return;}queue[(head+count)%Capacity]=action;count++;queued++;}
public:
 uint32_t queued=0,dropped=0,cancelled=0;
 bool needsTarget(bool down,bool allowStart=true)const{return down&&!active&&!blocked&&allowStart;}
 bool contactActive()const{return active||blocked;}
 int size()const{return count;}
 void reset(){active=up=consumed=blocked=moved=false;head=count=0;key.clear();}
 void waitForRelease(){reset();blocked=true;}
 void feed(bool down,int x,int y,uint32_t now,uint32_t currentScene,int channel=-1,const std::string &identity="",bool allowStart=true){
  if(blocked){if(!down){if(!up){up=true;released=now;}if(uint32_t(now-released)>=TOUCH_RELEASE_MS){blocked=up=false;}}else up=false;return;}
  if(active&&scene!=currentScene){cancelled++;active=false;blocked=down;up=false;key.clear();return;}
  if(down){
   if(!active){if(!allowStart){blocked=true;up=false;return;}active=true;consumed=moved=false;sx=px=x;sy=py=y;began=now;scene=currentScene;target=channel;key=identity;}
   up=false;px=x;py=y;
   if(abs(x-sx)>CHANNEL_HOLD_SLOP||abs(y-sy)>CHANNEL_HOLD_SLOP){target=-1;moved=true;key.clear();}
   if(!consumed&&target>=0&&uint32_t(now-began)>=CHANNEL_HOLD_MS){emit(TouchActionKind::Hold,now);consumed=true;}
  }else if(active){
   if(!up){up=true;released=now;}
   if(uint32_t(now-released)>=TOUCH_RELEASE_MS){if(!consumed){if(abs(py-sy)>35&&abs(py-sy)>=abs(px-sx))emit(TouchActionKind::Swipe,released);else if(!moved)emit(TouchActionKind::Tap,released);else if(abs(px-sx)>35)emit(TouchActionKind::Drag,released);else cancelled++;}active=up=false;key.clear();}
  }
 }
 bool pop(TouchAction &action){if(!count)return false;action=queue[head];queue[head]=TouchAction();head=(head+1)%Capacity;count--;return true;}
};
}
