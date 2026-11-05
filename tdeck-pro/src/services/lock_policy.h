#pragma once
#include <stdint.h>
namespace device {
constexpr uint32_t AUTO_LOCK_IDLE_MS=60000;
inline bool autoLockDue(uint32_t now,uint32_t lastActivity,bool locked){return !locked&&uint32_t(now-lastActivity)>=AUTO_LOCK_IDLE_MS;}
constexpr uint32_t LOCKED_CPU_FREQUENCY=40,NORMAL_CPU_FREQUENCY=240;
constexpr int SPACE_KEYCODE=33;
}
