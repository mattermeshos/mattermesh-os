#pragma once
// T-Deck Max capacitive keys under the display (heart, message, send), reported by the CST226 touch
// controller as key ids. They act only while a draft is being written in the chat composer.
namespace device {
constexpr int TOUCH_KEY_HEART=0,TOUCH_KEY_MESSAGE=1,TOUCH_KEY_SEND=2;
enum class TouchKeyAction{None,CursorLeft,CursorRight,Send};
inline TouchKeyAction touchKeyAction(int id,bool writing){
 if(!writing)return TouchKeyAction::None;
 if(id==TOUCH_KEY_HEART)return TouchKeyAction::CursorLeft;
 if(id==TOUCH_KEY_MESSAGE)return TouchKeyAction::CursorRight;
 if(id==TOUCH_KEY_SEND)return TouchKeyAction::Send;
 return TouchKeyAction::None;
}
}
