// Emulated ThinkNode M9 chips, same interface as plus_devices.cpp so the window and the scripted inputs work unchanged:
//  ST7789 SPI CS 16, D/C 15; backlight PWM on GPIO17, active low; battery on GPIO13 through the 1:2 divider
//  keyboard MCU I2C 0x6D (V2 board, as on the real M9 of 2026-11-02): GPIO12 rises per press, one code from register 0x01
//  (ASCII, D-pad 0xB4..0xB7 each followed by the OK 0x0D the real pad sends, MIC 0x81, Home 0x82)
//  PCF8563 RTC I2C 0x51: starts stopped (VL set) like a fresh cell, keeps counting once written
//  LR1110 CS 39, BUSY 41, RST 45, IRQ 42 on emu-air. No touch, no trackball, no codec.
#include "m9_devices.h"
#include "st7789.h"
#include "lr1110.h"
#include "air/air_client.h"
#include <Arduino.h>
#include <WiFi.h>
#include <emu_hal.h>
#include <ctime>
#include <deque>
#include <memory>
namespace m9dev {
namespace {
constexpr int PinTftCs=16,PinTftDc=15,PinBacklight=17,PinKeyInt=12,PinBattery=13;
St7789 lcd;int backlightValue=255;int ledLevel=0;bool onUsb=false,shift=false,sym=false;
struct KeyChip:emu::I2cDevice{
 uint8_t reg=0;std::deque<uint8_t> codes;
 bool write(const uint8_t *d,size_t n)override{if(n)reg=d[0];if(n>1&&reg==2)ledLevel=d[1];/* keyboard light duty */return true;}
 size_t read(uint8_t *d,size_t n)override{if(!n)return 0;d[0]=0;if(reg==1&&!codes.empty()){d[0]=codes.front();codes.pop_front();}return 1;}
 void press(uint8_t code){codes.push_back(code);emu::gpioDrive(PinKeyInt,HIGH);emu::gpioDrive(PinKeyInt,LOW);}
} keys;
// PCF8563: registers 0x02-0x08 (BCD seconds..years); a write sets the time and clears VL, reads give it advanced.
struct RtcChip:emu::I2cDevice{
 uint8_t reg=0;bool valid=false;int64_t base=0;uint32_t setAt=0;
 static uint8_t bcd(int v){return uint8_t((v/10)<<4|(v%10));}static int dec(uint8_t v){return (v>>4)*10+(v&15);}
 bool write(const uint8_t *d,size_t n)override{if(!n)return true;reg=d[0];if(n>=8&&reg==2){struct tm t={};t.tm_sec=dec(d[1]&0x7F);t.tm_min=dec(d[2]);t.tm_hour=dec(d[3]);t.tm_mday=dec(d[4]);t.tm_mon=dec(d[6]&0x1F)-1;t.tm_year=dec(d[7])+100;base=int64_t(timegm(&t));setAt=emu::millis32();valid=true;}return true;}
 size_t read(uint8_t *d,size_t n)override{time_t now=time_t(base+(emu::millis32()-setAt)/1000);struct tm t;gmtime_r(&now,&t);uint8_t r[7]={uint8_t(bcd(t.tm_sec)|(valid?0:0x80)),bcd(t.tm_min),bcd(t.tm_hour),bcd(t.tm_mday),uint8_t(t.tm_wday),bcd(t.tm_mon+1),bcd(t.tm_year-100)};
  for(size_t i=0;i<n;i++)d[i]=reg>=2&&reg-2+i<7?r[reg-2+i]:0;return n;}
} rtcChip;
Lr1110Model radioChip;std::unique_ptr<NetAir> air;
// The T-Deck key codes the window and the scripts send become the characters the M9 keyboard reports.
char capOf(int code){static const char letters[]="poiuytrewq\blkjhgfdsa\n$mnbvcxz",symbols[]="@+-_)(321#\b\"';:/654*\n0.,!?987";
 if(code<1||code>29)return 0;return sym?symbols[code-1]:letters[code-1];}
}
// GPIO13 is on ADC2: nothing readable while Wi-Fi is on.
static void battery(){emu::setAnalogMilliVolts(PinBattery,WiFi.getMode()==WIFI_OFF?(onUsb?2150:1960):0);}
void attach(const char *airName,int airPort){
 if(airPort>0)air.reset(new NetAir(airName,uint16_t(airPort)));
 radioChip.attach(39,41,45,42,air.get());
 emu::spiAttach(PinTftCs,&lcd);
 emu::onGpioWrite(PinTftDc,[](int level){lcd.dc=level!=0;});
 emu::onAnalogWrite(PinBacklight,[](int value){backlightValue=value;lcd.dirty=true;});
 emu::i2cAttach(0x6D,&keys);emu::i2cAttach(0x51,&rtcChip);
 emu::gpioDrive(PinKeyInt,LOW);usbPower(false);emu::addPumpHook(battery);
}
const uint16_t *frame(){return lcd.fb.data();}
int frameWidth(){return lcd.width();}
int frameHeight(){return lcd.height();}
bool frameDirty(bool clear){bool d=lcd.dirty;if(clear)lcd.dirty=false;return d;}
bool displayOn(){return lcd.on&&!lcd.sleeping;}
int backlight(){return (255-backlightValue)*16/255;}
void key(int code,bool pressed){
 if(code>=300){if(pressed)keys.press(uint8_t(code-300));return;}// script: "tap 3xx" sends the raw M9 byte xx-300 (e.g. 338 = '&')
 if(code==35||code==31){shift=pressed;return;}if(code==32){sym=pressed;return;}if(!pressed||code==30)return;
 // Function keys (real M9 codes): 34 OK held, 36 HOME, 37 MSG, 38 ADV held (GPS), 39 ADV, 40 CTRL, 41 BACK, 42 triangle,
 // 43 MAP, 44 @
 static const struct{int code;uint8_t mcu;}row[]={{34,0xA3},{36,0x82},{37,0x81},{38,0x87},{39,0x84},{40,0x90},{41,0x86},{42,0x88},{43,0x85},{44,0x83}};
 for(auto &r:row)if(r.code==code){keys.press(r.mcu);return;}
 if(code==33){keys.press(0x20);return;}if(code==21){keys.press(0x0D);return;}if(code==11){keys.press(0x08);return;}
 char c=capOf(code);if(!c)return;if(shift&&c>='a'&&c<='z')c=char(c-'a'+'A');keys.press(uint8_t(c));
}
void touch(int,int,bool){}// no touch panel
// Each D-pad direction comes with the OK the real pad sends right after it (the firmware drops that one).
void roll(int dx,int dy){auto dir=[](uint8_t c){keys.press(c);keys.press(0x0D);};for(;dy<0;dy++)dir(0xB5);for(;dy>0;dy--)dir(0xB6);for(;dx<0;dx++)dir(0xB4);for(;dx>0;dx--)dir(0xB7);}
void press(bool down){if(down)keys.press(0x0D);}// OK, the centre of the D-pad
void usbPower(bool on){onUsb=on;emu::gpioDrive(1,on?LOW:HIGH);battery();}
bool usbPowered(){return onUsb;}
int keyboardLight(){return ledLevel;}
}
void tone(uint8_t,unsigned int hz,unsigned long){static unsigned long tones=0;if(!(tones++%8)){printf("EMU_BUZZER hz=%u\n",hz);fflush(stdout);}}
void noTone(uint8_t){}
