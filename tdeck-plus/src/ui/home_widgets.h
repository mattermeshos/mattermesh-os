#pragma once
#include <stdint.h>
// Settings > Home widgets > Beside clock: the box right of the Home clock (the live weather takes it only while none was picked).
// The index is saved (NVS "mmui"/"homewg"): append new widgets only, never reorder.
namespace mm {
struct HomeWidgetInfo {const char *name,*description,*needs;};
inline constexpr HomeWidgetInfo HomeWidgets[]={
 {"MatterMesh","Logo, contacts and a rotating tip",""},{"Mesh live","Contacts heard right now",""},{"Unread","Messages waiting, chats and DMs",""},
 {"Repeater","Last repeater heard and its signal",""},{"Day arc","Daylight left, sunrise to sunset","Needs a position: GPS, or where the weather was fetched"},
 {"Sunset","Next sunset or sunrise","Needs a position: GPS, or where the weather was fetched"},{"Moon","Moon phase and next full moon",""},
 {"Last weather","Last forecast, kept while offline","Shown once Wi-Fi got the weather"},{"Altitude","Height above sea level","Needs a GPS fix"},
 {"Coordinates","Your position, to read out","Needs a GPS fix (the last one is kept an hour)"},{"Calendar","Day, date and week number",""},{"Wi-Fi button","Tap it to turn Wi-Fi on for weather",""}};
constexpr int HomeWidgetCount=sizeof(HomeWidgets)/sizeof(HomeWidgets[0]);
enum HomeWidgetId {WidgetMatterMesh,WidgetMeshLive,WidgetUnread,WidgetRepeater,WidgetDayArc,WidgetSunset,WidgetMoon,WidgetLastWeather,WidgetAltitude,WidgetCoordinates,WidgetCalendar,WidgetWifi};
constexpr int HomeWidgetDefault=WidgetSunset;
// Settings > Home widgets > Main area: the big widget in the place of the two chat cards (NVS "mmui"/"homemn").
inline constexpr HomeWidgetInfo MainWidgets[]={
 {"Chats","The two latest conversations",""},{"Timeline","Latest messages from every channel",""},{"Mesh radar","Contacts around you, distance and direction","Needs your GPS position and theirs"},
 {"Mesh activity","Packets, nodes and best signal, last 6 h","Counted since the T-Deck started"},{"Position","Your position to read out, with UTM","Needs a GPS fix"},{"GPS sky","Satellites in view and the fix","Needs GPS on"},
 {"Elevation","Altitude profile and climb, last 12 h","Needs a GPS fix, one point every 10 min"},{"Weather now","Current conditions in full","Shown once Wi-Fi got the weather"},{"Hourly weather","The next hours","Shown once Wi-Fi got the weather"},
 {"Note","A note always in sight","Tap it to write"}};
constexpr int MainWidgetCount=sizeof(MainWidgets)/sizeof(MainWidgets[0]);
enum MainWidgetId {MainChats,MainTimeline,MainRadar,MainActivity,MainPosition,MainSky,MainElevation,MainWeatherNow,MainHourly,MainNote};
constexpr int MainWidgetDefault=MainChats;constexpr unsigned NoteMaxBytes=200;constexpr int MainPacketBars=24;// = mm_home::PacketBuckets
// The page a tap on the Main area widget opens: Note its editor (39), weather ones Weather (10), mesh ones Contacts (1),
// GPS ones GPS (3); Chats and Timeline handle their own rows (-1).
constexpr int mainWidgetSection(int id){return id==MainNote?39:id==MainWeatherNow||id==MainHourly?10:id==MainRadar||id==MainActivity?1:id==MainChats||id==MainTimeline?-1:3;}
constexpr bool skyWidget(int id){return id==WidgetDayArc||id==WidgetSunset;}// sunrise / sunset from a position and the clock
constexpr bool gpsWidget(int id){return skyWidget(id)||id==WidgetAltitude||id==WidgetCoordinates;}
constexpr bool gpsMain(int id){return id==MainRadar||id==MainPosition||id==MainSky||id==MainElevation;}
constexpr uint32_t HomePositionMaxAgeMs=3600000UL,HomeGpsCheckMs=5000;// Coordinates: last fix kept an hour; GPS widgets: repaint check
}
