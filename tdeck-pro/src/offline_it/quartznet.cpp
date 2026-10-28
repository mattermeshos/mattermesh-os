#include "quartznet.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
// Compute kernels: the Arduino default -Os leaves the S3 FPU/SIMD loops slow.
#pragma GCC optimize("O2")
namespace mm_it {
namespace {
constexpr float Pi=3.14159265358979323846f;
constexpr unsigned Norm=1,DW=2,PW=3,Relu=1,SaveResidual=2,AddResidual=4,Classifier=8;
uint32_t u32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
float f32(const uint8_t *p){float v;memcpy(&v,p,4);return v;}
const uint8_t *op(const Model &m,unsigned i){return m.data+HeaderBytes+i*OpBytes;}
bool slice(size_t bytes,uint32_t offset,uint64_t length){return offset<=bytes&&length<=bytes-offset;}
size_t frames(size_t n){return n/160+1;}
size_t align16(size_t n){return (n+15)&~size_t(15);}
struct ArenaPlan {size_t residual=0,depthwise=0,scratch=0,bytes=0;};
ArenaPlan arenaPlan(const Model &m,size_t samples,bool externalScratch=false){
 if(!m.data||!m.ops||samples<400||samples>SampleRate*MaxSeconds)return {};
 // a holds features / pointwise output, b holds depthwise / classifier output.
 // a may grow over res only after every residual addition has completed.
 int lastResidual=-1;
 for(unsigned i=0;i<m.ops;i++)if(u32(op(m,i)+20)&(SaveResidual|AddResidual))lastResidual=int(i);
 size_t primary=align16(frames(samples)*MelBins*sizeof(float)),latePrimary=0,depth=0,residual=0;
 unsigned T=frames(samples),outT=T;
 for(unsigned i=0;i<m.ops;i++){
  const uint8_t *o=op(m,i);unsigned kind=u32(o),co=u32(o+8),flags=u32(o+20);
  if(kind==DW){outT=(T-1)/u32(o+16)+1;depth=std::max(depth,align16(size_t(outT)*co*sizeof(float)));}
  else if(kind==PW){
   if(flags&SaveResidual)residual=std::max(residual,align16(size_t(T)*co*sizeof(float)));
   else if(flags&Classifier)depth=std::max(depth,align16(size_t(T)*co*sizeof(float)));
   else{T=outT;size_t bytes=align16(size_t(T)*co*sizeof(float));if(int(i)<=lastResidual)primary=std::max(primary,bytes);else latePrimary=std::max(latePrimary,bytes);}
  }
 }
 ArenaPlan plan;plan.residual=primary;plan.depthwise=std::max(primary+residual,latePrimary);plan.scratch=plan.depthwise+depth;plan.bytes=15+plan.scratch+(externalScratch?0:ScratchBytes);return plan;
}
bool service(Hooks h,unsigned p){return !h.service||h.service(h.context,p);}
struct Complex {float re,im;};
void fft(const Complex *input,Complex *out,unsigned n,unsigned stride,const Complex *twiddle){
 if(n==1){out[0]=input[0];return;}
 unsigned radix=n%2==0?2:5,m=n/radix;
 for(unsigned j=0;j<radix;j++)fft(input+j*stride,out+j*m,m,stride*radix,twiddle);
 for(unsigned k=0;k<m;k++){
  Complex tmp[5];
  for(unsigned l=0;l<radix;l++){
   Complex sum={0,0};
   for(unsigned j=0;j<radix;j++){
    const auto a=out[j*m+k],w=twiddle[(j*(k+l*m)*(400/n))%400];
    sum.re+=a.re*w.re-a.im*w.im;sum.im+=a.re*w.im+a.im*w.re;
   }
   tmp[l]=sum;
  }
  for(unsigned l=0;l<radix;l++)out[l*m+k]=tmp[l];
 }
}
// 16-element input/weight alignment is guaranteed by the exporter/workspace.
// Only the scalar kernel is used on host; the S3 kernel is built/tested separately.
#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(MM_IT_SCALAR_ONLY)
__attribute__((noinline,optimize("no-branch-count-reg")))
int32_t dot(const int8_t *a,const int8_t *b,unsigned n){
 int32_t sum;unsigned count=n/16;
 __asm__ volatile(
  "ee.zero.accx\n"
  "loopnez %3, 1f\n"
  "ee.vld.128.ip q0, %1, 16\n"
  "ee.vld.128.ip q1, %2, 16\n"
  "ee.vmulas.s8.accx q0, q1\n"
  "1:\n"
  "rur.accx_0 %0\n"
  :"=r"(sum),"+r"(a),"+r"(b),"+r"(count)::"memory");
 return sum;
}
#else
int32_t dot(const int8_t *a,const int8_t *b,unsigned n){int32_t sum=0;for(unsigned i=0;i<n;i++)sum+=int32_t(a[i])*int32_t(b[i]);return sum;}
#endif
// Two-core split: part p of parts owns 8-channel blocks p, p+parts, ... so
// each core writes whole, distinct cache lines. Only the caller passes hooks
// (progress, yield, cancellation); the helper runs a bounded slice and returns.
struct PwJob {const Model *model;const uint8_t *weights,*scales,*bias;const float *input,*res,*qs;float *output;const int8_t *quant;unsigned ci,co,row,flags,first,count;};
bool pwBlocks(const PwJob &j,unsigned part,unsigned parts,const Hooks *hooks,unsigned progress){
 const Model &model=*j.model;
 // Eight output channels per frame fill one cache line in PSRAM; the eight
 // weight rows stay cached across the tile. Per-output arithmetic unchanged.
 for(unsigned cb=part*8;cb<j.co;cb+=8*parts){
  unsigned cn=std::min(8u,j.co-cb);
  for(unsigned t=0;t<j.count;t++)for(unsigned c=cb;c<cb+cn;c++){
   float ws=model.dtype==1?f32(j.scales+4*c):1.f,value=f32(j.bias+4*c);
   if(model.dtype==1)value+=float(dot(reinterpret_cast<const int8_t*>(j.weights)+size_t(c)*j.row,j.quant+t*j.row,j.row))*(j.qs[t]*ws);
   else{const float *in=j.input+size_t(j.first+t)*j.ci;for(unsigned k=0;k<j.ci;k++)value+=in[k]*f32(j.weights+(size_t(c)*j.row+k)*4);}
   size_t index=size_t(j.first+t)*j.co+c;
   if(j.flags&AddResidual)value+=j.res[index];
   j.output[index]=(j.flags&Relu)?std::max(0.f,value):value;
  }
  if(hooks&&!service(*hooks,progress))return false;
 }
 return true;
}
void pwHelper(void *job){pwBlocks(*static_cast<PwJob*>(job),1,2,nullptr,0);}
struct DwJob {const Model *model;const uint8_t *w,*s;const float *input;float *output;unsigned ci,kernel,stride,row,T,outT;int pad;};
bool dwBlocks(const DwJob &j,unsigned part,unsigned parts,float *cache,const Hooks *hooks,unsigned progress){
 // Cache a narrow channel strip in this core's 32 KiB internal scratch.
 // Each activation crosses PSRAM once instead of once per kernel tap.
 // Preserve each output accumulation order exactly. T <= 1001 at 10s.
 constexpr unsigned Block=8;const Model &model=*j.model;
 float weight[127][Block];
 for(unsigned first=part*Block;first<j.ci;first+=Block*parts){
  unsigned count=std::min(Block,j.ci-first);
  for(unsigned k=0;k<j.kernel;k++)for(unsigned c=0;c<count;c++)
   weight[k][c]=model.dtype==1?float(reinterpret_cast<const int8_t*>(j.w)[size_t(first+c)*j.row+k]):f32(j.w+(size_t(first+c)*j.row+k)*4);
  for(unsigned t=0;t<j.T;t++)memcpy(cache+size_t(t)*Block,j.input+size_t(t)*j.ci+first,count*sizeof(float));
  for(unsigned t=0;t<j.outT;t++){
   float sum[Block]={};
   unsigned begin=unsigned(std::max(0,j.pad-int(t*j.stride)));
   unsigned end=unsigned(std::min(int(j.kernel),int(j.T)+j.pad-int(t*j.stride)));
   if(count==Block){
    // Explicit independent accumulators let the S3 keep them in FP registers.
    // Keep each channel's k order unchanged, including boundary padding.
    float s0=0,s1=0,s2=0,s3=0,s4=0,s5=0,s6=0,s7=0;
    const float *in=cache+size_t(int(t*j.stride+begin)-j.pad)*Block;
    for(unsigned k=begin;k<end;k++,in+=Block){
     const float *wk=weight[k];
     s0+=in[0]*wk[0];s1+=in[1]*wk[1];s2+=in[2]*wk[2];s3+=in[3]*wk[3];
     s4+=in[4]*wk[4];s5+=in[5]*wk[5];s6+=in[6]*wk[6];s7+=in[7]*wk[7];
    }
    sum[0]=s0;sum[1]=s1;sum[2]=s2;sum[3]=s3;sum[4]=s4;sum[5]=s5;sum[6]=s6;sum[7]=s7;
   }else for(unsigned k=begin;k<end;k++){
    const float *in=cache+size_t(int(t*j.stride+k)-j.pad)*Block;
    for(unsigned c=0;c<count;c++)sum[c]+=in[c]*weight[k][c];
   }
   for(unsigned c=0;c<count;c++)j.output[size_t(t)*j.ci+first+c]=sum[c]*(model.dtype==1?f32(j.s+4*(first+c)):1.f);
   if(hooks&&(t&15)==15&&!service(*hooks,progress))return false;
  }
  if(hooks&&!service(*hooks,progress))return false;
 }
 return true;
}
struct DwHelper {DwJob job;float *cache;};
void dwHelper(void *helper){auto *h=static_cast<DwHelper*>(helper);dwBlocks(h->job,1,2,h->cache,nullptr,0);}
struct Run {
 const Model &model;Hooks hooks;Stats &stats;unsigned total;
 // Runs part 0 here and part 1 on the helper core when the host provides one;
 // always joins before returning, also on cancellation.
 template<class Job,class Blocks> bool split(void (*helper)(void*),void *helperArg,const Job &job,Blocks blocks){
  bool forked=hooks.fork&&hooks.join&&hooks.fork(hooks.context,helper,helperArg);
  bool ok=blocks(job,forked?2u:1u);
  if(forked)hooks.join(hooks.context);
  return ok;
 }
 bool pointwise(const uint8_t *o,const float *input,float *output,const float *res,unsigned T,int8_t *quant){
  unsigned ci=u32(o+4),co=u32(o+8),flags=u32(o+20),row=u32(o+24);
  const uint8_t *weights=model.data+u32(o+28),*scales=model.data+u32(o+32),*bias=model.data+u32(o+36);
  // Widest frame tile that fits the 32 KiB quant scratch (32..128 frames):
  // every tile re-reads the layer weights from flash, so fewer tiles is faster.
  constexpr unsigned MaxTile=128;const unsigned Tile=std::min(MaxTile,unsigned(ScratchBytes/row));float qs[MaxTile];
  for(unsigned first=0;first<T;first+=Tile){
   unsigned count=std::min(Tile,T-first);
   if(model.dtype==1)for(unsigned t=0;t<count;t++){
    const float *in=input+size_t(first+t)*ci;float peak=0;
    for(unsigned c=0;c<ci;c++)peak=std::max(peak,std::fabs(in[c]));
    qs[t]=peak>0?peak/127.f:1.f;
    for(unsigned c=0;c<ci;c++)quant[t*row+c]=int8_t(std::max(-127.f,std::min(127.f,std::nearbyint(in[c]/qs[t]))));
    memset(quant+t*row+ci,0,row-ci);
   }
   PwJob job{&model,weights,scales,bias,input,res,qs,output,quant,ci,co,row,flags,first,count};unsigned progress=stats.opsDone*100/total;
   if(!split(pwHelper,&job,job,[&](const PwJob &j,unsigned parts){return pwBlocks(j,0,parts,&hooks,progress);}))return false;
   stats.macs+=uint64_t(count)*ci*co;
  }
  return true;
 }
 bool depthwise(const uint8_t *o,const float *input,float *output,unsigned T,unsigned &outT,int8_t *scratch){
  unsigned ci=u32(o+4),kernel=u32(o+12),stride=u32(o+16),row=u32(o+24);outT=(T-1)/stride+1;
  const uint8_t *w=model.data+u32(o+28),*s=model.data+u32(o+32);int pad=(kernel-1)/2;
  if(kernel==1){
   // A single tap needs no sliding strip. Stream contiguous activations and
   // cache weights/scales once, avoiding a transpose through the scratch.
   float *weight=reinterpret_cast<float*>(scratch),*scale=weight+ci;
   for(unsigned c=0;c<ci;c++){
    weight[c]=model.dtype==1?float(reinterpret_cast<const int8_t*>(w)[size_t(c)*row]):f32(w+size_t(c)*row*4);
    scale[c]=model.dtype==1?f32(s+4*c):1.f;
   }
   for(unsigned t=0;t<outT;t++){
    const float *in=input+size_t(t*stride)*ci;float *out=output+size_t(t)*ci;
    for(unsigned c=0;c<ci;c++){float sum=0.f;sum+=in[c]*weight[c];out[c]=sum*scale[c];}
    if(!service(hooks,stats.opsDone*100/total))return false;
   }
   stats.macs+=uint64_t(outT)*ci;return true;
  }
  DwHelper helper{{&model,w,s,input,output,ci,kernel,stride,row,T,outT,pad},reinterpret_cast<float*>(hooks.helperScratch)};unsigned progress=stats.opsDone*100/total;
  // The helper needs its own strip; without one the caller does every block.
  Hooks local=hooks;if(!hooks.helperScratch)local.fork=nullptr;
  Run single{model,local,stats,total};
  if(!single.split(dwHelper,&helper,helper.job,[&](const DwJob &j,unsigned parts){return dwBlocks(j,0,parts,reinterpret_cast<float*>(scratch),&hooks,progress);}))return false;
  stats.macs+=uint64_t(outT)*ci*kernel;return true;
 }
};
}
namespace {
// Fixed read-only CRC table; cooperative checks also cover cold model loading.
const uint32_t CrcTable[256]={
 0x00000000u,0x77073096u,0xee0e612cu,0x990951bau,0x076dc419u,0x706af48fu,0xe963a535u,0x9e6495a3u,
 0x0edb8832u,0x79dcb8a4u,0xe0d5e91eu,0x97d2d988u,0x09b64c2bu,0x7eb17cbdu,0xe7b82d07u,0x90bf1d91u,
 0x1db71064u,0x6ab020f2u,0xf3b97148u,0x84be41deu,0x1adad47du,0x6ddde4ebu,0xf4d4b551u,0x83d385c7u,
 0x136c9856u,0x646ba8c0u,0xfd62f97au,0x8a65c9ecu,0x14015c4fu,0x63066cd9u,0xfa0f3d63u,0x8d080df5u,
 0x3b6e20c8u,0x4c69105eu,0xd56041e4u,0xa2677172u,0x3c03e4d1u,0x4b04d447u,0xd20d85fdu,0xa50ab56bu,
 0x35b5a8fau,0x42b2986cu,0xdbbbc9d6u,0xacbcf940u,0x32d86ce3u,0x45df5c75u,0xdcd60dcfu,0xabd13d59u,
 0x26d930acu,0x51de003au,0xc8d75180u,0xbfd06116u,0x21b4f4b5u,0x56b3c423u,0xcfba9599u,0xb8bda50fu,
 0x2802b89eu,0x5f058808u,0xc60cd9b2u,0xb10be924u,0x2f6f7c87u,0x58684c11u,0xc1611dabu,0xb6662d3du,
 0x76dc4190u,0x01db7106u,0x98d220bcu,0xefd5102au,0x71b18589u,0x06b6b51fu,0x9fbfe4a5u,0xe8b8d433u,
 0x7807c9a2u,0x0f00f934u,0x9609a88eu,0xe10e9818u,0x7f6a0dbbu,0x086d3d2du,0x91646c97u,0xe6635c01u,
 0x6b6b51f4u,0x1c6c6162u,0x856530d8u,0xf262004eu,0x6c0695edu,0x1b01a57bu,0x8208f4c1u,0xf50fc457u,
 0x65b0d9c6u,0x12b7e950u,0x8bbeb8eau,0xfcb9887cu,0x62dd1ddfu,0x15da2d49u,0x8cd37cf3u,0xfbd44c65u,
 0x4db26158u,0x3ab551ceu,0xa3bc0074u,0xd4bb30e2u,0x4adfa541u,0x3dd895d7u,0xa4d1c46du,0xd3d6f4fbu,
 0x4369e96au,0x346ed9fcu,0xad678846u,0xda60b8d0u,0x44042d73u,0x33031de5u,0xaa0a4c5fu,0xdd0d7cc9u,
 0x5005713cu,0x270241aau,0xbe0b1010u,0xc90c2086u,0x5768b525u,0x206f85b3u,0xb966d409u,0xce61e49fu,
 0x5edef90eu,0x29d9c998u,0xb0d09822u,0xc7d7a8b4u,0x59b33d17u,0x2eb40d81u,0xb7bd5c3bu,0xc0ba6cadu,
 0xedb88320u,0x9abfb3b6u,0x03b6e20cu,0x74b1d29au,0xead54739u,0x9dd277afu,0x04db2615u,0x73dc1683u,
 0xe3630b12u,0x94643b84u,0x0d6d6a3eu,0x7a6a5aa8u,0xe40ecf0bu,0x9309ff9du,0x0a00ae27u,0x7d079eb1u,
 0xf00f9344u,0x8708a3d2u,0x1e01f268u,0x6906c2feu,0xf762575du,0x806567cbu,0x196c3671u,0x6e6b06e7u,
 0xfed41b76u,0x89d32be0u,0x10da7a5au,0x67dd4accu,0xf9b9df6fu,0x8ebeeff9u,0x17b7be43u,0x60b08ed5u,
 0xd6d6a3e8u,0xa1d1937eu,0x38d8c2c4u,0x4fdff252u,0xd1bb67f1u,0xa6bc5767u,0x3fb506ddu,0x48b2364bu,
 0xd80d2bdau,0xaf0a1b4cu,0x36034af6u,0x41047a60u,0xdf60efc3u,0xa867df55u,0x316e8eefu,0x4669be79u,
 0xcb61b38cu,0xbc66831au,0x256fd2a0u,0x5268e236u,0xcc0c7795u,0xbb0b4703u,0x220216b9u,0x5505262fu,
 0xc5ba3bbeu,0xb2bd0b28u,0x2bb45a92u,0x5cb36a04u,0xc2d7ffa7u,0xb5d0cf31u,0x2cd99e8bu,0x5bdeae1du,
 0x9b64c2b0u,0xec63f226u,0x756aa39cu,0x026d930au,0x9c0906a9u,0xeb0e363fu,0x72076785u,0x05005713u,
 0x95bf4a82u,0xe2b87a14u,0x7bb12baeu,0x0cb61b38u,0x92d28e9bu,0xe5d5be0du,0x7cdcefb7u,0x0bdbdf21u,
 0x86d3d2d4u,0xf1d4e242u,0x68ddb3f8u,0x1fda836eu,0x81be16cdu,0xf6b9265bu,0x6fb077e1u,0x18b74777u,
 0x88085ae6u,0xff0f6a70u,0x66063bcau,0x11010b5cu,0x8f659effu,0xf862ae69u,0x616bffd3u,0x166ccf45u,
 0xa00ae278u,0xd70dd2eeu,0x4e048354u,0x3903b3c2u,0xa7672661u,0xd06016f7u,0x4969474du,0x3e6e77dbu,
 0xaed16a4au,0xd9d65adcu,0x40df0b66u,0x37d83bf0u,0xa9bcae53u,0xdebb9ec5u,0x47b2cf7fu,0x30b5ffe9u,
 0xbdbdf21cu,0xcabac28au,0x53b39330u,0x24b4a3a6u,0xbad03605u,0xcdd70693u,0x54de5729u,0x23d967bfu,
 0xb3667a2eu,0xc4614ab8u,0x5d681b02u,0x2a6f2b94u,0xb40bbe37u,0xc30c8ea1u,0x5a05df1bu,0x2d02ef8du,
};
bool checkedCrc(const uint8_t *p,size_t n,Hooks hooks,uint32_t &value){
 uint32_t crc=~0u;
 for(size_t i=0;i<n;i++){if(!(i&4095)&&!service(hooks,0))return false;crc=(crc>>8)^CrcTable[(crc^p[i])&255];}
 value=~crc;return true;
}
}
bool kernelSelfTest(){
 alignas(16) int8_t a[1024],b[1024];
 for(unsigned i=0;i<1024;i++){a[i]=int8_t((i*73+17)%256-128);b[i]=int8_t((i*19+7)%256-128);}
 for(unsigned n=16;n<=1024;n+=16){int32_t expected=0;for(unsigned i=0;i<n;i++)expected+=int32_t(a[i])*int32_t(b[i]);if(dot(a,b,n)!=expected)return false;}
 return true;
}
uint32_t crc32(const uint8_t *p,size_t n){uint32_t value=0;checkedCrc(p,n,{},value);return value;}
bool open(Model &m,const void *data,size_t bytes,bool checkCrc,Hooks hooks){
 m=Model{};if(!data||(reinterpret_cast<uintptr_t>(data)&15)||bytes<HeaderBytes||bytes>64u*1024*1024)return false;auto p=static_cast<const uint8_t*>(data);
 if(memcmp(p,"MMQIT001",8)||u32(p+8)!=1||u32(p+12)!=bytes||u32(p+24)!=MelBins||u32(p+28)!=Classes||u32(p+32)!=SampleRate)return false;
 unsigned count=u32(p+16),dtype=u32(p+20);if(count!=63||(dtype!=1&&dtype!=4)||HeaderBytes+count*OpBytes>bytes)return false;
 uint32_t payloadCrc=0;if(crc32(p,60)!=u32(p+60)||(checkCrc&&(!checkedCrc(p+HeaderBytes,bytes-HeaderBytes,hooks,payloadCrc)||payloadCrc!=u32(p+36))))return false;
 unsigned current=MelBins,depth=0,residual=0;bool hasDepth=false,classified=false;
 for(unsigned i=0;i<count;i++){
  auto o=p+HeaderBytes+i*OpBytes;unsigned kind=u32(o),ci=u32(o+4),co=u32(o+8),k=u32(o+12),stride=u32(o+16),flags=u32(o+20),row=u32(o+24);
  uint32_t wo=u32(o+28),so=u32(o+32),bo=u32(o+36);uint64_t size;
  if(!ci||ci>1024||!co||co>1024||flags&~15u||wo%16||so%4||bo%4||wo<HeaderBytes+count*OpBytes)return false;
  if(kind==Norm){if(i||ci!=80||co!=80||flags)return false;size=ci*4;if(!slice(bytes,bo,ci*4))return false;}
  else if(kind==DW){if(hasDepth||ci!=current||co!=ci||!k||k>127||!(k&1)||stride<1||stride>2||flags||row!=align16(k))return false;size=uint64_t(ci)*row*dtype;depth=ci;hasDepth=true;}
  else if(kind==PW){
   if(k!=1||stride!=1||row!=align16(ci))return false;
   if(flags&SaveResidual){if(flags!=SaveResidual||hasDepth||ci!=current||residual)return false;residual=co;}
   else if(flags&Classifier){if(flags!=Classifier||i!=count-1||hasDepth||residual||ci!=current||co!=Classes)return false;classified=true;}
   else{if(!hasDepth||ci!=depth)return false;hasDepth=false;current=co;if(flags&AddResidual){if(residual!=co)return false;residual=0;}}
   size=uint64_t(co)*row*dtype;if(!slice(bytes,bo,uint64_t(co)*4))return false;
  }else return false;
  if(!slice(bytes,wo,size)||wo+size>bytes)return false;
  if(kind!=Norm&&dtype==1&&!slice(bytes,so,uint64_t(co)*4))return false;
  if(kind==Norm||kind==PW)for(unsigned c=0;c<co;c++)if(!std::isfinite(f32(p+bo+c*4)))return false;
  if(kind==Norm){for(unsigned c=0;c<ci;c++)if(!std::isfinite(f32(p+wo+c*4)))return false;}
  else if(dtype==1)for(unsigned c=0;c<co;c++){float scale=f32(p+so+c*4);if(!std::isfinite(scale)||scale<=0)return false;}
 }
 if(!classified)return false;m.data=p;m.bytes=bytes;m.ops=count;m.dtype=dtype;return true;
}
size_t workspaceBytes(size_t samples){if(samples<400||samples>SampleRate*MaxSeconds)return 0;size_t T=(frames(samples)+1)/2;return 15+T*(1024+512+512)*sizeof(float)+32*1024;}
size_t workspaceBytes(const Model &model,size_t samples,bool externalScratch){return arenaPlan(model,samples,externalScratch).bytes;}
Error features(const int16_t *pcm,size_t n,float *out,size_t capacity,Hooks hooks){
 if(!pcm||!out||n<400||n>SampleRate*MaxSeconds||capacity<frames(n)*MelBins)return InvalidAudio;
 Complex twiddle[400],in[400],freq[400];float window[400],mel[82];
 for(unsigned i=0;i<400;i++){float angle=2*Pi*i/400;twiddle[i]={std::cos(angle),-std::sin(angle)};window[i]=.5f-.5f*std::cos(angle);}
 float upper=2595.f*std::log10(1.f+8000.f/700.f);
 for(unsigned b=0;b<82;b++)mel[b]=700.f*(std::pow(10.f,(upper*b/81)/2595.f)-1);
 // Fixed HTK filterbank: at most 16 nonzero/support bins per band here.
 // Compute divisions once per utterance, not 80*201 times per audio frame.
 struct Band {unsigned first,last;float weights[20];};Band bands[80];
 for(unsigned b=0;b<80;b++){
  auto &band=bands[b];band.first=unsigned(std::floor(mel[b]/40));band.last=std::min(200u,unsigned(std::ceil(mel[b+2]/40)));
  if(band.last-band.first>=20)return InvalidAudio;
  for(unsigned k=band.first;k<=band.last;k++){float hz=float(k)*40;band.weights[k-band.first]=std::max(0.f,std::min((hz-mel[b])/(mel[b+1]-mel[b]),(mel[b+2]-hz)/(mel[b+2]-mel[b+1])));}
 }
 float maximum=-std::numeric_limits<float>::infinity();size_t T=frames(n);
 for(size_t t=0;t<T;t++){
  for(int i=0;i<400;i++){
   int pos=int(t*160)+i-200;if(pos<0)pos=-pos;if(pos>=int(n))pos=2*int(n)-2-pos;
   in[i]={float(pcm[pos])/32768.f*window[i],0};
  }
  fft(in,freq,400,1,twiddle);
  float power[201];for(unsigned k=0;k<=200;k++)power[k]=freq[k].re*freq[k].re+freq[k].im*freq[k].im;
  for(unsigned b=0;b<80;b++){
   float sum=0;
   for(unsigned k=bands[b].first;k<=bands[b].last;k++)sum+=power[k]*bands[b].weights[k-bands[b].first];
   float db=10.f*std::log10(std::max(1e-10f,sum));out[t*80+b]=db;maximum=std::max(maximum,db);
  }
  if((t&3)==3&&!service(hooks,0))return Cancelled;
 }
 for(size_t i=0;i<T*80;i++)out[i]=std::max(out[i],maximum-80.f);return Ok;
}
Error transcribe(const Model &m,const int16_t *pcm,size_t n,void *workspace,size_t capacity,char *text,size_t maxText,Stats &stats,Hooks hooks,float *logits,size_t logitCapacity){
 stats=Stats{};if(!text||maxText<2)return OutputTooSmall;text[0]=0;
 if(!m.data||!m.ops)return InvalidModel;auto plan=arenaPlan(m,n,hooks.quantScratch!=nullptr);size_t required=plan.bytes;if(!required)return InvalidAudio;if(!workspace||capacity<required||(hooks.quantScratch&&(reinterpret_cast<uintptr_t>(hooks.quantScratch)&15)))return NoMemory;
 stats.workspaceBytes=required;
 uintptr_t aligned=(reinterpret_cast<uintptr_t>(workspace)+15)&~uintptr_t(15);
 auto *base=reinterpret_cast<uint8_t*>(aligned);float *a=reinterpret_cast<float*>(base),*b=reinterpret_cast<float*>(base+plan.depthwise),*res=reinterpret_cast<float*>(base+plan.residual);
 int8_t *quant=hooks.quantScratch?hooks.quantScratch:reinterpret_cast<int8_t*>(base+plan.scratch);
 Error e=features(pcm,n,a,plan.depthwise/sizeof(float),hooks);if(e!=Ok)return e;
 unsigned T=frames(n),outT=T;Run run{m,hooks,stats,m.ops};
 for(unsigned index=0;index<m.ops;index++){
  if(!service(hooks,index*100/m.ops))return Cancelled;
  const uint8_t *o=op(m,index);unsigned kind=u32(o),ci=u32(o+4),flags=u32(o+20);
  if(kind==Norm){
   const uint8_t *gamma=m.data+u32(o+28),*beta=m.data+u32(o+36);
   for(unsigned c=0;c<ci;c++){
    double mean=0,var=0;for(unsigned t=0;t<T;t++)mean+=a[t*ci+c];mean/=T;
    for(unsigned t=0;t<T;t++){double d=a[t*ci+c]-mean;var+=d*d;}var/=T;
    float scale=f32(gamma+4*c)/std::sqrt(float(var)+1e-5f),shift=f32(beta+4*c);
    for(unsigned t=0;t<T;t++)a[t*ci+c]=(a[t*ci+c]-float(mean))*scale+shift;
   }
  }else if(kind==DW){if(!run.depthwise(o,a,b,T,outT,quant))return Cancelled;}
  else if(flags&SaveResidual){if(!run.pointwise(o,a,res,nullptr,T,quant))return Cancelled;}
  else if(flags&Classifier){if(!run.pointwise(o,a,b,nullptr,T,quant))return Cancelled;}
  else{T=outT;if(!run.pointwise(o,b,a,(flags&AddResidual)?res:nullptr,T,quant))return Cancelled;}
  if(hooks.trace)hooks.trace(hooks.context,index,kind==DW||flags&Classifier?b:flags&SaveResidual?res:a,kind==DW?outT:T,u32(o+8));
  stats.opsDone=index+1;
 }
 stats.frames=T;
 if(logits){if(logitCapacity<size_t(T)*Classes)return OutputTooSmall;memcpy(logits,b,T*Classes*sizeof(float));}
 const char *letters[]={"",""," ","a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z","à","è","é","ì","ò","ù","'"};
 size_t used=0;unsigned previous=Classes;
 for(unsigned t=0;t<T;t++){
  unsigned best=0;for(unsigned c=1;c<Classes;c++)if(b[t*Classes+c]>b[t*Classes+best])best=c;
  if(best>1&&best!=previous){size_t len=strlen(letters[best]);if(used+len>=maxText){text[0]=0;return OutputTooSmall;}memcpy(text+used,letters[best],len);used+=len;}
  previous=best;
 }
 while(used&&text[used-1]==' ')used--;text[used]=0;
 size_t leading=0;while(text[leading]==' ')leading++;if(leading)memmove(text,text+leading,used-leading+1);
 if(!service(hooks,100)){text[0]=0;return Cancelled;}return Ok;
}
}
