#include "../board/device_model.h"
#include "map_tiles.h"
#include "map_math.h"
#include "connectivity.h"
#include "map_tiles_ca.h"
#include "ui/build_info.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>
#include <new>
namespace device {
struct TileKey {uint8_t z=0;uint32_t x=0,y=0,generation=0;bool operator==(const TileKey &o)const{return z==o.z&&x==o.x&&y==o.y;}};
struct TileResult {TileKey key;uint8_t *bits=nullptr;};
struct Slot {TileKey key;uint8_t *bits=nullptr;uint32_t used=0;bool valid=false;};
constexpr int RamTiles=20,QueueDepth=24,MissingMemo=32;constexpr size_t PngCapacity=160*1024;constexpr uint32_t RequestGapMs=250,RetryMs=60000;
static fs::FS *storage=nullptr;static bool sdOk=false;static Slot slots[RamTiles];static uint32_t useClock=0;
static TileKey pending[QueueDepth];static int pendingCount=0;static TileKey missing[MissingMemo];static int missingNext=0;
static struct {TileKey key;uint32_t at=0;} failedRecent[8];static int failedNext=0;
static TileKey present[32];static int presentNext=0;// tiles seen on SD by mapTilePrefetch (generation 1 = valid)
static QueueHandle_t requests=nullptr,results=nullptr;static std::atomic<uint32_t> generation{1},revision{1};static MapTilesState state;
// Nothing is allocated until the Maps app asks for a tile; the download task (12 KB internal stack, ~290 KB PSRAM of
// PNG/decode buffers) ends after a minute without requests and starts again with the next one.
static std::atomic<bool> workerRunning{false};constexpr uint32_t WorkerIdleMs=60000;

// ---- worker (core 0): HTTPS fetch, PNG decode, 1 bpp conversion ----------
struct Decode {PNG *png;uint8_t *luma,*tone;bool ok;};
static int drawLine(PNGDRAW *draw){auto *d=static_cast<Decode*>(draw->pUser);if(draw->iWidth!=MapTileSize||draw->y>=MapTileSize){d->ok=false;return 0;}
 uint16_t line[MapTileSize];d->png->getLineAsRGB565(draw,line,PNG_RGB565_LITTLE_ENDIAN,0xffffffff);size_t row=size_t(draw->y)*MapTileSize;
 for(int x=0;x<MapTileSize;x++){d->luma[row+x]=mapLuma565(line[x]);d->tone[row+x]=mapOsmTone(line[x]);}return 1;}
static bool fetch(WiFiClientSecure &client,HTTPClient &http,const TileKey &k,uint8_t *png,size_t &length){
 char path[48];snprintf(path,sizeof(path),"/%d/%lu/%lu.png",k.z,(unsigned long)k.x,(unsigned long)k.y);length=0;
 if(!http.begin(client,"tile.openstreetmap.org",443,path,true))return false;
 http.setUserAgent("MatterMeshOS/" MM_VERSION " (LilyGO " MM_DEVICE_MODEL "; on-device map cache)");bool ok=false;
 if(http.GET()==200){int size=http.getSize();WiFiClient *stream=http.getStreamPtr();
  if(size>0&&size_t(size)<=PngCapacity&&stream){uint32_t deadline=millis()+15000;while(length<size_t(size)&&int32_t(millis()-deadline)<0){int n=stream->readBytes(png+length,size-length);if(n<=0){delay(5);continue;}length+=n;}ok=length==size_t(size);}}
 http.end();return ok;
}
static void workLoop(){
 WiFiClientSecure client;client.setCACert(MAP_TILES_ROOT_CA);client.setHandshakeTimeout(8);HTTPClient http;http.setReuse(true);http.setConnectTimeout(6000);http.setTimeout(8000);
 auto *png=static_cast<uint8_t*>(heap_caps_malloc(PngCapacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));auto *luma=static_cast<uint8_t*>(heap_caps_malloc(2*MapTileSize*MapTileSize,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));// luma, then tone
 void *decoderMemory=heap_caps_malloc(sizeof(PNG),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);PNG *decoder=decoderMemory?new(decoderMemory) PNG():nullptr;
 uint32_t idleMs=0;
 for(;;){TileKey k;
  // The kept-alive TLS session holds ~60 KB of internal RAM: dropped after 20 s idle, the whole task after a minute.
  if(xQueueReceive(requests,&k,pdMS_TO_TICKS(20000))!=pdTRUE){if(client.connected())client.stop();idleMs+=20000;
   if(idleMs<WorkerIdleMs)continue;
   workerRunning=false;// a request queued meanwhile is picked up here, or by the task mapTile() starts
   if(uxQueueMessagesWaiting(requests)&&!workerRunning.exchange(true)){idleMs=0;continue;}
   break;}
  idleMs=0;TileResult r;r.key=k;
  if(png&&luma&&decoder&&k.generation==generation.load()&&wifiConnected()){size_t length=0;
   if(fetch(client,http,k,png,length)&&decoder->openRAM(png,length,drawLine)==PNG_SUCCESS){Decode d{decoder,luma,luma+MapTileSize*MapTileSize,true};
    if(decoder->getWidth()==MapTileSize&&decoder->getHeight()==MapTileSize&&decoder->decode(&d,0)==PNG_SUCCESS&&d.ok){r.bits=static_cast<uint8_t*>(heap_caps_malloc(MapTileBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(r.bits)mapConvertTile(d.luma,d.tone,r.bits);}
    decoder->close();}
   vTaskDelay(pdMS_TO_TICKS(RequestGapMs));}
  else r.key.generation=0; // stale or offline: report back without a tile
  xQueueSend(results,&r,portMAX_DELAY);// dedicated task: wait for the main loop, a dropped result would leave the tile pending forever
 }
 if(decoder)decoder->~PNG();heap_caps_free(decoderMemory);heap_caps_free(luma);heap_caps_free(png);
}
// Ends after the loop so the TLS client and HTTP objects are destroyed first.
static void worker(void*){workLoop();vTaskDelete(nullptr);}

// ---- main loop: RAM slots, SD cache, request bookkeeping -----------------
void mapTilesBegin(fs::FS &fs,bool sdReady){
 storage=&fs;sdOk=sdReady;state.sd=sdReady;
}
// First tile request: the RAM slots and the queues (kept from then on).
static bool started(){
 if(requests&&results)return true;
 for(auto &s:slots)if(!s.bits)s.bits=static_cast<uint8_t*>(heap_caps_malloc(MapTileBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!requests)requests=xQueueCreate(QueueDepth,sizeof(TileKey));if(!results)results=xQueueCreate(4,sizeof(TileResult));
 return requests&&results;
}
static void startWorker(){if(!workerRunning.exchange(true)&&xTaskCreatePinnedToCore(worker,"mmtiles",12288,nullptr,1,nullptr,0)!=pdPASS)workerRunning=false;}
static Slot *slotFor(const TileKey &k){for(auto &s:slots)if(s.valid&&s.key==k){s.used=++useClock;return &s;}return nullptr;}
static Slot *freeSlot(){Slot *best=nullptr;for(auto &s:slots){if(!s.bits)continue;if(!s.valid)return &s;if(!best||s.used<best->used)best=&s;}return best;}
static bool isPending(const TileKey &k){for(int i=0;i<pendingCount;i++)if(pending[i]==k)return true;return false;}
static void unpend(const TileKey &k){for(int i=0;i<pendingCount;i++)if(pending[i]==k){pending[i]=pending[--pendingCount];return;}}
static bool knownPresent(const TileKey &k){for(auto &p:present)if(p==k&&p.generation)return true;return false;}
static bool knownMissing(const TileKey &k){for(auto &m:missing)if(m==k&&m.generation)return true;return false;}
static bool recentlyFailed(const TileKey &k){for(auto &f:failedRecent)if(f.key==k&&f.at&&millis()-f.at<RetryMs)return true;return false;}
static bool loadFromSd(const TileKey &k,Slot &slot){
 if(!sdOk||knownMissing(k))return false;slot.valid=false;// the slot may hold another tile: never leave it half overwritten
 char path[48];mapTilePath(path,sizeof(path),k.z,k.x,k.y);File f=storage->open(path,FILE_READ);
 bool ok=f&&f.size()==MapTileBytes&&f.read(slot.bits,MapTileBytes)==MapTileBytes;if(f)f.close();
 if(!ok){TileKey m=k;m.generation=1;missing[missingNext]=m;missingNext=(missingNext+1)%MissingMemo;}return ok;
}
static void storeOnSd(const TileKey &k,const uint8_t *bits){
 if(!sdOk)return;
 // Folders made once per zoom level and per column (each mkdir is a FAT directory walk on a 4 MHz SD); a failed
 // write forgets them, so a re-inserted or re-formatted card gets them again on the next tile.
 static uint32_t levelsMade=0;static int columnZ=-1;static uint32_t columnX=0;char dir[40];
 if(!(levelsMade&(1u<<(k.z&31)))){storage->mkdir("/maps");storage->mkdir("/maps/osm1");snprintf(dir,sizeof(dir),"/maps/osm1/%d",k.z);storage->mkdir(dir);levelsMade|=1u<<(k.z&31);}
 if(columnZ!=k.z||columnX!=k.x){snprintf(dir,sizeof(dir),"/maps/osm1/%d/%lu",k.z,(unsigned long)k.x);storage->mkdir(dir);columnZ=k.z;columnX=k.x;}
 char path[48];mapTilePath(path,sizeof(path),k.z,k.x,k.y);File f=storage->open(path,FILE_WRITE);bool ok=f&&f.write(bits,MapTileBytes)==MapTileBytes;if(f)f.close();if(ok)state.stored++;else{storage->remove(path);levelsMade=0;columnZ=-1;}
 for(auto &m:missing)if(m==k)m.generation=0;
}
const uint8_t *mapTile(int z,uint32_t x,uint32_t y,bool download){
 if(!started())return nullptr;
 TileKey k;k.z=uint8_t(z);k.x=x;k.y=y;if(auto *s=slotFor(k))return s->bits;
 Slot *slot=freeSlot();if(slot&&loadFromSd(k,*slot)){slot->key=k;slot->valid=true;slot->used=++useClock;return slot->bits;}
 if(download&&requests&&wifiConnected()&&!isPending(k)&&!recentlyFailed(k)&&pendingCount<QueueDepth){k.generation=generation.load();if(xQueueSend(requests,&k,0)==pdTRUE){pending[pendingCount++]=k;startWorker();}}
 return nullptr;
}
// Ring tiles around the view: never read on the UI loop (20 RAM slots could not hold the whole ring, so tiles evicted
// each other and every frame re-read the card); only checked on SD and downloaded when missing.
void mapTilePrefetch(int z,uint32_t x,uint32_t y){
 if(!started())return;TileKey k;k.z=uint8_t(z);k.x=x;k.y=y;
 for(auto &s:slots)if(s.valid&&s.key==k)return;
 if(knownPresent(k))return;
 if(sdOk&&!knownMissing(k)){char path[48];mapTilePath(path,sizeof(path),k.z,k.x,k.y);TileKey m=k;m.generation=1;
  if(storage->exists(path)){present[presentNext]=m;presentNext=(presentNext+1)%32;return;}
  missing[missingNext]=m;missingNext=(missingNext+1)%MissingMemo;}
 if(requests&&wifiConnected()&&!isPending(k)&&!recentlyFailed(k)&&pendingCount<QueueDepth){k.generation=generation.load();if(xQueueSend(requests,&k,0)==pdTRUE){pending[pendingCount++]=k;startWorker();}}
}
void mapTilesForget(){generation++;}
void mapTilesRescan(){for(auto &m:missing)m.generation=0;for(auto &p:present)p.generation=0;for(auto &f:failedRecent)f.at=0;}
void mapTilesLoop(){
 if(!results)return;TileResult r;
 // A worker that could not start (internal RAM short) is retried here, or queued tiles would wait until reboot.
 if(requests&&uxQueueMessagesWaiting(requests)&&!workerRunning.load())startWorker();
 while(xQueueReceive(results,&r,0)==pdTRUE){unpend(r.key);
  if(r.bits){storeOnSd(r.key,r.bits);if(Slot *slot=freeSlot()){memcpy(slot->bits,r.bits,MapTileBytes);slot->key=r.key;slot->valid=true;slot->used=++useClock;}heap_caps_free(r.bits);state.downloaded++;revision++;}
  else if(r.key.generation){state.failed++;failedRecent[failedNext].key=r.key;failedRecent[failedNext].at=millis();failedNext=(failedNext+1)%8;revision++;}
 }
}
uint32_t mapTilesRevision(){return revision.load();}
MapTilesState mapTilesState(){auto s=state;s.online=wifiConnected();s.queued=pendingCount;return s;}
}
