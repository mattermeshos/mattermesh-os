#include "clock_limits.h"
#include "../board/device_model.h"
#include "weather.h"
#include "weather_ca.h"
#include "connectivity.h"
#include "location.h"
#include "ui/build_info.h"
#include "services/hex_util.h"
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <time.h>
#include <math.h>
#include <atomic>
#include <vector>
namespace device {
// The service only runs while the Weather app is on screen: opening it fetches once, then every 15 min.
// Closing it bumps the generation so an in-flight worker drops its result and ends; nothing runs while closed.
static std::atomic<uint32_t> revision{0},generation{0};
static Preferences preferences;static WeatherState state;static SemaphoreHandle_t mutex=nullptr;static bool cachePending=false;
static bool open=false,pending=false,locating=false,wifiByWeather=false,lastOk=false;static uint32_t lastAttempt=0,lastSuccess=0,scanStarted=0,scanBase=0;static uint8_t lastAttemptSource=WeatherSourceNone;
struct PlaceCache {uint32_t magic=0x4d4d5031;double latitude=0,longitude=0;char name[96]={};};
static PlaceCache placeCache;static bool placePending=false;static double placeTriedLat=1000,placeTriedLon=1000;static uint32_t placeAttempt=0;static bool placeAttempted=false;
struct Request {uint32_t generation;bool gps;double latitude,longitude;std::vector<AccessPoint> accessPoints;bool saved;double savedLatitude,savedLongitude;String placeApi;};
static void publish(){state.revision++;revision.store(state.revision,std::memory_order_relaxed);}
static void setPhase(WeatherPhase phase,const String &message){xSemaphoreTake(mutex,portMAX_DELAY);if(state.phase!=phase||state.message!=message){state.phase=phase;state.message=message;publish();}xSemaphoreGive(mutex);}
static bool placeMatches(double latitude,double longitude){return placeCache.name[0]&&fabs(latitude-placeCache.latitude)<0.02&&fabs(longitude-placeCache.longitude)<0.02;}
static void secure(WiFiClientSecure &client,HTTPClient &http){client.setCACert(WEATHER_ROOT_CA);client.setHandshakeTimeout(6);client.setTimeout(6);http.setConnectTimeout(5000);http.setTimeout(6000);}
static String resolvePlace(const String &endpoint,double latitude,double longitude){
 WiFiClientSecure client;HTTPClient http;secure(client,http);String name;
 String url=endpoint+"/reverse?lat="+String(latitude,5)+"&lon="+String(longitude,5)+"&lang=en&limit=1";
 if(http.begin(client,url)){http.setUserAgent("MatterMeshOS/" MM_VERSION " " MM_DEVICE_TOKEN " weather-locality");if(http.GET()==200){DynamicJsonDocument json(2048);if(!deserializeJson(json,http.getString())){auto place=json["features"][0]["properties"];name=place["city"]|"";if(name.isEmpty()&&String(place["osm_key"]|"")=="place")name=place["name"]|"";if(name.isEmpty())name=place["county"]|"";}}http.end();}
 name.replace("\n"," ");name.replace("\r"," ");name.trim();return name;
}
// Wi-Fi positioning from nearby BSSIDs (BeaconDB, no key); BeaconDB itself falls back to IP when no AP is known.
static bool geolocate(const std::vector<AccessPoint> &aps,double &latitude,double &longitude,uint8_t &source){
 DynamicJsonDocument body(1536);body["considerIp"]=true;auto list=body.createNestedArray("wifiAccessPoints");
 for(auto &ap:aps){auto row=list.createNestedObject();row["macAddress"]=String(mm::hexString(ap.bssid,6,false,':').c_str());row["signalStrength"]=ap.rssi;}
 String payload;serializeJson(body,payload);WiFiClientSecure client;HTTPClient http;secure(client,http);bool ok=false;
 if(http.begin(client,"https://api.beacondb.net/v1/geolocate")){http.setUserAgent("MatterMeshOS/" MM_VERSION " " MM_DEVICE_TOKEN " weather-position");http.addHeader("Content-Type","application/json");
  if(http.POST(payload)==200){DynamicJsonDocument json(1024);if(!deserializeJson(json,http.getString())){double lat=json["location"]["lat"]|NAN,lon=json["location"]["lng"]|NAN;if(isfinite(lat)&&isfinite(lon)&&fabs(lat)<=90&&fabs(lon)<=180){latitude=lat;longitude=lon;source=json["fallback"].isNull()?WeatherSourceWifi:WeatherSourceIp;ok=true;}}}http.end();}
 return ok;
}
const char *weatherDescription(int code){if(code==0)return "Clear sky";if(code==1||code==2)return "Partly cloudy";if(code==3)return "Overcast";if(code==45||code==48)return "Fog";if(code>=51&&code<=57)return "Drizzle";if((code>=61&&code<=67)||(code>=80&&code<=82))return "Rain";if((code>=71&&code<=77)||code==85||code==86)return "Snow";if(code>=95&&code<=99)return "Thunderstorm";return "Weather unavailable";}
const char *weatherSourceLabel(uint8_t source){switch(source){case WeatherSourceGps:return "GPS";case WeatherSourceWifi:return "Wi-Fi";case WeatherSourceIp:return "IP approx.";case WeatherSourceSaved:return "Last fix";default:return "";}}
static bool current(uint32_t id){return id==generation.load(std::memory_order_relaxed);}
static void fetchWeather(Request &request){
 WeatherData result;String error;double latitude=request.latitude,longitude=request.longitude;uint8_t source=request.gps?WeatherSourceGps:WeatherSourceNone;
 // Order: GPS fix, Wi-Fi positioning, the last good GPS position; the provider's IP location (often another region) only as a last resort.
 if(!request.gps){bool located=geolocate(request.accessPoints,latitude,longitude,source);
  if((!located||source==WeatherSourceIp)&&request.saved){latitude=request.savedLatitude;longitude=request.savedLongitude;source=WeatherSourceSaved;}
  else if(!located)error="Position unavailable";}
 if(!current(request.generation))return;
 if(error.isEmpty()){
  WiFiClientSecure client;HTTPClient http;secure(client,http);
  String url="https://api.open-meteo.com/v1/forecast?latitude="+String(latitude,5)+"&longitude="+String(longitude,5)+"&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,surface_pressure,wind_speed_10m&hourly=temperature_2m,weather_code&forecast_hours=13&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1";
  if(!http.begin(client,url))error="Weather connection failed";else{int status=http.GET();if(status==200){DynamicJsonDocument json(8192);if(deserializeJson(json,http.getString()))error="Invalid weather response";else{auto now=json["current"];auto daily=json["daily"];auto hourly=json["hourly"];
   if(now["temperature_2m"].isNull()||now["weather_code"].isNull()||daily["temperature_2m_max"][0].isNull()||daily["temperature_2m_min"][0].isNull())error="Weather fields missing";
   else{result.temperature=now["temperature_2m"];result.apparent=now["apparent_temperature"]|result.temperature;result.humidity=now["relative_humidity_2m"];result.pressure=now["surface_pressure"];result.wind=now["wind_speed_10m"];result.precipitation=now["precipitation"]|0.0f;result.code=now["weather_code"];result.high=daily["temperature_2m_max"][0];result.low=daily["temperature_2m_min"][0];
    // Next hours every 3 h; hourly starts at the current hour.
    for(int i=0;i<WeatherHours;i++){int at=3*(i+1);if(hourly["temperature_2m"][at].isNull()||hourly["weather_code"][at].isNull())break;result.hourTemp[i]=hourly["temperature_2m"][at];result.hourCode[i]=hourly["weather_code"][at];String stamp=hourly["time"][at]|"";strlcpy(result.hourTime[i],stamp.length()>=16?stamp.substring(11,16).c_str():"--:--",sizeof(result.hourTime[i]));result.hours=i+1;}
    result.latitude=latitude;result.longitude=longitude;result.source=source;strlcpy(result.stamp,now["time"]|"",sizeof(result.stamp));result.updated=time(nullptr);
    result.valid=isfinite(result.temperature)&&result.temperature>=-100&&result.temperature<=70&&isfinite(result.wind)&&result.wind>=0&&isfinite(result.high)&&isfinite(result.low);if(!result.valid)error="Weather values invalid";}}}
   else error="Weather retry / HTTP "+String(status);http.end();}
 }
 if(!current(request.generation))return;
 String place;bool lookup=false;
 if(result.valid){xSemaphoreTake(mutex,portMAX_DELAY);if(placeMatches(latitude,longitude))place=placeCache.name;else if(!placeAttempted||uint32_t(millis()-placeAttempt)>=WEATHER_REFRESH_MS||fabs(latitude-placeTriedLat)>=0.02||fabs(longitude-placeTriedLon)>=0.02){lookup=true;placeAttempted=true;placeAttempt=millis();placeTriedLat=latitude;placeTriedLon=longitude;}/* a new position is named at once */xSemaphoreGive(mutex);if(lookup)place=resolvePlace(request.placeApi,latitude,longitude);}
 xSemaphoreTake(mutex,portMAX_DELAY);
 if(current(request.generation)){
  if(result.valid){if(lookup&&!place.isEmpty()){placeCache.latitude=latitude;placeCache.longitude=longitude;strlcpy(placeCache.name,place.c_str(),sizeof(placeCache.name));placePending=true;}state.place=place;state.data=result;state.cached=false;state.phase=WeatherPhase::Ready;state.message="Updated";cachePending=true;lastOk=true;lastSuccess=millis();}
  else{state.cached=state.data.valid;state.phase=WeatherPhase::Error;state.message=error;lastOk=false;}
  publish();
 }
 xSemaphoreGive(mutex);
}
// Return from fetchWeather before deleting the task so TLS buffers and sockets are released.
static void worker(void *argument){auto request=static_cast<Request*>(argument);fetchWeather(*request);delete request;xSemaphoreTake(mutex,portMAX_DELAY);state.busy=false;publish();xSemaphoreGive(mutex);vTaskDelete(nullptr);}
static void startWorker(bool gps){
 auto location=locationState();auto request=new Request{generation.load(),gps,location.latitude,location.longitude,{},location.configured,location.latitude,location.longitude,preferences.getString("place_api","https://photon.komoot.io")};
 if(!gps)request->accessPoints=wifiAccessPoints();
 xSemaphoreTake(mutex,portMAX_DELAY);state.busy=true;state.phase=WeatherPhase::Fetching;state.message="Updating...";publish();xSemaphoreGive(mutex);
 if(xTaskCreate(worker,"matter-weather",8192,request,1,nullptr)!=pdPASS){delete request;xSemaphoreTake(mutex,portMAX_DELAY);state.busy=false;state.phase=WeatherPhase::Error;state.message="Weather worker unavailable";publish();xSemaphoreGive(mutex);}
}
void weatherBegin(){preferences.begin("mmweather",false);mutex=xSemaphoreCreateMutex();WeatherData cached;if(preferences.getBytesLength("cache")==sizeof(cached)&&preferences.getBytes("cache",&cached,sizeof(cached))==sizeof(cached)&&cached.magic==WeatherData().magic&&cached.valid){state.data=cached;state.cached=true;state.message="Cached";}if(preferences.getBytesLength("place")==sizeof(placeCache)){PlaceCache saved;if(preferences.getBytes("place",&saved,sizeof(saved))==sizeof(saved)&&saved.magic==placeCache.magic){saved.name[sizeof(saved.name)-1]=0;placeCache=saved;if(state.data.valid&&placeMatches(state.data.latitude,state.data.longitude))state.place=placeCache.name;}}}
void weatherSetOpen(bool on){
 if(on==open)return;open=on;locating=false;
 if(on){pending=!state.data.valid||!lastSuccess||uint32_t(millis()-lastSuccess)>=WEATHER_REOPEN_FRESH_MS;if(!pending)lastAttempt=lastSuccess;xSemaphoreTake(mutex,portMAX_DELAY);state.open=true;publish();xSemaphoreGive(mutex);return;}
 generation++;pending=false;
 if(wifiByWeather&&connectivityStatus().wifi)wifiEnabled(false,false);wifiByWeather=false;
 xSemaphoreTake(mutex,portMAX_DELAY);state.open=false;state.phase=WeatherPhase::Closed;state.cached=state.data.valid;state.message=state.data.valid?"Cached":"";publish();xSemaphoreGive(mutex);
}
void weatherRefresh(){if(open)pending=true;}
void weatherEnableWifi(){if(!open||connectivityStatus().wifi)return;wifiEnabled(true,false);wifiByWeather=true;pending=true;}
void weatherLoop(){
 static uint32_t lastLoop=0;if(!open||millis()-lastLoop<1000)return;lastLoop=millis();
 PlaceCache place;bool savePlace=false;WeatherData cache;bool save=false;bool busy;
 xSemaphoreTake(mutex,portMAX_DELAY);busy=state.busy;if(cachePending){cachePending=false;cache=state.data;save=true;}if(placePending){placePending=false;place=placeCache;savePlace=true;}xSemaphoreGive(mutex);
 if(savePlace)preferences.putBytes("place",&place,sizeof(place));if(save)preferences.putBytes("cache",&cache,sizeof(cache));
 if(busy)return;
 auto connection=connectivityStatus();
 if(!connection.wifi){locating=false;setPhase(WeatherPhase::WifiOff,"Wi-Fi is off");return;}
 if(!connection.connected){locating=false;setPhase(connection.connecting||connection.remembered?WeatherPhase::Connecting:WeatherPhase::NoNetwork,connection.connecting||connection.remembered?"Connecting to Wi-Fi...":"No saved Wi-Fi network");return;}
 if(time(nullptr)<MM_EPOCH_MIN){setPhase(WeatherPhase::WaitingClock,"Waiting for clock sync");return;}
 // GPS is used only if the user already has it on and it has a fix; its state is never touched.
 auto location=locationState();bool gps=location.enabled&&location.fix;
 if(gps&&lastAttemptSource!=WeatherSourceGps&&uint32_t(millis()-lastAttempt)>=WEATHER_RETRY_MS)pending=true;
 if(lastAttempt&&uint32_t(millis()-lastAttempt)>=(lastOk?WEATHER_REFRESH_MS:WEATHER_RETRY_MS))pending=true;
 if(locating){if(wifiScanCount()!=scanBase||uint32_t(millis()-scanStarted)>=8000){locating=false;startWorker(false);}return;}
 if(!pending){xSemaphoreTake(mutex,portMAX_DELAY);if(state.phase!=WeatherPhase::Ready&&state.phase!=WeatherPhase::Error&&state.data.valid){state.phase=WeatherPhase::Ready;publish();}xSemaphoreGive(mutex);return;}
 pending=false;lastAttempt=millis();lastAttemptSource=gps?WeatherSourceGps:WeatherSourceWifi;
 if(gps){startWorker(true);return;}
 scanBase=wifiScanCount();scanStarted=millis();locating=true;scan();setPhase(WeatherPhase::Locating,"Finding position...");
}
uint32_t weatherRevision(){return revision.load(std::memory_order_relaxed);}
WeatherState weatherState(){if(!mutex)return state;xSemaphoreTake(mutex,portMAX_DELAY);auto copy=state;xSemaphoreGive(mutex);if(open&&lastAttempt){uint32_t due=lastAttempt+(lastOk?WEATHER_REFRESH_MS:WEATHER_RETRY_MS);copy.nextRefresh=int32_t(due-millis())>0?(due-millis())/1000:0;}return copy;}
}
