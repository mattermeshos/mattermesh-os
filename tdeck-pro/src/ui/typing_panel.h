#pragma once
#include <GxEPD2_BW.h>
#include <gdeq/GxEPD2_310_GDEQ031T10.h>
#include <string.h>
// GDEQ031T10 / UC8253 with a "typing" partial refresh: a short waveform loaded into the LUT registers
// (PANEL SETTING REG=1, LUTC/LUTWW/LUTKW/LUTWK/LUTKK, PLL frame rate) drives only the pixels that changed
// (white<->black) for `frames` frames, then the OTP waveform is restored for the ordinary partial / full
// refreshes. Used for the chat composer while the user types: the letter shows in ~100-200 ms instead of
// the ~700 ms OTP partial; the ordinary partial repainted after a typing pause cleans the lighter ink.
// The driver keeps _setPartialRamArea/_PowerOn private, so the same command sequences are issued here.
class TypingPanel : public GxEPD2_310_GDEQ031T10 {
public:
 using GxEPD2_310_GDEQ031T10::GxEPD2_310_GDEQ031T10;
 // Tuned on the device with the user (2026-10-19): 48 frames at 100 Hz (~480 ms waveform) with the OTP power setting
 // give readable ink; 8-32 frames were too pale, forcing VSH/VSL to 15 V was declined ("cosi stiamo tranquilli").
 uint8_t typingFrames=48,typingPll=0x13/* 100 Hz */,typingRepeat=1,typingVsh=0/* 0 = OTP power setting; 1..63 = VSH/VSL level (63 = +-15 V) */;uint16_t typingWaitMs=900;
 uint32_t typingRefreshes=0,lastTypingBusyMs=0;
 void refreshTyping(int16_t x,int16_t y,int16_t w,int16_t h){
  if(_initial_refresh){refresh(false);return;}
  int16_t w1=x<0?w+x:w,h1=y<0?h+y:h,x1=x<0?0:x,y1=y<0?0:y;
  w1=x1+w1<int16_t(WIDTH)?w1:int16_t(WIDTH)-x1;h1=y1+h1<int16_t(HEIGHT)?h1:int16_t(HEIGHT)-y1;
  if(w1<=0||h1<=0)return;
  w1+=x1%8;if(w1%8>0)w1+=8-w1%8;x1-=x1%8;
  _writeCommand(0x91);partialWindow(x1,y1,w1,h1);
  loadTypingLut();
  _writeCommand(0x50);_writeData(0xD7);// VCOM / data interval as the OTP partial
  if(!_power_is_on){_writeCommand(0x04);_waitWhileBusy("_PowerOn",power_on_time);}_power_is_on=true;
  uint32_t started=millis();_writeCommand(0x12);_waitWhileBusy("_Update_Typing",typingWaitMs);lastTypingBusyMs=millis()-started;typingRefreshes++;
  _writeCommand(0x00);_writeData(0x1f);_writeData(0x0d);// back to the OTP LUT
  _writeCommand(0x30);_writeData(0x09);// PLL default (50 Hz)
  _writeCommand(0x92);
  _init_display_done=false;// as the driver does after every refresh
 }
private:
 void partialWindow(uint16_t x,uint16_t y,uint16_t w,uint16_t h){uint16_t xe=(x+w-1)|0x0007,ye=y+h-1;x&=0xFFF8;_writeCommand(0x90);_writeData(x);_writeData(xe);_writeData(y/256);_writeData(y%256);_writeData(ye/256);_writeData(ye%256);_writeData(0x01);}
 // One group: `frames` frames at `level` in state 1 phase 1, nothing else. level: 0 = 0 V / VCOM_DC, 1 = VSH (ink), 2 = VSL (erase).
 void lut(uint8_t command,int groups,uint8_t level){uint8_t data[56];memset(data,0,sizeof(data));data[0]=typingRepeat;data[1]=uint8_t((level<<6)|(typingFrames&63));data[5]=typingRepeat;_writeCommand(command);_writeData(data,groups*7);}
 void loadTypingLut(){
  _writeCommand(0x00);_writeData(0x3f);_writeData(0x0d);// PANEL SETTING: REG=1 (LUT from registers), KW, scan/shl as the driver init
  _writeCommand(0x30);_writeData(typingPll);
  if(typingVsh){_writeCommand(0x01);_writeData(0x03);_writeData(0x10);_writeData(typingVsh&63);_writeData(typingVsh&63);_writeData(0x0D);}// PWR: internal DC/DC, VGH 20 V, VSH/VSL level, VDHR default
  lut(0x20,8,0);// VCOM: VCOM_DC during the drive
  lut(0x21,6,0);// W2W: no drive
  lut(0x22,6,2);// K2W (erase): VSL, as the UC8151 register partial LUTs of GxEPD2
  lut(0x23,6,1);// W2K (ink): VSH
  lut(0x24,6,0);// K2K: no drive
 }
};
