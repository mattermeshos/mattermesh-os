#pragma once
// Double-buffered, one-operator-ahead prefetch of MMQIT002 blobs for the engine's
// blob hook. Portable: the platform supplies the readers (SD task, host file, ...).
// Resident blobs (a flash-mapped prefix of the model) are served by pointer and never
// copied. Buffer (i+1)&1 was released by the engine before blob(i) is called, so the
// prefetch of op i+1 can start while op i is computed.
#include "asr2.h"
namespace mm_asr {
struct BlobStream {
 struct Platform {
  void *context=nullptr;
  // Pointer to a resident copy of this op's blob, or nullptr when it must be read.
  const uint8_t *(*resident)(void*,const Op&)=nullptr;
  // Begin loading the op into dst (may complete synchronously): read op.storedBytes bytes at
  // op.blobOffset to dst+(op.blobBytes-op.storedBytes), verify the CRC, then unpackBlob(op,dst).
  bool (*start)(void*,unsigned index,const Op&,uint8_t *dst)=nullptr;
  // Block until the read started for `index` finished; false on failure (incl. CRC).
  bool (*wait)(void*,unsigned index)=nullptr;
 };
 const Model *model=nullptr;Platform platform;uint8_t *buffer[2]={nullptr,nullptr};size_t bufferBytes=0;
 int pending=-1;unsigned reads=0,prefetchHits=0,residentHits=0;bool failed=false;
 void begin(const Model &m,Platform p,uint8_t *a,uint8_t *b,size_t bytes){model=&m;platform=p;buffer[0]=a;buffer[1]=b;bufferBytes=bytes;pending=-1;reads=prefetchHits=residentHits=0;failed=false;}
 bool fits(const Op &o)const{return o.blobBytes<=bufferBytes&&buffer[0]&&buffer[1];}
 const uint8_t *resident(const Op &o)const{return !packed(o)&&platform.resident?platform.resident(platform.context,o):nullptr;}  // packed ops always go through a buffer
 const uint8_t *blob(unsigned index,const Op &op){
  if(!model||index>=model->ops||failed||!platform.start||!platform.wait)return fail();
  const uint8_t *out=resident(op);
  if(out)residentHits++;
  else{
   if(!fits(op))return fail();
   uint8_t *dst=buffer[index&1];
   if(pending==int(index))prefetchHits++;
   else{
    if(pending>=0){bool ok=platform.wait(platform.context,unsigned(pending));pending=-1;if(!ok)return fail();}
    if(!platform.start(platform.context,index,op,dst))return fail();reads++;
   }
   pending=-1;
   if(!platform.wait(platform.context,index))return fail();
   out=dst;
  }
  unsigned next=index+1;
  if(next<model->ops){const Op &n=model->op[next];if(!resident(n)&&fits(n)&&platform.start(platform.context,next,n,buffer[next&1])){pending=int(next);reads++;}}
  return out;
 }
 // Call after transcribe() returns (also on cancel/error) before the buffers are freed.
 void finish(){if(pending>=0&&platform.wait)platform.wait(platform.context,unsigned(pending));pending=-1;}
 private:
 const uint8_t *fail(){failed=true;return nullptr;}
};
}
