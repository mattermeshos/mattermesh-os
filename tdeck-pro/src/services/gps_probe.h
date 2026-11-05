#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include "services/hex_util.h"
namespace device {
// u-blox M10 SPG5.10: MON-VER (0A04), MON-RF (0A38), empty-payload polls.
// No CFG, reset, persistent writes, automatic retries or acquisition timeout.
// Raw diagnostic polls allow only NAV/MON polls and CFG-VALGET (06 8B, v0).
struct GpsProbe {
 static constexpr unsigned Capacity=512;static constexpr uint32_t Timeout=1500;
 uint8_t payload[Capacity]={},stage=0,cls=0,id=0,a=0,b=0,receivedA=0;
 uint16_t length=0,offset=0;int request=-1;bool waiting=false;
 uint32_t sentAt=0,versionAt=0,rfAt=0,valid=0,bad=0,timeouts=0;bool frameReady=false; // last checksum-valid frame (cls/id/payload/length) not yet consumed by the caller
 bool versionValid=false,rfValid=false;std::string version;
 uint8_t antenna=0,antennaPower=0,jamming=0,cwSuppression=0;uint16_t noise=0,agc=0;
 uint8_t rawFrame[72]={},raw[Capacity]={},rawCls=0,rawId=0,rawState=0;uint16_t rawSize=0,rawLength=0;uint32_t rawAt=0;
 enum RawState:uint8_t{RawIdle,RawQueued,RawWaiting,RawDone,RawTimeout,RawNak};
 static bool readOnly(uint8_t c,uint8_t i,const uint8_t *p,uint16_t n){return c==0x01||c==0x0a||(c==0x06&&i==0x8b&&n>=4&&p[0]==0);}
 bool poll(uint8_t c,uint8_t i,const uint8_t *p,uint16_t n){
  if(busy()||rawState==RawQueued||rawState==RawWaiting||n>sizeof(rawFrame)-8||!readOnly(c,i,p,n))return false;
  uint8_t *f=rawFrame;f[0]=0xb5;f[1]=0x62;f[2]=c;f[3]=i;f[4]=uint8_t(n);f[5]=uint8_t(n>>8);for(uint16_t k=0;k<n;k++)f[6+k]=p[k];
  uint8_t x=0,y=0;for(unsigned k=2;k<6u+n;k++){x+=f[k];y+=x;}f[6+n]=x;f[7+n]=y;rawSize=8+n;rawCls=c;rawId=i;rawLength=0;rawState=RawQueued;return true;
 }
 bool busy()const{return request>=0;}
 bool start(){if(busy()||rawState==RawQueued||rawState==RawWaiting)return false;request=0;waiting=false;return true;}
 void cancel(){request=-1;waiting=false;stage=0;}
 void sum(uint8_t c){a+=c;b+=a;}
 static std::string ascii(const uint8_t *p,unsigned n){std::string s;for(unsigned i=0;i<n&&p[i];i++)s+=p[i]>=32&&p[i]<=126?char(p[i]):'?';return s;}
 static uint16_t u16(const uint8_t *p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
 void accept(uint32_t now){
  if(rawState==RawWaiting&&((cls==rawCls&&id==rawId)||(cls==0x05&&id==0x00&&length>=2&&payload[0]==rawCls&&payload[1]==rawId))){
   rawState=cls==0x05?RawNak:RawDone;rawLength=length;for(uint16_t k=0;k<length;k++)raw[k]=payload[k];rawAt=now;return;
  }
  if(cls!=0x0a)return;bool accepted=false;
  if(id==0x04&&length>=40&&(length-40)%30==0){version=ascii(payload,30)+" / HW "+ascii(payload+30,10);for(unsigned i=40;i<length;i+=30)version+=" / "+ascii(payload+i,30);versionValid=true;versionAt=now;accepted=true;}
  if(id==0x38&&length>=4&&payload[0]==0&&length==4+unsigned(payload[1])*24){
   for(unsigned i=0;i<payload[1];i++){auto *p=payload+4+i*24;if(p[0]!=0)continue;jamming=p[1]&3;antenna=p[2];antennaPower=p[3];noise=u16(p+12);agc=u16(p+14);cwSuppression=p[16];rfValid=true;rfAt=now;accepted=true;break;}
  }
  if(accepted&&waiting&&id==(request==0?0x04:0x38)){waiting=false;if(++request==2)request=-1;}
 }
 // True means UBX byte consumed: binary replies must not enter NMEA parsing.
 bool feed(uint8_t c,uint32_t now){
  if(stage&&uint32_t(now-frameAt)>1000)stage=0;
  if(!stage){if(c!=0xb5)return false;stage=1;frameAt=now;return true;}
  if(stage==1){if(c!=0x62){stage=c==0xb5?1:0;return c==0xb5;}stage=2;a=b=0;return true;}
  if(stage==2){cls=c;sum(c);stage++;}
  else if(stage==3){id=c;sum(c);stage++;}
  else if(stage==4){length=c;sum(c);stage++;}
  else if(stage==5){length|=uint16_t(c)<<8;sum(c);offset=0;if(length>Capacity){bad++;stage=0;}else stage=length?6:7;}
  else if(stage==6){payload[offset++]=c;sum(c);if(offset==length)stage=7;}
  else if(stage==7){receivedA=c;stage=8;}
  else {stage=0;if(receivedA==a&&c==b){valid++;frameReady=true;accept(now);}else bad++;}
  return true;
 }
 template<class Send>void tick(uint32_t now,Send send){
  if(rawState==RawWaiting&&uint32_t(now-sentAt)>=Timeout)rawState=RawTimeout;
  if(!busy()&&rawState==RawQueued){send(rawFrame,rawSize);sentAt=now;rawState=RawWaiting;return;}
  if(!busy())return;
  if(waiting){if(uint32_t(now-sentAt)<Timeout)return;timeouts++;waiting=false;if(++request==2){request=-1;return;}}
  uint8_t poll[8]={0xb5,0x62,0x0a,uint8_t(request==0?0x04:0x38),0,0,0,0};
  for(unsigned i=2;i<6;i++){poll[6]+=poll[i];poll[7]+=poll[6];}
  send(poll,sizeof(poll));sentAt=now;waiting=true;
 }
 std::string diagnostic(uint32_t now)const{
  std::string s="pending="+std::to_string(busy())+" timeouts="+std::to_string(timeouts)+" ubx_bad="+std::to_string(bad);
  s+=versionValid?"\nversion_age_ms="+std::to_string(uint32_t(now-versionAt))+" "+version:"\nversion unavailable";
  if(rfValid)s+="\nrf_age_ms="+std::to_string(uint32_t(now-rfAt))+" antenna_status="+std::to_string(antenna)+" antenna_power="+std::to_string(antennaPower)+" jamming_state="+std::to_string(jamming)+" noise="+std::to_string(noise)+" agc="+std::to_string(agc)+" cw_suppression="+std::to_string(cwSuppression);
  else s+="\nRF data unavailable";return s;
 }
 std::string rawDiagnostic(uint32_t now)const{
  static const char *names[]={"idle","queued","waiting","done","timeout","nak"};
  std::string s="state="+std::string(names[rawState])+" class="+std::to_string(rawCls)+" id="+std::to_string(rawId)+" length="+std::to_string(rawLength);
  if(rawState==RawDone||rawState==RawNak){s+=" age_ms="+std::to_string(uint32_t(now-rawAt))+" payload=";mm::appendHex(s,raw,rawLength);}
  return s;
 }
 private:uint32_t frameAt=0;
};
}
