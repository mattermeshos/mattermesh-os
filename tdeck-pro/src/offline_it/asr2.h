#pragma once
// MatterMesh offline ASR engine v2: MMQIT002 models (QuartzNet-style separable
// 1-D CNN + CTC) whose per-operator weight blobs are supplied by the caller one
// at a time, so a 19 MB model can be streamed from SD through a small PSRAM
// staging buffer. NeMo front end (64 mel / 20 ms / ln / per-feature norm).
// Portable C++11, no heap: the caller provides the arena and the scratch areas.
#include <cstring>
#include <stddef.h>
#include <stdint.h>
namespace mm_asr {
constexpr unsigned HeaderBytes=128,OpBytes=64,MaxOps=256,MaxClasses=64,MaxKernel=127,MaxMels=80,MaxFrames=1008,SampleRate=16000,MaxSeconds=10;
constexpr size_t ScratchBytes=32*1024;
enum Error {Ok=0,InvalidModel=1,InvalidAudio=2,NoMemory=3,Cancelled=4,OutputTooSmall=5,BlobUnavailable=6};
enum Kind {Norm=1,Depthwise=2,Pointwise=3};
enum Flag {Relu=1,SaveResidual=2,AddResidual=4,Classifier=8};
// crc: CRC-32 of the stored blob (storedBytes at blobOffset). bits<8: the pointwise weights are stored
// packed (8 values in `bits` bytes, little-endian bit stream, two's complement) followed by the f32
// scales and bias; blobBytes/weights/scales/bias describe the unpacked int8 image a loader builds
// with unpackBlob(). bits==8: storedBytes==blobBytes and the stored blob is the image itself.
struct Op {uint32_t kind,ci,co,kernel,stride,flags,row,blobOffset,blobBytes,weights,scales,bias,dilation,crc,storedBytes,bits;};
inline bool packed(const Op &o){return o.bits<8;}
struct Model {
 uint32_t version=0,bytes=0,ops=0,dtype=0,mels=0,classes=0,sampleRate=0,frontend=0,window=0,nfft=0,hop=0,padTo=0,blank=0,vocabOffset=0,vocabBytes=0,maxBlob=0,flags=0,maxChannels=0,tableCrc=0,blobStart=0;
 Op op[MaxOps];
 char vocab[MaxClasses][5];
 bool valid() const {return version==2&&ops>0;}
};
// Same as model=Model() (all fields zero) without a ~17 KB temporary on the caller's stack.
inline void resetModel(Model &model){memset(&model,0,sizeof(model));}
// Random-access reader over the model file (flash mapping, SD file, host file).
struct Source {void *context=nullptr;bool (*read)(void*,uint32_t offset,void *dst,uint32_t bytes)=nullptr;};
struct Stats {size_t workspaceBytes=0;uint64_t macs=0;unsigned frames=0,opsDone=0;uint32_t featureMs=0;};
// blob(): returns the operator's blob (weights/scales/bias) in RAM, blocking
// until it is available; nullptr means failure. release() lets the staging
// buffer be reused. fork()/join(): optional second core, same contract as v1.
struct Hooks {
 void *context=nullptr;
 bool (*service)(void*,unsigned percent)=nullptr;
 const uint8_t *(*blob)(void*,unsigned index,const Op &op)=nullptr;
 void (*release)(void*,unsigned index)=nullptr;
 void (*trace)(void*,unsigned index,const float*,unsigned frames,unsigned channels)=nullptr;
 bool (*fork)(void*,void (*job)(void*),void *arg)=nullptr;
 void (*join)(void*)=nullptr;
 int8_t *quantScratch=nullptr;   // ScratchBytes, 16-aligned, internal RAM
 int8_t *helperScratch=nullptr;  // ScratchBytes for the second core (optional)
 uint32_t (*millis)()=nullptr;
};
// CRC-32 (zlib polynomial); `prev` chains partial results like zlib.crc32(data, prev).
uint32_t crc32(const uint8_t *data,size_t bytes,uint32_t prev=0);
// Expand a packed blob in place. The loader put the op's storedBytes at buffer+(blobBytes-storedBytes)
// (and verified the CRC); afterwards buffer holds the int8 image (weights, scales, bias) the Op describes.
// No-op for unpacked ops. The in-place expansion is safe because open() checks blobBytes-storedBytes.
void unpackBlob(const Op &o,uint8_t *buffer);
// Parse header + operator table + vocabulary. Verifies magic, sizes, the header CRC
// and the table CRC (operator records + vocabulary). Blob CRCs are left to the caller.
Error open(Model &model,Source source);
// Arena bytes for `samples` PCM samples (0 when unsupported).
size_t workspaceBytes(const Model &model,size_t samples);
// NeMo features: out holds frames*mels floats (frames = padded count). Returns frame counts.
Error features(const Model &model,const int16_t *pcm,size_t samples,float *out,size_t capacity,unsigned &validFrames,unsigned &paddedFrames,Hooks hooks);
// Full pipeline: features, operators, greedy CTC. logits (optional) receives frames*classes raw outputs.
Error transcribe(const Model &model,const int16_t *pcm,size_t samples,void *workspace,size_t capacity,char *text,size_t maxText,Stats &stats,Hooks hooks,float *logits=nullptr,size_t logitCapacity=0);
bool kernelSelfTest();
}
