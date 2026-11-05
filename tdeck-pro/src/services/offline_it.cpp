// Offline Italian dictation adapter, engine v2 (MMQIT002, QuartzNet-15x5 class models).
// The model lives on the SD card and is streamed one operator at a time through two
// PSRAM staging buffers (one-ahead prefetch on a dedicated task). An optional copy of
// the file's first bytes in the `asr_it` flash partition serves those operators by
// pointer, verified once per boot with the per-blob CRCs. Second core via the helper task.
#include "offline_it.h"
#include "dictation_policy.h"
#include "psram.h"
#include <algorithm>
#if defined(MM_OFFLINE_ITALIAN) && MM_OFFLINE_ITALIAN
#include "offline_it/asr2.h"
#include "offline_it/blob_stream.h"
#include <SD.h>
#include <esp_partition.h>
#include <esp_spi_flash.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
static_assert(device::DICTATION_RATE==mm_asr::SampleRate && device::DICTATION_MAX_SECONDS<=mm_asr::MaxSeconds,"Audio capture exceeds Italian engine bounds");
#endif
namespace device {
namespace {
const char *MODEL_PATH="/mattermesh/asr-it.mmq";
std::atomic<uint32_t> modelBytes{0},workspaceBytes{0},elapsedMs{0},progress{0};std::atomic<uint64_t> macs{0};std::atomic<int> lastError{0};std::atomic<bool> modelReady{false},busy{false};
std::atomic<uint32_t> stage{0},opIndex{0},stackMin{0},internalMin{0},psramMin{0},loadMs{0},featuresMs{0},lastOpMs{0},maxOpMs{0},depthwiseMs{0},pointwiseMs{0},cores{0};
std::atomic<uint32_t> modelOps{0},residentOps{0},prefixBytes{0},sdBytes{0},sdReadMs{0},sdWaitMs{0},sdReads{0},prefetchHits{0};
}
void offlineItalianReset(){modelBytes=workspaceBytes=elapsedMs=progress=0;macs=0;lastError=0;modelReady=false;stage=opIndex=stackMin=internalMin=psramMin=loadMs=featuresMs=lastOpMs=maxOpMs=depthwiseMs=pointwiseMs=cores=0;modelOps=residentOps=prefixBytes=sdBytes=sdReadMs=sdWaitMs=sdReads=prefetchHits=0;}
OfflineItalianMetrics offlineItalianMetrics(){OfflineItalianMetrics m;m.compiled=OFFLINE_ITALIAN_COMPILED;m.modelReady=modelReady;m.modelBytes=modelBytes;m.workspaceBytes=workspaceBytes;m.elapsedMs=elapsedMs;m.progress=progress;m.macs=macs;m.error=lastError;m.stage=stage;m.op=opIndex;m.stackMin=stackMin;m.internalMin=internalMin;m.psramMin=psramMin;m.loadMs=loadMs;m.featuresMs=featuresMs;m.lastOpMs=lastOpMs;m.maxOpMs=maxOpMs;m.depthwiseMs=depthwiseMs;m.pointwiseMs=pointwiseMs;m.cores=cores;m.modelOps=modelOps;m.residentOps=residentOps;m.prefixBytes=prefixBytes;m.sdBytes=sdBytes;m.sdReadMs=sdReadMs;m.sdWaitMs=sdWaitMs;m.sdReads=sdReads;m.prefetchHits=prefetchHits;return m;}
#if defined(MM_OFFLINE_ITALIAN) && MM_OFFLINE_ITALIAN
namespace {
// ---- cached model (parsed once per boot, or until reload) ----------------------------
// The model table (~17 KB) lives in PSRAM, allocated on first use: internal RAM stays free for TLS and BLE.
static mm_asr::Model &asrModel(){static mm_asr::Model *m=nullptr;if(!m){m=static_cast<mm_asr::Model*>(psramCalloc(1,sizeof(mm_asr::Model)));}return *m;}static bool cachedValid=false;static uint8_t cachedHeader[mm_asr::HeaderBytes];
static const uint8_t *prefix=nullptr;static spi_flash_mmap_handle_t prefixHandle;static uint32_t prefixMapped=0;static uint32_t residentMask[mm_asr::MaxOps/32];
struct FileSource {File *file;};
bool readFile(void *c,uint32_t offset,void *dst,uint32_t bytes){
 File &f=*static_cast<FileSource*>(c)->file;if(!f||!f.seek(offset))return false;
 uint8_t *p=static_cast<uint8_t*>(dst);uint32_t got=0;while(got<bytes){int n=f.read(p+got,bytes-got);if(n<=0)return false;got+=n;}return true;
}
void dropPrefix(){if(prefix){spi_flash_munmap(prefixHandle);prefix=nullptr;}prefixMapped=0;memset(residentMask,0,sizeof(residentMask));}
bool residentOp(unsigned index){return prefix&&index<asrModel().ops&&((residentMask[index>>5]>>(index&31))&1u);}
// A flash copy of the file's first bytes is used only when its header is byte-identical
// to the SD file's and every candidate blob passes its CRC (checked once per boot).
void mapPrefix(const mm_asr::Hooks &hooks){
 dropPrefix();
 const esp_partition_t *part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"asr_it");
 if(!part||part->size<1024*1024)return;
 uint8_t header[mm_asr::HeaderBytes];
 if(esp_partition_read(part,0,header,sizeof(header))!=ESP_OK||memcmp(header,cachedHeader,sizeof(header)))return;
 uint32_t bytes=std::min<uint32_t>(part->size,asrModel().bytes);const void *mapped=nullptr;
 if(esp_partition_mmap(part,0,bytes,SPI_FLASH_MMAP_DATA,&mapped,&prefixHandle)!=ESP_OK)return;
 prefix=static_cast<const uint8_t*>(mapped);prefixMapped=bytes;unsigned count=0;
 for(unsigned i=0;i<asrModel().ops;i++){
  const mm_asr::Op &o=asrModel().op[i];
  if(uint64_t(o.blobOffset)+o.storedBytes>bytes)break;
  if(mm_asr::crc32(prefix+o.blobOffset,o.storedBytes)!=o.crc)break;      // stop at the first damaged blob; later ones come from SD
  residentMask[i>>5]|=1u<<(i&31);count++;
  if(hooks.service&&!hooks.service(hooks.context,0))break;
 }
 residentOps=count;prefixBytes=count?bytes:0;
 if(!count)dropPrefix();
}
// ---- SD reader task (core 1): one blob per request, CRC verified -----------------------
struct SdJob {unsigned index=0;const mm_asr::Op *op=nullptr;uint8_t *dst=nullptr;bool ok=false;};
static std::atomic<bool> sdReadingCard{false};   // true while the reader task polls the SD SPI bus (core 1)
static SdJob sdJob;static File sdFile;static SemaphoreHandle_t sdStart=nullptr,sdDone=nullptr;static TaskHandle_t sdTask=nullptr;
void sdLoop(void*){
 for(;;){
  xSemaphoreTake(sdStart,portMAX_DELAY);uint32_t t=millis();const mm_asr::Op &o=*sdJob.op;
  const uint32_t stored=o.storedBytes;uint8_t *tail=sdJob.dst+(o.blobBytes-stored);bool ok;uint32_t got=0;
  bool fromFlash=residentOp(sdJob.index);
  if(fromFlash){memcpy(tail,prefix+o.blobOffset,stored);ok=true;}      // packed resident op: copy from flash (CRC verified once by mapPrefix), unpack below
  else{ok=sdFile&&sdFile.seek(o.blobOffset);while(ok&&got<stored){int n=sdFile.read(tail+got,std::min<uint32_t>(stored-got,8192));if(n<=0)ok=false;else got+=n;}sdBytes+=got;sdReadingCard=false;}
  sdJob.ok=ok&&(fromFlash||mm_asr::crc32(tail,stored)==o.crc);if(sdJob.ok)mm_asr::unpackBlob(o,sdJob.dst);sdReadMs+=millis()-t;sdReads++;
  xSemaphoreGive(sdDone);
 }
}
bool sdReady(){
 if(sdTask)return true;
 if(!sdStart)sdStart=xSemaphoreCreateBinary();if(!sdDone)sdDone=xSemaphoreCreateBinary();
 return sdStart&&sdDone&&xTaskCreatePinnedToCore(sdLoop,"mmsd",4096,nullptr,1,&sdTask,1)==pdPASS;
}
const uint8_t *platformResident(void*,const mm_asr::Op &o){unsigned index=unsigned(&o-asrModel().op);return residentOp(index)&&!mm_asr::packed(o)?prefix+o.blobOffset:nullptr;}
bool platformStart(void*,unsigned index,const mm_asr::Op &o,uint8_t *dst){sdJob.index=index;sdJob.op=&o;sdJob.dst=dst;sdJob.ok=false;if(!residentOp(index))sdReadingCard=true;xSemaphoreGive(sdStart);return true;}  // flag raised here (worker context) so the fork that follows sees it
bool platformWait(void*,unsigned index){uint32_t t=millis();xSemaphoreTake(sdDone,portMAX_DELAY);sdWaitMs+=millis()-t;return sdJob.ok&&sdJob.index==index;}
// ---- second core helper (same contract as before) --------------------------------------
static TaskHandle_t helperTask=nullptr;static SemaphoreHandle_t helperStart=nullptr,helperDone=nullptr;
static void (*helperJob)(void*)=nullptr;static void *helperArg=nullptr;
static std::atomic<uint32_t> helperJobs{0},helperStackMin{0},helperBusyUs{0},joinWaitUs{0};
void helperLoop(void*){for(;;){xSemaphoreTake(helperStart,portMAX_DELAY);uint32_t t=micros();helperJob(helperArg);helperBusyUs+=micros()-t;helperJobs++;helperStackMin=uxTaskGetStackHighWaterMark(nullptr);xSemaphoreGive(helperDone);}}
bool helperReady(){
 if(helperTask)return true;
 if(!helperStart)helperStart=xSemaphoreCreateBinary();if(!helperDone)helperDone=xSemaphoreCreateBinary();
 // Priority 2: above loopTask and the SD reader, so the helper's half of an operator is not time-sliced
 // with the reader's copy/CRC/unpack of the next blob (which then runs while the helper waits for the next fork).
 return helperStart&&helperDone&&xTaskCreatePinnedToCore(helperLoop,"mmit1",12288,nullptr,2,&helperTask,1)==pdPASS;
}
// No helper while the reader is busy on the SD card: those operators are I/O-bound anyway and the
// helper (priority 2, core 1) would only steal the reader's SPI polling time; measured on device.
bool forkHelper(void*,void (*job)(void*),void *arg){if(sdReadingCard.load())return false;helperJob=job;helperArg=arg;xSemaphoreGive(helperStart);return true;}
void joinHelper(void*){uint32_t t=micros();xSemaphoreTake(helperDone,portMAX_DELAY);joinWaitUs+=micros()-t;}
static int forcedCores=0;static bool serialTrace=false;
struct Context {std::atomic<bool> &cancel;uint32_t lastYield,start,lastOp;mm_asr::BlobStream stream;};
bool service(void *opaque,unsigned percent){auto &ctx=*static_cast<Context*>(opaque);progress=percent;uint32_t now=millis();elapsedMs=now-ctx.start;if(ctx.cancel.load())return false;if(uint32_t(now-ctx.lastYield)>=50){ctx.lastYield=now;vTaskDelay(1);}return !ctx.cancel.load();}
const uint8_t *blob(void *opaque,unsigned index,const mm_asr::Op &op){auto &ctx=*static_cast<Context*>(opaque);const uint8_t *p=ctx.stream.blob(index,op);prefetchHits=ctx.stream.prefetchHits;return p;}
void trace(void *opaque,unsigned index,const float*,unsigned,unsigned){
 auto &ctx=*static_cast<Context*>(opaque);uint32_t now=millis();lastOpMs=now-ctx.lastOp;ctx.lastOp=now;
 if(index==0)featuresMs=lastOpMs.load();else{maxOpMs=std::max(maxOpMs.load(),lastOpMs.load());(asrModel().op[index].kind==mm_asr::Depthwise?depthwiseMs:pointwiseMs)+=lastOpMs.load();}
 stage=3;opIndex=index+1;stackMin=uxTaskGetStackHighWaterMark(nullptr);
 if(serialTrace)Serial.printf("OFFLINE_IT_OP %u %lu ms core%d helper=%lu hstack=%lu helper_ms=%lu join_wait_ms=%lu\n",index,(unsigned long)lastOpMs.load(),xPortGetCoreID(),(unsigned long)helperJobs.load(),(unsigned long)helperStackMin.load(),(unsigned long)(helperBusyUs.load()/1000),(unsigned long)(joinWaitUs.load()/1000));
 internalMin=std::min(internalMin.load(),uint32_t(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));
 psramMin=std::min(psramMin.load(),uint32_t(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)));
}
void wipeFree(void *p,size_t bytes){if(p){memset(p,0,bytes);free(p);}}
}
bool offlineItalianReload(){if(busy.load())return false;cachedValid=false;dropPrefix();modelReady=false;return true;}
void offlineItalianDebug(int c,int t){if(c>=0)forcedCores=c>2?2:c;if(t>=0)serialTrace=t!=0;}
int offlineItalianTranscribe(const int16_t *pcm,size_t samples,std::atomic<bool> &cancel,String &text){
 text="";offlineItalianReset();uint32_t start=millis();busy=true;
 auto finish=[&](int error){elapsedMs=millis()-start;lastError=error;stage=4;busy=false;return error;};
 stage=1;
 if(cancel.load())return finish(24);
 if(samples<400||samples>size_t(mm_asr::SampleRate)*mm_asr::MaxSeconds)return finish(15);
 File file=SD.open(MODEL_PATH,FILE_READ);
 if(!file)return finish(10);
 Context ctx{cancel,millis(),start,millis(),{}};mm_asr::Hooks hooks;hooks.context=&ctx;hooks.service=service;hooks.trace=trace;hooks.millis=[](){return uint32_t(millis());};
 if(!cachedValid){
  if(!mm_asr::kernelSelfTest()){file.close();return finish(17);}
  FileSource fs{&file};mm_asr::Source source;source.context=&fs;source.read=readFile;
  bool ok=readFile(&fs,0,cachedHeader,sizeof(cachedHeader))&&mm_asr::open(asrModel(),source)==mm_asr::Ok&&asrModel().bytes==file.size()&&asrModel().dtype==1;
  if(!ok){mm_asr::resetModel(asrModel());file.close();return finish(11);}
  mapPrefix(hooks);cachedValid=true;
 }
 modelReady=true;modelBytes=asrModel().bytes;modelOps=asrModel().ops;loadMs=millis()-start;
 size_t required=mm_asr::workspaceBytes(asrModel(),samples);if(!required){file.close();return finish(15);}
 if(cancel.load()){file.close();return finish(24);}
 size_t staging=(size_t(asrModel().maxBlob)+15)&~size_t(15);
 // PSRAM: arena + two staging buffers, keeping the measured runtime reserve for MeshCore and the UI.
 if(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<required+2*staging+512*1024||heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<required){file.close();return finish(14);}
 void *arena=heap_caps_malloc(required,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 uint8_t *bufA=static_cast<uint8_t*>(heap_caps_aligned_alloc(16,staging,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)),*bufB=static_cast<uint8_t*>(heap_caps_aligned_alloc(16,staging,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 int8_t *scratch=static_cast<int8_t*>(heap_caps_aligned_alloc(16,mm_asr::ScratchBytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
 if(!arena||!bufA||!bufB||!scratch){free(arena);free(bufA);free(bufB);free(scratch);file.close();return finish(14);}
 workspaceBytes=required;hooks.quantScratch=scratch;
 if(!sdReady()){wipeFree(arena,required);free(bufA);free(bufB);free(scratch);file.close();return finish(18);}
 int8_t *helperScratch=nullptr;
 if(forcedCores!=1&&helperReady()){hooks.fork=forkHelper;hooks.join=joinHelper;helperScratch=static_cast<int8_t*>(heap_caps_aligned_alloc(16,mm_asr::ScratchBytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));hooks.helperScratch=helperScratch;}
 cores=hooks.fork&&helperScratch?2:1;helperBusyUs=0;joinWaitUs=0;
 internalMin=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);psramMin=heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 mm_asr::BlobStream::Platform platform;platform.context=nullptr;platform.resident=platformResident;platform.start=platformStart;platform.wait=platformWait;
 sdFile=file;ctx.stream.begin(asrModel(),platform,bufA,bufB,staging);hooks.blob=blob;
 stage=2;ctx.lastOp=millis();char buffer[512]={};mm_asr::Stats stats;
 int code=mm_asr::transcribe(asrModel(),pcm,samples,arena,required,buffer,sizeof(buffer),stats,hooks);
 ctx.stream.finish();sdFile=File();file.close();macs=stats.macs;sdReads=ctx.stream.reads;prefetchHits=ctx.stream.prefetchHits;
 if(code==mm_asr::Ok&&!cancel.load()){text=buffer;if(text.length()!=strlen(buffer))code=mm_asr::NoMemory;else text.trim();}
 memset(buffer,0,sizeof(buffer));wipeFree(arena,required);wipeFree(bufA,staging);wipeFree(bufB,staging);wipeFree(scratch,mm_asr::ScratchBytes);wipeFree(helperScratch,helperScratch?mm_asr::ScratchBytes:0);
 if(cancel.load())code=mm_asr::Cancelled;return finish(code?20+code:0);
}
#else
bool offlineItalianReload(){return false;}
void offlineItalianDebug(int,int){}
int offlineItalianTranscribe(const int16_t*,size_t,std::atomic<bool>&,String &text){text="";lastError=16;return 16;}
#endif
String offlineItalianMessage(){switch(lastError.load()){
 case 0:return "Offline dictation ready / review then Enter";
 case 10:return "Italian model not on SD card / draft kept";
 case 11:return "Italian model file invalid / draft kept";
 case 14:case 23:return "Not enough ASR memory / draft kept";
 case 15:case 22:return "Invalid recording / draft kept";
 case 17:return "ASR arithmetic check failed / draft kept";
 case 16:return "Offline IT not compiled / draft kept";
 case 18:return "ASR SD reader unavailable / draft kept";
 case 24:return "Dictation canceled / draft kept";
 case 25:return "Transcript too long / draft kept";
 case 26:return "Model read from SD failed / draft kept";
 default:return "Offline dictation failed / draft kept";
}}
}
