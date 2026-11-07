#include <cassert>
#include <iostream>
#include "services/unlock_taps.h"
#include "services/input_queue.h"

int main(){
 using device::UnlockTaps;
 UnlockTaps taps;
 // Regression: a second release used to overwrite unlockPending=false forever.
 assert(taps.press(100));assert(!taps.release(180));
 assert(taps.press(300));assert(taps.release(380));
 for(int i=0;i<10;i++){assert(taps.release(380+i));assert(!taps.press(500+i));}
 assert(taps.completed&&!taps.expire(100000));
 // Real controller FIFO edges have no physical timestamps. A complete pair
 // may be acquired together while the display is busy or after idle polling.
 for(unsigned gap:{0u,1u,10u,30u,100u}){
  taps.reset();device::InputQueue queue;uint32_t now=1000;
  for(int raw:{161,161,33,33,161,161,33,33}){assert(queue.push(raw,now,true));now+=gap;}
  device::InputEvent e;
  while(queue.pop(e)){if(e.raw&128)taps.press(e.at,e.hardwareDebounced);else taps.release(e.at,e.hardwareDebounced);}
  assert(taps.completed&&taps.taps==2&&!taps.down);
 }
 // Synthetic/non-debounced input still rejects bounce and incomplete taps.
 taps.reset();assert(taps.press(2000));assert(!taps.release(2010));assert(!taps.completed&&taps.taps==0);
 assert(!taps.press(2020));assert(taps.press(2050));assert(!taps.release(2100));
 assert(taps.expire(4601)&&!taps.active());assert(!taps.completed);
 // Missing release recovers to locked idle; it cannot itself authorize unlock.
 assert(taps.press(5000));assert(!taps.expire(14999));
 assert(taps.expire(15000)&&!taps.active()&&!taps.completed);
 assert(!taps.release(15100));
 assert(taps.press(15200));assert(!taps.release(15300));assert(taps.press(15400));assert(taps.release(15500));
 // Timer rollover, reset reuse, and corrupted/overflowed queues.
 taps.reset();assert(taps.press(0xfffffff0));assert(taps.expire(10000)&&!taps.completed);
 taps.reset();assert(taps.press(100));assert(!taps.release(200));
 device::InputQueue queue;queue.push(161,300,true);queue.invalidate();assert(queue.discardOverflow()&&queue.size()==0);
 taps.reset();assert(taps.press(210));assert(!taps.completed);
 std::cout<<"PASS: unlock completion latched across duplicate releases; debounced FIFO bursts; software bounce; missing release recovery; timeout/rollover; queue invalidation\n";
}
