#include "clock_limits.h"
#include "storm.h"
#include "weather.h"
#include "https.h"
#include "connectivity.h"
#include "location.h"
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <time.h>
#include <math.h>
namespace device {
static StormState state;static SemaphoreHandle_t mutex=nullptr;static Preferences preferences;
static bool busy=false,pending=true,lastOk=false;static uint32_t lastAttempt=0;static String alert;
static void publish(){state.revision++;}
static void setPhase(StormPhase phase,const String &message){xSemaphoreTake(mutex,portMAX_DELAY);if(state.phase!=phase||state.message!=message){state.phase=phase;state.message=message;publish();}xSemaphoreGive(mutex);}
const char *stormKindLabel(uint8_t kinds){if(kinds&StormThunder)return "Thunderstorm";if(kinds&StormGusts)return kinds&StormRain?"Wind and rain":"Strong gusts";if(kinds&StormRain)return "Heavy rain";return "Weather";}
static String clockText(uint32_t epoch){time_t t=epoch;struct tm local;localtime_r(&t,&local);char text[8];strftime(text,sizeof(text),"%H:%M",&local);return text;}
// Keys of the last announced events (NVS "ann", newest first): an event already announced does not hide a later one.
constexpr int AnnouncedMax=4;
static bool announced(uint32_t key){uint32_t keys[AnnouncedMax]={};if(preferences.isKey("ann"))preferences.getBytes("ann",keys,sizeof(keys));for(auto k:keys)if(k==key)return true;return uint32_t(preferences.getUInt("last",0))==key;}
static void remember(uint32_t key){uint32_t keys[AnnouncedMax]={};if(preferences.isKey("ann"))preferences.getBytes("ann",keys,sizeof(keys));memmove(keys+1,keys,sizeof(keys)-sizeof(keys[0]));keys[0]=key;preferences.putBytes("ann",keys,sizeof(keys));}
// The first event that starts within STORM_ALERT_AHEAD_S and was not announced yet becomes the alert.
static void checkAlert(){
 if(!state.alerts)return;uint32_t now=time(nullptr);
 for(int i=0;i<state.count;i++){auto &e=state.events[i];if(e.end+3600<now||e.start>now+STORM_ALERT_AHEAD_S)continue;
  uint32_t key=e.start^(uint32_t(e.kinds)<<24);if(announced(key))continue;remember(key);
  String text=String(stormKindLabel(e.kinds))+(e.start<=now?" now":" from "+clockText(e.start));
  if(e.gust>=STORM_GUST_KMH)text+=" / gusts "+String(int(lroundf(e.gust)))+" km/h";else if(e.rain>=1)text+=" / "+String(int(lroundf(e.rain)))+" mm";
  alert=text;return;}
}
struct Request {double latitude,longitude;};
static void fetch(const Request &r){
 StormState result;String error;WiFiClientSecure client;HTTPClient http;secureClient(client,http);
 String url="https://api.open-meteo.com/v1/forecast?latitude="+String(r.latitude,4)+"&longitude="+String(r.longitude,4)+"&hourly=weather_code,wind_gusts_10m,precipitation,cape&forecast_hours=24&timeformat=unixtime&timezone=GMT";
 if(!http.begin(client,url))error="Connection failed";
 else{int status=http.GET();
  if(status==200){DynamicJsonDocument json(12288);if(deserializeJson(json,http.getString()))error="Invalid response";
   else{auto h=json["hourly"];auto times=h["time"].as<JsonArray>();int n=times.size();
    if(!n)error="Forecast missing";
    // Consecutive flagged hours form one event (a one-hour gap still splits them).
    StormEvent open;bool inEvent=false;
    for(int i=0;i<=n;i++){uint8_t kinds=0;float gust=0,rain=0,cape=0;uint32_t at=0;
     if(i<n){at=times[i]|0u;int code=h["weather_code"][i]|0;gust=h["wind_gusts_10m"][i]|0.0f;rain=h["precipitation"][i]|0.0f;cape=h["cape"][i]|0.0f;
      if(code>=95||(cape>=STORM_CAPE&&rain>=1))kinds|=StormThunder;if(gust>=STORM_GUST_KMH)kinds|=StormGusts;if(rain>=STORM_RAIN_MM)kinds|=StormRain;}
     if(kinds){if(!inEvent){open=StormEvent();open.start=at;inEvent=true;}open.end=at;open.kinds|=kinds;open.gust=std::max(open.gust,gust);open.rain+=rain;open.cape=std::max(open.cape,cape);}
     else if(inEvent){if(result.count<StormMaxEvents)result.events[result.count++]=open;inEvent=false;}
    }
   }}
  else error="HTTP "+String(status);
  http.end();}
 xSemaphoreTake(mutex,portMAX_DELAY);
 if(error.isEmpty()){for(int i=0;i<StormMaxEvents;i++)state.events[i]=result.events[i];state.count=result.count;state.updated=time(nullptr);state.latitude=r.latitude;state.longitude=r.longitude;state.phase=StormPhase::Ready;state.message=state.count?String(state.count)+(state.count==1?" alert":" alerts")+" in 24 h":"No storms in 24 h";lastOk=true;checkAlert();}
 else{state.phase=StormPhase::Error;state.message=error;lastOk=false;}
 publish();xSemaphoreGive(mutex);
}
static void worker(void *argument){networkJobBegin();auto r=static_cast<Request*>(argument);fetch(*r);delete r;xSemaphoreTake(mutex,portMAX_DELAY);busy=false;publish();xSemaphoreGive(mutex);networkJobEnd();vTaskDelete(nullptr);}
void stormBegin(){mutex=xSemaphoreCreateMutex();preferences.begin("mmstorm",false);state.alerts=preferences.getBool("alerts",true);pending=state.alerts;}// alerts off: no background check until the STORMS tab asks
void stormRefresh(){pending=true;}
bool stormSetAlerts(bool on){if(!preferences.putBool("alerts",on))return false;// applied only once saved
 xSemaphoreTake(mutex,portMAX_DELAY);state.alerts=on;if(on)pending=true;publish();xSemaphoreGive(mutex);return true;}
void stormLoop(){
 static uint32_t lastLoop=0;if(!mutex||millis()-lastLoop<2000)return;lastLoop=millis();
 xSemaphoreTake(mutex,portMAX_DELAY);bool running=busy;xSemaphoreGive(mutex);if(running)return;
 auto connection=connectivityStatus();
 if(!connection.connected){setPhase(StormPhase::NoWifi,"Waiting for Wi-Fi");return;}
 if(time(nullptr)<MM_EPOCH_MIN){setPhase(StormPhase::WaitingClock,"Waiting for the clock");return;}
 if(state.alerts&&lastAttempt&&uint32_t(millis()-lastAttempt)>=(lastOk?STORM_REFRESH_MS:STORM_RETRY_MS))pending=true;// periodic only with alerts on
 if(!pending)return;
 // Position: the saved GPS position, else the last weather forecast position.
 double lat=NAN,lon=NAN;auto location=locationState();if(location.configured){lat=location.latitude;lon=location.longitude;}
 else{auto w=weatherState();if(w.data.valid){lat=w.data.latitude;lon=w.data.longitude;}}
 if(!isfinite(lat)||!isfinite(lon)){setPhase(StormPhase::NoPosition,"No position yet / open Weather once");return;}
 pending=false;lastAttempt=millis();
 auto request=new Request{lat,lon};
 xSemaphoreTake(mutex,portMAX_DELAY);busy=true;state.phase=StormPhase::Fetching;state.message="Checking the forecast...";publish();xSemaphoreGive(mutex);
 if(xTaskCreate(worker,"matter-storm",8192,request,1,nullptr)!=pdPASS){delete request;xSemaphoreTake(mutex,portMAX_DELAY);busy=false;state.phase=StormPhase::Error;state.message="Worker unavailable";publish();xSemaphoreGive(mutex);}
}
StormState stormState(){if(!mutex)return state;xSemaphoreTake(mutex,portMAX_DELAY);auto copy=state;xSemaphoreGive(mutex);return copy;}
uint32_t stormRevision(){if(!mutex)return 0;xSemaphoreTake(mutex,portMAX_DELAY);uint32_t r=state.revision;xSemaphoreGive(mutex);return r;}
String stormTakeAlert(){if(!mutex)return "";xSemaphoreTake(mutex,portMAX_DELAY);String a=alert;alert="";xSemaphoreGive(mutex);return a;}
}
