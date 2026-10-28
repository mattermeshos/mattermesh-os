#pragma once
#include "unlock_taps.h"
namespace device {
constexpr uint32_t SPACE_DOUBLE_TAP_MS=550,SPACE_MAX_TAP_MS=500,UNLOCK_COUNTDOWN_MS=500,LOCK_HINT_MS=2500;
// FIFO timestamps may coincide; hardware debounce is authoritative for real keys.
struct SpaceTaps:UnlockTaps {
 bool press(uint32_t now,bool hardware=false){
  if(down||completed||(!hardware&&uint32_t(now-lastRelease)<UNLOCK_TAP_DEBOUNCE_MS))return false;
  if(taps&&uint32_t(now-lastRelease)>SPACE_DOUBLE_TAP_MS)taps=0;
  down=true;started=now;taps++;return true;
 }
 bool release(uint32_t now,bool hardware=false){
  if(completed)return true;if(!down)return false;down=false;lastRelease=now;
  if(uint32_t(now-started)>SPACE_MAX_TAP_MS){taps=0;return false;}
  if(!hardware&&uint32_t(now-started)<UNLOCK_TAP_DEBOUNCE_MS){taps--;return false;}
  completed=taps>=2;return completed;
 }
 bool expire(uint32_t now){if(completed)return false;if((down&&uint32_t(now-started)>=UNLOCK_HELD_RECOVERY_MS)||(!down&&taps&&uint32_t(now-lastRelease)>SPACE_DOUBLE_TAP_MS)){reset();return true;}return false;}
};
}
