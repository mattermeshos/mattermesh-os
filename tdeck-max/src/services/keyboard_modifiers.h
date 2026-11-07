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
 // shift() is the right Shift alone; main.cpp picks the accent key (Settings > Accent key) and which Shift types capitals.
 bool shift()const{return rightShift;}
 bool symbols()const{return alt||sym||symLatch;}
 void reset(){alt=sym=leftShift=rightShift=symLatch=symUsed=false;}
 // Accent key rules (Settings > Accent key): accentCode is the chosen Shift's TCA8418 code (31 right, 35 left) or -1
 // when off, context is true in the message composer and the lock-screen quick reply. The chosen Shift accents there;
 // everywhere else, and for the other Shift, Shift types capitals.
 bool accentHeld(int accentCode,bool context)const{return accentCode>=0&&context&&(accentCode==35?leftShift:rightShift);}
 bool capitals(int accentCode,bool context)const{int code=context?accentCode:-1;return (leftShift&&code!=35)||(rightShift&&code!=31);}
};
}
