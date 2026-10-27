#pragma once
// T-Deck Plus ST7789 LCD (320x240 landscape): the UI renders an RGB565 frame (FastCanvas16) and present() sends the
// rows that changed over the shared SPI bus.
#include <Arduino.h>
#include <SPI.h>
#include "plus_board.h"
#include <algorithm>
#include <esp_heap_caps.h>
namespace plus {
// LCD write clock: 80 MHz (ST7789 on the T-Deck Plus); 40 MHz is the LilyGO TFT_eSPI setting.
constexpr uint32_t PanelSpiHz=80000000;
class Lcd {
 void command(uint8_t c){digitalWrite(BOARD_TFT_DC,LOW);SPI.transfer(c);digitalWrite(BOARD_TFT_DC,HIGH);}
 void begin(){SPI.beginTransaction(SPISettings(PanelSpiHz,MSBFIRST,SPI_MODE0));digitalWrite(BOARD_TFT_CS,LOW);}
 void end(){digitalWrite(BOARD_TFT_CS,HIGH);SPI.endTransaction();}
 void send(uint8_t c,std::initializer_list<uint8_t> data){command(c);for(uint8_t d:data)SPI.transfer(d);}
public:
 static constexpr int Width=320,Height=240;
 // Same fields as TypingPanel so the serial typing commands still parse; an LCD paints every edit at once.
 uint32_t pushes=0;
 // LilyGO T-Deck Plus init (examples/UnitTest lcd_st7789v), then landscape: MADCTL MX|MV as TFT_eSPI rotation 1.
 void init(){
  pinMode(BOARD_TFT_CS,OUTPUT);digitalWrite(BOARD_TFT_CS,HIGH);pinMode(BOARD_TFT_DC,OUTPUT);digitalWrite(BOARD_TFT_DC,HIGH);
  begin();command(0x01);end();delay(120);
  begin();command(0x11);end();delay(120);
  begin();
  send(0x3A,{0x05});send(0x36,{0x60});
  send(0xB2,{0x0C,0x0C,0x00,0x33,0x33});send(0xB7,{0x75});send(0xBB,{0x1A});send(0xC0,{0x2C});send(0xC2,{0x01});
  send(0xC3,{0x13});send(0xC4,{0x20});send(0xC6,{0x0F});send(0xD0,{0xA4,0xA1});send(0xD6,{0xA1});
  send(0xE0,{0xD0,0x0D,0x14,0x0D,0x0D,0x09,0x38,0x44,0x4E,0x3A,0x17,0x18,0x2F,0x30});
  send(0xE1,{0xD0,0x09,0x0F,0x08,0x07,0x14,0x37,0x44,0x4D,0x38,0x15,0x16,0x2C,0x3E});
  send(0x21,{});send(0x29,{});
  end();
 }
 // Colour frames: RGB565 canvas in panel byte order (theme.h rgb()). present() compares with the frame on the panel and
 // sends only the changed rows, each run of rows clipped to its changed columns, so a clock tick or a typed letter
 // costs a few kB on the bus instead of the 150 kB of a full frame.
 uint16_t *shown=nullptr;bool shownValid=false;uint32_t presentBytes=0,lastPresentUs=0;
 // Opens a RAMWR into the window x,y,w,h (caller writes the pixels, then end()).
 void window(int x,int y,int w,int h){begin();send(0x2A,{uint8_t(x>>8),uint8_t(x),uint8_t((x+w-1)>>8),uint8_t(x+w-1)});send(0x2B,{uint8_t(y>>8),uint8_t(y),uint8_t((y+h-1)>>8),uint8_t(y+h-1)});command(0x2C);}
 // The panel holds an intermediate frame on rows y0..y1: mark them changed so the next present() resends them.
 void dirtyRows(const uint16_t *frame,int y0,int y1){for(int y=y0;y<y1;y++){shown[y*Width]=uint16_t(~frame[y*Width]);shown[y*Width+Width-1]=uint16_t(~frame[y*Width+Width-1]);}}
 void push16(const uint16_t *frame,int x,int y,int w,int h){
  window(x,y,w,h);
  // Frame pixels are already in panel byte order: whole-width runs go out in one transfer.
  if(x==0&&w==Width)SPI.writeBytes(reinterpret_cast<const uint8_t*>(frame+y*Width),size_t(Width)*h*2);
  else for(int row=y;row<y+h;row++)SPI.writeBytes(reinterpret_cast<const uint8_t*>(frame+row*Width+x),size_t(w)*2);
  end();pushes++;presentBytes+=uint32_t(w)*h*2;
 }
 bool present(const uint16_t *frame,bool full=false){
  if(asleep)return false;
  uint32_t started=micros();
  if(!shown)shown=(uint16_t*)heap_caps_malloc(Width*Height*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!shown||!shownValid||full){push16(frame,0,0,Width,Height);if(shown){memcpy(shown,frame,Width*Height*2);shownValid=true;}lastPresentUs=micros()-started;return true;}
  bool changed=false;int runStart=-1,left=Width,right=-1;
  for(int row=0;row<=Height;row++){
   int l=Width,r=-1;
   if(row<Height){const uint16_t *a=frame+row*Width,*b=shown+row*Width;if(memcmp(a,b,Width*2)){l=0;while(a[l]==b[l])l++;r=Width-1;while(a[r]==b[r])r--;}}
   if(r>=0){if(runStart<0)runStart=row;left=std::min(left,l);right=std::max(right,r);continue;}
   if(runStart>=0){push16(frame,left,runStart,right-left+1,row-runStart);for(int k=runStart;k<row;k++)memcpy(shown+k*Width+left,frame+k*Width+left,(right-left+1)*2);changed=true;runStart=-1;left=Width;right=-1;}
  }
  lastPresentUs=micros()-started;return changed;
 }
 void invalidate(){shownValid=false;}
 // Backlight off = panel in sleep mode (DISPOFF + SLPIN): no scanning, no frame writes. The panel RAM keeps the
 // last image, so "shown" stays valid; frames rendered meanwhile are dropped and the caller repaints on wake.
 // ST7789: 120 ms between SLPIN and SLPOUT in either direction, 5 ms after SLPOUT before the next command.
 bool asleep=false;uint32_t sleepChangedAt=0,sleeps=0;
 bool sleep(bool on){
  if(on==asleep)return false;
  uint32_t since=millis()-sleepChangedAt;if(since<120)delay(120-since);
  if(on){begin();command(0x28);command(0x10);end();sleeps++;}
  else{begin();command(0x11);end();delay(5);begin();command(0x29);end();}
  asleep=on;sleepChangedAt=millis();return true;
 }
 // Smooth scroll: when the new frame is the shown one moved vertically inside a band of rows (a list or the chat
 // scrolled by one step), the move is played as eased frames before the exact frame is presented. The shift is
 // found by matching row hashes (right 8 px excluded: scroll bars) and blank rows do not count as evidence.
 uint32_t scrollAnimations=0;int lastScrollShift=0;
 static uint32_t rowHash(const uint16_t *row){uint32_t h=2166136261u;const uint32_t *w=reinterpret_cast<const uint32_t*>(row);for(int i=0;i<(Width-8)/2;i++){h^=w[i];h*=16777619u;}return h;}
 bool presentScrolling(const uint16_t *frame,int frames=6){
  if(asleep)return false;
  if(!shown||!shownValid)return present(frame,true);
  int y0=-1,y1=-1;for(int y=0;y<Height;y++)if(memcmp(frame+y*Width,shown+y*Width,(Width-8)*2)){if(y0<0)y0=y;y1=y+1;}
  if(y0<0||y1-y0<24)return present(frame);
  int h=y1-y0;static uint32_t oldHash[Height],newHash[Height];uint32_t blank=0;{static uint16_t flatRow[Width];for(int i=0;i<Width;i++)flatRow[i]=frame[y0*Width];blank=rowHash(flatRow);}
  for(int y=y0;y<y1;y++){oldHash[y]=rowHash(shown+y*Width);newHash[y]=rowHash(frame+y*Width);}
  int best=0,bestCount=0;
  for(int d=-(h-8);d<=h-8;d++){if(d>-6&&d<6)continue;int count=0;for(int y=std::max(y0,y0+d);y<std::min(y1,y1+d);y++)if(newHash[y]!=blank&&newHash[y]==oldHash[y-d])count++;if(count>bestCount){bestCount=count;best=d;}}
  if(bestCount<std::max(12,h/3))return present(frame);
  // Eased intermediate frames: row y shows the content moved by s (taken from the new frame where it exists).
  for(int k=1;k<frames;k++){float t=float(k)/frames;int sft=int(best*(1-(1-t)*(1-t)*(1-t)));
   window(0,y0,Width,y1-y0);
   for(int y=y0;y<y1;y++){int fromNew=y-sft+best,fromOld=y-sft;const uint16_t *row=fromNew>=y0&&fromNew<y1?frame+fromNew*Width:fromOld>=y0&&fromOld<y1?shown+fromOld*Width:frame+y*Width;SPI.writeBytes(reinterpret_cast<const uint8_t*>(row),Width*2);}
   end();}
  dirtyRows(frame,y0,y1);
  scrollAnimations++;lastScrollShift=best;return present(frame);
 }
 // Notification card / Quick settings pane: when it appears, the band y0..y1 plays eased frames where the new content
 // comes down by `slide` px while it fades in over what the panel shows; when it leaves, the reverse (the old one fades
 // out going up over the new frame). Rows changed outside the band are sent plain first; more than a third of the
 // screen changed there (a scroll, a popup) falls back to present(). Frames are paced to frameMs: the card band takes ~170 ms, the full-height pane ~250 ms
 // (SPI ~14 ms a frame plus the blend). Pixels are RGB565 in panel byte order, blended per channel.
 uint32_t cardAnimations=0;
 static uint16_t blend565(uint16_t under,uint16_t over,int alpha){// alpha 0..256 of `over`
  under=uint16_t((under<<8)|(under>>8));over=uint16_t((over<<8)|(over>>8));const int keep=256-alpha;
  int r=((under>>11)*keep+(over>>11)*alpha)>>8,g=(((under>>5)&63)*keep+((over>>5)&63)*alpha)>>8,b=((under&31)*keep+(over&31)*alpha)>>8;
  uint16_t p=uint16_t((r<<11)|(g<<5)|b);return uint16_t((p<<8)|(p>>8));}
 bool presentCardTransition(const uint16_t *frame,int y0,int y1,bool in,int frames=8,int slide=24,uint32_t frameMs=24){
  if(asleep)return false;
  if(!shown||!shownValid)return present(frame,true);
  y0=std::max(0,y0);y1=std::min(Height,y1);const int h=y1-y0;if(h<=0)return present(frame);
  // Rows that changed outside the band (an unread badge, a list row) go out first, plain; many of them (a scroll, a
  // new page) skip the animation.
  int outside=0;for(int y=0;y<Height;y++)if((y<y0||y>=y1)&&memcmp(frame+y*Width,shown+y*Width,Width*2))outside++;
  if(outside>Height/3)return present(frame);
  for(int y=0;y<Height&&outside;y++)if((y<y0||y>=y1)&&memcmp(frame+y*Width,shown+y*Width,Width*2)){push16(frame,0,y,Width,1);memcpy(shown+y*Width,frame+y*Width,Width*2);outside--;}
  static uint16_t *band=nullptr;if(!band)band=(uint16_t*)heap_caps_malloc(Width*Height*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!band)return present(frame);
  const uint16_t *under=in?shown:frame,*card=in?frame:shown;
  for(int k=1;k<frames;k++){uint32_t started=millis();float t=float(k)/frames,e=1-(1-t)*(1-t)*(1-t);int alpha=int(256*(in?e:1-e)),shift=int(slide*(in?1-e:e));
   for(int y=y0;y<y1;y++){uint16_t *out=band+(y-y0)*Width;const uint16_t *u=under+y*Width;int from=y+shift;
    if(from>=y0&&from<y1){const uint16_t *c=card+from*Width;for(int x=0;x<Width;x++)out[x]=u[x]==c[x]?u[x]:blend565(u[x],c[x],alpha);}
    else memcpy(out,u,Width*2);}
   window(0,y0,Width,y1-y0);
   SPI.writeBytes(reinterpret_cast<const uint8_t*>(band),size_t(Width)*h*2);end();
   uint32_t spent=millis()-started;if(spent<frameMs)delay(frameMs-spent);}
  dirtyRows(frame,y0,y1);
  cardAnimations++;return present(frame);
 }
};
}
