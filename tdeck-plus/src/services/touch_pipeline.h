#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include "touch_hold.h"
namespace device {
constexpr uint32_t TOUCH_RELEASE_MS=24,TOUCH_POLL_MS=8;
// Drag: a release after a mostly horizontal move (>35 px); only the map uses it.
// Move (stream mode, T-Deck Plus): emitted while the finger moves, dx/dy since the previous Move; a streamed touch ends silently.
// Fling (stream mode): a streamed touch released while still moving; dx/dy carry the finger velocity in px/s.
enum class TouchActionKind { Tap,Swipe,Hold,Drag,Move,Fling };
struct TouchAction {TouchActionKind kind=TouchActionKind::Tap;int x=0,y=0,dx=0,dy=0,target=-1;uint32_t scene=0,releasedAt=0;std::string key;};
// Acquisition never dispatches UI actions: safe to feed from the EPD BUSY callback.
class TouchPipeline {
 static constexpr int Capacity=8;TouchAction queue[Capacity];int head=0,count=0;
 bool active=false,up=false,consumed=false,blocked=false,moved=false,streamed=false;int sx=0,sy=0,px=0,py=0,lx=0,ly=0,target=-1;uint32_t lastMove=0,holdMs=CHANNEL_HOLD_MS;
 static constexpr int Samples=8;uint32_t st[Samples]={};int sxs[Samples]={},sys[Samples]={};int sn=0;// last contact samples: finger velocity at release
 static constexpr uint32_t FLING_WINDOW_MS=220,FLING_STILL_MS=40,FLING_SPAN_MS=60;static constexpr int FLING_MIN=250,FLING_MAX=3500;// px/s
 // Speed over the samples of the FLING_WINDOW_MS before the newest one (the release itself is seen late by a busy loop). A finger resting FLING_STILL_MS or more before lifting gives 0.
 int flingVelocity(int axis,uint32_t at)const{if(sn<2)return 0;int n=sn<Samples?sn:Samples;int k0=(sn-1)%Samples,kb=-1;
  (void)at;// the release is seen late when the loop is painting: measure back from the newest sample
  for(int i=1;i<n;i++){int k=(sn-1-i)%Samples;if(uint32_t(st[k0]-st[k])>FLING_WINDOW_MS)break;kb=k;if(uint32_t(st[k0]-st[k])>=FLING_SPAN_MS)break;}// the last ~60 ms: the speed at the lift, not the average
  if(kb<0)return 0;
  for(int i=1;i<n;i++){int k=(sn-1-i)%Samples;if(sxs[k]!=sxs[k0]||sys[k]!=sys[k0])break;if(uint32_t(st[k0]-st[k])>=FLING_STILL_MS)return 0;}
  uint32_t dt=st[k0]-st[kb];if(!dt)return 0;int d=axis?sys[k0]-sys[kb]:sxs[k0]-sxs[kb];long v=long(d)*1000/long(dt);if(v>FLING_MAX)v=FLING_MAX;if(v<-FLING_MAX)v=-FLING_MAX;return int(v);}
 uint32_t began=0,released=0,scene=0;std::string key;
 void emit(TouchActionKind kind,uint32_t at){TouchAction action;action.kind=kind;action.x=px;action.y=py;action.dx=px-sx;action.dy=py-sy;action.target=target;action.scene=scene;action.releasedAt=at;action.key=key;if(count==Capacity){dropped++;return;}queue[(head+count)%Capacity]=action;count++;queued++;}
public:
 uint32_t queued=0,dropped=0,cancelled=0;bool stream=false;int lastVy=0,lastSamples=0;uint32_t lastAge=0;// diagnostics of the last streamed release// stream: Move events while the finger moves (lists follow the finger)
 bool needsTarget(bool down,bool allowStart=true)const{return down&&!active&&!blocked&&allowStart;}
 bool contactActive()const{return active||blocked;}
 // The target under a finger still resting on it (no move, no Hold yet; -1: none): press feedback.
 bool still()const{return active&&!up&&!moved&&!consumed&&!streamed;}// a finger resting where it landed
 int pressed()const{return still()?target:-1;}
 // A finger resting where it landed (no move, no Hold yet): where and since when (press ring everywhere).
 bool resting(int &x,int &y,uint32_t &since,uint32_t currentScene)const{if(!still()||scene!=currentScene)return false;x=sx;y=sy;since=began;return true;}
 int size()const{return count;}
 void reset(){active=up=consumed=blocked=moved=false;head=count=0;key.clear();}
 void waitForRelease(){reset();blocked=true;}
 void feed(bool down,int x,int y,uint32_t now,uint32_t currentScene,int channel=-1,const std::string &identity="",bool allowStart=true,uint32_t hold=CHANNEL_HOLD_MS){// hold: how long a still finger on target makes a Hold
  if(blocked){if(!down){if(!up){up=true;released=now;}if(uint32_t(now-released)>=TOUCH_RELEASE_MS){blocked=up=false;}}else up=false;return;}
  if(active&&scene!=currentScene){if(streamed){scene=currentScene;}/* a scrolling finger changes the scene itself: keep following it */else{cancelled++;active=false;blocked=down;up=false;key.clear();return;}}
  if(down){
   if(!active){if(!allowStart){blocked=true;up=false;return;}active=true;consumed=moved=streamed=false;sn=0;sx=px=x;sy=py=y;lx=x;ly=y;lastMove=now;began=now;scene=currentScene;target=channel;key=identity;holdMs=hold;}
   up=false;px=x;py=y;st[sn%Samples]=now;sxs[sn%Samples]=x;sys[sn%Samples]=y;sn++;
   if(abs(x-sx)>CHANNEL_HOLD_SLOP||abs(y-sy)>CHANNEL_HOLD_SLOP){target=-1;moved=true;key.clear();}
   if(!consumed&&target>=0&&uint32_t(now-began)>=holdMs){emit(TouchActionKind::Hold,now);consumed=true;}
   if(stream&&moved&&!consumed&&(abs(x-lx)>=3||abs(y-ly)>=3)&&uint32_t(now-lastMove)>=16){int dx=x-lx,dy=y-ly;lx=x;ly=y;lastMove=now;streamed=true;
    if(count&&queue[(head+count-1)%Capacity].kind==TouchActionKind::Move){auto &last=queue[(head+count-1)%Capacity];last.dx+=dx;last.dy+=dy;last.x=x;last.y=y;}// coalesce with a Move not yet consumed
    else{TouchAction a;a.kind=TouchActionKind::Move;a.x=x;a.y=y;a.dx=dx;a.dy=dy;a.scene=scene;a.releasedAt=now;if(count==Capacity)dropped++;else{queue[(head+count)%Capacity]=a;count++;queued++;}}}
  }else if(active){
   if(!up){up=true;released=now;}
   if(uint32_t(now-released)>=TOUCH_RELEASE_MS){if(streamed&&!consumed&&(px!=lx||py!=ly)){int dx=px-lx,dy=py-ly;lx=px;ly=py;// the tail of the drag not yet sent
     if(count&&queue[(head+count-1)%Capacity].kind==TouchActionKind::Move){auto &last=queue[(head+count-1)%Capacity];last.dx+=dx;last.dy+=dy;}else{TouchAction a;a.kind=TouchActionKind::Move;a.x=px;a.y=py;a.dx=dx;a.dy=dy;a.scene=scene;a.releasedAt=released;if(count==Capacity)dropped++;else{queue[(head+count)%Capacity]=a;count++;queued++;}}}
    if(streamed&&!consumed){int vx=flingVelocity(0,released),vy=flingVelocity(1,released);lastVy=vy;lastSamples=sn;lastAge=sn?released-st[(sn-1)%Samples]:0;if(abs(vy)>=FLING_MIN||abs(vx)>=FLING_MIN){TouchAction a;a.kind=TouchActionKind::Fling;a.x=px;a.y=py;a.dx=vx;a.dy=vy;a.scene=scene;a.releasedAt=released;if(count==Capacity)dropped++;else{queue[(head+count)%Capacity]=a;count++;queued++;}}}
    if(!consumed&&!streamed){if(abs(py-sy)>35&&abs(py-sy)>=abs(px-sx))emit(TouchActionKind::Swipe,released);else if(!moved)emit(TouchActionKind::Tap,released);else if(abs(px-sx)>35)emit(TouchActionKind::Drag,released);else cancelled++;}active=up=false;key.clear();}
  }
 }
 bool pop(TouchAction &action){if(!count)return false;action=queue[head];queue[head]=TouchAction();head=(head+1)%Capacity;count--;return true;}
};
}
