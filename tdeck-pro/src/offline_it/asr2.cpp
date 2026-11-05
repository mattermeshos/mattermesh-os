#include "asr2.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <cstdlib>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif
#pragma GCC optimize("O2")
namespace mm_asr {
uint32_t crc32(const uint8_t *p,size_t n,uint32_t crc){
 static uint32_t table[256];static bool ready=false;
 if(!ready){for(uint32_t i=0;i<256;i++){uint32_t c=i;for(int k=0;k<8;k++)c=c&1?0xedb88320u^(c>>1):c>>1;table[i]=c;}ready=true;}
 crc=~crc;for(size_t i=0;i<n;i++)crc=table[(crc^p[i])&0xff]^(crc>>8);return ~crc;
}
namespace {
constexpr float Pi=3.14159265358979323846f;
uint32_t u32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
float f32(const uint8_t *p){float v;memcpy(&v,p,4);return v;}
size_t align16(size_t n){return (n+15)&~size_t(15);}
bool service(const Hooks &h,unsigned p){return !h.service||h.service(h.context,p);}
unsigned framesFor(size_t samples){return unsigned(samples/160+1);}
unsigned paddedFor(const Model &m,unsigned frames){unsigned p=m.padTo?m.padTo:1;return (frames+p-1)/p*p;}
struct ArenaPlan {size_t residual=0,depthwise=0,scratch=0,bytes=0;};
ArenaPlan arenaPlan(const Model &m,size_t samples,bool externalScratch){
 if(!m.valid()||samples<400||samples>size_t(SampleRate)*MaxSeconds)return {};
 unsigned T=paddedFor(m,framesFor(samples));if(T>MaxFrames)return {};
 int lastResidual=-1;for(unsigned i=0;i<m.ops;i++)if(m.op[i].flags&(SaveResidual|AddResidual))lastResidual=int(i);
 size_t primary=align16(size_t(T)*m.mels*sizeof(float)),latePrimary=0,depth=0,residual=0;unsigned outT=T;
 for(unsigned i=0;i<m.ops;i++){
  const Op &o=m.op[i];
  if(o.kind==Depthwise){outT=(T-1)/o.stride+1;depth=std::max(depth,align16(size_t(outT)*o.co*sizeof(float)));}
  else if(o.kind==Pointwise){
   if(o.flags&SaveResidual)residual=std::max(residual,align16(size_t(T)*o.co*sizeof(float)));
   else if(o.flags&Classifier)depth=std::max(depth,align16(size_t(T)*o.co*sizeof(float)));
   else{T=outT;size_t bytes=align16(size_t(T)*o.co*sizeof(float));if(int(i)<=lastResidual)primary=std::max(primary,bytes);else latePrimary=std::max(latePrimary,bytes);}
  }
 }
 ArenaPlan plan;plan.residual=primary;plan.depthwise=std::max(primary+residual,latePrimary);plan.scratch=plan.depthwise+depth;plan.bytes=15+plan.scratch+(externalScratch?0:ScratchBytes);return plan;
}
// ---- int8 dot product -------------------------------------------------------
#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(MM_IT_SCALAR_ONLY)
// Software-pipelined: each ee.vmulas.s8.accx.ld.ip accumulates the previous pair while loading the
// next 16 weights; exactly n/16 loads per operand, so nothing is read past the row (the quant tile
// ends exactly at the scratch boundary). n is a multiple of 16 and >= 16 (open() checks rows).
// Inlined into the block loops; their functions carry no-branch-count-reg so the compiler never
// nests its own zero-overhead loop around this LOOP.
__attribute__((always_inline)) inline
int32_t dot(const int8_t *a,const int8_t *b,unsigned n){
 int32_t sum;unsigned count=n/16,pairs=(count-1)/2,odd=(count-1)&1;
 __asm__ volatile(
  "ee.zero.accx\n"
  "ee.vld.128.ip q0, %1, 16\n"
  "ee.vld.128.ip q1, %2, 16\n"
  "loopnez %3, 1f\n"
  "ee.vmulas.s8.accx.ld.ip q2, %1, 16, q0, q1\n"
  "ee.vld.128.ip q3, %2, 16\n"
  "ee.vmulas.s8.accx.ld.ip q0, %1, 16, q2, q3\n"
  "ee.vld.128.ip q1, %2, 16\n"
  "1:\n"
  "beqz %4, 2f\n"
  "ee.vmulas.s8.accx.ld.ip q2, %1, 16, q0, q1\n"
  "ee.vld.128.ip q3, %2, 16\n"
  "ee.vmulas.s8.accx q2, q3\n"
  "j 3f\n"
  "2:\n"
  "ee.vmulas.s8.accx q0, q1\n"
  "3:\n"
  "rur.accx_0 %0\n"
  :"=r"(sum),"+r"(a),"+r"(b),"+r"(pairs),"+r"(odd)::"memory");
 return sum;
}
#else
int32_t dot(const int8_t *a,const int8_t *b,unsigned n){int32_t sum=0;for(unsigned i=0;i<n;i++)sum+=int32_t(a[i])*int32_t(b[i]);return sum;}
#endif
// ---- pointwise ---------------------------------------------------------------
struct PwJob {const Op *op;unsigned dtype;const uint8_t *weights,*scales,*bias;const float *input,*res,*qs;float *output;const int8_t *quant;unsigned first,count;};
#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(MM_IT_SCALAR_ONLY)
__attribute__((optimize("no-branch-count-reg")))
#endif
bool pwBlocks(const PwJob &j,unsigned part,unsigned parts,const Hooks *hooks,unsigned progress){
 // 32 output channels per block: each frame then touches 4 consecutive PSRAM cache lines for the
 // output (and the residual), which the ESP32-S3 serves ~1.6x faster than isolated 32-byte lines.
 // Arithmetic order is unchanged from the 8-channel version (bit-identical results).
 // Block size keeps a block's weights near 8 KB (a quarter of the shared data cache): 32 channels up
 // to row 256, 16 at row 512, 8 at row 1024, so the rows stay cached across the frame loop.
 const Op &o=*j.op;constexpr unsigned MaxBlock=32;const unsigned Block=std::max(8u,std::min(MaxBlock,8192u/o.row));float bias[MaxBlock],scale[MaxBlock];
 for(unsigned cb=part*Block;cb<o.co;cb+=Block*parts){
  const unsigned cn=std::min(Block,o.co-cb);
  for(unsigned c=0;c<cn;c++){bias[c]=f32(j.bias+4*(cb+c));scale[c]=f32(j.scales+4*(cb+c));}
  for(unsigned t=0;t<j.count;t++){
   const size_t frame=size_t(j.first+t);const float qt=j.qs[t];const int8_t *q=j.quant+t*o.row;
   float *out=j.output+frame*o.co+cb;const float *res=(o.flags&AddResidual)?j.res+frame*o.co+cb:nullptr;
   for(unsigned c=0;c<cn;c++){
    float value=bias[c];
    if(j.dtype==1)value+=float(dot(reinterpret_cast<const int8_t*>(j.weights)+size_t(cb+c)*o.row,q,o.row))*(qt*scale[c]);
    else{const float *in=j.input+frame*o.ci;const uint8_t *w=j.weights+size_t(cb+c)*o.row*4;for(unsigned k=0;k<o.ci;k++)value+=in[k]*f32(w+4*k);}
    if(res)value+=res[c];
    out[c]=(o.flags&Relu)?std::max(0.f,value):value;
   }
  }
  if(hooks&&!service(*hooks,progress))return false;
 }
 return true;
}
void pwHelper(void *job){pwBlocks(*static_cast<PwJob*>(job),1,2,nullptr,0);}
// ---- depthwise ---------------------------------------------------------------
struct DwJob {const Op *op;unsigned dtype;const uint8_t *w,*s;const float *input;float *output;unsigned T,outT;int pad;};
bool dwBlocks(const DwJob &j,unsigned part,unsigned parts,float *cache,const Hooks *hooks,unsigned progress){
 constexpr unsigned Block=8;const Op &o=*j.op;const unsigned K=o.kernel,D=o.dilation?o.dilation:1,S=o.stride;
 float weight[MaxKernel][Block];
 for(unsigned first=part*Block;first<o.ci;first+=Block*parts){
  unsigned count=std::min(Block,o.ci-first);
  for(unsigned k=0;k<K;k++)for(unsigned c=0;c<count;c++)
   weight[k][c]=j.dtype==1?float(reinterpret_cast<const int8_t*>(j.w)[size_t(first+c)*o.row+k]):f32(j.w+(size_t(first+c)*o.row+k)*4);
  for(unsigned t=0;t<j.T;t++)memcpy(cache+size_t(t)*Block,j.input+size_t(t)*o.ci+first,count*sizeof(float));
  for(unsigned t=0;t<j.outT;t++){
   int base=int(t*S)-j.pad;                       // input index of tap 0
   int a=-base;unsigned begin=a<=0?0:unsigned((a+int(D)-1)/int(D));
   int b=int(j.T)-1-base;unsigned end=b<0?0:std::min(K,unsigned(b/int(D))+1);
   float sum[Block]={};
   if(count==Block&&begin<end){
    float s0=0,s1=0,s2=0,s3=0,s4=0,s5=0,s6=0,s7=0;
    const float *in=cache+size_t(base+int(begin*D))*Block;const size_t step=size_t(D)*Block;
    for(unsigned k=begin;k<end;k++,in+=step){
     const float *wk=weight[k];
     s0+=in[0]*wk[0];s1+=in[1]*wk[1];s2+=in[2]*wk[2];s3+=in[3]*wk[3];
     s4+=in[4]*wk[4];s5+=in[5]*wk[5];s6+=in[6]*wk[6];s7+=in[7]*wk[7];
    }
    sum[0]=s0;sum[1]=s1;sum[2]=s2;sum[3]=s3;sum[4]=s4;sum[5]=s5;sum[6]=s6;sum[7]=s7;
   }else for(unsigned k=begin;k<end;k++){
    const float *in=cache+size_t(base+int(k*D))*Block;
    for(unsigned c=0;c<count;c++)sum[c]+=in[c]*weight[k][c];
   }
   for(unsigned c=0;c<count;c++)j.output[size_t(t)*o.ci+first+c]=sum[c]*(j.dtype==1?f32(j.s+4*(first+c)):1.f);
   if(hooks&&(t&15)==15&&!service(*hooks,progress))return false;
  }
  if(hooks&&!service(*hooks,progress))return false;
 }
 return true;
}
struct DwHelper {DwJob job;float *cache;};
void dwHelper(void *helper){auto *h=static_cast<DwHelper*>(helper);dwBlocks(h->job,1,2,h->cache,nullptr,0);}
struct Run {
 const Model &model;Hooks hooks;Stats &stats;
 template<class Job,class Blocks> bool split(void (*helper)(void*),void *helperArg,const Job &job,Blocks blocks){
  bool forked=hooks.fork&&hooks.join&&hooks.fork(hooks.context,helper,helperArg);
  bool ok=blocks(job,forked?2u:1u);
  if(forked)hooks.join(hooks.context);
  return ok;
 }
 unsigned progress(unsigned index)const{return index*100/model.ops;}
 bool pointwise(unsigned index,const Op &o,const uint8_t *blob,const float *input,float *output,const float *res,unsigned T,int8_t *quant){
  const uint8_t *weights=blob+(o.weights-o.blobOffset),*scales=blob+(o.scales-o.blobOffset),*bias=blob+(o.bias-o.blobOffset);
  constexpr unsigned MaxTile=128;const unsigned Tile=std::max(1u,std::min(MaxTile,unsigned(ScratchBytes/o.row)));float qs[MaxTile];
  for(unsigned first=0;first<T;first+=Tile){
   unsigned count=std::min(Tile,T-first);
   if(model.dtype==1)for(unsigned t=0;t<count;t++){
    const float *in=input+size_t(first+t)*o.ci;float peak=0;
    for(unsigned c=0;c<o.ci;c++)peak=std::max(peak,std::fabs(in[c]));
    qs[t]=peak>0?peak/127.f:1.f;
    for(unsigned c=0;c<o.ci;c++)quant[t*o.row+c]=int8_t(std::max(-127.f,std::min(127.f,std::nearbyint(in[c]/qs[t]))));
    memset(quant+t*o.row+o.ci,0,o.row-o.ci);
   }
   PwJob job{&o,model.dtype,weights,scales,bias,input,res,qs,output,quant,first,count};unsigned p=progress(index);
   if(!split(pwHelper,&job,job,[&](const PwJob &j,unsigned parts){return pwBlocks(j,0,parts,&hooks,p);}))return false;
   stats.macs+=uint64_t(count)*o.ci*o.co;
  }
  return true;
 }
 bool depthwise(unsigned index,const Op &o,const uint8_t *blob,const float *input,float *output,unsigned T,unsigned &outT,int8_t *scratch){
  const uint8_t *w=blob+(o.weights-o.blobOffset),*s=blob+(o.scales-o.blobOffset);unsigned D=o.dilation?o.dilation:1;outT=(T-1)/o.stride+1;int pad=int((o.kernel-1)*D/2);
  if(o.kernel==1&&o.stride==1){
   float *weight=reinterpret_cast<float*>(scratch),*scale=weight+o.ci;
   for(unsigned c=0;c<o.ci;c++){weight[c]=model.dtype==1?float(reinterpret_cast<const int8_t*>(w)[size_t(c)*o.row]):f32(w+size_t(c)*o.row*4);scale[c]=model.dtype==1?f32(s+4*c):1.f;}
   for(unsigned t=0;t<outT;t++){const float *in=input+size_t(t)*o.ci;float *out=output+size_t(t)*o.ci;for(unsigned c=0;c<o.ci;c++)out[c]=in[c]*weight[c]*scale[c];if((t&15)==15&&!service(hooks,progress(index)))return false;}
   stats.macs+=uint64_t(outT)*o.ci;return true;
  }
  DwHelper helper{{&o,model.dtype,w,s,input,output,T,outT,pad},reinterpret_cast<float*>(hooks.helperScratch)};unsigned p=progress(index);
  Hooks local=hooks;if(!hooks.helperScratch)local.fork=nullptr;
  Run single{model,local,stats};
  if(!single.split(dwHelper,&helper,helper.job,[&](const DwJob &j,unsigned parts){return dwBlocks(j,0,parts,reinterpret_cast<float*>(scratch),&hooks,p);}))return false;
  stats.macs+=uint64_t(outT)*o.ci*o.kernel;return true;
 }
};
// ---- FFT 512 (iterative radix-2) ----------------------------------------------
struct Complex {float re,im;};
void fft512(Complex *x,const Complex *tw){
 constexpr unsigned N=512;
 for(unsigned i=1,j=0;i<N;i++){unsigned bit=N>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(x[i],x[j]);}
 for(unsigned len=2;len<=N;len<<=1){
  unsigned step=N/len;
  for(unsigned i=0;i<N;i+=len)for(unsigned k=0;k<len/2;k++){
   Complex w=tw[k*step],u=x[i+k],v=x[i+k+len/2];Complex t={v.re*w.re-v.im*w.im,v.re*w.im+v.im*w.re};
   x[i+k]={u.re+t.re,u.im+t.im};x[i+k+len/2]={u.re-t.re,u.im-t.im};
  }
 }
}
float slaneyHzToMel(float f){const float minLog=1000.f,ml=minLog/(200.f/3.f),step=std::log(6.4f)/27.f;return f>=minLog?ml+std::log(f/minLog)/step:f/(200.f/3.f);}
float slaneyMelToHz(float m){const float minLog=1000.f,ml=minLog/(200.f/3.f),step=std::log(6.4f)/27.f;return m>=ml?minLog*std::exp(step*(m-ml)):m*(200.f/3.f);}
}
// ---- model ---------------------------------------------------------------------
Error open(Model &model,Source source){
 resetModel(model);if(!source.read)return InvalidModel;
 uint8_t h[HeaderBytes];if(!source.read(source.context,0,h,HeaderBytes)||memcmp(h,"MMQIT002",8))return InvalidModel;
 if(u32(h+124)!=crc32(h,124))return InvalidModel;
 uint32_t v[18];memcpy(v,h+8,sizeof(v));model.tableCrc=u32(h+80);
 model.version=v[0];model.bytes=v[1];model.ops=v[2];model.dtype=v[3];model.mels=v[4];model.classes=v[5];model.sampleRate=v[6];model.frontend=v[7];model.window=v[8];model.nfft=v[9];model.hop=v[10];model.padTo=v[11];model.blank=v[12];model.vocabOffset=v[13];model.vocabBytes=v[14];model.maxBlob=v[15];model.flags=v[16];model.maxChannels=v[17];
 if(model.version!=2||model.ops==0||model.ops>MaxOps||(model.dtype!=1&&model.dtype!=4)||model.mels==0||model.mels>MaxMels||model.classes<2||model.classes>MaxClasses||model.sampleRate!=SampleRate||model.frontend!=1||model.window!=320||model.nfft!=512||model.hop!=160||model.padTo==0||model.padTo>64||model.blank>=model.classes)return InvalidModel;
 if(model.vocabOffset!=HeaderBytes+OpBytes*model.ops||model.vocabBytes>model.classes*5||model.vocabOffset+model.vocabBytes>model.bytes)return InvalidModel;
 model.blobStart=uint32_t(align16(size_t(model.vocabOffset)+model.vocabBytes));if(model.blobStart>model.bytes)return InvalidModel;
 uint8_t rec[OpBytes];unsigned current=model.mels;bool classified=false;uint32_t table=0;
 for(unsigned i=0;i<model.ops;i++){
  if(!source.read(source.context,HeaderBytes+i*OpBytes,rec,OpBytes))return InvalidModel;
  table=crc32(rec,OpBytes,table);Op &o=model.op[i];uint32_t w[16];memcpy(w,rec,sizeof(w));
  o.kind=w[0];o.ci=w[1];o.co=w[2];o.kernel=w[3];o.stride=w[4];o.flags=w[5];o.row=w[6];o.blobOffset=w[7];o.blobBytes=w[8];o.weights=w[9];o.scales=w[10];o.bias=w[11];o.dilation=w[12]?w[12]:1;o.crc=w[13];o.storedBytes=w[14]?w[14]:o.blobBytes;o.bits=w[15]?w[15]:8;
  if(classified||o.ci==0||o.co==0||o.ci>4096||o.co>4096||o.kernel==0||o.kernel>MaxKernel||o.stride==0||o.stride>4||o.dilation>4||o.row%16||o.blobBytes==0||o.blobBytes>model.maxBlob||o.blobOffset<model.blobStart||o.storedBytes==0||o.storedBytes>o.blobBytes||uint64_t(o.blobOffset)+o.storedBytes>model.bytes||o.bits<4||o.bits>8||(o.bits==8&&o.storedBytes!=o.blobBytes))return InvalidModel;
  if(o.weights<o.blobOffset||o.scales<o.blobOffset||o.bias<o.blobOffset||o.weights>=o.blobOffset+o.blobBytes||o.scales>=o.blobOffset+o.blobBytes||o.bias>=o.blobOffset+o.blobBytes)return InvalidModel;
  size_t wsize=model.dtype==1?1:4;
  if(o.kind==Depthwise){
   if(packed(o))return InvalidModel;
   if(o.ci!=o.co||o.ci!=current||o.row<o.kernel||uint64_t(o.weights-o.blobOffset)+uint64_t(o.co)*o.row*wsize>o.blobBytes||uint64_t(o.scales-o.blobOffset)+4ull*o.co>o.blobBytes)return InvalidModel;
  }else if(o.kind==Pointwise){
   if(o.kernel!=1||o.stride!=1||o.ci!=current||o.row<o.ci||uint64_t(o.weights-o.blobOffset)+uint64_t(o.co)*o.row*wsize>o.blobBytes||uint64_t(o.scales-o.blobOffset)+4ull*o.co>o.blobBytes||uint64_t(o.bias-o.blobOffset)+4ull*o.co>o.blobBytes)return InvalidModel;
   if(packed(o)){uint64_t n=uint64_t(o.co)*o.row,pw=n*o.bits/8;if(model.dtype!=1||pw+8ull*o.co>o.storedBytes||uint64_t(o.blobBytes)-o.storedBytes<n-pw)return InvalidModel;}
   if(o.flags&Classifier){if(o.co!=model.classes)return InvalidModel;classified=true;}
   else if(!(o.flags&SaveResidual))current=o.co;
  }else return InvalidModel;
 }
 if(!classified)return InvalidModel;
 uint8_t vocab[MaxClasses*5+16];uint32_t tail=model.blobStart-model.vocabOffset;if(tail>sizeof(vocab)||!source.read(source.context,model.vocabOffset,vocab,tail))return InvalidModel;
 if(crc32(vocab,tail,table)!=model.tableCrc)return InvalidModel;
 size_t pos=0;for(unsigned c=0;c<model.classes;c++){if(pos>=model.vocabBytes)return InvalidModel;unsigned len=vocab[pos++];if(len>4||pos+len>model.vocabBytes)return InvalidModel;memcpy(model.vocab[c],vocab+pos,len);model.vocab[c][len]=0;pos+=len;}
 return Ok;
}
void unpackBlob(const Op &o,uint8_t *b){
 if(!packed(o))return;
 const size_t n=size_t(o.co)*o.row,pw=n*o.bits/8,base=o.blobBytes-o.storedBytes;
 // stored: [packed weights pw][f32 scales 4co][f32 bias 4co]; image: [weights n][scales @o.scales][bias @o.bias]
 // Order matters: the packed tail may extend past n into the image's scales slot, so the weights are
 // expanded first (writes stay below n <= base+pw), then scales, then bias (forward memmoves).
 const uint8_t *src=b+base;int8_t *dst=reinterpret_cast<int8_t*>(b);const unsigned bits=o.bits;const uint32_t mask=(1u<<bits)-1,sign=1u<<(bits-1);
 for(size_t i=0;i<n;i+=8,src+=bits){            // reads group g before writing values 8g..8g+7: never overtakes (base>=n-pw)
  uint64_t v=0;for(unsigned k=0;k<bits;k++)v|=uint64_t(src[k])<<(8*k);
  for(unsigned k=0;k<8;k++){uint32_t u=uint32_t(v>>(bits*k))&mask;dst[i+k]=int8_t(int32_t(u^sign)-int32_t(sign));}
 }
 memmove(b+(o.scales-o.blobOffset),b+base+pw,4*size_t(o.co));
 memmove(b+(o.bias-o.blobOffset),b+base+pw+4*size_t(o.co),4*size_t(o.co));
}
// Scratch held only while one call runs: PSRAM on the device (internal RAM is short while dictating).
static void *scratchAlloc(size_t bytes){
#ifdef ESP_PLATFORM
 if(void *p=heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))return p;
#endif
 return malloc(bytes);
}
size_t workspaceBytes(const Model &model,size_t samples){return arenaPlan(model,samples,true).bytes;}
Error features(const Model &m,const int16_t *pcm,size_t n,float *out,size_t capacity,unsigned &validFrames,unsigned &paddedFrames,Hooks hooks){
 if(!m.valid()||!pcm||!out||n<400||n>size_t(SampleRate)*MaxSeconds)return InvalidAudio;
 validFrames=framesFor(n);paddedFrames=paddedFor(m,validFrames);const unsigned M=m.mels;
 if(paddedFrames>MaxFrames||capacity<size_t(paddedFrames)*M)return InvalidAudio;
 Complex tw[256],frame[512];float window[320];
 for(unsigned i=0;i<256;i++){float a=-2*Pi*i/512;tw[i]={std::cos(a),std::sin(a)};}
 for(unsigned i=0;i<320;i++)window[i]=.5f-.5f*std::cos(2*Pi*i/319.f);
 // Slaney mel filterbank (librosa norm="slaney"), sparse per band.
 struct Band {unsigned first,count;float w[40];};
 std::unique_ptr<Band,void(*)(void*)> bandStore(static_cast<Band*>(scratchAlloc(sizeof(Band)*M)),free);if(!bandStore)return NoMemory;Band *bands=bandStore.get();
 {float lo=slaneyHzToMel(0),hi=slaneyHzToMel(8000);float hz[MaxMels+2];
  for(unsigned b=0;b<M+2;b++)hz[b]=slaneyMelToHz(lo+(hi-lo)*b/float(M+1));
  for(unsigned b=0;b<M;b++){
   Band &band=bands[b];band.first=0;band.count=0;float enorm=2.f/(hz[b+2]-hz[b]);bool started=false;
   for(unsigned k=0;k<=256;k++){
    float f=k*(SampleRate/512.f),lower=(f-hz[b])/(hz[b+1]-hz[b]),upper=(hz[b+2]-f)/(hz[b+2]-hz[b+1]),w=std::max(0.f,std::min(lower,upper))*enorm;
    if(w>0){if(!started){band.first=k;started=true;}if(band.count>=40)return InvalidModel;band.w[band.count++]=w;}
    else if(started)break;
   }
  }
 }
 const bool preemph=m.flags&1;const int N=int(n);
 auto sample=[&](int j)->float{ // pre-emphasised signal with reflect padding
  if(j<0)j=-j;if(j>=N)j=2*N-2-j;if(j<0)j=0;
  float x=float(pcm[j])/32768.f;if(preemph&&j>0)x-=0.97f*float(pcm[j-1])/32768.f;return x;
 };
 for(unsigned t=0;t<validFrames;t++){
  int centre=int(t*160);
  for(unsigned i=0;i<512;i++)frame[i]={0,0};
  for(unsigned i=0;i<320;i++)frame[96+i]={sample(centre-160+int(i))*window[i],0};
  fft512(frame,tw);
  float *row=out+size_t(t)*M;
  for(unsigned b=0;b<M;b++){const Band &band=bands[b];float s=0;for(unsigned i=0;i<band.count;i++){const Complex &c=frame[band.first+i];s+=(c.re*c.re+c.im*c.im)*band.w[i];}row[b]=std::log(s+5.960464477539063e-08f);}
  if((t&3)==3&&!service(hooks,0))return Cancelled;
 }
 for(unsigned b=0;b<M;b++){
  double mean=0,var=0;for(unsigned t=0;t<validFrames;t++)mean+=out[size_t(t)*M+b];mean/=validFrames;
  for(unsigned t=0;t<validFrames;t++){double d=out[size_t(t)*M+b]-mean;var+=d*d;}
  float sd=float(std::sqrt(validFrames>1?var/(validFrames-1):0.0))+1e-5f;
  for(unsigned t=0;t<validFrames;t++)out[size_t(t)*M+b]=(out[size_t(t)*M+b]-float(mean))/sd;
 }
 for(size_t i=size_t(validFrames)*M;i<size_t(paddedFrames)*M;i++)out[i]=0.f;
 return Ok;
}
Error transcribe(const Model &m,const int16_t *pcm,size_t n,void *workspace,size_t capacity,char *text,size_t maxText,Stats &stats,Hooks hooks,float *logits,size_t logitCapacity){
 stats=Stats{};if(!text||maxText<2)return OutputTooSmall;text[0]=0;
 if(!m.valid()||!hooks.blob)return InvalidModel;auto plan=arenaPlan(m,n,hooks.quantScratch!=nullptr);size_t required=plan.bytes;if(!required)return InvalidAudio;
 if(!workspace||capacity<required||(hooks.quantScratch&&(reinterpret_cast<uintptr_t>(hooks.quantScratch)&15)))return NoMemory;
 stats.workspaceBytes=required;
 uintptr_t aligned=(reinterpret_cast<uintptr_t>(workspace)+15)&~uintptr_t(15);
 auto *base=reinterpret_cast<uint8_t*>(aligned);float *a=reinterpret_cast<float*>(base),*b=reinterpret_cast<float*>(base+plan.depthwise),*res=reinterpret_cast<float*>(base+plan.residual);
 int8_t *quant=hooks.quantScratch?hooks.quantScratch:reinterpret_cast<int8_t*>(base+plan.scratch);
 uint32_t started=hooks.millis?hooks.millis():0;
 unsigned valid=0,T=0;Error e=features(m,pcm,n,a,plan.depthwise/sizeof(float),valid,T,hooks);if(e!=Ok)return e;
 if(hooks.millis)stats.featureMs=hooks.millis()-started;
 unsigned outT=T;Run run{m,hooks,stats};
 for(unsigned index=0;index<m.ops;index++){
  if(!service(hooks,index*100/m.ops))return Cancelled;
  const Op &o=m.op[index];const uint8_t *blob=hooks.blob(hooks.context,index,o);if(!blob)return BlobUnavailable;
  bool ok;
  if(o.kind==Depthwise)ok=run.depthwise(index,o,blob,a,b,T,outT,quant);
  else if(o.flags&SaveResidual)ok=run.pointwise(index,o,blob,a,res,nullptr,T,quant);
  else if(o.flags&Classifier)ok=run.pointwise(index,o,blob,a,b,nullptr,T,quant);
  else{T=outT;ok=run.pointwise(index,o,blob,b,a,(o.flags&AddResidual)?res:nullptr,T,quant);}
  if(hooks.release)hooks.release(hooks.context,index);
  if(!ok)return Cancelled;
  if(hooks.trace)hooks.trace(hooks.context,index,o.kind==Depthwise||(o.flags&Classifier)?b:(o.flags&SaveResidual)?res:a,o.kind==Depthwise?outT:T,o.co);
  stats.opsDone=index+1;
 }
 stats.frames=T;const unsigned C=m.classes;
 if(logits){if(logitCapacity<size_t(T)*C)return OutputTooSmall;memcpy(logits,b,size_t(T)*C*sizeof(float));}
 size_t used=0;unsigned previous=C;
 for(unsigned t=0;t<T;t++){
  unsigned best=0;const float *row=b+size_t(t)*C;for(unsigned c=1;c<C;c++)if(row[c]>row[best])best=c;
  if(best!=m.blank&&best!=previous){const char *s=m.vocab[best];size_t len=strlen(s);if(!(len==1&&s[0]==' '&&(used==0||text[used-1]==' '))){if(used+len>=maxText){text[0]=0;return OutputTooSmall;}memcpy(text+used,s,len);used+=len;}}
  previous=best;
 }
 while(used&&text[used-1]==' ')used--;text[used]=0;
 if(!service(hooks,100)){text[0]=0;return Cancelled;}return Ok;
}
#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(MM_IT_SCALAR_ONLY)
__attribute__((optimize("no-branch-count-reg")))
#endif
bool kernelSelfTest(){
 alignas(16) int8_t a[1024],b[1024];
 uint32_t x=0x9e3779b9u;for(int i=0;i<1024;i++){x=x*1664525u+1013904223u;a[i]=int8_t(x>>24);x=x*1664525u+1013904223u;b[i]=int8_t(x>>24);}
 for(unsigned n:{16u,32u,48u,64u,80u,256u,512u,1008u,1024u}){
  int32_t expected=0;for(unsigned i=0;i<n;i++)expected+=int32_t(a[i])*int32_t(b[i]);
  if(dot(a,b,n)!=expected)return false;
 }
 return true;
}
}
