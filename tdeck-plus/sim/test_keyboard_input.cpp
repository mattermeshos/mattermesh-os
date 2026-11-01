#include <cassert>
#include <iostream>
#include <string>
#include "services/keyboard_modifiers.h"
#include "services/accent_key.h"
int main(){using namespace device;
 for(char c:std::string("TyP9_X")){assert(keyboardCase(c,true,true)==c);assert(keyboardCase(c,false,true)==c);}
 assert(keyboardCase('W',true,false)=='w'&&keyboardCase('S',true,false)=='s');
 KeyboardModifiers m;m.update(31,true);assert(m.shift()&&!m.symbols());m.update(30,true);assert(m.alt&&m.symbols());m.update(30,false);m.update(31,false);assert(!m.shift()&&!m.symbols());
 m.update(35,true);assert(m.leftShift&&m.shift());m.update(35,false);assert(!m.shift());// both Shift keys type capitals (the accent key is set aside only in chats, main.cpp)
 assert(messageOpening("")&&messageOpening("@[Mario] ")&&messageOpening("@[Mario] @[Anna K] ")&&!messageOpening("ciao")&&!messageOpening("@[Mario] ciao ")&&!messageOpening("@[Mar"));
 assert(accented('e',"@[Mario] ",false)=="\xc3\x88");
 assert(accented('e',"perch",false)=="\xc3\xa9"&&accented('e',"",false)=="\xc3\x88"&&accented('e',"lui ",false)=="\xc3\xa8"&&accented('a',"citt",false)=="\xc3\xa0"&&accented('O',"per",true)=="\xc3\x92"&&accented('x',"",false).empty());
 m.update(32,true);assert(m.symbols());m.update(32,false);assert(!m.symbols());
 std::cout<<"PASS: case-sensitive password/CLI/name editors, case-insensitive menus, Alt/Sym/Shift, accented vowels\n";
}
