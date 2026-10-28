#pragma once
#include <stdint.h>
namespace device {
constexpr uint32_t UNLOCK_TAP_WINDOW_MS=2500,UNLOCK_TAP_DEBOUNCE_MS=30;
// A lost release must never leave SPACE latched until a reboot.
constexpr uint32_t UNLOCK_HELD_RECOVERY_MS=10000;
constexpr int UNLOCK_TAP_COUNT=2;
struct UnlockTaps {
 bool down=false,completed=false;int taps=0;uint32_t started=0,lastRelease=0;
 bool press(uint32_t now,bool hardwareDebounced=false){
  // TCA8418 edges are already debounced. FIFO timestamps describe acquisition,
  // not physical key duration: several distinct edges can share a timestamp.
  if(down||completed||(!hardwareDebounced&&uint32_t(now-lastRelease)<UNLOCK_TAP_DEBOUNCE_MS))return false;
  if(taps&&uint32_t(now-lastRelease)>UNLOCK_TAP_WINDOW_MS)taps=0;
  down=true;started=now;taps++;return true;
 }
 bool release(uint32_t now,bool hardwareDebounced=false){
  // Completion is latched; duplicate releases cannot revoke a valid gesture.
  if(completed)return true;
  if(!down)return false;down=false;lastRelease=now;
  if(!hardwareDebounced&&uint32_t(now-started)<UNLOCK_TAP_DEBOUNCE_MS){taps--;return false;}
  completed=taps>=UNLOCK_TAP_COUNT;return completed;
 }
 bool expire(uint32_t now){
  if(completed)return false;
  if((down&&uint32_t(now-started)>=UNLOCK_HELD_RECOVERY_MS)||(!down&&taps&&uint32_t(now-lastRelease)>UNLOCK_TAP_WINDOW_MS)){reset();return true;}
  return false;
 }
 bool active()const{return down||taps>0;}
 void reset(){down=completed=false;taps=0;started=lastRelease=0;}
};
}
