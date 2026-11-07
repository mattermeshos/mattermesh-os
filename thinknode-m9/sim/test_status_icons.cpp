#include <cassert>
#include <iostream>
#include <vector>
#include "ui/components.h"
#include "ui/fast_canvas.h"
static uint16_t pixel(const std::vector<uint16_t> &b,int x,int y){return b[y*mm::W+x];}
int main(){
 mm::FastCanvas16 frame(mm::W,mm::H);mm::Components c(frame);mm::Status s;
 auto ink=[&](int x,int y){return frame.getPixel(x,y)!=mm::StatusBg;};
 for(int mask=0;mask<16;mask++)for(bool charging:{false,true}){
  s=mm::Status{};s.clock="23:59";s.unread=mask&1;s.gps=mask&2;s.wifi=mask&4;s.bt=mask&8;s.charging=charging;
  int count=!!(mask&1)+!!(mask&2)+!!(mask&4)+!!(mask&8);
  s.battery=100;auto full=c.statusLayout(s);c.statusBar(s);std::vector<uint16_t> before(frame.getBuffer(),frame.getBuffer()+mm::W*mm::H);
  s.battery=99;auto narrow=c.statusLayout(s);c.statusBar(s);int shift=full.percentWidth-narrow.percentWidth;
  assert(shift>0&&narrow.batteryX-full.batteryX==shift&&narrow.iconsRight-full.iconsRight==shift);
  // Icons and battery translate by `shift`, except the right end of the level fill (18 px at 100 %, 17 px at 99 %).
  int first=count?full.iconsRight-((count-1)*mm::StatusIconStep+mm::IconStatus):full.batteryX,fillEnd=full.batteryX+2+(mm::StatusBatteryW-7);
  for(int y=0;y<mm::StatusH-1;y++)for(int x=first;x<full.batteryX+mm::StatusBatteryW;x++)if(!(y>=mm::StatusBatteryY+2&&y<mm::StatusBatteryY+mm::StatusBatteryH-2&&x>=fillEnd-3&&x<fillEnd))assert(pixel(before,x,y)==frame.getPixel(x+shift,y));
  // Clock never moves (the Plus draws no INT/EXT antenna label; the old antenna span is still checked as untouched).
  for(int y=0;y<mm::StatusH;y++)for(int x=0;x<mm::StatusAntennaX+mm::StatusAntennaW;x++)assert(pixel(before,x,y)==frame.getPixel(x,y));
  for(int percent:{100,99,50,10,9,1,0,-1,101}){
   s.battery=percent;auto layout=c.statusLayout(s);c.statusBar(s);
   assert(layout.percentX+layout.percentWidth==mm::W-mm::StatusRightMargin);
   assert(layout.batteryX+mm::StatusBatteryW+mm::StatusBatteryGap==layout.percentX);
   assert(layout.iconsRight+mm::StatusIndicatorGap==layout.batteryX);
   int start=layout.iconsRight-(count?(count-1)*mm::StatusIconStep+mm::IconStatus:0);
   assert(start>mm::StatusAntennaX+mm::StatusAntennaW);
   for(int y=mm::StatusIconY;y<mm::StatusIconY+mm::IconStatus;y++)for(int x=mm::StatusIconsX;x<start;x++)assert(!ink(x,y));
   for(int i=0;i<count;i++){bool on=false;for(int y=2;y<14;y++)for(int x=start+i*mm::StatusIconStep;x<start+i*mm::StatusIconStep+mm::IconStatus;x++)on=on||ink(x,y);assert(on);}
  }
 }
 // Plus battery: rounded outline (corner pixel is transparent, so the top edge is probed), empty inside at 0 %, accent fill at 100 %.
 s=mm::Status{};s.battery=0;c.statusBar(s);auto empty=c.statusLayout(s);assert(frame.getPixel(empty.batteryX+6,mm::StatusBatteryY)==mm::StatusFg&&!ink(empty.batteryX+3,mm::StatusBatteryY+3));
 s.battery=100;c.statusBar(s);auto full=c.statusLayout(s);assert(frame.getPixel(full.batteryX+3,mm::StatusBatteryY+3)==mm::Accent);
 s.battery=10;auto two=c.statusLayout(s);s.battery=9;auto one=c.statusLayout(s);assert(one.batteryX>two.batteryX);
 std::cout<<"PASS: dynamic 100->99 and10->9 status geometry, all16 icon masks, charging, actual translated pixels, no clock movement or overlap, unknown/invalid percentages\n";
}
