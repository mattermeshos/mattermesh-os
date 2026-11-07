#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include "ui/renderer.h"
#include "ui/fast_canvas.h"
#include "services/geo.h"
// Contacts page (Home tab, section 1): tabs in the header, search + count row and four rows in the content area, rendering states, distance text.
int main(){mm::FastCanvas16 canvas(mm::W,mm::H);mm::Renderer renderer(canvas);
 auto disjoint=[](mm::Rect a,mm::Rect b){return a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+a.h<=b.y||b.y+b.h<=a.y;};
 // The Plus draws no LoRa panel outline on this page (tabs are accent chips): rects must stay inside the content area instead.
 auto inContent=[](mm::Rect r){return r.x>=mm::Margin&&r.x+r.w<=mm::W-mm::Margin&&r.y>=mm::Top&&r.y+r.h<=mm::Bottom;};
 // Tabs sit in the header strip, end before the right margin's rounded corner and do not overlap each other or the back button.
 for(int i=0;i<mm::ContactTabCount;i++){auto t=mm::contactTabRect(i);assert(t.y>=mm::StatusH&&t.y+t.h<=mm::Top+1);assert(t.x+t.w<=mm::W-mm::Margin-mm::Radius);assert(t.x>=mm::BackButton.x+mm::BackButton.w);for(int j=0;j<i;j++)assert(disjoint(t,mm::contactTabRect(j)));}
 // Removed: pager (ContactPrevious/Next) and ContactNoticeY checks: the Plus scrolls by finger/trackball and shows notices in the count field.
 assert(inContent(mm::ContactSearch)&&inContent(mm::ContactCount)&&disjoint(mm::ContactSearch,mm::ContactCount));
 for(int i=0;i<mm::ContactRows;i++){auto r=mm::contactRowRect(i);assert(inContent(r));assert(disjoint(r,mm::ContactSearch)&&disjoint(r,mm::ContactCount));for(int j=0;j<i;j++)assert(disjoint(r,mm::contactRowRect(j)));}
 // Rendering: focused row filled with the accent, other rows on the surface colour, empty state and search state change the frame.
 mm::State s;s.page=mm::Page::Mesh;s.radioReady=true;s.filteredNodeCount=3;s.contactLive=2;
 const char *names[]={"River","Base Camp RPT","Archive"};for(int i=0;i<3;i++){mm::Node n;n.name=names[i];n.key=std::string(64,'a'+i);n.type=i==1?2:1;n.live=i<2;n.age=30;n.rssi=-80;n.snr=6;n.hops=i;n.distance=i==0?"1.2 km NE":"";s.nodes.push_back(n);}
 auto frame=[&](){return std::vector<uint16_t>(canvas.getBuffer(),canvas.getBuffer()+mm::W*mm::H);};
 auto fillAt=[&](mm::Rect r){return canvas.getPixel(r.x+2,r.y+r.h/2);};// left padding of a row, before the type badge
 renderer.draw(s);auto list=frame();auto r0=mm::contactRowRect(0),r1=mm::contactRowRect(1);
 assert(fillAt(r0)==mm::Accent&&fillAt(r1)==mm::Surface); // row 0 focused (filled), row 1 plain card
 assert(fillAt(mm::contactRowRect(3))==mm::BG); // no fourth row drawn
 s.contactFocus=1;renderer.draw(s);assert(fillAt(r0)==mm::Surface&&fillAt(r1)==mm::Accent);s.contactFocus=0;
 s.filter=2;renderer.draw(s);auto rpt=frame();assert(rpt!=list);s.filter=0;
 s.contactSearching=true;s.contactQuery="riv";renderer.draw(s);auto searching=frame();assert(searching!=list);s.contactSearching=false;s.contactQuery="";
 s.nodes.clear();s.filteredNodeCount=s.contactLive=0;renderer.draw(s);auto empty=frame();assert(empty!=list);for(int i=0;i<mm::ContactRows;i++)assert(fillAt(mm::contactRowRect(i))==mm::BG);
 s.radioReady=false;renderer.draw(s);assert(frame()!=empty);
 // Distance text.
 assert(mm::formatDistance(850.4,"NE")=="850 m NE");assert(mm::formatDistance(2440,"N")=="2.4 km N");assert(mm::formatDistance(120400,"W")=="120 km W");
 double d=mm::geoDistanceMeters(44.0,8.0,45.0,8.0);assert(d>111000&&d<111400);assert(!strcmp(mm::geoHeading(44.0,8.0,45.0,8.0),"N"));assert(!strcmp(mm::geoHeading(44.0,8.0,44.0,8.1),"E"));assert(!strcmp(mm::geoHeading(44.0,8.0,43.5,7.5),"SW"));
 std::cout<<"PASS: Contacts page layout (tabs in header, search/count/rows inside the content area), focus/search/filter/empty/offline frames, distance text"<<std::endl;return 0;}
