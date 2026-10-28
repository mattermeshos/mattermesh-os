#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "services/hex_util.h"
namespace device {
// Observes existing UART bytes, never sends configuration to the receiver.
// Checksum-valid GGA/GSV counts remain useful before a position fix.
struct GpsNmea {
 struct View {char talker[3]={},raw[128]={};uint16_t count=0,peakCno=0,trackedEntries=0;uint32_t at=0;bool valid=false;};
 char line[128]={},lastGsv[128]={};unsigned length=0;bool collecting=false;
 uint32_t bytes=0,valid=0,bad=0,lastValid=0,usedAt=0;unsigned used=0;bool usedValid=false;
 bool fixKnown=false,fix=false;uint32_t fixAt=0;int quality=-1,hdopHundredths=-1;
 View views[8];
 static bool number(const char *s,unsigned &n){if(!s||!*s)return false;n=0;while(*s){if(*s<'0'||*s>'9'||n>999)return false;n=n*10+(*s++-'0');}return n<=999;}
 static int decimalHundredths(const char *s){
  if(!s||!*s)return -1;unsigned whole=0,fraction=0,digits=0;bool dot=false;
  for(;*s;s++){if(*s=='.'&&!dot){dot=true;continue;}if(*s<'0'||*s>'9')return -1;
   if(!dot){whole=whole*10+unsigned(*s-'0');if(whole>999)return -1;}
   else {if(digits<2)fraction=fraction*10+unsigned(*s-'0');digits++;}
  }if(digits==1)fraction*=10;return int(whole*100+fraction);
 }
 void feed(char ch,uint32_t now){
  bytes++;
  if(ch=='$'){collecting=true;length=0;return;}
  if(!collecting)return;
  if(ch=='\r'||ch=='\n'){collecting=false;line[length]=0;parse(now);return;}
  if(length+1>=sizeof(line)){collecting=false;bad++;return;}
  line[length++]=ch;
 }
 void parse(uint32_t now){
  char *star=strchr(line,'*');if(!star||strlen(star)!=3){bad++;return;}
  int a=mm::hexNibble(star[1]),b=mm::hexNibble(star[2]);unsigned parity=0;for(char *p=line;p<star;p++)parity^=uint8_t(*p);
  if(a<0||b<0||parity!=unsigned(a*16+b)){bad++;return;}
  valid++;lastValid=now;if(star-line>=5&&!strncmp(line+2,"GSV",3)){strncpy(lastGsv,line,sizeof(lastGsv)-1);lastGsv[sizeof(lastGsv)-1]=0;}*star=0;
  char *fields[24]={line};unsigned n=1;for(char *p=line;*p&&n<24;p++)if(*p==','){*p=0;fields[n++]=p+1;}
  if(strlen(fields[0])!=5)return;unsigned count=0;
  if(!strcmp(fields[0]+2,"RMC")&&n>2&&(!strcmp(fields[2],"A")||!strcmp(fields[2],"V"))){fixKnown=true;fix=fields[2][0]=='A';fixAt=now;}
  if(!strcmp(fields[0]+2,"GGA")&&n>8&&number(fields[6],count)&&count<=8){quality=int(count);hdopHundredths=decimalHundredths(fields[8]);fixKnown=true;fix=count!=0;fixAt=now;}
  if(!strcmp(fields[0]+2,"GGA")&&n>7&&number(fields[7],count)){used=count;usedAt=now;usedValid=true;}
  if(!strcmp(fields[0]+2,"GSV")&&n>3&&number(fields[3],count)){
   View *slot=nullptr;
   for(auto &v:views)if(v.valid&&!strncmp(v.talker,fields[0],2)){slot=&v;break;}
   if(!slot)for(auto &v:views)if(!v.valid||uint32_t(now-v.at)>=15000){slot=&v;break;}
   if(slot){
    unsigned part=0;number(fields[2],part);
    if(!slot->valid||uint32_t(now-slot->at)>=15000||part==1)slot->peakCno=slot->trackedEntries=0;
    // C/N0 belongs to received GNSS signal entries, not satellites used for
    // navigation. Keep original GSV per talker for diagnosis, never positions.
    for(unsigned i=7;i<n;i+=4){unsigned cno=0;if(number(fields[i],cno)&&cno>0&&cno<=99){if(cno>slot->peakCno)slot->peakCno=cno;slot->trackedEntries++;}}
    strncpy(slot->raw,lastGsv,sizeof(slot->raw)-1);slot->raw[sizeof(slot->raw)-1]=0;
    slot->talker[0]=fields[0][0];slot->talker[1]=fields[0][1];slot->talker[2]=0;slot->count=count;slot->at=now;slot->valid=true;
   }
  }
 }
 unsigned satellitesUsed(uint32_t now)const{return usedValid&&uint32_t(now-usedAt)<15000?used:0;}
 unsigned satellitesVisible(uint32_t now)const{
  // GN is already combined. Never add it to the individual constellations.
  unsigned sum=0;bool chinese=false;
  for(auto &v:views)if(v.valid&&uint32_t(now-v.at)<15000&&!strcmp(v.talker,"GN"))return v.count;
  for(auto &v:views)if(v.valid&&uint32_t(now-v.at)<15000){if(!strcmp(v.talker,"GB")||!strcmp(v.talker,"BD")){if(chinese)continue;chinese=true;}sum+=v.count;}
  return sum;
 }
 bool fresh(uint32_t now)const{return valid&&uint32_t(now-lastValid)<15000;}
 bool positionTrusted(uint32_t now)const{return !fixKnown||(fix&&uint32_t(now-fixAt)<15000);}
 // A navigation solution worth showing and keeping: GGA quality 1..5 (6 = estimated, e.g. from the aiding position
 // or dead reckoning indoors), at least 4 satellites used and HDOP <= 5 when the receiver reports them.
 bool solutionGood(uint32_t now)const{if(quality==6||quality==0)return false;if(usedValid&&uint32_t(now-usedAt)<15000&&used<4)return false;if(hdopHundredths>500)return false;return true;}
};
}
