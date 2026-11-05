// Emulator window (SDL2): shows the LCD, turns the PC keyboard / mouse into the T-Deck Plus inputs.
//  letters, Backspace, Enter, Space   the same keys          Left/Right Shift  Shift keys (codes 35 / 31)
//  Left Alt                           Alt (30)               Left Ctrl         Sym (32)        F3  microphone key (34)
//  digits and punctuation             Sym + the key that carries them on the T-Deck ("$" is its own key)
//  arrows / mouse wheel               trackball rolls        Tab / middle click  trackball press
//  left mouse button                  touch                  F5  battery <-> USB power         F12 screenshot (dataDir)
#include "window.h"
#include "board_devices.h"
#include <Arduino.h>
#include <emu_hal.h>
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <string>
namespace window {
namespace {
SDL_Window *win=nullptr;SDL_Renderer *ren=nullptr;SDL_Texture *tex=nullptr;int scale=3,texW=0,texH=0;uint32_t presentedAt=0;bool mouseDown=false;int shots=0;
// Key caps of the T-Deck keyboard by code (plus_keyboard.h codeFor / main.cpp keys[][]).
int letterCode(char c){static const char letters[]="poiuytrewq\blkjhgfdsa\n$mnbvcxz";for(int i=0;letters[i];i++)if(letters[i]==c)return i+1;return 0;}
// Characters of the Sym layer and the letter key that carries each (main.cpp symbols[][]).
int symbolCode(char c){static const char symbols[]="@+-_)(321#\b\"';:/654*\n0.,!?987";for(int i=0;symbols[i];i++)if(symbols[i]==c&&c!='\b'&&c!='\n')return i+1;return 0;}
int keyCode(SDL_Keycode k){
 if(k>='a'&&k<='z')return letterCode(char(k));
 switch(k){case SDLK_BACKSPACE:return 11;case SDLK_RETURN:case SDLK_KP_ENTER:return 21;case SDLK_SPACE:return 33;
  case SDLK_LSHIFT:return 35;case SDLK_RSHIFT:return 31;case SDLK_LALT:return 30;case SDLK_LCTRL:return 32;case SDLK_F3:return 34;}
 return 0;
}
// A typed symbol: Sym + key pressed now, released 40 ms later (the firmware polls the C3 every 8 ms). One at a time:
// a second symbol typed meanwhile waits in the queue.
struct Tap {int code;bool sym;};Tap taps[32];int tapHead=0,tapCount=0;uint32_t tapReleaseAt=0;bool tapDown=false;
void tap(int code,bool sym){if(tapCount<32){taps[(tapHead+tapCount)%32]={code,sym};tapCount++;}}
void taps_tick(){
 uint32_t now=SDL_GetTicks();
 if(tapDown){if(int32_t(now-tapReleaseAt)<0)return;Tap &t=taps[tapHead];boarddev::key(t.code,false);if(t.sym)boarddev::key(32,false);tapDown=false;tapHead=(tapHead+1)%32;tapCount--;tapReleaseAt=now+40;return;}
 if(!tapCount||int32_t(now-tapReleaseAt)<0)return;
 Tap &t=taps[tapHead];if(t.sym)boarddev::key(32,true);boarddev::key(t.code,true);tapDown=true;tapReleaseAt=now+40;
}
void screenshot(){
 std::string path=emu::dataDir()+"/screenshot-"+std::to_string(++shots)+".ppm";FILE *f=fopen(path.c_str(),"wb");if(!f)return;
 int w=boarddev::frameWidth(),h=boarddev::frameHeight();fprintf(f,"P6\n%d %d\n255\n",w,h);const uint16_t *p=boarddev::frame();
 for(int i=0;i<w*h;i++){uint16_t c=p[i];unsigned char rgb[3]={uint8_t((c>>8&0xF8)|(c>>13)),uint8_t((c>>3&0xFC)|(c>>9&3)),uint8_t((c<<3&0xF8)|(c>>2&7))};fwrite(rgb,1,3,f);}
 fclose(f);printf("EMU_SCREENSHOT %s\n",path.c_str());fflush(stdout);
}
void events(){
 SDL_Event e;
 while(SDL_PollEvent(&e)){
  switch(e.type){
   case SDL_QUIT:emu::requestQuit();break;
   case SDL_KEYDOWN:case SDL_KEYUP:{bool down=e.type==SDL_KEYDOWN;SDL_Keycode k=e.key.keysym.sym;
    if(down&&e.key.repeat)break;
    if(down&&k==SDLK_F5){boarddev::usbPower(!boarddev::usbPowered());printf("EMU_POWER %s\n",boarddev::usbPowered()?"usb":"battery");fflush(stdout);break;}
    if(down&&k==SDLK_F12){screenshot();break;}
    if(k==SDLK_TAB){boarddev::press(down);break;}
    if(down){if(k==SDLK_UP)boarddev::roll(0,-1);else if(k==SDLK_DOWN)boarddev::roll(0,1);else if(k==SDLK_LEFT)boarddev::roll(-1,0);else if(k==SDLK_RIGHT)boarddev::roll(1,0);}
    if(int code=keyCode(k))boarddev::key(code,down);
    break;}
   case SDL_TEXTINPUT:{const char *t=e.text.text;if(t[0]&&!t[1]){char c=t[0];if(c=='$')tap(22,false);else if(!(c>='a'&&c<='z')&&!(c>='A'&&c<='Z')&&c!=' '){if(int code=symbolCode(c))tap(code,true);}}break;}
   case SDL_MOUSEBUTTONDOWN:case SDL_MOUSEBUTTONUP:{bool down=e.type==SDL_MOUSEBUTTONDOWN;
    if(e.button.button==SDL_BUTTON_LEFT){mouseDown=down;boarddev::touch(e.button.x/scale,e.button.y/scale,down);}
    else if(e.button.button==SDL_BUTTON_MIDDLE)boarddev::press(down);break;}
   case SDL_MOUSEMOTION:if(mouseDown)boarddev::touch(e.motion.x/scale,e.motion.y/scale,true);break;
   case SDL_MOUSEWHEEL:if(e.wheel.y)boarddev::roll(0,e.wheel.y>0?-1:1);break;
  }
 }
}
void present(){
 uint32_t now=SDL_GetTicks();int level=boarddev::backlight();static int shownLevel=-1;static bool shownOn=false;bool on=boarddev::displayOn();
 if(now-presentedAt<16)return;
 if(!boarddev::frameDirty(false)&&level==shownLevel&&on==shownOn)return;
 presentedAt=now;shownLevel=level;shownOn=on;boarddev::frameDirty(true);
 int w=boarddev::frameWidth(),h=boarddev::frameHeight();
 if(w!=texW||h!=texH){if(tex)SDL_DestroyTexture(tex);tex=SDL_CreateTexture(ren,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STREAMING,w,h);texW=w;texH=h;SDL_SetWindowSize(win,w*scale,h*scale);}
 SDL_UpdateTexture(tex,nullptr,boarddev::frame(),w*2);
 uint8_t light=uint8_t(on?level*255/16:0);SDL_SetTextureColorMod(tex,light,light,light);
 SDL_SetRenderDrawColor(ren,0,0,0,255);SDL_RenderClear(ren);SDL_RenderCopy(ren,tex,nullptr,nullptr);SDL_RenderPresent(ren);
}
}

bool open(int windowScale,bool headless){
 scale=windowScale<1?1:windowScale;
 if(headless)SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");
 if(SDL_Init(SDL_INIT_VIDEO)!=0){fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return false;}
 win=SDL_CreateWindow((std::string("MatterMesh OS - T-Deck ")+emuBoardName+" (emulator)").c_str(),SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,boarddev::frameWidth()*scale,boarddev::frameHeight()*scale,0);
 if(!win){fprintf(stderr,"SDL_CreateWindow: %s\n",SDL_GetError());return false;}
 ren=SDL_CreateRenderer(win,-1,headless?SDL_RENDERER_SOFTWARE:SDL_RENDERER_ACCELERATED);if(!ren)ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);
 SDL_StartTextInput();
 emu::addPumpHook([]{events();taps_tick();present();});
 return true;
}
void screenshotNow(){screenshot();}
}
