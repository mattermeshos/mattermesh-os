#pragma once
#include <cstdint>
// T-Deck Max chips, driven exclusively through the firmware's buses and GPIOs.
namespace maxdev {
void attach(const char *airName, int airPort);
const uint16_t *frame();
int frameWidth();
int frameHeight();
bool frameDirty(bool clear);
bool displayOn();
int backlight();
void key(int code, bool pressed);
void touch(int x, int y, bool down);
void roll(int dx, int dy); // Max has no trackball; intentionally inert.
void press(bool down);
void usbPower(bool on);
bool usbPowered();
int keyboardLight();
// XL9555 physical pin level; used by the shared GPS model for its Max power domain.
bool ioLevel(int pin);
}
