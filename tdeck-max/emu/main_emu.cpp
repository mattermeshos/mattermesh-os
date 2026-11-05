// Run the unchanged firmware setup()/loop() on the board chosen at build time.
//   emu-plus [--data DIR] [--scale N] [--headless] [--no-sd] [--shot-after MS]... [--quit-after MS] [--script FILE] [--wifi SSID:PASS] [--port-offset N] [--name NODE] [--air PORT|off] [--gps LAT,LON[,ALT]|off]
// DIR (default emu/data-plus) holds the SD card (sd/), the NVS (nvs.json) and screenshots.
#include <Arduino.h>
#include <emu_hal.h>
#include "devices/board_devices.h"
#include "devices/window.h"
#include "devices/script.h"
#include "devices/gps_m10.h"
#include <WiFi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <csignal>
#include <unistd.h>
void setup();
void loop();
// Ctrl+C / SIGTERM: the first asks the firmware loop to stop, a second one (or a stuck loop) exits at once.
static void onSignal(int){static volatile sig_atomic_t count=0;if(count++)_exit(130);emu::requestQuit();}
int main(int argc,char **argv){
 std::string data=std::string("emu/data-")+emuBoardName;int scale=3;bool headless=false,sd=true;static std::vector<uint32_t> shotsAt;static uint32_t quitAt=0;std::string wifiSsid="EMU-WiFi",wifiPass="meshcore";int portOffset=0;std::string node;int airPort=7300;bool gpsOn=true;double gpsLat=44.18795,gpsLon=8.24555,gpsAlt=420;
 for(int i=1;i<argc;i++){
  if(!strcmp(argv[i],"--data")&&i+1<argc)data=argv[++i];
  else if(!strcmp(argv[i],"--scale")&&i+1<argc)scale=atoi(argv[++i]);
  else if(!strcmp(argv[i],"--headless"))headless=true;
  else if(!strcmp(argv[i],"--no-sd"))sd=false;
  else if(!strcmp(argv[i],"--shot-after")&&i+1<argc)shotsAt.push_back(uint32_t(atol(argv[++i])));
  else if(!strcmp(argv[i],"--quit-after")&&i+1<argc)quitAt=uint32_t(atol(argv[++i]));
  else if(!strcmp(argv[i],"--script")&&i+1<argc){if(!script::load(argv[++i]))return 2;}
  else if(!strcmp(argv[i],"--wifi")&&i+1<argc){std::string v=argv[++i];auto colon=v.find(':');wifiSsid=v.substr(0,colon);wifiPass=colon==std::string::npos?"":v.substr(colon+1);}
  else if(!strcmp(argv[i],"--port-offset")&&i+1<argc)portOffset=atoi(argv[++i]);
  else if(!strcmp(argv[i],"--name")&&i+1<argc)node=argv[++i];
  else if(!strcmp(argv[i],"--gps")&&i+1<argc){const char *v=argv[++i];if(!strcmp(v,"off"))gpsOn=false;else if(sscanf(v,"%lf,%lf,%lf",&gpsLat,&gpsLon,&gpsAlt)<2){fprintf(stderr,"--gps LAT,LON[,ALT]\n");return 2;}}
  else if(!strcmp(argv[i],"--air")&&i+1<argc){const char *v=argv[++i];airPort=strcmp(v,"off")?atoi(v):0;}
  else{fprintf(stderr,"usage: %s [--data DIR] [--scale N] [--headless] [--no-sd] [--shot-after MS]... [--quit-after MS] [--script FILE] [--wifi SSID:PASS] [--port-offset N] [--name NODE] [--air PORT|off] [--gps LAT,LON[,ALT]|off]\n",argv[0]);return 2;}
 }
 emuwifi::configure(wifiSsid.c_str(),wifiPass.c_str(),portOffset);
 mkdir(data.c_str(),0755);emu::setDataDir(data);emu::setSdPresent(sd);
 // Node name on the air: --name, else the data folder name.
 if(node.empty()){auto slash=data.find_last_of('/');node=slash==std::string::npos?data:data.substr(slash+1);if(node.empty())node="plus";}
 boarddev::attach(node.c_str(),airPort);
 gpsm10::configure(gpsOn,gpsLat,gpsLon,gpsAlt);gpsm10::attach();
 if(!window::open(scale,headless))return 1;
 std::signal(SIGINT,onSignal);std::signal(SIGTERM,onSignal);// after SDL_Init, which installs its own handlers
 printf("EMU_START board=%s data=%s sd=%d wifi=%s port_offset=%d node=%s air=%d\n"
        "EMU_KEYS letters/Enter/Backspace/Space | LShift RShift | LAlt=Alt | LCtrl=Sym | digits+punctuation=Sym chord | "
        "Tab/middle=BOOT | mouse=touch | F5 USB/battery | F12 screenshot\n",emuBoardName,data.c_str(),sd,wifiSsid.c_str(),portOffset,node.c_str(),airPort);
 fflush(stdout);
 // Test helpers: screenshots and exit at fixed times after start (headless runs).
 emu::addPumpHook([]{uint32_t now=emu::millis32();for(auto &at:shotsAt)if(at&&now>=at){at=0;window::screenshotNow();}if(quitAt&&now>=quitAt)emu::requestQuit();});
 emuRunLoop(setup,loop);
 // No static destructors: the radio, the air link and the core threads stop with the process (NVS and SD writes
 // are already complete), and tearing them down in static order could deadlock.
 fflush(stdout);fflush(stderr);_exit(0);
}
