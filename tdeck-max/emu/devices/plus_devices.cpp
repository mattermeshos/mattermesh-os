// Emulated T-Deck Plus chips. Each model follows the protocol the firmware's driver speaks:
//  ST7789   SPI CS 12, D/C 11: SWRESET/SLPOUT/DISPON/MADCTL/CASET/RASET/RAMWR (RGB565, high byte first)
//  ESP32-C3 keyboard I2C 0x55: 0x03 raw mode (5 column bytes, one bit per row), 0x04 char mode, 0x01 light
//  GT911    I2C 0x5D: product id at 0x8140, status 0x814E, points from 0x814F, cleared by writing 0x814E
//  trackball G01..G04 = GPIO 3/2/15/1 toggled per detent, centre press = BOOT GPIO 0 (active low)
//  backlight GPIO42 pulse dimming (16 levels), battery on GPIO4 through the 1:2 divider
#include "plus_devices.h"
#include "sx1262.h"
#include "air/air_client.h"
#include <Arduino.h>
#include <emu_hal.h>
#include <algorithm>
#include <cstring>
#include <set>
#include <vector>
namespace plusdev {
namespace {
constexpr int PinTftCs=12,PinTftDc=11,PinBacklight=42,PinTouchInt=16,PinBoot=0,PinBattery=4;
constexpr int PinUp=3,PinRight=2,PinDown=15,PinLeft=1;

// ---- ST7789 ----
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
} lcd;

// ---- backlight: HIGH after a long LOW = full brightness, each short LOW pulse steps one level down ----
struct Backlight {int level=0;bool high=false;uint64_t lowSince=0;
 void write(int v){uint64_t now=emu::micros64();
  if(!v){if(high){high=false;lowSince=now;}return;}
  if(high)return;high=true;
  if(level==0||now-lowSince>=2500)level=16;else level=level>1?level-1:16;}
 int current(){if(!high&&emu::micros64()-lowSince>=2500)level=0;return level;}
} backlightLine;

// ---- ESP32-C3 keyboard ----
struct KeyboardC3:emu::I2cDevice {
 static constexpr uint8_t codes[5][7]={{10,9,32,20,30,33,34},{8,19,18,1,28,29,35},{7,16,6,31,26,27,17},{4,15,5,21,25,24,14},{2,12,3,11,22,23,13}};
 bool raw=false;int light=0;std::set<int> held;std::vector<char> typed;
 bool write(const uint8_t *d,size_t n)override{
  if(!n)return true;if(d[0]==0x03)raw=true;else if(d[0]==0x04)raw=false;else if(d[0]==0x01&&n>=2)light=d[1];return true;}
 size_t read(uint8_t *d,size_t n)override{
  if(raw){uint8_t cols[5]={};for(int c=0;c<5;c++)for(int r=0;r<7;r++)if(held.count(codes[c][r]))cols[c]|=uint8_t(1<<r);size_t k=std::min<size_t>(n,5);memcpy(d,cols,k);return k;}
  if(!n)return 0;d[0]=0;if(!typed.empty()){d[0]=uint8_t(typed.front());typed.erase(typed.begin());}return 1;}
} keyboardChip;

// ---- GT911 ----
struct Gt911:emu::I2cDevice {
 uint8_t regs[0x200]={};uint16_t pointer=0x8140;bool down=false,ready=false;int rawX=0,rawY=0;uint8_t track=0;
 Gt911(){const char id[4]={'9','1','1',0};memcpy(&regs[0x140],id,4);regs[0x144]=0x60;regs[0x145]=0x10;// firmware 0x1060
  regs[0x146]=240&0xFF;regs[0x147]=240>>8;regs[0x148]=320&0xFF;regs[0x149]=320>>8;regs[0x14A]=0x00;// vendor
  regs[0x047]=0x41;regs[0x048]=240&0xFF;regs[0x049]=240>>8;regs[0x04A]=320&0xFF;regs[0x04B]=320>>8;regs[0x04C]=5;}
 uint8_t *reg(uint16_t a){return a>=0x8000&&a<0x8200?&regs[a-0x8000]:nullptr;}
 void refresh(){regs[0x14E]=uint8_t((ready?0x80:0)|(down?1:0));uint8_t *p=&regs[0x14F];memset(p,0,8);
  if(down){p[0]=track;p[1]=uint8_t(rawX);p[2]=uint8_t(rawX>>8);p[3]=uint8_t(rawY);p[4]=uint8_t(rawY>>8);p[5]=24;}}
 bool write(const uint8_t *d,size_t n)override{
  if(n<2)return true;pointer=uint16_t(d[0]<<8|d[1]);
  for(size_t i=2;i<n;i++){uint16_t a=uint16_t(pointer+i-2);if(a==0x814E){ready=false;refresh();continue;}if(auto *r=reg(a))*r=d[i];}
  return true;}
 size_t read(uint8_t *d,size_t n)override{refresh();for(size_t i=0;i<n;i++){auto *r=reg(uint16_t(pointer+i));d[i]=r?*r:0;}pointer=uint16_t(pointer+n);return n;}
 // The panel is mounted portrait (240x320, origin at the top right of the landscape view): the firmware's
 // setSwapXY(true) + setMirrorXY(false,true) with max 320x240 turns it back into screen coordinates.
 void set(int x,int y,bool d){if(d&&!down)track++;down=d;rawX=std::clamp(240-y,0,240);rawY=std::clamp(x,0,319);ready=true;refresh();
  emu::gpioDrive(PinTouchInt,LOW);emu::gpioDrive(PinTouchInt,HIGH);}
} touchChip;

bool onUsb=false;
// SX1262 (Codex, emu/devices/sx1262.cpp) on the board pins: CS 9, BUSY 13, RST 17, DIO1 45.
constexpr int PinLoraCs=9,PinLoraBusy=13,PinLoraRst=17,PinLoraDio1=45;
Sx1262Model radioChip;std::unique_ptr<NetAir> air;
}

void attach(const char *airName,int airPort){
 if(airPort>0)air.reset(new NetAir(airName,uint16_t(airPort)));
 radioChip.attach(PinLoraCs,PinLoraBusy,PinLoraRst,PinLoraDio1,air.get());
 emu::spiAttach(PinTftCs,&lcd);
 emu::onGpioWrite(PinTftDc,[](int level){lcd.dc=level!=0;});
 emu::onGpioWrite(PinBacklight,[](int level){backlightLine.write(level);lcd.dirty=true;});
 emu::i2cAttach(0x55,&keyboardChip);
 emu::i2cAttach(0x5D,&touchChip);
 for(int pin:{PinUp,PinRight,PinDown,PinLeft,PinBoot,PinTouchInt})emu::gpioDrive(pin,HIGH);
 usbPower(false);
}
const uint16_t *frame(){return lcd.fb.data();}
int frameWidth(){return lcd.width();}
int frameHeight(){return lcd.height();}
bool frameDirty(bool clear){bool d=lcd.dirty;if(clear)lcd.dirty=false;return d;}
bool displayOn(){return lcd.on&&!lcd.sleeping;}
int backlight(){return backlightLine.current();}
void key(int code,bool pressed){if(pressed)keyboardChip.held.insert(code);else keyboardChip.held.erase(code);}
void touch(int x,int y,bool down){touchChip.set(x,y,down);}
// One unit = one detent movement: three line changes, as main.cpp counts TrackballStep edges per move.
void roll(int dx,int dy){
 auto toggle=[](int pin,int edges){for(int i=0;i<edges;i++)emu::gpioDrive(pin,emu::gpioLevel(pin)?LOW:HIGH);};
 if(dy<0)toggle(PinUp,-3*dy);if(dy>0)toggle(PinDown,3*dy);if(dx<0)toggle(PinLeft,-3*dx);if(dx>0)toggle(PinRight,3*dx);
}
void press(bool down){emu::gpioDrive(PinBoot,down?LOW:HIGH);}
void usbPower(bool on){onUsb=on;emu::setAnalogMilliVolts(PinBattery,on?2330:1960);}
bool usbPowered(){return onUsb;}
int keyboardLight(){return keyboardChip.light;}
}
