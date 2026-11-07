#include "link_viewer.h"
#include "link_html.h"
#include "ca_bundle.h"
#include "connectivity.h"
#include "ui/build_info.h"
#include "../board/device_model.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <JPEGDEC.h>
#include <PNGdec.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <algorithm>
#include <atomic>
#include <functional>
#include <new>
namespace device {
namespace {
// Shared with the UI (main loop): guarded by `mutex`. The picture buffer is written by the worker only while
// phase==Loading and read by the UI only while phase==Image, so it needs no lock of its own.
SemaphoreHandle_t mutex=nullptr;QueueHandle_t requests=nullptr;bool workerUp=false;
LinkState state;uint16_t *picture=nullptr;size_t pictureBytes=0;
std::atomic<uint32_t> generation{0};// bumped by open/close: a worker download of an older generation stops
std::atomic<uint32_t> revision{1};
struct Request {uint32_t generation;char url[512];};

void lock(){if(mutex)xSemaphoreTake(mutex,portMAX_DELAY);}void unlock(){if(mutex)xSemaphoreGive(mutex);}
bool current(uint32_t gen){return generation.load()==gen;}
void publish(uint32_t gen,const std::function<void(LinkState&)> &change){lock();if(current(gen)){change(state);state.revision++;revision++;}unlock();}
void fail(uint32_t gen,const std::string &message){publish(gen,[&](LinkState &s){s.phase=LinkPhase::Error;s.message=message;});}
void freePicture(){if(picture){heap_caps_free(picture);picture=nullptr;pictureBytes=0;}}

// Download sink: collects the body in PSRAM up to `limit` (HTTPClient decodes chunked transfers for us).
class Sink:public Stream{
public:
 uint8_t *data=nullptr;size_t size=0,capacity=0,limit=0,expected=0;bool overflow=false;/* expected: Content-Length (0 unknown) */uint32_t gen=0,lastReport=0;
 ~Sink(){if(data)heap_caps_free(data);}
 size_t write(uint8_t c)override{return write(&c,1);}
 size_t write(const uint8_t *buf,size_t n)override{
  if(!current(gen))return 0;// cancelled: HTTPClient stops on a short write
  if(size+n>limit){overflow=true;return 0;}
  if(size+n>capacity){size_t next=capacity?capacity*2:(expected&&expected<=limit?expected:64*1024);while(next<size+n)next*=2;if(next>limit)next=limit;
   auto *grown=static_cast<uint8_t*>(heap_caps_realloc(data,next,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!grown){overflow=true;return 0;}data=grown;capacity=next;}
  memcpy(data+size,buf,n);size+=n;
  if(millis()-lastReport>250||size==expected){lastReport=millis();size_t got=size;publish(gen,[&](LinkState &s){s.received=got;});}
  return n;}
 int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}void flush()override{}
};

// Reads the response body into the sink: Content-Length, chunked or until the server closes. Gives up after 15 s
// without data or 120 s in all, and as soon as the link is cancelled (HTTPClient::writeToStream waits forever on a
// connection that stays open without sending). Returns an error text, "" when the body is complete.
std::string readBody(WiFiClient *net,bool chunked,int length,Sink &sink){
 uint8_t buf[2048];uint32_t start=millis(),lastData=start;
 auto wait=[&]()->const char*{if(!current(sink.gen))return "cancelled";if(millis()-lastData>15000||millis()-start>120000)return "Server stopped sending";vTaskDelay(1);return nullptr;};
 auto take=[&](size_t want)->int{// bytes into buf (<= want); 0 = closed before any
  for(;;){int av=net->available();if(av>0){int n=net->read(buf,std::min<size_t>(std::min<size_t>(want,sizeof(buf)),size_t(av)));if(n>0){lastData=millis();return n;}}
   if(!net->connected()&&net->available()<=0)return 0;if(const char *e=wait()){(void)e;return -1;}}};
 auto store=[&](size_t n)->bool{return sink.write(buf,n)==n;};
 if(chunked){
  for(;;){std::string line;for(;;){int n=take(1);if(n<=0)return n<0?"Server stopped sending":"Connection closed";if(buf[0]=='\n')break;if(buf[0]!='\r'&&line.size()<20)line+=char(buf[0]);}
   if(line.empty())continue;size_t size=strtoul(line.c_str(),nullptr,16);if(size==0)return "";
   while(size){int n=take(size);if(n<=0)return n<0?"Server stopped sending":"Connection closed";if(!store(n))return sink.overflow?"overflow":"cancelled";size-=n;}}
 }
 size_t remaining=length>0?size_t(length):SIZE_MAX;
 while(remaining){int n=take(std::min<size_t>(remaining,sizeof(buf)));if(n<0)return "Server stopped sending";if(n==0)return length>0?"Connection closed":"";if(!store(n))return sink.overflow?"overflow":"cancelled";if(length>0)remaining-=n;}
 return "";
}
// PNG chunk check before decoding untrusted files: chunk lengths stay inside the file, PLTE is a multiple of 3 and at
// most 256 entries, tRNS at most 256 bytes (PNGdec copies them without bounds checks).
bool pngChunksSafe(const uint8_t *d,size_t n){
 size_t at=8;while(at+12<=n){uint32_t len=uint32_t(d[at])<<24|uint32_t(d[at+1])<<16|uint32_t(d[at+2])<<8|d[at+3];if(len>n-at-12)return false;
  const uint8_t *type=d+at+4;if(!memcmp(type,"PLTE",4)&&(len%3||len>768))return false;if(!memcmp(type,"tRNS",4)&&len>256)return false;if(!memcmp(type,"IEND",4))return true;at+=12+len;}
 return false;}

// ---- decoders: into a PSRAM picture of at most LinkImageMaxW x LinkImageMaxH (panel byte order) ----------
// Decoded picture: the source (after the decoder's own 1/2..1/8 scale) is sw x sh, fitted into w x h by nearest
// neighbour driven from the target: target row ty samples source row ty*sh/h, so every target row and column is set.
struct Picture {uint16_t *pixels=nullptr;int w=0,h=0,sw=0,sh=0;};
inline int sourceOf(int t,int sourceSize,int targetSize){return int(int64_t(t)*sourceSize/targetSize);}
inline int firstTarget(int s,int sourceSize,int targetSize){return int((int64_t(s)*targetSize+sourceSize-1)/sourceSize);}// first t with sourceOf(t)>=s
int jpegDraw(JPEGDRAW *d){auto *p=static_cast<Picture*>(d->pUser);
 int ty0=firstTarget(d->y,p->sh,p->h),ty1=std::min(p->h,firstTarget(d->y+d->iHeight,p->sh,p->h));
 int tx0=firstTarget(d->x,p->sw,p->w),tx1=std::min(p->w,firstTarget(d->x+d->iWidth,p->sw,p->w));
 for(int ty=ty0;ty<ty1;ty++){const uint16_t *row=d->pPixels+size_t(sourceOf(ty,p->sh,p->h)-d->y)*d->iWidth;uint16_t *out=p->pixels+size_t(ty)*p->w;
  for(int tx=tx0;tx<tx1;tx++)out[tx]=row[sourceOf(tx,p->sw,p->w)-d->x];}
 return 1;}
struct PngJob {PNG *png;Picture *pic;uint16_t *line;};
// PNGdec keeps one decoded row inside the PNG object: PNG_MAX_BUFFERED_PIXELS (platformio.ini) sets the widest row.
constexpr int PngMaxWidth=((PNG_MAX_BUFFERED_PIXELS-34)/2-1)/4;// two aligned rows of 4*width+1 bytes
int pngDraw(PNGDRAW *d){auto *job=static_cast<PngJob*>(d->pUser);auto *p=job->pic;int ty0=firstTarget(d->y,p->sh,p->h),ty1=std::min(p->h,firstTarget(d->y+1,p->sh,p->h));if(ty0>=ty1)return 1;
 job->png->getLineAsRGB565(d,job->line,PNG_RGB565_BIG_ENDIAN,0x00ffffff);// blend transparency on white (0xffffffff would ignore alpha)
 for(int ty=ty0;ty<ty1;ty++){uint16_t *out=p->pixels+size_t(ty)*p->w;for(int tx=0;tx<p->w;tx++)out[tx]=job->line[sourceOf(tx,p->sw,p->w)];}return 1;}
bool allocPicture(Picture &p,int sw,int sh){p.sw=sw;p.sh=sh;double f=std::min(1.0,std::min(double(LinkImageMaxW)/sw,double(LinkImageMaxH)/sh));p.w=std::max(1,int(sw*f+0.5));p.h=std::max(1,int(sh*f+0.5));
 p.pixels=static_cast<uint16_t*>(heap_caps_malloc(size_t(p.w)*p.h*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(p.pixels)for(size_t i=0;i<size_t(p.w)*p.h;i++)p.pixels[i]=0xffff;return p.pixels!=nullptr;}

bool decodeJpeg(uint8_t *data,size_t size,Picture &pic,int &srcW,int &srcH,std::string &error){
 void *mem=heap_caps_malloc(sizeof(JPEGDEC),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!mem){error="Out of memory";return false;}
 auto *jpeg=new(mem) JPEGDEC();bool ok=false;
 if(jpeg->openRAM(data,int(size),jpegDraw)){
  srcW=jpeg->getWidth();srcH=jpeg->getHeight();
  {// Progressive JPEGs decode at 1/8 only (DC coefficients): a small but whole picture.
   bool progressive=jpeg->getJPEGType()==JPEG_MODE_PROGRESSIVE;int scale=progressive?8:1,option=0;while((srcW/scale>LinkImageMaxW||srcH/scale>LinkImageMaxH)&&scale<8)scale*=2;
   option=scale==2?JPEG_SCALE_HALF:scale==4?JPEG_SCALE_QUARTER:scale==8?JPEG_SCALE_EIGHTH:0;
   int w=(srcW+scale-1)/scale,h=(srcH+scale-1)/scale;
   if(!allocPicture(pic,w,h))error="Out of memory";
   else{jpeg->setPixelType(RGB565_BIG_ENDIAN);jpeg->setUserPointer(&pic);ok=jpeg->decode(0,0,option)==1;if(!ok){error="JPEG decode failed";heap_caps_free(pic.pixels);pic.pixels=nullptr;}}}
  jpeg->close();}
 else error="Not a valid JPEG";
 jpeg->~JPEGDEC();heap_caps_free(mem);return ok;}
bool decodePng(uint8_t *data,size_t size,Picture &pic,int &srcW,int &srcH,std::string &error){
 if(!pngChunksSafe(data,size)){error="Damaged or unsafe PNG";return false;}
 void *mem=heap_caps_malloc(sizeof(PNG),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!mem){error="Out of memory";return false;}
 auto *png=new(mem) PNG();bool ok=false;
 int rc=png->openRAM(data,int(size),pngDraw);// PNG_TOO_BIG comes back without setting the last error
 if(rc==PNG_SUCCESS){
  srcW=png->getWidth();srcH=png->getHeight();
  auto *line=static_cast<uint16_t*>(heap_caps_malloc(size_t(srcW)*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!line||srcW<=0||srcH<=0||srcW>PngMaxWidth||srcH>8192)error=line?"PNG too wide (max "+std::to_string(PngMaxWidth)+" px)":"Out of memory";
  else if(!allocPicture(pic,srcW,srcH))error="Out of memory";
  else{PngJob job{png,&pic,line};ok=png->decode(&job,0)==PNG_SUCCESS;if(!ok){error="PNG decode failed";heap_caps_free(pic.pixels);pic.pixels=nullptr;}}
  if(line)heap_caps_free(line);png->close();}
 else{error=rc==PNG_TOO_BIG?"PNG too wide (max "+std::to_string(PngMaxWidth)+" px)":rc==PNG_UNSUPPORTED_FEATURE?"PNG format not supported (interlaced?)":"Not a valid PNG";}
 png->~PNG();heap_caps_free(mem);return ok;}

// ---- worker ------------------------------------------------------------------------------------------------
std::string lowerType(String type){type.toLowerCase();int semi=type.indexOf(';');if(semi>=0)type=type.substring(0,semi);type.trim();return type.c_str();}
bool startsWith(const std::string &s,const char *p){return s.rfind(p,0)==0;}
bool looksJpeg(const uint8_t *d,size_t n){return n>3&&d[0]==0xFF&&d[1]==0xD8&&d[2]==0xFF;}
bool looksPng(const uint8_t *d,size_t n){return n>8&&!memcmp(d,"\x89PNG\r\n\x1a\n",8);}

// fromPage: the og:image of a photo page; its errors keep the page on screen (pictureError) instead of failing.
// Redirects are followed here, not by HTTPClient: each hop gets a fresh client for its own host (HTTPClient would
// reuse the old connection), http -> https upgrades work, and an https -> http downgrade is refused.
void fetch(uint32_t gen,std::string url,bool fromPage=false){
 auto fail=[&](uint32_t g,const std::string &message){if(fromPage)publish(g,[&](LinkState &s){s.phase=LinkPhase::Page;s.pictureError=message;s.message="";});else device::fail(g,message);};
 if(!wifiConnected()){fail(gen,"Wi-Fi not connected");return;}
 std::string follow;
 for(int hop=0;;hop++){
 if(!current(gen))return;
 bool tls=linkStartsI(url.data(),url.size(),0,"https://");std::string location;
 auto *secure=tls?new WiFiClientSecure():nullptr;auto *plain=tls?nullptr:new WiFiClient();
 if(secure){secure->setCACertBundle(LINK_CA_BUNDLE);secure->setHandshakeTimeout(10);}
 {HTTPClient http;http.setReuse(false);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);http.setConnectTimeout(8000);http.setTimeout(10000);
  http.setUserAgent("MatterMeshOS/" MM_VERSION " (" MM_DEVICE_MODEL "; link viewer)");
  const char *keep[]={"Content-Type","Content-Length","Location","Transfer-Encoding"};http.collectHeaders(keep,4);
  bool begun=tls?http.begin(*secure,url.c_str()):http.begin(*plain,url.c_str());
  if(!begun){fail(gen,"Invalid address");}
  else{http.addHeader("Accept","text/html,image/jpeg,image/png;q=0.9,*/*;q=0.5");
   int code=current(gen)?http.GET():-1;
   if(!current(gen)){}
   else if(code>=300&&code<400&&code!=304&&http.hasHeader("Location"))location=http.header("Location").c_str();
   else if(code<=0){fail(gen,tls&&code==HTTPC_ERROR_CONNECTION_REFUSED?"Secure connection failed (certificate?)":std::string("Connection failed (")+HTTPClient::errorToString(code).c_str()+")");}
   else if(code!=200){fail(gen,"Server answered HTTP "+std::to_string(code));}
   else{
    std::string type=lowerType(http.header("Content-Type"));int length=http.getSize();
    bool image=startsWith(type,"image/");size_t limit=image?LinkMaxImageBytes:LinkMaxPageBytes;
    publish(gen,[&](LinkState &s){s.total=length>0?uint32_t(length):0;});
    if(image&&type!="image/jpeg"&&type!="image/jpg"&&type!="image/pjpeg"&&type!="image/png")fail(gen,"Picture format not supported ("+type+")");
    else if(!image&&!startsWith(type,"text/")&&!type.empty()&&type!="application/xhtml+xml")fail(gen,"Content not supported ("+type+")");
    else if(image&&length>0&&size_t(length)>limit)fail(gen,"Too large to open ("+std::to_string(length/1024)+" KB)");
    else{
     Sink sink;sink.gen=gen;sink.limit=limit;sink.expected=length>0?size_t(length):0;
     String encoding=http.header("Transfer-Encoding");encoding.toLowerCase();
     std::string readError=readBody(http.getStreamPtr(),encoding.indexOf("chunked")>=0,length,sink);
     if(!current(gen)){}
     else if(sink.overflow&&(image||!sink.size))fail(gen,"Too large to open");
     else if(!readError.empty()&&readError!="overflow"&&(image||!sink.size))fail(gen,"Download failed ("+readError+")");
     else if(image&&length>0&&sink.size<size_t(length))fail(gen,"Download interrupted ("+std::to_string(sink.size/1024)+" of "+std::to_string(length/1024)+" KB)");
     else if(image||looksJpeg(sink.data,sink.size)||looksPng(sink.data,sink.size)){
      Picture pic;int srcW=0,srcH=0;std::string error;
      bool ok=looksPng(sink.data,sink.size)?decodePng(sink.data,sink.size,pic,srcW,srcH,error):looksJpeg(sink.data,sink.size)?decodeJpeg(sink.data,sink.size,pic,srcW,srcH,error):(error="Not a JPEG or PNG picture",false);
      if(!ok)fail(gen,error);
      else{lock();if(current(gen)){freePicture();picture=pic.pixels;pictureBytes=size_t(pic.w)*pic.h*2;state.phase=LinkPhase::Image;state.pictureError="";state.imageW=pic.w;state.imageH=pic.h;state.sourceW=srcW;state.sourceH=srcH;state.message="";state.revision++;revision++;pic.pixels=nullptr;}unlock();
       if(pic.pixels)heap_caps_free(pic.pixels);}
     }else{
      auto page=linkParseHtml(reinterpret_cast<const char*>(sink.data),sink.size,url,LinkMaxText,LinkMaxLinks);
      if(sink.overflow)page.text+="\n\n(page truncated: first "+std::to_string(LinkMaxPageBytes/1024)+" KB)";
      bool photo=!fromPage&&linkPhotoPage(url,page);
      publish(gen,[&](LinkState &s){s.phase=photo?LinkPhase::Loading:LinkPhase::Page;s.page=true;s.title=page.title;s.text=page.text;s.imageUrl=page.image;s.links.clear();for(auto &l:page.links)s.links.push_back({l.text,l.url});s.message=photo?"Opening picture":"";s.received=s.total=0;});
      if(photo)follow=page.image;// fetched once this connection is closed (one TLS session at a time)
     }
    }
   }
   http.end();}
 }
 delete secure;delete plain;
 if(location.empty())break;
 std::string next=linkResolve(url,location);
 if(hop>=5){fail(gen,"Too many redirects");return;}
 if(next.empty()){fail(gen,"Bad redirect");return;}
 if(tls&&!linkStartsI(next.data(),next.size(),0,"https://")){fail(gen,"Redirect to an insecure http:// address refused");return;}
 url=next;publish(gen,[&](LinkState &s){s.host=linkHost(url);});
 }
 if(!follow.empty()&&current(gen))fetch(gen,follow,true);
}
// Started by linkOpen; after 20 s without requests it ends and gives its 16 KB of internal RAM back (TLS for the
// weather, radar and maps needs it). workerUp changes only under the mutex, and linkOpen queues under it too.
void worker(void*){Request r;for(;;){if(xQueueReceive(requests,&r,pdMS_TO_TICKS(20000))==pdTRUE){if(current(r.generation)){networkJobBegin();fetch(r.generation,r.url);networkJobEnd();}continue;}
 lock();if(!uxQueueMessagesWaiting(requests)){workerUp=false;unlock();vTaskDelete(nullptr);}unlock();}}
}

void linkBegin(){
 if(!mutex)mutex=xSemaphoreCreateMutex();if(!requests)requests=xQueueCreate(1,sizeof(Request));
}
bool linkOpen(const std::string &url){
 if(!mutex||!requests||!(linkStartsI(url.data(),url.size(),0,"http://")||linkStartsI(url.data(),url.size(),0,"https://"))||url.size()>=sizeof(Request::url))return false;
 uint32_t gen=++generation;
 Request r;r.generation=gen;strncpy(r.url,url.c_str(),sizeof(r.url));r.url[sizeof(r.url)-1]=0;
 lock();freePicture();state=LinkState();state.phase=LinkPhase::Loading;state.url=url;state.host=linkHost(url);state.message="Loading...";
 // 16 KB stack: TLS handshake + HTTPClient + the HTML pass (the decoders' state lives in PSRAM).
 if(!workerUp)workerUp=xTaskCreatePinnedToCore(worker,"mmlink",16384,nullptr,1,nullptr,0)==pdPASS;
 if(workerUp){Request old;while(xQueueReceive(requests,&old,0)==pdTRUE){}xQueueSend(requests,&r,0);}// only the newest request waits
 else{state.phase=LinkPhase::Error;state.message="Not enough memory to open links";}
 state.revision=revision.load()+1;revision++;unlock();return true;
}
void linkClose(){generation++;lock();freePicture();state=LinkState();state.revision=revision.load()+1;revision++;unlock();}
LinkState linkState(){lock();LinkState copy=state;unlock();return copy;}
uint32_t linkRevision(){return revision.load();}
LinkMeta linkMeta(){lock();LinkMeta m;m.phase=state.phase;m.page=state.page;m.url=state.url;m.host=state.host;m.title=state.title;m.message=state.message;m.received=state.received;m.total=state.total;m.imageUrl=state.imageUrl;m.imageW=state.imageW;m.imageH=state.imageH;m.sourceW=state.sourceW;m.sourceH=state.sourceH;m.links=int(state.links.size());m.revision=state.revision;unlock();return m;}
const uint16_t *linkPicture(int &w,int &h){lock();const uint16_t *p=state.phase==LinkPhase::Image?picture:nullptr;w=p?state.imageW:0;h=p?state.imageH:0;unlock();return p;}
std::string linkFind(const std::string &text){return linkFindUrl(text);}
std::string linkMemory(){
 char line[200];size_t page=0;lock();page=state.text.size();for(auto &l:state.links)page+=l.text.size()+l.url.size();size_t pic=pictureBytes;unlock();
 snprintf(line,sizeof(line),"heap_free=%u heap_min=%u heap_largest=%u psram_free=%u psram_largest=%u picture=%u page=%u",
  unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
  unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)),unsigned(pic),unsigned(page));
 return line;
}
}
