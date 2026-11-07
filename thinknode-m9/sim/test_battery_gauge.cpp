// Plus battery gauge: curve, load compensation, sag recovery; optional replay of a battery-log.csv
// (prints minute, old shown %, new shown % so the two can be compared).
#include "services/battery_gauge.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
using namespace device;
int main(int argc,char **argv){
 // M9 4.35 V cell (provisional curve in battery_gauge.h)
 assert(plusBatteryCurve(3300)==0&&plusBatteryCurve(3000)==0&&plusBatteryCurve(4320)==100&&plusBatteryCurve(3850)==50);
 for(int mv=3300;mv<4320;mv+=5)assert(plusBatteryCurve(mv+5)>=plusBatteryCurve(mv));
 BatteryLoad idle,busy;busy.active=busy.wifi=busy.gps=true;
 // A 3-minute sag under load must not cost percent once the device rests again.
 PlusGauge g;uint32_t t=0;int raw=3700;for(int i=0;i<30;i++){g.update(raw,idle,t,1030);t+=10000;}int before=g.shown;
 for(int i=0;i<18;i++){g.update(raw-150,busy,t,1030);t+=10000;}// sag beyond the compensation
 int dipped=g.shown;for(int i=0;i<60;i++){g.update(raw,idle,t,1030);t+=10000;}
 assert(dipped<=before&&g.shown>=before-1);
 // A real decline still goes down.
 PlusGauge d;t=0;for(int i=0;i<200;i++){d.update(3900-i*2,idle,t,1030);t+=10000;}assert(d.shown<plusBatteryCurve(int(3900*1.03))-20);
 std::cout<<"PASS: Plus battery curve monotonic, 0 % at 3.30 V, sag recovered at rest, real decline followed\n";
 if(argc>1){// replay: uptime_s,epoch,raw_mv,filtered_mv,curve_pct,shown_pct,source,cpu_mhz,locked,display_on,keyboard_light,wifi,ble_app,gps,...
  std::ifstream f(argv[1]);std::string line;std::getline(f,line);PlusGauge r;uint32_t first=0;
  while(std::getline(f,line)){std::vector<std::string> c;std::stringstream ss(line);std::string x;while(std::getline(ss,x,','))c.push_back(x);if(c.size()<14||c[6]!="1")continue;
   uint32_t epoch=std::stoul(c[1]);if(!first)first=epoch;BatteryLoad l;l.active=c[8]=="0";l.wifi=c[11]=="1";l.gps=c[13]=="1";l.cpuHigh=c[7]=="240";
   int shown=r.update(std::stoi(c[2]),l,(epoch-first)*1000,1030);printf("%lu,%s,%d,%d\n",(unsigned long)epoch,c[5].c_str(),shown,int(r.restMv+0.5f));}
 }
}
