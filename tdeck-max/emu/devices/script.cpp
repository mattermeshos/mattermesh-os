// Timed input script for headless runs (--script FILE). One event per line, '#' starts a comment:
//   <ms> key <code> down|up        T-Deck key cap by code (plus_keyboard.h), e.g. 21 = Enter, 33 = Space
//   <ms> tap <code>                 press, release 60 ms later
//   <ms> type <text>                letters / space typed as key taps 90 ms apart (Shift for capitals)
//   <ms> touch <x> <y> down|up      finger on the screen (landscape coordinates)
//   <ms> click <x> <y>              touch down, up 80 ms later
//   <ms> roll <dx> <dy>             trackball detent movements
//   <ms> press down|up | <ms> pressclick   trackball centre button
//   <ms> serial <line>              a line on the firmware's USB serial
//   <ms> usb on|off                 power source
//   <ms> shot                       screenshot (dataDir/screenshot-N.ppm)
//   <ms> quit
// Times are absolute milliseconds since the emulator started.
#include "script.h"
#include "board_devices.h"
#include "window.h"
#include <Arduino.h>
#include <emu_hal.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <functional>
#include <vector>
namespace script {
namespace {
struct Event {uint32_t at;std::function<void()> run;};
std::vector<Event> events;size_t next=0;
int letterCode(char c){static const char letters[]="poiuytrewq\blkjhgfdsa\n$mnbvcxz";for(int i=0;letters[i];i++)if(letters[i]==c)return i+1;return 0;}
void add(uint32_t at,std::function<void()> f){events.push_back({at,std::move(f)});}
}
bool load(const std::string &path){
 std::ifstream in(path);if(!in){fprintf(stderr,"script: cannot open %s\n",path.c_str());return false;}
 std::string line;int lineNo=0;
 while(std::getline(in,line)){lineNo++;auto hash=line.find('#');if(hash!=std::string::npos)line.resize(hash);
  std::istringstream s(line);uint32_t at;std::string cmd;if(!(s>>at>>cmd))continue;
  if(cmd=="key"){int code;std::string state;s>>code>>state;bool down=state=="down";add(at,[=]{boarddev::key(code,down);});}
  else if(cmd=="tap"){int code;s>>code;add(at,[=]{boarddev::key(code,true);});add(at+60,[=]{boarddev::key(code,false);});}
  else if(cmd=="type"){std::string text;std::getline(s,text);if(!text.empty()&&text[0]==' ')text.erase(0,1);uint32_t t=at;
   for(char c:text){bool upper=c>='A'&&c<='Z';int code=c==' '?33:letterCode(char(upper?c-'A'+'a':c));if(!code)continue;
    if(upper)add(t,[]{boarddev::key(35,true);});add(t+5,[=]{boarddev::key(code,true);});add(t+55,[=]{boarddev::key(code,false);});if(upper)add(t+60,[]{boarddev::key(35,false);});t+=90;}}
  else if(cmd=="touch"){int x,y;std::string state;s>>x>>y>>state;bool down=state=="down";add(at,[=]{boarddev::touch(x,y,down);});}
  else if(cmd=="click"){int x,y;s>>x>>y;add(at,[=]{boarddev::touch(x,y,true);});add(at+80,[=]{boarddev::touch(x,y,false);});}
  else if(cmd=="roll"){int dx,dy;s>>dx>>dy;add(at,[=]{boarddev::roll(dx,dy);});}
  else if(cmd=="press"){std::string state;s>>state;bool down=state=="down";add(at,[=]{boarddev::press(down);});}
  else if(cmd=="pressclick"){add(at,[]{boarddev::press(true);});add(at+80,[]{boarddev::press(false);});}
  else if(cmd=="serial"){std::string text;std::getline(s,text);if(!text.empty()&&text[0]==' ')text.erase(0,1);text+="\n";add(at,[=]{Serial.feed(reinterpret_cast<const uint8_t*>(text.data()),text.size());});}
  else if(cmd=="usb"){std::string state;s>>state;bool on=state=="on";add(at,[=]{boarddev::usbPower(on);});}
  else if(cmd=="shot")add(at,[]{window::screenshotNow();});
  else if(cmd=="quit")add(at,[]{emu::requestQuit();});
  else{fprintf(stderr,"script %s:%d: unknown command %s\n",path.c_str(),lineNo,cmd.c_str());return false;}
 }
 std::stable_sort(events.begin(),events.end(),[](const Event &a,const Event &b){return a.at<b.at;});
 emu::addPumpHook([]{uint32_t now=emu::millis32();while(next<events.size()&&events[next].at<=now)events[next++].run();});
 return true;
}
}
