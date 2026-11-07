#pragma once
// ThinkNode M9 keyboard: an MCU on I2C (0x6C, 0x6D on later boards) raises GPIO12 on a key press and gives one code per
// press from register 0x01 (Meshtastic kbI2cBase.cpp, 0x12 case); nothing is sent while a key is held. ASCII codes become
// the T-Deck key events of the character path, so the shared keyboard code runs unchanged; the D-pad feeds the trackball
// counters, one press = one move. Elecrow documents an ESP32-S2 and Meshtastic an STC8H as this MCU, and Meshtastic's own
// comment says register 0x05 while its code reads 0x01: the code is followed here, to confirm on a real M9.
#include <Arduino.h>
#include <Wire.h>
#include "m9_board.h"
namespace hw {
enum class KeyAction:uint8_t{None,Unlock,Home,Gps,Advert,QuickSettings,Chats,Back,Map,Hold,Menu,Compose};
static volatile int16_t trackballX=0,trackballY=0;
static volatile uint16_t m9KeyInterrupts=0;
static void IRAM_ATTR m9KeyIsr(){m9KeyInterrupts++;}
constexpr int M9TrackballStep=3;
constexpr uint32_t M9PadOkGhostMs=250;// the OK the pad sends after a direction comes within this time// main.cpp TrackballStep: three line changes make one move
class Keyboard {
 uint8_t address=BOARD_I2C_ADDR_KEYBOARD;uint32_t padAt=0;uint8_t queue[32];uint8_t head=0,count=0;uint16_t handled=0;KeyAction action=KeyAction::None;
 void push(uint8_t event){if(count<sizeof(queue)){queue[(head+count)%sizeof(queue)]=event;count++;}else overflow=true;}
 static int codeFor(char c,bool &shift,bool &sym){
  static const char letters[]="poiuytrewq\blkjhgfdsa\n$mnbvcxz";// codes 1..29 in order, as keys[][] in main.cpp
  static const char symbolKeys[]="@+-_)(321#\b\"';:/654*\n0.,!?987";
  shift=sym=false;if(c==' ')return 33;if(c=='\r'||c=='\n')return 21;if(c=='\b')return 11;
  for(int i=0;letters[i];i++)if(letters[i]==c)return i+1;
  if(c>='A'&&c<='Z'){int code=codeFor(char(c-'A'+'a'),shift,sym);shift=true;return code;}
  for(int i=0;symbolKeys[i];i++)if(symbolKeys[i]==c){sym=true;return i+1;}
  return 0;
 }
 // A character with no T-Deck key (& % = < > [ ] { } ~ ^ | ...: the M9 controller sends the final character) goes to
 // the text fields as it is; it used to be dropped, so a Wi-Fi password with one of them never matched.
 char direct[16];uint8_t directCount=0;
 void pushChar(char c){bool shift,sym;int code=codeFor(c,shift,sym);if(!code){if(c>=32&&c<=126&&directCount<sizeof(direct))direct[directCount++]=c;return;}
  if(shift)push(0x80|35);if(sym)push(0x80|32);push(uint8_t(0x80|code));push(uint8_t(code));if(sym)push(32);if(shift)push(35);}
 void route(uint8_t c){
  if(trace)Serial.printf("M9_KEY code=0x%02X\n",c);// hardware bring-up: every raw code ("kbd trace on")
  switch(c){
   case 0xB5:padAt=millis();trackballY-=M9TrackballStep;break;case 0xB6:padAt=millis();trackballY+=M9TrackballStep;break;
   case 0xB4:padAt=millis();trackballX-=M9TrackballStep;break;case 0xB7:padAt=millis();trackballX+=M9TrackballStep;break;
   // Function keys of the real M9 (codes seen on a V2 board 2026-11-02; names from the keyboard controller's tables as
   // documented by the wadamesh port): MSG 0x81, HOME 0x82, @ 0x83, ADV 0x84 (held 0x87), MAP 0x85, BACK 0x86,
   // triangle 0x88, CTRL 0x90, OK held 0xA3.
   case 0x81:action=KeyAction::Chats;break;case 0x83:action=KeyAction::Compose;break;// big bubble: chats; small message box: new chat / channel
   case 0x82:action=KeyAction::Home;break;
   case 0x84:action=KeyAction::Advert;break;case 0x87:action=KeyAction::Gps;break;case 0x85:action=KeyAction::Map;break;
   case 0x86:action=KeyAction::Back;break;case 0x88:action=KeyAction::Menu;break;case 0x90:action=KeyAction::QuickSettings;break;
   case 0xA3:action=KeyAction::Unlock;break;// OK held: unlocks the lock screen, elsewhere the hold action
   // A real M9 sends an OK (0x0D) right after each D-pad direction (seen 2026-11-02 on every arrow): that one is not a
   // press of the centre key, or every move would also open the row it reached.
   case 0x0D:if(uint32_t(millis()-padAt)<M9PadOkGhostMs){if(trace)Serial.println("M9_KEY ok-after-arrow ignored");break;}pushChar('\n');break;case 0x08:case 0x89:pushChar('\b');break;
   case 0x02:if(trace)Serial.println("M9_KEY 0x02 ignored");break;/* sent by the controller on its own (from boot, about every minute on a real M9, 2026-11-03): not a key */
   default:if(c>=0x20&&c<0x7F)pushChar(char(c));else if(c)Serial.printf("M9_KEY unknown=0x%02X\n",c);
  }
 }
public:
 bool raw=false,overflow=false,confirmed=false,trace=false;uint32_t polls=0,errors=0,stuckReads=0,idleReads=0,idleHits=0,readAt=0;
 uint16_t irqCount()const{return m9KeyInterrupts;}uint16_t handledCount()const{return handled;}
 bool begin(uint8_t,TwoWire*){
  bool found=false;for(uint8_t a:{uint8_t(0x6C),uint8_t(0x6D)}){Wire.beginTransmission(a);if(Wire.endTransmission()==0){address=a;found=true;break;}}
  if(!found)return false;
  boardV2()=address==0x6D;keyboardAddress()=address;
  // Long press time of the keyboard MCU, register 0x03 (Meshtastic initVariant).
  Wire.beginTransmission(address);Wire.write(0x03);Wire.write(uint8_t(700>>8));Wire.write(uint8_t(700&0xFF));Wire.endTransmission();
  pinMode(BOARD_KEYBOARD_INT,INPUT);attachInterrupt(digitalPinToInterrupt(BOARD_KEYBOARD_INT),m9KeyIsr,RISING);
  Serial.printf("M9_KEYBOARD ready=1 address=0x%02X\n",address);return true;
 }
 bool matrix(uint8_t,uint8_t){return true;}
 void enableDebounce(){}void enableInterrupts(){}void flush(){count=0;}
 KeyAction takeAction(){KeyAction a=action;action=KeyAction::None;return a;}
 bool poll(){
  if(count)return true;polls++;
  // A press raises GPIO12 until its code is read. Edges alone are not enough: a read that fails (the 0xFF seen at boot on a
  // real M9) leaves the line high with no new edge, and the keyboard went silent for good. A line still high is read too.
  bool waiting=handled!=m9KeyInterrupts,stuck=!waiting&&digitalRead(BOARD_KEYBOARD_INT)==HIGH&&uint32_t(millis()-readAt)>20;
  // And every 40 ms anyway: on a real M9 the line stopped rising after a lock / unlock while presses went on (2026-11-02);
  // a read with nothing queued gives 0 and costs one short I2C transfer.
  bool idleRead=!waiting&&!stuck&&uint32_t(millis()-readAt)>=40;
  if(!waiting&&!stuck&&!idleRead)return false;if(waiting)handled++;else if(stuck)stuckReads++;else idleReads++;readAt=millis();
  Wire.beginTransmission(address);Wire.write(0x01);
  if(Wire.endTransmission(false)!=0||Wire.requestFrom(address,uint8_t(1))!=1){errors++;return false;}
  uint8_t code=uint8_t(Wire.read());if(idleRead&&code&&code!=0xFF)idleHits++;if(idleRead&&(code==0||code==0xFF))return false;/* nothing queued */
  route(code);return count>0||directCount>0;
 }
 /* serial "pad u|d|l|r|ok" (remote tests): a D-pad code through the same route as the keyboard MCU */
 void inject(uint8_t code){route(code);}
 uint8_t readRegister(uint8_t){poll();uint8_t status=count?1:0;if(overflow){status|=8;overflow=false;}return status;}
 void writeRegister(uint8_t,uint8_t){}
 int available(){return count;}
 int takeChar(){if(!directCount)return 0;char c=direct[0];memmove(direct,direct+1,--directCount);return c;}
 int getEvent(){if(!count)return 0;uint8_t e=queue[head];head=(head+1)%sizeof(queue);count--;return e;}
};
struct Trackball {
 static void take(int &x,int &y){noInterrupts();x=trackballX;y=trackballY;trackballX=trackballY=0;interrupts();}
};
}
