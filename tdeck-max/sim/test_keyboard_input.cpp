#include <cassert>
#include <iostream>
#include <string>
#include "services/keyboard_modifiers.h"
#include "services/accent_key.h"
int main(){using namespace device;
 for(char c:std::string("TyP9_X")){assert(keyboardCase(c,true,true)==c);assert(keyboardCase(c,false,true)==c);}
 assert(keyboardCase('W',true,false)=='w'&&keyboardCase('S',true,false)=='s');
 KeyboardModifiers m;m.update(31,true);assert(m.shift()&&!m.symbols());m.update(30,true);assert(m.alt&&m.symbols());m.update(30,false);m.update(31,false);assert(!m.shift()&&!m.symbols());
 m.update(35,true);assert(m.leftShift&&!m.shift());m.update(35,false);// left Shift is the accent key
 // Accent key rules for every mode (-1 off, 31 right, 35 left), in and out of a chat, with either Shift held.
 for(int code:{-1,31,35})for(int context=0;context<2;context++)for(int key:{31,35}){KeyboardModifiers k;k.update(key,true);
  bool chosen=context&&code==key;assert(k.accentHeld(code,context)==chosen&&k.capitals(code,context)==!chosen);}
 {KeyboardModifiers k;assert(!k.capitals(35,false)&&!k.accentHeld(35,true));k.update(35,true);k.update(31,true);assert(k.accentHeld(35,true)&&k.capitals(35,true));}
 assert(messageOpening("")&&messageOpening("@[Mario] ")&&messageOpening("@[Mario] @[Anna K] ")&&!messageOpening("ciao")&&!messageOpening("@[Mario] ciao ")&&!messageOpening("@[Mar"));
 assert(accented('e',"@[Mario] ",false)=="\xc3\x88");
 assert(accented('e',"perch",false)=="\xc3\xa9"&&accented('e',"",false)=="\xc3\x88"&&accented('e',"lui ",false)=="\xc3\xa8"&&accented('a',"citt",false)=="\xc3\xa0"&&accented('O',"per",true)=="\xc3\x92"&&accented('x',"",false).empty());
 m.update(32,true);assert(m.symbols());m.update(32,false);assert(!m.symbols());
 std::cout<<"PASS: case-sensitive password/CLI/name editors, case-insensitive menus, Alt/Sym/Shift, accent key rules per mode and context\n";
}
