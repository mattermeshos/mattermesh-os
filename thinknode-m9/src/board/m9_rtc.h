#pragma once
// PCF8563 real-time clock of the ThinkNode M9 (I2C 0x51 on its own bus, SDA 7 / SCL 6, with a backup cell): keeps UTC
// across a restart or power-off, so the clock shows at once instead of waiting for GPS or Wi-Fi again. Registers 0x02-0x08
// hold seconds..years in BCD; bit 7 of the seconds (VL) means the clock lost power and its time is not to be trusted.
#include <Arduino.h>
#include <Wire.h>
#include <time.h>
namespace hw {
constexpr uint8_t RtcAddress=0x51,RtcSda=7,RtcScl=6;
// The bus is opened only around each read / write and released after: with it held open the D-pad stopped sending its
// arrows on a real M9 (2026-11-02, arrows came back once the pins were let go).
inline TwoWire &rtcBus(){Wire1.begin(RtcSda,RtcScl,100000);return Wire1;}
inline void rtcRelease(){Wire1.end();pinMode(RtcSda,INPUT);pinMode(RtcScl,INPUT);}
inline uint8_t rtcBcd(uint8_t v){return (v>>4)*10+(v&15);}
inline uint8_t rtcToBcd(int v){return uint8_t((v/10)<<4|(v%10));}
// Days from 1970-01-01 to y-m-d (proleptic Gregorian, UTC; no time zone involved).
inline int64_t rtcDays(int y,int m,int d){y-=m<=2;int64_t era=(y>=0?y:y-399)/400;int yoe=int(y-era*400);int doy=(153*(m+(m>2?-3:9))+2)/5+d-1;int doe=yoe*365+yoe/4-yoe/100+doy;return era*146097+doe-719468;}
// UTC epoch from the clock; false when the chip is missing or its time is not valid.
inline bool rtcRead(uint32_t &epoch){
 auto &w=rtcBus();w.beginTransmission(RtcAddress);w.write(0x02);if(w.endTransmission(false)!=0||w.requestFrom(RtcAddress,uint8_t(7))!=7){rtcRelease();return false;}
 uint8_t r[7];for(auto &b:r)b=w.read();rtcRelease();if(r[0]&0x80)return false;// VL: the clock stopped
 int sec=rtcBcd(r[0]&0x7F),min=rtcBcd(r[1]&0x7F),hour=rtcBcd(r[2]&0x3F),day=rtcBcd(r[3]&0x3F),month=rtcBcd(r[5]&0x1F),year=2000+rtcBcd(r[6]);
 if(sec>59||min>59||hour>23||day<1||day>31||month<1||month>12)return false;
 epoch=uint32_t(rtcDays(year,month,day)*86400+hour*3600+min*60+sec);return true;}
inline bool rtcWrite(uint32_t epoch){
 time_t t=epoch;struct tm u;gmtime_r(&t,&u);if(u.tm_year+1900<2000||u.tm_year+1900>2099)return false;
 auto &w=rtcBus();
 // Control 1 = 0: clears a latched STOP bit, or a stopped clock would keep the time written and never count.
 w.beginTransmission(RtcAddress);w.write(0x00);w.write(0x00);if(w.endTransmission()!=0){rtcRelease();return false;}
 w.beginTransmission(RtcAddress);w.write(0x02);
 w.write(rtcToBcd(u.tm_sec));w.write(rtcToBcd(u.tm_min));w.write(rtcToBcd(u.tm_hour));w.write(rtcToBcd(u.tm_mday));w.write(uint8_t(u.tm_wday));w.write(rtcToBcd(u.tm_mon+1));w.write(rtcToBcd(u.tm_year+1900-2000));
 bool ok=w.endTransmission()==0;rtcRelease();return ok;}
}
