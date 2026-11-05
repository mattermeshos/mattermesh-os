#pragma once
#include "esp_system.h"
inline esp_err_t spi_flash_get_chip_size(size_t *n) {
  if (!n)
    return ESP_ERR_INVALID_ARG;
  *n = 16 * 1024 * 1024;
  return ESP_OK;
}
