#pragma once
namespace device {
// Menus accept upper/lower-case shortcuts. Editors (especially passwords and
// repeater CLI) must receive exactly the shifted character the user typed.
inline char keyboardCase(char ch,bool navigation,bool editing){return navigation&&!editing&&ch>='A'&&ch<='Z'?ch-'A'+'a':ch;}
// TCA8418 codes from the LilyGO factory matrix (reverse column wiring).
struct KeyboardModifiers {
 bool alt=false,sym=false,leftShift=false,rightShift=false;
 /* sym for one hand: pressed and released alone it stays on for the next key only (symLatch, a second tap cancels);
    held while other keys are pressed it works as before and ends when released */
 bool symLatch=false,symUsed=false;
 bool update(int code,bool pressed){
  if(pressed&&sym&&code!=32)symUsed=true;/* any other key while sym is held, a modifier too */
  switch(code){case 30:alt=pressed;break;case 31:rightShift=pressed;break;case 35:leftShift=pressed;break;
   case 32:if(pressed){sym=true;symUsed=false;}else{sym=false;if(!symUsed)symLatch=!symLatch;}break;
   default:return false;}
  return true;
 }
 void typed(){symLatch=false;}/* a key used the symbols: a latched sym is spent */
 bool symShown()const{return sym||symLatch;}
 bool shift()const{return leftShift||rightShift;}// the accent key's Shift is left out in chats (main.cpp shiftHeld)
 bool symbols()const{return alt||sym||symLatch;}
 void reset(){alt=sym=leftShift=rightShift=symLatch=symUsed=false;}
};
}
