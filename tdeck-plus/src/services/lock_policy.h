#pragma once
#include <stdint.h>
namespace device {
constexpr uint32_t LOCK_HOLD_MS=2000,LOCK_DEBOUNCE_MS=30,LOCK_PROGRESS_DELAY_MS=500;
constexpr uint32_t AUTO_LOCK_IDLE_MS=30000;// T-Deck Plus: the shortest auto-lock, 30 s after the last key / touch / trackball move
// The lock follows the display timeout: it comes when the screen goes dark, never before 30 s; Never (0) never locks by
// itself (the lock key still does).
inline bool autoLockDue(uint32_t now,uint32_t lastActivity,bool locked,uint32_t displaySeconds){if(locked||!displaySeconds)return false;
 uint32_t idle=displaySeconds>AUTO_LOCK_IDLE_MS/1000?displaySeconds*1000u:AUTO_LOCK_IDLE_MS;return uint32_t(now-lastActivity)>=idle;}
// T-Deck Plus: 80 MHz is the floor. Its octal PSRAM runs at 80 MHz from the PLL; with the CPU on the 40 MHz crystal
// the PSRAM timing is no longer valid and the device resets (TG1WDT_SYS_RST seen on the lock at 40 MHz).
constexpr uint32_t LOCKED_CPU_FREQUENCY=80,NORMAL_CPU_FREQUENCY=240,LOCKED_CPU_FALLBACK=80;// fallback: a refused locked clock
constexpr int SPACE_KEYCODE=33;
struct LockKey {
 bool down=false,consumed=false;uint32_t started=0,lastRelease=0;int progress=0;
 bool press(uint32_t now,bool hardwareDebounced=false){if(down||(!hardwareDebounced&&uint32_t(now-lastRelease)<LOCK_DEBOUNCE_MS))return false;down=true;consumed=false;started=now;progress=0;return true;}
 bool release(uint32_t now,bool locked){if(!down)return false;bool shortSpace=!consumed&&!locked;down=false;progress=0;lastRelease=now;return shortSpace;}
 bool tick(uint32_t now,bool locked){if(locked||!down||consumed)return false;uint32_t elapsed=now-started,limit=LOCK_HOLD_MS;progress=elapsed<LOCK_PROGRESS_DELAY_MS?0:1+int((elapsed-LOCK_PROGRESS_DELAY_MS)*5/(limit-LOCK_PROGRESS_DELAY_MS));if(elapsed<limit)return false;consumed=true;progress=0;return true;}
};
}
