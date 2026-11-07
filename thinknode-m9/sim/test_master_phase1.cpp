#include <cassert>
#include <iostream>
#include "services/space_gesture.h"
#include "services/reply_policy.h"
#include "services/received_repeater.h"
#include "services/lock_messages.h"
#include "ui/renderer.h"
#include "ui/fast_canvas.h"
#include <vector>
int main(){
 using namespace device;
 auto p=replyPlan("it-vda",7);assert(p.kind==ReplyKind::Scoped&&p.scope=="it-vda"&&p.hops==7);
 assert(replyPlan("*",0).kind==ReplyKind::Scoped);assert(replyPlan("unknown",-1).kind==ReplyKind::Scoped);assert(replyPlan("",-1).kind==ReplyKind::Scoped);
 assert(incomingRepeater("flood:ab,1234,abcdef")=="abcdef");assert(incomingRepeater("flood:").empty());assert(incomingRepeater("direct").empty());assert(incomingRepeater("flood:xy").empty());
 ReceivedRepeater rx;rx.receive(1,2,"flood:ab,cd",0xfffffff0);assert(rx.visible(20,6000));assert(!rx.visible(7000,6000));rx.receive(2,3,"direct",8000);assert(!rx.visible(8000,6000));
 SpaceTaps t;assert(t.press(100));assert(!t.release(200));assert(!t.completed);assert(t.press(400));assert(t.release(500));assert(t.release(501));
 t.reset();t.press(100,true);assert(!t.release(3100,true)&&!t.completed);t.reset();t.press(100,true);t.release(100,true);t.press(100,true);assert(t.release(100,true));
 t.reset();t.press(100);t.release(200);assert(t.expire(751));t.press(1000);assert(t.expire(11000));
 t.reset();t.press(0xfffffff0,true);t.release(20,true);t.press(80,true);assert(t.release(110,true));
 LockMessages empty;empty.reset({});assert(!empty.next({})&&empty.ended);assert(empty.next({123})==123&&!empty.ended);
 LockMessages q;q.reset({1,2,3});assert(q.current==3);assert(q.next({1,2,3})==2);assert(q.next({1,2,3})==1);assert(!q.next({1,2,3})&&q.ended);assert(!q.next({1,2,3}));
 q.receive({1,2,3,4},4);assert(q.current==4&&!q.browsing);assert(q.next({1,2,3,4})==3);q.receive({2,3,4,5},5);assert(q.current==5);assert(q.next({2,3,4,5})==4);
 for(uint32_t n=6;n<2000;n++){q.next({n-3,n-2,n-1,n});assert(q.ids.size()<=4);}
 mm::FastCanvas16 canvas(mm::W,mm::H);mm::Renderer renderer(canvas);mm::State s;auto shot=[&]{return std::vector<uint16_t>(canvas.getBuffer(),canvas.getBuffer()+mm::W*mm::H);};
 renderer.lock(s);auto none=shot();s.lockHasMessage=true;s.receivedPreview="A new received message";renderer.lock(s);assert(shot()!=none);
 // The Plus card shows sender/time and ONE preview line (QuickPreviewLines=1), not the Max three-line preview: check that line fits the card.
 assert(31+mm::Components::lineHeight(mm::Font::ChatBody)<=mm::LockMessageH);
 assert(mm::LockRepeater.y>=mm::LockMessage.y+mm::LockMessage.h+mm::LockRepeaterGap);
 assert(mm::LockRepeater.h>=mm::Components::lineHeight(mm::Font::ChatMeta)&&mm::LockRepeater.y+mm::LockRepeater.h<=mm::H);
 auto noRepeater=shot();
 s.lockRepeater="Received via: Mongioie";renderer.lock(s);bool footerInk=false;
 for(int y=0;y<mm::H;y++)for(int x=0;x<mm::W;x++)if(noRepeater[y*mm::W+x]!=canvas.getPixel(x,y)){assert(mm::LockRepeater.contains(x,y));footerInk=true;}
 assert(footerInk);
 // Removed: "no permanent footer" (rows below the card blank): the Plus lock always shows the unlock hint band at H-24.
 s.lockRepeater="";renderer.lock(s);assert(shot()==noRepeater);
 // Every theme (the Plus replaces the Max wallpapers) keeps the lock anatomy: card painted in BG over the picture, band at the bottom,
 // clock (lock face + status bar) drawn only above the card; the Max hero mask / summit checks do not apply to the photo themes.
 for(int t=0;t<mm::ThemeCount;t++){mm::setTheme(t);s.status.clock="10:24";renderer.lock(s);auto before=shot();
  assert(canvas.getPixel(mm::LockMessage.x+mm::LockMessage.w/2,mm::LockMessage.y+2)==mm::BG&&canvas.getPixel(mm::LockMessage.x+4,mm::LockMessage.y+mm::LockMessage.h/2)==mm::BG);
  assert(canvas.getPixel(0,mm::H-1)==(mm::theme(t).lockLayout==mm::LockLayout::RightColumn?mm::FG:mm::StatusBg));
  s.status.clock="22:51";renderer.lock(s);bool moved=false;
  for(int y=0;y<mm::H;y++)for(int x=0;x<mm::W;x++)if(before[y*mm::W+x]!=canvas.getPixel(x,y)){assert(y<mm::LockMessage.y);moved=true;}assert(moved);}
 mm::setTheme(0);s.status.clock="--:--";
 // Max partial-refresh planning (RefreshTracker) does not exist on the LCD; the intent kept: a new RX repaints only the card.
 renderer.lock(s);auto rxFrame=shot();s.receivedPreview="Other message";renderer.lock(s);bool card=false;
 for(int y=0;y<mm::H;y++)for(int x=0;x<mm::W;x++)if(rxFrame[y*mm::W+x]!=canvas.getPixel(x,y)){assert(mm::LockMessage.contains(x,y));card=true;}assert(card);
 std::cout<<"PASS: structured reply scope/hops; no guessed incoming hop; timers/rollover; double SPACE/no long-press/bounce/FIFO; lock read queue; one-line preview/isolated RPT footer/theme lock anatomy; RX repaints only the card\n";
}
