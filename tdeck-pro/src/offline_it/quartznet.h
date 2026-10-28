#pragma once
#include <stddef.h>
#include <stdint.h>
namespace mm_it {
// Portable utterance engine for the published Italian QuartzNet 5x5 checkpoint.
// Q8 weights are read-only: on ESP32 they must be mapped from a model partition.
constexpr unsigned SampleRate=16000, MaxSeconds=10, MelBins=80, Classes=36;
constexpr unsigned HeaderBytes=64, OpBytes=64, MaxOps=64;
constexpr size_t ScratchBytes=32*1024;
enum Error { Ok=0, InvalidModel=1, InvalidAudio=2, NoMemory=3, Cancelled=4, OutputTooSmall=5 };
struct Model {const uint8_t *data=nullptr;size_t bytes=0;unsigned ops=0,dtype=0;};
// fork/join (optional): run task(arg) on a second core while the caller works,
// then wait for it. helperScratch: ScratchBytes, 16-aligned, for the helper's
// depthwise strip. Results are bit-identical with or without a helper.
struct Hooks {void *context=nullptr;bool (*service)(void*,unsigned)=nullptr;void (*trace)(void*,unsigned,const float*,unsigned,unsigned)=nullptr;int8_t *quantScratch=nullptr;
 bool (*fork)(void*,void(*)(void*),void*)=nullptr;void (*join)(void*)=nullptr;int8_t *helperScratch=nullptr;};
struct Stats {size_t workspaceBytes=0;uint64_t macs=0;unsigned frames=0,opsDone=0;};
bool kernelSelfTest();
bool open(Model &model,const void *data,size_t bytes,bool checkCrc=true,Hooks hooks={});
size_t workspaceBytes(size_t samples);
// Model-aware arena: residual storage is reused only after its final consumer.
// The overload above remains a conservative size for callers without a model.
// With externalScratch, Hooks.quantScratch must provide ScratchBytes aligned to 16.
// No duplicate scratch is reserved inside the arena in this mode.
size_t workspaceBytes(const Model &,size_t samples,bool externalScratch=false);
// Workspace is caller-owned, sized by the matching workspaceBytes overload. Callbacks
// are bounded computational slices, never main-thread radio/UI callbacks.
Error transcribe(const Model &,const int16_t *,size_t,void *,size_t,char *,size_t,
                 Stats &,Hooks hooks={},float *logits=nullptr,size_t logitCapacity=0);
// Exposed for independent preprocessing comparisons with the upstream pipeline.
Error features(const int16_t *,size_t,float *,size_t,Hooks hooks={});
uint32_t crc32(const uint8_t *,size_t);
}
