#pragma once
#include <cstdint>
extern uint32_t emuGpioRouting[64];
#define GPIO_FUNC0_OUT_SEL_CFG_REG (reinterpret_cast<uintptr_t>(emuGpioRouting))
