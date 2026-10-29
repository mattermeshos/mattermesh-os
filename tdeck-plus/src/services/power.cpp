#include "power.h"
#include <Preferences.h>
#include "../board/board.h"
namespace device {
static bool locked=false,unlockPreview=false;static int flashPhase=-1;static uint32_t flashAt=0,lastFlash=0;static bool flashed=false;
static uint32_t wakeUntil=0;static bool wakeSet=false;
constexpr uint8_t NOTIFICATION_PWM=191;constexpr uint32_t FLASH_ON_MS=160,FLASH_OFF_MS=140;static Preferences preferences;static PowerState state;static uint8_t displayBrightness=100,keyboardBrightness=180;
static void apply(){bool woken=wakeSet&&int32_t(wakeUntil-millis())>0;
 bool lockedLight=woken;// LCD: on the lock screen only the wake timers (SPACE 10 s, message 5 s) light the panel
 uint8_t display=state.displayEnabled&&((!locked&&(state.displayAwake||woken))||(locked&&lockedLight))?displayBrightness:0,keyboard=state.keyboardEnabled&&((!locked&&state.keyboardAwake)||(locked&&lockedLight))?keyboardBrightness:0;if(flashPhase>=0)keyboard=(flashPhase%2==0)?NOTIFICATION_PWM:0;if(display!=state.displayPWM){
 plus::backlight(display);
 state.displayPWM=display;}if(keyboard!=state.keyboardPWM){
 plus::keyboardLight(keyboard);
 state.keyboardPWM=keyboard;}}
void powerBegin(uint8_t display,uint8_t keyboard){preferences.begin("mmpower",false);state.displaySeconds=preferences.getUInt("display",30);state.keyboardSeconds=preferences.getUInt("keyboard",30);if(!validTimeout(state.displaySeconds))state.displaySeconds=30;if(!validTimeout(state.keyboardSeconds))state.keyboardSeconds=30;if(preferences.getUInt("display",0)!=state.displaySeconds)preferences.putUInt("display",state.displaySeconds);if(preferences.getUInt("keyboard",0)!=state.keyboardSeconds)preferences.putUInt("keyboard",state.keyboardSeconds);/* only a reset invalid value is written back at boot */state.displayEnabled=preferences.getBool("display_on",true);state.keyboardEnabled=preferences.getBool("keyboard_on",true);displayBrightness=display;keyboardBrightness=keyboard;state.displayPWM=255;state.keyboardPWM=255;activity();}
void powerLocked(bool value){locked=value;unlockPreview=false;if(!locked)activity();else apply();}
void unlockLightPreview(bool active){unlockPreview=locked&&active;apply();}
int32_t wakeRemaining(){return wakeSet?int32_t(wakeUntil-millis()):0;}
bool unlockPreviewActive(){return unlockPreview;}
void wakeDisplay(uint32_t ms){uint32_t until=millis()+ms;if(!wakeSet||int32_t(until-wakeUntil)>0)wakeUntil=until;wakeSet=true;apply();}
void activity(){if(locked)return;state.lastActivity=millis();state.displayAwake=state.keyboardAwake=true;apply();}
void brightness(uint8_t display,uint8_t keyboard){displayBrightness=display;keyboardBrightness=keyboard;apply();}
bool timeout(bool keyboard,uint32_t seconds){if(!validTimeout(seconds))return false;if(preferences.putUInt(keyboard?"keyboard":"display",seconds)!=sizeof(uint32_t))return false;(keyboard?state.keyboardSeconds:state.displaySeconds)=seconds;activity();return true;}
bool lightEnabled(bool keyboard,bool enabled){if(preferences.putBool(keyboard?"keyboard_on":"display_on",enabled)!=1)return false;(keyboard?state.keyboardEnabled:state.displayEnabled)=enabled;apply();return true;}
void flashKeyboardNotification(){uint32_t now=millis();if(flashPhase>=0||(flashed&&uint32_t(now-lastFlash)<2000))return;flashed=true;lastFlash=flashAt=now;flashPhase=0;state.notificationFlashes++;apply();}
bool notificationFlashing(){return flashPhase>=0;}
void powerLoop(){uint32_t now=millis();if(flashPhase>=0&&uint32_t(now-flashAt)>=(flashPhase%2==0?FLASH_ON_MS:FLASH_OFF_MS)){flashAt=now;if(++flashPhase>=4)flashPhase=-1;}
 // A sound playing (a message tone) is not user activity: it never holds off the screen-off and the lock.
 state.displayAwake=!expired(now,state.lastActivity,state.displaySeconds);state.keyboardAwake=!expired(now,state.lastActivity,state.keyboardSeconds);
 apply();}
PowerState powerState(){return state;}
}
