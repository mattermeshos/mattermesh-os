#pragma once
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef int gpio_num_t;
typedef int gpio_mode_t;
typedef int gpio_int_type_t;
#define GPIO_MODE_INPUT 1
#define GPIO_MODE_OUTPUT 2
#define GPIO_INTR_DISABLE 0
#define GPIO_INTR_NEGEDGE 2
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_PULLDOWN_ENABLE 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLUP_ENABLE 1
#define BIT64(n) (1ULL << (n))
typedef struct { uint64_t pin_bit_mask; gpio_mode_t mode; int pull_up_en, pull_down_en; gpio_int_type_t intr_type; } gpio_config_t;
esp_err_t gpio_set_level(gpio_num_t, uint32_t);
int gpio_get_level(gpio_num_t);
esp_err_t gpio_config(const gpio_config_t *);
esp_err_t gpio_install_isr_service(int);
esp_err_t gpio_isr_handler_add(gpio_num_t, void (*)(void *), void *);
esp_err_t gpio_isr_handler_remove(gpio_num_t);
#ifdef __cplusplus
}
#endif
