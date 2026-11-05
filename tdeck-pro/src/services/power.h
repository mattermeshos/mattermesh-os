#pragma once
#include <Arduino.h>
#include "policy.h"
namespace device {
struct PowerState {uint32_t displaySeconds=30,keyboardSeconds=30,lastActivity=0;bool displayAwake=true,keyboardAwake=true,displayEnabled=true,keyboardEnabled=true;uint32_t notificationFlashes=0;uint8_t displayPWM=0,keyboardPWM=0;};
void powerLocked(bool locked);
void unlockLightPreview(bool active);
bool lightEnabled(bool keyboard,bool enabled);void flashKeyboardNotification();bool notificationFlashing();
void powerBegin(uint8_t display,uint8_t keyboard);void powerLoop();void activity();
// Hold: while active (dictation recording/transcribing) the display/keyboard lights keep their current
// state and the idle clock does not advance (so no dimming and no auto-lock); when released, the idle
// timers start one second later. Call every loop with the current busy state.
void powerHold(bool active);void brightness(uint8_t display,uint8_t keyboard);bool timeout(bool keyboard,uint32_t seconds);PowerState powerState();
}
