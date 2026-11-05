#pragma once
#include <Arduino.h>
namespace device {
// Where the forecast position came from. GPS is read only when the user has it on; the GPS power state is never changed here.
enum WeatherSource:uint8_t {WeatherSourceNone,WeatherSourceGps,WeatherSourceWifi,WeatherSourceIp,WeatherSourceSaved};
// What the open Weather app is waiting for; Closed means no work, no timers, no network.
enum class WeatherPhase:uint8_t {Closed,WifiOff,Connecting,NoNetwork,WaitingClock,Locating,Fetching,Ready,Error};
constexpr int WeatherHours=4;
struct WeatherData {uint32_t magic=0x4d4d5732,updated=0;float temperature=0,apparent=0,humidity=0,pressure=0,wind=0,precipitation=0,high=0,low=0;int code=0;double latitude=0,longitude=0;char stamp[28]={};bool valid=false;uint8_t source=WeatherSourceNone;
 int16_t hourCode[WeatherHours]={};float hourTemp[WeatherHours]={};char hourTime[WeatherHours][6]={};uint8_t hours=0;};
struct WeatherState {WeatherData data;bool busy=false,cached=false,open=false;WeatherPhase phase=WeatherPhase::Closed;String message,place;uint32_t revision=0,nextRefresh=0;};
constexpr uint32_t WEATHER_REFRESH_MS=900000UL,WEATHER_RETRY_MS=60000UL,WEATHER_REOPEN_FRESH_MS=120000UL;
uint32_t weatherRevision();
void weatherBegin();void weatherLoop();void weatherSetOpen(bool open);void weatherRefresh();void weatherEnableWifi();
WeatherState weatherState();const char *weatherDescription(int code);const char *weatherSourceLabel(uint8_t source);
}
