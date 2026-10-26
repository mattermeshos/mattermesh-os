#include "mesh/chat_limits.h"
#include "services/clock_limits.h"
#include "services/ota.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include "board/device_model.h"
#include <nvs.h>
#include <algorithm>
#include <memory>
#include <functional>
// Chat/modals nest state construction and LCD rendering in the UI task.
// Headroom for SD reads under loop->handle->render->uiState (16 KiB overflowed in 0.4.1.4 testing).
constexpr size_t UI_TASK_STACK_BYTES=24*1024;
SET_LOOP_TASK_STACK_SIZE(UI_TASK_STACK_BYTES);
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <Adafruit_TCA8418.h>
#include "board/board.h"
#include <TouchDrvGT911.hpp>
#include "board/plus_display.h"
#include "board/plus_keyboard.h"
#include "ui/fast_canvas.h"
#include "ui/theme.h"
#include "services/link_viewer.h"
#include "services/chat_session.h"
#include "services/channel_form.h"
#include "services/channel_qr.h"
#include "services/direct_chat.h"
#include "services/geo.h"
#include "services/prefs.h"
#include "services/hex_util.h"
#include "services/sd_replace.h"
#include "ui/artwork.h"
#include "ui/renderer.h"
#include "ui/utf8.h"
#include "ui/emoji_choices.h"
#include "ui/navigation_catalog.h"
#include "ui/keyboard_navigation.h"
#include "ui/build_info.h"
#include <time.h>
#include <esp_system.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "services/connectivity.h"
#include "services/wifi_policy.h"
#include "services/weather.h"
#include "services/storm.h"
#include "services/radar.h"
#include "services/map_math.h"
#include "services/map_tiles.h"
#include <esp_heap_caps.h>
#include <soc/soc_memory_layout.h>
#include "services/location.h"
#include "services/time_sync.h"
#include "services/battery.h"
#include "services/power.h"
#include "services/lock_policy.h"
#include "services/unlock_taps.h"
#include "services/space_gesture.h"
#include "services/reply_policy.h"
#include "services/quick_reply.h"
#include "services/received_repeater.h"
#include "services/lock_messages.h"
#include "services/lock_policy.h"
#include "services/scope_policy.h"
#include "services/touch_pipeline.h"
#include "mesh/companion_settings.h"
#include "services/gps_position_policy.h"
#include "services/channel_catalog_policy.h"
#include "services/message_banner.h"
#include "mesh/history_page.h"
#include "services/input_queue.h"
#include "services/first_repeater.h"
#include "services/audio.h"
#include "services/ringtones.h"
#include "services/sd_transfer.h"
#include "services/wdt_trace.h"
#include "services/keyboard_layout.h"
#include "mesh/service.h"
#include "ui/cli_layout.h"
#include "mesh/radio_parameters.h"
#include "mesh/radio_presets.h"
#include "services/gps_backup.h"
#include "services/touch_keys.h"
#include "services/keyboard_modifiers.h"
#include "services/accent_key.h"
#include "mesh/journal.h"
#include "mesh/protocol_test.h"

// MatterMesh UI + MeshCore companion. Legacy demo records retain explicit labels.
plus::Lcd lcd;
mm::FastCanvas16 frame(mm::W,mm::H);// RGB565 (ui/fast_canvas.h), presented by changed rows (board/plus_display.h)
int paintedScreen=-1,paintedSection=-1;
static_assert(mm::W==plus::Lcd::Width&&mm::H==plus::Lcd::Height,"UI must match physical panel");
plus::Keyboard keyboard;
bool sdReady=false, keyboardReady=false;
static bool batteryLogOn=false;static uint32_t batteryLogAt=0,batteryLogRows=0;// Plus battery log (batteryLogLoop)
// Keyboard languages (Settings > Keyboard languages, section 12): enabled set and the one typing now, NVS "mmkbd".
uint32_t kbdMask=mm_kbd::DefaultMask;mm_kbd::Lang kbdLang=mm_kbd::Lang::Italian;String kbdQuery;bool kbdSearching=false;int kbdFocus=0;
void loadKeyboardLanguages(){kbdMask=mm_kbd::sanitize(device::prefGetUInt("mmkbd","mask",mm_kbd::DefaultMask));kbdLang=mm_kbd::active(kbdMask,mm_kbd::Lang(constrain(device::prefGetInt("mmkbd","lang",0),0,mm_kbd::LangCount-1)));}
bool saveKeyboardLanguages(){return device::prefPutUInt("mmkbd","mask",kbdMask)&&device::prefPutInt("mmkbd","lang",int(kbdLang));}
void toggleKeyboardLanguage(int id);void switchKeyboardLanguage();
std::vector<int> keyboardLanguageRows(){return mm_kbd::rows(kbdQuery.c_str());}
// Cyrillic layouts: a letter key typed into a message (composer or lock-screen quick reply) becomes its ЙЦУКЕН letter;
// menus, searches and passwords keep the keys.
std::string layoutLetter(char c){return mm_kbd::info(kbdLang).cyrillic&&((c>='a'&&c<='z')||(c>='A'&&c<='Z'))?mm_kbd::letter(kbdLang,c,c>='A'&&c<='Z'):std::string();}
int accentTaps=0;bool accentTyped=false;// accent key taps before the next letter (1 = first variant ... 4) / a letter typed while it is held
inline void accentReset(){accentTaps=0;accentTyped=false;}// dropped by Space, Backspace, the 0 key, a chat change, Back, lock / unlock and the quick reply opening, closing or sending
String command, input;
TouchDrvGT911 legacyTouch;
int touchBackend=0;
unsigned long touchEvents=0;device::TouchPipeline touchPipeline;bool panelPainting=false,touchTransition=false,touchDisplayReady=false;uint32_t visibleTouchScene=0,touchDispatched=0,touchStale=0,lastTouchDispatchMs=0,maxTouchDispatchMs=0;void captureTouch(bool force=false);uint32_t touchScene();void serviceReplay();
device::KeyboardModifiers keyboardModifiers;
unsigned long keyboardEvents=0,lastPaint=0,deleteAt=0,repeatAt=0;
bool deleteHeld=false;
device::FirstRepeater firstRepeater;bool firstRepeaterPainted=false;String chatError;uint32_t chatErrorUntil=0;
// A short message over the chat (send failures), cleared by the loop after `ms`.
static inline void showChatError(const String &message,uint32_t ms){chatError=message;chatErrorUntil=millis()+ms;}
uint8_t keyboardBrightness=180;bool forceFullRefresh=false;uint32_t lastPhysicalPaint=0;
device::InputQueue keyboardQueue;bool batchingInput=false;uint32_t busyServiceCalls=0,lastRasterMs=0,lastPaintMs=0,renderCalls=0;
constexpr uint32_t UI_INPUT_SETTLE_MS=0,UI_TYPING_FRAME_MS=16,UI_MIN_FRAME_MS=16;// LCD: repaint as soon as the input arrives, ~60 fps cap

enum class Screen { Chats, Chat, NewChat, Home, Section, Launcher, PickContact, Welcome };
Screen screen=Screen::Home,chatsParent=Screen::Home,sectionParent=Screen::Home;
Screen chatParent=Screen::Chats;int chatParentSection=0;
bool deviceLocked=false,lockScreenDirty=false;Screen previousScreen=Screen::Home;
device::LockKey lockKey;device::SpaceTaps unlockTaps;device::LockMessages lockMessages;device::ReceivedRepeater receivedRepeater;bool lockHintPainted=false;int trackballUnlockTaps=0;/* Plus: trackball clicks of the unlock gesture, shown as the banner dots */String stormBanner,stormLockText;uint32_t stormBannerUntil=0,stormLockUntil=0;/* storm alert: banner, lock band (weather_apps.inc) */uint32_t lockHintUntil=0,unlockCountdownAt=0;bool locked40Supported=true;uint32_t keyboardHardwareOverflows=0,lockRecoveries=0;int renderedUnlockTaps=-1;uint32_t lockedCpuPollAt=0;uint32_t lockMinute=0;
device::LockEnter lockEnter;device::QuickReply quickReply;
// ENTER on the lock screen with a message: a banner fills two dots over QUICK_REPLY_HOLD_MS, then the quick reply
// opens (no need to keep ENTER down: works with keyboards that only report taps). 0 = no countdown.
uint32_t quickCountdownAt=0;bool quickEmojiPicker=false;/* emoji picker open over the quick reply (Plus) */bool quickTxReady=false,quickTxSuccess=false;uint32_t quickTxToken=0;
device::ReplyOriginal pendingQuickOriginal;uint32_t lockMessageShownAt=0,paintedLockToken=0;
std::string lockReadError;bool consumeLockMessage(uint32_t token);
extern uint32_t quickReplyInputAt;void openQuickReply();void closeQuickReply();void sendQuickReply();void quickReplyKey(char c);void pollQuickReply();static void resolveQuickOriginal();static void lockShowNext(uint32_t token);static int quickCountdownDots();
String lastReceivedChannel="Mesh messages";int lastReceivedChat=-1;bool lastReceivedRead=true;uint32_t lastReceivedToken=0;int paintedReadChat=-1;uint32_t paintedReadToken=0;uint16_t paintedUnread=0;
String lastReceivedSender="No messages",lastReceivedBody="Waiting for mesh messages",lastReceivedTime="--:--";
uint16_t unreadMessages[matter::CHAT_SLOTS]={};bool composerExpanded=false;int paintedComposerTop=mm::ComposerY;int inputCursor=-1;/* caret byte offset in input, -1 = end */
// Typing refresh: a composer edit paints at once (no settle/frame pacing) with the short register waveform; after a
// pause the touched area gets an ordinary OTP partial that cleans the lighter ink.
bool composerUrgent=false;
String *editableField();
static void composerEdited(){if(!deviceLocked&&screen==Screen::Chat&&editableField()==&input)composerUrgent=true;}
static unsigned cursorAt();static void inputInsert(const String &text);unsigned long touchKeyEvents=0;int lastTouchKey=-1;
String messageTime(uint32_t timestamp);void enterLockMode();void exitLockMode();void updateLockScreen();void handleLockKey(bool pressed,uint32_t capturedAt=0,bool hardwareDebounced=false);void pollLockKey();void initTouch();void setTouchEnabled(bool enabled);void restoreLockedCpu();

int launcherSelection=-1,settingsFocus=0,settingsScroll=0,filesScroll=0,filesCount=0;
struct NavEntry{Screen screen;int section;};
std::vector<NavEntry> navigation;


int homeSelection=-1,section=0,discoverFilter=0,homeScroll=0;
// Section 30 is the CLI (cli_app.inc).
void openContact(int index);void contactPage(int direction);void openSection(int id,bool paint=true);void scrollChat(int delta);void touchScroll(int dy);void swipe(int dy);void loadMapStyle();
// Secrets (passwords, deferred commands) are zeroed before the String is released.
static void wipeString(String &s){for(unsigned i=0;i<s.length();i++)s.setCharAt(i,0);s="";}
void weatherTabsCompose(mm::State &v);bool weatherTabKey(char c);bool weatherTabTap(int x,int y);void weatherAppsTick();extern int weatherTab;
void toolsOpen(int id);void toolsClose(int id);void toolsCompose(mm::State &v);bool toolsBack();String *toolsField();
void mapOpen();void mapClose();void routeOpen(const std::string &path,const std::string &sender);void routeClose();bool routeLoading();void routeCompose(mm::State &v);void mapCompose(mm::State &v);void mapPan(int dx,int dy);
bool radioPowerEditing=false,radioDevicePage=false;int radioDeviceFocus=0;constexpr int CompanionNameField=matter::RadioFieldCount;String radioPowerInput;
matter::Config radioDraft;bool radioDraftLoaded=false;int radioEditField=matter::TxPower,radioPresetSelection=-1,radioBandSelection=868;
int radioFocus=3,nodeScroll=0,powerField=0,wifiOffset=0,wifiFocus=0;
String contactQuery;bool contactSearching=false;int contactFocus=0; // Contacts page search / keyboard focus
// T-Deck Plus: the "wallpaper" index is the colour theme (palette + lock illustration), same NVS slot.
#define WALLPAPER_COUNT mm::ThemeCount
#define WALLPAPER_NAME(i) mm::theme(i).name
// Settings > Date / time (Plus): manual editor, local time (CET/CEST), applied through matter::syncClock.
int dtField[5]={1,1,2026,0,0},dtFocus=0;
static int dtDays(int month,int year){static const int d[]={31,28,31,30,31,30,31,31,30,31,30,31};return month==2&&(year%4==0&&(year%100||year%400==0))?29:d[(month+11)%12];}
void dtLoad(){time_t now=time(nullptr);struct tm tm={};if(now>=MM_EPOCH_MIN)localtime_r(&now,&tm);else{tm.tm_year=2026-1900;tm.tm_mday=1;}
 dtField[0]=tm.tm_mday;dtField[1]=tm.tm_mon+1;dtField[2]=tm.tm_year+1900;dtField[3]=tm.tm_hour;dtField[4]=tm.tm_min;}
void dtStep(int field,int delta){const int lo[]={1,1,2024,0,0};int hi[]={dtDays(dtField[1],dtField[2]),12,2099,23,59};int span=hi[field]-lo[field]+1;
 dtField[field]=lo[field]+((dtField[field]-lo[field]+delta)%span+span)%span;dtField[0]=min(dtField[0],dtDays(dtField[1],dtField[2]));}
bool dtApply(String &reason){struct tm t={};t.tm_mday=dtField[0];t.tm_mon=dtField[1]-1;t.tm_year=dtField[2]-1900;t.tm_hour=dtField[3];t.tm_min=dtField[4];t.tm_isdst=-1;
 time_t e=mktime(&t);if(e<MM_EPOCH_MIN){reason="Invalid date";return false;}if(!matter::syncClock(uint32_t(e),reason))return false;device::timeSyncManual(uint32_t(e));return true;}
int supportTab=0;// Settings > Donate / Unlock: 0 Donate, 1 Extra apps
int chatTextSize=1,textSizeFocus=1;// Plus: chat text size (0 compact, 1 large), NVS "mmui"/"chatlg"
void applyTextSize(){
 mm::setChatTextLarge(chatTextSize==1);
}
// One NVS integer: the default when the namespace cannot be opened; put reports whether all 4 bytes were written.
void loadTextSize(){chatTextSize=device::prefGetInt("mmui","chatlg",1)?1:0;textSizeFocus=chatTextSize;applyTextSize();}
bool setTextSize(int index){index=index?1:0;if(!device::prefPutInt("mmui","chatlg",index))return false;chatTextSize=index;applyTextSize();return true;}// applied only once saved
int lockWallpaper=0,wallpaperFocus=0;// theme index (mm::theme), persisted in NVS "mmlock"
void loadWallpaper(){lockWallpaper=constrain(device::prefGetInt("mmlock","wall",0),0,WALLPAPER_COUNT-1);wallpaperFocus=lockWallpaper;
 mm::setTheme(lockWallpaper);
}
bool setWallpaper(int index){if(index<0||index>=WALLPAPER_COUNT)return false;bool ok=device::prefPutInt("mmlock","wall",index);if(ok)lockWallpaper=index;
 if(ok)mm::setTheme(index);// palette swap: every row changes, the next frame repaints the whole panel
 return ok;}
bool wifiEditing=false,wifiHidden=false,wifiShowPassword=false;int wifiEditField=1;String wifiTarget,wifiPassword;
// Settings > Bluetooth (Plus): the Bluetooth name being edited (the part after "MeshCore-").
bool bleNameEditing=false;String bleNameInput;
matter::Journal radioJournal;
matter::HistoryPager archivePager;
String channelKeys[matter::CHAT_SLOTS],peerKeys[matter::CHAT_SLOTS];
bool channelReady[matter::CHAT_SLOTS]={};
unsigned long radioChangedAt=0;bool radioDirty=false;
static inline void markRadioDirty(){radioDirty=true;radioChangedAt=millis();}
device::MessageBanner incomingBanner;
uint8_t displayBrightness=100;
// Sound page: step the message or mention ringtone and play it once as a preview.
void cycleRingtone(bool mention,int step){auto a=device::audioState();int tone=((mention?a.mentionTone:a.messageTone)+step+device::RINGTONE_COUNT)%device::RINGTONE_COUNT;device::audioRingtone(mention,tone);device::audioPreview(tone);}
// Radio RX integration will call this only for a new incoming message, after deduplication.
void onIncomingMessage(bool mention=false){
 device::wakeDisplay(5000);// LCD: an incoming message lights the screen for 5 s
if(deviceLocked&&getCpuFrequencyMhz()<80)setCpuFrequencyMhz(80);device::flashKeyboardNotification();device::audioNotify(false,mention);}

struct ChatEntry { String name,scope; bool direct; ChatEntry(String n="",String s="",bool d=false):name(n),scope(s),direct(d){} };
constexpr int MAX_CHATS=matter::CHAT_SLOTS;
device::ChatDrafts<String,MAX_CHATS> chatDrafts;
ChatEntry chats[MAX_CHATS];
int chatCount=1,selected=0,activeChat=0,formField=0;
int newForm=0,messageMenuFocus=0;bool dirty=false;
unsigned long lastEdit=0;
// Repaint requests: now (skips the typing settle delay) or after the settle delay since this edit.
static inline void repaintNow(){dirty=true;lastEdit=millis()-UI_INPUT_SETTLE_MS;}
static inline void repaintSettled(){dirty=true;lastEdit=millis();}
String newName,newScope="*",newKey,notice,searchQuery;
bool searching=false,messagesMenu=false,chatDetails=false,emojiPicker=false;int emojiFocus=0,emojiTab=0;std::vector<int> emojiRecent;static std::vector<int> emojiItems();bool shutdownConfirm=false;
std::vector<mm::MessageHit> renderedChatMessages,paintedChatMessages;int paintedChatConversation=-1;uint32_t chatRowsRevision=0;
int chatOffset=0,chatPixel=0;// chatPixel (Plus): continuous scroll inside the messages (see Renderer::chat)
bool scopePicker=false,scopeEditing=false,clearChatConfirm=false;int chatMenuFocus=0,scopeFocus=0;String scopeInput;std::vector<std::string> regionScopes={"*"};
bool channelOptions=false,deleteChannelConfirm=false,positionShare=false,channelKeyView=false;int channelOptionTarget=-1,channelOptionFocus=0;
String channelKeyHex;int channelQrSize=0;std::vector<uint8_t> channelQr;
// New direct chat picker: saved companion contacts (type 1), filtered by pickerQuery.
bool pickerSearching=false;String pickerQuery;int pickerFocus=0;std::vector<matter::Node> pickerContacts;
struct ChannelHit {mm::Rect area;int id;};std::vector<ChannelHit> paintedChannels,renderedChannels;
void deleteSelectedChannel();void shareGpsPosition();
void render(bool partial=false,int batteryPreview=-2);
extern int accentMode,accentFocus;static bool accentContext();static bool accentHeld();static bool shiftHeld();bool setAccentMode(int mode);void loadAccentMode();// accent key (defined with the keyboard handling)
void trackballBegin();bool appUsbLinked();void appLinkFill(mm::State &v);void linkFill(mm::State &v);bool linkBack();// link viewer (link_app.inc)
// MeshCore app links (companion_app.inc)
// A panel over the chat (options, position, details, scopes, clear, message actions/path, emoji picker).

// Touch browsing (Plus): the finger scrolls the view only, like a phone list; no row is highlighted until a key or the
// trackball moves the focus again (then the focus is pulled back into the visible rows). chatsTop = first row shown.
// First-boot welcome (Plus): Wi-Fi (and clock) -> band -> preset -> companion name -> path hash bytes -> summary; NVS mmsetup/done afterwards.
// welcomeWifi: the Wi-Fi page was opened from the welcome; BACK, or a connection, returns to it.
// welcomeArchive: SD folder that received the previous installation's data on a clean install ("" when none).
bool welcomeWifi=false;int welcomeWifiChoice=0;int welcomeStep=0,welcomeBand=868,welcomeFocus=0,welcomeBytes=2;String welcomeName,welcomeArchive;std::vector<int> welcomeList;
bool pickerMention=false;// the contact picker chooses who to mention in the open chat ("@"), not a direct chat
bool touchBrowse=false;int chatsTop=0;bool flingCaught=false;int renderHold=0;bool renderPending=false;
static int chatsRows(){return mm::ListRows;}// Plus: the search field sits in the bottom bar
static int chatsFocusIndex(const std::vector<mm::Conversation> &list){for(unsigned i=0;i<list.size();i++)if(list[i].id==selected)return i;return 0;}
static int chatsStart(const std::vector<mm::Conversation> &list){int rows=chatsRows();if(touchBrowse)return constrain(chatsTop,0,max(0,int(list.size())-rows));return chatsFocusIndex(list)/rows*rows;}
void beginTouchBrowse();void endTouchBrowse();std::vector<mm::Conversation> conversationList();
uint32_t mutedChannels=0;bool messageActions=false,messagePath=false,messageBodyView=false,messageRouteView=false;int messagePathOffset=0,heardOffset=0,contextConversation=-1;mm::Message contextMessage;
size_t messageByteLimit(int conversation=-1);
void resendContext();bool queueRadioMessage(String body,const String *scopeOverride=nullptr,int conversation=-1,uint32_t *queuedToken=nullptr);void showLockHint();std::vector<uint32_t> lockMessageTokens();
void loadChannelNotifications(){mutedChannels=device::prefGetUInt("mmchats","muted",0);}
bool toggleChannelNotifications(int id){if(id<0||id>=chatCount)return false;uint32_t next=mutedChannels^(uint32_t(1)<<id);bool ok=device::prefPutUInt("mmchats","muted",next);if(ok)mutedChannels=next;return ok;}
// Records beyond the 64-entry RAM cache: open SD page, one-record memo, then one SD read.
matter::Record recordMemo;uint32_t recordMemoStamp=0;
uint32_t journalStamp(){uint32_t v=radioJournal.clearedChats;for(auto &s:radioJournal.summaries)v+=s.revision;return v;}
// The message a quick reply answers, from its journal record (the lock screen and the notification share it).
static device::ReplyOriginal replyOriginalFrom(const matter::Record &r){device::ReplyOriginal o;o.chat=r.chat;o.token=r.token;o.hops=r.hops;o.sender=r.sender.c_str();o.body=r.body.c_str();o.scope=r.scope.c_str();o.path=r.path.c_str();o.channel=chats[r.chat].name.c_str();o.direct=chats[r.chat].direct;return o;}
bool loadedRecord(uint32_t token,matter::Record &out){
 if(!token)return false;for(auto &r:radioJournal.records)if(r.token==token){out=r;return true;}
 auto &p=archivePager.page;auto s=radioJournal.summary(p.chat);if(p.ready&&s&&p.revision==s->revision)for(auto &r:p.records)if(r.token==token){out=r;return true;}
 auto stamp=journalStamp();if(recordMemo.token==token&&recordMemoStamp==stamp){out=recordMemo;return true;}
 if(!radioJournal.readRecord(token,out,false))return false;recordMemo=out;recordMemoStamp=stamp;return true;
}
std::vector<matter::Record> contextRecords(){std::vector<matter::Record> out;matter::Record r;if(loadedRecord(contextMessage.token,r))out.push_back(r);return out;}
mm::Message contextSnapshot(){auto message=contextMessage;matter::Record record;if(message.token&&loadedRecord(message.token,record)&&record.chat==contextConversation){message.repeaters=record.repeaters;message.heard=record.heard.c_str();message.meta=record.status.c_str();}return message;}
std::vector<std::string> repeaterRows(const mm::Message &message){
 std::vector<std::string> rows;String hashes=message.heard.c_str();int start=0;
 while(start<int(hashes.length())&&rows.size()<64){int end=hashes.indexOf(',',start);if(end<0)end=hashes.length();String hash=hashes.substring(start,end);start=end+1;if(hash.length()<2||hash.length()>8||hash.length()%2)continue;bool valid=true;for(unsigned i=0;i<hash.length();i++)if(!isxdigit((unsigned char)hash[i]))valid=false;if(!valid)continue;hash.toLowerCase();int matches;String name=matter::nodeNameForPrefix(hash,matches,2);String row="["+hash+"] ";if(matches==1)row+=mm::senderDisplayName(name.c_str()).c_str();else row+=matches>1?"Ambiguous / "+String(matches)+" RPT":"Unknown repeater";rows.push_back(row.c_str());}
 return rows;
}
String fullMessagePath(){
 if(messageBodyView)return String(contextMessage.sender.c_str())+"\n"+String(contextMessage.time.c_str())+"\n\n"+String(contextMessage.body.c_str());
 if(contextMessage.mine){auto message=contextSnapshot();String text="Status: "+String(message.meta.c_str())+"\nRepeaters heard: "+(message.repeaters<0?String("Unknown"):String(message.repeaters))+"\n";for(auto &row:repeaterRows(message))text+=String(row.c_str())+"\n";if(message.heard.empty())text+=message.repeaters>0?"Names not recorded for this old message.\n":"No echoes recorded.\n";return text+"Echo paths are not destination ACKs.\n\nMessage:\n"+String(contextMessage.body.c_str());}
 String path=contextMessage.path.c_str();String text="Sender: "+String(contextMessage.sender.c_str())+"\nTime: "+String(contextMessage.time.c_str())+"\n"+String(contextMessage.meta.c_str())+"\n";
 if(!path.length())return text+"No path saved for this message.";
 if(path=="direct")return text+"Direct route: the received packet does not contain its full traversed path.";
 if(!path.startsWith("flood:"))return text+"Path unavailable.";
 path.remove(0,6);if(!path.length())return text+"Direct reception: 0 repeaters.";
 text+="Path RX (hash):\n";int step=1;
 while(path.length()){int split=path.indexOf(',');String hash=split<0?path:path.substring(0,split);int matches=0;String name=matter::nodeNameForPrefix(hash,matches);text+=String(step++)+". "+hash;if(matches==1)text+=" / "+name;else if(matches>1)text+=" / ambiguous";else text+=" / unknown";text+='\n';if(split<0)break;path.remove(0,split+1);}
 return text+"Destination: this T-Deck";
}
void scrollMessagePath(int delta){mm::Components c(frame);int lines=c.lines(fullMessagePath().c_str(),mm::W-2*mm::Margin-16,mm::Font::ChatBody).size();int rows=(mm::Bottom-mm::Top-8-70)/c.lineHeight(mm::Font::ChatBody);messagePathOffset=constrain(messagePathOffset+delta,0,max(0,lines-rows));}
void messageAction(int action){
 if(contextMessage.mine&&action==0){resendContext();return;}
 if(action==0&&!contextMessage.mine){
  String prefix=device::replyPrefix(contextMessage.sender).c_str();
  if(input.startsWith(prefix)){messageActions=false;}
  else if(prefix.length()+input.length()>messageByteLimit()){showChatError("Draft full: not enough space",3000);messageActions=false;}
  else{input=prefix+input;if(inputCursor>=0)inputCursor+=prefix.length();notice="";messageActions=false;}
 }else if(contextMessage.mine&&action==4){String prefix=String("> ")+String(contextMessage.body.c_str())+" ";if(prefix.length()+input.length()<=messageByteLimit()){input=prefix+input;if(inputCursor>=0)inputCursor+=prefix.length();messageActions=false;}}

 else if(action==1){notice=toggleChannelNotifications(activeChat)?((mutedChannels&(uint32_t(1)<<activeChat))?"Notifications muted":"Notifications unmuted"):"Cannot save notification settings";}
 else if(action==2){messageActions=false;messagePath=true;messageBodyView=messageRouteView=false;messagePathOffset=0;}
 else if(action==mm::MessageMapAction&&!contextMessage.mine){messageActions=messageBodyView=false;messagePath=messageRouteView=true;routeOpen(contextMessage.path,contextMessage.sender);}
 else if(action==5&&!contextMessage.mine){messageActions=messageRouteView=false;messagePath=true;messageBodyView=true;messagePathOffset=0;}
 else messageActions=messagePath=false;
 render();
}

const char keys[4][10]={{'q','w','e','r','t','y','u','i','o','p'}, {'a','s','d','f','g','h','j','k','l','\b'}, {2,'z','x','c','v','b','n','m','$','\n'}, {0,0,0,0,0,1,0,' ',3,1}};
const char symbols[4][10]={{'#','1','2','3','(',')','_','-','+','@'}, {'*','4','5','6','/',':',';','\'','"','\b'}, {0,'7','8','9','?','!',',','.','0','\n'}, {0,0,0,0,0,2,0,' ',3,2}};
void splash(int progress,const String &stage);
void submitRadio();
void configureChannels();
mm::State uiState();
// Plus: keys and the trackball scroll by ChatKeyStepPx (continuous scroll); the Max steps one message.
constexpr int ChatKeyStepPx=60;
void scrollChat(int delta){{if(delta){chatPixel+=delta>0?ChatKeyStepPx:-ChatKeyStepPx;render();}return;}}
void incomingRadio(const matter::Received &message);
void deliveredRadio(const matter::Delivery &delivery);
void submitLocal(){submitRadio();}
void renderChat(){
 chatDrafts.recoverIfEmpty(activeChat,input);
 mm::Renderer r(frame);auto state=uiState();firstRepeaterPainted=!state.firstRepeater.empty();r.chat(state);{chatOffset=max(0,chatOffset+r.chatShiftOut);chatPixel=r.chatPixelOut;/* still below the newest loaded message: step past the window or page edge (the pager reloads), next frame goes on */if(chatPixel<0&&chatOffset>0){chatOffset--;repaintNow();}}renderedChatMessages=r.snapshotMessages(state);paintedComposerTop=r.chatComposerTop;
 if(!deviceLocked&&!chatDetails&&!scopePicker&&!clearChatConfirm&&!positionShare&&!channelOptions&&!messageActions&&!messagePath&&!emojiPicker&&chatOffset==0){
  uint32_t latest=0;for(auto it=state.messages.rbegin();it!=state.messages.rend();++it)if(!it->mine){latest=it->token;break;}
  for(auto &hit:r.messageBounds())if(latest&&state.messages[hit.second].token==latest){paintedReadChat=activeChat;paintedReadToken=latest;paintedUnread=unreadMessages[activeChat];break;}
 }
}
void commitReadFrame(){
 if(paintedReadChat<0)return;auto summary=radioJournal.summary(paintedReadChat);uint32_t latest=summary?summary->lastRX:0;
 auto &unread=unreadMessages[paintedReadChat];uint16_t before=unread;
 if(!device::commitPaintedRead(radioJournal,paintedReadChat,paintedReadToken,latest,paintedUnread,unread)){
  if(notice!="Read state not saved / SD error"){notice="Read state not saved / SD error";showChatError(notice,3000);markRadioDirty();}
  paintedReadChat=-1;return;
 }
 if(before!=unread){markRadioDirty();}if(lastReceivedChat==paintedReadChat&&lastReceivedToken==paintedReadToken)lastReceivedRead=true;
 paintedReadChat=-1;
}


String clean(String value) {
 value.trim();String result;
 for(unsigned i=0;i<value.length()&&result.length()<31;i++) {
  unsigned char c=value[i];if(c>=32&&c<=126&&c!='\t')result+=char(c);
 }
 return result;
}
bool saveChats() {
 if(!sdReady)return false;
 if(!SD.exists("/maxchat")&&!SD.mkdir("/maxchat"))return false;
 File f=SD.open("/maxchat/catalog.tmp",FILE_WRITE);
 if(!f)return false;
 bool ok=f.print("MAXCHAT1\n")>0;
 for(int i=0;i<chatCount;i++) {
  String row=String(chats[i].direct?'D':'C')+"\t"+chats[i].scope+"\t"+chats[i].name+"\n";
  if(f.print(row)!=row.length())ok=false;
 }
 f.flush();f.close();if(!ok){SD.remove("/maxchat/catalog.tmp");return false;}
 return device::sdReplace(SD,"/maxchat/catalog.tmp","/maxchat/catalog.tsv","/maxchat/catalog.bak");
}
void loadRegionScopes(){
 auto add=[](const std::string &value){auto normalized=device::normalizedScope(value);if(normalized.empty()||regionScopes.size()>=device::MAX_REGION_SCOPES)return;if(std::find(regionScopes.begin(),regionScopes.end(),normalized)==regionScopes.end())regionScopes.push_back(normalized);};
 String saved=device::prefGetString("mmscope","list");int start=0;while(start<int(saved.length())){int end=saved.indexOf('\n',start);if(end<0)end=saved.length();add(saved.substring(start,end).c_str());start=end+1;}
 for(int i=0;i<chatCount;i++)add(chats[i].scope.c_str());
}
bool saveRegionScopes(){String text;for(auto &value:regionScopes)text+=String(value.c_str())+"\n";return device::prefPutString("mmscope","list",text);}
void openScopePicker(bool editing=false){scopePicker=true;scopeEditing=editing;chatDetails=clearChatConfirm=false;scopeInput="";notice="";scopeFocus=0;for(int i=0;i<int(regionScopes.size());i++)if(regionScopes[i]==chats[activeChat].scope.c_str())scopeFocus=i;render();}
void saveNewScope(){auto name=device::normalizedScope(scopeInput.c_str());if(name.empty()){notice="Use letters, numbers, hyphens";render();return;}auto it=std::find(regionScopes.begin(),regionScopes.end(),name);if(it==regionScopes.end()){
 if(regionScopes.size()>=device::MAX_REGION_SCOPES){notice="Scope limit: 64";render();return;}regionScopes.push_back(name);if(!saveRegionScopes()){regionScopes.pop_back();notice="Scope save failed";render();return;}matter::registerScope(name.c_str());scopeFocus=regionScopes.size()-1;
 }else scopeFocus=it-regionScopes.begin();scopeEditing=false;scopeInput="";notice="Select APPLY to use scope";render();}
void applySelectedScope(){if(scopeFocus<0||scopeFocus>=int(regionScopes.size()))return;String old=chats[activeChat].scope,next=regionScopes[scopeFocus].c_str();String reason;
 if(!chats[activeChat].direct&&channelReady[activeChat]&&!matter::channel(activeChat,chats[activeChat].name,channelKeys[activeChat],next,reason)){notice=reason;render();return;}
 chats[activeChat].scope=next;if(!saveChats()){chats[activeChat].scope=old;if(!chats[activeChat].direct&&channelReady[activeChat])matter::channel(activeChat,chats[activeChat].name,channelKeys[activeChat],old,reason);notice="Scope save failed";render();return;}
 matter::registerScope(next);scopePicker=scopeEditing=chatDetails=false;notice="";showChatError("Scope: "+next,1500);render();
}
void refreshLastReceived(){lastReceivedRead=true;lastReceivedToken=0;lastReceivedChat=-1;lastReceivedChannel="Mesh messages";lastReceivedSender="No messages";lastReceivedBody="Waiting for mesh messages";lastReceivedTime="--:--";
 for(auto it=radioJournal.index.rbegin();it!=radioJournal.index.rend();++it)if(!it->mine&&it->chat>=0&&it->chat<chatCount){matter::Record rec;if(!loadedRecord(it->token,rec))break;lastReceivedChat=rec.chat;lastReceivedToken=rec.token;lastReceivedRead=unreadMessages[rec.chat]==0;lastReceivedChannel=chats[rec.chat].name;lastReceivedSender=rec.sender;lastReceivedBody=rec.body;lastReceivedTime=messageTime(rec.timestamp);break;}
}
void clearActiveChat(){
 if(!sdReady||!radioJournal.clearChat(activeChat)){notice="Clear failed / check SD";render();return;}
 unreadMessages[activeChat]=0;if(lastReceivedChat==activeChat)refreshLastReceived();lockScreenDirty=true;firstRepeater=device::FirstRepeater{};firstRepeaterPainted=false;
 archivePager.close();chatOffset=0;chatPixel=0;chatDetails=clearChatConfirm=messageActions=messagePath=false;notice="";showChatError("Chat cleared",1500);render();
}
void chatOption(int index){chatMenuFocus=index;if(index==0)openScopePicker();else if(index==1)openScopePicker(true);else{chatDetails=false;clearChatConfirm=index==2;positionShare=index==3;notice="";render();}}
void loadChats() {
 chats[0]={"Public","*",false};// fallback when the SD catalog is missing
 if(!sdReady)return;
 const char *path=device::sdSavedPath(SD,"/maxchat/catalog.tsv","/maxchat/catalog.bak");
 File f=SD.open(path);if(!f)return;
 String header=f.readStringUntil('\n');header.trim();
 if(header!="MAXCHAT1"){f.close();return;}
 int n=0;
 while(f.available()&&n<MAX_CHATS){
  String row=f.readStringUntil('\n');row.trim();
  int a=row.indexOf('\t'),b=row.indexOf('\t',a+1);
  if(a!=1||b<2||(row[0]!='C'&&row[0]!='D'))continue;
  String name=clean(row.substring(b+1)),scope=clean(row.substring(a+1,b));
  if(!name.length())continue;
  chats[n++]={name,scope,row[0]=='D'};
 }
 f.close();chatCount=n;
}
String radioField(const String &row,int index){int a=0;while(index--){a=row.indexOf('\t',a);if(a<0)return "";a++;}int b=row.indexOf('\t',a);return b<0?row.substring(a):row.substring(a,b);}
void loadBindings(){
 File f=SD.open(device::sdSavedPath(SD,"/mattermesh/bindings.tsv","/mattermesh/bindings.bak"));if(f){while(f.available()){String row=f.readStringUntil('\n');row.trim();int id=radioField(row,0).toInt();if(id<0||id>=chatCount)continue;peerKeys[id]=radioField(row,1);channelKeys[id]=radioField(row,2);}f.close();}
}
bool saveBindings(){
 SD.mkdir("/mattermesh");File f=SD.open("/mattermesh/bindings.tmp","w");if(!f)return false;
 for(int i=0;i<chatCount;i++){String row=String(i)+"\t"+peerKeys[i]+"\t"+channelKeys[i]+"\n";if(f.print(row)!=row.length()){f.close();SD.remove("/mattermesh/bindings.tmp");return false;}}f.flush();f.close();
 return device::sdReplace(SD,"/mattermesh/bindings.tmp","/mattermesh/bindings.tsv","/mattermesh/bindings.bak");
}
void configureChannels(){matter::clearChannels();for(int i=0;i<MAX_CHATS;i++){String reason;channelReady[i]=i<chatCount&&!chats[i].direct&&matter::channel(i,chats[i].name,channelKeys[i],chats[i].scope,reason);}}
// Stage catalog, bindings and both journals together; retain unchanged chat IDs
// through the supplied mapping, and roll back filesystem commits on failure.
bool commitChannelCatalog(const ChatEntry *next,const String *keys,const String *peers,int count,const int *mapping){
 if(!sdReady||deviceLocked||matter::busy()||count<0||count>MAX_CHATS)return false;
 uint16_t unread[MAX_CHATS]={};uint32_t mute=0;String drafts[MAX_CHATS];
 for(int i=0;i<chatCount;i++)if(mapping[i]>=0){int j=mapping[i];unread[j]=unreadMessages[i];drafts[j]=chatDrafts.saved[i];if(mutedChannels&(uint32_t(1)<<i))mute|=uint32_t(1)<<j;}
 uint32_t mappedLocalBoundary[MAX_CHATS]={};
 const char *paths[]={"/maxchat/catalog.tsv","/mattermesh/bindings.tsv","/mattermesh/messages.tsv"};
 bool existed[3]={},committed[3]={};bool ok=true;
 for(int i=0;i<3&&ok;i++){
  String tmp=String(paths[i])+".channels.tmp";SD.remove(tmp);File out=SD.open(tmp,"w");if(!out){ok=false;break;}
  auto put=[&](const String &row){if(out.print(row)!=row.length())ok=false;};
  if(i==0){put("MAXCHAT1\n");for(int j=0;j<count;j++)put(String(next[j].direct?"D":"C")+"\t"+next[j].scope+"\t"+next[j].name+"\n");}
  else if(i==1){for(int j=0;j<count;j++)put(String(j)+"\t"+peers[j]+"\t"+keys[j]+"\n");}
  else {
   File in=SD.open(paths[i]);if(in){while(in.available()&&ok){String row=in.readStringUntil('\n');
    std::string mapped=row.c_str();auto result=device::remapChatRow(mapped,i==2,mapping,chatCount,mappedLocalBoundary);
    if(result==device::RemapRow::Invalid){ok=false;break;}if(result==device::RemapRow::Drop)continue;row=mapped.c_str();
    put(row+"\n");
   }in.close();}
  }
  out.flush();out.close();
 }
 if(ok){for(int i=0;i<3;i++){
  String backup=String(paths[i])+".channels.bak",tmp=String(paths[i])+".channels.tmp";
  existed[i]=SD.exists(paths[i]);if(!device::sdReplace(SD,tmp.c_str(),paths[i],backup.c_str())){ok=false;break;}committed[i]=true;
 }}
 if(ok)ok=device::prefPutUInt("mmchats","muted",mute);
 if(!ok){for(int i=2;i>=0;i--)if(committed[i]){SD.remove(paths[i]);if(existed[i])SD.rename(String(paths[i])+".channels.bak",paths[i]);}for(auto path:paths)SD.remove(String(path)+".channels.tmp");return false;}
 chatCount=count;for(int i=0;i<MAX_CHATS;i++){peerKeys[i]=i<count?peers[i]:String();channelKeys[i]=i<count?keys[i]:String();channelReady[i]=false;unreadMessages[i]=unread[i];chatDrafts.saved[i]=drafts[i];chats[i]=i<count?next[i]:ChatEntry();}
 radioJournal.begin(SD);archivePager.close();mutedChannels=mute;refreshLastReceived();firstRepeater=device::FirstRepeater{};firstRepeaterPainted=false;
 configureChannels();selected=activeChat=0;input="";inputCursor=-1;chatOffset=0;chatPixel=0;searching=chatDetails=messageActions=messagePath=scopePicker=scopeEditing=clearChatConfirm=channelOptions=deleteChannelConfirm=positionShare=false;channelOptionTarget=-1;navigation.clear();screen=Screen::Chats;lockScreenDirty=true;return true;
}
// Replace channel configuration through USB without embedding shared keys.
bool replaceChannels(const String &payload){
 if(!sdReady||deviceLocked||matter::busy())return false;
 ChatEntry next[MAX_CHATS];String keys[MAX_CHATS];int count=0,start=0;
 while(start<int(payload.length())){
  int end=payload.indexOf(';',start);if(end<0)end=payload.length();String row=payload.substring(start,end);row.replace(',','\t');
  if(count>=MAX_CHATS)return false;
  String name=radioField(row,0),scope=radioField(row,1),secret=radioField(row,2);
  if(name!=clean(name)||!name.length()||scope!=clean(scope)||!scope.length())return false;
  if(secret.length()!=32&&secret.length()!=64&&!(secret.isEmpty()&&name=="Public"))return false;
  for(unsigned j=0;j<secret.length();j++)if(!isxdigit((unsigned char)secret[j]))return false;
  for(int j=0;j<count;j++)if(next[j].name==name)return false;
  next[count]={name,scope,false};keys[count++]=secret;start=end+1;
 }
 if(!count)return false;
 int mapping[MAX_CHATS];for(int i=0;i<MAX_CHATS;i++)mapping[i]=-1;
 for(int i=0;i<chatCount;i++)for(int j=0;j<count;j++)if(!chats[i].direct&&chats[i].name==next[j].name)mapping[i]=j;
 String peers[MAX_CHATS];return commitChannelCatalog(next,keys,peers,count,mapping);
}
void closeChannelKey(){channelKeyView=false;channelKeyHex="";channelQr.clear();channelQrSize=0;}
void openChannelOptions(int id){if(id<0||id>=chatCount||deviceLocked)return;channelOptionTarget=id;channelOptionFocus=0;channelOptions=true;deleteChannelConfirm=false;closeChannelKey();messagesMenu=false;notice="";render();}
void channelOption(int index){
 if(channelOptionTarget<0||channelOptionTarget>=chatCount)return;
 if(index==0)notice=toggleChannelNotifications(channelOptionTarget)?"Notification setting saved":"Cannot save notifications";
 else if(index==1){if(chats[channelOptionTarget].direct)return;String key=channelKeys[channelOptionTarget];if(!key.length()&&!matter::channelSecret(channelOptionTarget,key)){notice="Key unavailable / radio not ready";render();return;}
  key.toLowerCase();channelKeyHex=key;channelQrSize=mm::encodeQr(mm::channelShareUrl(chats[channelOptionTarget].name.c_str(),key.c_str(),chats[channelOptionTarget].scope.c_str()),channelQr);channelKeyView=true;notice="";}
 else if(!chats[channelOptionTarget].direct&&chats[channelOptionTarget].name=="Public")notice="Public cannot be deleted";
 else{deleteChannelConfirm=true;notice="";}render();
}
void deleteSelectedChannel(){
 int id=channelOptionTarget;if(!channelOptions||!deleteChannelConfirm||id<0||id>=chatCount)return;
 if(!sdReady){notice="SD unavailable";render();return;}if(matter::busy()){notice="Radio busy / retry";render();return;}
 ChatEntry next[MAX_CHATS];String keys[MAX_CHATS],peers[MAX_CHATS];int mapping[MAX_CHATS],count=0;
 for(int i=0;i<MAX_CHATS;i++)mapping[i]=-1;
 for(int i=0;i<chatCount;i++)if(i!=id){mapping[i]=count;next[count]=chats[i];keys[count]=channelKeys[i];peers[count]=peerKeys[i];count++;}
 if(!commitChannelCatalog(next,keys,peers,count,mapping)){notice="Delete failed / check SD";render();return;}
 notice="Channel deleted";render();
}

int directConversation(const String &key,const String &name){
 for(int i=0;i<chatCount;i++)if(chats[i].direct&&peerKeys[i]==key)return i;
 if(chatCount>=MAX_CHATS)return -1;
 int id=chatCount++;chats[id]={clean(name.length()?name:"Unknown contact"),"*",true};peerKeys[id]=key;
 if(!saveBindings()){chatCount--;peerKeys[id]="";return -1;}if(!saveChats()){chatCount--;peerKeys[id]="";saveBindings();return -1;}return id;
}
void openConversation(int id,Screen parent,int parentSection=0){
 if(id<0||id>=chatCount)return;
 accentReset();
 if(screen==Screen::Chat)chatDrafts.leave(activeChat,input);
 chatParent=parent;chatParentSection=parentSection;activeChat=selected=id;composerExpanded=false;chatDrafts.take(id,input);inputCursor=-1;
 archivePager.close();screen=Screen::Chat;chatOffset=0;chatPixel=0;searching=chatDetails=messageActions=messagePath=messageBodyView=false;notice="";render();
}
// Contacts page: a companion opens (or creates) its direct chat.
void openContact(int index){auto page=matter::contacts(discoverFilter,contactQuery.c_str(),index,1);if(page.rows.empty())return;auto n=page.rows[0];if(n.type!=1){notice="Not a chat contact";render();return;}int id=directConversation(n.key,n.name);if(id<0){notice="Cannot save contact conversation";render();return;}openConversation(id,screen,section);}
void contactPage(int direction){int total=matter::contactsCount(discoverFilter,contactQuery.c_str());int last=max(0,(total-1)/mm::ContactRows*mm::ContactRows);nodeScroll=constrain(nodeScroll+direction*mm::ContactRows,0,last);contactFocus=nodeScroll;}
void incomingRadio(const matter::Received &m){
 lastReceivedSender=m.sender;lastReceivedBody=m.body;lastReceivedTime=messageTime(m.timestamp);lastReceivedChannel=m.direct?"Direct chat":m.channelId>=0&&m.channelId<chatCount?chats[m.channelId].name:m.channel;lockScreenDirty=true;
 int id=-1;if(m.direct)id=directConversation(m.key,m.sender);else if(m.channelId>=0&&m.channelId<chatCount&&!chats[m.channelId].direct&&channelReady[m.channelId])id=m.channelId;
 lastReceivedChat=id;
 if(id>=0&&unreadMessages[id]<65535)unreadMessages[id]++;
 if(id<0){notice="RX received / conversation unavailable";markRadioDirty();return;}
 matter::Record r;r.chat=id;do r.token=esp_random();while(!r.token||radioJournal.index.find(r.token));r.timestamp=m.timestamp;r.hops=m.hops;r.sender=m.sender;r.body=m.body;r.scope=m.scope;r.path=m.path;r.status="Received";lastReceivedToken=r.token;lastReceivedRead=false;receivedRepeater.receive(r.token,id,m.path.c_str(),millis());
 if(!radioJournal.append(r)){notice="RX message / SD save failed";markRadioDirty();return;}archivePager.appended(id);if(screen==Screen::Chat&&activeChat==id&&(chatOffset>0||chatPixel>0))chatOffset++;chatRowsRevision++;
 incomingBanner.receive(id,m.sender.c_str(),m.body.c_str(),millis(),deviceLocked,screen==Screen::Chat&&activeChat==id,mutedChannels&(uint32_t(1)<<id));
 device::restoreUnreadCounts(radioJournal.index,radioJournal.readTokens,unreadMessages,size_t(chatCount));
 if(deviceLocked){lockMessages.receive(lockMessageTokens(),r.token);lockMessageShownAt=millis();lockHintUntil=0;lockReadError.clear();}
 if(!(mutedChannels&(uint32_t(1)<<id)))onIncomingMessage(device::mentionsName(m.body.c_str(),matter::snapshot().config.name.c_str()));markRadioDirty();
}
void deliveredRadio(const matter::Delivery &d){if(d.txComplete&&quickReply.active&&quickReply.pending&&d.token==quickReply.txToken){quickTxReady=true;quickTxSuccess=d.txSuccess;quickTxToken=d.token;}if(!radioJournal.index.find(d.token))return;if(firstRepeater.observe(d.token,d.firstRepeater.c_str(),millis())){firstRepeaterPainted=false;}if(!(d.repeaters>=0?radioJournal.repeats(d.token,d.repeaters,d.heard):radioJournal.update(d.token,d.state)))notice="ACK received / SD save failed";markRadioDirty();}
size_t messageByteLimit(int conversation){int chat=conversation<0?activeChat:conversation;return matter::textLimit(chat>=0&&chat<chatCount&&chats[chat].direct);}
bool queueRadioMessage(String body,const String *scopeOverride,int conversation,uint32_t *queuedToken){int chat=conversation<0?activeChat:conversation;if(chat<0||chat>=chatCount){notice="Conversation unavailable";return false;}body.trim();if(!body.length()){notice="Message empty";return false;}
 if(body.length()>messageByteLimit(chat)){notice=matter::messageLimitError(messageByteLimit(chat)).c_str();return false;}
 if(!sdReady){notice="SD unavailable";return false;}
 if(chats[chat].direct&&!peerKeys[chat].length()){notice="Select a discovered contact first";return false;}
 if(!chats[chat].direct&&!channelReady[chat]){notice="Channel key required";return false;}
 if(!matter::snapshot().ready){notice="Radio offline";return false;}
 matter::Record r;do r.token=esp_random();while(!r.token||radioJournal.index.find(r.token));r.chat=chat;r.mine=true;r.sender="YOU";r.body=body;r.scope=scopeOverride?*scopeOverride:chats[chat].scope;time_t now=time(nullptr);r.timestamp=now>=MM_EPOCH_MIN?now:0;r.status="Pending send";r.repeaters=chats[chat].direct?-1:0;
 if(!radioJournal.append(r)){notice="SD save failed / not sent";return false;}archivePager.appended(chat);chatRowsRevision++;
 device::restoreUnreadCounts(radioJournal.index,radioJournal.readTokens,unreadMessages,size_t(chatCount));
 String reason;bool ok=matter::send(chat,peerKeys[chat],r.scope,body,r.token,reason);radioJournal.update(r.token,ok?reason:"Not sent: "+reason);
 if(ok){if(queuedToken)*queuedToken=r.token;if(chat==activeChat){chatOffset=0;chatPixel=0;}firstRepeater.sent(r.token,chat);}notice=reason;Serial.printf("RADIO_MESSAGE_QUEUED ok=%d\n",ok);return ok;
}
void submitRadio(){if(device::audioState().busy||!input.length())return;if(queueRadioMessage(input)){input="";inputCursor=-1;chatDrafts.clear(activeChat);}else{showChatError(notice,3000);}render();}
void shareGpsPosition(){
 auto gps=device::locationState();String body=device::gpsCoordinateMessage(gps.available,gps.enabled,gps.fix,gps.fixAgeMs,gps.latitude,gps.longitude).c_str();
 if(!gps.available)notice="GPS unavailable";
 else if(!gps.enabled)notice=device::locationEnabled(true)?"Acquiring GPS position":"GPS switch failed";
 else if(body.isEmpty())notice="Waiting for a fresh GPS fix";
 else if(queueRadioMessage(body)){positionShare=false;notice="";showChatError("Position queued",1500);}
 render();
}
bool resendBody(String &body){
 auto payload=device::resendPayload(contextMessage,contextConversation,activeChat,contextRecords());body=payload.body.c_str();return payload.valid;
}
void resendContext(){auto payload=device::resendPayload(contextMessage,contextConversation,activeChat,contextRecords());if(!payload.valid){notice=payload.error.c_str();render();return;}String scope=payload.scope.c_str();messageActions=false;if(queueRadioMessage(payload.body.c_str(),payload.scoped?&scope:nullptr))notice="Resent / "+notice;else{showChatError(notice,3000);}render();}

// localtime_r re-reads the time zone on every call (~150 us): a chat frame formats a time per message, so the last
// minutes formatted are kept (cleared every 10 minutes, so a time zone or DST change shows up).
String messageTime(uint32_t timestamp){if(timestamp<MM_EPOCH_MIN)return "--:--";
 struct Entry{uint32_t minute=0;char clock[6]="";};static Entry cache[64];static uint32_t clearedAt=0;
 if(millis()-clearedAt>600000){clearedAt=millis();for(auto &e:cache)e.minute=0;}
 uint32_t minute=timestamp/60;Entry &e=cache[minute%64];if(e.minute==minute)return e.clock;
 time_t t=timestamp;struct tm tm;localtime_r(&t,&tm);strftime(e.clock,sizeof(e.clock),"%H:%M",&tm);e.minute=minute;return e.clock;}
void renderMenu(){auto v=uiState();mm::Renderer r(frame);r.messages(v);if(!messagesMenu){int rows=chatsRows();int start=chatsStart(v.conversations);for(int i=0;i<rows&&start+i<int(v.conversations.size());i++)renderedChannels.push_back({mm::messageListRect(i,searching),v.conversations[start+i].id});}}
void renderForm(){mm::Renderer r(frame);r.form(uiState());}

uint32_t uiStateUs=0,uiStateCalls=0;// time spent building the view model (UI_STATS state_us)
// Chat list rows (Home and Chats): filtered by the search on Chats. Handlers use it instead of a whole uiState().
// Rows conversationList() returns (same search filter), without building previews: touch scrolling calls it per step.
int conversationCount(){if(!(screen==Screen::Chats&&searching&&searchQuery.length()))return chatCount;String query=searchQuery;query.toLowerCase();int n=0;for(int i=0;i<chatCount;i++){String name=chats[i].name;name.toLowerCase();if(name.indexOf(query)>=0)n++;}return n;}
std::vector<mm::Conversation> conversationList(){std::vector<mm::Conversation> out;time_t now=time(nullptr);for(int i=0;i<chatCount;i++){
  if(screen==Screen::Chats&&searching&&searchQuery.length()){String name=chats[i].name,query=searchQuery;name.toLowerCase();query.toLowerCase();if(name.indexOf(query)<0)continue;}
  mm::Conversation c;c.name=chats[i].name.c_str();c.scope=chats[i].scope.c_str();c.direct=chats[i].direct;c.id=i;c.unread=unreadMessages[i];c.preview="No messages";if(auto s=radioJournal.summary(i))if(s->total){auto &r=s->last;c.preview=r.body.c_str();c.sender=r.mine?"YOU":r.sender.c_str();c.time=messageTime(r.timestamp).c_str();c.activity=1+int(s->order);if(now>=MM_EPOCH_MIN&&r.timestamp>=MM_EPOCH_MIN&&r.timestamp<=uint32_t(now)){uint32_t minutes=(uint32_t(now)-r.timestamp)/60;c.age=minutes<60?std::to_string(minutes)+"m":minutes<1440?std::to_string(minutes/60)+"h":std::to_string(minutes/1440)+"d";}}out.push_back(c);}
 // Most recent conversation first (stable: ties keep the catalogue order), as on Home: new messages move a chat up.
 std::stable_sort(out.begin(),out.end(),[](const mm::Conversation &a,const mm::Conversation &b){return a.activity>b.activity;});
 return out;}
bool otaConfirm=false;uint32_t otaBannerUntil=0;int32_t otaAnnounced=0;std::string otaBannerText;// Updates page: first Enter on INSTALL asks, the second one installs and restarts
static std::string megabytes(uint32_t bytes){char text[16];snprintf(text,sizeof(text),"%.1f MB",bytes/1048576.0);return text;}
void fillOta(mm::State &v){
 auto o=device::otaState();using P=device::OtaPhase;v.otaConfirm=otaConfirm&&o.phase==P::Available;v.otaProgress=-1;
 switch(o.phase){
  case P::Idle:v.otaStatus=o.rolledBack?"Update rolled back":o.installed?"Updated / checking soon":"Not checked";v.otaDetail=o.rolledBack?"The new build did not start: old one restored":"Checked automatically on Wi-Fi";v.otaAction="CHECK NOW";break;
  case P::Checking:v.otaStatus="Checking...";v.otaDetail="";v.otaAction="";break;
  case P::UpToDate:v.otaStatus="Up to date";v.otaDetail="Checked "+std::to_string((millis()-o.checkedAt)/60000)+" min ago";v.otaAction="CHECK NOW";break;
  case P::Available:{std::string notes=o.notes.c_str(),item;auto add=[&]{while(!item.empty()&&item[0]==' ')item.erase(0,1);while(!item.empty()&&(item.back()==' '||item.back()=='\r'))item.pop_back();if(!item.empty())v.otaNotes.push_back(item);item.clear();};
   for(char ch:notes){if(ch==';'||ch=='\n')add();else item+=ch;}add();}
   v.otaStatus=std::string(o.version.c_str())+" available";v.otaDetail=megabytes(o.size)+", signed release";/* the notes are listed under WHAT'S NEW *//* under the Launcher the image is only saved on the SD card: the Launcher installs it */
   if(o.toSd)v.otaAction=v.otaConfirm?"ENTER AGAIN: SAVE TO SD CARD":"SAVE "+std::string(o.version.c_str())+" TO SD CARD";else v.otaAction=v.otaConfirm?"ENTER AGAIN: INSTALL + RESTART":"INSTALL "+std::string(o.version.c_str());break;
  case P::Verifying:if(o.toSd){/* written to the card, then read back: two passes */v.otaProgress=o.size?int(uint64_t(o.done)*100/(2ull*o.size)):0;v.otaStatus="Saving to SD "+std::to_string(v.otaProgress)+"%";v.otaDetail="Keep the SD card in";v.otaAction="";break;}
   [[fallthrough]];
  case P::Downloading:v.otaProgress=o.size?int(uint64_t(o.done)*100/o.size):0;v.otaStatus="Downloading "+std::to_string(v.otaProgress)+"%";v.otaDetail=megabytes(o.done)+" / "+megabytes(o.size)+", keep Wi-Fi on";v.otaAction="";break;
  case P::Restarting:v.otaStatus="Installed / restarting";v.otaDetail=o.message.c_str();v.otaAction="";break;
  case P::Error:v.otaStatus=o.message.c_str();v.otaDetail="";v.otaAction="CHECK AGAIN";break;
  case P::Saved:v.otaStatus="Saved to SD card";v.otaNotesTitle="INSTALL FROM THE LAUNCHER";v.otaNotes={"Turn the T-Deck off and on","Launcher: SD > "+std::string(o.sdFile.c_str()+1)+" > Install","Then delete the old copy; your data stays"};v.otaAction="CHECK AGAIN";break;
 }
}
// Enter / tap on the Updates button: check, then a two-step install.
void otaAction(){
 auto phase=device::otaState().phase;using P=device::OtaPhase;
 if(phase==P::Available){if(!otaConfirm){otaConfirm=true;render();return;}otaConfirm=false;notice=device::otaInstall()?"":"Install could not start";render();return;}
 if(phase!=P::Checking&&phase!=P::Downloading&&phase!=P::Verifying&&phase!=P::Restarting){otaConfirm=false;device::otaCheck();render();}// otaCheck() refuses while busy too
}
mm::State uiStateBuild();
mm::State uiState(){uint32_t t=micros();auto v=uiStateBuild();uiStateUs+=micros()-t;uiStateCalls++;return v;}
mm::State uiStateBuild(){
 auto audio=device::audioState();
 mm::State v;if(screen==Screen::Section&&section==mm::SupportSection){static String code=device::otaUnlockCode();v.unlockCode=code.c_str();v.supportTab=supportTab;}if(screen==Screen::Section&&section==17)fillOta(v);if(!deviceLocked&&screen==Screen::Section&&section==mm::LinkSection)linkFill(v);if(screen==Screen::Section&&section==14)appLinkFill(v);v.quickPending=quickReply.pending;v.quickSent=quickReply.sent;v.shutdownConfirm=shutdownConfirm;auto timeState=device::timeSyncState();v.timeSource=timeState.acquiring?timeState.message:timeState.source==device::TimeSource::WiFi?"Wi-Fi / NTP":timeState.source==device::TimeSource::GPS?"GPS":timeState.source==device::TimeSource::Manual?"Set by hand":timeState.message;if(screen==Screen::Section&&section==mm::QuickSettingsSection){auto source=device::batteryPowerSource();v.usbPower=source==device::PowerSource::USB;v.powerSourceKnown=source!=device::PowerSource::Unknown;}if(screen==Screen::Section&&section==24&&sdReady){v.storageTotal=SD.totalBytes();v.storageUsed=SD.usedBytes();}v.emojiPicker=emojiPicker;v.quickEmoji=quickEmojiPicker&&quickReply.active;v.emojiFocus=emojiFocus;v.emojiTab=emojiTab;if(emojiPicker||quickEmojiPicker){if(emojiRecent.empty())emojiItems();v.emojiRecent=emojiRecent;}v.audioReady=audio.ready;v.audioNotifications=audio.notifications;v.audioVolume=audio.volume;v.messageTone=device::RINGTONES[device::ringtoneIndex(audio.messageTone)].name;v.mentionTone=device::RINGTONES[device::ringtoneIndex(audio.mentionTone)].name;v.audioMessage=audio.message.c_str();v.unlockTapCount=max(int(unlockTaps.taps),trackballUnlockTaps);v.scopePicker=scopePicker;v.scopeEditing=scopeEditing;v.clearChatConfirm=clearChatConfirm;v.chatMenuFocus=chatMenuFocus;v.scopeFocus=scopeFocus;v.scopeOffset=scopeFocus/mm::ScopeRows*mm::ScopeRows;v.scopeInput=scopeInput.c_str();if(scopePicker&&!deviceLocked)v.regionScopes=regionScopes;v.receivedName=lastReceivedSender.c_str();v.receivedPreview=lastReceivedBody.c_str();v.receivedTime=lastReceivedTime.c_str();v.lockHasMessage=lastReceivedToken!=0;v.lockHint=lockHintUntil&&int32_t(lockHintUntil-millis())>0;v.unlockCounting=unlockCountdownAt!=0;v.quickCountdown=quickCountdownAt&&deviceLocked&&!quickReply.active?quickCountdownDots():-1;
 std::string lockRxHash;if(deviceLocked){v.lockHasMessage=false;matter::Record r;if(loadedRecord(lockMessages.current,r)&&!r.mine){v.lockHasMessage=true;v.receivedName=r.sender.c_str();v.receivedPreview=r.body.c_str();v.receivedTime=messageTime(r.timestamp).c_str();lockRxHash=device::incomingRepeater(r.path.c_str());}}
 auto repeaterLabel=[&](const std::string &hash){int matches=0;String name=matter::nodeNameForPrefix(hash.c_str(),matches,2);return matches==1&&!name.isEmpty()?mm::senderDisplayName(name.c_str()):"["+hash+"]";};auto incomingLabel=[&](){return repeaterLabel(receivedRepeater.hash);};
 if(deviceLocked&&!lockRxHash.empty()&&uint32_t(millis()-lockMessageShownAt)<device::LOCK_RX_REPEATER_MS)v.lockRepeater="Received via: "+repeaterLabel(lockRxHash);for(auto n:unreadMessages)v.unread+=n;v.status.unread=v.unread;v.cpuMHz=getCpuFrequencyMhz();v.stackFree=uxTaskGetStackHighWaterMark(nullptr);v.rasterMs=lastRasterMs;v.paintMs=lastPaintMs;v.heapKB=ESP.getFreeHeap()/1024;v.flashMB=ESP.getFlashChipSize()/1048576;v.sd=sdReady;v.touch=touchBackend;v.keyboard=keyboardReady;
 v.quickReply=quickReply.active;v.quickReplyEditable=quickReply.editable();v.quickSender=quickReply.original.sender;v.quickChannel=quickReply.original.channel;v.quickPreview=quickReply.original.body;v.quickInput=quickReply.text;v.quickError=quickReply.error;v.quickMessageLimit=quickReply.byteLimit;
 if(deviceLocked&&!lockReadError.empty())v.lockRepeater=lockReadError;
 if(deviceLocked&&v.lockRepeater.empty()&&stormLockUntil)v.lockRepeater=std::string("Storm alert: ")+stormLockText.c_str();
 v.keyboardLight=keyboardBrightness;v.displayLight=displayBrightness;v.notice=notice.c_str();v.input=input.c_str();v.inputCursor=cursorAt();v.composerExpanded=input.length()>0;v.messageLimit=messageByteLimit();v.focus=homeSelection;v.scroll=homeScroll;
 auto radio=matter::snapshot();v.status.mesh=radio.ready;v.radioReady=radio.ready;
v.radioTX=radio.tx;v.radioError=radio.error;v.radioFrequency=radio.config.frequency;v.radioBW=radio.config.bandwidth;v.radioSF=radio.config.sf;v.radioCR=radio.config.cr;v.radioPower=radio.config.power;v.radioPowerEditing=radioPowerEditing;v.radioPowerInput=radioPowerInput.c_str();v.radioName=radio.config.name.c_str();v.radioPathBytes=radio.config.pathBytes;v.radioDevicePage=radioDevicePage;v.radioDeviceFocus=radioDeviceFocus;v.radioPacketsRX=radio.received;v.radioPacketsTX=radio.sent;v.radioQueued=radio.queued;v.nodeCount=radio.nodes;v.radioFocus=radioFocus;v.nodeScroll=nodeScroll;
 if(section==2&&radioDraftLoaded){v.radioName=radioDraft.name.c_str();v.radioPathBytes=radioDraft.pathBytes;v.radioFrequency=radioDraft.frequency;v.radioBW=radioDraft.bandwidth;v.radioSF=radioDraft.sf;v.radioCR=radioDraft.cr;v.radioPower=radioDraft.power;v.radioEditField=radioEditField;v.radioPreset=radioPresetSelection;v.radioBand=radioBandSelection;v.radioDraftChanged=radioDraft.name!=radio.config.name||radioDraft.pathBytes!=radio.config.pathBytes||radioDraft.frequency!=radio.config.frequency||radioDraft.bandwidth!=radio.config.bandwidth||radioDraft.sf!=radio.config.sf||radioDraft.cr!=radio.config.cr||radioDraft.power!=radio.config.power;}
 if(!deviceLocked&&screen==Screen::Section&&section==1){auto page=matter::contacts(discoverFilter,contactQuery.c_str(),nodeScroll,mm::ContactRows);v.filteredNodeCount=page.total;v.contactLive=page.live;v.contactQuery=contactQuery.c_str();v.contactSearching=contactSearching;v.contactFocus=contactFocus;auto gps=device::locationState();bool here=gps.fix&&(gps.latitude!=0||gps.longitude!=0);for(auto &n:page.rows){mm::Node node;node.name=n.name.c_str();node.key=n.key.c_str();node.type=n.type;node.live=n.live;node.age=n.live?(millis()-n.seen)/1000:0;node.rssi=n.rssi;node.snr=n.snr;node.hops=n.hops;if(here&&(n.lat||n.lon)){double lat=n.lat/1e6,lon=n.lon/1e6;node.distance=mm::formatDistance(mm::geoDistanceMeters(gps.latitude,gps.longitude,lat,lon),mm::geoHeading(gps.latitude,gps.longitude,lat,lon));}v.nodes.push_back(node);}}
 v.caps=shiftHeld();if(accentTaps&&!accentContext())accentReset();// a tap left in a chat ends with it, whatever path left it (an emoji picker opening too)
 v.accent=accentContext()&&(accentTaps>0||accentHeld());v.accentTaps=accentTaps;v.accentMode=accentMode;v.accentFocus=accentFocus;v.symbols=keyboardModifiers.symbols();
 v.field=formField;v.newName=newName.c_str();v.newScope=newScope.c_str();v.newKey=newKey.c_str();v.newForm=newForm;v.messageMenuFocus=messageMenuFocus;
 // The manual clock editor: the welcome clock step and Settings > Date / time.
 if(screen==Screen::Welcome||(screen==Screen::Section&&section==16)){for(int i=0;i<5;i++)v.dtField[i]=dtField[i];v.dtFocus=dtFocus;}
 if(screen==Screen::Welcome){v.welcomeStep=welcomeStep;v.welcomeWifiChoice=welcomeWifiChoice;v.welcomeBand=welcomeBand;v.welcomeFocus=welcomeFocus;v.welcomeBytes=welcomeBytes;v.welcomeName=welcomeName.c_str();v.welcomeArchive=welcomeArchive.c_str();
  v.welcomeScroll=max(0,min(welcomeFocus-mm::WelcomeRows/2,int(welcomeList.size())-mm::WelcomeRows));
  for(int p:welcomeList){const auto &r=matter::RADIO_PRESETS[p];char d[48];snprintf(d,sizeof(d),"%.3f MHz / %g kHz / SF%d / CR%d",r.frequency,r.bandwidth,r.sf,r.cr);v.welcomePresets.push_back({r.name,d});}
  if(welcomeFocus>=0&&welcomeFocus<int(welcomeList.size()))v.welcomePreset=matter::RADIO_PRESETS[welcomeList[welcomeFocus]].name;}
 if(screen==Screen::PickContact){v.pickerMention=pickerMention;v.pickerSearching=pickerSearching;v.pickerQuery=pickerQuery.c_str();v.pickerFocus=pickerFocus;v.pickerTotal=pickerContacts.size();v.pickerStart=pickerFocus/mm::PickerRows*mm::PickerRows;
  for(int i=v.pickerStart;i<int(pickerContacts.size())&&i<v.pickerStart+mm::PickerRows;i++){auto &n=pickerContacts[i];std::string detail=n.key.length()==0?"wrote in this chat":n.live?"heard "+std::to_string((millis()-n.seen)/60000)+" min ago":"saved contact";/* recent senders of a Mention have no key */if(n.hops>=0)detail+=n.hops==0?" / direct":" / "+std::to_string(n.hops)+(n.hops==1?" hop":" hops");bool open=false;for(int c=0;c<chatCount;c++)if(chats[c].direct&&peerKeys[c]==n.key)open=true;if(open)detail+=" / chat";v.pickerRows.push_back({n.name.c_str(),detail});}}
 v.hideFocus=touchBrowse;v.listStart=screen==Screen::Chats&&touchBrowse?chatsTop:-1;v.searching=searching;v.messageMenu=messagesMenu;if(screen==Screen::Chats){v.focus=selected==chatCount?-1:selected;v.input=searchQuery.c_str();}
 if(screen==Screen::Chat&&!deviceLocked){
  if(receivedRepeater.chat==activeChat&&receivedRepeater.visible(millis(),device::CHAT_RX_REPEATER_MS)){v.firstRepeater="Received via: "+incomingLabel();}
  else if(firstRepeater.visible(activeChat,millis())){int matches=0;String name=matter::nodeNameForPrefix(firstRepeater.hash.c_str(),matches,2);std::string label=mm::senderDisplayName(name.c_str());if(matches!=1||label.empty())label="["+firstRepeater.hash+"]";v.firstRepeater="First RPT: "+label;if(matches>1)v.firstRepeater+=" / ambiguous";}
  else if(chats[activeChat].direct&&peerKeys[activeChat].length()){uint8_t encoded=mm::DirectPathUnknown,bytes[64];if(matter::contactPath(peerKeys[activeChat],encoded,bytes,sizeof(bytes)))v.firstRepeater=mm::directPathLine(encoded,bytes);}// kept under DM chats; First RPT / Received via take the slot for a few seconds
  v.messageBodyView=messageBodyView;v.quickPending=quickReply.pending;v.messageActions=messageActions;v.messagePath=messagePath;if(messagePath&&messageRouteView)routeCompose(v);v.pathOffset=messagePathOffset;if(messageActions||messagePath){v.contextMessage=contextSnapshot();v.contextHasLink=!v.contextMessage.mine&&!device::linkFind(v.contextMessage.body).empty();v.heardPeers=repeaterRows(v.contextMessage);}v.heardOffset=heardOffset;v.channelMuted=mutedChannels&(uint32_t(1)<<activeChat);if(messagePath)v.pathText=fullMessagePath().c_str();
  v.chatDirect=chats[activeChat].direct;v.active=activeChat;v.chatName=chats[activeChat].name.c_str();v.scope=chats[activeChat].scope.c_str();v.chatOffset=chatOffset;v.chatPixel=chatPixel;v.messageMenu=chatDetails;
  v.latestOwnMessage=-1;
  // Logical history = demo/local rows, then all SD rows. Rows hold newest-indices [rowsOffset, rowsOffset+size).
  auto summary=radioJournal.summary(activeChat);int radioTotal=summary?int(summary->total):0;
  std::vector<matter::Record> cached;// filled only for the fallback below (a frame used to copy every cached record)
  const std::vector<matter::Record> empty;const std::vector<matter::Record> *rows=nullptr;int rowsOffset=0;auto &page=archivePager.page;
  if(chatOffset>=radioTotal){rows=&empty;rowsOffset=radioTotal;}
  else if(page.ready&&page.chat==activeChat&&chatOffset>=page.offset&&chatOffset<page.offset+int(page.records.size())){rows=&page.records;rowsOffset=page.offset;}
  else{for(auto &r:radioJournal.records)if(r.chat==activeChat)cached.push_back(r);
   if(chatOffset<int(cached.size()))rows=&cached;// newest suffix while the page (re)loads
   else{v.historyError=archivePager.error;v.historyLoading=v.historyError.empty();}}
  // Plus: only a window around the shown position becomes Messages (a few newer for scrolling back down, enough
  // older for a screen); the renderer re-anchors through chatShiftOut, and the next frame moves the window.
  size_t rowFirst=0,rowEnd=rows?rows->size():0;
  if(rows&&!rows->empty()){int n=int(rows->size()),lo=max(rowsOffset,chatOffset-8),hi=min(rowsOffset+n,chatOffset+24);
   rowFirst=size_t(max(0,n-(hi-rowsOffset)));rowEnd=size_t(max(int(rowFirst),n-(lo-rowsOffset)));rowsOffset=lo;}
  if(summary&&summary->lastOwn)v.latestOwnMessage=-1;
  if(rows)for(size_t j=rowFirst;j<rowEnd;j++){const auto &r=(*rows)[j];
   String meta=r.mine?r.status:(r.hops<0?String("? hop"):String(r.hops)+" hop");if(!r.mine)meta+=" | "+r.scope;
   mm::Message message{r.mine?"YOU":r.sender.c_str(),messageTime(r.timestamp).c_str(),r.body.c_str(),meta.c_str(),r.mine};message.path=r.path.c_str();message.scope=r.scope.c_str();message.hops=r.hops;message.repeaters=r.repeaters;message.token=r.token;message.heard=r.heard.c_str();if(r.mine&&summary&&r.token==summary->lastOwn)v.latestOwnMessage=v.messages.size();v.messages.push_back(message);
  }
  v.chatOffset=rows?chatOffset-rowsOffset:0;
  v.chatTotal=radioTotal;v.chatPosition=chatOffset;
 }
 auto battery=device::batteryState();v.status.battery=battery.percent;v.status.charging=battery.charging;v.voltage=battery.voltage;v.batteryEstimated=battery.estimated;v.batteryCharging=battery.charging;v.batteryFull=battery.full;
 auto connection=device::connectivityStatus();v.status.wifi=connection.wifi;v.status.bt=connection.ble&&connection.bleReady;v.wifiEnabled=connection.wifi;v.wifiConnected=connection.connected;v.wifiConnecting=connection.connecting;v.wifiScanning=connection.scanning;v.wifiMessage=connection.message.c_str();v.wifiSSID=wifiEditing?wifiTarget.c_str():connection.ssid.c_str();v.wifiIP=connection.address.c_str();v.wifiEditing=wifiEditing;v.wifiPasswordMask=device::wifiInputDisplay(wifiPassword.c_str(),wifiShowPassword);v.wifiHidden=wifiHidden;v.wifiShowPassword=wifiShowPassword;v.wifiEditField=wifiEditField;v.wifiOffset=wifiOffset;v.wifiFocus=wifiFocus;v.bleEnabled=connection.ble;v.bleReady=connection.bleReady;
 if(!deviceLocked&&screen==Screen::Section&&section==4)for(auto &network:device::connectivityState().networks){mm::State::Network n;n.ssid=network.ssid.c_str();n.rssi=network.rssi;n.secured=network.protectedNetwork;n.saved=network.saved;v.networks.push_back(n);}
 auto weather=device::weatherState();v.weatherValid=weather.data.valid;v.weatherBusy=weather.busy;v.temperature=weather.data.temperature;v.humidity=weather.data.humidity;v.pressure=weather.data.pressure;v.wind=weather.data.wind;v.high=weather.data.high;v.low=weather.data.low;v.weatherCode=weather.data.code;v.weatherText=weather.data.valid?device::weatherDescription(weather.data.code):"No weather data";v.weatherUpdated=weather.data.stamp;v.weatherPlace=weather.place.c_str();v.weatherMessage=weather.message.c_str();v.weatherPhase=mm::WeatherPhaseView(int(weather.phase));v.weatherSource=device::weatherSourceLabel(weather.data.source);v.apparent=weather.data.apparent;v.weatherHours=weather.data.hours;for(int i=0;i<device::WeatherHours&&i<4;i++){v.weatherHourCode[i]=weather.data.hourCode[i];v.weatherHourTemp[i]=weather.data.hourTemp[i];v.weatherHourTime[i]=weather.data.hourTime[i];}
 auto location=device::locationState();v.locationValid=location.configured;v.latitude=location.latitude;v.longitude=location.longitude;v.gpsData=location.data;v.gpsNmeaFresh=location.nmeaFresh;v.gpsTimeValid=location.timeValid;v.gpsTemporary=location.timeLease&&!location.userEnabled;v.gpsUsed=location.satellitesUsed;v.gpsVisible=location.satellitesVisible;v.gpsBaud=location.baud;v.gpsFix=location.fix;v.gpsEnabled=location.enabled;v.gpsAvailable=location.available;v.status.gps=location.enabled&&location.available;v.status.acquiringTime=timeState.acquiring&&(location.enabled||connection.connected);if(timeState.acquiring&&!v.status.acquiringTime)v.timeSource="Time not synchronized";v.altitude=location.altitude;
 v.kbdQuery=kbdQuery.c_str();v.kbdSearching=kbdSearching;v.kbdFocus=kbdFocus;v.kbdLang=int(kbdLang);v.kbdMask=kbdMask;if(mm_kbd::enabledCount(kbdMask)>1)v.status.keyboard=mm_kbd::info(kbdLang).code;v.wallpaper=lockWallpaper;v.wallpaperFocus=wallpaperFocus;v.textSize=chatTextSize;v.textSizeFocus=textSizeFocus;auto power=device::powerState();v.displayLightEnabled=power.displayEnabled;v.keyboardLightEnabled=power.keyboardEnabled;v.displayTimeout=power.displaySeconds;v.keyboardTimeout=power.keyboardSeconds;v.powerField=powerField;
 time_t now=time(nullptr);if(now>=MM_EPOCH_MIN){static time_t shownMinute=-1;static char clock[6],date[30],menuDate[20];if(now/60!=shownMinute){shownMinute=now/60;struct tm tm;localtime_r(&now,&tm);strftime(clock,sizeof(clock),"%H:%M",&tm);strftime(date,sizeof(date),"%a, %b %d, %Y",&tm);strftime(menuDate,sizeof(menuDate),"%a, %b %d",&tm);}v.menuDate=menuDate;v.status.clock=clock;v.date=date;}// formatted once a minute, not every frame
 if(!deviceLocked&&(screen==Screen::Home||screen==Screen::Chats))v.conversations=conversationList();
 v.positionShare=positionShare;if(positionShare){auto gps=device::locationState();v.positionCoordinates=device::gpsCoordinateMessage(gps.available,gps.enabled,gps.fix,gps.fixAgeMs,gps.latitude,gps.longitude);v.positionMessage=!gps.available?"GPS receiver unavailable":!gps.enabled?"GPS is off. Enable it to acquire your position.":"Waiting for a fresh GPS fix...";}
 v.channelOptions=channelOptions;v.deleteChannelConfirm=deleteChannelConfirm;v.channelOptionFocus=channelOptionFocus;v.channelKeyView=channelKeyView;if(channelKeyView&&channelOptionTarget>=0&&channelOptionTarget<chatCount){v.channelKeyHex=channelKeyHex.c_str();v.channelKeyScope=chats[channelOptionTarget].scope.c_str();v.qrSize=channelQrSize;v.qrModules=channelQr;}
 if(channelOptionTarget>=0&&channelOptionTarget<chatCount){v.channelOptionName=chats[channelOptionTarget].name.c_str();v.channelOptionDirect=chats[channelOptionTarget].direct;v.channelOptionMuted=mutedChannels&(uint32_t(1)<<channelOptionTarget);}
 // The lock screen keeps `section`: do not rebuild the map view (PSRAM + tile requests) or tool state while locked.
 if(!deviceLocked&&screen==Screen::Section&&section==9)mapCompose(v);
 if(!deviceLocked&&screen==Screen::Section&&mm::toolsSection(section))toolsCompose(v);
 if(!deviceLocked&&screen==Screen::Section&&section==10)weatherTabsCompose(v);
 return v;
}
// Home fact line (Plus, no weather): nodes heard, packets, uptime, unread, then a tip; one per minute.
String homeFact(){auto radio=matter::snapshot();uint32_t minutes=millis()/60000;unsigned unread=0;for(auto n:unreadMessages)unread+=n;
 const char *tips[]={"Tap the status bar for quick settings","Hold the trackball to lock","N / M switch the emoji tabs","ENTER on the lock for Quick Reply","Trackball click opens the focused item","Themes live under Menu > Themes"};
 switch(minutes%6){case 0:return String(radio.nodes)+" nodes heard";case 1:return String(radio.received)+" packets received";case 2:return "Up "+String(minutes/60)+" h "+String(minutes%60)+" min";case 3:return unread?String(unread)+" unread messages":String("No unread messages");default:return tips[(minutes/6+minutes)%6];}}
void renderHome(){auto v=uiState();v.homeFact=homeFact().c_str();mm::Renderer renderer(frame);renderer.home(v);/* conversations come sorted by activity (conversationList) */int first=constrain(homeScroll,0,max(0,int(v.conversations.size())-mm::MenuHomeRows));for(int i=0;i<mm::MenuHomeRows&&first+i<int(v.conversations.size());i++)renderedChannels.push_back({mm::menuHomeRowRect(i),v.conversations[first+i].id});}
void renderLauncher(){auto state=uiState();state.focus=launcherSelection;mm::Renderer renderer(frame);renderer.launcher(state);}

bool saveLights(){
 if(!sdReady){notice="SD unavailable";return false;}
 SD.mkdir("/maxchat");File f=SD.open("/maxchat/lights.tmp",FILE_WRITE);
 String row=String(keyboardBrightness)+" "+String(displayBrightness)+" 0 0\n";// keyboard, display; the last two fields (vibration on, level) are kept at 0 for the file format
 bool saved=f&&f.print(row)==row.length();if(f){f.flush();f.close();}
 saved=saved&&device::sdReplace(SD,"/maxchat/lights.tmp","/maxchat/lights.conf","/maxchat/lights.bak",false);if(!saved)SD.remove("/maxchat/lights.tmp");
 notice=saved?"Settings saved":"Save failed";return saved;
}
void loadLights(){
 if(!sdReady)return;File f=SD.open(device::sdSavedPath(SD,"/maxchat/lights.conf","/maxchat/lights.bak"));if(!f)return;
 String v=f.readStringUntil('\n');int kb,ep,vibe=1,level=75;
 if(sscanf(v.c_str(),"%d %d %d %d",&kb,&ep,&vibe,&level)>=2&&kb>=0&&kb<=255&&ep>=0&&ep<=255){keyboardBrightness=kb;displayBrightness=ep;}
 f.close();
}
mm::Page sectionPage(int id){
 switch(id){case mm::LinkSection:return mm::Page::Link;case 1:return mm::Page::Mesh;case 2:return mm::Page::Radio;case 3:return mm::Page::GPS;case 4:return mm::Page::Wifi;case 5:return mm::Page::Battery;case 6:return mm::Page::Controls;case 7:return mm::Page::Files;case 8:return mm::Page::Info;case 9:return mm::Page::Maps;case 10:return mm::Page::Weather;case 11:return mm::Page::Terminal;case 14:return mm::Page::Bluetooth;case 16:return mm::Page::DateTime;case 17:return mm::Page::Updates;case mm::QuickSettingsSection:return mm::Page::QuickSettings;case 19:return mm::Page::Settings;case 20:return mm::Page::Power;case 21:return mm::Page::Audio;case 22:return mm::Page::Wallpaper;case mm::KeyboardSection:return mm::Page::Keyboard;case 24:return mm::Page::Storage;case mm::TextSizeSection:return mm::Page::TextSize;case mm::AccentKeySection:return mm::Page::AccentKey;case mm::CliSection:return mm::Page::Cli;case mm::SupportSection:return mm::Page::Support;default:return mm::Page::Info;}
}
void renderSection(){
 auto v=uiState();v.page=sectionPage(section);v.filter=discoverFilter;v.settingsFocus=settingsFocus;v.settingsOffset=settingsScroll;
 v.version=MM_RELEASE_LABEL;v.build=__DATE__;v.commit=MM_GIT_COMMIT;// version e.g. "ALPHA 0.3.0.7"
 if(section==7){v.scroll=filesScroll;if(sdReady){static std::vector<std::string> cache;static uint32_t cachedAt=0;if(cache.empty()||uint32_t(millis()-cachedAt)>=5000){cache.clear();File dir=SD.open("/");while(cache.size()<64){File f=dir.openNextFile();if(!f)break;cache.push_back(std::string(f.isDirectory()?"[DIR] ":"")+f.name());f.close();}dir.close();cachedAt=millis();}v.files=cache;}filesCount=v.files.size();}
 mm::Renderer renderer(frame);renderer.draw(v);
}

void confirmShutdown(){
 if(!shutdownConfirm)return;auto source=device::batteryPowerSource();if(source!=device::PowerSource::Battery){notice=source==device::PowerSource::USB?"Unplug USB to power off":"Power controller unavailable";render();return;}
 device::audioCancel();device::powerLocked(true);setTouchEnabled(false);frame.fillScreen(mm::BG);mm::Components c(frame);c.centered(8,145,224,"POWER OFF",mm::Font::ChatTitle);
 lcd.present(frame.getBuffer());

 if(!device::batteryShutdown()){device::powerLocked(false);setTouchEnabled(true);notice="Power off failed";render();return;}delay(200);device::powerLocked(false);setTouchEnabled(true);notice="Power cut not completed";render();
}
void loadRadioDraft(bool keepSelection);
void openSection(int id,bool paint){
 
 if(id==mm::SupportSection)supportTab=0;// Settings > Donate / Unlock opens on Donate
 shutdownConfirm=otaConfirm=false;emojiPicker=false;scopePicker=scopeEditing=clearChatConfirm=channelOptions=deleteChannelConfirm=positionShare=false;
 if(navigation.size()<16)navigation.push_back({screen,section});
 section=constrain(id,0,mm::MaxSection);notice="";if(section==1){contactSearching=false;contactQuery="";contactFocus=nodeScroll=0;}if(section==22)wallpaperFocus=lockWallpaper;if(section==mm::KeyboardSection){kbdQuery="";kbdSearching=false;kbdFocus=0;}if(section==mm::TextSizeSection)textSizeFocus=chatTextSize;if(section==mm::AccentKeySection)accentFocus=accentMode;if(section==10)weatherTab=0;if(section==16){dtLoad();dtFocus=0;}if(section==9)mapOpen();if(mm::toolsSection(section))toolsOpen(section);
 radioPowerEditing=false;radioDevicePage=false;radioDeviceFocus=0;if(section==2)loadRadioDraft(false);
 if(section==0){searching=false;searchQuery="";messagesMenu=false;chatsParent=screen==Screen::Launcher?Screen::Launcher:Screen::Home;screen=Screen::Chats;}
 else {sectionParent=screen==Screen::Launcher?Screen::Launcher:Screen::Home;screen=Screen::Section;}
 if(paint)render();
}
// Unlocked idle policy: nothing to draw, no input for 3 s, no radio/audio/network work -> 80 MHz and a 20 ms loop; any activity or frame brings 240 MHz back.
bool idleCpuLoop(bool audioBusy){if(deviceLocked)return false;bool busy=dirty||radioDirty||quickReply.active||device::networkJobActive()||deleteHeld||touchPipeline.contactActive()||matter::busy()||audioBusy||uint32_t(millis()-device::powerState().lastActivity)<3000||uint32_t(millis()-lastPhysicalPaint)<1500;uint32_t want=busy?device::NORMAL_CPU_FREQUENCY:80;if(getCpuFrequencyMhz()!=want)setCpuFrequencyMhz(want);return !busy;}
void render(bool partialHint,int batteryPreview) {
 if(renderHold){renderPending=true;return;}// touchScroll: one frame for all the rows of a gesture step
 {static int seenScreen=-1,seenSection=-1;if(int(screen)!=seenScreen||section!=seenSection){seenScreen=int(screen);seenSection=section;touchBrowse=false;}}// touch browsing ends with the page
 if(!deviceLocked&&screen==Screen::Chat&&sdReady)archivePager.ensure(radioJournal,activeChat,chatOffset);
 if(screen!=Screen::Chat&&input.length())chatDrafts.leave(activeChat,input);if(!input.length())composerExpanded=false;
 if(batchingInput){repaintNow();return;}
 // Notification feedback (keyboard flash) keeps running from the display busy callback, so the frame is painted together with it.
 if(!deviceLocked&&getCpuFrequencyMhz()<device::NORMAL_CPU_FREQUENCY)setCpuFrequencyMhz(device::NORMAL_CPU_FREQUENCY); // raster at full clock
 uint32_t frameScene=touchScene();int frameChat=screen==Screen::Chat&&!deviceLocked?activeChat:-1;renderedChatMessages.clear();bool frameConfirmation=deviceLocked&&quickReply.active&&quickReply.sent;uint32_t frameLockToken=deviceLocked?lockMessages.current:0;uint32_t renderStarted=millis();renderCalls++;paintedReadChat=-1;renderedChannels.clear();
 (void)partialHint; // Dirty-region planner selects the refresh strategy.
 frame.fillScreen(mm::BG);frame.setTextWrap(false);frame.setTextSize(1);
 if(deviceLocked){mm::Renderer renderer(frame);renderer.lock(uiState());}
 else if(screen==Screen::Home)renderHome();
 else if(screen==Screen::Launcher)renderLauncher();
 else if(screen==Screen::Section)renderSection();
 else if(screen==Screen::Chats)renderMenu();
 else if(screen==Screen::NewChat)renderForm();
 else if(screen==Screen::PickContact){mm::Renderer r(frame);auto v=uiState();v.page=mm::Page::ContactPicker;r.draw(v);}
 else if(screen==Screen::Welcome){mm::Renderer r(frame);r.welcome(uiState());}
 else renderChat();
 if(!deviceLocked&&channelOptions){mm::Renderer renderer(frame);renderer.channelOptions(uiState());}
 if(!deviceLocked){mm::Renderer renderer(frame);
  if(lockKey.progress)renderer.holdBanner("Lock device",lockKey.progress,min(20,int((millis()-lockKey.started)/100)));
  else if(stormBannerUntil&&int32_t(stormBannerUntil-millis())>0)renderer.holdBanner(stormBanner.c_str(),6,20,true,mm::Icon::Weather);
  else if(otaBannerUntil&&int32_t(otaBannerUntil-millis())>0)renderer.messageBanner("Update available",otaBannerText);
  else if(screen==Screen::Chat&&chatErrorUntil&&int32_t(chatErrorUntil-millis())>0){frame.fillRect(0,mm::StatusH,mm::W,mm::HeaderH,mm::BG);mm::Components c(frame);c.text(mm::Margin,mm::StatusH+9,chatError.c_str(),mm::Font::ChatMeta,mm::FG,mm::W-2*mm::Margin);}
  else if(incomingBanner.active&&!(screen==Screen::Chat&&activeChat==incomingBanner.chat))renderer.messageBanner(incomingBanner.sender,incomingBanner.body);
 }
 // USB-only single-frame preview: never changes battery state or calibration.
 if(batteryPreview>=-1&&batteryPreview<=100){auto status=uiState().status;status.battery=batteryPreview;mm::Components components(frame);components.statusBar(status);}
 if(deviceLocked||screen!=Screen::Chat)firstRepeaterPainted=false;
 int paintPage=deviceLocked?6:int(screen);
 lastRasterMs=millis()-renderStarted;
 // LCD: no refresh planning, only the changed rows go to the panel.
 struct {bool full=false,changed=false;} plan;plan.full=paintedScreen<0||forceFullRefresh;forceFullRefresh=false;composerUrgent=false;
 panelPainting=true;touchTransition=frameScene!=visibleTouchScene;plan.changed=!plan.full&&!deviceLocked&&paintedScreen==paintPage&&paintedSection==section&&screen!=Screen::Chat?lcd.presentScrolling(frame.getBuffer()):lcd.present(frame.getBuffer(),plan.full);/* the chat scrolls pixel by pixel: no eased frames *//* same page: a vertical move is animated */panelPainting=false;if(plan.changed)lastPhysicalPaint=millis();
 paintedChatMessages.swap(renderedChatMessages);paintedChatConversation=frameChat;
 if(frameConfirmation)quickReply.didPaint(millis());
 if(deviceLocked&&!quickReply.active)paintedLockToken=frameLockToken;if(!deviceLocked)commitReadFrame();lastPaintMs=millis()-renderStarted;if(deviceLocked){renderedUnlockTaps=unlockTaps.taps;lockHintPainted=lockHintUntil&&int32_t(lockHintUntil-millis())>0;}paintedScreen=paintPage;paintedSection=section;dirty=false;lastPaint=millis();visibleTouchScene=frameScene;if(!touchDisplayReady)touchPipeline.waitForRelease();touchDisplayReady=true;touchTransition=false;paintedChannels.swap(renderedChannels);
 if(!appUsbLinked()&&Serial.availableForWrite()>=64)Serial.printf("UI_READY screen=%d active=%d redraw=%d full=%d\n",int(screen),activeChat,plan.changed,plan.full);

}
// Fresh 16-byte channel secret from the hardware RNG (same source MeshCore uses for identities).
String randomChannelSecret(){uint8_t secret[mm::ChannelSecretBytes];do esp_fill_random(secret,sizeof(secret));while(mm::zeroSecret(mm::channelSecretHex(secret)));return mm::channelSecretHex(secret).c_str();}
void refreshPicker(){
 pickerContacts.clear();
 // Mention: people who wrote in this chat lately come first (they may not be saved contacts); repeaters never.
 std::vector<String> recent;if(pickerMention)for(auto it=radioJournal.records.rbegin();it!=radioJournal.records.rend()&&recent.size()<12;++it)if(it->chat==activeChat&&!it->mine&&it->sender.length()&&std::find(recent.begin(),recent.end(),it->sender)==recent.end()&&mm::contactMatches(it->sender.c_str(),pickerQuery.c_str()))recent.push_back(it->sender);
 for(auto &n:matter::nodes(1,0,256))if(n.type==1&&mm::contactMatches(n.name.c_str(),pickerQuery.c_str())&&std::find(recent.begin(),recent.end(),n.name)==recent.end())pickerContacts.push_back(n);
 std::stable_sort(pickerContacts.begin(),pickerContacts.end(),[](const matter::Node &a,const matter::Node &b){if(a.live!=b.live)return a.live;if(a.live&&a.seen!=b.seen)return int32_t(a.seen-b.seen)>0;return strcasecmp(a.name.c_str(),b.name.c_str())<0;});
 if(!recent.empty()){std::vector<matter::Node> people;for(auto &name:recent){matter::Node n;n.name=name;n.type=1;n.live=true;n.seen=0;people.push_back(n);}pickerContacts.insert(pickerContacts.begin(),people.begin(),people.end());}
 pickerFocus=constrain(pickerFocus,0,max(0,int(pickerContacts.size())-1));
}
void openMentionPicker(){pickerMention=true;screen=Screen::PickContact;pickerSearching=true;pickerQuery="";pickerFocus=0;notice="";refreshPicker();render();}
void openContactPicker(){pickerMention=false;messagesMenu=false;searching=false;screen=Screen::PickContact;pickerSearching=false;pickerQuery="";pickerFocus=0;notice="";refreshPicker();render();}
// Existing direct chat with that contact is reused; otherwise a new one is saved, then opened (empty chat shows "Start a new conversation").
void pickContact(int index){if(index<0||index>=int(pickerContacts.size()))return;auto n=pickerContacts[index];
 if(pickerMention){pickerMention=false;pickerSearching=false;pickerQuery="";pickerContacts.clear();screen=Screen::Chat;String tag=String("@[")+n.name+"] ";if(input.length()+tag.length()<=messageByteLimit())inputInsert(tag);render();return;}int id=directConversation(n.key,n.name);if(id<0){notice=chatCount>=MAX_CHATS?String("Limit: ")+MAX_CHATS+" conversations":String("Cannot save conversation");render();return;}pickerSearching=false;pickerQuery="";pickerContacts.clear();Serial.printf("CONTACT_PICKED id=%d\n",id);openConversation(id,Screen::Chats);}
void beginNewChannel(int form);
void messagesMenuChoose(int row){if(row==0)openContactPicker();else beginNewChannel(row-1);}
void openMessagesMenu(){searching=false;messagesMenu=true;messageMenuFocus=0;render();}
void beginNewChannel(int form){
 messagesMenu=false;if(chatCount>=MAX_CHATS){notice=String("Limit: ")+MAX_CHATS+" conversations";render();return;}
 searching=false;screen=Screen::NewChat;newForm=constrain(form,0,mm::ChannelFormCount-1);formField=0;newName="";newScope="*";newKey=newForm==int(mm::ChannelForm::CreatePrivate)?randomChannelSecret():String();notice="";render();
}
// Validate the open form and append the channel: bindings first (rows past chatCount are ignored on load), then the catalog.
void saveNewChannel(){
 const auto form=mm::ChannelForm(newForm);const bool hashtag=form==mm::ChannelForm::JoinHashtag;
 String name=hashtag?"#"+newName:String(mm::cleanChannelName(newName.c_str()).c_str()),scope=clean(newScope),key=hashtag?String():newKey;
 if(!scope.length())scope="*";
 if(hashtag?!mm::validHashtag(newName.c_str()):!name.length()){formField=0;notice=hashtag?"Use a-z, 0-9 and '-'":"Enter a channel name";render();return;}
 if(!hashtag&&!mm::validSecret(key.c_str())){formField=1;notice="Secret key: 32 hex characters";render();return;}
 if(!hashtag&&mm::zeroSecret(key.c_str())){formField=1;notice="Secret key cannot be all zero";render();return;}
 if(name=="Public"){formField=0;notice="Name reserved / pick another";render();return;}
 for(int i=0;i<chatCount;i++)if(!chats[i].direct){if(chats[i].name==name){formField=0;notice="Channel already exists";render();return;}if(!hashtag&&channelKeys[i]==key){formField=1;notice="Key already used by "+chats[i].name;render();return;}}
 if(chatCount>=MAX_CHATS){notice=String("Limit: ")+MAX_CHATS+" conversations";render();return;}
 if(!sdReady){notice="SD unavailable";render();return;}if(matter::busy()){notice="Radio busy / retry";render();return;}
 int id=chatCount;chats[id]={name,scope,false};channelKeys[id]=key;peerKeys[id]="";unreadMessages[id]=0;chatDrafts.saved[id]="";mutedChannels&=~(uint32_t(1)<<id);chatCount++;
 if(!saveBindings()){chatCount--;channelKeys[id]="";notice="SD save failed";render();return;}
 if(!saveChats()){chatCount--;channelKeys[id]="";saveBindings();notice="SD save failed";render();return;}
 configureChannels();newKey="";newName="";selected=id;screen=Screen::Chats;
 notice=channelReady[id]?(form==mm::ChannelForm::CreatePrivate?"Channel created":"Channel joined"):"Saved / radio not ready";
 Serial.printf("CHANNEL_SAVED id=%d form=%d ready=%d\n",id,newForm,channelReady[id]);render();
}
static bool presetMatchesConfig(int index,const matter::Config &cfg){if(index<0||index>=matter::RADIO_PRESET_COUNT)return false;auto &p=matter::RADIO_PRESETS[index];return fabsf(cfg.frequency-p.frequency)<0.0004f&&cfg.bandwidth==p.bandwidth&&cfg.sf==p.sf&&cfg.cr==p.cr;}
void loadRadioDraft(bool keepSelection=false){
 radioDraft=matter::snapshot().config;radioDraftLoaded=true;radioPowerEditing=false;radioPowerInput="";radioFocus=0;radioBandSelection=matter::radioBand(radioDraft.frequency);
 if(keepSelection&&presetMatchesConfig(radioPresetSelection,radioDraft))return;
 radioPresetSelection=matter::matchPreset(radioDraft.frequency,radioDraft.bandwidth,radioDraft.sf,radioDraft.cr);
 String name=device::prefGetString("mmlora","preset");for(int i=0;name.length()&&i<matter::RADIO_PRESET_COUNT;i++)if(name==matter::RADIO_PRESETS[i].name&&presetMatchesConfig(i,radioDraft)){radioPresetSelection=i;break;}
}
void selectRadioPreset(int index){
 if(index<0||index>=matter::RADIO_PRESET_COUNT)return;
 auto &p=matter::RADIO_PRESETS[index];radioPresetSelection=index;radioBandSelection=matter::radioBand(p.frequency);
 radioDraft.frequency=p.frequency;radioDraft.bandwidth=p.bandwidth;radioDraft.sf=p.sf;radioDraft.cr=p.cr;
 notice="Preview / APPLY to radio";render();
}
void cycleRadioPreset(int direction){selectRadioPreset(matter::nextRadioPreset(radioPresetSelection,radioBandSelection,direction));}
void selectRadioBand(int band){if(band!=radioBandSelection){radioBandSelection=band;selectRadioPreset(matter::nextRadioPreset(-1,band,1));}}
void editRadioField(int field){
 if(field<0||field>CompanionNameField)return;radioEditField=field;
 radioPowerInput=field==CompanionNameField?radioDraft.name:field==matter::Frequency?String(radioDraft.frequency,3):field==matter::Bandwidth?String(radioDraft.bandwidth,2):field==matter::SpreadFactor?String(radioDraft.sf):field==matter::CodingRate?String(radioDraft.cr):String(radioDraft.power);
 radioPowerEditing=true;notice="";render();
}
void editRadioPower(){editRadioField(matter::TxPower);}
void saveRadioPower(){
 if(radioEditField==CompanionNameField){String name=radioPowerInput;name.trim();if(!matter::validCompanionName(name.c_str())){notice="Name required / max 31 bytes";render();return;}radioDraft.name=name;radioPowerEditing=false;notice="Edits ready / press APPLY";render();return;}
 float value;if(!matter::parseRadioField(radioEditField,radioPowerInput.c_str(),value)){notice="Invalid value / check range";render();return;}
 switch(radioEditField){case matter::Frequency:radioDraft.frequency=value;break;case matter::Bandwidth:radioDraft.bandwidth=value;break;case matter::SpreadFactor:radioDraft.sf=value;break;case matter::CodingRate:radioDraft.cr=value;break;case matter::TxPower:radioDraft.power=value;break;}
 if(!presetMatchesConfig(radioPresetSelection,radioDraft))radioPresetSelection=matter::matchPreset(radioDraft.frequency,radioDraft.bandwidth,radioDraft.sf,radioDraft.cr);
 radioBandSelection=matter::radioBand(radioDraft.frequency);radioPowerEditing=false;notice="Edits ready / press APPLY";render();
}
void applyRadioDraft(){
 auto cfg=matter::snapshot().config;cfg.frequency=radioDraft.frequency;cfg.bandwidth=radioDraft.bandwidth;cfg.sf=radioDraft.sf;cfg.cr=radioDraft.cr;cfg.power=radioDraft.power;cfg.name=radioDraft.name;cfg.pathBytes=radioDraft.pathBytes;
 if(matter::configure(cfg,notice)){
  bool labelSaved=device::prefPutString("mmlora","preset",radioPresetSelection<0?String():String(matter::RADIO_PRESETS[radioPresetSelection].name));
  loadRadioDraft(true);notice=labelSaved?"LoRa settings saved":"Radio saved / label not saved";
 }
 render();
}
void selectLoRaTab(bool device,bool focusTab=false){radioDevicePage=device;radioFocus=device?9:8;radioDeviceFocus=focusTab?(device?6:5):0;notice="";render();}
void activateRadioFocus(){if(radioFocus==0)cycleRadioPreset(1);else if(radioFocus<=5)editRadioField(radioFocus-1);else if(radioFocus==6)applyRadioDraft();else if(radioFocus>=8)selectLoRaTab(radioFocus==9);else{matter::advertise(notice);render();}}

void back() {
 accentReset();
 if(deviceLocked){if(quickReply.active)closeQuickReply();return;}
 if(screen==Screen::Section&&section==mm::KeyboardSection&&kbdSearching){kbdSearching=false;kbdQuery="";kbdFocus=0;render();return;}
 if(screen==Screen::Section&&mm::toolsSection(section)&&toolsBack())return;
 if(screen==Screen::Section&&section==mm::LinkSection&&linkBack())return;// BACK walks the pages opened from links first
 if(channelOptions){if(channelKeyView)closeChannelKey();else if(deleteChannelConfirm)deleteChannelConfirm=false;else{channelOptions=false;channelOptionTarget=-1;}notice="";render();return;}
 if(screen==Screen::Home)return;
 if(screen==Screen::Chat&&positionShare){positionShare=false;notice="";render();return;}
 if(screen==Screen::Chat&&scopeEditing){scopeEditing=false;scopeInput="";notice="";render();return;}
 if(screen==Screen::Chat&&(scopePicker||clearChatConfirm)){scopePicker=clearChatConfirm=false;notice="";render();return;}
 if(shutdownConfirm){shutdownConfirm=false;notice="";render();return;}
 if(screen==Screen::Section&&section==4&&wifiEditing){wifiEditing=false;wifiShowPassword=false;wifiPassword="";notice="";render();return;}
 if(screen==Screen::Section&&section==14&&bleNameEditing){bleNameEditing=false;bleNameInput="";notice="";render();return;}
 if(screen==Screen::Chat&&emojiPicker){emojiPicker=false;render();return;}
 if(screen==Screen::Section&&section==2&&radioPowerEditing){radioPowerEditing=false;notice="";render();return;}
 if(screen==Screen::Section&&section==2&&radioDevicePage){selectLoRaTab(false);return;}
 if(screen==Screen::Chat&&(messageActions||messagePath)){if(messageRouteView)routeClose();messageActions=messagePath=messageRouteView=false;render();return;}
 if(screen==Screen::Chat&&chatDetails){chatDetails=false;render();return;}
 if(screen==Screen::Chats&&searching){searching=false;searchQuery="";render();return;}
 if(screen==Screen::PickContact&&pickerSearching){pickerSearching=false;render();return;}
 if(screen==Screen::Section&&section==1&&contactSearching){contactSearching=false;contactQuery="";contactFocus=nodeScroll=0;render();return;}
 if(screen==Screen::Chats&&messagesMenu){messagesMenu=false;render();return;}
 messagesMenu=false;
 chatDetails=false;
 if(screen==Screen::Chat){chatDrafts.leave(activeChat,input);screen=chatParent;if(screen==Screen::Section)section=chatParentSection;}
 else if(screen==Screen::NewChat){newKey="";screen=Screen::Chats;}
 else if(screen==Screen::PickContact){pickerContacts.clear();pickerQuery="";if(pickerMention){pickerMention=false;screen=Screen::Chat;if(input.length()<messageByteLimit())inputInsert("@");}else screen=Screen::Chats;}// a cancelled mention keeps the typed "@"
 else if((screen==Screen::Chats||screen==Screen::Section)&&!navigation.empty()){auto n=navigation.back();navigation.pop_back();screen=n.screen;section=n.section;}
 else if(screen==Screen::Chats)screen=chatsParent;
 else if(screen==Screen::Section)screen=sectionParent;
 else screen=Screen::Home;
 input="";inputCursor=-1;wifiPassword="";wifiEditing=false;notice="";selected=activeChat;render();
}
void selectWifi(int index){auto networks=device::connectivityState().networks;if(index<0||index>=int(networks.size()))return;wifiFocus=index;wifiTarget=networks[index].ssid;wifiPassword="";wifiHidden=false;wifiShowPassword=false;wifiEditField=1;String known;if(networks[index].protectedNetwork&&!device::knownNetwork(wifiTarget,known))wifiEditing=true;else{device::connect(wifiTarget,known);notice="Connecting...";}/* a network joined before connects with its saved password */for(unsigned i=0;i<known.length();i++)known.setCharAt(i,0);render();}
void hiddenWifi(){wifiEditing=wifiHidden=true;wifiShowPassword=false;wifiEditField=0;wifiTarget=wifiPassword="";notice="";render();}
void connectWifi(){if(device::connect(wifiTarget,wifiPassword)){wifiPassword="";wifiEditing=false;wifiShowPassword=false;notice="Connecting...";}else notice="Check SSID/password";render();}
static void eraseLastCodepoint(String &text){if(!text.length())return;int at=int(mm::utf8PrevStart(text,text.length()));text.remove(at);}
String *editableField(){
 if(deviceLocked||channelOptions)return nullptr;
 if(screen==Screen::Chat){if(scopeEditing)return &scopeInput;if(scopePicker||clearChatConfirm||positionShare||messageActions||messagePath||chatDetails||emojiPicker)return nullptr;return &input;}
 if(screen==Screen::Chats&&searching)return &searchQuery;
 if(screen==Screen::Section&&section==mm::KeyboardSection&&kbdSearching)return &kbdQuery;
 if(screen==Screen::PickContact&&pickerSearching)return &pickerQuery;
 if(screen==Screen::Welcome&&welcomeStep==4)return &welcomeName;
 if(screen==Screen::Section&&section==1&&contactSearching)return &contactQuery;
 if(screen==Screen::Section&&section==14&&bleNameEditing)return &bleNameInput;
 if(screen==Screen::NewChat){if(formField==1)return newForm==int(mm::ChannelForm::JoinPrivate)?&newKey:nullptr;return formField==0?&newName:&newScope;}
 if(screen==Screen::Section){if(mm::toolsSection(section))return toolsField();if(section==2&&radioPowerEditing)return &radioPowerInput;if(section==4&&wifiEditing)return wifiEditField==0?&wifiTarget:&wifiPassword;}
 return nullptr;
}
#include "map_app.inc"
#include "cli_app.inc"
// The tools hooks: the CLI.
void toolsOpen(int id){notice="";if(id==mm::CliSection)cliOpen();}
void toolsClose(int id){if(id==mm::CliSection)cliClose();}
void toolsCompose(mm::State &v){cliCompose(v);}
bool toolsKey(char c){if(section==mm::CliSection)return cliKey(c);if(c=='a'){back();return true;}return false;}
bool toolsTap(int x,int y){return section==mm::CliSection&&cliTap(x,y);}
bool toolsBack(){return false;}
void toolsScroll(int dy){if(section==mm::CliSection)cliScroll(dy);render();}
String *toolsField(){return section==mm::CliSection&&!cliLogging?&cliMirror:nullptr;}
void toolsTick(){cliTick();}
#include "weather_apps.inc"
#include "companion_app.inc"
#include "link_app.inc"
// Apps are left in many ways (Back, Home, lock, app grid, serial "home"/"section"):
// one visibility tracker runs the close hook so nothing keeps working or holding RAM
// after an app disappears from the screen. Weather does the same via weatherSetOpen.
int visibleApp=-1,visibleScene=-1;
// Chat: the SD history page (24 records), the packet-route picture and the channel QR are
// rebuilt on the next open, so they are released as soon as the chat leaves the screen.
void chatLeft(){composerExpanded=false;messageActions=messagePath=messageBodyView=chatDetails=false;archivePager.close();routeClose();messageRouteView=false;if(channelKeyView)closeChannelKey();contextMessage=mm::Message();}
void pickerLeft(){pickerContacts.clear();pickerContacts.shrink_to_fit();pickerQuery="";pickerSearching=false;}
void formLeft(){wipeString(newKey);newName="";}
void wifiPageLeft(){wipeString(wifiPassword);wifiTarget="";wifiEditing=wifiShowPassword=wifiHidden=false;}
void startBleNameEdit(){bleNameEditing=true;bleNameInput=appBleCustomName;notice="";render();}
void saveBleName(){if(appSetBleName(bleNameInput)){bleNameEditing=false;bleNameInput="";notice="Bluetooth name saved";}else notice=String("Max ")+device::BLE_NAME_MAX_BYTES+" letters, digits, spaces";render();}
void appVisibilityLoop(){
 int now=!deviceLocked&&screen==Screen::Section?section:-1;
 if(now!=visibleApp){int previous=visibleApp;visibleApp=now;
  if(previous==9)mapClose();else if(previous==mm::LinkSection){if(!(deviceLocked&&screen==Screen::Section&&section==mm::LinkSection))linkLeft();}/* a lock keeps the page */else if(previous==4)wifiPageLeft();else if(previous==14){bleNameEditing=false;bleNameInput="";}else if(mm::toolsSection(previous))toolsClose(previous);
  }
 int scene=deviceLocked?-1:int(screen);
 if(scene!=visibleScene){int previous=visibleScene;visibleScene=scene;
  if(previous==int(Screen::Chat))chatLeft();else if(previous==int(Screen::PickContact))pickerLeft();else if(previous==int(Screen::NewChat))formLeft();}
}
// Per-emoji use counts, persisted in NVS ("mmemoji"); tab 0 shows the most used first.
static uint16_t emojiCounts[mm_emoji_picker::count]={};static bool emojiCountsLoaded=false;
static void loadEmojiCounts(){if(emojiCountsLoaded)return;emojiCountsLoaded=true;Preferences p;if(p.begin("mmemoji",true)){if(p.getUShort("n",0)==mm_emoji_picker::count&&p.getBytesLength("counts")==sizeof(emojiCounts))p.getBytes("counts",emojiCounts,sizeof(emojiCounts));p.end();}}
static void saveEmojiCounts(){Preferences p;if(p.begin("mmemoji",false)){p.putUShort("n",mm_emoji_picker::count);p.putBytes("counts",emojiCounts,sizeof(emojiCounts));p.end();}}
static std::vector<int> emojiItems(){if(emojiTab==0){loadEmojiCounts();emojiRecent=mm::frequentEmoji(emojiCounts);return emojiRecent;}std::vector<int> items;auto &cat=mm_emoji_picker::categories[emojiTab-1];for(int i=0;i<cat.count;i++)items.push_back(cat.first+i);return items;}
static void openEmojiPicker(){messageActions=messagePath=chatDetails=false;emojiPicker=true;emojiTab=0;emojiFocus=0;if(emojiItems().empty())emojiTab=1;}// nothing used yet: first category
static void emojiSwitchTab(int delta){emojiTab=(emojiTab+delta+mm::EmojiTabs)%mm::EmojiTabs;emojiFocus=0;}
// Insert the focused glyph, count the use, close the picker.
static void insertEmoji(int item){if(deviceLocked)quickReplyInputAt=millis();auto items=emojiItems();if(item<0||item>=int(items.size()))return;int index=items[item];String glyph=mm_emoji_picker::glyphs[index].text;
 if(quickEmojiPicker&&quickReply.active){// quick reply on the lock screen
  size_t before=quickReply.text.size();quickReply.appendText(glyph.c_str());if(quickReply.text.size()!=before){mm::countEmoji(emojiCounts,index);saveEmojiCounts();}
  quickEmojiPicker=false;lockScreenDirty=true;render(true);return;}
 if(input.length()+glyph.length()<=messageByteLimit()){inputInsert(glyph);notice="";mm::countEmoji(emojiCounts,index);saveEmojiCounts();}
 else notice=matter::messageLimitError(messageByteLimit()).c_str();emojiPicker=false;render();}
// Quick reply emoji picker: same keys as in a chat (A D / J K move, W S rows, N M tabs, ENTER adds, BACK closes).
// Emoji picker keys shared by the chat and the quick reply: A D / J K move, W S rows, N M tabs. Returns 2 for
// ENTER (insert), 1 when the key moved the focus or the tab, 0 for any other key.
static int emojiPickerKey(char c){int count=emojiItems().size();
 if(c=='a'||c=='k')emojiFocus=(emojiFocus+count-1)%max(1,count);else if(c=='d'||c=='j')emojiFocus=(emojiFocus+1)%max(1,count);else if(c=='w')emojiFocus=max(0,emojiFocus-mm::EmojiCols);else if(c=='s')emojiFocus=min(count-1,emojiFocus+mm::EmojiCols);
 else if(c=='m'||c=='\t')emojiSwitchTab(1);else if(c=='n')emojiSwitchTab(-1);else if(c=='\n')return 2;else return 0;return 1;}
static void quickEmojiKey(char c){int k=emojiPickerKey(c);if(k==2){insertEmoji(emojiFocus);return;}if(!k&&(c==3||c=='\b'))quickEmojiPicker=false;lockScreenDirty=true;render(true);}
static void quickEmojiOpen(){if(!quickReply.active||!quickReply.editable())return;quickEmojiPicker=true;emojiTab=0;emojiFocus=0;if(emojiItems().empty())emojiTab=1;/* nothing used yet: first category */lockScreenDirty=true;render(true);}
static bool quickEmojiTap(int x,int y){
 if(quickEmojiPicker){for(int t=0;t<mm::EmojiTabs;t++)if(mm::emojiTabRect(t).contains(x,y)){emojiTab=t;emojiFocus=0;lockScreenDirty=true;render(true);return true;}
  int count=emojiItems().size();for(int i=0;i<count&&i<mm::EmojiPageSize;i++)if(mm::emojiChoiceRect(i).contains(x,y)){insertEmoji(i);return true;}
  if(!mm::EmojiPopup.contains(x,y)){quickEmojiPicker=false;lockScreenDirty=true;render(true);}return true;}
 if(mm::QuickReplyEmoji.contains(x,y)){quickEmojiOpen();return true;}return false;}
// Composer caret: typing, emoji and backspace act at the caret; the T-Deck Max heart / message keys move it.
static unsigned cursorAt(){unsigned n=input.length();return inputCursor<0||unsigned(inputCursor)>=n?n:unsigned(inputCursor);}
static void inputInsert(const String &text){unsigned at=cursorAt();if(at>=input.length()){input+=text;inputCursor=-1;}else{input=input.substring(0,at)+text+input.substring(at);inputCursor=int(at+text.length());}composerEdited();}
static void inputEraseBeforeCursor(){unsigned at=cursorAt();if(!at)return;int from=int(mm::utf8PrevStart(input,at));bool atEnd=at>=input.length();input.remove(from,at-from);inputCursor=atEnd?-1:from;composerEdited();}
static bool moveCursor(int delta){unsigned at=cursorAt(),n=input.length();if(delta<0){if(!at)return false;int from=int(mm::utf8PrevStart(input,at));inputCursor=from;}else{if(at>=n)return false;unsigned next=at+1;while(next<n&&(uint8_t(input[next])&0xc0)==0x80)next++;inputCursor=next>=n?-1:int(next);}composerEdited();return true;}
static bool composerWriting(){return !deviceLocked&&screen==Screen::Chat&&editableField()==&input&&input.length()>0;}
void beginTouchBrowse(){if(touchBrowse)return;auto list=conversationList();chatsTop=chatsFocusIndex(list)/chatsRows()*chatsRows();touchBrowse=true;}
// First key after touch browsing: show the focus again, inside the rows on screen.
void endTouchBrowse(){if(!touchBrowse)return;
 if(screen==Screen::Chats){auto list=conversationList();int start=chatsStart(list),rows=chatsRows(),f=chatsFocusIndex(list);if(!list.empty()&&(f<start||f>=start+rows))selected=list[start].id;}
 settingsFocus=constrain(settingsFocus,settingsScroll,mm::settingsLastVisible(settingsScroll));
 contactFocus=constrain(contactFocus,nodeScroll,nodeScroll+mm::ContactRows-1);
 touchBrowse=false;}
// ---- First boot ------------------------------------------------------------
// A clean flash (the web installer always erases) leaves the NVS empty: that is a new installation. MatterMesh data
// of an earlier installation on the SD is moved aside (nothing deleted) so the device starts empty, then the welcome
// runs. A device updated in place (OTA or USB without erase) keeps everything.
// mmsetup/done: 1 after the welcome (or for a device updated in place); 2 while a clean install's welcome is unfinished.
// mmsetup/archived: the SD data of the earlier installation was moved aside (retried at boot until the card mounts).
static bool nvsEmpty(){nvs_iterator_t it=nvs_entry_find(NVS_DEFAULT_PART_NAME,nullptr,NVS_TYPE_ANY);if(!it)return true;nvs_release_iterator(it);return false;}
// Moves /mattermesh and /maxchat into /mm-previous-N (map tile caches in /maps stay).
// done: false when something was left in place (mkdir/rename failed): mmsetup/archived stays 0 and the next boot retries.
static String archiveSdData(bool &done){done=true;
 if(!SD.exists("/mattermesh")&&!SD.exists("/maxchat"))return "";
 String base;for(int n=1;n<100;n++){base="/mm-previous-"+String(n);if(!SD.exists(base))break;}
 if(SD.exists(base)||!SD.mkdir(base)){done=false;return "";}bool moved=false;
 for(const char *dir:{"/mattermesh","/maxchat"})if(SD.exists(dir)){if(SD.rename(dir,base+dir))moved=true;else done=false;}
 Serial.printf("FIRST_BOOT archived=%d to=%s\n",moved,base.c_str());return moved?base:"";
}
static void welcomeFillPresets(){welcomeList.clear();for(int p=0;p<matter::RADIO_PRESET_COUNT;p++)if(matter::radioBand(matter::RADIO_PRESETS[p].frequency)==welcomeBand)welcomeList.push_back(p);
 int pick=0;for(int k=0;k<int(welcomeList.size());k++)if(!strcmp(matter::RADIO_PRESETS[welcomeList[k]].name,welcomeBand==868?"EU/UK (Narrow)":"USA")){pick=k;break;}welcomeFocus=pick;}
void startWelcome(){screen=Screen::Welcome;welcomeStep=0;welcomeWifiChoice=0;welcomeWifi=false;welcomeBand=868;welcomeBytes=matter::OUTGOING_PATH_HASH_BYTES;notice="";
 if(!welcomeName.length()){uint64_t mac=ESP.getEfuseMac();const uint8_t tail[2]={uint8_t(mac>>32),uint8_t(mac>>40)};welcomeName=String("MatterMesh-")+mm::hexString(tail,2,true).c_str();}welcomeFillPresets();render();}
static void finishWelcome(){
 if(welcomeFocus<0||welcomeFocus>=int(welcomeList.size()))return;const auto &p=matter::RADIO_PRESETS[welcomeList[welcomeFocus]];
 auto cfg=matter::snapshot().config;cfg.frequency=p.frequency;cfg.bandwidth=p.bandwidth;cfg.sf=p.sf;cfg.cr=p.cr;cfg.name=welcomeName;cfg.pathBytes=welcomeBytes;
 String reason;if(!matter::configure(cfg,reason)){notice=reason.length()?reason:"Radio not saved";render();return;}
 {Preferences pr;if(pr.begin("mmlora",false)){pr.putString("preset",p.name);pr.end();}}device::prefPutInt("mmsetup","done",1);loadRadioDraft(true);
 Serial.printf("WELCOME_DONE preset=%s name=%s bytes=%d\n",p.name,welcomeName.c_str(),welcomeBytes);notice="";welcomeArchive="";screen=Screen::Home;homeSelection=-1;render();}
// Wi-Fi step: "connect" opens the normal Wi-Fi page (Wi-Fi switched on and a scan started); "skip" goes on.
static void welcomeOpenWifi(){welcomeWifi=true;device::wifiEnabled(true);device::scan();openSection(4);}
static void welcomeNext(){notice="";
 if(welcomeStep==1&&welcomeWifiChoice==0&&!device::wifiConnected()){welcomeOpenWifi();return;}
 if(welcomeStep==2)welcomeFillPresets();
 if(welcomeStep==4){welcomeName.trim();if(!matter::validCompanionName(welcomeName.c_str())){notice="Name required / max 31 bytes";render();return;}}
 if(welcomeStep==6){finishWelcome();return;}welcomeStep++;render();}
static void welcomeKey(char c){
 if(c=='\b'){if(welcomeStep==4&&welcomeName.length()){eraseLastCodepoint(welcomeName);render();return;}if(welcomeStep>0){welcomeStep--;notice="";render();}return;}
 if(c=='\n'){welcomeNext();return;}
 if(welcomeStep==4){if(c>=32&&c<=126&&welcomeName.length()<matter::COMPANION_NAME_MAX_BYTES){welcomeName+=c;repaintSettled();}return;}
 bool prev=c=='w'||c=='a'||c=='k'||c=='h',next=c=='s'||c=='d'||c=='j'||c=='l';if(!prev&&!next)return;
 if(welcomeStep==1)welcomeWifiChoice=welcomeWifiChoice?0:1;
 else if(welcomeStep==2)welcomeBand=welcomeBand==868?900:868;
 else if(welcomeStep==3)welcomeFocus=constrain(welcomeFocus+(next?1:-1),0,max(0,int(welcomeList.size())-1));
 else if(welcomeStep==5)welcomeBytes=constrain(welcomeBytes+(next?1:-1),1,3);
 render();}
static void welcomeTap(int x,int y){
 if(mm::WelcomeNext.contains(x,y)){welcomeNext();return;}if(welcomeStep>0&&mm::WelcomeBack.contains(x,y)){welcomeKey('\b');return;}
 if(welcomeStep==1){for(int i=0;i<2;i++)if(mm::welcomeCard(i,2).contains(x,y)){welcomeWifiChoice=i;if(i==0&&!device::wifiConnected())welcomeOpenWifi();else render();}}
 else if(welcomeStep==2){for(int i=0;i<2;i++)if(mm::welcomeCard(i,2).contains(x,y)){welcomeBand=i?900:868;render();}}
 else if(welcomeStep==3){int scroll=max(0,min(welcomeFocus-mm::WelcomeRows/2,int(welcomeList.size())-mm::WelcomeRows));for(int i=0;i<mm::WelcomeRows;i++)if(mm::welcomeRow(i).contains(x,y)&&scroll+i<int(welcomeList.size())){welcomeFocus=scroll+i;render();}}
 else if(welcomeStep==5){for(int b=0;b<3;b++)if(mm::welcomeCard(b,3).contains(x,y)){welcomeBytes=b+1;render();}}}
// Back on the welcome once the Wi-Fi page reports a connection (or the user left it with BACK).
static void welcomeWifiLoop(){
 if(!welcomeWifi)return;if(screen==Screen::Welcome){welcomeWifi=false;render();return;}
 static uint32_t connectedAt=0;if(screen==Screen::Section&&section==4&&!wifiEditing&&device::wifiConnected()){if(!connectedAt)connectedAt=millis();if(millis()-connectedAt>=1500){connectedAt=0;back();}}else connectedAt=0;}
void key(char c) {
 if(deviceLocked){if(quickReply.active)quickReplyKey(c);return;}
 if(screen==Screen::Welcome){device::activity();welcomeKey(c);return;}
 if(touchBrowse){bool was=screen==Screen::Chats||screen==Screen::Section;endTouchBrowse();if(was&&(c=='j'||c=='k'||c=='w'||c=='s')){render();return;}}// the first move only reveals the focus
 device::activity();
 if(c==3){back();return;}
 if(screen==Screen::Section&&mm::toolsSection(section)&&toolsKey(c))return;
 if(screen==Screen::Section&&section==mm::LinkSection&&linkKey(c))return;
 if(c=='\b'){auto field=editableField();if(!field||!field->length()){back();return;}if(field==&input)inputEraseBeforeCursor();else eraseLastCodepoint(*field);notice="";if(screen==Screen::Chats&&searching){auto list=conversationList();selected=list.empty()?chatCount:list[0].id;}if(screen==Screen::PickContact){pickerFocus=0;refreshPicker();}if(screen==Screen::Section&&section==1)contactFocus=nodeScroll=0;if(screen==Screen::Section&&section==mm::KeyboardSection)kbdFocus=0;repaintSettled();return;}
 if(screen==Screen::Section&&section==9&&mapKey(c))return;
 if(channelOptions){if(c=='a'){back();return;}if(deleteChannelConfirm){if(c=='\n')deleteSelectedChannel();return;}if(channelKeyView){if(c=='\n')back();return;}const bool direct=chats[channelOptionTarget].direct;const int slots=direct?2:3;if(c=='w'||c=='k'||c=='s'||c=='j'){channelOptionFocus=(channelOptionFocus+(c=='s'||c=='j'?1:slots-1))%slots;render();}else if(c=='\n'||c=='d')channelOption(direct&&channelOptionFocus==1?2:channelOptionFocus);return;}
 if(screen==Screen::Chat&&positionShare){if(c=='\n'||c=='d')shareGpsPosition();else if(c=='a')back();return;}
 if(screen==Screen::Chat&&scopeEditing){if(c=='\n')saveNewScope();else if(c>=32&&c<=126&&scopeInput.length()<32){scopeInput+=c;notice="";repaintSettled();}return;}
 if(screen==Screen::Chat&&scopePicker){if(c=='w'||c=='k'||c=='s'||c=='j'){scopeFocus=constrain(scopeFocus+((c=='s'||c=='j')?1:-1),0,int(regionScopes.size())-1);notice="";render();}else if(c=='\n'||c=='d')applySelectedScope();else if(c=='n')openScopePicker(true);else if(c=='a')back();return;}
 if(screen==Screen::Chat&&clearChatConfirm){if(c=='\n')clearActiveChat();else if(c=='a')back();return;}
 if(screen==Screen::Chat&&chatDetails){if(c=='w'||c=='k'||c=='s'||c=='j'){chatMenuFocus=constrain(chatMenuFocus+((c=='s'||c=='j')?1:-1),0,mm::ChatOptionCount-1);render();}else if(c=='\n'||c=='d')chatOption(chatMenuFocus);else if(c=='a')back();return;}
 if(screen==Screen::Chat&&messageActions){if(contextMessage.mine&&(c=='j'||c=='k')){int count=repeaterRows(contextSnapshot()).size();heardOffset=constrain(heardOffset+(c=='j'?1:-1),0,max(0,count-mm::HeardRows));render();}else if(c=='m'&&!contextMessage.mine)messageAction(mm::MessageMapAction);else if(c=='l'&&!contextMessage.mine)linkOpenFromChat(contextMessage.body);else if(c>='1'&&c<=(contextMessage.mine?'5':'0'+mm::ReceivedMessageActions))messageAction(contextMessage.mine?c-'1':mm::receivedMessageAction(c-'1'));return;}
 if(screen==Screen::Chat&&messagePath){if(messageRouteView)return;if(c=='j')scrollMessagePath(5);else if(c=='k')scrollMessagePath(-5);render();return;}
 if(screen==Screen::Home){
  if(c=='['||c==']'){homeScroll=constrain(homeScroll+(c==']'?1:-1),0,max(0,chatCount-mm::MenuHomeRows));render();return;}
  if(c=='j'||c=='s'||c=='d'){homeSelection=homeSelection<0?0:(homeSelection+1)%mm::MenuHomeTabCount;render();}
  else if(c=='k'||c=='w'||c=='a'){homeSelection=homeSelection<0?mm::MenuHomeTabCount-1:(homeSelection+mm::MenuHomeTabCount-1)%mm::MenuHomeTabCount;render();}
  else if(c=='\n'){int route=mm::HomeTabs[homeSelection<0?0:homeSelection].section;if(route<0){screen=Screen::Launcher;render();}else openSection(route);}
  return;
 }
 if(screen==Screen::Launcher){
  if(mm::directionKey(c)){launcherSelection=mm::moveAppFocus(launcherSelection,c,mm::ApplicationCount,mm::AppCols);render();}
  else if(c=='j'){launcherSelection=launcherSelection<0?0:(launcherSelection+1)%mm::ApplicationCount;render();}
  else if(c=='k'){launcherSelection=launcherSelection<0?mm::ApplicationCount-1:(launcherSelection+mm::ApplicationCount-1)%mm::ApplicationCount;render();}
  else if(c=='\n')openSection(mm::Applications[launcherSelection<0?0:launcherSelection].section);
  return;
 }
 if(screen==Screen::Section){
  if(section==2&&radioDevicePage&&!radioPowerEditing){
   if(c=='w'||c=='k'||c=='s'||c=='j'){radioDeviceFocus=constrain(radioDeviceFocus+((c=='s'||c=='j')?1:-1),0,6);render();}
   else if(radioDeviceFocus>=5&&(c=='a'||c=='d'))selectLoRaTab(c=='d',true);else if(c=='a')back();else if(c=='d'||c=='\n'){if(!radioDeviceFocus)editRadioField(CompanionNameField);else if(radioDeviceFocus<=3){radioDraft.pathBytes=radioDeviceFocus;notice="Edits ready / press APPLY";render();}else if(radioDeviceFocus==4)applyRadioDraft();else selectLoRaTab(radioDeviceFocus==6);}return;
  }
  if(section==2&&!radioPowerEditing){
   if(c=='w'||c=='k'||c=='s'||c=='j'){radioFocus=constrain(radioFocus+((c=='s'||c=='j')?1:-1),0,9);render();}
   else if((c=='a'||c=='d')&&radioFocus>=8)selectLoRaTab(c=='d',true);
   else if((c=='a'||c=='d')&&radioFocus==0)cycleRadioPreset(c=='a'?-1:1);
   else if(c=='a')back();else if(c=='d'||c=='\n')activateRadioFocus();else if(c=='b')selectRadioBand(radioBandSelection==868?900:868);else if(c=='p')editRadioPower();return;
  }
  if(!editableField()){if(c=='a'){back();return;}if(section==19&&c=='d')c='\n';if(section!=4){if(c=='w')c='k';else if(c=='s')c='j';}}
  if(section==3&&(c=='\n'||c==' ')){notice=device::locationToggleUser()?(device::locationState().enabled?"GPS enabled":"GPS disabled"):"GPS switch failed";render();return;}
  if(section==17){if(otaConfirm&&c=='n'){otaConfirm=false;render();}else if(c=='\n'||(otaConfirm&&c=='y'))otaAction();return;}
  if(section==2&&radioPowerEditing){if(c=='\n'){saveRadioPower();return;}if(radioEditField==CompanionNameField){if(c>=32&&c<=126&&radioPowerInput.length()<matter::COMPANION_NAME_MAX_BYTES)radioPowerInput+=c;}else if(((c>='0'&&c<='9')||c=='-'||c=='+'||c=='.')&&radioPowerInput.length()<12)radioPowerInput+=c;notice="";repaintSettled();return;}
  if(section==21){if(c=='h'){openSection(6);return;}else if(c=='n')device::audioNotifications(!device::audioState().notifications);else if(c=='t'||c=='\n')device::audioNotify(true);else if(c=='m'||c=='r')cycleRingtone(c=='r',1);else if(c=='j'||c=='k')device::audioVolume(device::audioState().volume+(c=='j'?5:-5));render();return;}
  if(section==4&&wifiEditing){if(c=='\n'){if(wifiEditField==0)wifiEditField=1;else{connectWifi();return;}}else if(c=='\t')wifiEditField=1-wifiEditField;else if(c>=32&&c<=126){String &field=wifiEditField==0?wifiTarget:wifiPassword;if(field.length()<(wifiEditField==0?32:63))field+=c;}repaintSettled();return;}
  if(section==16){if(c=='h'||c=='a')dtFocus=(dtFocus+4)%5;else if(c=='l'||c=='d')dtFocus=(dtFocus+1)%5;else if(c=='k'||c=='w')dtStep(dtFocus,1);else if(c=='j'||c=='s')dtStep(dtFocus,-1);
   else if(c=='\n'){String reason;notice=dtApply(reason)?"Date and time set":reason;}else return;render();return;}
  if(section==mm::KeyboardSection){auto rows=keyboardLanguageRows();
   if(kbdSearching){if(c=='\n'){kbdSearching=false;render();return;}if(c>=32&&c<=126&&kbdQuery.length()<15){kbdQuery+=c;kbdFocus=0;notice="";}repaintSettled();return;}
   if(c=='/'){kbdSearching=true;render();return;}
   if((c=='w'||c=='k')&&kbdFocus>0){kbdFocus--;render();return;}if((c=='s'||c=='j')&&kbdFocus+1<int(rows.size())){kbdFocus++;render();return;}
   if((c=='\n'||c==' ')&&kbdFocus<int(rows.size())){toggleKeyboardLanguage(rows[kbdFocus]);return;}
   return;}
  if(section==mm::AccentKeySection){if(c=='l'||c=='j'){accentFocus=(accentFocus+1)%3;notice="";}else if(c=='h'||c=='k'){accentFocus=(accentFocus+2)%3;notice="";}else if(c=='\n'||c==' '){bool ok=setAccentMode(accentFocus);notice=ok?"Accent key applied":"Save failed";if(!ok)accentFocus=accentMode;}render();return;}
  if(section==mm::SupportSection){if(c=='j'||c=='k'||c=='h'||c=='l'||c=='d'||c=='\n'||c==' '){supportTab=1-supportTab;render();}return;}
  if(section==mm::TextSizeSection){if(c=='j'||c=='l'||c=='k'||c=='h'){textSizeFocus=1-textSizeFocus;render();}else if(c=='\n'||c==' '){bool ok=setTextSize(textSizeFocus);notice=ok?"Text size applied":"Save failed";if(!ok)textSizeFocus=chatTextSize;render();}return;}
  if(section==22){if(c=='j'||c=='l'){wallpaperFocus=(wallpaperFocus+1)%WALLPAPER_COUNT;render();}else if(c=='k'||c=='h'){wallpaperFocus=(wallpaperFocus+WALLPAPER_COUNT-1)%WALLPAPER_COUNT;render();}else if(c=='\n'||c==' '){notice=setWallpaper(wallpaperFocus)?("Theme applied"):"Save failed";render();}return;}
  if(section==20){if(c=='j'||c=='k'){powerField=1-powerField;render();}else if(c=='\n'||c==' '){auto power=device::powerState();uint32_t current=powerField?power.keyboardSeconds:power.displaySeconds;int i=0;while(i<6&&device::TIMEOUT_VALUES[i]!=current)i++;device::timeout(powerField!=0,device::TIMEOUT_VALUES[(i+1)%6]);render();}return;}
  if(section==4){auto connection=device::connectivityStatus();if(c=='h'){hiddenWifi();return;}else if(c=='x'){device::disconnect();render();return;}else if(c=='r'){device::reconnect();render();return;}else if(c=='f'){device::scan();render();}else if(c==' '){device::wifiEnabled(!connection.wifi);render();}else if(c=='j'||c=='k'||c=='w'||c=='s'){wifiFocus=constrain(wifiFocus+((c=='j'||c=='s')?1:-1),0,max(0,int(connection.networkCount)-1));wifiOffset=wifiFocus/mm::WifiRows*mm::WifiRows;render();}else if(c=='\n'||c=='d')selectWifi(wifiFocus);return;}
  if(section==14&&bleNameEditing){if(c=='\n')saveBleName();else if(c>=32&&c<=126&&bleNameInput.length()<device::BLE_NAME_MAX_BYTES){bleNameInput+=c;repaintSettled();}return;}
  if(section==14&&c=='n'){startBleNameEdit();return;}
  if(section==14&&(c=='\n'||c==' ')){device::bleEnabled(!device::connectivityStatus().ble);render();return;}
  if(section==14&&(c=='t'||c=='s'||c=='j')){appWifiToggle();render();return;}// Plus: the app-over-Wi-Fi switch
  if(section==10&&weatherTabKey(c))return;
  if(section==10&&(c=='r'||c=='\n'||c=='w')){auto phase=device::weatherState().phase;if(phase==device::WeatherPhase::WifiOff)device::weatherEnableWifi();else if(phase==device::WeatherPhase::NoNetwork){openSection(4);return;}else if(c!='w')device::weatherRefresh();render();return;}
  if(section==1){if(contactSearching){if(c=='\n'){contactSearching=false;render();return;}if(c>=32&&c<=126&&contactQuery.length()<31){contactQuery+=c;contactFocus=nodeScroll=0;}repaintSettled();return;}
   if(c=='/'){contactSearching=true;render();return;}if(c=='d'){discoverFilter=(discoverFilter+1)%3;contactFocus=nodeScroll=0;render();return;}if(c=='n'||c=='p'){contactPage(c=='n'?1:-1);render();return;}
   if(c=='j'||c=='k'){int total=matter::contactsCount(discoverFilter,contactQuery.c_str());contactFocus=constrain(contactFocus+(c=='j'?1:-1),0,max(0,total-1));if(contactFocus<nodeScroll)nodeScroll=contactFocus;if(contactFocus>=nodeScroll+mm::ContactRows)nodeScroll=contactFocus-mm::ContactRows+1;render();return;}
   if(c=='\n')openContact(contactFocus);return;}
  if(section==19){
   if(c=='j'||c=='k'){settingsFocus=constrain(settingsFocus+(c=='j'?1:-1),0,mm::SettingsCount-1);if(settingsFocus<settingsScroll)settingsScroll=settingsFocus;while(!mm::settingsVisible(settingsFocus,settingsScroll))settingsScroll++;render();}
   else if(c=='\n')openSection(mm::SettingsItems[settingsFocus].section);
  }
  return;
 }
 if(screen==Screen::Chats) {
  if(!searching){if(c=='a'){back();return;}if(c=='d')c='\n';else if(c=='w')c='k';else if(c=='s')c='j';}
  if(messagesMenu){if(c=='j'||c=='k')messageMenuFocus=(messageMenuFocus+(c=='j'?1:mm::MessagesMenuRows-1))%mm::MessagesMenuRows;else if(c>='1'&&c<='4'){messagesMenuChoose(c-'1');return;}else if(c=='\n'){messagesMenuChoose(messageMenuFocus);return;}render();return;}
  if(searching){
   if(c=='\n'){searching=false;render();return;}
   if(c>=32&&c<=126&&searchQuery.length()<31)searchQuery+=c;
   auto list=conversationList();selected=list.empty()?chatCount:list[0].id;repaintSettled();return;
  }
  if(c=='j'||c=='k'){
   auto list=conversationList();int pos=list.size();for(unsigned i=0;i<list.size();i++)if(list[i].id==selected)pos=i;
   pos=constrain(pos+(c=='j'?1:-1),0,int(list.size()));selected=pos==int(list.size())?chatCount:list[pos].id;render();
  }else if(c=='n')openMessagesMenu();
  else if(c=='/'){searching=true;searchQuery="";render();}
  else if(c=='\n'){
   if(selected==chatCount)openMessagesMenu();else{openConversation(selected,Screen::Chats);}
  }
 } else if(screen==Screen::PickContact) {
  if(pickerSearching){if(c=='\n'){pickerSearching=false;render();return;}if(c>=32&&c<=126&&pickerQuery.length()<31){pickerQuery+=c;pickerFocus=0;refreshPicker();}repaintSettled();return;}
  int count=pickerContacts.size();
  if(c=='a'){back();return;}
  if(c==' '||c=='f'){pickerSearching=true;render();return;}
  if(c=='w'||c=='k'){pickerFocus=max(0,pickerFocus-1);render();}else if(c=='s'||c=='j'){pickerFocus=min(max(0,count-1),pickerFocus+1);render();}
  else if(c=='n'||c=='p'){pickerFocus=constrain(pickerFocus+(c=='n'?mm::PickerRows:-mm::PickerRows),0,max(0,count-1));render();}
  else if(c=='\n'||c=='d')pickContact(pickerFocus);
  return;
 } else if(screen==Screen::NewChat) {
  const bool hashtag=newForm==int(mm::ChannelForm::JoinHashtag),create=newForm==int(mm::ChannelForm::CreatePrivate);
  auto nextField=[&](int f){f=(f+1)%3;return f==1&&(hashtag||create)?2:f;};// the generated key / hashtag note is not typed into
  if(c=='\n'){if(formField==2){saveNewChannel();return;}formField=nextField(formField);notice="";render();return;}
  if(c=='\t'){formField=nextField(formField);notice="";render();return;}
  if(formField==1){if(create)return;std::string k=newKey.c_str();if(!mm::acceptSecretChar(k,c)){notice=k.size()>=mm::ChannelSecretHex?"Key complete: 32 characters":"Key uses 0-9 and a-f";render();return;}newKey=k.c_str();}
  else if(formField==0&&hashtag){std::string n=newName.c_str();if(c=='#'&&n.empty())return;if(!mm::acceptHashtagChar(n,c)){notice="Use a-z, 0-9 and '-'";render();return;}newName=n.c_str();}
  else{String &field=formField==0?newName:newScope;if(c>=32&&c<=126&&field.length()<31)field+=c;}
  notice="";repaintSettled();
 } else {
  if(emojiPicker){int k=emojiPickerKey(c);if(k==2){insertEmoji(emojiFocus);return;}if(!k&&c==3)emojiPicker=false;render();return;}
  if(c=='\n'){submitLocal();return;}
  else if(c=='@'){openMentionPicker();return;}// Plus: "@" opens the people search, the pick inserts @[name]
  else if(c>=32&&c<=126){auto cyr=layoutLetter(c);String text=cyr.empty()?String(c):String(cyr.c_str());if(input.length()+text.length()<=messageByteLimit())inputInsert(text);else{notice=matter::messageLimitError(messageByteLimit()).c_str();repaintSettled();return;}}
  notice="";repaintSettled();
 }
}
void touchKeyPress(int id){touchKeyEvents++;lastTouchKey=id;bool writing=composerWriting();auto action=device::touchKeyAction(id,writing);Serial.printf("TOUCH_KEY id=%d writing=%d action=%d\n",id,writing,int(action));if(action==device::TouchKeyAction::None)return;device::activity();
 if(action==device::TouchKeyAction::Send){submitLocal();return;}
 if(moveCursor(action==device::TouchKeyAction::CursorLeft?-1:1)){notice="";repaintSettled();}}
// GT911 in landscape, as the LilyGO T-Deck examples: swapped axes, Y mirrored.
void initTouch() {
 legacyTouch.setPins(-1,BOARD_TOUCH_INT);
 if(legacyTouch.begin(Wire,BOARD_I2C_ADDR_TOUCH,BOARD_I2C_SDA,BOARD_I2C_SCL)){
  legacyTouch.setMaxCoordinates(mm::W,mm::H);legacyTouch.setSwapXY(true);legacyTouch.setMirrorXY(false,true);touchBackend=2;
 }
 Serial.printf("PLUS_TOUCH ready=%d\n",touchBackend==2);
}
// Quick settings: sliders write the same stored levels as the Lights and Audio
// pages; tiles reuse the existing toggles; Power Off asks before shutting down.
void quickSettingsTap(int x,int y){
 if(shutdownConfirm){if(mm::QuickConfirmPowerOff.contains(x,y))confirmShutdown();else if(mm::QuickConfirmCancel.contains(x,y)){shutdownConfirm=false;notice="";render();}return;}
 for(int row=0;row<2;row++)if(mm::quickSliderRect(row).contains(x,y)){
  int step=mm::quickStepAt(mm::quickSliderRect(row),x);
  if(row==0){displayBrightness=(step*255+mm::QuickSteps/2)/mm::QuickSteps;if(!device::powerState().displayEnabled)device::lightEnabled(false,true);saveLights();device::brightness(displayBrightness,keyboardBrightness);}
  else{device::audioVolume((step*100+mm::QuickSteps/2)/mm::QuickSteps);if(step&&!device::audioState().notifications)device::audioNotifications(true);}
  render();return;
 }
 for(int i=0;i<mm::QuickTileCount;i++)if(mm::quickTileRect(i).contains(x,y)){
  switch(i){
   case mm::QuickWifi:device::wifiEnabled(!device::connectivityStatus().wifi);break;
   case mm::QuickBluetooth:device::bleEnabled(!device::connectivityStatus().ble);break;
   case mm::QuickGps:notice=device::locationToggleUser()?"":"GPS switch failed";break;
   case mm::QuickSound:device::audioNotifications(!device::audioState().notifications);break;
   case mm::QuickPowerOff:shutdownConfirm=true;break;
   default:return;// reserved slot: inactive
  }
  render();return;
 }
}
void tap(int x,int y) {
 if(x<0||x>=mm::W||y<0||y>=mm::H)return;
 if(screen==Screen::Welcome&&!deviceLocked){device::activity();welcomeTap(x,y);return;}
 if(deviceLocked){if(quickReply.active){quickReplyInputAt=millis();if(quickEmojiTap(x,y))return;if(mm::QuickReplySend.contains(x,y))sendQuickReply();else if(mm::QuickReplyCancel.contains(x,y)||mm::BackButton.contains(x,y))closeQuickReply();}return;}device::activity();
 if(channelOptions){if(mm::BackButton.contains(x,y)){back();return;}if(deleteChannelConfirm){if(mm::ChatClearYes.contains(x,y))deleteSelectedChannel();else if(mm::ChatClearNo.contains(x,y))back();}else if(channelKeyView)back();else{const bool direct=chats[channelOptionTarget].direct;for(int slot=0;slot<(direct?2:3);slot++)if(mm::channelOptionRect(slot).contains(x,y)){channelOption(direct&&slot==1?2:slot);break;}}return;}
 Serial.printf("TOUCH_TAP x=%d y=%d screen=%d\n",x,y,int(screen));
 // Status bar: opens Quick settings from any screen; tapping it there closes the panel.
 if(y<mm::StatusH){if(screen==Screen::Section&&section==mm::QuickSettingsSection)back();else openSection(mm::QuickSettingsSection);return;}
 if(screen==Screen::Home){
  if(mm::MenuHomeClock.contains(x,y)){navigation.clear();screen=Screen::Launcher;render();return;}
  for(int row=0;row<mm::MenuHomeRows;row++)if(mm::menuHomeRowRect(row).contains(x,y)){auto list=conversationList();/* sorted by activity, as painted */int at=max(0,min(homeScroll,max(0,int(list.size())-mm::MenuHomeRows)))+row;/* rows follow homeScroll, as painted */if(at<int(list.size())){chatsParent=Screen::Home;openConversation(list[at].id,Screen::Home);}return;}
  for(int tab=0;tab<mm::MenuHomeTabCount;tab++)if(mm::menuHomeTabRect(tab).contains(x,y)){homeSelection=tab;int route=mm::HomeTabs[tab].section;if(route<0){navigation.clear();screen=Screen::Launcher;render();}else openSection(route);return;}return;
 }
 if(screen==Screen::Launcher){
  if(mm::BackButton.contains(x,y)){screen=Screen::Home;render();return;}
  for(int i=0;i<mm::ApplicationCount;i++)if(mm::appRect(i).contains(x,y)){launcherSelection=i;openSection(mm::Applications[i].section);return;}
  return;
 }
 if(screen==Screen::Section){
  if(mm::toolsSection(section)&&toolsTap(x,y))return;
  if(section==mm::QuickSettingsSection&&shutdownConfirm){quickSettingsTap(x,y);return;}
  if(section==mm::LinkSection){linkTap(x,y);return;}// its own bar: back disc, PICTURE / PAGE switch
  if(mm::BackButton.contains(x,y)){back();return;}
  if(section==mm::QuickSettingsSection){quickSettingsTap(x,y);return;}
  if(section==17&&mm::UpdatesAction.contains(x,y)){otaAction();return;}
  if(section==3&&mm::GpsToggle.contains(x,y)){notice=device::locationToggleUser()?(device::locationState().enabled?"GPS enabled":"GPS disabled"):"GPS switch failed";render();return;}
  if(section==21){if(mm::AudioNotify.contains(x,y))device::audioNotifications(!device::audioState().notifications);else if(mm::AudioToneMessage.contains(x,y)||mm::AudioToneMention.contains(x,y)){bool mention=mm::AudioToneMention.contains(x,y);cycleRingtone(mention,mm::stepRowTap(mm::AudioToneMessage,x));}else if(mm::AudioMinus.contains(x,y))device::audioVolume(device::audioState().volume-5);else if(mm::AudioPlus.contains(x,y))device::audioVolume(device::audioState().volume+5);else if(mm::AudioSlider.contains(x,y)){int track=mm::AudioSlider.w-16;device::audioVolume((constrain(x-mm::AudioSlider.x-8,0,track)*20+track/2)/track*5);}render();return;}
  if(section==16){if(mm::DateTimeSet.contains(x,y)){String reason;notice=dtApply(reason)?"Date and time set":reason;render();return;}
   for(int i=0;i<5;i++){mm::Rect r=mm::dateTimeBox(i);if(r.contains(x,y)){dtFocus=i;dtStep(i,y<r.y+r.h/2?1:-1);notice="";render();return;}}return;}
  if(section==mm::KeyboardSection){if(mm::KeyboardSearch.contains(x,y)){kbdSearching=true;render();return;}
   auto rows=keyboardLanguageRows();int first=mm_kbd::firstRow(kbdFocus,int(rows.size()),mm::KeyboardRows);
   for(int i=0;i<mm::KeyboardRows&&first+i<int(rows.size());i++)if(mm::keyboardRowRect(i).contains(x,y)){kbdSearching=false;kbdFocus=first+i;toggleKeyboardLanguage(rows[first+i]);return;}
   return;}
  if(section==mm::AccentKeySection){for(int i=0;i<3;i++)if(mm::accentCardRect(i).contains(x,y)){bool ok=setAccentMode(i);notice=ok?"Accent key applied":"Save failed";accentFocus=ok?i:accentMode;render();return;}return;}
  if(section==mm::SupportSection){for(int i=0;i<2;i++)if(mm::supportTabRect(i).contains(x,y)&&supportTab!=i){supportTab=i;render();}return;}
  if(section==mm::TextSizeSection){for(int i=0;i<2;i++)if(mm::textSizeCardRect(i).contains(x,y)){bool ok=setTextSize(i);notice=ok?"Text size applied":"Save failed";textSizeFocus=ok?i:chatTextSize;render();return;}return;}
  if(section==22){for(int i=0;i<WALLPAPER_COUNT&&i<mm::WallpaperCardCount;i++)if(mm::wallpaperCardRect(i).contains(x,y)){wallpaperFocus=i;notice=setWallpaper(i)?("Theme applied"):"Save failed";render();return;}return;}
  if(section==20){for(int i=0;i<2;i++)if(mm::powerRowRect(i).contains(x,y)){powerField=i;render();return;}for(int i=0;i<6;i++)if(mm::timeoutOptionRect(i).contains(x,y)){notice=device::timeout(powerField!=0,device::TIMEOUT_VALUES[i])?"Timeout saved":"Save failed";render();return;}return;}
  if(section==4){if(wifiEditing){if(mm::WifiEye.contains(x,y)){wifiShowPassword=!wifiShowPassword;render();return;}if(mm::WifiSSIDInput.contains(x,y)&&wifiHidden){wifiEditField=0;render();return;}if(mm::WifiPassword.contains(x,y)){wifiEditField=1;render();return;}if(mm::WifiConnect.contains(x,y)){connectWifi();return;}if(mm::WifiCancel.contains(x,y)){wifiPassword="";wifiEditing=false;wifiShowPassword=false;render();return;}return;}if(mm::WifiToggle.contains(x,y)){device::wifiEnabled(!device::connectivityStatus().wifi);render();return;}if(mm::WifiScan.contains(x,y)){device::scan();render();return;}if(mm::WifiManual.contains(x,y)){hiddenWifi();return;}if(mm::WifiDisconnect.contains(x,y)){if(device::connectivityStatus().connected||device::connectivityStatus().connecting)device::disconnect();else device::reconnect();render();return;}for(int row=0;row<mm::WifiRows;row++)if(mm::wifiRowRect(row).contains(x,y)){selectWifi(wifiOffset+row);return;}}

  if(section==14&&mm::AppNameCard.contains(x,y)){if(bleNameEditing)saveBleName();else startBleNameEdit();return;}
  if(section==14&&mm::BleToggle.contains(x,y)){device::bleEnabled(!device::connectivityStatus().ble);render();return;}
  if(section==14&&mm::AppWifiToggle.contains(x,y)){appWifiToggle();render();return;}
  if(section==10&&weatherTabTap(x,y))return;
  if(section==10){auto phase=device::weatherState().phase;bool offline=phase==device::WeatherPhase::WifiOff||phase==device::WeatherPhase::NoNetwork;if(offline&&mm::WeatherWifiButton.contains(x,y)){if(phase==device::WeatherPhase::WifiOff){device::weatherEnableWifi();render();}else openSection(4);return;}if(!offline&&mm::WeatherRefresh.contains(x,y)){device::weatherRefresh();render();return;}}
  if(section==19){for(int i=0;i<mm::SettingsCount;i++){if(mm::settingsVisible(i,settingsScroll)&&mm::settingsRect(i,settingsScroll).contains(x,y)){settingsFocus=i;openSection(mm::SettingsItems[i].section);return;}}}
  if(section==2){
   if(radioPowerEditing){if(mm::RadioPowerSave.contains(x,y))saveRadioPower();else if(mm::RadioPowerCancel.contains(x,y)){radioPowerEditing=false;notice="";render();}return;}
   if(mm::LoRaRadio.contains(x,y)){selectLoRaTab(false);return;}if(mm::LoRaDevice.contains(x,y)){selectLoRaTab(true);return;}
   if(radioDevicePage){if(mm::LoRaName.contains(x,y)){radioDeviceFocus=0;editRadioField(CompanionNameField);}else if(mm::LoRaDeviceApply.contains(x,y))applyRadioDraft();else for(int bytes=1;bytes<=3;bytes++)if(mm::loraBytesRect(bytes).contains(x,y)){radioDeviceFocus=bytes;radioDraft.pathBytes=bytes;notice="Edits ready / press APPLY";render();break;}return;}
   if(mm::LoRaBand868.contains(x,y))selectRadioBand(868);else if(mm::LoRaBand900.contains(x,y))selectRadioBand(900);
   else if(mm::LoRaPresetPrev.contains(x,y))cycleRadioPreset(-1);else if(mm::LoRaPresetNext.contains(x,y))cycleRadioPreset(1);
   else if(mm::LoRaPreset.contains(x,y))cycleRadioPreset(1);
   else if(mm::LoRaApply.contains(x,y))applyRadioDraft();else if(mm::LoRaAdvert.contains(x,y)){matter::advertise(notice);render();}
   else for(int i=0;i<matter::RadioFieldCount;i++)if(mm::loraFieldRect(i).contains(x,y)){radioFocus=i+1;editRadioField(i);break;}
   return;
  }
  if(section==1){for(int i=0;i<mm::ContactTabCount;i++)if(mm::contactTabRect(i).contains(x,y)){discoverFilter=i;contactFocus=nodeScroll=0;contactSearching=false;render();return;}
   if(mm::ContactSearch.contains(x,y)){contactSearching=true;render();return;}if(mm::ContactPrevious.contains(x,y)){contactPage(-1);render();return;}if(mm::ContactNext.contains(x,y)){contactPage(1);render();return;}
   int total=matter::contactsCount(discoverFilter,contactQuery.c_str());for(int i=0;i<mm::ContactRows;i++)if(mm::contactRowRect(i).contains(x,y)){if(nodeScroll+i<total){contactFocus=nodeScroll+i;openContact(contactFocus);}return;}return;}
  if(section==6){
   
   for(int row=0;row<2;row++)if(mm::lightToggleRect(row).contains(x,y)){auto p=device::powerState();notice=device::lightEnabled(row==1,!(row==1?p.keyboardEnabled:p.displayEnabled))?"Light preference saved":"Cannot save light preference";render();return;}
   for(int row=0;row<2;row++)if(mm::lightSliderRect(row).contains(x,y)){
    int percent=(constrain(x-mm::HapticTrackX,0,mm::HapticTrackW)*100+mm::HapticTrackW/2)/mm::HapticTrackW;uint8_t value=(percent*255+50)/100;
    if(row==0)displayBrightness=value;else keyboardBrightness=value;
    saveLights();device::brightness(displayBrightness,keyboardBrightness);render();return;
   }return;
  }
  if(section==9&&mapTap(x,y))return;
  return;
 }
 if(screen==Screen::Chats){
  if(mm::BackButton.contains(x,y)){back();return;}
  if(mm::MessageMenu.contains(x,y)){if(messagesMenu){messagesMenu=false;render();}else openMessagesMenu();return;}
  if(messagesMenu){
   if(mm::MessagesPopup.contains(x,y))messagesMenuChoose(constrain((y-mm::MessagesPopup.y-2)/mm::MessagesPopupRowH,0,mm::MessagesMenuRows-1));
   else{messagesMenu=false;render();}return;
  }
  if(mm::ChatsNew.contains(x,y)){openMessagesMenu();return;}
  if(mm::ChatsSearch.contains(x,y)){searching=!searching;if(!searching)searchQuery="";render();return;}
  auto list=conversationList();int rows=chatsRows();
  int start=chatsStart(list);
  for(int row=0;row<rows&&start+row<int(list.size());row++){mm::Rect r=mm::messageListRect(row,searching);if(r.contains(x,y)){openConversation(list[start+row].id,Screen::Chats);return;}}
 }else if(screen==Screen::PickContact){
  if(mm::BackButton.contains(x,y)){back();return;}
  if(mm::MessageMenu.contains(x,y)||mm::PickerSearch.contains(x,y)){pickerSearching=!pickerSearching||mm::PickerSearch.contains(x,y);render();return;}
  int start=pickerFocus/mm::PickerRows*mm::PickerRows,count=pickerContacts.size();
  if(mm::PickerPrev.contains(x,y)&&start>0){pickerFocus=start-mm::PickerRows;render();return;}
  if(mm::PickerNext.contains(x,y)&&start+mm::PickerRows<count){pickerFocus=start+mm::PickerRows;render();return;}
  for(int row=0;row<mm::PickerRows&&start+row<count;row++)if(mm::pickerRowRect(row).contains(x,y)){pickContact(start+row);return;}
 }else if(screen==Screen::NewChat){
  if(mm::BackButton.contains(x,y)){back();return;}
  if(mm::formRect(0).contains(x,y)){formField=0;notice="";render();}
  else if(mm::formRect(1).contains(x,y)){if(newForm==int(mm::ChannelForm::CreatePrivate)){newKey=randomChannelSecret();notice="New key generated";}else if(newForm==int(mm::ChannelForm::JoinPrivate)){formField=1;notice="";}render();}
  else if(mm::formRect(2).contains(x,y)){formField=2;notice="";render();}
  else if(mm::SaveButton.contains(x,y))saveNewChannel();
  else if(mm::CancelButton.contains(x,y))back();
 }else if(screen==Screen::Chat){
  if(emojiPicker){for(int t=0;t<mm::EmojiTabs;t++)if(mm::emojiTabRect(t).contains(x,y)){emojiTab=t;emojiFocus=0;render();return;}int count=emojiItems().size();for(int i=0;i<count&&i<mm::EmojiPageSize;i++)if(mm::emojiChoiceRect(i).contains(x,y)){insertEmoji(i);return;}if(!mm::EmojiPopup.contains(x,y)){emojiPicker=false;render();}return;}
  if(scopePicker||clearChatConfirm||chatDetails||positionShare){
   if(mm::BackButton.contains(x,y)){back();return;}
   if(positionShare){if(mm::PositionSend.contains(x,y))shareGpsPosition();else if(mm::PositionCancel.contains(x,y))back();return;}
   if(clearChatConfirm){if(mm::ChatClearYes.contains(x,y))clearActiveChat();else if(mm::ChatClearNo.contains(x,y))back();return;}
   if(scopeEditing){if(mm::ScopeSave.contains(x,y))saveNewScope();else if(mm::ScopeEditCancel.contains(x,y))back();return;}
   if(scopePicker){int offset=scopeFocus/mm::ScopeRows*mm::ScopeRows;for(int i=0;i<mm::ScopeRows;i++)if(mm::scopeRowRect(i).contains(x,y)&&offset+i<int(regionScopes.size())){scopeFocus=offset+i;notice="";render();return;}if(mm::ScopeAdd.contains(x,y))openScopePicker(true);else if(mm::ScopeApply.contains(x,y))applySelectedScope();else if(mm::ScopeCancel.contains(x,y))back();return;}
   for(int i=0;i<mm::ChatOptionCount;i++)if(mm::chatOptionRect(i).contains(x,y)){chatOption(i);return;}chatDetails=false;render();return;
  }
  if(!messageActions&&!messagePath&&mm::ComposerEmoji.contains(x,y)){openEmojiPicker();render();return;}
  if(messageActions){if(mm::BackButton.contains(x,y)){back();return;}if(contextMessage.mine){for(int i=0;i<5;i++)if(mm::ownMessageActionRect(i).contains(x,y)){messageAction(i);return;}if(mm::HeardList.contains(x,y))return;}else if(mm::messageMapRect().contains(x,y)){messageAction(mm::MessageMapAction);return;}else if(mm::messageLinkRect().contains(x,y)&&!device::linkFind(contextMessage.body).empty()){linkOpenFromChat(contextMessage.body);return;}else for(int i=0;i<mm::ReceivedMessageActions;i++)if(mm::messageActionRect(i).contains(x,y)){messageAction(mm::receivedMessageAction(i));return;}messageActions=false;render();return;}
  if(messagePath&&messageRouteView){if(mm::BackButton.contains(x,y))back();return;} // static picture: Back only
  if(messagePath){if(mm::BackButton.contains(x,y)){back();return;}if(y>=mm::Bottom-40){if(x<mm::Margin+70)scrollMessagePath(-5);else if(x<mm::Margin+134)scrollMessagePath(5);else messagePath=false;render();}return;}
  if(mm::BackButton.contains(x,y)){back();return;}
  if(mm::ChatMenu.contains(x,y)){chatDetails=true;chatMenuFocus=0;notice="";render();return;}
  if(y<mm::ComposerY&&paintedChatConversation==activeChat){for(auto &hit:paintedChatMessages)for(auto &l:hit.links)if(l.contains(x,y)){linkOpenFromChat(hit.message.body);return;}// tap on the underlined link: open it
  for(auto &hit:paintedChatMessages)if(hit.area.contains(x,y)){contextMessage=hit.message;contextConversation=activeChat;heardOffset=0;if(hit.sender.contains(x,y)){messageAction(0);return;}messageActions=true;render();return;}}

 }
}

// Finger scrolling (Plus): Move events step the list under the finger one row per row height; other pages page per 90 px.
int touchScrollAcc=0;uint32_t touchScrollAt=0;uint32_t touchScrollSteps=0;
// Kinetic scrolling (Plus): a Fling keeps the list moving with the finger's speed, decaying with a 450 ms time constant
// like a phone; stops when the speed fades, the list hits its end, the finger lands again or the screen changes.
uint32_t flingStarts=0,flingStopEnd=0,flingStopSlow=0,moveEvents=0;long movePx=0;
struct FlingState{bool active=false;float v=0,rem=0;uint32_t last=0;long sig=0;} fling;constexpr float FlingTauMs=450.f;constexpr int FlingStopPxS=50;
static long scrollSignature(){
 return long(homeScroll)+7L*chatsTop+13L*chatOffset+41L*chatPixel+17L*settingsScroll+19L*nodeScroll+23L*wifiOffset+29L*filesScroll+37L*cliScrollLines;}
void flingStart(int vPxS){flingStarts++;fling.active=true;fling.v=float(vPxS);fling.rem=0;fling.last=millis();fling.sig=scrollSignature();}
void flingTick(){if(!fling.active)return;if(deviceLocked){fling.active=false;return;}if(touchPipeline.contactActive()){fling.active=false;flingCaught=true;return;}// a finger landing on a moving list only stops it
 uint32_t now=millis(),dt=now-fling.last;if(dt<8)return;fling.last=now;float move=fling.v*float(dt)/1000.f+fling.rem;int dy=int(move);fling.rem=move-float(dy);
 uint32_t steps=touchScrollSteps;if(dy)touchScroll(dy);
 if(touchScrollSteps!=steps){long sig=scrollSignature();if(sig==fling.sig){fling.active=false;flingStopEnd++;return;}fling.sig=sig;}// a step that moved nothing: the end of the list
 fling.v*=expf(-float(dt)/FlingTauMs);if(fabsf(fling.v)<float(FlingStopPxS)){fling.active=false;flingStopSlow++;}}
void touchScrollRows(int dy);
void touchScroll(int dy){if(deviceLocked)return;renderHold++;touchScrollRows(dy);renderHold--;if(!renderHold&&renderPending){renderPending=false;render();}}
void touchScrollRows(int dy){device::activity();uint32_t now=millis();if(uint32_t(now-touchScrollAt)>600)touchScrollAcc=0;touchScrollAt=now;touchScrollAcc+=dy;
 // fn(+1): the finger moves up, show later items; fn(-1): earlier items.
 auto step=[&](int px,std::function<void(int)> fn){while(touchScrollAcc>=px){touchScrollAcc-=px;touchScrollSteps++;fn(-1);}while(touchScrollAcc<=-px){touchScrollAcc+=px;touchScrollSteps++;fn(1);}};
 if(channelOptions||positionShare){return;}
 if(screen==Screen::Home){step(mm::MenuHomeRowH+mm::MenuHomeRowGap,[&](int d){homeScroll=constrain(homeScroll+d,0,max(0,chatCount-mm::MenuHomeRows));render();});return;}
 if(screen==Screen::Chats&&!messagesMenu){beginTouchBrowse();step(mm::ChatRowH,[&](int d){int n=conversationCount();chatsTop=constrain(chatsTop+d,0,max(0,n-chatsRows()));render();});return;}
 if(screen==Screen::Chat&&!(scopePicker||messageActions||messagePath||chatDetails||clearChatConfirm||emojiPicker)){{if(touchScrollAcc){chatPixel+=touchScrollAcc;touchScrollAcc=0;touchScrollSteps++;render();}return;}}
 if(screen==Screen::Section){
  if(section==19){beginTouchBrowse();step(mm::SettingsRowH,[&](int d){settingsScroll=constrain(settingsScroll+d,0,mm::settingsMaxScroll());render();});return;}
  if(section==1&&!contactSearching){beginTouchBrowse();step(mm::ContactRowH,[&](int d){int total=matter::contactsCount(discoverFilter,contactQuery.c_str());nodeScroll=constrain(nodeScroll+d,0,max(0,total-mm::ContactRows));render();});return;}
  if(section==4&&!wifiEditing){step(mm::WifiRowH,[&](int d){wifiOffset=constrain(wifiOffset+d,0,max(0,int(device::connectivityStatus().networkCount)-mm::WifiRows));render();});return;}
  if(section==7){step(mm::FileRowH,[&](int d){filesScroll=constrain(filesScroll+d,0,max(0,filesCount-mm::FileRows));render();});return;}
  if(section==mm::CliSection){step(13,[&](int d){auto v=cliState();mm::Renderer r(frame);cliScrollLines=max(0,min(r.cliMaxScroll(v),cliScrollLines-d));render();});return;}
 }
 step(90,[&](int d){swipe(d>0?-60:60);});
}
void swipe(int dy) {if(deviceLocked)return;
 device::activity();
 if(channelOptions||positionShare)return;
 if(screen==Screen::Section&&section==mm::KeyboardSection){int count=int(keyboardLanguageRows().size());kbdFocus=constrain(kbdFocus+(dy<0?mm::KeyboardRows:-mm::KeyboardRows),0,max(0,count-1));render();return;}
 if(screen==Screen::Home){homeScroll=constrain(homeScroll+(dy<0?1:-1),0,max(0,chatCount-mm::MenuHomeRows));render();return;}
 if(screen==Screen::PickContact){int start=pickerFocus/mm::PickerRows*mm::PickerRows+(dy<0?mm::PickerRows:-mm::PickerRows);if(start>=0&&start<int(pickerContacts.size())){pickerFocus=start;render();}return;}
 if(screen==Screen::Chat&&scopePicker){if(!scopeEditing){scopeFocus=constrain(scopeFocus+(dy<0?mm::ScopeRows:-mm::ScopeRows),0,int(regionScopes.size())-1);render();}return;}
 if(screen==Screen::Chat&&messageActions){if(contextMessage.mine){int count=repeaterRows(contextSnapshot()).size();heardOffset=constrain(heardOffset+(dy<0?mm::HeardRows:-mm::HeardRows),0,max(0,count-mm::HeardRows));render();}return;}
 if(screen==Screen::Chat&&messagePath){if(messageRouteView)return;scrollMessagePath(dy<0?5:-5);render();return;}
 if(screen==Screen::Chat){emojiPicker=false;scrollChat(dy>0?1:-1);return;}
 if(screen==Screen::Section){
  if(mm::toolsSection(section)){toolsScroll(dy);return;}
  if(section==2&&!radioPowerEditing&&!radioDevicePage){cycleRadioPreset(dy<0?1:-1);}
  else if(section==4&&!wifiEditing){wifiOffset=constrain(wifiOffset+(dy<0?mm::WifiRows:-mm::WifiRows),0,max(0,int(device::connectivityStatus().networkCount)-mm::WifiRows));render();}
  else if(section==1){contactPage(dy<0?1:-1);render();}
  else if(section==19){settingsScroll=mm::settingsPage(settingsScroll,dy<0);settingsFocus=constrain(settingsFocus,settingsScroll,mm::settingsLastVisible(settingsScroll));render();}
  else if(section==7){filesScroll=constrain(filesScroll+(dy<0?mm::FileRows:-mm::FileRows),0,max(0,filesCount-mm::FileRows));render();}
  return;
 }
 if(screen!=Screen::Chats)return;
 auto list=conversationList();if(list.empty())return;int at=-1;for(int i=0;i<int(list.size());i++)if(list[i].id==selected)at=i;// -1: the Messages menu row above the list
 if(at<0&&dy>=0)return;at=dy<0?min(int(list.size())-1,at+7):max(0,at-7);selected=list[at].id;render();// rows as shown (by activity), not chat ids
}
void initKeyboard() {
 keyboardReady=keyboard.begin(BOARD_I2C_ADDR_KEYBOARD,&Wire);
 if(keyboardReady){keyboardReady=keyboard.matrix(4,10);keyboard.enableDebounce();keyboard.enableInterrupts();keyboard.flush();}
 plus::Trackball::begin();trackballBegin();
}
// Accent key (Settings > Accent key, NVS "mmui"/"accent": 0 off, 1 right Shift, the default, 2 left Shift). Only in
// chats (the message composer and Quick Reply): tapped before a letter it picks the accented variant (1 tap the
// first, 2 taps the second ... up to 4, a fifth tap disarms); held, every letter typed meanwhile takes the first.
// Elsewhere, or when off, both Shift keys type capitals.
int accentMode=1,accentFocus=1;
static int accentCode(){return accentMode==1?31:accentMode==2?35:-1;}// TCA8418 codes: right Shift 31, left Shift 35
static bool accentContext(){return deviceLocked?quickReply.active&&quickReply.editable()&&!quickEmojiPicker:screen==Screen::Chat&&!emojiPicker&&editableField()==&input;}
static bool accentHeld(){int code=accentCode();return code>=0&&accentContext()&&(code==35?keyboardModifiers.leftShift:keyboardModifiers.rightShift);}
static bool shiftHeld(){int code=accentContext()?accentCode():-1;return (keyboardModifiers.leftShift&&code!=35)||(keyboardModifiers.rightShift&&code!=31);}
void loadAccentMode(){accentMode=constrain(device::prefGetInt("mmui","accent",1),0,2);accentFocus=accentMode;}
bool setAccentMode(int mode){if(mode<0||mode>2)return false;if(!device::prefPutInt("mmui","accent",mode))return false;accentMode=mode;accentReset();return true;}
void accentKey(bool pressed){if(pressed){if(deviceLocked)quickReplyInputAt=millis();accentTaps=accentTaps>=4?0:accentTaps+1;accentTyped=false;}else if(accentTyped){accentTaps=0;accentTyped=false;}if(deviceLocked)lockScreenDirty=true;else repaintSettled();}
static void typeText(const std::string &text){
 device::activity();
 if(deviceLocked){quickReplyInputAt=millis();quickReply.appendText(text);lockScreenDirty=true;return;}
 if(input.length()+text.size()<=messageByteLimit()){inputInsert(String(text.c_str()));notice="";}else notice=matter::messageLimitError(messageByteLimit()).c_str();
 repaintSettled();
}
// The accented letter for `taps` accent presses before it (0 = accent held); "" when the key has none in this language.
std::string accentedText(char ch,int taps){
 bool upper=ch>='A'&&ch<='Z';int index=taps>0?taps-1:0;
 if(index>=mm_kbd::variants(kbdLang,ch).count)return std::string();// no variant at this count: the key types plainly (the bar lists none)
 if(kbdLang==mm_kbd::Lang::Italian){// word rules (perché, È at a sentence start); two taps on e give the other e (è <-> é)
  std::string before=deviceLocked?quickReply.text:std::string(input.substring(0,cursorAt()).c_str());
  std::string text=device::accented(ch,before,upper);if(index==0)return text;
  static const std::pair<const char*,const char*> swap[]={{"\xc3\xa8","\xc3\xa9"},{"\xc3\xa9","\xc3\xa8"},{"\xc3\x88","\xc3\x89"},{"\xc3\x89","\xc3\x88"}};
  for(auto &p:swap)if(text==p.first)return p.second;return text;
 }
 return mm_kbd::variant(kbdLang,ch,index,upper);
}
// A letter key while the accent is armed or held: the accented letter is typed and the key consumed (true).
bool typeAccented(char ch,int taps){
 if(!accentContext())return false;
 auto text=accentedText(ch,taps);if(text.empty())return false;
 typeText(text);return true;
}
static bool accentLetter(char ch){
 if(!(accentTaps>0||accentHeld()))return false;
 int taps=accentTaps;bool held=accentHeld();accentTaps=0;if(held)accentTyped=true;
 return !keyboardModifiers.symbols()&&typeAccented(ch,taps);
}
// Settings > Keyboard languages: turn the focused language on or off (one always stays on).
void toggleKeyboardLanguage(int id){
 uint32_t bit=1u<<id;if((kbdMask&bit)&&mm_kbd::enabledCount(kbdMask)==1){notice="One language always stays on";render();return;}
 auto before=kbdLang;kbdMask^=bit;kbdLang=mm_kbd::active(kbdMask,kbdLang);
 // Not saved: back to what was on, so the page matches NVS (the mask may already have been written: write it back too)
 if(saveKeyboardLanguages())notice="";else{kbdMask^=bit;kbdLang=before;saveKeyboardLanguages();notice="Save failed";}render();
}
void switchKeyboardLanguage(){
 auto before=kbdLang;kbdLang=mm_kbd::next(kbdMask,kbdLang);// only the language changes: the enabled set is already saved
 bool saved=device::prefPutInt("mmkbd","lang",int(kbdLang));if(!saved)kbdLang=before;
 String text=saved?"Keyboard: "+String(mm_kbd::info(kbdLang).name):String("Save failed");
 if(deviceLocked)lockScreenDirty=true;else{if(screen==Screen::Chat)showChatError(text,1500);else notice=text;repaintSettled();}
}
void pollBanners(){
 if(deviceLocked)return;
 if(chatErrorUntil&&int32_t(millis()-chatErrorUntil)>=0){chatErrorUntil=0;chatError="";if(screen==Screen::Chat){repaintNow();}}
 static bool rxVisibleBefore=false;bool rxVisible=screen==Screen::Chat&&receivedRepeater.chat==activeChat&&receivedRepeater.visible(millis(),device::CHAT_RX_REPEATER_MS);if(rxVisible!=rxVisibleBefore){rxVisibleBefore=rxVisible;repaintNow();}bool rptVisible=screen==Screen::Chat&&(firstRepeater.visible(activeChat,millis())||rxVisible);if(rptVisible!=firstRepeaterPainted){repaintNow();}
}
void keyboardEvent(int event,uint32_t capturedAt=0,bool hardwareDebounced=false) {
 bool pressed=event&0x80;int code=event&0x7f;

 if(deviceLocked){
  if(code!=21&&pressed&&quickCountdownAt){quickCountdownAt=0;lockEnter.consumed=true;lockShowNext(pendingQuickOriginal.token);return;}// any other key: no reply, mark read / next message
  if(code==21){
   uint32_t now=capturedAt?capturedAt:millis();
   if(pressed){
    if(!quickReply.active)device::wakeDisplay(10000);// Plus: a quick ENTER lights the lock screen for 10 s
    if(lockEnter.press(now,hardwareDebounced)&&!quickReply.active){resolveQuickOriginal();
     if(pendingQuickOriginal.token&&!quickCountdownAt){quickCountdownAt=millis();lockHintUntil=0;lockScreenDirty=true;}}}
   else{auto action=lockEnter.release(now,hardwareDebounced);if(quickReply.active){if(action==device::EnterAction::Short){if(quickEmojiPicker)insertEmoji(emojiFocus);else sendQuickReply();}}}// the countdown opens the quick reply; without a message ENTER only lights the screen
  }else if(quickReply.active){
   if(code==accentCode()&&(!pressed||accentContext()))accentKey(pressed);
   if(keyboardModifiers.update(code,pressed))return;
   if(code==device::SPACE_KEYCODE){if(pressed&&keyboardModifiers.alt&&mm_kbd::enabledCount(kbdMask)>=2){switchKeyboardLanguage();return;}if(pressed){accentReset();quickReplyKey(' ');}return;}
   int k=code-1;if(k<0||k>=40)return;if(code==device::MICROPHONE_KEYCODE){if(pressed){accentReset();if(keyboardModifiers.symbols())quickReplyKey('0');}return;}
   char ch=keys[k/10][9-k%10];
   if(ch=='\b'){deleteHeld=pressed;if(pressed){accentReset();deleteAt=repeatAt=millis();quickReplyKey('\b');}return;}
   if(pressed){if(keyboardModifiers.symbols())ch=symbols[k/10][9-k%10];else if(shiftHeld()&&ch>='a'&&ch<='z')ch=ch-'a'+'A';if(accentLetter(ch))return;if(ch)quickReplyKey(ch);}
  }else if(pressed)showLockHint();// Plus: the trackball unlocks, Space only shows the hint
  return;
 }
 // Alt + Space: next keyboard language; otherwise Space is only a space (the Plus locks with the trackball)
 if(code==device::SPACE_KEYCODE){if(pressed&&keyboardModifiers.alt&&mm_kbd::enabledCount(kbdMask)>=2){device::activity();switchKeyboardLanguage();return;}if(pressed){accentReset();device::activity();key(' ');}return;}
 if(pressed)device::activity();int k=(event&0x7f)-1;
 if(k<0||k>=40)return;
 if(code==device::MICROPHONE_KEYCODE){if(pressed){accentReset();key('0');}return;}// no dictation on the Plus: the MIC key is the digit 0
 keyboardEvents++;if(code==accentCode()&&(!pressed||accentContext()))accentKey(pressed);if(keyboardModifiers.update(code,pressed)){repaintSettled();return;}int r=k/10,c=9-k%10;char ch=keys[r][c];
 if(screen==Screen::Chat&&pressed&&keyboardModifiers.alt&&(keyboardModifiers.leftShift||keyboardModifiers.rightShift)&&editableField()==&input&&(ch=='w'||ch=='s')){if(accentHeld())accentTyped=true;scrollChat(ch=='w'?1:-1);return;}// the accent Shift used here does not stay armed
 if(screen==Screen::Section&&section==4&&wifiEditing&&pressed&&keyboardModifiers.alt&&ch=='v'){wifiShowPassword=!wifiShowPassword;render();return;}
 if(screen==Screen::Section&&section==mm::CliSection&&pressed&&keyboardModifiers.alt&&cliAltKey(ch))return;
 if(ch=='\b'){
  auto field=editableField();deleteHeld=pressed&&field&&field->length();
  if(pressed){accentReset();deleteAt=repeatAt=millis();key('\b');}return;
 }
 if(!pressed)return;
 if(keyboardModifiers.symbols())ch=symbols[r][c];
 else if(shiftHeld()&&ch>='a'&&ch<='z')ch=ch-'a'+'A';
 if(accentLetter(ch))return;
 // Menu shortcuts remain case-insensitive when caps mode is enabled.
 ch=device::keyboardCase(ch,(screen==Screen::Chats&&!searching)||(screen==Screen::PickContact&&!pickerSearching)||screen==Screen::Home||screen==Screen::Launcher||screen==Screen::Section,editableField()!=nullptr);
 if(event&128)device::activity();
 if(ch)key(ch);
}
void captureKeyEvent(int event,uint32_t now){keyboardQueue.push(event,now,true);}
void captureKeyboard(){
 if(!keyboardReady)return;
 uint8_t status=keyboard.readRegister(TCA8418_REG_INT_STAT);
 if(status&TCA8418_REG_STAT_OVR_FLOW_INT){keyboardHardwareOverflows++;keyboardQueue.invalidate();}
 // Bound a drain even if the controller reports a stuck event count.
 for(int n=0;n<32&&keyboard.available();n++){int event=keyboard.getEvent();if(!event)break;captureKeyEvent(event,millis());}
 keyboard.writeRegister(TCA8418_REG_INT_STAT,TCA8418_REG_STAT_K_INT|TCA8418_REG_STAT_GPI_INT|TCA8418_REG_STAT_OVR_FLOW_INT);
}
void pollKeyboard() {
 if(!keyboardReady)return;static uint32_t polled=0;
 bool idle=!keyboard.poll();// no interrupt line from the C3: the adapter polls every 8 ms
 if(idle&&keyboardQueue.size()==0&&!deleteHeld&&millis()-polled<500)return;polled=millis();captureKeyboard();
 if(keyboardQueue.discardOverflow()){deleteHeld=false;keyboardModifiers.reset();lockEnter.reset();lockKey.down=lockKey.consumed=false;lockKey.progress=0;unlockTaps.reset();device::unlockLightPreview(quickReply.active);if(deviceLocked)restoreLockedCpu();}
 device::InputEvent event;batchingInput=true;
 while(keyboardQueue.pop(event)){
  keyboardEvent(event.raw,event.at,event.hardwareDebounced);
 }
 batchingInput=false;unsigned long now=millis();if(deleteHeld&&now-deleteAt>500&&now-repeatAt>100){if(quickReply.active){if(quickReply.editable()&&!quickReply.text.empty()){repeatAt=now;quickReplyKey('\b');}else deleteHeld=false;}else{auto field=editableField();if(field&&field->length()){repeatAt=now;key('\b');}else deleteHeld=false;}}
}
// Navigation database store for the GPS (gps_backup.h): one file on the SD card, written via a temporary name.
bool gpsStoreLoad(std::vector<uint8_t> &out){if(!sdReady)return false;File f=SD.open(device::sdSavedPath(SD,"/mattermesh/gps_nav.bin","/mattermesh/gps_nav.bak"));if(!f)return false;size_t n=f.size();if(n<device::GpsBackup::HeaderSize||n>device::GpsBackup::MaxBytes){f.close();return false;}out.resize(n);size_t got=f.read(out.data(),n);f.close();return got==n;}
bool gpsStoreSave(const std::vector<uint8_t> &data){if(!sdReady||data.empty())return false;File f=SD.open("/mattermesh/gps_nav.tmp",FILE_WRITE);if(!f)return false;bool ok=f.write(data.data(),data.size())==data.size();f.close();if(!ok){SD.remove("/mattermesh/gps_nav.tmp");return false;}return device::sdReplace(SD,"/mattermesh/gps_nav.tmp","/mattermesh/gps_nav.bin","/mattermesh/gps_nav.bak");}
int channelAtPoint(int x,int y){if(deviceLocked||channelOptions)return -1;for(auto &hit:paintedChannels)if(hit.area.contains(x,y))return hit.id;return -1;}

void longPressChannel(int x,int y){int id=channelAtPoint(x,y);if(id>=0){device::activity();openChannelOptions(id);}}
uint32_t touchScene(){uint32_t hash=2166136261u;auto add=[&](uint32_t value){hash=(hash^value)*16777619u;};add(int(screen));add(section);add(activeChat);if(screen==Screen::Chat&&!deviceLocked)add(chatRowsRevision);add(channelOptions);add(channelKeyView);add(deleteChannelConfirm);add(positionShare);add(chatDetails);add(scopePicker);add(scopeEditing);add(clearChatConfirm);add(messageActions);add(messagePath);add(messageBodyView);add(emojiPicker);add(searching);add(messagesMenu);add(contactSearching);add(discoverFilter);add(shutdownConfirm);add(radioPowerEditing);add(radioDevicePage);add(wifiEditing);add(chatOffset);add(nodeScroll);add(settingsScroll);add(filesScroll);add(wifiOffset);if(screen==Screen::Chats)add(selected);add(deviceLocked);add(quickReply.active);add(quickReply.pending);add(quickReply.sent);return hash;}
struct UiReplay {int kind=0,step=0;uint32_t started=0;bool released=false;};UiReplay uiReplay;uint32_t injectedTouchUntil=0;int injectedTouchX=0,injectedTouchY=0;
// "touch drag x y0 y1 ms": a finger sliding at constant speed, sampled like the real panel (test of drag and fling).
struct InjectedDrag{bool on=false,ended=false;int x=0,y0=0,y1=0;uint32_t start=0,ms=1;} injectedDrag;
void feedTouch(bool down,int x,int y,uint32_t now){
 bool allowed=!(panelPainting&&touchTransition);int target=-1;std::string key;
 if(down){device::activity();touchEvents++;}
 if(touchPipeline.needsTarget(down,allowed)){target=channelAtPoint(x,y);}
 touchPipeline.feed(down,x,y,now,touchScene(),target,key,allowed);
}
void captureTouch(bool force){
 if((deviceLocked&&!quickReply.active)||!touchBackend||!touchDisplayReady||uiReplay.kind==1)return;static uint32_t polled=0;uint32_t now=millis();if(!force&&uint32_t(now-polled)<device::TOUCH_POLL_MS)return;polled=now;
 if(injectedDrag.on){uint32_t t=now-injectedDrag.start;if(t<=injectedDrag.ms){feedTouch(true,injectedDrag.x,injectedDrag.y0+int(long(injectedDrag.y1-injectedDrag.y0)*long(t)/long(injectedDrag.ms)),now);return;}if(!injectedDrag.ended){injectedDrag.ended=true;feedTouch(true,injectedDrag.x,injectedDrag.y1,injectedDrag.start+injectedDrag.ms);return;}/* the panel still reports the last point */injectedDrag.on=false;feedTouch(false,0,0,now);return;}
 if(injectedTouchUntil&&int32_t(injectedTouchUntil-now)>0){feedTouch(true,injectedTouchX,injectedTouchY,now);return;}injectedTouchUntil=0;
 int16_t x=0,y=0;bool down;
 down=legacyTouch.getPoint(&x,&y,1)>0;
 feedTouch(down,x,y,now);
}
void serviceReplay(){if(!uiReplay.kind)return;uint32_t now=millis();if(!uiReplay.started)uiReplay.started=now;uint32_t age=now-uiReplay.started;if(uiReplay.kind==1){if(age>=100&&age<160)feedTouch(true,mm::ComposerEmoji.x+15,mm::ComposerEmoji.y+15,now);else if(age>=160){feedTouch(false,0,0,now);if(age>=200)uiReplay.kind=0;}}else if(uiReplay.kind==3){const uint32_t times[]={100,160,220,220};const int edges[]={33,161,33,33};while(uiReplay.step<4&&age>=times[uiReplay.step])captureKeyEvent(edges[uiReplay.step++],now);if(uiReplay.step==4)uiReplay.kind=0;}else if(age>=100){captureKeyEvent(device::MICROPHONE_KEYCODE,now);uiReplay.released=true;uiReplay.kind=0;}}
void pollTouch(){
 captureTouch();device::TouchAction event;if(!touchPipeline.pop(event))return;
 const bool relative=event.kind==device::TouchActionKind::Move||event.kind==device::TouchActionKind::Fling;// gestures, not hits on content: never stale
 if((deviceLocked&&!quickReply.active)||(!relative&&event.scene!=touchScene())){touchStale++;return;}
 lastTouchDispatchMs=millis()-event.releasedAt;maxTouchDispatchMs=max(maxTouchDispatchMs,lastTouchDispatchMs);touchDispatched++;
 if(flingCaught&&event.kind!=device::TouchActionKind::Move&&event.kind!=device::TouchActionKind::Fling){flingCaught=false;if(event.kind==device::TouchActionKind::Tap)return;}else if(event.kind==device::TouchActionKind::Move)flingCaught=false;
 if(event.kind==device::TouchActionKind::Hold){if(event.target>=0&&event.target<chatCount){openChannelOptions(event.target);}}
 else if(event.kind==device::TouchActionKind::Move){moveEvents++;movePx+=event.dy;if(screen==Screen::Section&&section==9)mapPan(-event.dx,-event.dy);else if(screen==Screen::Section&&section==mm::LinkSection)linkDrag(event.dx,event.dy);else touchScroll(event.dy);}
 else if(event.kind==device::TouchActionKind::Fling){if(!(screen==Screen::Section&&(section==9||section==mm::LinkSection)))flingStart(event.dy);}
 else if(screen==Screen::Section&&section==9&&(event.kind==device::TouchActionKind::Swipe||event.kind==device::TouchActionKind::Drag))mapPan(-event.dx,-event.dy);
 else if(screen==Screen::Section&&section==mm::LinkSection&&(event.kind==device::TouchActionKind::Swipe||event.kind==device::TouchActionKind::Drag))linkDrag(event.dx,event.dy);
 else if(event.kind==device::TouchActionKind::Swipe)swipe(event.dy);
 else if(event.kind==device::TouchActionKind::Drag)return;
 else if(event.target>=0&&event.target<chatCount&&(screen==Screen::Home||screen==Screen::Chats)){openConversation(event.target,screen,section);}
 else tap(event.x,event.y);
}
static bool touchDisabled=false;// locked: touch handling off (LOCK_STATUS touch_disabled)
void setTouchEnabled(bool enabled){injectedTouchUntil=0;touchPipeline.reset();paintedChannels.clear();touchDisabled=!enabled;if(enabled){touchPipeline.waitForRelease();initTouch();}}

// Locked: 80 MHz (the PSRAM floor, lock_policy.h); a quick reply and background network work get full speed. No state
// copies every 500 ms.
uint32_t requestedLockedCpu(){return quickReply.active||device::networkJobActive()?device::NORMAL_CPU_FREQUENCY:device::LOCKED_CPU_FREQUENCY;}
void restoreLockedCpu(){uint32_t wanted=requestedLockedCpu();if(!locked40Supported&&wanted<80)wanted=80;if(getCpuFrequencyMhz()!=wanted&&!setCpuFrequencyMhz(wanted)){locked40Supported=false;setCpuFrequencyMhz(device::LOCKED_CPU_FALLBACK);Serial.println("LOCK_CPU_FALLBACK");}}
void enterLowPowerMode(){device::powerLocked(true);device::connectivityLowPower(true);restoreLockedCpu();}
void exitLowPowerMode(){setCpuFrequencyMhz(device::NORMAL_CPU_FREQUENCY);device::connectivityLowPower(false);device::powerLocked(false);}
std::vector<uint32_t> lockMessageTokens(){return device::unreadMessageTokens(radioJournal.index,unreadMessages,size_t(chatCount),64);}
void showLockHint(){lockHintUntil=millis()+device::LOCK_HINT_MS;lockScreenDirty=true;}
bool consumeLockMessage(uint32_t token){
 if(!token)return true;auto eligible=lockMessageTokens();if(std::find(eligible.begin(),eligible.end(),token)==eligible.end())return true;
 if(auto e=radioJournal.index.find(token))if(!e->mine){if(!radioJournal.markOneRead(e->chat,token)){lockReadError="Read not saved / try again";return false;}device::restoreUnreadCounts(radioJournal.index,radioJournal.readTokens,unreadMessages,size_t(chatCount));if(token==lastReceivedToken)lastReceivedRead=true;lockReadError.clear();return true;}return true;
}
// The lock screen message being answered: the one painted now (a newer one may have arrived during the countdown).
static void resolveQuickOriginal(){pendingQuickOriginal=device::ReplyOriginal{};matter::Record r;if(loadedRecord(paintedLockToken,r)&&!r.mine&&r.chat>=0&&r.chat<chatCount)pendingQuickOriginal=replyOriginalFrom(r);}
// Another key during the countdown: no reply; mark the shown message read and step to the next one.
static void lockShowNext(uint32_t token){lockHintUntil=0;if(consumeLockMessage(token)){if(lockMessages.current==token||!lockMessages.current)lockMessages.next(lockMessageTokens());else lockMessages.receive(lockMessageTokens(),lockMessages.current);}lockMessageShownAt=millis();lockScreenDirty=true;}
static int quickCountdownDots(){return min(2,int((millis()-quickCountdownAt)*2/device::QUICK_REPLY_HOLD_MS)+1);}
void openQuickReply(){
 quickEmojiPicker=false;accentReset();quickReplyInputAt=millis();if(deviceLocked&&!quickReply.active)resolveQuickOriginal();
 if(!deviceLocked||quickReply.active||!quickReply.open(pendingQuickOriginal,messageByteLimit(pendingQuickOriginal.chat))){quickCountdownAt=0;lockScreenDirty=true;return;}
 quickCountdownAt=0;if(lockEnter.down)lockEnter.consumed=true;// the ENTER still held must not send the empty reply
 unlockTaps.reset();unlockCountdownAt=lockHintUntil=0;keyboardModifiers.reset();deleteHeld=false;
 setCpuFrequencyMhz(device::NORMAL_CPU_FREQUENCY);device::unlockLightPreview(true);setTouchEnabled(true);lockScreenDirty=true;render(true);
}
void closeQuickReply(){
 quickEmojiPicker=false;accentReset();
 quickReply.close();keyboardModifiers.reset();deleteHeld=false;if(lockEnter.down)lockEnter.consumed=true;
 unlockTaps.reset();unlockCountdownAt=lockHintUntil=0;device::unlockLightPreview(false);setTouchEnabled(false);restoreLockedCpu();lockScreenDirty=true;render(true);
}
void sendQuickReply(){
 quickEmojiPicker=false;accentReset();quickReplyInputAt=millis();
 if(!quickReply.editable())return;
 uint32_t token=0;
 bool accepted=quickReply.send([&](int chat,const std::string &scope,const std::string &body,std::string &error){String region=scope.c_str();bool ok=queueRadioMessage(body.c_str(),region.isEmpty()?nullptr:&region,chat,&token);error=notice.c_str();return ok;});
 if(accepted)quickReply.txToken=token;
 lockScreenDirty=true;render(true);
}
uint32_t quickReplyInputAt=0;// last key or touch in the quick reply (pollQuickReply closes it after a minute idle)
void quickReplyKey(char c){quickReplyInputAt=millis();
 if(!quickReply.active)return;
 if(quickEmojiPicker){quickEmojiKey(c);return;}
 if(c==3){closeQuickReply();return;}if(c=='\n'){sendQuickReply();return;}
 if(c=='\b'){if(quickReply.text.empty()||!quickReply.editable()){closeQuickReply();return;}quickReply.erase();}
 else if(c>=32&&c<=126){auto cyr=layoutLetter(c);if(cyr.empty())quickReply.append(c);else quickReply.appendText(cyr);}
 lockScreenDirty=true;
}
void pollQuickReply(){
 if(quickTxReady){quickTxReady=false;uint32_t original=quickReply.original.token;
  if(quickReply.complete(quickTxToken,quickTxSuccess)){quickReplyInputAt=millis();// the result gets its own minute
   if(quickTxSuccess){consumeLockMessage(original);lockMessages.reset(lockMessageTokens());lockMessageShownAt=millis();lockScreenDirty=true;}
   else lockScreenDirty=true;
  }
 }
 if(quickReply.confirmationExpired(millis()))closeQuickReply();
 // Left open in a pocket: a minute without keys or touches closes it (the text is not sent).
 if(quickReply.active&&quickReply.editable()&&uint32_t(millis()-quickReplyInputAt)>=60000UL)closeQuickReply();// a send waiting for the radio, or its result, stays

 if(quickCountdownAt){if(!deviceLocked||quickReply.active)quickCountdownAt=0;else{uint32_t held=millis()-quickCountdownAt;static int paintedDots=-1;int dots=quickCountdownDots();
  if(held>=device::QUICK_REPLY_HOLD_MS){paintedDots=-1;openQuickReply();}else if(dots!=paintedDots){paintedDots=dots;lockScreenDirty=true;}}}
 lockEnter.recover(millis());static bool wasBoot=false;bool boot=digitalRead(BOARD_BOOT_PIN)==LOW;if(quickReply.active&&boot&&!wasBoot)closeQuickReply();if(quickCountdownAt&&boot&&!wasBoot){quickCountdownAt=0;lockEnter.consumed=true;lockScreenDirty=true;}wasBoot=boot;
}
void enterLockMode(){
 device::audioCancel();previousScreen=screen;deviceLocked=true;quickTxReady=false;quickReply.close();quickCountdownAt=0;quickEmojiPicker=false;lockReadError.clear();lockEnter.reset();trackballUnlockTaps=0;unlockTaps.reset();lockMessages.reset(lockMessageTokens());lockMessageShownAt=millis();lockHintUntil=unlockCountdownAt=0;lockHintPainted=false;renderedUnlockTaps=-1;
 chatErrorUntil=0;deleteHeld=false;keyboardModifiers.reset();accentReset();setTouchEnabled(false);enterLowPowerMode();lockMinute=time(nullptr)/60;lockScreenDirty=false;
 bool batched=batchingInput;batchingInput=false;render();batchingInput=batched;
}
void exitLockMode(){
 exitLowPowerMode();deviceLocked=false;accentReset();quickCountdownAt=0;quickEmojiPicker=false;trackballUnlockTaps=0;unlockTaps.reset();lockHintUntil=unlockCountdownAt=0;lockKey.down=lockKey.consumed=false;lockKey.progress=0;
 quickReply.close();lockEnter.reset();if(activeChat>=0&&activeChat<MAX_CHATS&&!input.isEmpty()){chatDrafts.saved[activeChat]=input;input="";inputCursor=-1;}
 scopePicker=scopeEditing=clearChatConfirm=chatDetails=messageActions=messagePath=emojiPicker=channelOptions=deleteChannelConfirm=positionShare=false;scopeInput="";notice="";navigation.clear();
 screen=Screen::Home;homeSelection=-1;
 setTouchEnabled(true);radioDirty=false;lockScreenDirty=false;bool batched=batchingInput;batchingInput=false;render();batchingInput=batched;
}
void updateLockScreen(){if(!deviceLocked)return;static bool viaPainted=false;bool via=uint32_t(millis()-lockMessageShownAt)<device::LOCK_RX_REPEATER_MS;if(via!=viaPainted){viaPainted=via;lockScreenDirty=true;}uint32_t minute=time(nullptr)/60;if(minute!=lockMinute){lockMinute=minute;lockScreenDirty=true;}uint32_t settle=quickReply.active?UI_INPUT_SETTLE_MS:500;if(lockScreenDirty&&!matter::busy()&&millis()-lastPaint>=settle){lockScreenDirty=false;render(true);}}
void handleLockKey(bool pressed,uint32_t capturedAt,bool hardwareDebounced){
 uint32_t now=capturedAt?capturedAt:millis();
 if(deviceLocked){
  if(quickReply.active){if(getCpuFrequencyMhz()!=device::NORMAL_CPU_FREQUENCY)setCpuFrequencyMhz(device::NORMAL_CPU_FREQUENCY);return;}
  showLockHint();if(unlockCountdownAt)return;
  if(pressed){
   device::wakeDisplay(10000);// LCD: one SPACE press shows the lock screen for 10 s
   if(unlockTaps.press(now,hardwareDebounced)){setCpuFrequencyMhz(device::NORMAL_CPU_FREQUENCY);device::unlockLightPreview(true);}}
  else if(unlockTaps.release(now,hardwareDebounced)){unlockCountdownAt=millis();}
  lockScreenDirty=true;return;
 }
}
void pollLockKey(){
 uint32_t now=millis();
 if(deviceLocked){
  if(quickReply.active){if(getCpuFrequencyMhz()!=device::NORMAL_CPU_FREQUENCY)setCpuFrequencyMhz(device::NORMAL_CPU_FREQUENCY);return;}
  if(unlockCountdownAt&&uint32_t(now-unlockCountdownAt)>=device::UNLOCK_COUNTDOWN_MS){exitLockMode();return;}
  bool held=unlockTaps.down;if(unlockTaps.expire(now)){if(held)lockRecoveries++;device::unlockLightPreview(false);restoreLockedCpu();lockScreenDirty=true;}
  bool hint=lockHintUntil&&int32_t(lockHintUntil-now)>0;if(hint!=lockHintPainted||unlockTaps.taps!=renderedUnlockTaps){lockHintPainted=hint;lockScreenDirty=true;}
  static bool jobSeen=false;bool job=device::networkJobActive();if(job!=jobSeen){jobSeen=job;restoreLockedCpu();}
  if(!unlockTaps.active()&&millis()-lockedCpuPollAt>=500){lockedCpuPollAt=millis();restoreLockedCpu();}return;
 }
}

void list(File dir,const String &prefix) {
 while(true){File f=dir.openNextFile();if(!f)break;String p=prefix+"/"+String(f.name());
 Serial.printf("%s %u %s\n",f.isDirectory()?"DIR":"FILE",(unsigned)f.size(),p.c_str());f.close();}
}
// Each Journal holds CHAT_SLOTS (32) chat summaries (~3.5 KiB): heap-allocate, never inline into handle().
__attribute__((noinline)) bool chatStorageSelftest(const String &path){
 std::unique_ptr<matter::Journal> test(new matter::Journal),boot(new matter::Journal),next(new matter::Journal);
 test->begin(SD,path.c_str());matter::Record one;one.chat=0;one.token=1;one.body="isolated fixture";bool ok=test->append(one);one.chat=1;one.token=2;ok=ok&&test->append(one);ok=ok&&test->clearChat(0,100);
 boot->begin(SD,path.c_str());ok=ok&&boot->records.size()==1&&boot->records[0].chat==1&&boot->localClearThrough[0]==100;one.chat=0;one.token=3;ok=ok&&boot->append(one);
 next->begin(SD,path.c_str());return ok&&next->records.size()==2;
}
void handle(const String &s) {
 if(device::sdTransferCommand(s))return;
 if(s=="catalog status"){
  Serial.printf("CATALOG_STATUS total=%d sd=%d searching=%d cached_messages=%u cache_limit=%u\n",chatCount,sdReady,searching,unsigned(radioJournal.records.size()),unsigned(matter::JOURNAL_CACHE_RECORDS));
  for(int id=0;id<chatCount;id++){int retained=0;for(auto &record:radioJournal.records)if(record.chat==id)retained++;Serial.printf("CATALOG_ROW id=%d name=%s direct=%d retained=%d\n",id,chats[id].name.c_str(),chats[id].direct,retained);}
  return;
 }
 if(s=="quick reply preview"){
  if(deviceLocked&&!quickReply.active)for(auto it=radioJournal.index.rbegin();it!=radioJournal.index.rend();++it)if(!it->mine&&it->chat>=0&&it->chat<chatCount){matter::Record rec;if(!loadedRecord(it->token,rec))break;pendingQuickOriginal=replyOriginalFrom(rec);openQuickReply();break;}
  return;
 }
 if(s=="wallpaper status"){Serial.printf("WALLPAPER index=%d name=%s focus=%d count=%d\n",lockWallpaper,WALLPAPER_NAME(lockWallpaper),wallpaperFocus,WALLPAPER_COUNT);return;}
 if(s=="map style"||s=="map style light"||s=="map style night"){int want=s.endsWith("night")?1:s.endsWith("light")?0:-1;if(want>=0&&want!=mapStyleIndex)mapToggleStyle();else if(want<0&&screen==Screen::Section&&section==9)mapToggleStyle();Serial.printf("MAP_STYLE style=%d look=%s\n",mapStyleIndex,mapStyleIndex?"night":"light");return;}
 if(s=="storm status"){auto st=device::stormState();Serial.printf("STORM_STATUS phase=%d count=%u updated=%lu alerts=%d message=%s\n",int(st.phase),st.count,(unsigned long)st.updated,st.alerts,st.message.c_str());for(int i=0;i<st.count;i++)Serial.printf("STORM_EVENT %lu %lu kinds=%u gust=%.0f rain=%.1f cape=%.0f\n",(unsigned long)st.events[i].start,(unsigned long)st.events[i].end,st.events[i].kinds,st.events[i].gust,st.events[i].rain,st.events[i].cape);return;}
 if(s=="storm check"){device::stormRefresh();Serial.println("STORM_CHECK queued");return;}
 if(s.startsWith("time set ")){int v[5];if(sscanf(s.c_str()+9,"%d-%d-%d %d:%d",&v[2],&v[1],&v[0],&v[3],&v[4])==5){for(int i=0;i<5;i++)dtField[i]=v[i];String reason;bool ok=dtApply(reason);Serial.printf("TIME_SET ok=%d %s\n",ok,reason.c_str());}else Serial.println("TIME_SET ok=0 use: time set YYYY-MM-DD HH:MM");render();return;}
 if(s.startsWith("accent set ")){bool ok=setAccentMode(s.substring(11).toInt());accentFocus=accentMode;Serial.printf("ACCENT_SET ok=%d mode=%d\n",ok,accentMode);render();return;}
 if(s=="composer text"){Serial.print("COMPOSER_TEXT hex=");for(unsigned i=0;i<input.length();i++)Serial.printf("%02x",uint8_t(input[i]));Serial.printf(" quick=");for(auto ch:quickReply.text)Serial.printf("%02x",uint8_t(ch));Serial.println();return;}
 if(s=="keyboard languages"){Serial.printf("KEYBOARD_LANGUAGES mask=%lu current=%s enabled=%d focus=%d searching=%d query_len=%u taps=%d\n",(unsigned long)kbdMask,mm_kbd::info(kbdLang).code,mm_kbd::enabledCount(kbdMask),kbdFocus,kbdSearching,unsigned(kbdQuery.length()),accentTaps);return;}
 if(s=="accent status"){Serial.printf("ACCENT_STATUS mode=%d key=%d context=%d taps=%d held=%d shift=%d\n",accentMode,accentCode(),accentContext(),accentTaps,accentHeld(),shiftHeld());return;}
 if(s.startsWith("textsize set ")){bool ok=setTextSize(s.substring(13).toInt());textSizeFocus=chatTextSize;Serial.printf("TEXTSIZE_SET ok=%d large=%d\n",ok,chatTextSize);render();return;}
 if(s.startsWith("wallpaper set ")){int index=s.substring(14).toInt();bool ok=setWallpaper(index);wallpaperFocus=lockWallpaper;Serial.printf("WALLPAPER_SET ok=%d index=%d name=%s\n",ok,lockWallpaper,WALLPAPER_NAME(lockWallpaper));render();return;}
 if(s=="quick reply status"){Serial.printf("QUICK_REPLY sent=%d active=%d locked=%d cpu=%u chat=%d token=%lu painted=%lu kind=%d editable=%d bytes=%u enter_down=%d hops=%d\n",quickReply.sent,quickReply.active,deviceLocked,getCpuFrequencyMhz(),quickReply.original.chat,quickReply.original.token,paintedLockToken,int(quickReply.plan.kind),quickReply.editable(),unsigned(quickReply.text.size()),lockEnter.down,quickReply.original.hops);return;}
 if(s=="map cleanup step"){device::mapCleanupTrace=true;device::mapTilesCleanup();device::mapCleanupTrace=false;Serial.println("MAP_CLEANUP_DONE");return;}// diagnostics: one cleanup step, traced
 if(s=="ui unlock"){if(deviceLocked&&!quickReply.active)exitLockMode();Serial.printf("UI_UNLOCK locked=%d\n",deviceLocked);return;}// tests: the Plus unlocks with the trackball only
 if(s=="quick reply cancel"){if(deviceLocked&&quickReply.active)closeQuickReply();return;}
 if(s=="meshcore status"){Serial.printf("MESHCORE_STATUS version=%s commit=%s\n",MM_MESHCORE_VERSION,MM_MESHCORE_COMMIT);return;}
 if(s=="lock messages status"){auto eligible=lockMessageTokens();auto state=uiState();Serial.printf("LOCK_MESSAGES browsing=%d ended=%d current=%lu position=%d count=%u unread=%u latest=%lu rpt=%d hint=%d countdown=%d\n",lockMessages.browsing,lockMessages.ended,lockMessages.current,lockMessages.position,unsigned(eligible.size()),state.unread,lastReceivedToken,!state.lockRepeater.empty(),lockHintUntil&&int32_t(lockHintUntil-millis())>0,unlockCountdownAt!=0);return;}
 if(s=="lock messages list"){StaticJsonDocument<1024> json;auto tokens=json.to<JsonArray>();for(auto id:lockMessageTokens())tokens.add(id);Serial.print("LOCK_MESSAGE_IDS ");serializeJson(json,Serial);Serial.println();return;}
 if(s=="wifi status"){auto w=device::connectivityStatus();Serial.printf("WIFI_STATUS on=%d connected=%d connecting=%d scanning=%d saved=%d networks=%u paused=%d\n",w.wifi,w.connected,w.connecting,w.scanning,w.remembered,unsigned(w.networkCount),w.reconnectPaused);return;}
 if(s=="wifi link"){auto w=device::connectivityStatus();StaticJsonDocument<256> json;json["connected"]=w.connected;json["ssid"]=w.connected?w.ssid:String("");json["address"]=w.address;json["rssi"]=w.rssi;json["message"]=w.message;Serial.print("WIFI_LINK ");serializeJson(json,Serial);Serial.println();return;}
 if(s=="wifi editor status"){Serial.printf("WIFI_EDITOR editing=%d hidden=%d visible=%d field=%d ssid_bytes=%u password_bytes=%u\n",wifiEditing,wifiHidden,wifiShowPassword,wifiEditField,wifiTarget.length(),wifiPassword.length());return;}
 if(s.startsWith("ble name ")){String n=s.substring(9);if(n=="-")n="";bool saved=appSetBleName(n);Serial.printf("BLE_NAME saved=%d name=%s\n",saved,device::bleCompanionName().c_str());return;}// "-" = back to the node name
 if(s.startsWith("link open ")){std::string url=s.substring(10).c_str();linkHistory.clear();linkGo(url,false);Serial.printf("LINK_OPEN %s\n",url.c_str());return;}
 if(s=="link status"){auto st=device::linkState();Serial.printf("LINK_STATUS open=%d phase=%d host=%s bytes=%lu/%lu image=%dx%d source=%dx%d text=%u links=%u og_image=%d zoom=%d scroll=%d rows=%u history=%u message=%s %s\n",screen==Screen::Section&&section==mm::LinkSection,int(st.phase),st.host.c_str(),(unsigned long)st.received,(unsigned long)st.total,st.imageW,st.imageH,st.sourceW,st.sourceH,unsigned(st.text.size()),unsigned(st.links.size()),!st.imageUrl.empty(),linkZoom,linkScroll,unsigned(linkAllRows.size()),unsigned(linkHistory.size()),st.message.c_str(),device::linkMemory().c_str());return;}
 if(s=="link memory"){Serial.printf("LINK_MEMORY %s\n",device::linkMemory().c_str());return;}
 if(s=="app status"){Serial.printf("APP_STATUS ble=%d ble_name=%s pin=%lu usb=%d wifi_enabled=%d wifi_server=%d wifi_client=%d\n",device::bleCompanionConnected(),device::bleCompanionName().c_str(),(unsigned long)appPinValue,appUsbLinked(),appWifiEnabled,appServerOn,appClient&&appClient.connected());return;}
 if(s=="app wifi"){appWifiToggle();Serial.printf("APP_WIFI %d\n",appWifiEnabled);return;}
 if(s=="welcome start"){startWelcome();Serial.println("WELCOME_STARTED");return;}
 if(s=="welcome status"){Serial.printf("WELCOME_STATUS active=%d step=%d band=%d preset=%s name=%s bytes=%d done=%d archive=%s\n",screen==Screen::Welcome,welcomeStep,welcomeBand,welcomeFocus>=0&&welcomeFocus<int(welcomeList.size())?matter::RADIO_PRESETS[welcomeList[welcomeFocus]].name:"",welcomeName.c_str(),welcomeBytes,device::prefGetInt("mmsetup","done",0),welcomeArchive.c_str());return;}
 if(s=="unlock code"){Serial.printf("UNLOCK_CODE free=%d unlocked=%d code=%s\n",mm::FreeBuild,device::otaAppsUnlocked(),device::otaUnlockCode().c_str());return;}
 if(s=="ota status"){auto o=device::otaState();Serial.printf("OTA_STATUS board=%s code=%ld phase=%d message=\"%s\" version=%s offered=%ld done=%lu size=%lu servers=%d rolled_back=%d installed=%d refused=%ld launcher=%d to_sd=%d sd_file=%s\n",device::otaBoard(),long(MM_VERSION_CODE),int(o.phase),o.message.c_str(),o.version.c_str(),long(o.code),(unsigned long)o.done,(unsigned long)o.size,device::otaServerCount(),o.rolledBack,o.installed,long(o.refused),device::otaLauncherHosted(),o.toSd,o.sdFile.c_str());return;}
 if(s=="ota check"){Serial.println(device::otaCheck()?"OTA_CHECK started":"OTA_CHECK not started (busy, no Wi-Fi, no server or no worker)");return;}
 if(s=="ota install"){Serial.printf("OTA_INSTALL started=%d\n",device::otaInstall());return;}
 if(s.startsWith("ota servers ")){device::otaSetServers(s.substring(12));Serial.printf("OTA_SERVERS count=%d\n",device::otaServerCount());return;}
 if(s=="map status"){auto t=device::mapTilesState();Serial.printf("MAP_STATUS zoom=%d lat=%.5f lon=%.5f follow=%d visible=%d missing=%d sd=%d online=%d queued=%d downloaded=%lu stored=%lu failed=%lu sd_png=%lu purged=%lu read_ms=%lu decode_ms=%lu bytes=%lu worker=%d decoder=%d heap_internal=%u\n",mapView.zoom,mapView.lat,mapView.lon,mapView.follow,mapVisibleCount,mapMissingCount,t.sd,t.online,t.queued,t.downloaded,t.stored,t.failed,t.sdLoaded,t.purged,t.lastReadMs,t.lastDecodeMs,t.lastBytes,t.worker,t.decoder,unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));return;}
 if(s.startsWith("map zoom ")){mapView.zoom=constrain(int(s.substring(9).toInt()),device::MapMinZoom,device::MapMaxZoom);device::mapTilesForget();render();return;}
 if(s.startsWith("map route ")){if(screen==Screen::Chat){String rest=s.substring(10);int bar=rest.indexOf('|');messageActions=messageBodyView=false;messagePath=messageRouteView=true;routeOpen((bar<0?rest:rest.substring(0,bar)).c_str(),bar<0?"":rest.substring(bar+1).c_str());render();}Serial.printf("MAP_ROUTE open=%d\n",screen==Screen::Chat&&messageRouteView);return;}
 if(s=="route status"){Serial.printf("ROUTE_STATUS open=%d loading=%d map=%d zoom=%d stops=%u segments=%u pins=%u self=%d summary=%s\n",messagePath&&messageRouteView,routeLoading(),routeView.map,routeView.zoom,unsigned(routeView.stops.size()),unsigned(routeView.segments.size()),unsigned(routeView.pins.size()),routeView.self,routeView.summary.c_str());return;}
 if(s.startsWith("map goto ")){double lat=0,lon=0;if(sscanf(s.c_str()+9,"%lf %lf",&lat,&lon)==2&&fabs(lat)<=85&&fabs(lon)<=180){mapView.lat=lat;mapView.lon=lon;mapView.follow=false;mapView.ready=true;device::mapTilesForget();render();}return;}
 if(s=="gps backup"){Serial.printf("GPS_BACKUP started=%d\n",device::locationBackupNow());return;}
 if(s=="gps backup status"){Serial.print("GPS_BACKUP ");Serial.println(device::locationBackupDiagnostic().c_str());return;}
 if(s=="gps probe"){Serial.printf("GPS_PROBE started=%d readonly=1\n",device::locationProbe());return;}
 if(s=="gps ubx result"){auto result=device::locationUbxResult();Serial.print("GPS_UBX ");Serial.println(result.c_str());return;}
 if(s.startsWith("gps ubx ")){std::string hex=s.substring(8).c_str();uint8_t bytes[66];size_t n=0;bool ok=hex.size()>=4&&hex.size()%2==0&&hex.size()/2<=sizeof(bytes);if(ok){n=hex.size()/2;ok=mm::parseHex(hex.data(),hex.size(),bytes,n);}
  Serial.printf("GPS_UBX_POLL started=%d readonly=1\n",ok&&device::locationUbxPoll(bytes[0],bytes[1],bytes+2,uint16_t(n-2)));return;}
 if(s=="gps health"){auto diagnostic=device::locationReceiverDiagnostic();StaticJsonDocument<2048> doc;doc["summary"]=diagnostic.c_str();Serial.print("GPS_HEALTH ");serializeJson(doc,Serial);Serial.println();return;}
 if(s=="gps nmea"){auto diagnostic=device::locationNmeaDiagnostic();StaticJsonDocument<2048> doc;doc["gsv"]=diagnostic.c_str();Serial.print("GPS_NMEA ");serializeJson(doc,Serial);Serial.println();return;}
 if(s=="gps on"||s=="gps off"){bool ok=device::locationEnabled(s=="gps on");Serial.printf("GPS_SWITCH ok=%d enabled=%d\n",ok,device::locationState().enabled);return;}
 if(s=="gps status"){auto gps=device::locationState();Serial.printf("GPS_STATUS enabled=%d available=%d user_enabled=%d temporary=%d baud=%lu data=%d fix=%d cached=%d bytes=%lu nmea=%lu bad=%lu fresh=%d age=%lu used=%lu visible=%lu satellites=%lu time_valid=%d\n",gps.enabled,gps.available,gps.userEnabled,gps.timeLease,gps.baud,gps.data,gps.fix,gps.configured,gps.serialBytes,gps.nmeaValid,gps.nmeaBad,gps.nmeaFresh,gps.nmeaAgeMs,gps.satellitesUsed,gps.satellitesVisible,gps.satellites,gps.timeValid);return;}
 if(s=="rpt status"){Serial.printf("RPT_STATUS observed=%d visible=%d chat=%d hash=%s age=%lu timeout=%lu\n",firstRepeater.observed,firstRepeater.visible(activeChat,millis()),firstRepeater.chat,firstRepeater.hash.c_str(),firstRepeater.observed?millis()-firstRepeater.shownAt:0,device::FIRST_REPEATER_DISPLAY_MS);return;}
 if(s=="draft fingerprint"){uint32_t hash=2166136261u;for(unsigned i=0;i<input.length();i++){hash^=uint8_t(input[i]);hash*=16777619u;}Serial.printf("DRAFT_STATE bytes=%u hash=%08lx\n",input.length(),hash);return;}
 if(s=="resend preview"){String body;bool ok=resendBody(body);uint32_t hash=2166136261u;for(unsigned i=0;i<body.length();i++){hash^=uint8_t(body[i]);hash*=16777619u;}Serial.printf("RESEND_PREVIEW ok=%d chat=%d token=%08lx bytes=%u hash=%08lx draft_bytes=%u\n",ok,activeChat,contextMessage.token,body.length(),hash,input.length());return;}
 if(s=="scope list"){StaticJsonDocument<3072> json;auto list=json.to<JsonArray>();for(auto &name:regionScopes)list.add(name);Serial.print("SCOPE_LIST ");serializeJson(json,Serial);Serial.println();return;}
 if(s.startsWith("scope import ")){StaticJsonDocument<3072> json;bool ok=!deserializeJson(json,s.substring(13))&&json.is<JsonArray>();std::vector<std::string> next;if(ok)for(JsonVariant value:json.as<JsonArray>()){if(!value.is<const char*>()){ok=false;break;}auto name=device::normalizedScope(value.as<const char*>());if(name.empty()||std::find(next.begin(),next.end(),name)!=next.end()||next.size()>=device::MAX_REGION_SCOPES){ok=false;break;}next.push_back(name);}if(next.empty())ok=false;if(ok){auto old=regionScopes;regionScopes=next;if(!saveRegionScopes()){regionScopes=old;ok=false;}else for(auto &name:regionScopes)matter::registerScope(name.c_str());}Serial.printf("SCOPE_IMPORTED ok=%d count=%u\n",ok,unsigned(regionScopes.size()));return;}
 if(s=="chat storage selftest"){
  String path="/mattermesh/.chat-clear-test-"+String(esp_random(),HEX)+".tsv";bool unique=!SD.exists(path),ok=sdReady&&unique&&chatStorageSelftest(path),cleaned=sdReady&&unique&&SD.remove(path);
  Serial.printf("CHAT_STORAGE_TEST ok=%d isolated=%d cleanup=%d\n",ok&&cleaned,unique,cleaned);return;
 }
 if(s=="contacts status"){auto page=matter::contacts(discoverFilter,contactQuery.c_str(),nodeScroll,mm::ContactRows);Serial.printf("CONTACTS_STATUS section=%d filter=%d searching=%d query_len=%u focus=%d offset=%d total=%d live=%d rows=%u\n",section,discoverFilter,contactSearching,unsigned(contactQuery.length()),contactFocus,nodeScroll,page.total,page.live,unsigned(page.rows.size()));return;}
 if(s=="contacts rows"){auto page=matter::contacts(discoverFilter,contactQuery.c_str(),nodeScroll,mm::ContactRows);for(size_t i=0;i<page.rows.size();i++){auto &n=page.rows[i];Serial.printf("CONTACT_ROW index=%u type=%u live=%d hops=%d rssi=%.0f snr=%.1f key=%s name=%s\n",unsigned(nodeScroll+i),n.type,n.live,n.hops,n.rssi,n.snr,n.key.c_str(),n.name.c_str());}Serial.printf("CONTACT_ROWS total=%d live=%d\n",page.total,page.live);return;}
 if(s.startsWith("cli ")){size_t before=cliOutput.size();cliRun(s.substring(4).c_str());render();Serial.printf("CLI lines=%u\n",unsigned(cliOutput.size()));for(size_t i=before<cliOutput.size()?before:0;i<cliOutput.size();i++)Serial.printf("CLI> %s\n",cliOutput[i].c_str());return;}
 if(s=="channel menu status"){auto gps=device::locationState();bool valid=!device::gpsCoordinateMessage(gps.available,gps.enabled,gps.fix,gps.fixAgeMs,gps.latitude,gps.longitude).empty();Serial.printf("CHANNEL_MENU_STATUS key_view=%d qr_size=%d key_chars=%u count=%d open=%d target=%d muted=%d delete_confirm=%d position=%d position_ready=%d\n",channelKeyView,channelQrSize,channelKeyHex.length(),chatCount,channelOptions,channelOptionTarget,channelOptionTarget>=0&&channelOptionTarget<chatCount&&bool(mutedChannels&(uint32_t(1)<<channelOptionTarget)),deleteChannelConfirm,positionShare,valid);return;}
 if(s=="channel list"){DynamicJsonDocument json(8192);// 32 conversations
 auto list=json.to<JsonArray>();for(int i=0;i<chatCount;i++){auto entry=list.createNestedObject();entry["id"]=i;entry["name"]=chats[i].name;entry["direct"]=chats[i].direct;entry["muted"]=bool(mutedChannels&(uint32_t(1)<<i));entry["ready"]=channelReady[i];entry["peer_bound"]=!peerKeys[i].isEmpty();}Serial.print("CHANNEL_LIST ");serializeJson(json,Serial);Serial.println();return;}
 if(s=="scope status"){Serial.printf("SCOPE_STATUS active=%d count=%u scope=%s picker=%d editing=%d focus=%d\n",activeChat,unsigned(regionScopes.size()),chats[activeChat].scope.c_str(),scopePicker,scopeEditing,scopeFocus);return;}
 if(s=="lora status"){auto cfg=matter::snapshot().config;Serial.printf("LORA_STATUS frequency=%.3f bandwidth=%.2f sf=%u cr=%u power=%d draft_frequency=%.3f draft_bandwidth=%.2f draft_sf=%u draft_cr=%u draft_power=%d preset=%d band=%d field=%d editing=%d focus=%d path_bytes=%u draft_path_bytes=%u device_page=%d device_focus=%d name_bytes=%u draft_name_bytes=%u\n",cfg.frequency,cfg.bandwidth,cfg.sf,cfg.cr,cfg.power,radioDraft.frequency,radioDraft.bandwidth,radioDraft.sf,radioDraft.cr,radioDraft.power,radioPresetSelection,radioBandSelection,radioEditField,radioPowerEditing,radioFocus,cfg.pathBytes,radioDraft.pathBytes,radioDevicePage,radioDeviceFocus,cfg.name.length(),radioDraft.name.length());return;}
 if(s=="keyboard status"){Serial.printf("KEYBOARD_STATUS sym=%d shift=%d symbols=%d caps=%d bytes=%u alt=%d\n",keyboardModifiers.sym,keyboardModifiers.shift(),keyboardModifiers.symbols(),keyboardModifiers.shift(),input.length(),keyboardModifiers.alt);return;}
 // Bounded keyboard FIFO regressions: no RF sends, no settings writes.
 if(s=="ui replay lock-burst"){if(!deviceLocked){uint32_t now=millis();for(int edge:{161,33,161,33})captureKeyEvent(edge,now);}Serial.println("LOCK_REPLAY_DONE");return;}
 if(s=="ui replay unlock-burst"){if(deviceLocked&&!lockKey.down&&!unlockTaps.active()){uint32_t now=millis();for(int edge:{161,33,161,33,33})captureKeyEvent(edge,now);}Serial.println("UNLOCK_REPLAY_DONE");return;}
 if(s=="ui replay unlock-busy"){if(deviceLocked&&!lockKey.down&&!unlockTaps.active()){uiReplay.kind=3;uiReplay.step=0;uiReplay.started=0;handleLockKey(true,millis(),true);render(true);}Serial.println("UNLOCK_REPLAY_DONE");return;}
 if(s=="ui replay touch"||s=="ui replay mic-release"){if(!deviceLocked&&screen==Screen::Chat&&!emojiPicker&&input.length()+6<=messageByteLimit()){uiReplay.kind=s.endsWith("touch")?1:2;uiReplay.started=0;uiReplay.released=false;input+=" ";render();}Serial.println("UI_REPLAY_DONE");return;}
 if(s=="touch sample down"||s=="touch sample up"){bool down=s.endsWith("down");injectedTouchUntil=down?millis()+1500:0;injectedTouchX=120;injectedTouchY=280;feedTouch(down,120,280,millis());Serial.println("TOUCH_SAMPLE");return;}
 if(s.startsWith("touch drag ")){int x,y0,y1,ms;if(sscanf(s.c_str()+11,"%d %d %d %d",&x,&y0,&y1,&ms)==4&&ms>0){injectedDrag.x=x;injectedDrag.y0=y0;injectedDrag.y1=y1;injectedDrag.start=millis();injectedDrag.ms=uint32_t(ms);injectedDrag.ended=false;injectedDrag.on=true;}Serial.printf("TOUCH_DRAG fling=%d\n",int(fling.active));return;}
 if(s=="fling status"){Serial.printf("FLING active=%d v=%d caught=%d browse=%d steps=%lu acc=%d settings_scroll=%d max=%d starts=%lu stop_end=%lu stop_slow=%lu moves=%lu move_px=%ld last_vy=%d samples=%d age=%lu\n",int(fling.active),int(fling.v),int(flingCaught),int(touchBrowse),(unsigned long)touchScrollSteps,touchScrollAcc,settingsScroll,mm::settingsMaxScroll(),(unsigned long)flingStarts,(unsigned long)flingStopEnd,(unsigned long)flingStopSlow,(unsigned long)moveEvents,movePx,touchPipeline.lastVy,touchPipeline.lastSamples,(unsigned long)touchPipeline.lastAge);return;}
 if(s.startsWith("touch sample ")){int down,x,y;if(sscanf(s.c_str()+13,"%d %d %d",&down,&x,&y)==3){injectedTouchUntil=down?millis()+1500:0;injectedTouchX=x;injectedTouchY=y;feedTouch(down!=0,x,y,millis());}Serial.println("TOUCH_SAMPLE");return;}
 if(s=="touch stats"){Serial.printf("TOUCH_STATS queued=%d dispatched=%lu stale=%lu dropped=%lu cancelled=%lu latency=%lu max=%lu active=%d release_ms=%lu busy_capture=1\n",touchPipeline.size(),touchDispatched,touchStale,touchPipeline.dropped,touchPipeline.cancelled,lastTouchDispatchMs,maxTouchDispatchMs,touchPipeline.contactActive(),device::TOUCH_RELEASE_MS);return;}
 if(s.startsWith("ui status preview ")){int percent=-2;bool valid=sscanf(s.c_str()+18,"%d",&percent)==1&&percent>=-1&&percent<=100;bool drawn=valid&&!batchingInput&&!device::notificationFlashing();if(drawn)render(true,percent);Serial.printf("STATUS_PREVIEW percent=%d drawn=%d temporary=1\n",percent,drawn);return;}
 if(s=="bench gfx"){// colour canvas primitives, microseconds each
  auto t=[](std::function<void()> fn){uint32_t a=micros();fn();return micros()-a;};mm::Components c(frame);mm::Renderer r(frame);auto v=uiState();
  uint32_t fill=t([]{frame.fillScreen(mm::BG);}),rects=t([]{for(int i=0;i<10;i++)frame.fillRoundRect(8,40+i*18,300,16,8,mm::Surface);}),
   text=t([&]{for(int i=0;i<10;i++)c.text(10,40+i*18,"Meet at the ridge? 10:18 Cervinia",mm::Font::ChatBody);}),
   icons=t([&]{for(int i=0;i<10;i++)c.packIcon(mm::PackIcon::Chat,10+i*28,100,24,mm::FG);}),
   status=t([&]{c.statusBar(v.status);}),home=t([&]{r.home(v);});
  Serial.printf("BENCH_GFX fill=%lu rects10=%lu text10=%lu icons10=%lu status=%lu home=%lu\n",(unsigned long)fill,(unsigned long)rects,(unsigned long)text,(unsigned long)icons,(unsigned long)status,(unsigned long)home);
  // Same primitives on a 320x40 strip in internal RAM and on an 8-bit 320x240 canvas in PSRAM.
  {mm::FastCanvas16 strip(mm::W,40);mm::Components cs(strip);GFXcanvas8 c8(mm::W,mm::H);mm::Components c8c(c8);
   uint32_t sf=t([&]{for(int i=0;i<6;i++)strip.fillScreen(mm::BG);}),st=t([&]{for(int i=0;i<10;i++)cs.text(10,4+(i%2)*18,"Meet at the ridge? 10:18 Cervinia",mm::Font::ChatBody);}),
    sr=t([&]{for(int i=0;i<10;i++)strip.fillRoundRect(8,(i%2)*20,300,16,8,mm::Surface);}),
    f8=t([&]{c8.fillScreen(3);}),t8=t([&]{for(int i=0;i<10;i++)c8c.text(10,40+i*18,"Meet at the ridge? 10:18 Cervinia",mm::Font::ChatBody,1);}),r8=t([&]{for(int i=0;i<10;i++)c8.fillRoundRect(8,40+i*18,300,16,8,2);});
   Serial.printf("BENCH_MEM strip_internal=%d fill6=%lu text10=%lu rects10=%lu | c8_psram=%d fill=%lu text10=%lu rects10=%lu heap=%u\n",esp_ptr_internal(strip.getBuffer()),(unsigned long)sf,(unsigned long)st,(unsigned long)sr,!esp_ptr_internal(c8.getBuffer()),(unsigned long)f8,(unsigned long)t8,(unsigned long)r8,unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)));}
  dirty=true;return;}
 if(s=="ui stats"){Serial.printf("UI_STATS render=%lu raster=%lu paint=%lu busy=%lu queued=%d drop=%lu stack=%u state_us=%lu state_calls=%lu\n",renderCalls,lastRasterMs,lastPaintMs,busyServiceCalls,keyboardQueue.size(),keyboardQueue.dropped,unsigned(uxTaskGetStackHighWaterMark(nullptr)),(unsigned long)uiStateUs,(unsigned long)uiStateCalls);uiStateUs=uiStateCalls=0;
  Serial.printf("UI_PRESENT us=%lu bytes=%lu pushes=%lu cpu=%lu scrolls=%lu shift=%d asleep=%d sleeps=%lu\n",(unsigned long)lcd.lastPresentUs,(unsigned long)lcd.presentBytes,(unsigned long)lcd.pushes,(unsigned long)getCpuFrequencyMhz(),(unsigned long)lcd.scrollAnimations,lcd.lastScrollShift,int(lcd.asleep),(unsigned long)lcd.sleeps);
  return;}
 if(s=="weather status"){auto w=device::weatherState();auto c=device::connectivityStatus();Serial.printf("WEATHER_STATUS valid=%d busy=%d cached=%d open=%d phase=%d source=%d hours=%d next=%lu wifi_on=%d connected=%d place=%s message=%s\n",w.data.valid,w.busy,w.cached,w.open,int(w.phase),w.data.source,w.data.hours,(unsigned long)w.nextRefresh,c.wifi,c.connected,w.place.c_str(),w.message.c_str());return;}
 if(s=="emoji status"){loadEmojiCounts();auto top=mm::frequentEmoji(emojiCounts);unsigned used=0;for(auto n:emojiCounts)used+=n>0;Serial.printf("EMOJI_STATUS used=%u first=%lu first_count=%u\n",used,(unsigned long)mm_emoji_picker::glyphs[top[0]].code,emojiCounts[top[0]]);return;}
 if(s=="emoji reset"){loadEmojiCounts();for(auto &n:emojiCounts)n=0;saveEmojiCounts();Serial.println("EMOJI_RESET ok=1");return;}
 if(s=="ui state"){Serial.printf("UI_STATE screen=%d section=%d emoji=%d confirm_off=%d bytes=%u channel=%d own_menu=%d path=%d launcher_focus=%d settings_focus=%d home_focus=%d scope_picker=%d scope_edit=%d scope_focus=%d clear_confirm=%d options=%d settings_scroll=%d\n",int(screen),section,emojiPicker,shutdownConfirm,input.length(),activeChat,messageActions&&contextMessage.mine,messagePath,launcherSelection,settingsFocus,homeSelection,scopePicker,scopeEditing,scopeFocus,clearChatConfirm,chatDetails,settingsScroll);return;}
 if(s=="input state"){auto field=editableField();Serial.printf("INPUT_STATE editing=%d bytes=%u screen=%d section=%d repeat=%d depth=%u\n",field!=nullptr,field?field->length():0,int(screen),section,deleteHeld,unsigned(navigation.size()));return;}
 if(s=="battery calibrate"){bool ok=device::batteryCalibrateFull();Serial.printf("BATTERY_CALIBRATED ok=%d full_mv=%d\n",ok,device::batteryFullMillivolts());render();return;}
 if(s.startsWith("battery gain ")){int mv=s.substring(13).toInt();bool ok=device::batterySetTrueMillivolts(mv);Serial.printf("BATTERY_GAIN ok=%d gain_x1000=%d\n",ok,device::batteryGainX1000());return;}// multimeter mV at the cell
 if(s=="battery raw"){Serial.printf("BATTERY_RAW mv=%d source=%d\n",plus::batteryMillivolts(),int(device::batteryPowerSource()));return;}
 if(s=="battery log on"||s=="battery log off"){batteryLogOn=s=="battery log on";device::prefPutInt("mmbatlog","on",batteryLogOn);Serial.printf("BATTERY_LOG on=%d\n",batteryLogOn);return;}
 if(s=="battery log clear"){bool ok=!SD.exists("/mattermesh/battery-log.csv")||SD.remove("/mattermesh/battery-log.csv");batteryLogRows=0;batteryLogAt=0;Serial.printf("BATTERY_LOG cleared=%d\n",ok);return;}
 if(s=="battery log status"){File f=SD.open("/mattermesh/battery-log.csv");Serial.printf("BATTERY_LOG on=%d rows=%lu bytes=%lu\n",batteryLogOn,(unsigned long)batteryLogRows,f?(unsigned long)f.size():0UL);if(f)f.close();return;}
 if(s=="battery status"){auto b=device::batteryState();Serial.printf("BATTERY_STATUS mv=%d percent=%d charging=%d usb=%d full=%d full_mv=%d\n",b.voltage,b.percent,b.charging,device::batteryPowerSource()==device::PowerSource::USB,b.full,device::batteryFullMillivolts());return;}
 if(s=="lights status"){auto p=device::powerState();Serial.printf("LIGHTS_STATUS d_level=%u k_level=%u d_on=%d k_on=%d d_pwm=%u k_pwm=%u\n",displayBrightness,keyboardBrightness,p.displayEnabled,p.keyboardEnabled,p.displayPWM,p.keyboardPWM);return;}
 if(s=="wake status"){Serial.printf("WAKE_STATUS preview=%d wake_ms=%ld display_pwm=%u locked=%d\n",device::unlockPreviewActive(),(long)device::wakeRemaining(),device::powerState().displayPWM,deviceLocked);return;}
 if(s=="feedback status"){auto p=device::powerState();Serial.printf("FEEDBACK_STATUS display_enabled=%d keyboard_enabled=%d display_pwm=%u keyboard_pwm=%u flash_active=%d flash_count=%lu\n",p.displayEnabled,p.keyboardEnabled,p.displayPWM,p.keyboardPWM,device::notificationFlashing(),p.notificationFlashes);return;}
 if(s=="time status"){auto t=device::timeSyncState();Serial.printf("TIME_STATUS acquiring=%d gps_waiting=%d temporary=%d timeout=%d source=%s wifi_connected=%d epoch=%lu\n",t.acquiring,t.gpsWaiting,t.gpsTemporary,t.timedOut,t.source==device::TimeSource::WiFi?"wifi":t.source==device::TimeSource::GPS?"gps":"none",device::wifiConnected(),uint32_t(time(nullptr)));return;}
 if(s=="speaker status"){auto a=device::audioState();Serial.printf("SPEAKER_STATUS ready=%d volume=%d busy=%d played=%lu bytes=%lu error=%d\n",a.ready,a.volume,a.busy,a.playbacks,a.txBytes,a.lastError);return;}
 if(s=="audio status"){auto a=device::audioState();Serial.printf("AUDIO_STATUS ready=%d notify=%d volume=%d busy=%d\n",a.ready,a.notifications,a.volume,a.busy);return;}
 if(s=="audio test"&&!deviceLocked){device::audioNotify(true);render();return;}
 if(s=="audio cancel"){device::audioCancel();render();return;}
 if(s=="lock status"){auto p=device::powerState();Serial.printf("LOCK_STATUS locked=%d previous=%d cpu=%u touch_disabled=%d display=%u keyboard=%u progress=%d unread=%u rx=%lu target=%d taps=%d boost=%d requested=%lu last_read=%d down=%d hold_down=%d complete=%d recoveries=%lu hw_overflow=%lu irq=%d uptime=%lu\n",deviceLocked,int(previousScreen),getCpuFrequencyMhz(),deviceLocked&&touchDisabled,p.displayPWM,p.keyboardPWM,lockKey.progress,uiState().unread,matter::snapshot().received,lastReceivedChat,unlockTaps.taps,unlockTaps.active(),requestedLockedCpu(),lastReceivedRead,unlockTaps.down,lockKey.down,unlockTaps.completed,lockRecoveries,keyboardHardwareOverflows,digitalRead(BOARD_KEYBOARD_INT),millis());return;}
 if(s=="lock test-rx"||s.startsWith("lock test-rx ")){matter::Received m;for(int i=0;i<chatCount;i++)if(channelReady[i]){m.channelId=i;break;}m.sender="LOCK TEST (synthetic)";m.body=s.length()>13?s.substring(13).c_str():"MATTERMESH LOCK callback test";/* optional body: lock test-rx TEXT */m.timestamp=time(nullptr);m.hops=2;m.scope="it-lig";m.path="flood:ab,cd";incomingRadio(m);render();return;}
 if(deviceLocked&&!(quickReply.active&&(s.startsWith("tap ")||s.startsWith("type ")||s.startsWith("key ")))&&s!="status"&&s!="shot"&&s!="render"&&s!="notify incoming"&&s!="radio status"&&s!="device status"&&s!="wake status"&&!s.startsWith("event ")&&!s.startsWith("battery ")&&s!="read /mattermesh/battery-log.csv"){Serial.println("UI_READY locked=1 ignored=1");return;}
 if(s=="status")Serial.printf("STATUS demo=1 reset=%d radio=meshcore sd=%d keyboard=%d screen=%d chats=%d selected=%d active=%d field=%d touch=%d touch_events=%lu key_events=%lu caps=%d symbols=%d kbd_light=%u input_len=%u section=%d filter=%d screen_light=%u searching=%d query_len=%u chat_offset=%d file_count=%d message_actions=%d message_path=%d channel_muted=%d stack_free=%u radio_power_edit=%d node_scroll=%d\n",(int)esp_reset_reason(),sdReady,keyboardReady,int(screen),chatCount,selected,activeChat,formField,touchBackend,touchEvents,keyboardEvents,keyboardModifiers.shift(),keyboardModifiers.symbols(),keyboardBrightness,input.length(),section,discoverFilter,displayBrightness,searching,unsigned(searchQuery.length()),chatOffset,filesCount,messageActions,messagePath,bool(mutedChannels&(uint32_t(1)<<activeChat)),unsigned(uxTaskGetStackHighWaterMark(nullptr)),radioPowerEditing,nodeScroll);
 else if(s=="chat bubbles"){for(auto &hit:paintedChatMessages)Serial.printf("CHAT_BUBBLE index=%d x=%d y=%d w=%d h=%d mine=%d token=%08lx\n",hit.index,hit.area.x,hit.area.y,hit.area.w,hit.area.h,hit.message.mine,hit.message.token);Serial.println("CHAT_BUBBLES_DONE");}

 else if(s=="shot") {Serial.printf("PBM_BEGIN %u\n",unsigned(mm::W*mm::H*3));Serial.printf("P6\n%d %d\n255\n",mm::W,mm::H);uint8_t rgb[mm::W*3];const uint16_t *px=frame.getBuffer();for(int y=0;y<mm::H;y++){for(int x=0;x<mm::W;x++){uint16_t c=mm::panelToRgb565(px[y*mm::W+x]);rgb[3*x]=(c>>8&0xF8)|(c>>13);rgb[3*x+1]=(c>>3&0xFC)|(c>>9&3);rgb[3*x+2]=(c<<3&0xF8)|(c>>2&7);}Serial.write(rgb,sizeof(rgb));}Serial.println("\nPBM_END");}
 else if(s=="device status"){auto c=device::connectivityStatus();auto p=device::powerState();auto b=device::batteryState();auto w=device::weatherState();auto loc=device::locationState();Serial.printf("DEVICE_STATUS wifi=%d connected=%d scanning=%d networks=%u network_saved=%d ble=%d ble_ready=%d display_timeout=%lu keyboard_timeout=%lu display_pwm=%u keyboard_pwm=%u inactivity=%lu battery=%d voltage=%d gauge=%d full=%d weather=%d weather_busy=%d cached=%d location=%d gps=%d heap=%u psram=%u\n",c.wifi,c.connected,c.scanning,unsigned(c.networkCount),c.remembered,c.ble,c.bleReady,p.displaySeconds,p.keyboardSeconds,p.displayPWM,p.keyboardPWM,millis()-p.lastActivity,b.percent,b.voltage,b.gauge,b.full,w.data.valid,w.busy,w.cached,loc.configured,loc.fix,ESP.getFreeHeap(),ESP.getFreePsram());}
 else if(s=="time sync"){device::timeSyncRequest();render();}
 else if(s=="wifi hidden"){hiddenWifi();}
 else if(s=="wifi disconnect"){device::disconnect();render();}
 else if(s=="wifi reconnect"){device::reconnect();render();}
 else if(s=="wifi scan"){device::scan();render();}
 else if(s=="wifi on"||s=="wifi off"){device::wifiEnabled(s=="wifi on");render();}
 else if(s.startsWith("wifi join ")){StaticJsonDocument<256> json;if(!deserializeJson(json,s.substring(10))&&json["ssid"].is<const char*>()&&json["password"].is<const char*>())notice=device::connect(json["ssid"].as<String>(),json["password"].as<String>())?"Connecting...":"Connection settings rejected";else notice="Invalid connection settings";render();}
 else if(s=="ble on"||s=="ble off"){device::bleEnabled(s=="ble on");render();}
 else if(s.startsWith("power display ")||s.startsWith("power keyboard ")){int split=s.lastIndexOf(' ');uint32_t seconds=s.substring(split+1).toInt();notice=device::timeout(s.startsWith("power keyboard "),seconds)?"Timeout saved":"Invalid timeout";render();}
 else if(s=="power wake"){device::activity();Serial.println("POWER_AWAKE");}
 else if(s.startsWith("weather location ")){double lat,lon;if(sscanf(s.c_str()+17,"%lf %lf",&lat,&lon)==2&&device::locationSet(lat,lon)){device::weatherRefresh();notice="Location saved";}else notice="Invalid coordinates";render();}
 else if(s=="weather refresh"){device::weatherRefresh();render();}
 else if(s=="radio selftest"){static bool done=false;if(!done){done=true;Serial.println("PROTOCOL_TEST "+matter::protocolSelfTest());}else Serial.println("PROTOCOL_TEST already run / reboot to repeat");}
 else if(s=="discovery status"){Serial.printf("DISCOVERY_STATUS capacity=%d nodes=%d companions=%d repeaters=%d heap=%u psram=%u\n",matter::NODE_CAPACITY,matter::nodeCount(0),matter::nodeCount(1),matter::nodeCount(2),ESP.getFreeHeap(),ESP.getFreePsram());}
 else if(s=="radio diag"){auto d=matter::radioDiag();Serial.printf("RADIO_DIAG tcxo=%.1f boosted=%d device_errors=0x%04x noise_floor=%d received=%lu recv_errors=%lu rssi=%.1f snr=%.2f\n",d.tcxo,d.boosted,d.deviceErrors,d.noiseFloor,(unsigned long)d.received,(unsigned long)d.recvErrors,d.rssi,d.snr);}
 else if(s=="radio clear errors"){Serial.printf("RADIO_CLEAR ok=%d\n",matter::radioClearErrors());}
 else if(s=="radio status"){auto r=matter::snapshot();Serial.printf("RADIO_STATUS ready=%d rx=%d tx=%d error=%d nodes=%d received=%lu sent=%lu queued=%d frequency=%.3f bw=%.2f sf=%u cr=%u power=%d identity=%d journal=%u capacity=%d\n",r.ready,r.rx,r.tx,r.error,r.nodes,r.received,r.sent,r.queued,r.config.frequency,r.config.bandwidth,r.config.sf,r.config.cr,r.config.power,r.identity,unsigned(radioJournal.records.size()),matter::NODE_CAPACITY);}
 else if(s=="radio identity"){auto r=matter::snapshot();Serial.printf("RADIO_IDENTITY name=%s path_bytes=%u ready=%d rx=%d\n",r.config.name.c_str(),r.config.pathBytes,r.ready,r.rx);}
 else if(s.startsWith("radio config ")){auto cfg=matter::snapshot().config;int sf,cr,power;float f,bw;if(sscanf(s.c_str()+13,"%f %f %d %d %d",&f,&bw,&sf,&cr,&power)==5&&sf>=5&&sf<=12&&cr>=5&&cr<=8&&power>=matter::MIN_TX_POWER&&power<=matter::MAX_TX_POWER){cfg.frequency=f;cfg.bandwidth=bw;cfg.sf=sf;cfg.cr=cr;cfg.power=power;if(matter::configure(cfg,notice))loadRadioDraft(false);}else notice="Use frequency BW SF CR power";render();}
 else if(s.startsWith("radio name ")){auto cfg=matter::snapshot().config;cfg.name=clean(s.substring(11));matter::configure(cfg,notice);render();}
 else if(s.startsWith("radio time ")){matter::syncClock(strtoul(s.c_str()+11,nullptr,10),notice);render();}
 else if(s=="radio advert"){matter::advertise(notice);render();}
 else if(s.startsWith("channels replace ")){bool ok=replaceChannels(s.substring(17));Serial.printf("CHANNELS_REPLACED ok=%d count=%d\n",ok,chatCount);notice=ok?"Channels saved":"Channel import failed / retry";render();}
 else if(s.startsWith("channel key ")){int split=s.indexOf(' ',12);int id=s.substring(12,split).toInt();if(split>12&&id>=0&&id<chatCount&&!chats[id].direct){String old=channelKeys[id];channelKeys[id]=s.substring(split+1);String reason;if(matter::channel(id,chats[id].name,channelKeys[id],chats[id].scope,reason)&&saveBindings()){channelReady[id]=true;notice="Channel key saved";}else{channelKeys[id]=old;configureChannels();notice="Channel save failed";}}else notice="Invalid channel";render();}
 else if(s=="render")render();
 else if(s=="menu"){searching=false;searchQuery="";messagesMenu=false;screen=Screen::Chats;render();}
 else if(s=="notify incoming"){onIncomingMessage();Serial.println("RX_NOTIFICATION_TEST radio=off");}
 else if(s=="notify mention"){onIncomingMessage(true);Serial.println("RX_NOTIFICATION_TEST mention=1 radio=off");}
 else if(s=="ringtone status"){auto a=device::audioState();Serial.printf("RINGTONE_STATUS message=%d:%s mention=%d:%s message_ms=%d mention_ms=%d count=%d busy=%d bytes=%lu\n",a.messageTone,device::RINGTONES[a.messageTone].name,a.mentionTone,device::RINGTONES[a.mentionTone].name,device::ringtoneMs(a.messageTone),device::ringtoneMs(a.mentionTone),device::RINGTONE_COUNT,a.busy,(unsigned long)a.txBytes);}
 else if(s=="wdt status")device::wdtTraceReport();
 else if(s=="splash")splash(0,"Initializing mesh services...");
 else if(s=="apps"){screen=Screen::Launcher;render();}
 else if(s=="home"){navigation.clear();screen=Screen::Home;render();}
 else if(s.startsWith("section "))openSection(s.substring(8).toInt());
 else if(s=="contact picker")openContactPicker();
 else if(s=="contact picker status"){Serial.printf("PICKER_STATUS open=%d searching=%d total=%u focus=%d query=%s first=%s repeaters=%d\n",screen==Screen::PickContact,pickerSearching,unsigned(pickerContacts.size()),pickerFocus,pickerQuery.c_str(),pickerContacts.empty()?"":String(pickerContacts[0].name).substring(0,12).c_str(),int(std::count_if(pickerContacts.begin(),pickerContacts.end(),[](const matter::Node &n){return n.type!=1;})));}
 else if(s=="dm line status"){auto v=uiState();Serial.printf("DM_LINE_STATUS screen=%d direct=%d messages=%u line=%s\n",int(screen),screen==Screen::Chat&&chats[activeChat].direct,unsigned(v.messages.size()),v.firstRepeater.c_str());}
 else if(s=="channel form status"){Serial.printf("CHANNEL_FORM_STATUS open=%d form=%d field=%d name=%s scope=%s key_chars=%u key_hex=%d key_head=%.4s notice=%s\n",screen==Screen::NewChat,newForm,formField,newName.c_str(),newScope.c_str(),newKey.length(),mm::validSecret(newKey.c_str()),newKey.c_str(),notice.c_str());}
 else if(s.startsWith("channel form ")){beginNewChannel(s.substring(13).toInt());Serial.printf("CHANNEL_FORM form=%d field=%d\n",newForm,formField);}
 else if(s.startsWith("event ")){keyboardEvent(s.substring(6).toInt());}
 else if(s=="key DEL")key('\b');
 else if(s.startsWith("kbdlight ")){keyboardBrightness=constrain(s.substring(9).toInt(),0,255);device::activity();device::brightness(displayBrightness,keyboardBrightness);}
 else if(s.startsWith("swipe ")){swipe(s.substring(6).toInt());}
 else if(s.startsWith("tap ")){int x,y;if(sscanf(s.c_str(),"tap %d %d",&x,&y)==2)tap(x,y);}
 else if(s.startsWith("hold ")){int x,y;if(sscanf(s.c_str(),"hold %d %d",&x,&y)==2&&x>=0&&x<mm::W&&y>=0&&y<mm::H)longPressChannel(x,y);}
 else if(s=="key ENTER")key('\n');
 else if(s=="key BACK")key(3);
 else if(s=="key SPACE")key(' ');
 else if(s.startsWith("key ")&&s.length()==5)key(s[4]);
 else if(s=="draft"&&screen==Screen::Chat){input="";inputCursor=-1;render();}
 else if(s.startsWith("touch key ")){int id=s.substring(10).toInt();touchKeyPress(id);Serial.printf("TOUCH_KEY_DONE id=%d bytes=%u cursor=%u\n",id,input.length(),cursorAt());}
 else if(s=="cursor status"){Serial.printf("CURSOR_STATUS bytes=%u cursor=%u writing=%d keys=%lu last_key=%d\n",input.length(),cursorAt(),composerWriting(),touchKeyEvents,lastTouchKey);}
 else if(s=="composer status"){Serial.printf("COMPOSER_STATUS bytes=%u expanded=%d composer_top=%d cursor=%u\n",input.length(),input.length()>0,paintedComposerTop,cursorAt());}
 else if(s.startsWith("draft ")&&screen==Screen::Chat){String text=s.substring(6);if(text.length()<=messageByteLimit()){input=text;inputCursor=-1;notice="";}else notice=matter::messageLimitError(messageByteLimit()).c_str();render();}
 else if(s.startsWith("type ")){for(unsigned i=5;i<s.length();i++)key(s[i]);render();}
 else if(s.startsWith("ls")){String path=s.length()>3?s.substring(3):"/";File f=SD.open(path);if(f&&f.isDirectory())list(f,path=="/"?"":path);f.close();Serial.println("LIST_END");}
 else if(s.startsWith("read ")&&sdReady){File f=SD.open(s.substring(5));if(f&&!f.isDirectory()){Serial.printf("FILE_BEGIN %u\n",(unsigned)f.size());while(f.available()){uint8_t b[512];size_t n=f.read(b,sizeof(b));Serial.write(b,n);}f.close();Serial.println("\nFILE_END");}else{Serial.println("FILE_ERROR");f.close();}}
 else Serial.println("Commands: status, shot, render, ls [path], read path");
}
void splash(int progress,const String &stage){
 mm::splashScreen(frame,progress,std::string(stage.c_str()));
 lcd.present(frame.getBuffer(),progress==0);
 Serial.printf("BOOT_SPLASH progress=%d\n",progress);
}

#ifndef MM_EMULATOR
#include <mbedtls/platform.h>
#include <esp_heap_caps.h>
// HTTPS (weather, radar, maps, link viewer) allocates its TLS buffers in PSRAM, falling back to internal RAM:
// the framework default keeps them internal (~40 KB per connection), which left little room with Wi-Fi on.
static void *tlsCalloc(size_t n,size_t size){void *p=heap_caps_calloc(n,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);return p?p:heap_caps_calloc(n,size,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);}
static void tlsFree(void *p){heap_caps_free(p);}
#endif
void setup(){
 // Read before anything below writes the NVS: an empty one means a clean flash (see archiveSdData).
 const int setupMark=device::prefGetInt("mmsetup","done",0);const bool cleanInstall=!setupMark&&nvsEmpty();
#ifndef MM_EMULATOR
 mbedtls_platform_set_calloc_free(tlsCalloc,tlsFree);
#endif
 touchPipeline.stream=true;// lists follow the finger (services/touch_pipeline.h Move events)
 pinMode(BOARD_POWERON,OUTPUT);digitalWrite(BOARD_POWERON,HIGH);// LCD, LoRa, GPS and keyboard power
 Serial.setTxBufferSize(16384);Serial.setRxBufferSize(2*SD_TRANSFER_CHUNK+512);Serial.setTxTimeoutMs(1000);Serial.begin(115200);/* RX queue holds two `sd put` chunks plus commands; never resized after begin */delay(800);device::wdtTraceBegin();pinMode(BOARD_BOOT_PIN,INPUT_PULLUP);
 for(int pin:{BOARD_LORA_CS,BOARD_SD_CS,BOARD_EPD_CS}){pinMode(pin,OUTPUT);digitalWrite(pin,HIGH);}
 // Shared SPI: every device retains its own chip select and transaction settings.
 SPI.begin(BOARD_SPI_SCK,BOARD_SPI_MISO,BOARD_SPI_MOSI);
 // The first T-Deck has no pull-up on the shared MISO line: without it the SX1262 may not answer at boot ("Radio
 // unavailable"). Set after SPI.begin, which reconfigures the pin (LilyGO examples and WADAMESH pull it up too).
 gpio_set_pull_mode(gpio_num_t(BOARD_SPI_MISO),GPIO_PULLUP_ONLY);
 lcd.init();analogWrite(BOARD_EPD_BL,100);splash(0,"Initializing mesh services...");
 pinMode(BOARD_I2C_SDA,INPUT_PULLUP);pinMode(BOARD_I2C_SCL,OUTPUT_OPEN_DRAIN);digitalWrite(BOARD_I2C_SCL,HIGH);
 if(digitalRead(BOARD_I2C_SDA)==LOW){
  for(int i=0;i<9;i++){digitalWrite(BOARD_I2C_SCL,LOW);delayMicroseconds(10);digitalWrite(BOARD_I2C_SCL,HIGH);delayMicroseconds(10);}
  pinMode(BOARD_I2C_SDA,OUTPUT_OPEN_DRAIN);digitalWrite(BOARD_I2C_SDA,LOW);delayMicroseconds(10);digitalWrite(BOARD_I2C_SDA,HIGH);
 }
 Wire.begin(BOARD_I2C_SDA,BOARD_I2C_SCL);Wire.setClock(100000);initTouch();
 initKeyboard();device::timeSyncBegin(uint32_t(time(nullptr)));splash(55,"Loading device services...");
 // A cold card can miss its first initialization on the shared SPI bus.
 // Retry before any data-dependent service starts, without formatting the card.
 for(uint32_t frequency:{4000000u,2000000u,1000000u}){
  for(int pin:{BOARD_LORA_CS,BOARD_SD_CS,BOARD_EPD_CS})digitalWrite(pin,HIGH);
  sdReady=SD.begin(BOARD_SD_CS,SPI,frequency,"/sd",5,false);
  Serial.printf("BOOT_SD frequency=%lu mounted=%d\n",frequency,sdReady);
  if(sdReady)break;
  SD.end();delay(150);
 }
 device::mapTilesBegin(SD,sdReady);
 if(cleanInstall)device::prefPutInt("mmsetup","done",2);
 // mmsetup/archived: 0 from a clean install until the old data is moved (retried at every boot, also after the welcome,
 // e.g. when the card was missing); absent on devices installed before (nothing to archive).
 if(cleanInstall)device::prefPutInt("mmsetup","archived",0);
 if(sdReady&&!device::prefGetInt("mmsetup","archived",1)){bool done=false;welcomeArchive=archiveSdData(done);if(done)device::prefPutInt("mmsetup","archived",1);}
 loadLights();device::powerBegin(displayBrightness,keyboardBrightness);loadChats();loadRegionScopes();loadChannelNotifications();loadWallpaper();loadTextSize();loadAccentMode();loadKeyboardLanguages();
 batteryLogOn=device::prefGetInt("mmbatlog","on",0)!=0;// off by default; "battery log on" enables it (saved)
loadMapStyle();
 device::audioBegin();
 if(sdReady){loadBindings();radioJournal.begin(SD);
  uint32_t latest[MAX_CHATS]={};for(auto &record:radioJournal.index)if(!record.mine&&record.chat>=0&&record.chat<chatCount)latest[record.chat]=record.token;
  for(int i=0;i<chatCount;i++)if(!(radioJournal.readTrackedChats&(uint32_t(1)<<i)))radioJournal.markRead(i,latest[i]);
  device::restoreUnreadCounts(radioJournal.index,radioJournal.readTokens,unreadMessages,size_t(chatCount));
  refreshLastReceived();matter::begin(SD,incomingRadio,deliveredRadio);for(auto &scope:regionScopes)matter::registerScope(scope.c_str());configureChannels();}
 {device::BatteryLoad boot;boot.active=true;boot.cpuHigh=true;device::batteryLoad(boot);}// screen on at boot: the first sample is taken under load
 device::batteryLoop();device::locationBegin();if(sdReady){device::LocationStore store;store.load=gpsStoreLoad;store.save=gpsStoreSave;device::locationStore(store);}appLinkBegin();device::linkBegin();device::connectivityBegin();device::weatherBegin();device::stormBegin();device::radarBegin();device::otaBegin();
 {auto r=matter::snapshot();Serial.printf("RADIO_BOOT ready=%d frequency=%.3f band=%d bw=%.2f sf=%u cr=%u power=%d error=%d\n",r.ready,r.config.frequency,matter::radioBand(r.config.frequency),r.config.bandwidth,r.config.sf,r.config.cr,r.config.power,r.error);}
 splash(100,sdReady?"Ready for the field.":"SD unavailable / check card");
 if(!sdReady)notice="SD unavailable / check card";
 // Clean install (mark 2) -> welcome. Without a mark (set up before the mark existed): radio settings on the SD mean
 // the device is set up, otherwise the welcome runs.
 {int mark=device::prefGetInt("mmsetup","done",0);if(mark==2||(!mark&&!(sdReady&&SD.exists("/mattermesh/radio.conf"))))
  {Serial.printf("FIRST_BOOT clean=%d sd=%d\n",cleanInstall,sdReady);delay(400);startWelcome();handle("status");return;}
  if(!mark)device::prefPutInt("mmsetup","done",1);}
 delay(400);render();handle("status");
}
// T-Deck Plus trackball: click = Enter (Back stays on Backspace with an empty field). Rolling moves the caret or
// scrolls the chat while writing, elsewhere it sends the w/a/s/d navigation keys of the keyboard.
constexpr uint32_t TrackballLockMs=900;constexpr int TrackballStep=3;constexpr uint32_t TrackballDoubleMs=1000;
// Presses counted by an interrupt: while locked the loop can be busy repainting the lock screen for a few hundred ms,
// and a click that both starts and ends in that time was lost by polling (the double click never counted).
static volatile uint32_t trackballPresses=0,trackballPressAt=0;
static void IRAM_ATTR trackballPressIsr(){uint32_t t=millis();if(uint32_t(t-trackballPressAt)>40){trackballPresses++;trackballPressAt=t;}}
void trackballBegin(){attachInterrupt(digitalPinToInterrupt(BOARD_BOOT_PIN),trackballPressIsr,FALLING);}
void pollTrackball(){
 // Click: unlock on the lock screen; elsewhere a short click is Enter (on release) and a 0.9 s hold locks the device.
 static bool clickDown=false,lockFired=false;static uint32_t clickAt=0;bool down=digitalRead(BOARD_BOOT_PIN)==LOW;uint32_t now=millis();
 int dx,dy;plus::Trackball::take(dx,dy);
 // Locked: a double click (second press within 600 ms) unlocks; a single click only lights the screen for 10 s.
 static uint32_t pressesSeen=0,firstPressAt=0;
 if(deviceLocked){
  uint32_t presses=trackballPresses,at=trackballPressAt;clickDown=down;
  if(presses!=pressesSeen&&!quickReply.active){uint32_t fresh=presses-pressesSeen;pressesSeen=presses;
   // Second click: both dots lit, unlock after the usual short countdown (pollLockKey); its release is not an Enter.
   if(fresh>=2||(firstPressAt&&uint32_t(at-firstPressAt)<=TrackballDoubleMs)){firstPressAt=0;clickAt=now;lockFired=true;clickDown=down;trackballUnlockTaps=2;showLockHint();if(!unlockCountdownAt)unlockCountdownAt=millis();lockScreenDirty=true;return;}
   firstPressAt=at;trackballUnlockTaps=1;device::wakeDisplay(10000);showLockHint();lockScreenDirty=true;}
  else if(presses!=pressesSeen)pressesSeen=presses;
  if(firstPressAt&&uint32_t(now-firstPressAt)>TrackballDoubleMs){firstPressAt=0;if(trackballUnlockTaps==1){trackballUnlockTaps=0;lockScreenDirty=true;}}
  return;}
 pressesSeen=trackballPresses;firstPressAt=0;// presses made while unlocked never count towards an unlock
 if(down&&!clickDown){clickDown=true;clickAt=now;lockFired=false;device::activity();}
 else if(down&&clickDown&&!lockFired){uint32_t held=now-clickAt;int dots=int(held*7/TrackballLockMs);if(held>=TrackballLockMs){lockFired=true;lockKey.progress=0;if(screen!=Screen::Welcome&&!welcomeWifi)enterLockMode();else render();return;}// no lock during the welcome
  if(held>=200&&dots!=lockKey.progress&&screen!=Screen::Welcome&&!welcomeWifi){lockKey.progress=dots;lockKey.started=clickAt;render();}}
 else if(!down&&clickDown){clickDown=false;if(lockKey.progress){lockKey.progress=0;render();}if(!lockFired&&uint32_t(now-clickAt)<TrackballLockMs)key('\n');}
 // Rolling: the balls toggle a line per detent step; three steps make one move (it used to be one, too twitchy).
 static int accX=0,accY=0;static uint32_t rolledAt=0;if(dx||dy){if(uint32_t(now-rolledAt)>400)accX=accY=0;rolledAt=now;accX+=dx;accY+=dy;}
 dx=accX/TrackballStep;dy=accY/TrackballStep;accX-=dx*TrackballStep;accY-=dy*TrackballStep;
 if(!dx&&!dy)return;device::activity();
 auto field=editableField();
 if(field){
  if(field==&input&&dx&&moveCursor(dx>0?1:-1)){notice="";repaintSettled();}
  if(dy&&screen==Screen::Chat)scrollChat(dy<0?1:-1);
  return;
 }
 renderHold++;for(int n=0;n<abs(dy);n++)key(dy<0?'w':'s');
 for(int n=0;n<abs(dx);n++)key(dx<0?'a':'d');
 renderHold--;if(!renderHold&&renderPending){renderPending=false;render();}// one frame for all the steps of a roll
}
// OTA phase as of the last otaRevision() change (every phase change bumps the revision): loop() reads it without
// taking the OTA mutex and copying its Strings on every pass.
static device::OtaPhase otaPhaseNow=device::OtaPhase::Idle;
// Battery log for calibrating the Plus percent curve: one CSV row every 30 s on the SD (/mattermesh/battery-log.csv),
// with the loads that sag the cell. Serial "battery log on|off|status"; NVS "mmbatlog"/"on", off by default.
static void batteryLogLoop(){
 if(!batteryLogOn||!sdReady||(batteryLogAt&&millis()-batteryLogAt<30000))return;batteryLogAt=millis();
 auto t=device::batteryTrace();if(t.rawMv<0)return;
 bool fresh=!SD.exists("/mattermesh/battery-log.csv");File f=SD.open("/mattermesh/battery-log.csv",FILE_APPEND);if(!f)return;
 if(fresh)f.println("uptime_s,epoch,raw_mv,filtered_mv,curve_pct,shown_pct,source,cpu_mhz,locked,display_on,keyboard_light,wifi,ble_app,gps,radio_sent,radio_received");
 auto c=device::connectivityStatus();auto p=device::powerState();auto r=matter::radioCounters();uint32_t now=uint32_t(time(nullptr));
 f.printf("%lu,%lu,%d,%d,%d,%d,%d,%lu,%d,%d,%d,%d,%d,%d,%lu,%lu\n",(unsigned long)(millis()/1000),(unsigned long)(now>=1704067200?now:0),t.rawMv,t.filteredMv,t.curvePercent,t.shownPercent,t.source,(unsigned long)getCpuFrequencyMhz(),deviceLocked,p.displayAwake&&p.displayEnabled,p.keyboardAwake&&p.keyboardEnabled,c.connected,device::bleCompanionConnected(),device::locationState().enabled,(unsigned long)r.sent,(unsigned long)r.received);
 f.close();batteryLogRows++;}
void loop(){
 while(Serial.available()){char c=Serial.read();if((appUsbState||(command.length()==0&&c=='<'))&&appUsbByte(uint8_t(c)))continue;// MeshCore app frame over USB
  if(c=='\n'){
  command.trim();uint32_t request=0;if(command.startsWith("@")){int split=command.indexOf(' ');if(split>1){request=strtoul(command.c_str()+1,nullptr,10);command.remove(0,split+1);}}
  handle(command);command="";if(request)Serial.printf("USB_DONE %lu\n",request);
 }else if(c!='\r'&&command.length()<256)command+=c;}
 matter::loop();device::audioLoop();
 if(!deviceLocked&&screen==Screen::Chat&&archivePager.poll(radioJournal)){chatRowsRevision++;repaintNow();}
 if(deviceLocked)incomingBanner.clear();else if(incomingBanner.expire(millis())){repaintNow();}
 static uint32_t audioRevision=0;auto nextAudioRevision=device::audioRevision();if(nextAudioRevision!=audioRevision){audioRevision=nextAudioRevision;auto audio=device::audioState();if(!deviceLocked){bool visibleAudio=screen==Screen::Section&&section==21;if(visibleAudio){notice=audio.message;repaintSettled();}}}
 // Recording counter 0..6 s: repaint the composer once per elapsed second.
 const bool audioBusy=device::audioBusy();
 device::powerHold(audioBusy);
 {static uint32_t loadAt=0;if(millis()-loadAt>=5000){loadAt=millis();device::BatteryLoad l;l.active=!deviceLocked||quickReply.active;l.wifi=device::connectivityStatus().connected;l.gps=device::locationState().enabled;l.cpuHigh=getCpuFrequencyMhz()>=160;device::batteryLoad(l);}}// sag compensation
 device::batteryLoop();batteryLogLoop();device::powerLoop();
 // LCD asleep while the backlight is off (it kept scanning and taking frames in the dark); on wake the whole frame is sent again.
 if(lcd.sleep(device::powerState().displayPWM==0)&&!lcd.asleep){lcd.invalidate();repaintNow();}
 // OTA: repaint Settings / OTA update on progress; a newly offered release shows a banner once per boot.
 {static uint32_t otaSeen=0;static bool lockedSeen=false;auto revision=device::otaRevision();if(revision!=otaSeen||deviceLocked!=lockedSeen){otaSeen=revision;lockedSeen=deviceLocked;auto o=device::otaState();otaPhaseNow=o.phase;
  if(!deviceLocked&&screen==Screen::Section&&(section==17||section==19))markRadioDirty();
  if(o.phase==device::OtaPhase::Available&&o.code!=otaAnnounced&&!deviceLocked){otaAnnounced=o.code;otaBannerText=std::string("MatterMesh ")+o.version.c_str()+" / Settings > OTA update";otaBannerUntil=millis()+6000;if(!(screen==Screen::Section&&section==17))repaintNow();else otaBannerUntil=0;}/* found while locked: shown after the unlock */}
  if(otaBannerUntil&&int32_t(otaBannerUntil-millis())<=0){otaBannerUntil=0;if(!deviceLocked)repaintNow();}}
 static uint32_t serviceAt=0;if(millis()-serviceAt>=(deviceLocked?100u:20u)){serviceAt=millis();device::connectivityLoop();device::locationLoop();device::timeSyncLoop();device::weatherSetOpen(!deviceLocked&&((screen==Screen::Section&&section==10)||(screen==Screen::Home)));/* Plus: the Home header keeps the weather fresh (15 min) */device::weatherLoop();device::stormLoop();device::mapTilesLoop();if(deviceLocked)device::mapTilesCleanup();/* Plus: old OpenTopoMap caches */device::otaLoop(device::wifiConnected(),lastPaint!=0);appLinkLoop();welcomeWifiLoop();}
 static uint32_t timeRevision=0;auto clockRevision=device::timeSyncState().revision;if(clockRevision!=timeRevision){timeRevision=clockRevision;lockScreenDirty=true;if(!deviceLocked){markRadioDirty();}}
 serviceReplay();appVisibilityLoop();toolsTick();weatherAppsTick();pollKeyboard();pollQuickReply();pollLockKey();pollBanners();if(deviceLocked){if(quickReply.active){pollTouch();static uint32_t keepAt=0;/* Plus: the screen stays on while the quick reply is open */if(millis()-keepAt>=1000){keepAt=millis();device::wakeDisplay(15000);}}
 pollTrackball();/* the click unlocks: read it before the locked loop returns */if(!deviceLocked)return;
 updateLockScreen();delay(quickReply.active?5:20);return;}
 // Connectivity / weather / GPS revisions repaint only the pages that draw them; elsewhere only a status-bar icon change counts.
 static uint32_t connectionRevision=0,weatherRevision=0,locationRevision=0,statusSignature=0;auto connection=device::connectivityRevision();auto weather=device::weatherRevision();auto location=device::locationRevision();
 if(connection!=connectionRevision||weather!=weatherRevision||location!=locationRevision){bool page=screen==Screen::Section&&((connection!=connectionRevision&&(section==4||section==14||section==10||section==8||section==11))||(weather!=weatherRevision&&section==10)||(location!=locationRevision&&(section==3||section==9||section==16)));if(screen==Screen::Home&&(weather!=weatherRevision||connection!=connectionRevision))page=true;auto c=device::connectivityStatus();auto loc=device::locationState();bool gpsOn=loc.enabled&&loc.available;uint32_t signature=(c.wifi?1u:0u)|(c.ble&&c.bleReady?2u:0u)|(gpsOn?4u:0u);connectionRevision=connection;weatherRevision=weather;locationRevision=location;if(page||signature!=statusSignature){statusSignature=signature;markRadioDirty();}}
 static uint32_t lastPackets=0,lastSent=0,countersPaintedAt=0;static bool lastReady=false;auto radio=matter::radioCounters();if(radio.ready!=lastReady){lastReady=radio.ready;markRadioDirty();}
 if((radio.received!=lastPackets||radio.sent!=lastSent)&&screen==Screen::Section&&section==11&&uint32_t(millis()-countersPaintedAt)>=5000){lastPackets=radio.received;lastSent=radio.sent;countersPaintedAt=millis();markRadioDirty();}
 // The Contacts list follows the node table (new adverts) at most every 2 s.
 static uint32_t nodesRevision=0,nodesPaintedAt=0;auto nodesNow=matter::nodesRevision();if(nodesNow!=nodesRevision&&screen==Screen::Section&&section==1&&uint32_t(millis()-nodesPaintedAt)>=2000){nodesRevision=nodesNow;nodesPaintedAt=millis();markRadioDirty();}
 static uint32_t linkRev=0;if(device::linkRevision()!=linkRev){linkRev=device::linkRevision();if(screen==Screen::Section&&section==mm::LinkSection&&!deviceLocked){device::activity();markRadioDirty();}}// a page arriving restarts the auto-lock timer
 static uint32_t mapRevision=0;if(device::mapTilesRevision()!=mapRevision){mapRevision=device::mapTilesRevision();if((screen==Screen::Section&&section==9)||(screen==Screen::Chat&&messagePath&&messageRouteView&&routeLoading())){markRadioDirty();}}
 pollTouch();flingTick();static uint32_t radioFirstPending=0;if(radioDirty&&!radioFirstPending)radioFirstPending=millis();
 if(radioDirty&&(uint32_t(millis()-radioChangedAt)>=250||uint32_t(millis()-radioFirstPending)>=1000)&&!matter::busy()&&!touchPipeline.contactActive()){radioDirty=false;radioFirstPending=0;render(true);}if(!radioDirty)radioFirstPending=0;
 pollKeyboard();
 pollTrackball();
 {static uint32_t otaKeepAt=0;auto phase=otaPhaseNow;if((phase==device::OtaPhase::Downloading||phase==device::OtaPhase::Verifying||phase==device::OtaPhase::Restarting)&&uint32_t(millis()-otaKeepAt)>=1000){otaKeepAt=millis();device::activity();}}
 // Plus: an app from the Apps menu on screen (Maps, Weather, Files, CLI) keeps the device awake and unlocked.
 if(!deviceLocked&&screen==Screen::Section){static uint32_t appKeepAt=0;bool app=false;for(int i=0;i<mm::ApplicationCount;i++)if(mm::Applications[i].section==section)app=true;if(app&&uint32_t(millis()-appKeepAt)>=1000){appKeepAt=millis();device::activity();}}
 if(screen!=Screen::Welcome&&!welcomeWifi&&!(screen==Screen::Section&&section==mm::LinkSection&&device::linkMeta().phase==device::LinkPhase::Loading)&&device::autoLockDue(millis(),device::powerState().lastActivity,deviceLocked)){enterLockMode();delay(20);return;}
 static unsigned long statusAt=0;
 static time_t shownMinute=0;if(millis()-statusAt>=1000){statusAt=millis();time_t minute=time(nullptr)/60;if(minute!=shownMinute){shownMinute=minute;radioDirty=true;if(!radioFirstPending)radioChangedAt=millis();}}
 if(dirty&&(composerUrgent||((millis()-lastEdit>=UI_INPUT_SETTLE_MS||millis()-lastPaint>=UI_TYPING_FRAME_MS)&&millis()-lastPaint>=UI_MIN_FRAME_MS)))render(screen!=Screen::Chats);
 delay(idleCpuLoop(audioBusy)?20:5);
}
