#pragma once
#include "esp_system.h"
#include <stdlib.h>
#define ESP_ERROR_CHECK(x) do { if ((x) != ESP_OK) abort(); } while(0)
static inline const char *esp_err_to_name(esp_err_t e) { return e == ESP_OK ? "ESP_OK" : "ESP_FAIL"; }
