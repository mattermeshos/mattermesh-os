#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <emu_hal.h>
#include "devices/max_devices.h"
#include "ui/typing_panel.h"
#include <Adafruit_TCA8418.h>
#include <ExtensionIOXL9555.hpp>
#include <HynTouch.h>
#include <esp_codec.h>
#include <cassert>
#include <cstdio>
#include <vector>

static uint8_t reg(int address,int a){Wire.beginTransmission(address);Wire.write(a);assert(Wire.endTransmission()==0);assert(Wire.requestFrom(address,size_t(1))==1);return Wire.read();}
static void put(int address,int a,int v){Wire.beginTransmission(address);Wire.write(a);Wire.write(v);assert(Wire.endTransmission()==0);}
static bool allWhite(){const auto *p=maxdev::frame();for(int i=0;i<240*320;i++)if(p[i]!=65535)return false;return true;}
int main(){
 maxdev::attach("max-device-tests",0);Wire.begin(13,14);SPI.begin(36,47,33);
 TypingPanel panel(34,35,9,37);panel.init(0,true,2,false);
 std::vector<uint16_t> old(maxdev::frame(),maxdev::frame()+240*320);bool checkBusy=true;unsigned busySamples=0;
 emu::addPumpHook([&]{if(checkBusy&&digitalRead(37)==LOW){assert(std::equal(old.begin(),old.end(),maxdev::frame()));++busySamples;}});
 assert(allWhite());uint8_t black=0;
 panel.writeScreenBuffer(); // real initial full refresh
 panel.writeImage(&black,0,0,8,1);
 assert(allWhite());auto t=millis();panel.refresh(0,0,8,1);
 assert(millis()-t>=650&&millis()-t<1200);assert(maxdev::frame()[0]==0&&maxdev::frame()[7]==0&&maxdev::frame()[8]==65535&&maxdev::frame()[240]==65535);
 puts("PASS real GxEPD2 RAM writes remain invisible until active-low BUSY completes; partial window and 650 ms timing");
 old.assign(maxdev::frame(),maxdev::frame()+240*320);
 uint8_t pattern=0x55;panel.writeImage(&pattern,8,1,8,1);t=millis();panel.refresh(false);
 assert(millis()-t>=1015&&millis()-t<1600);assert(maxdev::frame()[248]==0&&maxdev::frame()[249]==65535);
 puts("PASS full refresh commits controller RAM after 1015 ms");
 old.assign(maxdev::frame(),maxdev::frame()+240*320);
 panel.writeImage(&black,16,2,8,1);t=millis();panel.refreshTyping(16,2,8,1);
 assert(millis()-t>=480&&millis()-t<1000);assert(maxdev::frame()[496]==0);assert(panel.lastTypingBusyMs>=480);
 puts("PASS real TypingPanel register LUT/PLL uses 480 ms waveform and completes without timeout");
 old.assign(maxdev::frame(),maxdev::frame()+240*320);panel.hibernate();assert(std::equal(old.begin(),old.end(),maxdev::frame()));
 digitalWrite(9,LOW);digitalWrite(9,HIGH);assert(std::equal(old.begin(),old.end(),maxdev::frame()));assert(digitalRead(37)==HIGH);assert(busySamples>10);checkBusy=false;
 puts("PASS sleep/reset retains physical ink");
 Adafruit_TCA8418 keyboard;assert(keyboard.begin(0x34,&Wire));assert(keyboard.matrix(4,10));keyboard.enableInterrupts();keyboard.flush();
 maxdev::key(21,true);maxdev::key(21,true);maxdev::key(21,false);assert(keyboard.available()==2);assert(digitalRead(15)==LOW);assert(keyboard.getEvent()==149);assert(keyboard.getEvent()==21);assert(keyboard.available()==0);put(0x34,2,1);assert(digitalRead(15)==HIGH);
 for(int i=1;i<=11;i++)maxdev::key(i,true);assert(keyboard.available()==10);assert(reg(0x34,2)&8);keyboard.flush();
 puts("PASS real TCA8418 matrix config, FIFO press/release, duplicate suppression, latched IRQ/W1C and overflow");
 ExtensionIOXL9555 io;assert(io.init(Wire,13,14,0x20));io.pinMode(7,OUTPUT);io.digitalWrite(7,LOW);Wire.beginTransmission(0x1a);assert(Wire.endTransmission()!=0);io.digitalWrite(7,HIGH);hyn_touch_attach_xl9555(&io);assert(hyn_touch_init());
 int16_t x=0,y=0;maxdev::touch(123,234,true);assert(hyn_touch_get_point(&x,&y,1)==1);assert(hyn_touch_get_contact_point(&x,&y)&&x==123&&y==234);maxdev::touch(123,234,false);hyn_touch_get_point(&x,&y,1);assert(!hyn_touch_get_contact_point(&x,&y));
 io.pinMode(9,OUTPUT);io.digitalWrite(9,LOW);assert(keyboard.available()==0);io.digitalWrite(9,HIGH);
 io.pinMode(1,OUTPUT);
 auto packetType=[](){digitalWrite(3,LOW);SPI.transfer(0x11);SPI.transfer(0);int v=SPI.transfer(0);digitalWrite(3,HIGH);return v;};
 io.digitalWrite(1,LOW);assert(packetType()==0);io.digitalWrite(1,HIGH);assert(packetType()==1);
 puts("PASS XL9555 LoRa power gating and radio reset on power cycle");
 puts("PASS real HynTouch probes CST328, portrait coordinates/release, XL9555 resets");
 assert(reg(0x55,0x2c)==82);assert((reg(0x55,8)|(reg(0x55,9)<<8))==3920);assert(reg(0x6a,0x0b)==0);maxdev::usbPower(true);assert((reg(0x6a,0x0b)>>3&3)==2);assert(reg(0x55,0x0a)==0x28);assert(int16_t(reg(0x55,0x0c)|(reg(0x55,0x0d)<<8))==450);
 analogWrite(42,137);assert(maxdev::keyboardLight()==137);put(0x5a,1,5);put(0x5a,2,65);assert(reg(0x5a,2)==65);
 puts("PASS fuel gauge, USB/charging toggle, keyboard PWM and haptic RTP registers");
 EspCodec codec;codec.setPaParams(-1,3.3);codec.setPins(38,39,18,40,17);assert(codec.begin(Wire,0x18,CODEC_TYPE_ES8311));assert(codec.open(16,2,16000)==0);codec.setVolume(20);codec.setMute(true);assert(codec.setOutputMute(false)==0);uint8_t pcm[640]{};assert(codec.write(pcm,sizeof(pcm))==0);assert(codec.read(pcm,sizeof(pcm))==0);codec.close();
 puts("PASS real esp_codec_dev/ES8311 initializes, opens, mutes, discards PCM playback and captures silence");
 puts("PASS all Max device tests");fflush(stdout);return 0;
}
