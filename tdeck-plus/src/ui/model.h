#pragma once
#include "components.h"
#include "mesh/version.h"
#include "mesh/power_limits.h"
#include "mesh/discovery_limits.h"
#include "mesh/message_limits.h"
#include "home_widgets.h"
namespace mm {
enum class WeatherPhaseView {Closed,WifiOff,Connecting,NoNetwork,WaitingClock,Locating,Fetching,Ready,Error};
enum class Page { Home,Launcher,Messages,Chat,NewChat,Settings,Controls,Mesh,Radio,GPS,Maps,Weather,Info,Files,Wifi,Bluetooth,Terminal,DateTime,Updates,Battery,Power,Audio,Wallpaper,Keyboard,Storage,QuickSettings,Cli,ContactPicker,TextSize,AccentKey ,Link,Support,HomeWidgets,BesideClock,MainArea,Note};
struct Conversation { std::string name,scope,preview,time="--:--",sender,age="--";bool direct=false;int id=0,unread=0,activity=0; };
struct Message {std::string sender,time,body,meta;bool mine;std::string path,heard,scope;int hops=-1;uint32_t token=0;int repeaters=-1;Message(std::string s="",std::string t="",std::string b="",std::string m="",bool own=false):sender(s),time(t),body(b),meta(m),mine(own){} };
struct ContactRow {std::string name,detail;};
struct Node {std::string name,key;int type=0;bool live=false;uint32_t age=0;float rssi=0,snr=0;int hops=-1;std::string distance;}; // distance: "2.4 km NE" when both positions are known
struct MapPin {int x=0,y=0;bool repeater=false;std::string name;int step=-1;}; // step: route stop (0 = sender, 1.. = repeaters)
struct MapSegment {int x0=0,y0=0,x1=0,y1=0;bool dashed=false;};
struct State {
 const uint8_t *mapBits=nullptr;const uint16_t *mapPixels=nullptr;/* Plus: RGB565 map view */int mapStyle=0;/* Plus: 0 LIGHT, 1 NIGHT */int mapSelfX=-1,mapSelfY=-1,mapScalePx=0;bool mapFollow=false,messageRouteView=false;std::string mapStatus,mapScaleLabel;std::vector<MapPin> mapPins;std::vector<MapSegment> mapRoute;std::vector<std::string> mapLegend;
 bool historyLoading=false;std::string historyError;
 // CLI (30): raw output lines (wrapped when drawn), prompt, edit line, scroll in wrapped lines from the bottom.
 std::vector<std::string> cliLines;std::string cliPrompt,cliInput;int cliCursor=0,cliScroll=0;bool cliStreaming=false,cliCanPaste=false;
 std::string firstRepeater;
 bool displayLightEnabled=true,keyboardLightEnabled=true;
 bool audioReady=false,audioNotifications=true;int audioVolume=20;std::string audioMessage;
 Page page=Page::Home;Status status;
 // MeshCore app link (Plus): BLE name / PIN, Wi-Fi (TCP 5000) switch and address, which links are in use.
 // Link viewer: mode 0 loading, 1 page, 2 picture, 3 error; rows = visible page rows (kind -1 text, -2 heading,
 // -3 "view picture", >=0 link index); linkPixels = composed picture view (LinkArea, panel byte order).
 bool contextHasLink=false;int linkMode=0,linkFocus=-1,linkProgress=-1,linkScrollTop=0,linkScrollTotal=0;std::string linkHost,linkTitle,linkMessage,linkInfo,linkSwitch;
 std::vector<std::pair<std::string,int>> linkRows;const uint16_t *linkPixels=nullptr;bool linkZoomButtons=false;
 std::string appBleName,appPin,appWifiDetail,appBleInput;bool appBleEditing=false,appWifi=false,appBleLinked=false,appUsbLinked=false,appWifiLinked=false;
 bool wifiEnabled=true,wifiConnected=false,wifiConnecting=false,wifiScanning=false,wifiEditing=false,bleEnabled=true,bleReady=false;
 bool wifiHidden=false,wifiShowPassword=false;int wifiEditField=1;
 int wifiOffset=0,wifiFocus=0;std::string wifiSSID,wifiIP,wifiMessage,wifiPasswordMask;struct Network{std::string ssid;int rssi=0;bool secured=true,saved=false;};std::vector<Network> networks;
 bool weatherValid=false,weatherBusy=false,locationValid=false,gpsFix=false;
 bool gpsEnabled=true,gpsAvailable=false,gpsData=false,gpsNmeaFresh=false,gpsTimeValid=false,gpsTemporary=false;unsigned gpsUsed=0,gpsVisible=0,gpsBaud=0;
 float temperature=0,humidity=0,pressure=0,wind=0,high=0,low=0;int weatherCode=2;double latitude=0,longitude=0,altitude=0;
 std::string weatherText="No weather data",weatherUpdated,weatherMessage,weatherPlace,weatherSource;
 // Home widget beside the clock (Settings > Home widgets): the chosen one, the carousel focus and what the widgets show.
 // Times are local "HH:MM"; empty strings and false flags mean the data is not there yet.
 struct HomeWidgetData {bool sun=false;int sunPolar=0;std::string sunrise,sunset,nextSunTime;bool nextIsSunset=true;int dayPermille=-1;/* -1 before sunrise, 1001 after sunset */
  bool moonWaxing=true;int moonIllumination=0;double moonAge=0;std::string moonName,moonFull;
  bool gpsOn=false,gpsFix=false,position=false;/* gpsFix: live fix; position: a fix within the hour (Coordinates) */int altitude=0,satellites=0;double latitude=0,longitude=0;uint32_t fixAgeS=0;
  int live=0;bool repeater=false;std::string repeaterName;float repeaterSnr=0;uint32_t repeaterAgeS=0;
  bool clock=false;std::string month,weekday;int day=0,week=0;uint32_t weatherAgeS=UINT32_MAX;};/* clock: the time is set (Moon, Calendar, the sun); weatherAgeS: UINT32_MAX unknown */
 int homeWidget=HomeWidgetDefault,homeWidgetFocus=HomeWidgetDefault;HomeWidgetData widget;
 // Main area (Settings > Home widgets > Main area): the big widget under the clock band; its GPS widgets also read `widget`.
 struct TimelineRow {std::string time,channel,text;bool direct=false,unread=false;int chat=-1;};
 struct NearbyNode {std::string name;int meters=0,bearing=0;bool live=false;};
 struct SkySatView {int azimuth=0,elevation=0;bool used=false;};
 struct MainWidgetData {std::vector<TimelineRow> timeline;std::vector<NearbyNode> nearby;int placed=0;/* contacts with a position (the radar shows the nearest) */
  int packets[MainPacketBars]={};int packetsHour=0,nodesHeard=0;float bestSnr=0;bool snrKnown=false;
  std::string utm,spokenLat,spokenLon;int hdopHundredths=-1;std::vector<SkySatView> sky;
  std::vector<int> elevation;int climb=0;std::string elevationFrom;std::string note;};
 int homeMain=MainWidgetDefault,homeMainFocus=MainWidgetDefault,homeWidgetsRow=0;/* homeWidgetsRow: focused row on Home widgets */MainWidgetData main;std::string noteInput;
 std::string homeFact;// T-Deck Plus Home without weather: one rotating line under the contact count (nodes heard, uptime, a tip)
 WeatherPhaseView weatherPhase=WeatherPhaseView::Closed;int weatherHours=0;float apparent=0;int weatherHourCode[4]={};float weatherHourTemp[4]={};std::string weatherHourTime[4];
 uint32_t displayTimeout=30,keyboardTimeout=30;int powerField=0;int wallpaper=0,wallpaperFocus=0;int textSize=1,textSizeFocus=1,accentMode=1,accentFocus=1;/* accent key: 0 off, 1 right Shift, 2 left Shift */// chat text size (0 compact, 1 large) and the focused card on its page// lock-screen wallpaper (mm_wallpaper::list index) and the focused card on its page
bool batteryEstimated=false,batteryCharging=false,batteryFull=false;
 std::string meshCoreVersion=MM_MESHCORE_VERSION;
 std::string messageTone="Classic",mentionTone="Rising";
 std::string menuDate="Date not set",date="DATE NOT SET",notice,version="0.4.2.9",build="",commit="local",input;
 // First-boot welcome (Plus): step 0 intro, 1 Wi-Fi and clock, 2 band, 3 preset, 4 name, 5 path bytes, 6 summary.
 int dtField[5]={1,1,2026,0,0},dtFocus=0;
 std::string kbdQuery;bool kbdSearching=false;int kbdFocus=0,kbdLang=0;uint32_t kbdMask=1;// Settings > Keyboard languages
 // Storms app (Plus): status line, one row per event of the next 24 h.
 struct StormRow{std::string title,when,detail;bool thunder=false;};std::vector<StormRow> stormRows;std::string stormHead,stormSub;bool stormAlerts=true,stormBusy=false,stormReady=false;int stormOffset=0,weatherTab=0;// weatherTab: 0 now, 1 storms, 2 radar
 std::string radarTime,radarMessage;int radarZoom=7;// RADAR tab: frame time, status, zoom (the view itself is mapPixels)// Date / time editor (Plus): day, month, year, hour, minute
 int welcomeWifiChoice=0,welcomeStep=0,welcomeBand=868,welcomeFocus=0,welcomeScroll=0,welcomeBytes=2;std::string welcomeName,welcomePreset,welcomeArchive;std::vector<std::pair<std::string,std::string>> welcomePresets;
 // Updates page (section 17): texts from device::otaState(); otaProgress -1 = no download bar.
 std::vector<std::string> otaNotes;std::string otaNotesTitle="WHAT'S NEW";// release notes, one item per line ("What's new"); Launcher steps once saved to the SD
 std::string otaStatus="Not checked",otaDetail,otaAction="CHECK NOW";/* the update server is never shown */int otaProgress=-1;bool otaConfirm=false;std::string timeSource="Acquiring";uint64_t storageTotal=0,storageUsed=0;
 bool emojiPicker=false,quickEmoji=false;int emojiFocus=0,emojiTab=0;bool composerExpanded=false;/* any draft: the composer grows upward so every line can be proofread */size_t inputCursor=std::string::npos;/* byte offset of the caret in input; npos = end */std::vector<int> emojiRecent;
 bool quickSent=false;
 int unread=0;std::string receivedName="No messages",receivedPreview="Waiting for mesh messages",receivedTime="--:--";
 bool lockHasMessage=false,lockHint=false,unlockCounting=false;int quickCountdown=-1;// ENTER on the lock: dots 0..2 filled while the quick reply opens (-1 none)
std::string lockRepeater;
 bool quickReply=false,quickReplyEditable=false;std::string quickSender,quickChannel,quickPreview,quickInput,quickError;
 size_t messageLimit=matter::MESSAGE_MAX_TEXT_BYTES,quickMessageLimit=matter::MESSAGE_MAX_TEXT_BYTES;
 int unlockTapCount=0;
 bool scopePicker=false,scopeEditing=false,clearChatConfirm=false;int chatMenuFocus=0,scopeFocus=0,scopeOffset=0;
 std::string scopeInput;std::vector<std::string> regionScopes;
 bool channelOptions=false,deleteChannelConfirm=false,channelOptionMuted=false,channelOptionDirect=false,positionShare=false;
 int channelOptionFocus=0;std::string channelOptionName,positionCoordinates,positionMessage;
 std::vector<ContactRow> pickerRows;int pickerFocus=0,pickerTotal=0,pickerStart=0;bool pickerSearching=false,pickerMention=false;std::string pickerQuery;// pickerMention: "@" in a chat picks who to mention
 std::string unlockCode;int supportTab=0;// Settings > Donate / Unlock: 0 Donate, 1 Extra apps (QR + unlock code)
 bool channelKeyView=false;std::string channelKeyHex,channelKeyScope;int qrSize=0;std::vector<uint8_t> qrModules;
 // -2: infer from the complete thread; -1: latest own message is filtered out.
 int latestOwnMessage=-2;
 bool chatDirect=false;
 bool messageBodyView=false,quickPending=false;bool messageActions=false,messagePath=false,channelMuted=false;int pathOffset=0,heardOffset=0;Message contextMessage;std::string pathText;std::vector<std::string> heardPeers;
 int filteredNodeCount=0;
 // Contacts page (section 1): search field, keyboard focus, live (heard this boot) count of the filtered list.
 std::string contactQuery;bool contactSearching=false;int contactFocus=0,contactLive=0;
 bool radioPowerEditing=false;std::string radioPowerInput;
 int radioEditField=4,radioPreset=-1,radioBand=868,radioPathBytes=2,radioDeviceFocus=0;bool radioDraftChanged=false,radioDevicePage=false;
 bool radioReady=false,radioTX=false;
 int radioError=0,radioSF=8,radioCR=8,radioPower=matter::MAX_TX_POWER,radioFocus=0,radioQueued=0,nodeCount=0,nodeScroll=0;
 uint32_t radioPacketsRX=0,radioPacketsTX=0;
 float radioFrequency=869.618f,radioBW=62.5f;
 std::string radioName="MatterMesh";std::vector<Node> nodes;
 bool sd=false,touch=false,keyboard=false,caps=false,symbols=false,accent=false;int accentTaps=0;// accent armed (taps 1..4) or held (0)
 int cpuMHz=240;uint32_t stackFree=0,rasterMs=0,paintMs=0;
 int heapKB=0,flashMB=16;
 int keyboardLight=180,displayLight=100,voltage=-1;
 int settingsFocus=0;bool hideFocus=false;int listStart=-1;// touch browsing (Plus): no highlighted row; chat list first row
 int focus=0,scroll=0,filter=0,active=0,field=0,settingsOffset=0,chatOffset=0,chatPixel=0;// chatPixel (Plus): the newest shown message pushed down by this many px
 int chatTotal=0,chatPosition=0;// whole thread length and newest-first offset, for the chat scroll bar (0 = use messages/chatOffset)
 std::string chatName,scope;
 std::string newName,newScope="*",newKey;int newForm=0,messageMenuFocus=0;bool searching=false,messageMenu=false;
 std::vector<std::string> files;
 std::vector<Conversation> conversations;std::vector<Message> messages;
 int quickPressed=-1;// Quick settings: the tile under a resting finger (-1 none)
};
// The Home weather header shows a forecast fetched over Wi-Fi; offline (Wi-Fi off, no network) the widget takes its place.
inline bool weatherLive(const State &s){return s.weatherValid&&s.weatherPhase!=WeatherPhaseView::WifiOff&&s.weatherPhase!=WeatherPhaseView::NoNetwork;}
}
