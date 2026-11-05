#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
namespace device {
// u-blox M10 navigation database kept by the host: this board has no GPS backup cell, so
// every power-off loses ephemerides, almanac and the AssistNow Autonomous orbits. The dump
// (UBX-MGA-DBD poll, N entries, closed by MGA-ACK msgId 0x80) goes to the SD card and is
// replayed at the next power-on, one entry per MGA-ACK (CFG-NAVSPG-ACKAIDING on), so the
// receiver starts warm instead of cold. RAM only on the receiver side: no flash writes.
constexpr uint32_t GPS_BACKUP_AFTER_FIX_MS=30000;   // first dump: ephemerides need ~30 s after the fix
constexpr uint32_t GPS_BACKUP_INTERVAL_MS=600000;   // then every 10 min while the fix holds
constexpr uint32_t GPS_BACKUP_BEFORE_OFF_MS=60000;  // a lease release dumps first unless saved within 60 s
constexpr uint32_t GPS_BACKUP_OFF_DEADLINE_MS=4000; // ... and never keeps the GPS on longer than this
struct GpsBackup {
 static constexpr size_t MaxBytes=16384,HeaderSize=12;static constexpr uint32_t DumpIdleMs=1500,AckTimeoutMs=150;
 enum Phase:uint8_t{Idle,Dumping,Restoring};Phase phase=Idle;
 std::vector<uint8_t> blob;size_t cursor=0;uint32_t lastAt=0,sentAt=0;unsigned entries=0,sent=0,acks=0,dumps=0,restores=0,aborted=0;bool ready=false,waitingAck=false;
 static void frame(std::vector<uint8_t> &out,uint8_t cls,uint8_t id,const uint8_t *p,uint16_t n){
  size_t start=out.size();out.push_back(0xb5);out.push_back(0x62);out.push_back(cls);out.push_back(id);out.push_back(uint8_t(n));out.push_back(uint8_t(n>>8));
  out.insert(out.end(),p,p+n);uint8_t a=0,b=0;for(size_t i=start+2;i<out.size();i++){a+=out[i];b+=a;}out.push_back(a);out.push_back(b);
 }
 static void put32(uint8_t *p,uint32_t v){p[0]=uint8_t(v);p[1]=uint8_t(v>>8);p[2]=uint8_t(v>>16);p[3]=uint8_t(v>>24);}
 static uint32_t get32(const uint8_t *p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
 // "MMGN", version 1, 3 reserved, saved epoch (0 = clock unknown), then the MGA-DBD frames verbatim.
 static int count(const std::vector<uint8_t> &d){
  if(d.size()<HeaderSize||d.size()>MaxBytes||d[0]!='M'||d[1]!='M'||d[2]!='G'||d[3]!='N'||d[4]!=1)return -1;int n=0;
  for(size_t i=HeaderSize;i<d.size();){if(i+8>d.size()||d[i]!=0xb5||d[i+1]!=0x62||d[i+2]!=0x13||d[i+3]!=0x80)return -1;size_t len=d[i+4]|size_t(d[i+5])<<8,size=8+len;if(i+size>d.size())return -1;
   uint8_t a=0,b=0;for(size_t k=i+2;k<i+6+len;k++){a+=d[k];b+=a;}if(d[i+6+len]!=a||d[i+7+len]!=b)return -1;i+=size;n++;}
  return n;
 }
 static uint32_t savedEpoch(const std::vector<uint8_t> &d){return count(d)>=0?get32(d.data()+8):0;}
 bool idle()const{return phase==Idle;}
 template<class Send>bool startDump(uint32_t now,uint32_t epoch,Send send){
  if(phase!=Idle)return false;blob.assign(HeaderSize,0);blob[0]='M';blob[1]='M';blob[2]='G';blob[3]='N';blob[4]=1;put32(blob.data()+8,epoch);
  entries=0;ready=false;phase=Dumping;lastAt=now;std::vector<uint8_t> poll;frame(poll,0x13,0x80,nullptr,0);send(poll.data(),poll.size());return true;
 }
 template<class Send>bool startRestore(const std::vector<uint8_t> &data,uint32_t now,Send send){
  int n=count(data);if(phase!=Idle||n<=0)return false;blob=data;cursor=HeaderSize;entries=unsigned(n);sent=0;acks=0;phase=Restoring;waitingAck=false;next(now,send);return true;
 }
 void onFrame(uint8_t cls,uint8_t id,const uint8_t *p,uint16_t n,uint32_t now){
  if(cls!=0x13)return;
  if(phase==Dumping){
   if(id==0x80){if(blob.size()+n+8>MaxBytes){blob.clear();aborted++;phase=Idle;return;}frame(blob,0x13,0x80,p,n);entries++;lastAt=now;}
   else if(id==0x60&&n>=8&&p[3]==0x80)finish();
  }
  else if(phase==Restoring&&id==0x60&&waitingAck){acks++;waitingAck=false;}
 }
 template<class Send>void tick(uint32_t now,Send send){
  if(phase==Dumping){if(uint32_t(now-lastAt)>=DumpIdleMs)finish();}
  else if(phase==Restoring&&(!waitingAck||uint32_t(now-sentAt)>=AckTimeoutMs))next(now,send);
 }
 // Dump result: the caller stores it; the buffer is released.
 std::vector<uint8_t> take(){ready=false;std::vector<uint8_t> out=std::move(blob);blob.clear();blob.shrink_to_fit();return out;}
 std::string diagnostic()const{static const char *names[]={"idle","dumping","restoring"};return std::string("phase=")+names[phase]+" entries="+std::to_string(entries)+" bytes="+std::to_string(blob.size())+" ready="+std::to_string(ready)+" dumps="+std::to_string(dumps)+" restores="+std::to_string(restores)+" sent="+std::to_string(sent)+" acks="+std::to_string(acks)+" aborted="+std::to_string(aborted);}
 private:
 void finish(){if(entries){ready=true;dumps++;}else{blob.clear();aborted++;}phase=Idle;}
 template<class Send>void next(uint32_t now,Send send){
  if(cursor+8<=blob.size()){size_t len=blob[cursor+4]|size_t(blob[cursor+5])<<8,size=8+len;send(blob.data()+cursor,size);cursor+=size;sent++;sentAt=now;waitingAck=true;return;}
  phase=Idle;restores++;waitingAck=false;blob.clear();blob.shrink_to_fit();
 }
};
}
