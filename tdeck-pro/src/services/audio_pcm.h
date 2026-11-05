#pragma once
#include <stdint.h>
#include <math.h>
namespace device {
constexpr int SPEAKER_RATE=16000,SPEAKER_PEAK=16000;
// ES8311 start-up transient: ~110 ms at about 8x speech level on real captures.
// Silence the first 100 ms and fade in until 160 ms; duration is unchanged.
constexpr int CAPTURE_MUTE_SAMPLES=16000*100/1000,CAPTURE_FADE_END_SAMPLES=16000*160/1000;
inline void suppressCaptureStart(int16_t *pcm,int samples){
 for(int i=0;i<samples&&i<CAPTURE_FADE_END_SAMPLES;i++)pcm[i]=i<CAPTURE_MUTE_SAMPLES?0:int16_t(int32_t(pcm[i])*(i-CAPTURE_MUTE_SAMPLES)/(CAPTURE_FADE_END_SAMPLES-CAPTURE_MUTE_SAMPLES));
}
// Select the varying speech channel, not a silent DAC reference or a DC rail.
inline int microphoneMono(const int16_t *stereo,int16_t *mono,int count){
 int64_t sum[2]={0,0},energy[2]={0,0};for(int i=0;i<count;i++)for(int c=0;c<2;c++){int v=stereo[2*i+c];sum[c]+=v;energy[c]+=int64_t(v)*v;}
 for(int c=0;c<2;c++)energy[c]-=sum[c]*sum[c]/count;
 int channel=energy[1]>energy[0]?1:0,dc=int(sum[channel]/count),peak=0;
 for(int i=0;i<count;i++){int value=int(stereo[2*i+channel])-dc;if(value>32767)value=32767;if(value<-32768)value=-32768;mono[i]=value;int absolute=value<0?-value:value;if(absolute>peak)peak=absolute;}
 return peak;
}
}
