#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
// Home Main area widgets: data kept in RAM since boot (altitude profile, packets per quarter hour) and position
// formats (UTM, a coordinate read out digit by digit). Pure code, host tested (sim/test_home_data.cpp).
namespace mm_home {
// Elevation: one GPS altitude every AltitudeStepMs, the last AltitudeSamples (12 h); climb() over those samples.
constexpr uint32_t AltitudeStepMs=600000UL;constexpr int AltitudeSamples=72;
struct AltitudeTrack {
 int16_t meters[AltitudeSamples]={};uint32_t epoch[AltitudeSamples]={};int count=0,head=0;uint32_t lastAt=0;
 // A sample at most every AltitudeStepMs (the first at once); epoch: the clock (0 when not set) for the axis labels.
 bool add(uint32_t now,int altitude,uint32_t clock){
  if(count&&uint32_t(now-lastAt)<AltitudeStepMs)return false;
  lastAt=now;meters[head]=int16_t(altitude);epoch[head]=clock;head=(head+1)%AltitudeSamples;if(count<AltitudeSamples)count++;return true;}
 int at(int i)const{return meters[(head-count+i+AltitudeSamples)%AltitudeSamples];}// i: 0 oldest .. count-1 newest
 uint32_t epochAt(int i)const{return epoch[(head-count+i+AltitudeSamples)%AltitudeSamples];}
 // Metres climbed over the stored samples: a rise counts once it is ClimbStep above the lowest point since the last
 // counted one, so GPS altitude noise (+-10..20 m) on a resting T-Deck adds nothing.
 static constexpr int ClimbStep=25;
 int climb()const{int total=0,low=count?at(0):0;for(int i=1;i<count;i++){int a=at(i);if(a<low)low=a;else if(a-low>=ClimbStep){total+=a-low;low=a;}}return total;}
};
// Mesh activity: packets received per quarter hour, the last 24 (6 h); fed with the radio's running total.
constexpr uint32_t PacketBucketMs=900000UL;constexpr int PacketBuckets=24,BucketsPerHour=int(3600000UL/PacketBucketMs);
struct PacketHistory {
 uint16_t count[PacketBuckets]={};uint32_t bucketStart=0,total=0;bool started=false;// bucketStart: millis of the current quarter (rollover safe)
 void update(uint32_t now,uint32_t received){
  if(!started){started=true;bucketStart=now;total=received;return;}
  uint32_t steps=uint32_t(now-bucketStart)/PacketBucketMs;if(steps){bucketStart+=steps*PacketBucketMs;int n=int(std::min<uint32_t>(steps,PacketBuckets));std::copy(count+n,count+PacketBuckets,count);std::fill(count+PacketBuckets-n,count+PacketBuckets,uint16_t(0));}
  if(received>=total){uint32_t add=received-total;count[PacketBuckets-1]=uint16_t(count[PacketBuckets-1]+add>65535?65535:count[PacketBuckets-1]+add);}total=received;}// a radio restart counts from 0 again
 int at(int i)const{return count[i];}// 0 oldest .. PacketBuckets-1 the current quarter
 int lastHour()const{int n=0;for(int i=PacketBuckets-BucketsPerHour;i<PacketBuckets;i++)n+=count[i];return n;}
};
// UTM (WGS84) "32T 393407 5086512"; empty outside 80 S .. 84 N. Norway / Svalbard zone exceptions are left out.
inline std::string utm(double lat,double lon){
 if(!std::isfinite(lat)||!std::isfinite(lon)||lat<-80||lat>84||lon<-180||lon>180)return "";
 int zone=std::min(60,int((lon+180)/6)+1);const char *bands="CDEFGHJKLMNPQRSTUVWX";char band=bands[std::min(19,int((lat+80)/8))];
 const double a=6378137.0,f=1/298.257223563,k0=0.9996,e2=f*(2-f),ep2=e2/(1-e2),rad=M_PI/180;
 double phi=lat*rad,lambda=(lon-(zone*6-183))*rad,n=a/std::sqrt(1-e2*std::sin(phi)*std::sin(phi)),t=std::tan(phi)*std::tan(phi),c=ep2*std::cos(phi)*std::cos(phi),A=std::cos(phi)*lambda;
 double m=a*((1-e2/4-3*e2*e2/64-5*e2*e2*e2/256)*phi-(3*e2/8+3*e2*e2/32+45*e2*e2*e2/1024)*std::sin(2*phi)+(15*e2*e2/256+45*e2*e2*e2/1024)*std::sin(4*phi)-(35*e2*e2*e2/3072)*std::sin(6*phi));
 double east=k0*n*(A+(1-t+c)*A*A*A/6+(5-18*t+t*t+72*c-58*ep2)*A*A*A*A*A/120)+500000;
 double north=k0*(m+n*std::tan(phi)*(A*A/2+(5-t+9*c+4*c*c)*A*A*A*A/24+(61-58*t+t*t+600*c-330*ep2)*A*A*A*A*A*A/720));if(lat<0)north+=10000000;
 char text[32];std::snprintf(text,sizeof text,"%d%c %ld %ld",zone,band,long(std::lround(east)),long(std::lround(north)));return text;
}
// "45.9213 N" read out: "four five point nine two one three north" (degrees, 4 decimals, hemisphere).
inline std::string spoken(double value,bool latitude){
 static const char *digits[]={"zero","one","two","three","four","five","six","seven","eight","nine"};
 char text[16];std::snprintf(text,sizeof text,"%.4f",std::fabs(value));std::string out;
 for(const char *p=text;*p;p++){if(!out.empty())out+=' ';out+=*p=='.'?"point":digits[*p-'0'];}
 return out+(latitude?(value<0?" south":" north"):(value<0?" west":" east"));
}
}
