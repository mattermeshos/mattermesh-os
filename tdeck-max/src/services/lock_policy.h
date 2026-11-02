#pragma once
#include <stdint.h>
namespace device {
constexpr uint32_t AUTO_LOCK_IDLE_MS=60000;
// The lock follows the display timeout: never before AUTO_LOCK_IDLE_MS; Never (0) does not lock by itself (the lock key still does).
inline bool autoLockDue(uint32_t now,uint32_t lastActivity,bool locked,uint32_t displaySeconds){if(locked||!displaySeconds)return false;
 uint32_t idle=displaySeconds>AUTO_LOCK_IDLE_MS/1000?displaySeconds*1000u:AUTO_LOCK_IDLE_MS;return uint32_t(now-lastActivity)>=idle;}
constexpr uint32_t LOCKED_CPU_FREQUENCY=40,NORMAL_CPU_FREQUENCY=240;
constexpr int SPACE_KEYCODE=33;
}
