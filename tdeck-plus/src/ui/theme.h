#pragma once
#include <stdint.h>
#include <algorithm>
namespace mm {
// T-Deck Plus ST7789 LCD, landscape 320x240, colour. No vibration motor and no on-device dictation.
constexpr int W=320,H=240,MaxSection=43;// 43: chat Path (dm_path_layout.h)

constexpr bool Landscape=true;
// T-Deck Plus colour LCD: the inks are runtime values so the themes (palette + illustrations) share every component.
// Defaults are "Alpine Light": warm paper background, night-blue ink, alpine green accent, sage / glacier surfaces.
// Colours are stored in the panel's byte order (RGB565 high byte first in memory), so a frame row goes to the
// ST7789 as it is; shot/preview code swaps back with panelToRgb565().
constexpr uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b){return uint16_t(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3));}
constexpr uint16_t rgb(uint8_t r,uint8_t g,uint8_t b){return uint16_t(rgb565(r,g,b)<<8|rgb565(r,g,b)>>8);}
constexpr uint16_t panelToRgb565(uint16_t c){return uint16_t(c<<8|c>>8);}
constexpr uint16_t rgb565ToPanel(uint16_t c){return panelToRgb565(c);}// the same byte swap, back
// lockLayout: where the lock screen puts the clock over the theme's illustration.
enum class LockLayout {RightColumn,BottomLeft,TopLeft};
struct Palette {const char *name,*detail;uint16_t bg,fg,accent,onAccent,surface,surfaceAlt,muted,line,mist,statusBg,statusFg,lockInk;LockLayout lockLayout;};
extern uint16_t BG,FG,Accent,OnAccent,Surface,SurfaceAlt,Muted,Line,Mist,StatusBg,StatusFg,LockInk;
constexpr int ThemeCount=3;
int themeIndex();const Palette &theme(int index);void setTheme(int index);
// Chat text size (Settings > Text size): Large = ChatText 12/17, Compact = 11/16. Persisted by main.cpp.
void setChatTextLarge(bool large);bool chatTextLarge();
constexpr bool Color=true;
constexpr int Margin=8,Gap=6,Radius=5,StatusH=18,HeaderH=32,FooterH=18;
constexpr int Top=StatusH+HeaderH,Bottom=H-Margin,FooterY=H-FooterH;
constexpr int Touch=28,IconStatus=12,IconRow=24;
constexpr int StatusTextY=3,StatusIconY=2,StatusClockW=40;
constexpr int StatusAntennaX=Margin+StatusClockW+5,StatusAntennaW=30;
constexpr int HoldBannerY=StatusH,HoldBannerH=36;
constexpr int HoldDots=6,HoldDotR=3,HoldDotStep=9,HoldDotsW=HoldDots*HoldDotStep;// hold banner progress dots (trackball lock hold), right-aligned like the unlock taps
constexpr int StatusIconStep=18,StatusIconsX=W-152;
constexpr int StatusBatteryY=3,StatusBatteryW=25,StatusBatteryH=12;// Plus: iOS-style battery

constexpr int StatusRightMargin=Margin,StatusBatteryGap=5,StatusIndicatorGap=4;
constexpr int ChatSubtitleH=0;
constexpr int BubbleMargin=10,BubbleMaxW=W-2*BubbleMargin-12,BubblePadding=4,BubbleGap=2;
constexpr int ComposerH=36,ComposerY=H-ComposerH,ComposerMaxLines=6;// expanded (dictated) composer: up to 6 wrapped rows
constexpr int FirstRepeaterH=15,FirstRepeaterY=ComposerY-FirstRepeaterH;
constexpr unsigned MessageCounterThreshold=95;
// Plus chat list: four 36 px cards (avatar, name, one preview line) over a search / New bar; the Max keeps 60 px rows.
constexpr int ChatRowH=38,ListRows=4;
// Landscape form: name and region scope side by side, the key below, then the actions.
constexpr int FormNameY=Top+24,FormKeyY=(Top+76),FormKeyH=40,FormScopeY=FormNameY,FormActionsY=(FormKeyY+FormKeyH+8);
constexpr int AppCols=4,AppGap=3,AppW=(W-12-AppGap*(AppCols-1))/AppCols,AppTileH=((Bottom-Top-4-AppGap)/2),AppStep=(AppTileH+AppGap);// landscape: 4 x 2 tiles filling the page

// Lock screen: date row (replaced by the unlock hint while it shows), big clock with the stylized Matterhorn in front
// of its lower part (hero bitmap from LockHeroY, sky transparent), message card over the slopes, repeater footer below.

constexpr int LockMessageY=152,LockMessageH=58,LockRepeaterGap=4; // sender/time + channel rows above the preview

struct Rect { int x,y,w,h; bool contains(int px,int py)const{return px>=x&&px<x+w&&py>=y&&py<y+h;} };
constexpr Rect LockMessage={Margin,LockMessageY,W-2*Margin,LockMessageH};
constexpr Rect LockRepeater={Margin,LockMessageY+LockMessageH+LockRepeaterGap,W-2*Margin,H-(LockMessageY+LockMessageH+LockRepeaterGap)};
// Quick Reply covers the lock screen below the date row (clock + photo), taller composer.
constexpr Rect QuickReplyPanel={Margin,HoldBannerY+HoldBannerH,W-2*Margin,H-(HoldBannerY+HoldBannerH)-8},QuickReplyComposer={Margin+8,130,W-2*Margin-16,64};
// Landscape: tighter header rows, one preview line, buttons on the right of a two-line key hint.
// Emoji button of the quick reply, left of SEND (Plus); opens the chat emoji picker over the panel.
constexpr Rect QuickReplyEmoji={(W-Margin-174-36),(H-38),30,28};
constexpr Rect QuickReplySend={(W-Margin-174),(H-38),80,28},QuickReplyCancel={(W-Margin-88),(H-38),80,28};
constexpr int QuickTitleDY=6,QuickChannelDY=27,QuickSenderDY=43,QuickPreviewDY=61,QuickPreviewLines=1;
inline Rect appRect(int index){return {6+(index%AppCols)*(AppW+AppGap),Top+4+(index/AppCols)*AppStep,AppW,AppTileH};} // 3 rows fit above Bottom

// New-channel form: 0 name, 1 secret key (two lines) or hashtag note, 2 region scope.
inline Rect formRect(int field){if(field!=1)return {field?W/2+3:Margin,FormNameY,(W-2*Margin-6)/2,30};return {Margin,field==0?FormNameY:field==1?FormKeyY:FormScopeY,W-2*Margin,field==1?FormKeyH:30};}
constexpr Rect ComposerEmoji={W-Margin-32,ComposerY+2,30,30};
constexpr Rect NotificationBanner={6,2,W-12,44};constexpr int NotificationRadius=12,NotificationDisc=30,NotificationTagW=90;// floats over the status bar and the page header (title + menu), its shadow ends above Top-1
// Emoji picker: 6x4 glyphs on the Max; the Plus spreads 8x3 across the width with a lower panel (tab/choice rects in emoji_choices.h).
constexpr int EmojiCols=8,EmojiRows=3;
constexpr Rect EmojiPopup=(Rect{Margin,ComposerY-162,W-2*Margin,160});

constexpr Rect ChatMenu={W-32,StatusH,32,HeaderH};
// T-Deck Plus: no Antenna row (one LoRa antenna), Wallpaper becomes Themes.
constexpr int SettingsCount=19,SettingsRowH=36;
// Six groups, each label above its first row (navigation_catalog.h SettingsItems is in this order).
constexpr int SettingsGroupCount=6;
inline constexpr int SettingsGroupFirst[SettingsGroupCount]={0,1,4,9,12,15};
inline constexpr const char *SettingsGroupNames[SettingsGroupCount]={"RADIO & MESH","CONNECTIONS","DISPLAY","KEYBOARD & SOUND","DEVICE","SYSTEM"};
// First-boot welcome (Plus landscape): BACK / NEXT at the bottom, two or three choice cards, preset rows.
constexpr Rect WelcomeBack={Margin,H-38,92,30},WelcomeNext={W-Margin-128,H-38,128,30};
constexpr int WelcomeRows=4,WelcomeRowH=34,WelcomeRowY=60;
inline Rect welcomeCard(int i,int n){int gap=8,w=(W-2*Margin-gap*(n-1))/n;return {Margin+i*(w+gap),70,w,92};}
inline Rect welcomeRow(int i){return {Margin,WelcomeRowY+i*WelcomeRowH,W-2*Margin-6,WelcomeRowH-3};}
constexpr Rect WelcomeNameField={Margin,74,W-2*Margin,34};
// OTA update page: the action button at the bottom (Plus landscape) or under the cards (Max).
constexpr Rect UpdatesAction=(Rect{Margin,Bottom-34,W-2*Margin,32});
constexpr int SettingsGpsIndex=3;
// Settings scroll by pixels like the chat: scroll = content pixels above the list top. Rows (and group labels)
// partly out of the list are drawn and cut by the header, which is painted again on top.
constexpr int SettingsHeaderH=18,SettingsCardH=34,SettingsListY=Top+4,SettingsViewH=Bottom-SettingsListY;
inline bool settingsGroupFirst(int index){for(int f:SettingsGroupFirst)if(f==index)return true;return false;}
inline int settingsContentY(int index){int groups=0;for(int f:SettingsGroupFirst)if(f<=index)groups++;return index*SettingsRowH+groups*SettingsHeaderH;}
inline Rect settingsRect(int index,int scroll){return {Margin,SettingsListY+settingsContentY(index)-scroll,W-2*Margin,SettingsCardH};}
// Visible: whole inside the list (focus, keys); shown: any part of it on screen (drawn, tappable below the header).
inline bool settingsVisible(int index,int scroll){auto r=settingsRect(index,scroll);return r.y>=SettingsListY&&r.y+r.h<=Bottom;}
inline bool settingsShown(int index,int scroll){auto r=settingsRect(index,scroll);return r.y+r.h>SettingsListY&&r.y<H;}
inline int settingsFirstVisible(int scroll){int i=0;while(i+1<SettingsCount&&!settingsVisible(i,scroll))i++;return i;}
inline int settingsLastVisible(int scroll){int i=settingsFirstVisible(scroll);while(i+1<SettingsCount&&settingsVisible(i+1,scroll))i++;return i;}
inline int settingsMaxScroll(){return std::max(0,settingsContentY(SettingsCount-1)+SettingsCardH-SettingsViewH);}
// The nearest scroll showing a row whole, with its group label when it opens a group.
inline int settingsReveal(int index,int scroll){int top=settingsContentY(index)-(settingsGroupFirst(index)?SettingsHeaderH:0),bottom=settingsContentY(index)+SettingsCardH;
 if(top<scroll)scroll=top;else if(bottom>scroll+SettingsViewH)scroll=bottom-SettingsViewH;return std::max(0,std::min(settingsMaxScroll(),scroll));}
// One page: the list height less one row, so the last row seen stays on screen.
inline int settingsPage(int scroll,bool down){int d=SettingsViewH-SettingsRowH;return std::max(0,std::min(settingsMaxScroll(),scroll+(down?d:-d)));}
// Settings pages share one layout: switch card on top, then uppercase group labels with cards below.
constexpr Rect SettingsSwitch={Margin,Top+6,W-2*Margin,40};
constexpr int SettingsSectionY=Top+54,SettingsBodyY=Top+70;
inline Rect settingsHalf(int column,int y,int h=40){return {column?W/2+3:Margin,y,(W-2*Margin-6)/2,h};}
inline Rect settingsThird(int column,int y,int span=1,int h=40){int w=(W-2*Margin-12)/3;return {Margin+column*(w+6),y,w*span+6*(span-1),h};}
constexpr const char *DeviceLabel="T-Deck Plus 320x240";
constexpr Rect GpsToggle=SettingsSwitch;
// Text size page (Plus, section 31): two sample cards (Compact / Large) and the info line below.
// Settings > Keyboard languages (section 12): search, KeyboardRows language rows with a switch, help under the list.
constexpr int KeyboardSection=12,KeyboardRows=3,KeyboardRowH=28;// one-line rows: name, code / English name, switch
constexpr Rect KeyboardSearch={Margin+6,Top+6,W-2*(Margin+6),26};
inline Rect keyboardRowRect(int i){return {Margin,Top+36+i*(KeyboardRowH+2),W-2*Margin,KeyboardRowH};}
constexpr int KeyboardHelpY=Top+38+KeyboardRows*(KeyboardRowH+2);// one how-to line and three help lines fit below the rows
constexpr int TextSizeSection=31,TextSizeInfoY=Top+126;
// Date / time (Plus): five spinner boxes DD MM YYYY  HH MM (upper half +, lower half -) and SET at the bottom.
constexpr int DateTimeBoxY=Top+80,DateTimeBoxH=50;constexpr Rect DateTimeSet={Margin,DateTimeBoxY+DateTimeBoxH+8,W-2*Margin,28};
inline Rect dateTimeBox(int i){const int w[5]={44,44,66,44,44},gap[5]={0,8,8,24,8};int x=Margin+(W-2*Margin-(44*4+66+48))/2;for(int k=0;k<=i;k++)x+=gap[k]+(k?w[k-1]:0);return {x,DateTimeBoxY,w[i],DateTimeBoxH};}
inline Rect textSizeCardRect(int i){return settingsHalf(i,Top+6,112);}
// Accent key page (Plus, section 32): three choice cards (Off / Right Shift / Left Shift), the help lines below.
constexpr int AccentKeySection=32,AccentInfoY=Top+92;
inline constexpr char AccentKeyHowTo[]="In a chat, tap it before a letter: once for the first accent, twice for the second.";
// Link viewer (section 33, opened from a chat message): a 26 px bar under the status bar (back disc, host / title,
// PICTURE or PAGE switch), then the page text rows or the picture; round zoom buttons float over a picture.
constexpr int LinkSection=33,LinkBarH=26,LinkRowH=18;
constexpr Rect LinkBack={4,StatusH+2,22,22},LinkSwitch={W-Margin-62,StatusH+3,62,20},LinkArea={0,StatusH+LinkBarH,W,H-StatusH-LinkBarH};
constexpr int LinkRows=(H-StatusH-LinkBarH-4)/LinkRowH;
// Settings > Donate / Unlock (section 34, between OTA update and About): Donate / Extra apps tabs and the QR of the
// selected one, the text beside it.
constexpr int SupportSection=34,SupportQrY=Top+42;
inline Rect supportTabRect(int i){return settingsHalf(i,Top+6,28);}
constexpr Rect LinkZoomOut={W-Margin-30,H-Margin-30,30,30},LinkZoomIn={W-Margin-30,H-Margin-64,30,30};
inline Rect linkRowRect(int row){return {0,StatusH+LinkBarH+2+row*LinkRowH,W,LinkRowH};}
inline Rect accentCardRect(int i){return settingsThird(i,Top+6,1,72);}
// Lights: display and keyboard label + slider rows (44 px rows, 22 px sliders), then a hint line.
constexpr int ControlFirstY=(Top+8),ControlStep=44,ControlSliderH=22;
constexpr int HapticY=ControlFirstY+ControlStep*2;
constexpr Rect lightSliderRect(int row){return {Margin,ControlFirstY+row*ControlStep+18,W-2*Margin,ControlSliderH};}

constexpr int HapticTrackX=Margin+8,HapticTrackW=W-2*Margin-16;
// Ringtone / vibration rows end with "<" (at w-44) and ">" (at w-20): a tap on "<" steps back, anywhere else forward.
inline int stepRowTap(Rect r,int x){return x>=r.x+r.w-56&&x<r.x+r.w-26?-1:1;}

constexpr Rect lightToggleRect(int row){return {W-62,ControlFirstY+row*ControlStep-4,54,22};}
constexpr Rect BackButton={0,StatusH,32,HeaderH};

constexpr Rect ComposeButton={W-32,StatusH,32,HeaderH};

constexpr Rect MessageMenu={W-32,StatusH,32,HeaderH};
constexpr int MessagesPopupRowH=42,MessagesMenuRows=4;// row 0 new direct chat, rows 1..3 the channel forms
constexpr Rect MessagesPopup={14,Top+2,W-28,MessagesMenuRows*MessagesPopupRowH+2};
// New direct chat: contact picker (search field, 4 rows, footer pager).
constexpr Rect PickerSearch={Margin,Top+4,W-2*Margin,26};
constexpr int PickerRowH=46,PickerRows=3;
inline Rect pickerRowRect(int row){return {Margin,Top+34+row*PickerRowH,W-2*Margin,PickerRowH-4};}
constexpr Rect PickerPrev={W-76,FooterY,34,FooterH},PickerNext={W-38,FooterY,34,FooterH};
constexpr Rect messageListRect(int row,bool searching=false){return (Rect{Margin,Top+2+row*ChatRowH,W-2*Margin-6,ChatRowH-2});}
constexpr Rect ChatsSearch={Margin,H-30,W-2*Margin-82,26},ChatsNew={W-Margin-76,H-30,76,26};// Plus bottom bar
// Wi-Fi page. Plus: ON/OFF in the header, a status line, four one-line network rows, SCAN / HIDDEN / DISCONNECT at the bottom.
constexpr Rect WifiToggle=(Rect{W-Margin-64,StatusH+6,64,22}),WifiScan=(Rect{Margin,H-30,84,26});
constexpr int WifiListY=(Top+30),WifiRowH=32,WifiRows=4;
inline Rect wifiRowRect(int index){return (Rect{Margin,WifiListY+index*WifiRowH,W-2*Margin-6,WifiRowH-3});}
constexpr Rect WifiManual=(Rect{Margin+90,H-30,92,26}),WifiDisconnect=(Rect{Margin+188,H-30,W-2*Margin-188,26});
constexpr Rect WifiSSIDInput={Margin,Top+24,W-2*Margin,32},WifiEye={W-Margin-32,Top+82,32,32};
constexpr Rect WifiPassword={Margin,Top+82,W-2*Margin,32},WifiConnect={Margin,Top+128,108,30},WifiCancel={124,Top+128,108,30};
constexpr Rect BleToggle=SettingsSwitch;
// Plus Bluetooth page: name / PIN cards, then the app-over-Wi-Fi switch.
constexpr Rect AppNameCard={Margin,Top+52,(W-2*Margin-6)*3/5,40},AppPinCard={Margin+(W-2*Margin-6)*3/5+6,Top+52,(W-2*Margin-6)-(W-2*Margin-6)*3/5,40},AppWifiToggle={Margin,Top+98,W-2*Margin,40};
constexpr int WeatherPlaceY=Top+3,WeatherHeroY=Top+22,WeatherIconX=Margin+4,WeatherTempX=86,WeatherHoursY=Top+90,WeatherHoursH=58,WeatherHoursColumns=4,WeatherGridY=(Top+152),WeatherCellH=30,WeatherStatusY=(Top+22);
// Landscape: status, source and REFRESH beside the temperature; the four values on one row under the hours.
constexpr int WeatherSideX=W-Margin-96,WeatherSideW=96;
// Plus Weather tabs in the header: NOW, STORMS, RADAR. Storms tab: status line, three two-line event rows, REFRESH / ALERTS.
inline Rect weatherTabRect(int i){return {W-Margin-3*66+i*66,StatusH+6,62,21};}
constexpr int StormRows=3,StormRowY=Top+28,StormRowH=42;
inline Rect stormRowRect(int row){return {Margin,StormRowY+row*StormRowH,W-2*Margin-6,StormRowH-3};}
constexpr Rect StormRefresh={Margin,H-30,96,26},StormAlertsButton={Margin+102,H-30,130,26};
// RADAR tab: the map below the header, zoom - / + at the bottom right.
constexpr Rect RadarZoomOut={W-Margin-70,H-34,30,26},RadarZoomIn={W-Margin-34,H-34,30,26};
constexpr Rect WeatherRefresh=(Rect{WeatherSideX,Top+56,WeatherSideW,26}),WeatherPrompt={Margin,WeatherHoursY,W-2*Margin,92},WeatherWifiButton=(Rect{Margin+20,WeatherHoursY+62,W-2*Margin-40,26});
constexpr int WeatherPromptTitleDY=28,WeatherPromptTextDY=46,WeatherPromptIconDY=4;
inline Rect weatherCell(int i){{int w=(W-2*Margin-18)/4;return {Margin+i*(w+6),WeatherGridY,w,WeatherCellH};}}
constexpr int FileListY=Top+20,FileRowH=34,FileRows=(Bottom-FileListY)/FileRowH;
inline Rect fileRowRect(int row){return {Margin,FileListY+row*FileRowH,W-2*Margin,FileRowH-2};}
// Wallpaper: four thumbnail cards (hero at 1/5 scale) side by side, focused = filled, active = inner frame;
// the focused wallpaper's name and description sit below the row.
// T-Deck Plus: the Wallpaper page is the Themes page (three cards: lock thumbnail, name, colour swatches).
constexpr int WallpaperCardCount=3;
inline Rect wallpaperCardRect(int index){{int w=(W-2*Margin-12)/3;return {Margin+index*(w+6),Top+6,w,112};}}
constexpr int WallpaperInfoY=(Top+124);
// Landscape: 44 px timer rows, the six timeout chips spread over the width.
inline Rect powerRowRect(int index){return {Margin,Top+22+index*(44),W-2*Margin,40};}
inline Rect timeoutOptionRect(int index){return (Rect{Margin+index*51,Top+126,46,28});}
// Sound page: switch, volume row (- slider +), message / mention ringtone rows.
constexpr Rect AudioNotify=SettingsSwitch,AudioMinus={Margin,Top+70,36,28},AudioSlider={Margin+42,Top+70,W-2*Margin-84,28},AudioPlus={W-Margin-36,Top+70,36,28};
// Landscape: the two ringtone rows side by side (the page is 240 px tall).
constexpr Rect AudioToneMessage=(Rect{Margin,Top+124,(W-2*Margin-6)/2,40}),AudioToneMention=(Rect{W/2+3,Top+124,(W-2*Margin-6)/2,40});

constexpr Rect SaveButton={Margin,FormActionsY,(W-3*Margin)/2,30};
constexpr Rect CancelButton={W/2+Margin/2,FormActionsY,(W-3*Margin)/2,30};
constexpr int ReceivedMessageActions=5;
// Row 1 ("View full path") shares its line with MAP (packet route on the map, action 6).
constexpr int MessageMapW=64,MessageMapAction=6;
constexpr int MessageActionY=(Top+33),MessageActionStep=29,MessageActionH=26;
constexpr Rect messageMapRect(){return {Margin+8+(W-2*Margin-16)-MessageMapW,MessageActionY+MessageActionStep,MessageMapW,MessageActionH};}
// LINK (open the message's first link in the link viewer) sits right of "Read message", like MAP on the path row.
constexpr Rect messageLinkRect(){return {Margin+8+(W-2*Margin-16)-MessageMapW,MessageActionY+2*MessageActionStep,MessageMapW,MessageActionH};}
constexpr int receivedMessageAction(int row){return row==0?0:row==1?2:row==2?5:row==3?1:3;}
constexpr Rect messageActionRect(int index){return {Margin+8,MessageActionY+index*MessageActionStep,W-2*Margin-16,MessageActionH};}
// Landscape: the options as a 2-column grid (a direct chat adds Path as the fifth) (one column of 44 px rows ran off the 240 px screen).
inline Rect chatOptionRect(int index){int w=(W-2*Margin-16-8)/2;return {Margin+8+(index%2)*(w+8),Top+36+(index/2)*46,w,38};}
constexpr int ChatOptionCount=4;// channels; a direct chat adds Path
inline int chatOptionCount(bool direct){return direct?ChatOptionCount+1:ChatOptionCount;}

// Channel options rows: notifications, show key + QR (channels only), delete.
// Landscape: the first action full width, the other two side by side below it.
inline Rect channelOptionRect(int slot){{if(!slot)return {Margin+8,Top+70,W-2*Margin-16,38};int w=(W-2*Margin-22)/2;return {Margin+8+(slot-1)*(w+6),Top+116,w,38};}}
constexpr Rect ChannelAction={Margin+8,(Top+70),W-2*Margin-16,38};
// Landscape dialogs (T-Deck Plus, 240 px tall): the action buttons sit on one row at the bottom of the card.
constexpr int DialogButtonY=Bottom-40,DialogHalfW=(W-2*Margin-22)/2;
constexpr Rect PositionSend=(Rect{Margin+8,DialogButtonY,DialogHalfW,30});
constexpr Rect PositionCancel=(Rect{W-Margin-8-DialogHalfW,DialogButtonY,DialogHalfW,30});
constexpr int ScopeRows=6,ScopeRowH=30,ScopeRowY=Top+42;
inline Rect scopeRowRect(int row){{int w=(W-2*Margin-22)/2;return {Margin+8+(row%2)*(w+6),ScopeRowY+(row/2)*ScopeRowH,w,28};}}
// Landscape: APPLY / ADD / CANCEL share the bottom row, scopes in two columns above.
constexpr Rect ScopeAdd=(Rect{Margin+106,Bottom-38,W-2*Margin-212,28});
constexpr Rect ScopeApply={Margin+8,(Bottom-38),92,28},ScopeCancel={(W-Margin-100),(Bottom-38),92,28};
constexpr Rect ScopeInput={Margin+8,Top+88,W-2*Margin-16,36};
constexpr Rect ScopeSave={Margin+8,Top+142,101,30},ScopeEditCancel={W-Margin-109,Top+142,101,30};
constexpr Rect ChatClearYes={Margin+8,DialogButtonY,101,30},ChatClearNo={W-Margin-109,DialogButtonY,101,30};

constexpr Rect RadioPowerInput={Margin+8,Top+88,W-2*Margin-16,36};
constexpr Rect RadioPowerSave={Margin+8,Top+142,(W-2*Margin-22)/2,30};
constexpr Rect RadioPowerCancel={Margin+14+(W-2*Margin-22)/2,Top+142,(W-2*Margin-22)/2,30};
constexpr Rect LoRaPanel={Margin,Top,W-2*Margin,Bottom-Top};
constexpr int LoRaInset=Margin+6,LoRaContentW=W-2*LoRaInset;
// Landscape LoRa page: band row, preset, the five fields on the left and APPLY / ADVERTISE stacked on the right;
// the RX / path status moves to the footer line.
constexpr int LoRaBandY=(Top+4),LoRaBandH=22,LoRaPresetY=(Top+30),LoRaPresetH=30;
constexpr int LoRaFieldY=(Top+64),LoRaFieldStep=20,LoRaFieldW=(LoRaContentW-96),LoRaValueX=100;
// Band buttons 868 / 900 / 433 in thirds of the content width.
constexpr int LoRaBandW=(LoRaContentW-2*Gap)/3;
constexpr Rect LoRaBand868={LoRaInset,LoRaBandY,LoRaBandW,LoRaBandH},LoRaBand900={LoRaInset+LoRaBandW+Gap,LoRaBandY,LoRaBandW,LoRaBandH},LoRaBand433={LoRaInset+2*(LoRaBandW+Gap),LoRaBandY,LoRaBandW,LoRaBandH};
constexpr Rect LoRaPreset={LoRaInset,LoRaPresetY,LoRaContentW,LoRaPresetH};
constexpr Rect LoRaPresetPrev={LoRaInset,LoRaPresetY,30,LoRaPresetH},LoRaPresetNext={W-LoRaInset-30,LoRaPresetY,30,LoRaPresetH};
inline Rect loraFieldRect(int field){return {LoRaInset,LoRaFieldY+field*LoRaFieldStep,LoRaFieldW,LoRaFieldStep-1};}
constexpr Rect LoRaApply=(Rect{W-LoRaInset-90,LoRaFieldY,90,47});
constexpr Rect LoRaAdvert=(Rect{W-LoRaInset-90,LoRaFieldY+52,90,47});
constexpr int LoRaTabW=62,LoRaTabGap=4,LoRaTabY=StatusH+2,LoRaTabH=Top-LoRaTabY+1;
constexpr Rect LoRaRadio={W-Margin-2*LoRaTabW-LoRaTabGap,LoRaTabY,LoRaTabW,LoRaTabH},LoRaDevice={W-Margin-LoRaTabW,LoRaTabY,LoRaTabW,LoRaTabH};
// Contacts (Home tab, section 1): ALL / COMP / RPT tabs opening into the LoRa panel, search field, four rows, pager.
constexpr int ContactTabCount=3,ContactTabGap=2;constexpr int ContactTabWidths[ContactTabCount]={46,62,46};
inline Rect contactTabRect(int i){int x=W-Margin-Radius;for(int t=ContactTabCount-1;t>=i;t--)x-=ContactTabWidths[t]+(t==ContactTabCount-1?0:ContactTabGap);return {x,LoRaTabY,ContactTabWidths[i],LoRaTabH};}
// Plus: search field + count on one row, four full-width 36 px rows, finger / trackball scrolling (no pager).
constexpr Rect ContactSearch=(Rect{Margin,Top+6,W-2*Margin-96,26});
constexpr Rect ContactCount={W-Margin-90,Top+6,90,26};
constexpr int ContactRows=4,ContactRowH=36,ContactRowY=Top+38,ContactNoticeY=Bottom-46;
// Landscape lists keep their four rows as a 2 x 2 grid (read left to right).
inline Rect contactRowRect(int i){return {Margin,ContactRowY+i*ContactRowH,W-2*Margin-6,ContactRowH-3};}
constexpr Rect ContactPrevious=(Rect{-100,-100,1,1}),ContactNext=(Rect{-100,-100,1,1});// Plus: no pager buttons
constexpr Rect LoRaName={LoRaInset,Top+12,LoRaContentW,56};
constexpr int LoRaBytesY=Top+105;
constexpr int LoRaByteGap=4,LoRaByteW=(LoRaContentW-2*LoRaByteGap)/3;
inline Rect loraBytesRect(int bytes){return {LoRaInset+(bytes-1)*(LoRaByteW+LoRaByteGap),LoRaBytesY,LoRaByteW,34};}
constexpr Rect LoRaDeviceApply={LoRaInset,(Bottom-30),LoRaContentW,28};
// Landscape Home: two conversation rows and a compact tab bar (icon beside the label) at the bottom.
constexpr int MenuHomeRows=2,MenuHomeRowY=74,MenuHomeRowH=58,MenuHomeRowGap=4;
constexpr int MenuHomeTabCount=4,MenuHomeTabY=(H-42),MenuHomeTabH=38,MenuHomeTabGap=6;
constexpr int MenuHomeTabW=(W-2*Margin-(MenuHomeTabCount-1)*MenuHomeTabGap)/MenuHomeTabCount,MenuHomeTabIcon=24;
constexpr Rect MenuHomeClock={Margin,22,112,46};
constexpr int MenuHomeInfoX=MenuHomeClock.x+MenuHomeClock.w+Gap,MenuHomeInfoW=W-Margin-MenuHomeInfoX;
constexpr Rect MenuHomeDate={MenuHomeInfoX,MenuHomeClock.y,MenuHomeInfoW,MenuHomeClock.h/2};
constexpr Rect MenuHomeStatus={MenuHomeInfoX,MenuHomeClock.y+MenuHomeClock.h/2,MenuHomeInfoW,MenuHomeClock.h/2};
constexpr Rect NotificationBannerHome={6,2,W-12,MenuHomeRowY-8};// Home: covers the big clock too, two text lines
// Home: the widget box right of the clock, the clock band it sits in.
constexpr Rect HomeWidgetBox={W/2+4,StatusH+6,W-Margin-(W/2+4),40};// painted from its y; taps: the Wi-Fi button
constexpr int HomeWidgetGap=8,HomeWidgetTitleY=2,HomeWidgetDetailY=23;// mark to text, title and detail rows from the box top
constexpr int HomeBandY=StatusH+2,HomeBandH=MenuHomeRowY-1-HomeBandY;
// Settings > Home widgets (36): the Beside clock row and a preview of the Home band. Beside clock (37): a carousel,
// the Home band with the focused widget, the < name > selector, what it shows, and the position dots.
constexpr int HomeWidgetsSection=36,BesideClockSection=37;// 35 is Trails on the test line
constexpr Rect HomeWidgetsRow={Margin,Top+6,W-2*Margin,SettingsCardH};
constexpr Rect BesidePreview={Margin-2,Top+4,W-2*Margin+4,HomeBandH+4};
constexpr Rect BesideSelector={Margin,BesidePreview.y+BesidePreview.h+8,W-2*Margin,28};
// Home Main area (the two chat cards' place) and its Settings carousel (38: preview at full size, selector, a line).
constexpr Rect MainArea={6,MenuHomeRowY,W-12,MenuHomeTabY-4-MenuHomeRowY};
constexpr int MainAreaSection=38,NoteSection=39;
constexpr int MainTimelineRows=4,MainRowH=27,MainRowTop=9,MainRowSlop=4;// Timeline rows (drawn and tapped); slop: the divider sits 4 px above the next row
constexpr int MainDialX=60,MainDialR=52,MainTextX=130,MainPositionTextX=170,MainRadarMax=8,MainRadarRows=3,MainRadarRowH=20;// Radar / GPS sky dial centre and radius, text columns
constexpr int mainTimelineRow(int y){return (y-MainArea.y-MainRowTop+MainRowSlop)/MainRowH;}// the row a tap at y falls in; the caller checks the range
constexpr Rect MainPreview={MainArea.x,Top+4,MainArea.w,MainArea.h};
constexpr Rect MainSelector={Margin,MainPreview.y+MainPreview.h+8,W-2*Margin,26};
constexpr Rect HomeWidgetsMainRow={Margin,HomeWidgetsRow.y+HomeWidgetsRow.h+4,W-2*Margin,SettingsCardH};
constexpr Rect HomeHalfPreview={W/4,HomeWidgetsMainRow.y+HomeWidgetsMainRow.h+8,W/2,H-(HomeWidgetsMainRow.y+HomeWidgetsMainRow.h+8)-2};// Home widgets: the whole Home at half size (cropped at the bottom)
constexpr Rect NoteBox={Margin,Top+6,W-2*Margin,H-Top-34};
constexpr int BesideTextY=BesideSelector.y+BesideSelector.h+8,BesideDotsY=H-30,BesideDotPitch=10,BesideArrowW=56;// arrow zones: each end of the selector
constexpr Rect menuHomeRowRect(int row){return {6,MenuHomeRowY+row*(MenuHomeRowH+MenuHomeRowGap),W-12,MenuHomeRowH};}
constexpr Rect menuHomeTabRect(int tab){return {Margin+tab*(MenuHomeTabW+MenuHomeTabGap),MenuHomeTabY,MenuHomeTabW,MenuHomeTabH};}
}

namespace mm {
constexpr int HeardRows=2,HeardRowH=18,HeardListY=Top+94,HeardFooterY=(Top+132);
constexpr Rect HeardList={Margin+8,Top+90,W-2*Margin-16,70};
inline Rect ownMessageActionRect(int index){if(index==0)return {Margin+8,Top+36,W-2*Margin-16,30};
 {int w=(W-2*Margin-16-18)/4,slot=index==4?0:index==2?1:index==1?2:3;return {Margin+8+slot*(w+6),DialogButtonY+4,w,30};}}
// Maps app (section 9): full-width map above three buttons (zoom out, centre, zoom in).
// Plus: the map fills the page under the status bar; floating back / style pill at the top, round centre / + / - buttons bottom right.
constexpr Rect MapArea=(Rect{0,StatusH,W,H-StatusH});
constexpr Rect mapButtonRect(int index){return (Rect{W-Margin-30,H-Margin-30-(index==0?0:index==2?34:68),30,30});}
constexpr Rect MapStyleButton={W-Margin-60,StatusH+6,60,24},MapBackFloat={6,StatusH+6,28,28};
// Quick settings, opened by tapping the status bar: two 0..16 sliders with a
// label line above each track, then a 2x2 tile grid (index = row*2+column).
// Section 18 is Quick settings.
constexpr int QuickSettingsSection=18,QuickSteps=16,QuickTileCount=4;
enum QuickTile {QuickWifi,QuickBluetooth,QuickGps,QuickSound};
// A tile held QuickHoldMs (1 s) opens its Settings page (touch Hold target QuickHoldTarget + tile; chat ids stay below it).
constexpr int QuickHoldTarget=1000;constexpr uint32_t QuickHoldMs=1000;
constexpr int quickTileSection(int tile){return tile==QuickWifi?4:tile==QuickBluetooth?14:tile==QuickGps?3:21;}
// Landscape: shorter slider rows and the four tiles as 2 x 2.
constexpr int QuickSliderStep=44,QuickTileColumns=2,QuickTileY=(Top+94),QuickTileH=42;
constexpr Rect quickSliderRect(int row){return {Margin,Top+(20)+row*QuickSliderStep,W-2*Margin,24};}
constexpr int quickSliderLabelY(int row){return Top+4+row*QuickSliderStep;}
constexpr Rect quickTileRect(int index){return {Margin+(index%QuickTileColumns)*((W-2*Margin-Gap*(QuickTileColumns-1))/QuickTileColumns+Gap),QuickTileY+(index/QuickTileColumns)*(QuickTileH+(4)),(W-2*Margin-Gap*(QuickTileColumns-1))/QuickTileColumns,QuickTileH};}
inline int quickTileAt(int x,int y){for(int i=0;i<QuickTileCount;i++)if(quickTileRect(i).contains(x,y))return i;return -1;}
// Same inner track as Components::slider: step 0..QuickSteps under x.
inline int quickStepAt(Rect r,int x){int track=r.w-16,at=x-(r.x+8);at=at<0?0:at>track?track:at;return (at*QuickSteps+track/2)/track;}
}
