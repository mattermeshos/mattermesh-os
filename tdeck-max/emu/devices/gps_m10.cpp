// Emulated u-blox MIA-M10Q GPS on Serial2 (the firmware's SerialGPS: src/services/location.cpp, gps_nmea.h,
// gps_probe.h, TinyGPS++). Speaks 38400 baud like the module; at another baud the firmware sees nothing and
// switches, as on the device. Once a second, while the UART is open and the module awake: RMC, GGA, GSA, GSV
// with the PC's UTC time. Cold start: "searching" sentences for ColdStartMs, then a 3D fix (9 satellites,
// HDOP 0.9) at the configured position with ~1 m of jitter. UBX from the firmware: RXM-PMREQ puts the module
// in backup (silent) until the next RX edge (the firmware's 0xFF wake bytes); MON-VER is answered; CFG-* gets
// ACK-ACK. Other polls (NAV / MON-RF / MGA) are left unanswered, as a busy receiver would.
#include "gps_m10.h"
#ifdef MM_BOARD_MAX
#include "max_devices.h"
#endif
#include <Arduino.h>
#include <emu_hal.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>
namespace gpsm10 {
namespace {
constexpr unsigned long ModuleBaud=38400;constexpr uint32_t ColdStartMs=6000;
bool powered(){
#ifdef MM_BOARD_MAX
 return maxdev::ioLevel(2);
#else
 return true;
#endif
}
bool present=true;double lat=44.18795,lon=8.24555,alt=420;
std::mutex mutex;bool asleep=false;uint32_t poweredAt=0,lastBurst=0;bool wasOpen=false;
std::vector<uint8_t> pending;// UBX replies, sent with the next burst
std::vector<uint8_t> ubxIn;
void checksumUbx(std::vector<uint8_t> &f){uint8_t a=0,b=0;for(size_t i=2;i<f.size();i++){a+=f[i];b+=a;}f.push_back(a);f.push_back(b);}
std::vector<uint8_t> ubx(uint8_t cls,uint8_t id,const std::vector<uint8_t> &payload){
 std::vector<uint8_t> f={0xB5,0x62,cls,id,uint8_t(payload.size()),uint8_t(payload.size()>>8)};f.insert(f.end(),payload.begin(),payload.end());checksumUbx(f);return f;}
std::vector<uint8_t> monVer(){
 std::vector<uint8_t> p(40,0);memcpy(p.data(),"ROM SPG 5.10 (7b202e)",21);memcpy(p.data()+30,"00080000",8);
 for(const char *ext:{"FWVER=SPG 5.10","PROTVER=34.10","MOD=MIA-M10Q","GPS;GLO;GAL;BDS","SBAS;QZSS"}){std::vector<uint8_t> e(30,0);memcpy(e.data(),ext,strlen(ext));p.insert(p.end(),e.begin(),e.end());}
 return ubx(0x0A,0x04,p);}
// Firmware -> module bytes (UART TX). Any byte is an RX edge: it wakes a module in backup.
void onWrite(const uint8_t *d,size_t n){
 std::lock_guard<std::mutex> g(mutex);
 if(!present||!powered())return;
 if(asleep&&n){asleep=false;poweredAt=millis();}// warm start after backup: the fix comes back quickly
 for(size_t i=0;i<n;i++){
  ubxIn.push_back(d[i]);
  if(ubxIn.size()==1&&ubxIn[0]!=0xB5){ubxIn.clear();continue;}
  if(ubxIn.size()==2&&ubxIn[1]!=0x62){ubxIn.clear();continue;}
  if(ubxIn.size()<6)continue;size_t len=ubxIn[4]|ubxIn[5]<<8;if(len>1024){ubxIn.clear();continue;}
  if(ubxIn.size()<8+len)continue;
  uint8_t cls=ubxIn[2],id=ubxIn[3];ubxIn.clear();
  if(cls==0x02&&id==0x41){asleep=true;continue;}// RXM-PMREQ: backup
  if(cls==0x0A&&id==0x04&&len==0){auto f=monVer();pending.insert(pending.end(),f.begin(),f.end());continue;}
  if(cls==0x06){auto f=ubx(0x05,0x01,{cls,id});pending.insert(pending.end(),f.begin(),f.end());}// ACK-ACK
 }
}
std::string nmea(const std::string &body){uint8_t c=0;for(char ch:body)c^=uint8_t(ch);char tail[8];snprintf(tail,sizeof(tail),"*%02X\r\n",c);return "$"+body+tail;}
std::string coord(double v,bool latitude){double a=fabs(v);int deg=int(a);double min=(a-deg)*60;char out[32];
 snprintf(out,sizeof(out),latitude?"%02d%08.5f,%c":"%03d%08.5f,%c",deg,min,latitude?(v<0?'S':'N'):(v<0?'W':'E'));return out;}
void burst(){
 time_t now=time(nullptr);tm u;gmtime_r(&now,&u);char hms[16],date[8];snprintf(hms,sizeof(hms),"%02d%02d%02d.00",u.tm_hour,u.tm_min,u.tm_sec);snprintf(date,sizeof(date),"%02d%02d%02d",u.tm_mday,u.tm_mon+1,u.tm_year%100);
 bool fix=millis()-poweredAt>=ColdStartMs;std::string out;
 double jlat=lat+(random(-100,101)/100.0)*0.000009,jlon=lon+(random(-100,101)/100.0)*0.000012;
 if(fix){
  out+=nmea(std::string("GNRMC,")+hms+",A,"+coord(jlat,true)+","+coord(jlon,false)+",0.02,,"+date+",,,A,V");
  char gga[160];snprintf(gga,sizeof(gga),"GNGGA,%s,%s,%s,1,09,0.90,%.1f,M,47.0,M,,",hms,coord(jlat,true).c_str(),coord(jlon,false).c_str(),alt);out+=nmea(gga);
  out+=nmea("GNGSA,A,3,02,05,12,15,18,24,25,29,,,,,1.60,0.90,1.32,1");
  out+=nmea("GPGSV,3,1,11,02,47,302,38,05,22,064,33,12,71,181,42,15,10,140,29,1");
  out+=nmea("GPGSV,3,2,11,18,35,095,36,24,58,248,40,25,40,200,37,29,15,320,31,1");
  out+=nmea("GPGSV,3,3,11,13,05,030,,20,03,110,,23,08,290,,1");
 }else{
  out+=nmea(std::string("GNRMC,")+hms+",V,,,,,,,"+date+",,,N,V");
  out+=nmea(std::string("GNGGA,")+hms+",,,,,0,00,99.99,,,,,,");
  out+=nmea("GNGSA,A,1,,,,,,,,,,,,,99.99,99.99,99.99,1");
  out+=nmea("GPGSV,1,1,03,02,47,302,21,12,71,181,19,24,58,248,,1");
 }
 std::vector<uint8_t> bytes(out.begin(),out.end());bytes.insert(bytes.end(),pending.begin(),pending.end());pending.clear();
 Serial2.feed(bytes.data(),bytes.size());
}
void tick(){
 std::lock_guard<std::mutex> g(mutex);bool open=Serial2.emuOpen()&&powered();
 if(open&&!wasOpen&&!asleep)poweredAt=millis();wasOpen=open;
 if(!present||!open||asleep||Serial2.emuBaud()!=ModuleBaud)return;
 uint32_t now=millis();if(now-lastBurst<1000)return;lastBurst=now;burst();
}
}
void configure(bool on,double latitude,double longitude,double altitude){present=on;lat=latitude;lon=longitude;alt=altitude;}
void attach(){Serial2.emuOnWrite(onWrite);poweredAt=millis();emu::addPumpHook(tick);}
}
