#pragma once
#include "components.h"
#include "mesh/version.h"
#include "mesh/power_limits.h"
#include "mesh/discovery_limits.h"
#include "mesh/message_limits.h"
namespace mm {
enum class WeatherPhaseView {Closed,WifiOff,Connecting,NoNetwork,WaitingClock,Locating,Fetching,Ready,Error};
enum class Page { Home,Launcher,Messages,Chat,NewChat,Settings,Controls,Mesh,Radio,GPS,Maps,Weather,Info,Files,Wifi,Bluetooth,Terminal,DateTime,Updates,Battery,Power,Audio,Wallpaper,Antenna,Storage,QuickSettings,Cli,ContactPicker,Support,Keyboard,AccentKey };
struct Conversation { std::string name,scope,preview,time="--:--",sender,age="--";bool direct=false;int id=0,unread=0,activity=0; };
struct Message {std::string sender,time,body,meta;bool mine;std::string path,heard,scope;int hops=-1;uint32_t token=0;int repeaters=-1;Message(std::string s="",std::string t="",std::string b="",std::string m="",bool own=false):sender(std::move(s)),time(std::move(t)),body(std::move(b)),meta(std::move(m)),mine(own){} };
struct ContactRow {std::string name,detail;};
struct Node {std::string name,key;int type=0;bool live=false;uint32_t age=0;float rssi=0,snr=0;int hops=-1;std::string distance;}; // distance: "2.4 km NE" when both positions are known
struct MapPin {int x=0,y=0;bool repeater=false;std::string name;int step=-1;}; // step: route stop (0 = sender, 1.. = repeaters)
struct MapSegment {int x0=0,y0=0,x1=0,y1=0;bool dashed=false;};
struct State {
 const uint8_t *mapBits=nullptr;int mapSelfX=-1,mapSelfY=-1,mapScalePx=0;bool mapFollow=false,mapFull=false,messageRouteView=false;std::string mapStatus,mapScaleLabel;std::vector<MapPin> mapPins;std::vector<MapSegment> mapRoute;std::vector<std::string> mapLegend;
 bool historyLoading=false;std::string historyError;
 // CLI (30): raw output lines (wrapped when drawn), prompt, edit line, scroll in wrapped lines from the bottom.
 std::vector<std::string> cliLines;std::string cliPrompt,cliInput;int cliCursor=0,cliScroll=0;bool cliStreaming=false,cliCanPaste=false;
 std::string firstRepeater;
 int vibrationPercent=75;
 bool displayLightEnabled=true,keyboardLightEnabled=true;
 bool audioTranscribing=false,offlineEngineCompiled=false;int audioSeconds=0;bool audioReady=false,audioNotifications=true,audioRecording=false,audioWarming=false,dictationConfigured=false;int audioVolume=20;std::string audioMessage;
 Page page=Page::Home;Status status;
 // MeshCore app link: BLE name / PIN, Wi-Fi (TCP 5000) switch and address, which links are in use.
 std::string appBleName,appPin,appWifiDetail,appBleInput;bool appBleEditing=false,appWifi=false,appBleLinked=false,appUsbLinked=false,appWifiLinked=false;
 bool wifiEnabled=true,wifiConnected=false,wifiConnecting=false,wifiScanning=false,wifiEditing=false,bleEnabled=true,bleReady=false,bleClient=false;
 bool wifiHidden=false,wifiShowPassword=false;int wifiEditField=1;
 // First-boot welcome: step 0 intro, 1 Wi-Fi and clock, 2 band, 3 preset, 4 name, 5 path bytes, 6 summary.
 int welcomeWifiChoice=0,welcomeStep=0,welcomeBand=868,welcomeFocus=0,welcomeScroll=0,welcomeBytes=2;std::string welcomeName,welcomePreset,welcomeArchive;std::vector<std::pair<std::string,std::string>> welcomePresets;
 int wifiOffset=0,wifiFocus=0;std::string wifiSSID,wifiIP,wifiMessage,wifiPasswordMask;struct Network{std::string ssid;int rssi=0;bool secured=true,saved=false;};std::vector<Network> networks;
 bool weatherValid=false,weatherBusy=false,locationValid=false,gpsFix=false;
 bool gpsEnabled=true,gpsAvailable=false,gpsData=false,gpsNmeaFresh=false,gpsTimeValid=false,gpsTemporary=false;unsigned gpsUsed=0,gpsVisible=0,gpsBaud=0;
 float temperature=0,humidity=0,pressure=0,wind=0,high=0,low=0;int weatherCode=2;double latitude=0,longitude=0,altitude=0;int satellites=0;
 std::string weatherText="No weather data",weatherUpdated,weatherMessage,weatherPlace,weatherSource;
 WeatherPhaseView weatherPhase=WeatherPhaseView::Closed;int weatherNext=0,weatherHours=0;float apparent=0,precipitation=0;int weatherHourCode[4]={};float weatherHourTemp[4]={};std::string weatherHourTime[4];
 uint32_t displayTimeout=30,keyboardTimeout=30;int powerField=0;int wallpaper=0,wallpaperFocus=0;std::string kbdQuery;bool kbdSearching=false;int kbdFocus=0,kbdLang=0;uint32_t kbdMask=1;int accentMode=2,accentFocus=2,accentTaps=0;// lock-screen wallpaper (mm_wallpaper::list index) and the focused card on its page
bool batteryEstimated=false,batteryCharging=false,batteryFull=false;

 std::string meshCoreVersion=MM_MESHCORE_VERSION;
 std::string messageTone="Classic",mentionTone="Rising",messageVibration="Single",mentionVibration="Double";
 std::string menuDate="Date not set",date="DATE NOT SET",notice,version="",build="",commit="local",input;
 bool antennaConfirm=false,shutdownConfirm=false,usbPower=false,powerSourceKnown=false;
 // Updates page (section 17): texts from device::otaState(); otaProgress -1 = no download bar.
 // Date / time page: manual editor (day, month, year, hour, minute) and the field being edited.
 bool dateEditing=false;int dateFocus=0,dateFields[5]={1,1,2026,12,0};
 std::string otaStatus="Not checked",otaDetail,otaAction="CHECK NOW";/* the server address is never shown */int otaProgress=-1;bool otaConfirm=false;std::string timeSource="Acquiring";uint64_t storageTotal=0,storageUsed=0;
 bool emojiPicker=false;int emojiFocus=0,emojiTab=0;bool composerExpanded=false;/* any draft: the composer grows upward so every line can be proofread */size_t inputCursor=std::string::npos;/* byte offset of the caret in input; npos = end */std::vector<int> emojiRecent;std::string receivedChannel="Mesh messages";
 bool quickSent=false;int receivedHops=-1;
 int unread=0;std::string receivedName="No messages",receivedPreview="Waiting for mesh messages",receivedTime="--:--";
 bool lockHasMessage=false,lockEnd=false,lockHint=false,unlockCounting=false;std::string lockRepeater;
 bool quickReply=false,quickReplyEditable=false;std::string quickSender,quickChannel,quickPreview,quickInput,quickError;
 size_t messageLimit=matter::MESSAGE_MAX_TEXT_BYTES,quickMessageLimit=matter::MESSAGE_MAX_TEXT_BYTES;
 int unlockTapCount=0;
 bool scopePicker=false,scopeEditing=false,clearChatConfirm=false;int chatMenuFocus=0,scopeFocus=0,scopeOffset=0;
 std::string scopeInput;std::vector<std::string> regionScopes;
 bool channelOptions=false,deleteChannelConfirm=false,channelOptionMuted=false,channelOptionDirect=false,positionShare=false;
 int channelOptionFocus=0;std::string channelOptionName,positionCoordinates,positionMessage;
 std::vector<ContactRow> pickerRows;int pickerFocus=0,pickerTotal=0,pickerStart=0;bool pickerSearching=false;std::string pickerQuery;
 bool extraAppsUnlocked=false;std::string unlockCode;int supportTab=0;// Settings > Donate / Unlock: 0 Donate, 1 Extra apps (QR + unlock code)
 bool channelKeyView=false;std::string channelKeyHex,channelKeyScope;int qrSize=0;std::vector<uint8_t> qrModules;
 // -2: infer from the complete thread; -1: latest own message is filtered out.
 int latestOwnMessage=-2;
 bool chatDirect=false,chatRadio=false;
 bool messageBodyView=false,quickPending=false;bool messageActions=false,messagePath=false,channelMuted=false;int pathOffset=0,heardOffset=0;Message contextMessage;std::string pathText;std::vector<std::string> heardPeers;
 int filteredNodeCount=0;
 // Contacts page (section 1): search field, keyboard focus, live (heard this boot) count of the filtered list.
 std::string contactQuery;bool contactSearching=false;int contactFocus=0,contactLive=0;
 bool radioPowerEditing=false;std::string radioPowerInput;
 int radioEditField=4,radioPreset=-1,radioBand=868,radioPathBytes=2,radioDeviceFocus=0;bool radioDraftChanged=false,radioDevicePage=false;
 bool radioReady=false,radioInternal=true,radioRX=false,radioTX=false;
 int radioError=0,radioSF=8,radioCR=8,radioPower=matter::MAX_TX_POWER,radioFocus=0,radioQueued=0,nodeCount=0,nodeScroll=0;
 uint32_t radioPacketsRX=0,radioPacketsTX=0;
 float radioFrequency=869.618f,radioBW=62.5f,radioRSSI=-999,radioSNR=-999;
 std::string radioName="MatterMesh";std::vector<Node> nodes;
 bool sd=false,touch=false,keyboard=false,hapticReady=false,haptic=true,caps=false,symbols=false,accent=false;
 int cpuMHz=240;uint32_t stackFree=0,rasterMs=0,paintMs=0;
 int heapKB=0,flashMB=16;
 int keyboardLight=180,displayLight=100,voltage=-1;
 int settingsFocus=0;
 int focus=0,scroll=0,filter=0,active=0,field=0,settingsOffset=0,chatOffset=0;
 int chatTotal=0,chatPosition=0;// whole thread length and newest-first offset, for the chat scroll bar (0 = use messages/chatOffset)
 std::string chatName,scope;
 std::string newName,newScope="*",newKey;int newForm=0,messageMenuFocus=0;bool searching=false,messageMenu=false;
 std::vector<std::string> files;
 std::vector<Conversation> conversations;std::vector<Message> messages;
};
}
