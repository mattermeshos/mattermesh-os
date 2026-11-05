#pragma once
#include "lock_policy.h"
namespace device {
constexpr uint32_t LOCKED_CPU_FALLBACK=80;
inline uint32_t lockedCpuFrequency(bool wifi,bool bluetooth,bool gps,bool work){return wifi||bluetooth||gps||work?LOCKED_CPU_FALLBACK:LOCKED_CPU_FREQUENCY;}
}
