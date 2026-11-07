#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/fast_canvas.h"
int main(){mm::FastCanvas16 frame(mm::W,mm::H);mm::Components c(frame);mm::Renderer r(frame);mm::State s;s.page=mm::Page::Chat;s.chatName="Public";s.input="poiu";s.messages.push_back({"River","10:24","12°","2 hop | it",false});
 auto shot=[&]{return std::vector<uint16_t>(frame.getBuffer(),frame.getBuffer()+mm::W*mm::H);};
 // The Plus has one LoRa antenna and draws no INT/EXT status label: only the clock bound and its gap to the indicators are checked.
 assert(c.width("23:59",mm::Font::ChatBody)<=mm::StatusClockW);assert(mm::Margin+mm::StatusClockW<mm::StatusIconsX);
 r.chat(s);auto baseline=shot();s.notice="Reply ready / queued / saved";r.chat(s);assert(shot()==baseline);
 s.firstRepeater="First RPT: [abcd] Field repeater";r.chat(s);assert(shot()!=baseline);
 // Removed: INT/EXT antenna frame difference (no antenna indicator on the Plus status bar).
 /* the banner still in use */{const char *label="Lock device";assert(c.width(label,mm::Font::ChatMeta)<=mm::W-2*mm::Margin-30-mm::HoldDotsW-8);r.holdBanner(label,3);int cy=mm::HoldBannerY+3+(mm::HoldBannerH-6)/2,x0=mm::W-mm::Margin-mm::HoldDotsW+mm::HoldDotR;assert(frame.getPixel(x0,cy)==mm::FG&&frame.getPixel(x0+2*mm::HoldDotStep,cy)==mm::FG&&frame.getPixel(x0+3*mm::HoldDotStep,cy)==mm::Surface&&frame.getPixel(x0+5*mm::HoldDotStep,cy)==mm::Surface);}
 r.holdBanner("Lock device",6);{int cy=mm::HoldBannerY+3+(mm::HoldBannerH-6)/2,x0=mm::W-mm::Margin-mm::HoldDotsW+mm::HoldDotR;for(int i=0;i<6;i++)assert(frame.getPixel(x0+i*mm::HoldDotStep,cy)==mm::FG);assert(frame.getPixel(mm::Margin,cy)==mm::Surface&&frame.getPixel(mm::Margin-1,cy)==mm::BG);
  bool icon=false;for(int y=cy-8;y<cy+8;y++)for(int x=mm::Margin+8;x<mm::Margin+24;x++)icon|=frame.getPixel(x,y)!=mm::Surface;assert(icon);}assert(mm::HoldBannerY>=mm::StatusH&&mm::HoldBannerY+mm::HoldBannerH<mm::ComposerY);
 std::cout<<"PASS: readable time bounds, chat notices removed without reserved space, first RPT strip, the lock banner label fits beside the icon and the six hold dots (unlock-style card)\n";
}
