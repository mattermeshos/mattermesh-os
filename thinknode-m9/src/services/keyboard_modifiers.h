#pragma once
namespace device {
// Menus accept upper/lower-case shortcuts. Editors (especially passwords and
// repeater CLI) must receive exactly the shifted character the user typed.
inline char keyboardCase(char ch,bool navigation,bool editing){return navigation&&!editing&&ch>='A'&&ch<='Z'?ch-'A'+'a':ch;}
// TCA8418 codes from the LilyGO factory matrix (reverse column wiring).
struct KeyboardModifiers {
 bool alt=false,sym=false,leftShift=false,rightShift=false;
 bool update(int code,bool pressed){
  switch(code){case 30:alt=pressed;break;case 31:rightShift=pressed;break;case 32:sym=pressed;break;case 35:leftShift=pressed;break;default:return false;}
  return true;
 }
 bool shift()const{return leftShift||rightShift;}// the accent key's Shift is left out in chats (main.cpp shiftHeld)
 bool symbols()const{return alt||sym;}
 void reset(){alt=sym=leftShift=rightShift=false;}
};
}
