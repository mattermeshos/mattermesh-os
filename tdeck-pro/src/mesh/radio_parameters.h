#pragma once
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include "power_limits.h"
namespace matter {
enum RadioField { Frequency,Bandwidth,SpreadFactor,CodingRate,TxPower,RadioFieldCount };
static constexpr float RADIO_BANDWIDTHS[]={7.8f,10.4f,15.6f,20.8f,31.25f,41.7f,62.5f,125.0f,250.0f,500.0f};
inline bool validRadioField(int field,float value){
 if(!std::isfinite(value))return false;
 switch(field){
 case Frequency:return (value>=863&&value<=870)||(value>=902&&value<=928);
 case Bandwidth:for(float bw:RADIO_BANDWIDTHS)if(value==bw)return true;return false;
 case SpreadFactor:return value==std::floor(value)&&value>=5&&value<=12;
 case CodingRate:return value==std::floor(value)&&value>=5&&value<=8;
 case TxPower:return value==std::floor(value)&&value>=MIN_TX_POWER&&value<=MAX_TX_POWER;
 default:return false;
 }
}
inline bool validRadioParameters(float frequency,float bandwidth,int sf,int cr,int power){
 return validRadioField(Frequency,frequency)&&validRadioField(Bandwidth,bandwidth)&&validRadioField(SpreadFactor,sf)&&validRadioField(CodingRate,cr)&&validRadioField(TxPower,power);
}
inline bool parseRadioField(int field,const char *text,float &value){
 if(!text||!*text)return false;
 const char *p=text;if(*p=='-'||*p=='+')p++;
 bool digit=false,dot=false;
 for(;*p;p++){if(*p>='0'&&*p<='9')digit=true;else if(*p=='.'&&!dot&&field<=Bandwidth)dot=true;else return false;}
 if(!digit)return false;
 errno=0;char *end;float parsed=std::strtof(text,&end);
 if(errno||*end||!validRadioField(field,parsed))return false;
 value=parsed;return true;
}
}
