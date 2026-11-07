#pragma once
#include <Arduino.h>
namespace device {
// Storm alerts (Plus): Open-Meteo hourly forecast for the next 24 h at the last known position, checked in the
// background every 30 min while Wi-Fi is connected and alerts are on (the STORMS tab checks on demand). Hours with a thunderstorm, strong gusts or heavy rain are
// grouped into events; a new event starting within 12 h raises one alert (banner, lock screen).
enum StormKind:uint8_t {StormThunder=1,StormGusts=2,StormRain=4};
constexpr int StormMaxEvents=8;
constexpr float STORM_GUST_KMH=60,STORM_RAIN_MM=7,STORM_CAPE=1500;
constexpr uint32_t STORM_REFRESH_MS=1800000UL,STORM_RETRY_MS=300000UL,STORM_ALERT_AHEAD_S=12*3600;
struct StormEvent {uint32_t start=0,end=0;uint8_t kinds=0;float gust=0,rain=0,cape=0;};// start/end: UTC epoch, end = start of the last hour
enum class StormPhase:uint8_t {Idle,NoWifi,WaitingClock,NoPosition,Fetching,Ready,Error};
struct StormState {StormPhase phase=StormPhase::Idle;String message;StormEvent events[StormMaxEvents];uint8_t count=0;uint32_t updated=0,revision=0;bool alerts=true;double latitude=0,longitude=0;};
void stormBegin();void stormLoop();void stormRefresh();StormState stormState();uint32_t stormRevision();
bool stormSetAlerts(bool on);
// One-shot: the text of a new alert (empty when none); the caller shows it once.
String stormTakeAlert();
const char *stormKindLabel(uint8_t kinds);
}
