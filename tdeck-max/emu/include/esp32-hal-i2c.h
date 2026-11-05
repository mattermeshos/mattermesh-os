#pragma once
#include "driver/i2c.h"
#ifdef __cplusplus
extern "C" {
#endif
bool i2cIsInit(int);
esp_err_t i2cInit(int, int, int, uint32_t);
#ifdef __cplusplus
}
#endif
