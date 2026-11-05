#pragma once
#include "theme.h"
#include <vector>
#include <cstring>
namespace mm {
struct RefreshPlan {bool changed=false,full=false,typing=false/* short register waveform for the composer while typing */;Rect area={0,0,W,H};uint32_t changedPixels=0;};
class RefreshTracker {
 std::vector<uint8_t> previous;bool ready=false;uint32_t partialArea=0;bool fullDue=false;
public:
 void reset(){ready=false;partialArea=0;fullDue=false;}
 // Ghosting budget overrun: a flashing full refresh is wanted, but only when the user is idle (see loop()).
 bool maintenanceDue()const{return fullDue;}
 // Tiny partials (clock, icons) do not count toward PartialLimit; the area budget still accumulates them.
 static bool countsAsPartial(const RefreshPlan &plan){return plan.changed&&!plan.full&&plan.area.w*plan.area.h>=W*H*3/100;}
 // A full refresh is taken at once only on the first frame, on the transitions the caller marks (lock / unlock /
 // idle maintenance) or past the hard caps; everything else, page navigation included, is a fast partial update.
 RefreshPlan plan(const uint8_t *pixels,bool pageChanged,int partialCount){
  RefreshPlan p;const int stride=(W+7)/8,size=stride*H;
  if(!ready||pageChanged){p.changed=true;p.full=true;return p;}
  int left=stride,right=-1,top=H,bottom=-1;
  // Unchanged rows (most of them on a keystroke) are skipped with one memcmp each.
  for(int y=0;y<H;y++){if(!memcmp(&previous[y*stride],pixels+y*stride,stride))continue;for(int x=0;x<stride;x++)if(previous[y*stride+x]!=pixels[y*stride+x]){p.changedPixels+=__builtin_popcount(unsigned(previous[y*stride+x]^pixels[y*stride+x]));left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}}
  if(right<0)return p;
  p.changed=true;p.area={left*8,top,std::min(W,(right+1)*8)-left*8,bottom-top+1};
  bool budget=partialCount>=PartialLimit||partialArea>=W*H*6u;
  p.full=partialCount>=PartialLimit*3||partialArea>=W*H*12u;
  if(budget&&!p.full)fullDue=true;return p;
 }
 void commit(const uint8_t *pixels,const RefreshPlan &plan){if(plan.full){partialArea=0;fullDue=false;}else partialArea+=plan.area.w*plan.area.h;commit(pixels);}
 void commit(const uint8_t *pixels){previous.assign(pixels,pixels+((W+7)/8)*H);ready=true;}
};
}
