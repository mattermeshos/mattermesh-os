#include "hyn_platform.h"

#include <Arduino.h>

#include "HynTouch.h"

// T-Deck Pro build: no XL9555 IO expander on this board, so this file carries only the generic optional
// callback hook (unused here) plus a no-op hyn_touch_attach_xl9555, to keep HynTouch.h's public API the same
// as the T-Deck Max build. The touch reset line is a plain ESP32 GPIO, toggled directly by the caller
// (main.cpp's initTouch()/setTouchEnabled()) before/around hyn_touch_init(), not through this platform layer.

namespace {
HynTouchVirtualGpioWriteCallback g_gpio_write_callback = nullptr;
HynTouchVirtualGpioReadCallback g_gpio_read_callback = nullptr;
void *g_gpio_callback_user_data = nullptr;
} // namespace

int hyn_platform_gpio_set_value(uint32_t gpio_id, int value)
{
    if (g_gpio_write_callback && g_gpio_write_callback(gpio_id, value != 0, g_gpio_callback_user_data)) {
        return 1;
    }
    return 0;
}

int hyn_platform_gpio_get_value(uint32_t gpio_id, int *out_value)
{
    if (!out_value) {
        return -1;
    }
    if (g_gpio_read_callback && g_gpio_read_callback(gpio_id, out_value, g_gpio_callback_user_data)) {
        return 1;
    }
    return 0;
}

void hyn_touch_set_virtual_gpio_callbacks(
    HynTouchVirtualGpioWriteCallback write_callback,
    HynTouchVirtualGpioReadCallback read_callback,
    void *user_data)
{
    g_gpio_write_callback = write_callback;
    g_gpio_read_callback = read_callback;
    g_gpio_callback_user_data = user_data;
}

void hyn_touch_attach_xl9555(ExtensionIOXL9555 *io)
{
    (void)io; // no IO expander on this board
}
