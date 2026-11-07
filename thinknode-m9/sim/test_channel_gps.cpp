#include <cassert>
#include <iostream>
#include <limits>
#include "services/gps_position_policy.h"
#include "ui/renderer.h"
#include "ui/fast_canvas.h"
#include "../hardware/lib/HynTouch/src/TouchContact.h"
int main(){
 HynTouchContact contact;contact.report(true,1,50,100);assert(contact.down&&contact.x==50&&contact.y==100);contact.report(false,0,0,0);assert(contact.down);contact.report(true,0,0,0);assert(!contact.down);contact.report(true,1,60,110);contact.reset();assert(!contact.down);


 using device::gpsCoordinateMessage;
 assert(gpsCoordinateMessage(true,true,true,0,45.934123,7.629456)=="GPS: 45.934123, 7.629456");
 assert(gpsCoordinateMessage(true,true,true,14999,-45.1,-179.2)=="GPS: -45.100000, -179.200000");
 assert(!gpsCoordinateMessage(true,true,true,0,0,0).empty());
 assert(gpsCoordinateMessage(false,true,true,0,44,8).empty());assert(gpsCoordinateMessage(true,false,true,0,44,8).empty());assert(gpsCoordinateMessage(true,true,false,0,44,8).empty());assert(gpsCoordinateMessage(true,true,true,15000,44,8).empty());
 assert(gpsCoordinateMessage(true,true,true,UINT32_MAX,44,8).empty());assert(gpsCoordinateMessage(true,true,true,0,91,8).empty());assert(gpsCoordinateMessage(true,true,true,0,44,-181).empty());assert(gpsCoordinateMessage(true,true,true,0,std::numeric_limits<double>::quiet_NaN(),8).empty());
 mm::FastCanvas16 canvas(mm::W,mm::H);mm::Renderer r(canvas);mm::State s;s.page=mm::Page::Messages;s.channelOptions=true;s.channelOptionName="Italia";auto frame=[&]{return std::vector<uint16_t>(canvas.getBuffer(),canvas.getBuffer()+mm::W*mm::H);};r.draw(s);auto loud=frame();s.channelOptionMuted=true;r.draw(s);auto muted=frame();assert(loud!=muted);s.deleteChannelConfirm=true;r.draw(s);assert(frame()!=muted);
 for(auto rect:{mm::ChannelAction,mm::channelOptionRect(1),mm::channelOptionRect(2),mm::ChatClearYes,mm::ChatClearNo,mm::PositionSend,mm::PositionCancel}){assert(rect.x>=mm::Margin&&rect.x+rect.w<=mm::W-mm::Margin);assert(rect.y>=mm::Top&&rect.y+rect.h<=mm::Bottom);}
 s.channelOptions=false;s.page=mm::Page::Chat;s.positionShare=true;s.gpsAvailable=true;s.gpsEnabled=false;s.positionMessage="GPS is off. Enable it to acquire your position.";r.draw(s);auto off=frame();s.gpsEnabled=true;s.positionMessage="Waiting for a fresh GPS fix...";r.draw(s);auto waiting=frame();assert(waiting!=off);s.positionCoordinates=gpsCoordinateMessage(true,true,true,0,45.934123,7.629456);r.draw(s);assert(frame()!=waiting);
 // Chat options sheet (the entry point of "Send GPS position"): every option must sit on the 240 px screen above the bottom margin.
 for(int i=0;i<mm::ChatOptionCount;i++){auto rect=mm::chatOptionRect(i);assert(rect.x>=mm::Margin&&rect.x+rect.w<=mm::W-mm::Margin&&rect.y>=mm::Top&&rect.y+rect.h<=mm::Bottom);}
 std::cout<<"PASS: live GPS coordinates/age/OFF/cache-invalid/nonfinite/range; channel and position modal bounds\n";
}
