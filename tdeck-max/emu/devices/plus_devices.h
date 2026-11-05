#pragma once
// Emulated T-Deck Plus chips, attached to the emulated buses (emu_hal.h) at the board's pins and addresses
// (src/board/plus_board.h). The firmware's own drivers talk to them unchanged.
#include <cstdint>
namespace plusdev {
// Registers every chip; call before setup(). The SX1262 joins the shared air (artifacts/emu-air on airPort) as
// airName; with airPort 0 it has no antenna (TX completes, nothing is received).
void attach(const char *airName,int airPort);
// LCD (ST7789): the panel memory as the user sees it (RGB565, standard byte order), its size and state.
const uint16_t *frame();
int frameWidth();int frameHeight();
bool frameDirty(bool clear);             // anything drawn or a state change since the last call
bool displayOn();                        // sleep out + display on
int backlight();                         // 0..16 (LilyGO pulse dimming on GPIO42)
// Keyboard (ESP32-C3 matrix, raw mode): a key cap by its T-Deck key code (1..35, see plus_keyboard.h).
void key(int code,bool pressed);
// Touch (GT911): a finger at screen coordinates (landscape 320x240), or lifted.
void touch(int x,int y,bool down);
// Trackball: rolls (each unit = one detent movement) and the centre press (BOOT pin).
void roll(int dx,int dy);
void press(bool down);
// Battery: on battery (3.92 V) or on USB (4.66 V at the divider), as the firmware's ADC reading sees it.
void usbPower(bool on);bool usbPowered();
// Keyboard light duty 0..255 (I2C command 0x01).
int keyboardLight();
}
