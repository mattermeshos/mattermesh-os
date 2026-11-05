#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t i2c_master_write_to_device(int, uint8_t, const uint8_t *, size_t, TickType_t);
esp_err_t i2c_master_read_from_device(int, uint8_t, uint8_t *, size_t, TickType_t);
#ifdef __cplusplus
}
#endif
