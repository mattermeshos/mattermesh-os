#include "radar.h"
#include "https.h"
#include "connectivity.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <new>
namespace device {
// Worker (core 0) while the tab is open: frame list every RADAR_FRAMES_MS, then the tiles the view asks for.
constexpr int Slots=6,Pending=8,TileSize=256;constexpr size_t PngCapacity=96*1024;
struct Slot {int z=-1;uint32_t x=0,y=0,frame=0,used=0;uint16_t *rgb=nullptr;uint8_t *alpha=nullptr;bool ready=false;};
struct Want {int z;uint32_t x,y;};
static Slot slots[Slots];static Want pending[Pending];static int pendingCount=0;
static RadarState state;static SemaphoreHandle_t mutex=nullptr;static String host,path;static uint32_t framesAt=0,useClock=0;
static TaskHandle_t worker=nullptr;static volatile bool running=false;static bool framesWanted=true;
static void publish(){state.revision++;}
static void setMessage(const String &m){xSemaphoreTake(mutex,portMAX_DELAY);if(state.message!=m){state.message=m;publish();}xSemaphoreGive(mutex);}
// RainViewer frame list: the newest "past" frame (the nowcast is not offered any more).
static bool fetchFrames(){
 WiFiClientSecure client;HTTPClient http;secureClient(client,http);bool ok=false;
 if(http.begin(client,"https://api.rainviewer.com/public/weather-maps.json")){int status=http.GET();
  if(status==200){DynamicJsonDocument json(8192);if(!deserializeJson(json,http.getString())){auto past=json["radar"]["past"].as<JsonArray>();String h=json["host"]|"";
    if(past.size()&&h.length()){auto last=past[past.size()-1];String p=last["path"]|"";uint32_t t=last["time"]|0u;
     if(p.length()&&t){xSemaphoreTake(mutex,portMAX_DELAY);bool changed=t!=state.frameTime;host=h;path=p;state.frameTime=t;if(changed)for(auto &s:slots)s.ready=false;state.message="";publish();xSemaphoreGive(mutex);ok=true;}}}}
  if(!ok)setMessage("Radar list unavailable / HTTP "+String(status));http.end();}
 else setMessage("Radar connection failed");
 return ok;
}
// PNG rows -> RGB565 (panel byte order) + alpha; RainViewer tiles are RGBA (palette+tRNS also handled).
struct Decode {PNG *png;Slot *slot;bool ok;};
static int drawRow(PNGDRAW *d){auto *dec=static_cast<Decode*>(d->pUser);if(d->iWidth!=TileSize||d->y>=TileSize){dec->ok=false;return 0;}
 uint16_t *rgb=dec->slot->rgb+size_t(d->y)*TileSize;uint8_t *alpha=dec->slot->alpha+size_t(d->y)*TileSize;const uint8_t *p=d->pPixels;
 auto put=[&](int x,uint8_t r,uint8_t g,uint8_t b,uint8_t a){uint16_t v=uint16_t(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3));rgb[x]=uint16_t(v<<8|v>>8);alpha[x]=a;};
 if(d->iPixelType==PNG_PIXEL_TRUECOLOR_ALPHA&&d->iBpp==8)/* iBpp: bits per channel */for(int x=0;x<TileSize;x++)put(x,p[x*4],p[x*4+1],p[x*4+2],p[x*4+3]);
 else if(d->iPixelType==PNG_PIXEL_INDEXED&&d->iBpp==8)for(int x=0;x<TileSize;x++){uint8_t i=p[x];const uint8_t *c=d->pPalette+i*3;put(x,c[0],c[1],c[2],d->iHasAlpha?d->pPalette[768+i]:255);}
 else{dec->ok=false;return 0;}
 return 1;}
static void worker_(void *){
 auto *png=static_cast<uint8_t*>(heap_caps_malloc(PngCapacity,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));void *mem=heap_caps_malloc(sizeof(PNG),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);PNG *decoder=mem?new(mem) PNG():nullptr;
 uint32_t lastFrames=0;
 // One kept-alive TLS connection for all tiles: a handshake per tile kept core 0 busy long enough to trip the task watchdog.
 WiFiClientSecure tileClient;HTTPClient tileHttp;secureClient(tileClient,tileHttp);tileHttp.setReuse(true);
 while(running){
  if(!connectivityStatus().connected){setMessage("Waiting for Wi-Fi");vTaskDelay(pdMS_TO_TICKS(1000));continue;}
  bool wantFrames;xSemaphoreTake(mutex,portMAX_DELAY);wantFrames=framesWanted||!path.length()||uint32_t(millis()-framesAt)>=RADAR_FRAMES_MS;xSemaphoreGive(mutex);
  if(wantFrames&&(!lastFrames||uint32_t(millis()-lastFrames)>=5000)){lastFrames=millis();bool ok=fetchFrames();xSemaphoreTake(mutex,portMAX_DELAY);framesWanted=false;framesAt=ok?millis():millis()-RADAR_FRAMES_MS+RADAR_RETRY_MS;xSemaphoreGive(mutex);continue;}
  // Next wanted tile, into the least recently used slot.
  Want w;String url;Slot *slot=nullptr;uint32_t frame=0;
  xSemaphoreTake(mutex,portMAX_DELAY);
  if(pendingCount&&path.length()){w=pending[0];for(int i=1;i<pendingCount;i++)pending[i-1]=pending[i];pendingCount--;
   for(auto &s:slots)if(!slot||(!s.ready&&slot->ready)||(s.ready==slot->ready&&s.used<slot->used))slot=&s;
   slot->ready=false;slot->z=w.z;slot->x=w.x;slot->y=w.y;frame=state.frameTime;slot->frame=frame;
   url=host+path+"/256/"+String(w.z)+"/"+String(w.x)+"/"+String(w.y)+"/2/1_1.png";state.busy=true;}
  xSemaphoreGive(mutex);
  if(!slot){vTaskDelay(pdMS_TO_TICKS(100));continue;}
  if(!slot->rgb)slot->rgb=static_cast<uint16_t*>(heap_caps_malloc(TileSize*TileSize*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!slot->alpha)slot->alpha=static_cast<uint8_t*>(heap_caps_malloc(TileSize*TileSize,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  bool ok=false;
  if(png&&decoder&&slot->rgb&&slot->alpha){HTTPClient &http=tileHttp;
   if(http.begin(tileClient,url)){if(http.GET()==200){int size=http.getSize();WiFiClient *stream=http.getStreamPtr();size_t length=0;
     if(size>0&&size_t(size)<=PngCapacity&&stream){uint32_t deadline=millis()+15000;while(length<size_t(size)&&int32_t(millis()-deadline)<0){int n=stream->readBytes(png+length,size-length);if(n<=0){delay(5);continue;}length+=n;}}
     if(length&&length==size_t(size)&&decoder->openRAM(png,length,drawRow)==PNG_SUCCESS){Decode d{decoder,slot,true};ok=decoder->getWidth()==TileSize&&decoder->getHeight()==TileSize&&decoder->decode(&d,0)==PNG_SUCCESS&&d.ok;decoder->close();}}
    http.end();}}
  vTaskDelay(pdMS_TO_TICKS(ok?10:2000));// a failed tile is asked for again by the next frame: do not loop on it
  xSemaphoreTake(mutex,portMAX_DELAY);state.busy=false;if(ok&&slot->frame==state.frameTime){slot->ready=true;slot->used=++useClock;}else if(!ok){slot->z=-1;state.message="Radar tile failed";}publish();xSemaphoreGive(mutex);
 }
 tileHttp.end();tileClient.stop();if(decoder)decoder->~PNG();heap_caps_free(mem);heap_caps_free(png);
 xSemaphoreTake(mutex,portMAX_DELAY);for(auto &s:slots){heap_caps_free(s.rgb);heap_caps_free(s.alpha);s=Slot();}pendingCount=0;worker=nullptr;publish();xSemaphoreGive(mutex);
 vTaskDelete(nullptr);
}
void radarBegin(){mutex=xSemaphoreCreateMutex();}
void radarSetOpen(bool open){
 if(!mutex||open==state.open)return;
 xSemaphoreTake(mutex,portMAX_DELAY);state.open=open;publish();xSemaphoreGive(mutex);
 if(open){if(worker)return;running=true;framesWanted=!path.length()||uint32_t(millis()-framesAt)>=RADAR_FRAMES_MS;// Priority 0 (= idle) on core 0: TLS and PNG work share the core with IDLE0 instead of starving it.
  if(xTaskCreatePinnedToCore(worker_,"matter-radar",10240,nullptr,tskIDLE_PRIORITY,&worker,0)!=pdPASS){worker=nullptr;running=false;setMessage("Radar worker unavailable");}}
 else running=false;// the worker frees the tiles and ends after its current download
}
void radarRefresh(){if(!mutex)return;xSemaphoreTake(mutex,portMAX_DELAY);framesWanted=true;xSemaphoreGive(mutex);}
RadarState radarState(){if(!mutex)return state;xSemaphoreTake(mutex,portMAX_DELAY);auto copy=state;xSemaphoreGive(mutex);return copy;}
uint32_t radarRevision(){if(!mutex)return 0;xSemaphoreTake(mutex,portMAX_DELAY);uint32_t r=state.revision;xSemaphoreGive(mutex);return r;}
bool radarTile(int z,uint32_t x,uint32_t y,const uint16_t *&rgb,const uint8_t *&alpha){
 if(!mutex||!state.open)return false;bool found=false;
 xSemaphoreTake(mutex,portMAX_DELAY);
 for(auto &s:slots)if(s.ready&&s.z==z&&s.x==x&&s.y==y&&s.frame==state.frameTime){s.used=++useClock;rgb=s.rgb;alpha=s.alpha;found=true;break;}
 if(!found){bool queued=false;for(int i=0;i<pendingCount;i++)if(pending[i].z==z&&pending[i].x==x&&pending[i].y==y)queued=true;
  for(auto &s:slots)if(!s.ready&&s.z==z&&s.x==x&&s.y==y&&state.busy)queued=true;// downloading now
  if(!queued&&pendingCount<Pending)pending[pendingCount++]={z,x,y};}
 xSemaphoreGive(mutex);return found;
}
}
