#pragma once
#include <stdint.h>
#include <algorithm>
namespace mm {
// GDEQ031T10 physical portrait panel. Alternate simulator sizes reuse this layout.
#ifndef MM_WIDTH
#define MM_WIDTH 240
#endif
#ifndef MM_HEIGHT
#define MM_HEIGHT 320
#endif
constexpr int W=MM_WIDTH,H=MM_HEIGHT,MaxSection=31;
constexpr uint16_t BG=0,FG=1; // GFXcanvas1 ink semantics; white=0, black=1.
constexpr int Margin=8,Gap=6,Radius=5,StatusH=18,HeaderH=32,FooterH=18;
constexpr int Top=StatusH+HeaderH,Bottom=H-Margin,FooterY=H-FooterH;
constexpr int Touch=28,IconStatus=12,IconRow=24;
constexpr int StatusTextY=3,StatusIconY=2,StatusClockW=40;
constexpr int StatusAntennaX=Margin+StatusClockW+5,StatusAntennaY=4,StatusAntennaW=30;
constexpr int HoldBannerY=StatusH,HoldBannerH=36;
constexpr int HoldDots=6,HoldDotR=3,HoldDotStep=9,HoldDotsW=HoldDots*HoldDotStep;// dots of the banner cards (unlock taps, input notices)
constexpr int StatusIconStep=18,StatusIconsX=W-152;
constexpr int StatusBatteryY=4,StatusBatteryW=14,StatusBatteryH=9;
constexpr int StatusRightMargin=Margin,StatusBatteryGap=5,StatusIndicatorGap=4;
constexpr int ChatSubtitleH=0;
constexpr int BubbleMargin=10,BubbleMaxW=W-2*BubbleMargin-12,BubblePadding=4,BubbleGap=2;
constexpr int ComposerH=36,ComposerY=H-ComposerH,ComposerMaxLines=6;// expanded (dictated) composer: up to 6 wrapped rows
constexpr int FirstRepeaterH=15,FirstRepeaterY=ComposerY-FirstRepeaterH;
constexpr unsigned MessageCounterThreshold=95;
constexpr int ChatRowH=60,ListRows=(Bottom-Top-4)/ChatRowH;
constexpr int FormNameY=Top+24,FormKeyY=FormNameY+62,FormKeyH=40,FormScopeY=FormKeyY+FormKeyH+28,FormActionsY=FormScopeY+42;
constexpr int AppCols=3,AppGap=3,AppW=(W-12-AppGap*2)/3;
// Lock screen: date row (replaced by the unlock hint while it shows), big clock with the stylized Matterhorn in front
// of its lower part (hero bitmap from LockHeroY, sky transparent), message card over the slopes, repeater footer below.
constexpr int LockDateY=StatusH+12,LockClockY=HoldBannerY+HoldBannerH+2,LockHeroY=StatusH;
constexpr int LockMessageY=200,LockMessageH=96,LockRepeaterGap=4,LockCardHeader=40; // sender/time + channel rows above the preview
constexpr int PartialLimit=20;
struct Rect { int x,y,w,h; bool contains(int px,int py)const{return px>=x&&px<x+w&&py>=y&&py<y+h;} };
constexpr Rect LockMessage={Margin,LockMessageY,W-2*Margin,LockMessageH};
constexpr Rect LockRepeater={Margin,LockMessageY+LockMessageH+LockRepeaterGap,W-2*Margin,H-(LockMessageY+LockMessageH+LockRepeaterGap)};
// Quick Reply covers the lock screen below the date row (clock + photo), taller composer.
constexpr Rect QuickReplyPanel={Margin,HoldBannerY+HoldBannerH,W-2*Margin,H-(HoldBannerY+HoldBannerH)-8},QuickReplyComposer={Margin+8,160,W-2*Margin-16,96};
constexpr Rect QuickReplySend={Margin+8,278,100,28},QuickReplyCancel={124,278,100,28};
inline Rect appRect(int index){return {6+(index%3)*(AppW+AppGap),Top+4+(index/3)*86,AppW,82};} // 3 rows fit above Bottom
// New-channel form: 0 name, 1 secret key (two lines) or hashtag note, 2 region scope.
inline Rect formRect(int field){return {Margin,field==0?FormNameY:field==1?FormKeyY:FormScopeY,W-2*Margin,field==1?FormKeyH:30};}
constexpr Rect ComposerEmoji={W-Margin-32,ComposerY+2,30,30};
constexpr Rect MessageBannerArea={0,StatusH,W,HeaderH};// covers the page header (title + menu)
constexpr Rect EmojiPopup={Margin,ComposerY-198,W-2*Margin,196};
constexpr Rect emojiTabRect(int index){return {EmojiPopup.x+4+index*36,EmojiPopup.y+4,34,26};}
constexpr Rect emojiChoiceRect(int index){return {EmojiPopup.x+4+(index%6)*36,EmojiPopup.y+34+(index/6)*36,34,34};}
constexpr Rect ChatMenu={W-32,StatusH,32,HeaderH};
constexpr int SettingsCount=18,SettingsRowH=36,SettingsDeviceFirst=5,SettingsSystemFirst=12;
constexpr int SettingsGpsIndex=4;
// Settings scroll by whole rows: scroll = first visible row, its group label above it
// when it opens a group. Rows outside the viewport get a y off screen and are not drawn.
constexpr int SettingsHeaderH=18,SettingsCardH=34,SettingsListY=Top+4;
inline bool settingsGroupFirst(int index){return index==0||index==SettingsDeviceFirst||index==SettingsSystemFirst;}
inline int settingsContentY(int index){return SettingsHeaderH+index*SettingsRowH+(index>=SettingsDeviceFirst?SettingsHeaderH:0)+(index>=SettingsSystemFirst?SettingsHeaderH:0);}
inline int settingsSnapY(int first){return settingsContentY(first)-(settingsGroupFirst(first)?SettingsHeaderH:0);}
inline Rect settingsRect(int index,int scroll){return {Margin,SettingsListY+settingsContentY(index)-settingsSnapY(scroll),W-2*Margin,SettingsCardH};}
inline bool settingsVisible(int index,int scroll){auto r=settingsRect(index,scroll);return index>=scroll&&r.y+r.h<=Bottom;}
inline int settingsLastVisible(int scroll){int i=scroll;while(i+1<SettingsCount&&settingsVisible(i+1,scroll))i++;return i;}
// Settings pages share one layout: switch card on top, then uppercase group labels with cards below.
constexpr Rect SettingsSwitch={Margin,Top+6,W-2*Margin,40};
constexpr int SettingsSectionY=Top+54,SettingsBodyY=Top+70;
inline Rect settingsHalf(int column,int y,int h=40){return {column?W/2+3:Margin,y,(W-2*Margin-6)/2,h};}
// Settings > Donate / Unlock (section 31, between OTA update and About): Donate / Extra apps tabs and the QR of the
// selected one, the text below it.
constexpr int SupportSection=31,SupportQrY=Top+42;
inline Rect supportTabRect(int i){return settingsHalf(i,Top+6,28);}
constexpr Rect GpsToggle=SettingsSwitch;
constexpr Rect UpdatesAction={Margin,Top+200,W-2*Margin,34};
// Date / time: SET MANUALLY, then fields day/month/year on one row and hour/minute below, -/+ and SAVE/CANCEL.
constexpr Rect DateManual={Margin,Top+210,W-2*Margin,34};
constexpr int DateFieldCount=5;
constexpr Rect dateFieldRect(int i){return i==0?Rect{Margin,Top+22,60,34}:i==1?Rect{Margin+66,Top+22,60,34}:i==2?Rect{Margin+132,Top+22,W-2*Margin-132,34}:i==3?Rect{Margin,Top+82,(W-2*Margin-6)/2,34}:Rect{W/2+3,Top+82,(W-2*Margin-6)/2,34};}
constexpr Rect DateMinus={Margin,Top+128,(W-2*Margin-6)/2,34},DatePlus={W/2+3,Top+128,(W-2*Margin-6)/2,34};
constexpr Rect DateSave={Margin,Top+210,(W-2*Margin-6)/2,34},DateCancel={W/2+3,Top+210,(W-2*Margin-6)/2,34};
inline int settingsMaxScroll(){int first=0;while(!settingsVisible(SettingsCount-1,first))first++;return first;}
// One page down starts at the row after the last visible one; one page up ends just above the current first row.
inline int settingsPage(int scroll,bool down){if(down)return std::min(settingsMaxScroll(),settingsLastVisible(scroll)+1);int first=scroll;while(first>0&&settingsLastVisible(first-1)>=scroll-1)first--;return first;}
// Lights / vibration: three label + slider rows, then the message / mention vibration pattern rows.
constexpr int ControlFirstY=Top+10,ControlStep=58;
constexpr int HapticY=ControlFirstY+ControlStep*2;
constexpr Rect lightSliderRect(int row){return {Margin,ControlFirstY+row*ControlStep+18,W-2*Margin,28};}
constexpr Rect HapticSlider={Margin,HapticY+18,W-2*Margin,28};
constexpr int HapticTrackX=Margin+8,HapticTrackW=W-2*Margin-16;
// Ringtone / vibration rows end with "<" (at w-44) and ">" (at w-20): a tap on "<" steps back, anywhere else forward.
inline int stepRowTap(Rect r,int x){return x>=r.x+r.w-56&&x<r.x+r.w-26?-1:1;}
constexpr Rect VibrationMessage={Margin,HapticY+70,W-2*Margin,32},VibrationMention={Margin,HapticY+104,W-2*Margin,32};
constexpr Rect lightToggleRect(int row){return {W-62,ControlFirstY+row*ControlStep-4,54,22};}
constexpr Rect RadioAdvert={Margin,Top+202,W-2*Margin,28};
constexpr Rect BackButton={0,StatusH,32,HeaderH};
constexpr Rect ComposeButton={W-32,StatusH,32,HeaderH};
constexpr Rect MessageMenu={W-32,StatusH,32,HeaderH};
constexpr int MessagesPopupRowH=42,MessagesMenuRows=4;// row 0 new direct chat, rows 1..3 the channel forms
constexpr Rect MessagesPopup={14,Top+2,W-28,MessagesMenuRows*MessagesPopupRowH+2};
// New direct chat: contact picker (search field, 4 rows, footer pager).
constexpr Rect PickerSearch={Margin,Top+4,W-2*Margin,26};
constexpr int PickerRowH=46,PickerRows=4;
inline Rect pickerRowRect(int row){return {Margin,Top+34+row*PickerRowH,W-2*Margin,PickerRowH-4};}
constexpr Rect PickerPrev={W-76,FooterY,34,FooterH},PickerNext={W-38,FooterY,34,FooterH};
constexpr Rect messageListRect(int row,bool searching=false){return {6,Top+4+(searching?30:0)+row*ChatRowH,W-12,ChatRowH-4};}
constexpr Rect WifiToggle=SettingsSwitch,WifiScan={Margin,Top+52,64,28};
constexpr int WifiListY=Top+104,WifiRowH=36,WifiRows=(Bottom-WifiListY)/WifiRowH;
inline Rect wifiRowRect(int index){return {Margin,WifiListY+index*WifiRowH,W-2*Margin,34};}
constexpr Rect WifiManual={Margin+70,Top+52,64,28},WifiDisconnect={Margin+140,Top+52,W-2*Margin-140,28};
constexpr Rect WifiSSIDInput={Margin,Top+24,W-2*Margin,32},WifiEye={W-Margin-32,Top+82,32,32};
constexpr Rect WifiPassword={Margin,Top+82,W-2*Margin,32},WifiConnect={Margin,Top+128,108,30},WifiCancel={124,Top+128,108,30};
constexpr Rect BleToggle=SettingsSwitch;
// Bluetooth page: MeshCore app name / PIN cards, then the app-over-Wi-Fi switch.
constexpr Rect AppNameCard={Margin,SettingsBodyY,(W-2*Margin-6)*7/10,40},AppPinCard={Margin+(W-2*Margin-6)*7/10+6,SettingsBodyY,(W-2*Margin-6)-(W-2*Margin-6)*7/10,40},AppWifiToggle={Margin,SettingsBodyY+46,W-2*Margin,40};
constexpr int WeatherPlaceY=Top+3,WeatherHeroY=Top+22,WeatherIconX=Margin+4,WeatherTempX=86,WeatherHoursY=Top+90,WeatherHoursH=60,WeatherHoursColumns=4,WeatherGridY=Top+154,WeatherCellH=28,WeatherStatusY=Bottom-44;
constexpr Rect WeatherRefresh={Margin,Bottom-30,W-2*Margin,30},WeatherPrompt={Margin,WeatherHoursY,W-2*Margin,120},WeatherWifiButton={Margin+20,WeatherHoursY+76,W-2*Margin-40,34};
inline Rect weatherCell(int i){return {Margin+(i%2)*((W-2*Margin)/2+3),WeatherGridY+(i/2)*(WeatherCellH+4),(W-2*Margin)/2-3,WeatherCellH};}
constexpr int FileListY=Top+20,FileRowH=34,FileRows=(Bottom-FileListY)/FileRowH;
inline Rect fileRowRect(int row){return {Margin,FileListY+row*FileRowH,W-2*Margin,FileRowH-2};}
// Wallpaper: three thumbnail cards (the 240x302 art at 1/5 scale) side by side, focused = filled, active = inner
// frame; the focused wallpaper's name and description sit below the row.
constexpr int WallpaperCardCount=3,WallpaperThumbScale=5,WallpaperThumbW=48,WallpaperThumbH=60;
inline Rect wallpaperCardRect(int index){return {Margin+index*76,Top+20,72,68};}
constexpr int WallpaperInfoY=Top+100;
inline Rect powerRowRect(int index){return {Margin,Top+22+index*47,W-2*Margin,43};}
inline Rect timeoutOptionRect(int index){return {Margin+index*38,Top+138,34,28};}
// Sound page: switch, volume row (- slider +), message / mention ringtone rows, dictation card.
constexpr Rect AudioNotify=SettingsSwitch,AudioMinus={Margin,Top+70,36,28},AudioSlider={Margin+42,Top+70,W-2*Margin-84,28},AudioPlus={W-Margin-36,Top+70,36,28};
constexpr Rect AudioToneMessage={Margin,Top+124,W-2*Margin,36},AudioToneMention={Margin,Top+164,W-2*Margin,36};
constexpr Rect AudioBackend={Margin,Top+226,W-2*Margin,34};
constexpr Rect SaveButton={Margin,FormActionsY,(W-3*Margin)/2,30};
constexpr Rect CancelButton={W/2+Margin/2,FormActionsY,(W-3*Margin)/2,30};
constexpr int ReceivedMessageActions=5;
// Row 1 ("View full path") shares its line with MAP (packet route on the map, action 6).
constexpr int MessageMapW=64,MessageMapAction=6;
constexpr Rect messageMapRect(){return {Margin+8+(W-2*Margin-16)-MessageMapW,Top+40+32,MessageMapW,28};}
constexpr int receivedMessageAction(int row){return row==0?0:row==1?2:row==2?5:row==3?1:3;}
constexpr Rect messageActionRect(int index){return {Margin+8,Top+40+index*32,W-2*Margin-16,28};}
inline Rect chatOptionRect(int index){return {Margin+8,Top+36+index*44,W-2*Margin-16,38};}
constexpr int ChatOptionCount=4;
// Channel options rows: notifications, show key + QR (channels only), delete.
inline Rect channelOptionRect(int slot){return {Margin+8,Top+78+slot*48,W-2*Margin-16,38};}
constexpr Rect ChannelAction={Margin+8,Top+78,W-2*Margin-16,38};
constexpr Rect PositionSend={Margin+8,Top+173,W-2*Margin-16,30};
constexpr Rect PositionCancel={Margin+8,Top+213,W-2*Margin-16,30};
constexpr int ScopeRows=5,ScopeRowH=30,ScopeRowY=Top+42;
inline Rect scopeRowRect(int row){return {Margin+8,ScopeRowY+row*ScopeRowH,W-2*Margin-16,28};}
constexpr Rect ScopeAdd={Margin+8,Top+194,W-2*Margin-16,24};
constexpr Rect ScopeApply={Margin+8,Top+224,101,28},ScopeCancel={W-Margin-109,Top+224,101,28};
constexpr Rect ScopeInput={Margin+8,Top+88,W-2*Margin-16,36};
constexpr Rect ScopeSave={Margin+8,Top+142,101,30},ScopeEditCancel={W-Margin-109,Top+142,101,30};
constexpr Rect ChatClearYes={Margin+8,Top+173,101,30},ChatClearNo={W-Margin-109,Top+173,101,30};
constexpr Rect RadioPowerInput={Margin+8,Top+88,W-2*Margin-16,36};
constexpr Rect RadioPowerSave={Margin+8,Top+142,(W-2*Margin-22)/2,30};
constexpr Rect RadioPowerCancel={Margin+14+(W-2*Margin-22)/2,Top+142,(W-2*Margin-22)/2,30};
constexpr Rect LoRaPanel={Margin,Top,W-2*Margin,Bottom-Top};
constexpr int LoRaInset=Margin+6,LoRaContentW=W-2*LoRaInset;
constexpr Rect LoRaBand868={LoRaInset,Top+18,(LoRaContentW-Gap)/2,24},LoRaBand900={W/2+Gap/2,Top+18,(LoRaContentW-Gap)/2,24};
constexpr Rect LoRaPreset={LoRaInset,Top+48,LoRaContentW,50};
constexpr Rect LoRaPresetPrev={LoRaInset,Top+48,30,50},LoRaPresetNext={W-LoRaInset-30,Top+48,30,50};
inline Rect loraFieldRect(int field){return {LoRaInset,Top+102+field*22,LoRaContentW,21};}
constexpr Rect LoRaApply={LoRaInset,Top+214,(LoRaContentW-Gap)/2,28},LoRaAdvert={W/2+Gap/2,Top+214,(LoRaContentW-Gap)/2,28};
constexpr int LoRaTabW=62,LoRaTabGap=4,LoRaTabY=StatusH+2,LoRaTabH=Top-LoRaTabY+1;
constexpr Rect LoRaRadio={W-Margin-2*LoRaTabW-LoRaTabGap,LoRaTabY,LoRaTabW,LoRaTabH},LoRaDevice={W-Margin-LoRaTabW,LoRaTabY,LoRaTabW,LoRaTabH};
// Contacts (Home tab, section 1): ALL / COMP / RPT tabs opening into the LoRa panel, search field, four rows, pager.
constexpr int ContactTabCount=3,ContactTabGap=2;constexpr int ContactTabWidths[ContactTabCount]={46,62,46};
inline Rect contactTabRect(int i){int x=W-Margin-Radius;for(int t=ContactTabCount-1;t>=i;t--)x-=ContactTabWidths[t]+(t==ContactTabCount-1?0:ContactTabGap);return {x,LoRaTabY,ContactTabWidths[i],LoRaTabH};}
constexpr Rect ContactSearch={LoRaInset,Top+6,LoRaContentW,26};
// Settings > Keyboard languages: search, KeyboardRows language rows with a switch (toggleRow needs 40 px for its two
// lines), help under the list: up to 7 lines, enough for the two-line how-to plus the longest accent help.
constexpr int KeyboardSection=12,KeyboardRows=3,KeyboardRowH=40;
constexpr Rect KeyboardSearch={LoRaInset,Top+6,LoRaContentW,26};
inline Rect keyboardRowRect(int i){return {Margin,Top+38+i*(KeyboardRowH+2),W-2*Margin,KeyboardRowH};}
constexpr int KeyboardHelpY=Top+40+KeyboardRows*(KeyboardRowH+2);
// Settings > Accent key: one row per choice (Off, Right Shift, Left Shift), how it works below.
constexpr int AccentKeySection=13,AccentRowH=38;
inline Rect accentRowRect(int i){return {Margin,Top+6+i*(AccentRowH+2),W-2*Margin,AccentRowH};}
constexpr int AccentInfoY=Top+6+3*(AccentRowH+2)+4;
constexpr int ContactRows=4,ContactRowH=44,ContactRowY=Top+38,ContactNoticeY=Bottom-46;
inline Rect contactRowRect(int i){return {LoRaInset,ContactRowY+i*ContactRowH,LoRaContentW,ContactRowH-3};}
constexpr Rect ContactPrevious={LoRaInset,Bottom-28,40,22},ContactNext={W-LoRaInset-40,Bottom-28,40,22};
constexpr Rect LoRaName={LoRaInset,Top+12,LoRaContentW,56};
constexpr int LoRaBytesY=Top+105;
constexpr int LoRaByteGap=4,LoRaByteW=(LoRaContentW-2*LoRaByteGap)/3;
inline Rect loraBytesRect(int bytes){return {LoRaInset+(bytes-1)*(LoRaByteW+LoRaByteGap),LoRaBytesY,LoRaByteW,34};}
constexpr Rect LoRaDeviceApply={LoRaInset,Top+190,LoRaContentW,32};
constexpr int MenuHomeRows=3,MenuHomeRowY=80,MenuHomeRowH=58,MenuHomeRowGap=6;
constexpr int MenuHomeTabCount=4,MenuHomeTabY=272,MenuHomeTabH=44,MenuHomeTabGap=4;
constexpr int MenuHomeTabW=(W-2*Margin-(MenuHomeTabCount-1)*MenuHomeTabGap)/MenuHomeTabCount,MenuHomeTabIcon=24,MenuHomeTabLabelY=30;
constexpr Rect MenuHomeClock={Margin,22,112,46};
constexpr int MenuHomeInfoX=MenuHomeClock.x+MenuHomeClock.w+Gap,MenuHomeInfoW=W-Margin-MenuHomeInfoX;
constexpr Rect MenuHomeDate={MenuHomeInfoX,MenuHomeClock.y,MenuHomeInfoW,MenuHomeClock.h/2};
constexpr Rect MenuHomeStatus={MenuHomeInfoX,MenuHomeClock.y+MenuHomeClock.h/2,MenuHomeInfoW,MenuHomeClock.h/2};
constexpr Rect menuHomeRowRect(int row){return {6,MenuHomeRowY+row*(MenuHomeRowH+MenuHomeRowGap),W-12,MenuHomeRowH};}
constexpr Rect menuHomeTabRect(int tab){return {Margin+tab*(MenuHomeTabW+MenuHomeTabGap),MenuHomeTabY,MenuHomeTabW,MenuHomeTabH};}
}

namespace mm {
constexpr Rect AntennaInternal={Margin,Top+22,W-2*Margin,44},AntennaExternal={Margin,Top+72,W-2*Margin,44};
constexpr int HeardRows=3,HeardRowH=18,HeardListY=Top+94;
constexpr Rect HeardList={Margin+8,Top+90,W-2*Margin-16,70};
inline Rect ownMessageActionRect(int index){if(index==0)return {Margin+8,Top+36,W-2*Margin-16,30};return {index==2||index==3?124:Margin+8,Top+(index==2||index==4?168:204),100,30};}
// First-boot welcome (portrait): title and hint under the status bar, full-width choice cards or preset rows,
// step dots and BACK / NEXT at the bottom.
constexpr Rect WelcomeBack={Margin,H-38,92,30},WelcomeNext={W-Margin-112,H-38,112,30};
constexpr int WelcomeBodyY=StatusH+66,WelcomeRows=4,WelcomeRowH=40;
inline Rect welcomeCard(int i,int n){int h=n>2?52:64;return {Margin,WelcomeBodyY+i*(h+8),W-2*Margin,h};}
inline Rect welcomeWifiCard(int i){return {Margin,WelcomeBodyY+i*52,W-2*Margin,46};}// connect / skip, then the clock note
inline Rect welcomeRow(int i){return {Margin,WelcomeBodyY+i*WelcomeRowH,W-2*Margin-6,WelcomeRowH-4};}
constexpr Rect WelcomeNameField={Margin,WelcomeBodyY+4,W-2*Margin,34};
// Packet-route picture (chat MAP): map under the header, legend below it.
constexpr Rect MapArea={0,Top,W,Bottom-40-Top};
// Maps app (section 9): the map fills the page under the status bar; a floating back button at the top left,
// round zoom out / center / zoom in buttons stacked at the bottom right (index 0 lowest).
constexpr Rect MapPageArea={0,StatusH,W,H-StatusH};
constexpr Rect MapBackFloat={6,StatusH+6,30,30};
constexpr Rect mapButtonRect(int index){return {W-Margin-32,H-Margin-32-(index==0?0:index==2?38:76),32,32};}
constexpr Rect ConfirmYes={Margin,Top+173,108,30},ConfirmNo={124,Top+173,108,30};
// Quick settings, opened by tapping the status bar: two 0..16 sliders with a
// label line above each track, then a 2x3 tile grid (index = row*2+column).
// Section 18 is unused.
constexpr int QuickSettingsSection=18,QuickSteps=16,QuickTileCount=6;
enum QuickTile {QuickWifi,QuickBluetooth,QuickGps,QuickSound,QuickPowerOff,QuickReserved};
constexpr Rect quickSliderRect(int row){return {Margin,Top+22+row*50,W-2*Margin,26};}
constexpr int quickSliderLabelY(int row){return Top+4+row*50;}
constexpr Rect quickTileRect(int index){return {Margin+(index%2)*((W-2*Margin-Gap)/2+Gap),Top+108+(index/2)*52,(W-2*Margin-Gap)/2,46};}
// Same inner track as Components::slider: step 0..QuickSteps under x.
inline int quickStepAt(Rect r,int x){int track=r.w-16,at=x-(r.x+8);at=at<0?0:at>track?track:at;return (at*QuickSteps+track/2)/track;}
constexpr Rect QuickConfirmBox={Margin+4,Top+46,W-2*Margin-8,138};
constexpr Rect QuickConfirmCancel={QuickConfirmBox.x+8,QuickConfirmBox.y+QuickConfirmBox.h-40,(QuickConfirmBox.w-24)/2,32};
constexpr Rect QuickConfirmPowerOff={QuickConfirmCancel.x+QuickConfirmCancel.w+8,QuickConfirmCancel.y,QuickConfirmCancel.w,32};
}
