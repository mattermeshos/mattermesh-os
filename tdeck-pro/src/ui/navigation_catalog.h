#pragma once
#include "components.h"
namespace mm {
struct NavItem {const char *label;Icon icon;int section;};
struct AppItem {const char *label;PackIcon icon;int section;};
inline constexpr AppItem HomeTabs[]={{"Chat",PackIcon::Chat,0},{"Contacts",PackIcon::Contacts,1},{"Settings",PackIcon::Settings,19},{"App",PackIcon::Apps,-1}};
static_assert(sizeof(HomeTabs)/sizeof(HomeTabs[0])==MenuHomeTabCount,"Home catalog/layout must agree");
// Donate / Unlock QRs: donations (any amount) and the Extra apps product (its page leads to mattermeshos.com/unlock).
constexpr const char *DonateUrl="https://ko-fi.com/mattermeshos";
constexpr const char *UnlockUrl="https://ko-fi.com/s/1f0b36fd8b";
constexpr bool FreeBuild=true;
// Free image: the Extra apps are not compiled in, the App screen lists the OS apps.
inline constexpr AppItem Applications[]={{"Maps",PackIcon::Map,9},{"Weather",PackIcon::Weather,10},{"Files",PackIcon::Files,7},{"CLI",PackIcon::Keyboard,30}};
constexpr int ApplicationCount=sizeof(Applications)/sizeof(Applications[0]);
inline constexpr NavItem SettingsItems[]={{"Wi-Fi",Icon::Wifi,4},{"Bluetooth",Icon::Bluetooth,14},{"LoRa",Icon::Radio,2},{"Antenna",Icon::Signal,23},{"GPS",Icon::GPS,3},{"Lights / vibration",Icon::Vibration,6},{"Wallpaper",Icon::Mountain,22},{"Keyboard languages",Icon::Keyboard,KeyboardSection},{"Accent key",Icon::Keyboard,AccentKeySection},{"Sound / dictation",Icon::Messages,21},{"Power / timeouts",Icon::Power,20},{"Battery",Icon::Battery,5},{"Date / time",Icon::Clock,16},{"Storage",Icon::Files,24},{"Diagnostics",Icon::Terminal,11},{"OTA update",Icon::Tools,17},{"Donate / Unlock",Icon::Lock,SupportSection},{"About",Icon::Info,8}};
static_assert(sizeof(SettingsItems)/sizeof(SettingsItems[0])==SettingsCount,"Settings catalog/layout must agree");
}
