// Weather app tabs (Plus), included by main.cpp: STORMS (storm alerts of the next 24 h, device::storm*, REFRESH,
// ALERTS on/off) and RADAR (rain radar over the map, RainViewer) next to the NOW forecast page (section 10).
#include "services/storm.h"
static int stormOffset=0;static uint32_t stormPainted=0;
static std::string stormClock(uint32_t epoch){time_t t=epoch;struct tm local;localtime_r(&t,&local);char text[8];strftime(text,sizeof(text),"%H:%M",&local);return text;}
static void stormCompose(mm::State &v){
 auto st=device::stormState();uint32_t now=time(nullptr);v.stormAlerts=st.alerts;v.stormBusy=st.phase==device::StormPhase::Fetching;v.stormReady=st.updated!=0;v.stormOffset=stormOffset;
 v.stormHead=st.updated?(st.count?std::to_string(st.count)+(st.count==1?" alert in 24 h":" alerts in 24 h"):std::string("No storms in 24 h")):std::string(st.message.c_str());
 v.stormSub=st.updated?"checked "+stormClock(st.updated):"";if(st.updated&&st.phase!=device::StormPhase::Ready&&st.phase!=device::StormPhase::Fetching)v.stormSub=st.message.c_str();
 time_t today=now;struct tm t0;localtime_r(&today,&t0);
 for(int i=0;i<st.count;i++){auto &e=st.events[i];mm::State::StormRow row;row.thunder=e.kinds&device::StormThunder;row.title=device::stormKindLabel(e.kinds);
  time_t start=e.start;struct tm ts;localtime_r(&start,&ts);std::string day=ts.tm_yday!=t0.tm_yday?"Tom. ":"";
  row.when=e.start<=now?"now-"+stormClock(e.end+3600):day+stormClock(e.start)+"-"+stormClock(e.end+3600);
  char d[64];int n=0;if(e.gust>=1)n+=snprintf(d+n,sizeof(d)-n,"Gusts %d km/h",int(lroundf(e.gust)));if(e.rain>=0.5f)n+=snprintf(d+n,sizeof(d)-n,"%sRain %d mm",n?" / ":"",int(lroundf(e.rain)));if(e.cape>=500)snprintf(d+n,sizeof(d)-n,"%sCAPE %d",n?" / ":"",int(e.cape));
  row.detail=d;v.stormRows.push_back(row);}
}
static void stormScroll(int delta){auto st=device::stormState();stormOffset=constrain(stormOffset+delta,0,max(0,int(st.count)-mm::StormRows));}
static void stormToggleAlerts(){auto st=device::stormState();notice=device::stormSetAlerts(!st.alerts)?(!st.alerts?"Storm alerts on":"Storm alerts off"):"Save failed";}
// New alert from the service: a 6 s banner when unlocked; on the lock screen it takes the bottom band for 30 min.
static void stormAlertTick(){
 String alert=device::stormTakeAlert();
 if(alert.length()){Serial.printf("STORM_ALERT %s\n",alert.c_str());stormBanner=alert;stormBannerUntil=millis()+6000;stormLockText=alert;stormLockUntil=millis()+1800000UL;
  if(deviceLocked){device::wakeDisplay(10000);lockScreenDirty=true;}else repaintNow();}
 if(stormBannerUntil&&int32_t(millis()-stormBannerUntil)>=0){stormBannerUntil=0;if(!deviceLocked)repaintNow();}
 if(stormLockUntil&&int32_t(millis()-stormLockUntil)>=0){stormLockUntil=0;stormLockText="";if(deviceLocked)lockScreenDirty=true;}
}
// ---- RADAR tab: the OpenStreetMap view (map_app.inc, LIGHT / NIGHT look) with RainViewer's latest frame blended over it ----
#include "services/radar.h"
static uint8_t *radarBase=nullptr,*radarView=nullptr;static int radarZoom=7;static uint32_t radarPainted=0,radarMapPainted=0;
static struct {double lat=0,lon=0;int z=-1,style=-1;uint32_t mapRev=0,radarRev=0;bool overlay=false;} radarComposed;
// Centre: the saved position, else the forecast's, else the north of Italy.
static void radarCenter(double &lat,double &lon){auto g=device::locationState();if(g.configured){lat=g.latitude;lon=g.longitude;return;}auto w=device::weatherState();if(w.data.valid){lat=w.data.latitude;lon=w.data.longitude;return;}lat=device::MapHomeLat;lon=device::MapHomeLon;}
static void radarRelease(){heap_caps_free(radarBase);heap_caps_free(radarView);radarBase=radarView=nullptr;radarComposed.z=-1;}
// Blend one 256x256 radar tile (RGB565 panel order + alpha) at (sx,sy) of the view; alpha scaled to 80 % so the map stays readable.
static void radarBlend(uint16_t *view,int w,int h,const uint16_t *rgb,const uint8_t *alpha,int sx,int sy){
 for(int ty=std::max(0,-sy);ty<256&&sy+ty<h;ty++){uint16_t *row=view+size_t(sy+ty)*w;const uint16_t *src=rgb+ty*256;const uint8_t *a=alpha+ty*256;
  for(int tx=std::max(0,-sx);tx<256&&sx+tx<w;tx++){uint32_t k=a[tx]*205/255;if(!k)continue;uint16_t d=row[sx+tx],s=src[tx];d=uint16_t(d<<8|d>>8);s=uint16_t(s<<8|s>>8);
   uint32_t r=(((s>>11)&31)*k+((d>>11)&31)*(255-k))/255,g=(((s>>5)&63)*k+((d>>5)&63)*(255-k))/255,b=((s&31)*k+(d&31)*(255-k))/255;uint16_t o=uint16_t(r<<11|g<<5|b);row[sx+tx]=uint16_t(o<<8|o>>8);}}}
static void radarCompose(mm::State &v){
 auto st=device::radarState();v.radarZoom=radarZoom;
 if(st.frameTime){time_t t=st.frameTime;struct tm local;localtime_r(&t,&local);char text[24];strftime(text,sizeof(text),"Radar %H:%M",&local);v.radarTime=text;}
 v.radarMessage=st.message.c_str();
 if(!radarBase)radarBase=mapAllocView();if(!radarView)radarView=mapAllocView();if(!radarBase||!radarView){radarRelease();v.radarMessage="Not enough memory for the map";return;}
 double lat,lon;radarCenter(lat,lon);const int z=radarZoom,w=mm::MapArea.w,h=mm::MapArea.h;
 uint32_t mapRev=device::mapTilesRevision(),radarRev=device::radarRevision();
 bool baseOk=radarComposed.z==z&&radarComposed.lat==lat&&radarComposed.lon==lon&&radarComposed.mapRev==mapRev&&radarComposed.style==mapStyleNow();
 if(!baseOk){int missing=mapComposeTiles(radarBase,lat,lon,z);radarComposed.lat=lat;radarComposed.lon=lon;radarComposed.z=missing?-1:z;radarComposed.mapRev=mapRev;radarComposed.style=mapStyleNow();radarComposed.overlay=false;}
 if(!baseOk||!radarComposed.overlay||radarComposed.radarRev!=radarRev){
  memcpy(radarView,radarBase,MapViewBytes);auto center=device::mapProject(lat,lon,z);device::MapTileRef refs[16];int count=device::mapVisibleTiles(center,z,w,h,0,refs,16),have=0;
  for(int i=0;i<count;i++){const uint16_t *rgb;const uint8_t *alpha;if(device::radarTile(z,refs[i].x,refs[i].y,rgb,alpha)){radarBlend(reinterpret_cast<uint16_t*>(radarView),w,h,rgb,alpha,refs[i].screenX,refs[i].screenY);have++;}}
  radarComposed.overlay=have==count;radarComposed.radarRev=radarRev;if(have<count&&v.radarMessage.empty())v.radarMessage="Loading radar "+std::to_string(have)+"/"+std::to_string(count);}
 v.mapPixels=reinterpret_cast<const uint16_t*>(radarView);
 double x,y;mapScreen(lat,lon,lat,lon,z,x,y);v.mapSelfX=lround(x);v.mapSelfY=lround(y);mapScale(lat,z,v);
}
static void radarZoomBy(int d){radarZoom=constrain(radarZoom+d,device::RadarMinZoom,device::RadarMaxZoom);radarComposed.z=-1;}
// Weather tabs (Plus): 0 NOW (forecast), 1 STORMS, 2 RADAR. Left / right switch tabs; each tab keeps its own keys.
int weatherTab=0;
static void weatherSelectTab(int tab){weatherTab=constrain(tab,0,2);notice="";stormOffset=0;
 if(weatherTab==1){auto st=device::stormState();if(!st.updated||uint32_t(time(nullptr)-st.updated)>600)device::stormRefresh();stormPainted=0;}}
void weatherTabsCompose(mm::State &v){v.weatherTab=weatherTab;stormCompose(v);if(weatherTab==2)radarCompose(v);}
bool weatherTabKey(char c){
 if(c=='h'||c=='l'){weatherSelectTab(weatherTab+(c=='l'?1:-1));render();return true;}
 if(weatherTab==1){if(c=='j'||c=='s')stormScroll(1);else if(c=='k'||c=='w')stormScroll(-1);else if(c=='\n'||c=='r'){device::stormRefresh();notice="";}else if(c=='t')stormToggleAlerts();else return false;render();return true;}
 if(weatherTab==2){if(c=='k'||c=='w'||c=='+')radarZoomBy(1);else if(c=='j'||c=='s'||c=='-')radarZoomBy(-1);else if(c=='\n'||c=='r')device::radarRefresh();else return false;render();return true;}
 return false;}
void weatherAppsTick(){
 stormAlertTick();
 bool radarOpen=!deviceLocked&&screen==Screen::Section&&section==10&&weatherTab==2;device::radarSetOpen(radarOpen);if(!radarOpen&&radarBase)radarRelease();
 if(!deviceLocked&&screen==Screen::Section&&section==10){uint32_t r=device::stormRevision();if(r!=stormPainted){stormPainted=r;render();}}
 if(radarOpen){static uint32_t at=0;uint32_t r=device::radarRevision(),m=device::mapTilesRevision();if((r!=radarPainted||m!=radarMapPainted)&&millis()-at>=500){at=millis();radarPainted=r;radarMapPainted=m;render();}}
}
// M9: OK on the element chosen with the D-pad presses it like a finger did on the T-Deck (main.cpp focusPress).
bool weatherTabTap(int x,int y){
 for(int i=0;i<3;i++)if(mm::weatherTabRect(i).contains(x,y)){weatherSelectTab(i);render();return true;}
 if(weatherTab==1){if(mm::StormRefresh.contains(x,y)){device::stormRefresh();notice="";render();}else if(mm::StormAlertsButton.contains(x,y)){stormToggleAlerts();render();}return true;}
 if(weatherTab==2){if(mm::RadarZoomIn.contains(x,y))radarZoomBy(1);else if(mm::RadarZoomOut.contains(x,y))radarZoomBy(-1);else return true;render();return true;}
 return false;}
