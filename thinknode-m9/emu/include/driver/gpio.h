#pragma once
#include "esp_system.h"
using gpio_num_t = int;
esp_err_t gpio_set_level(gpio_num_t, uint32_t);
// Pull resistors are not modelled: the emulated buses always answer.
enum gpio_pull_mode_t {GPIO_PULLUP_ONLY,GPIO_PULLDOWN_ONLY,GPIO_PULLUP_PULLDOWN,GPIO_FLOATING};
inline esp_err_t gpio_set_pull_mode(gpio_num_t,gpio_pull_mode_t){return 0;}
