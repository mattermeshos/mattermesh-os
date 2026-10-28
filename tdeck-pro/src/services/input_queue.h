#pragma once
#include <stdint.h>
namespace device {
struct InputEvent {uint8_t raw=0;uint32_t at=0;bool hardwareDebounced=false;};
class InputQueue {
 static constexpr int Capacity=128;InputEvent events[Capacity];int head=0,count=0;bool overflowed=false;
public:
 uint32_t dropped=0;
 bool push(uint8_t raw,uint32_t at,bool hardwareDebounced=false){if(count==Capacity){invalidate();return false;}auto &event=events[(head+count)%Capacity];event.raw=raw;event.at=at;event.hardwareDebounced=hardwareDebounced;count++;return true;}
 void invalidate(){dropped++;overflowed=true;}
 bool pop(InputEvent &event){if(!count)return false;event=events[head];head=(head+1)%Capacity;count--;return true;}
 int size()const{return count;}
 bool discardOverflow(){if(!overflowed)return false;overflowed=false;head=count=0;return true;}
};
}
