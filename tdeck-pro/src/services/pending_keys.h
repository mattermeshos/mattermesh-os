#pragma once
#include <string.h>
namespace device {
// Typed text in arrival order, one key (up to 4 UTF-8 bytes) per entry. An entry can wait (the space key while it is
// still unclear whether it is a tap or the lock gesture): what follows is held back until it is resolved, so a fast
// "o" + space + "è" is never reordered.
struct PendingKeys {
 struct Entry {char text[5]={0};bool waiting=false;};
 Entry queue[128];int count=0;
 bool waiting()const{for(int i=0;i<count;i++)if(queue[i].waiting)return true;return false;}
 void reset(){count=0;}
 bool append(const char *text,bool waiting=false){if(count==128){reset();return false;}auto &e=queue[count++];strncpy(e.text,text,4);e.text[4]=0;e.waiting=waiting;return true;}
 bool append(char character,bool waiting=false){const char text[2]={character,0};return append(text,waiting);}
 // Every waiting entry: typed (a space tap) or dropped (the lock gesture).
 void resolveAll(bool consumed){for(int i=0;i<count;i++)if(queue[i].waiting){queue[i].waiting=false;if(consumed)queue[i].text[0]=0;}}
 template<class Emit>void flush(Emit emit){while(count&&!queue[0].waiting){Entry e=queue[0];for(int i=1;i<count;i++)queue[i-1]=queue[i];count--;if(e.text[0])emit(e.text);}}
};
}
