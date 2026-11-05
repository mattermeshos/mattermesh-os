#include "map_tiles.h"
#include "map_math.h"
#include "ui/theme.h"
#include "connectivity.h"
#include "https.h"
#include "ui/build_info.h"
#include "board/device_model.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <cstring>
#include <utility>
#include <new>
namespace device {
// T-Deck Plus: OpenStreetMap PNG files on SD (/maps/osm), decoded on core 0 to RGB565 (128 KB each); 12 kept in PSRAM.
constexpr size_t TileBytes=MapTileColorBytes;constexpr int RamTiles=12;
struct TileKey {uint8_t z=0;uint32_t x=0,y=0,generation=0;bool operator==(const TileKey &o)const{return z==o.z&&x==o.x&&y==o.y;}};
static void tilePath(char *out,size_t size,const TileKey &k){mapTilePathOsm(out,size,k.z,k.x,k.y);}
static const char *tileRoot(const TileKey &){return "/maps/osm";}
static constexpr const char *TileHost="tile.openstreetmap.org";
struct TileResult {TileKey key;uint8_t *bits=nullptr;uint8_t *png=nullptr;uint32_t pngLen=0;bool fromSd=false;};
struct DecodeJob {TileKey key;uint8_t *data=nullptr;uint32_t len=0;};
struct Slot {TileKey key;uint8_t *bits=nullptr;uint32_t used=0;bool valid=false;};
constexpr int QueueDepth=24,MissingMemo=32;constexpr size_t PngCapacity=160*1024;constexpr uint32_t RequestGapMs=250,RetryMs=60000;
static fs::FS *storage=nullptr;static bool sdOk=false;static Slot slots[RamTiles];static uint32_t useClock=0;
static TileKey pending[QueueDepth];static int pendingCount=0;static TileKey missing[MissingMemo];static int missingNext=0;
static struct {TileKey key;uint32_t at=0;} failedRecent[8];static int failedNext=0;
static std::atomic<uint32_t> decodeMs{0};// written by the core-0 decoder
static char madeDir[40]="";// last tile column directory created on SD
static int sdLoadsThisPass=0;static bool sdDeferred=false;constexpr int SdLoadsPerPass=1;
static QueueHandle_t requests=nullptr,results=nullptr,decodes=nullptr;
// The download and SD-decode tasks (12 + 8 KB of internal stack, ~370 KB of PSRAM buffers and PNG decoders) start with
// the first tile they are given and end after a minute without work; the next tile starts them again.
static std::atomic<bool> workerRunning{false},decoderRunning{false};constexpr uint32_t TaskIdleMs=60000;static std::atomic<uint32_t> generation{1},revision{1};static MapTilesState state;

// ---- worker (core 0): HTTPS fetch, PNG decode, 1 bpp conversion ----------
struct Decode {PNG *png;uint8_t *luma;bool ok;};
static int drawLine(PNGDRAW *draw){auto *d=static_cast<Decode*>(draw->pUser);if(draw->iWidth!=MapTileSize||draw->y>=MapTileSize){d->ok=false;return 0;}
 // Big-endian RGB565 in memory = the LCD panel byte order used by the UI frame: the tile is drawn with a row copy.
 d->png->getLineAsRGB565(draw,reinterpret_cast<uint16_t*>(d->luma+size_t(draw->y)*MapTileSize*2),PNG_RGB565_BIG_ENDIAN,0xffffffff);return 1;
}
static bool decodeTile(PNG *decoder,uint8_t *png,size_t length,uint8_t *out){
 if(!decoder||!out||decoder->openRAM(png,length,drawLine)!=PNG_SUCCESS)return false;Decode d{decoder,out,true};
 bool ok=decoder->getWidth()==MapTileSize&&decoder->getHeight()==MapTileSize&&decoder->decode(&d,0)==PNG_SUCCESS&&d.ok;decoder->close();return ok;}
static bool fetch(WiFiClientSecure &client,HTTPClient &http,const TileKey &k,uint8_t *png,size_t &length){
 char path[48];snprintf(path,sizeof(path),"/%d/%lu/%lu.png",k.z,(unsigned long)k.x,(unsigned long)k.y);length=0;
 if(!http.begin(client,TileHost,443,path,true))return false;
 http.setUserAgent("MatterMeshOS/" MM_VERSION " (LilyGO " MM_DEVICE_MODEL "; on-device map cache)");bool ok=false;
 if(http.GET()==200){int size=http.getSize();WiFiClient *stream=http.getStreamPtr();
  if(size>0&&size_t(size)<=PngCapacity&&stream){uint32_t deadline=millis()+15000;while(length<size_t(size)&&int32_t(millis()-deadline)<0){int n=stream->readBytes(png+length,size-length);if(n<=0){delay(5);continue;}length+=n;}ok=length==size_t(size);}}
 http.end();return ok;
}
static void workLoop(){
 WiFiClientSecure client;HTTPClient http;secureClient(client,http);http.setReuse(true);
 auto *png=static_cast<uint8_t*>(heap_caps_malloc(PngCapacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 void *decoderMemory=heap_caps_malloc(sizeof(PNG),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);PNG *decoder=decoderMemory?new(decoderMemory) PNG():nullptr;
 uint32_t idleMs=0;
 for(;;){TileKey k;
  // The kept-alive TLS session holds ~60 KB of internal RAM: dropped after 20 s idle, the whole task after a minute.
  if(xQueueReceive(requests,&k,pdMS_TO_TICKS(20000))!=pdTRUE){if(client.connected())client.stop();idleMs+=20000;
   if(idleMs<TaskIdleMs)continue;
   workerRunning=false;// a request queued meanwhile is picked up here, or by the task its sender starts
   if(uxQueueMessagesWaiting(requests)&&!workerRunning.exchange(true)){idleMs=0;continue;}
   break;}
  idleMs=0;TileResult r;r.key=k;
  if(png&&decoder&&k.generation==generation.load()&&wifiConnected()){networkJobBegin();size_t length=0;// 240 MHz + WDT pause only while fetching
   if(fetch(client,http,k,png,length)){
    r.bits=static_cast<uint8_t*>(heap_caps_malloc(TileBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));// decoded in place: no 128 KB copy
    if(r.bits&&!decodeTile(decoder,png,length,r.bits)){heap_caps_free(r.bits);r.bits=nullptr;}
    if(r.bits){r.png=static_cast<uint8_t*>(heap_caps_malloc(length,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(r.png){memcpy(r.png,png,length);r.pngLen=length;}}// the Plus caches the PNG itself
   }
   networkJobEnd();vTaskDelay(pdMS_TO_TICKS(RequestGapMs));}
  else r.key.generation=0; // stale or offline: report back without a tile
  xQueueSend(results,&r,portMAX_DELAY);// dedicated task: wait for the main loop (a dropped result would stay pending forever)
 }
 if(decoder)decoder->~PNG();heap_caps_free(decoderMemory);heap_caps_free(png);
}
// Ends after the loop so the TLS client and HTTP objects are destroyed first.
static void worker(void*){workLoop();vTaskDelete(nullptr);}

// Core 0: PNG bytes read from SD by the main loop -> RGB565 tile; never touches the SPI bus.
static void decoderTask(void*){
 void *mem=heap_caps_malloc(sizeof(PNG),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);PNG *decoder=mem?new(mem) PNG():nullptr;
 uint32_t idleMs=0;
 for(;;){DecodeJob j;
  if(xQueueReceive(decodes,&j,pdMS_TO_TICKS(20000))!=pdTRUE){idleMs+=20000;if(idleMs<TaskIdleMs)continue;
   decoderRunning=false;if(uxQueueMessagesWaiting(decodes)&&!decoderRunning.exchange(true)){idleMs=0;continue;}
   break;}
  idleMs=0;TileResult r;r.key=j.key;r.fromSd=true;uint32_t t0=millis();
  auto *out=static_cast<uint8_t*>(heap_caps_malloc(TileBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(out&&decodeTile(decoder,j.data,j.len,out))r.bits=out;else if(out)heap_caps_free(out);
  heap_caps_free(j.data);decodeMs.store(millis()-t0);
  xQueueSend(results,&r,portMAX_DELAY);}
 if(decoder)decoder->~PNG();heap_caps_free(mem);vTaskDelete(nullptr);
}
// ---- main loop: RAM slots, SD cache, request bookkeeping -----------------
void mapTilesBegin(fs::FS &fs,bool sdReady){
 storage=&fs;sdOk=sdReady;state.sd=sdReady;if(requests)return;
 requests=xQueueCreate(QueueDepth,sizeof(TileKey));results=xQueueCreate(4,sizeof(TileResult));
 decodes=xQueueCreate(6,sizeof(DecodeJob));
}
// A task that cannot start (internal RAM short) leaves its flag clear: the next tile tries again.
static void startWorker(){if(!workerRunning.exchange(true)&&xTaskCreatePinnedToCore(worker,"mmtiles",12288,nullptr,1,nullptr,0)!=pdPASS)workerRunning=false;}
static void startDecoder(){if(!decoderRunning.exchange(true)&&xTaskCreatePinnedToCore(decoderTask,"mmdecode",8192,nullptr,2,nullptr,0)!=pdPASS)decoderRunning=false;}
static Slot *slotFor(const TileKey &k){for(auto &s:slots)if(s.valid&&s.key==k){s.used=++useClock;return &s;}return nullptr;}
// RAM slots get their buffer on first use: the 12 x 128 KB stay free in PSRAM until the Map app needs them.
static Slot *freeSlot(){Slot *best=nullptr;for(auto &s:slots){if(!s.bits&&!s.valid)s.bits=static_cast<uint8_t*>(heap_caps_malloc(TileBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!s.bits)continue;if(!s.valid)return &s;if(!best||s.used<best->used)best=&s;}return best;}
static bool isPending(const TileKey &k){for(int i=0;i<pendingCount;i++)if(pending[i]==k)return true;return false;}
static void unpend(const TileKey &k){for(int i=0;i<pendingCount;i++)if(pending[i]==k){pending[i]=pending[--pendingCount];return;}}
static bool knownMissing(const TileKey &k){for(auto &m:missing)if(m==k&&m.generation)return true;return false;}
static bool recentlyFailed(const TileKey &k){for(auto &f:failedRecent)if(f.key==k&&f.at&&millis()-f.at<RetryMs)return true;return false;}
void mapTilesRescan(){for(auto &m:missing)m.generation=0;for(auto &f:failedRecent)f.at=0;}
static void markMissing(const TileKey &k){TileKey m=k;m.generation=1;missing[missingNext]=m;missingNext=(missingNext+1)%MissingMemo;}
static void storeOnSd(const TileKey &k,const uint8_t *bits,size_t bytes=TileBytes){
 if(!sdOk)return;char dir[40];const char *root=tileRoot(k);snprintf(dir,sizeof(dir),"%s/%d/%lu",root,k.z,(unsigned long)k.x);
 if(strcmp(dir,madeDir)){storage->mkdir("/maps");storage->mkdir(root);char zdir[40];snprintf(zdir,sizeof(zdir),"%s/%d",root,k.z);storage->mkdir(zdir);storage->mkdir(dir);strcpy(madeDir,dir);}// 4 directory lookups only when the column changes
 char path[48];tilePath(path,sizeof(path),k);File f=storage->open(path,FILE_WRITE);bool ok=f&&f.write(bits,bytes)==bytes;if(f)f.close();if(ok)state.stored++;else{storage->remove(path);madeDir[0]=0;}
 for(auto &m:missing)if(m==k)m.generation=0;
}
const uint8_t *mapTile(int z,uint32_t x,uint32_t y,bool download){
 TileKey k;k.z=uint8_t(z);k.x=x;k.y=y;if(auto *s=slotFor(k))return s->bits;
 Slot *slot=freeSlot();
 if(isPending(k))return nullptr;// decoding or downloading
 if(slot&&sdOk&&!knownMissing(k)&&decodes){if(sdLoadsThisPass>=SdLoadsPerPass){sdDeferred=true;return nullptr;}
  char path[48];tilePath(path,sizeof(path),k);uint32_t t0=millis();File f=storage->open(path,FILE_READ);
  if(f){size_t size=f.size();if(pendingCount>=QueueDepth){f.close();sdDeferred=true;return nullptr;}// busy: retry on a later pass
   uint8_t *buf=size&&size<=PngCapacity?static_cast<uint8_t*>(heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)):nullptr;
   if(!buf&&size&&size<=PngCapacity){f.close();sdDeferred=true;return nullptr;}// out of PSRAM for now: not a missing tile
   bool read=buf&&f.read(buf,size)==size;f.close();sdLoadsThisPass++;state.lastReadMs=millis()-t0;state.lastBytes=size;
   if(read){DecodeJob j;j.key=k;j.data=buf;j.len=size;if(xQueueSend(decodes,&j,0)==pdTRUE){pending[pendingCount++]=k;startDecoder();return nullptr;}}
   if(buf)heap_caps_free(buf);if(read){sdDeferred=true;return nullptr;}markMissing(k);}// unreadable or empty: download again
  else{sdLoadsThisPass++;markMissing(k);}}// not on SD: download only
 if(download&&requests&&wifiConnected()&&!isPending(k)&&!recentlyFailed(k)&&pendingCount<QueueDepth){k.generation=generation.load();if(xQueueSend(requests,&k,0)==pdTRUE){pending[pendingCount++]=k;startWorker();}}
 return nullptr;
}
// OpenStreetMap tile policy: no prefetching, only the tiles on screen are requested.
// The old OpenTopoMap caches (/maps/otmp PNG, /maps/otmc RGB565) and the CARTO cache (/maps/cartoc) are deleted a few files at a time while the
// device is locked: one tile column directory per call, at most CleanupBudgetMs on the SD bus.
static const char *const OldRoots[]={"/maps/otmp","/maps/otmc","/maps/cartoc"};constexpr int OldRootCount=sizeof(OldRoots)/sizeof(OldRoots[0]);static int cleanupRoot=0;constexpr uint32_t CleanupBudgetMs=30;
static bool firstChild(const char *dir,char *out,size_t size,bool &isDir){
 File d=storage->open(dir);if(!d||!d.isDirectory())return false;File e=d.openNextFile();if(!e)return false;
 const char *name=e.name();const char *base=strrchr(name,'/');snprintf(out,size,"%s/%s",dir,base?base+1:name);isDir=e.isDirectory();return true;}
bool mapCleanupTrace=false;// serial "map cleanup step": print each step
void mapTilesCleanup(){
 if(mapCleanupTrace){Serial.printf("MAP_CLEANUP state sd=%d storage=%d root=%d\n",sdOk,storage!=nullptr,cleanupRoot);if(storage)for(auto *r:OldRoots){File d=storage->open(r);Serial.printf("MAP_CLEANUP probe %s exists=%d open=%d dir=%d\n",r,storage->exists(r),bool(d),d&&d.isDirectory());}if(cleanupRoot>=OldRootCount)cleanupRoot=0;}
 if(!sdOk||!storage||cleanupRoot>=OldRootCount)return;uint32_t t0=millis();const char *root=OldRoots[cleanupRoot];
 auto trace=[](const char *what,const char *path,int ok){if(mapCleanupTrace)Serial.printf("MAP_CLEANUP %s %s -> %d\n",what,path,ok);};
 if(!storage->exists(root)){trace("gone",root,1);cleanupRoot++;return;}
 char z[48],x[64],f[96];bool dir=false;
 if(!firstChild(root,z,sizeof(z),dir)){trace("rmdir",root,storage->rmdir(root));cleanupRoot++;return;}
 if(!dir){trace("remove",z,storage->remove(z));return;}
 if(!firstChild(z,x,sizeof(x),dir)){trace("rmdir",z,storage->rmdir(z));return;}
 if(!dir){trace("remove",x,storage->remove(x));return;}
 t0=millis();/* the budget starts here: on the 4 MHz SD the directory walk above alone can take 30 ms */
 do{if(!firstChild(x,f,sizeof(f),dir)){trace("rmdir",x,storage->rmdir(x));return;}bool ok=dir?storage->rmdir(f):storage->remove(f);trace(dir?"rmdir":"remove",f,ok);if(!ok)return;state.purged++;}while(millis()-t0<CleanupBudgetMs);
}
void mapTilesForget(){generation++;}
void mapTilesBeginPass(){sdLoadsThisPass=0;}
void mapTilesLoop(){
 if(sdDeferred){sdDeferred=false;revision++;}// another SD tile waits: repaint soon
 if(!results)return;TileResult r;
 while(xQueueReceive(results,&r,0)==pdTRUE){unpend(r.key);
  if(r.bits){
   if(!r.fromSd&&r.png)storeOnSd(r.key,r.png,r.pngLen);
   if(Slot *slot=freeSlot()){std::swap(slot->bits,r.bits);slot->key=r.key;slot->valid=true;slot->used=++useClock;}heap_caps_free(r.bits);/* hand the buffer over, free the slot's old one */if(r.fromSd)state.sdLoaded++;else state.downloaded++;revision++;}
  else if(r.fromSd){markMissing(r.key);revision++;}// undecodable: marked missing, the next download overwrites it (never deleted here)
  else if(r.key.generation){state.failed++;failedRecent[failedNext].key=r.key;failedRecent[failedNext].at=millis();failedNext=(failedNext+1)%8;revision++;}
  if(r.png)heap_caps_free(r.png);
 }
}
uint32_t mapTilesRevision(){return revision.load();}
MapTilesState mapTilesState(){auto s=state;s.worker=workerRunning.load();s.decoder=decoderRunning.load();s.lastDecodeMs=decodeMs.load();s.online=wifiConnected();s.queued=pendingCount;return s;}
}
