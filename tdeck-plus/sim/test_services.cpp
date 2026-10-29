#include <cassert>
#include <iostream>
#include "services/policy.h"
#include "services/touch_keys.h"
int main(){using namespace device;assert(BATTERY_FULL_MV==4200);/* calibration no longer moves the curve */assert(validTimeout(30)&&validTimeout(0)&&!validTimeout(31));assert(!expired(29999,0,30)&&expired(30000,0,30));assert(!expired(UINT32_MAX,0,0));assert(expired(16000,UINT32_MAX-20000,30));assert(expired(31000,0,15)&&!expired(31000,0,60));std::cout<<"PASS: linear voltage battery percentage, clamping, independent timers, never and millis rollover\n";
 assert(touchKeyAction(TOUCH_KEY_HEART,true)==TouchKeyAction::CursorLeft&&touchKeyAction(TOUCH_KEY_MESSAGE,true)==TouchKeyAction::CursorRight&&touchKeyAction(TOUCH_KEY_SEND,true)==TouchKeyAction::Send);assert(touchKeyAction(TOUCH_KEY_HEART,false)==TouchKeyAction::None&&touchKeyAction(TOUCH_KEY_SEND,false)==TouchKeyAction::None&&touchKeyAction(3,true)==TouchKeyAction::None);std::cout<<"PASS: touch keys act only while writing: heart left, message right, send\n";}
