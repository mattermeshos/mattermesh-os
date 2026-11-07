#pragma once
// Emulated ST7789 panel (CASET/RASET/RAMWR, MADCTL, sleep/display on), shared by the T-Deck Plus and the ThinkNode M9.
#include <emu_hal.h>
#include <algorithm>
#include <cstdint>
#include <vector>
struct St7789:emu::SpiDevice {
 static constexpr int Native=320;// panel 240x320; MV (MADCTL bit 5) gives the 320x240 landscape the firmware uses
 std::vector<uint16_t> fb=std::vector<uint16_t>(320*240,0);
 bool dc=true,selected=false,sleeping=true,on=false,dirty=true;uint8_t madctl=0,cmd=0;uint8_t params[8];int nparams=0;
 int xs=0,xe=319,ys=0,ye=239,x=0,y=0;bool hiByte=true;uint8_t hi=0;
 int width() const {return madctl&0x20?320:240;}int height() const {return madctl&0x20?240:320;}
 void reset(){sleeping=true;on=false;madctl=0;xs=ys=0;xe=width()-1;ye=height()-1;dirty=true;}
 void select(bool s)override{selected=s;}
 void command(uint8_t c){cmd=c;nparams=0;
  switch(c){case 0x01:reset();break;case 0x10:sleeping=true;dirty=true;break;case 0x11:sleeping=false;dirty=true;break;
   case 0x28:on=false;dirty=true;break;case 0x29:on=true;dirty=true;break;case 0x2C:x=xs;y=ys;hiByte=true;break;}}
 void pixel(uint16_t v){int w=width(),h=height();if(x>=0&&x<w&&y>=0&&y<h){fb[size_t(y)*w+x]=v;dirty=true;}
  if(++x>xe){x=xs;if(++y>ye)y=ys;}}
 void data(uint8_t b){
  if(cmd==0x2C){if(hiByte){hi=b;hiByte=false;}else{pixel(uint16_t(hi<<8|b));hiByte=true;}return;}
  if(nparams<int(sizeof(params)))params[nparams++]=b;
  if(cmd==0x2A&&nparams==4){xs=params[0]<<8|params[1];xe=params[2]<<8|params[3];}
  else if(cmd==0x2B&&nparams==4){ys=params[0]<<8|params[1];ye=params[2]<<8|params[3];}
  else if(cmd==0x36&&nparams==1){bool rotated=(madctl^b)&0x20;madctl=b;if(rotated){std::fill(fb.begin(),fb.end(),0);dirty=true;}}
 }
 uint8_t transfer(uint8_t b)override{if(!dc)command(b);else data(b);return 0;}
 void write(const uint8_t *d,size_t n)override{
  if(!dc||cmd!=0x2C){for(size_t i=0;i<n;i++)transfer(d[i]);return;}
  size_t i=0;if(!hiByte&&n){pixel(uint16_t(hi<<8|d[0]));hiByte=true;i=1;}
  for(;i+1<n;i+=2)pixel(uint16_t(d[i]<<8|d[i+1]));
  if(i<n){hi=d[i];hiByte=false;}
 }
};
