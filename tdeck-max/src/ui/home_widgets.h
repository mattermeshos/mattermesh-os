#pragma once
#include <stdint.h>
// Settings > Home widgets > Beside clock: the box right of the Home clock. Classic keeps the big clock with the date
// and the contact count beside it (the Home before the widgets).
// The index is saved (NVS "mmui"/"homewg"): append new widgets only, never reorder.
namespace mm {
struct HomeWidgetInfo {const char *name,*description,*needs;};
inline constexpr HomeWidgetInfo HomeWidgets[]={
 {"MatterMesh","Logo, contacts and a rotating tip",""},{"Mesh live","Nodes heard in the last 15 minutes",""},{"Unread","Messages waiting, chats and DMs",""},
 {"Repeater","Last repeater heard and its signal",""},{"Day arc","Daylight left, sunrise to sunset","Needs a position: GPS, or where the weather was fetched"},
 {"Sunset","Next sunset or sunrise","Needs a position: GPS, or where the weather was fetched"},{"Moon","Moon phase and next full moon",""},
 {"Weather","Last forecast from the Weather app","Shown once Wi-Fi got the weather"},{"Altitude","Height above sea level","Needs a GPS fix"},
 {"Coordinates","Your position, to read out","Needs a GPS fix (the last one is kept an hour)"},{"Calendar","Day, date and week number",""},
 {"Classic","Big clock, the date and your contacts",""}};
constexpr int HomeWidgetCount=sizeof(HomeWidgets)/sizeof(HomeWidgets[0]);
enum HomeWidgetId {WidgetMatterMesh,WidgetMeshLive,WidgetUnread,WidgetRepeater,WidgetDayArc,WidgetSunset,WidgetMoon,WidgetWeather,WidgetAltitude,WidgetCoordinates,WidgetCalendar,WidgetClassic};
constexpr int HomeWidgetDefault=WidgetClassic;// the Home before the widgets until one is picked
// Settings > Home widgets > Main area: the big widget in the place of the chat cards (NVS "mmui"/"homemn"). The
// e-ink set: what changes slowly (no live satellite sky); repaints follow the minute clock or a coarse change.
inline constexpr HomeWidgetInfo MainWidgets[]={
 {"Chats","The latest conversations",""},{"Timeline","Latest messages from every channel",""},{"Mesh radar","Contacts around you, distance and direction","Needs your GPS position and theirs"},
 {"Mesh activity","Packets, nodes and best signal, last 6 h","Counted since the T-Deck started"},{"Position","Your position to read out, with UTM","Needs a GPS fix"},
 {"Elevation","Altitude profile and climb, last 12 h","Needs a GPS fix, one point every 10 min"},{"Weather now","Current conditions in full","Shown once Wi-Fi got the weather"},
 {"Hourly weather","The next hours","Shown once Wi-Fi got the weather"},{"Note","A note always in sight","Tap it to write"}};
constexpr int MainWidgetCount=sizeof(MainWidgets)/sizeof(MainWidgets[0]);
enum MainWidgetId {MainChats,MainTimeline,MainRadar,MainActivity,MainPosition,MainElevation,MainWeatherNow,MainHourly,MainNote};
constexpr int MainWidgetDefault=MainChats;constexpr unsigned NoteMaxBytes=200;
constexpr bool gpsMain(int id){return id==MainRadar||id==MainPosition||id==MainElevation;}
constexpr bool weatherMain(int id){return id==MainWeatherNow||id==MainHourly;}
constexpr bool skyWidget(int id){return id==WidgetDayArc||id==WidgetSunset;}// sunrise / sunset from a position and the clock
constexpr bool gpsWidget(int id){return skyWidget(id)||id==WidgetAltitude||id==WidgetCoordinates;}
constexpr uint32_t HomePositionMaxAgeMs=3600000UL,HomeGpsCheckMs=5000,HomeLiveWindowMs=900000UL;// Mesh live: nodes heard in the last 15 min// Coordinates: last fix kept an hour; GPS widgets: repaint check
}
