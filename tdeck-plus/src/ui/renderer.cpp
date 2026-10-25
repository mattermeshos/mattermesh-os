#include "renderer.h"
#include <unordered_map>
#include "services/link_html.h"
#include "composer_caret.h"
#include "utf8.h"
#include "services/keyboard_layout.h"
#include "mesh/radio_parameters.h"
#include "mesh/radio_presets.h"
#include "emoji_choices.h"
#include <cctype>
#include "navigation_catalog.h"
#include "services/channel_qr.h"
#include "cli_layout.h"
#include "fast_canvas.h"
#include "mesh/rx_log.h"
#include "lock_photo.h"
#include "lock_photo_night.h"
#include "lock_photo_expedition.h"
#include "packed.h"
#include "services/channel_form.h"
#include "os_logo_bitmap.h"
#include <cmath>
#include <algorithm>
namespace mm {

std::string accentBarText(const State &s){return mm_kbd::accentBar(mm_kbd::Lang(s.kbdLang),s.accentTaps,"  ");}// "": no bar (English)
std::string keyboardHowto(const State &s){return mm_kbd::enabledCount(s.kbdMask)>1?"Alt + Space: next language (now "+std::string(mm_kbd::info(mm_kbd::Lang(s.kbdLang)).code)+")":std::string("Turn on two languages for Alt + Space");}
std::string keyboardHelp(const State &s,int lang){std::string help=mm_kbd::accentHelp(mm_kbd::Lang(lang));if(help.empty())return help;
 return (s.accentMode==1?"Right Shift: accent. ":s.accentMode==2?"Left Shift: accent. ":"Accent key off (Settings > Accent key). ")+help;}
std::string upperAscii(std::string text){for(auto &ch:text)if(ch>='a'&&ch<='z')ch-=32;return text;}
// "42s", "5m", "3h": time since a node was heard.
static std::string agoText(uint32_t sec){return sec<60?std::to_string(sec)+"s":sec<3600?std::to_string(sec/60)+"m":std::to_string(sec/3600)+"h";}
// Contact row detail: signal, age and route of a node heard since boot.
template<class N> static std::string contactLine(const N &n){if(!n.live)return "Saved / not heard since boot";std::string route=n.hops==0?"direct":n.hops>0?std::to_string(n.hops)+(n.hops==1?" hop":" hops"):"via mesh";char text[48];snprintf(text,sizeof(text),"%.0f dBm / %s ago / %s",n.rssi,agoText(n.age).c_str(),route.c_str());return text;}
// Home: conversations most recent first, as pointers (stable: ties keep list order) instead of a sorted copy per frame.
struct ConversationOrder{std::vector<const Conversation*> p;const Conversation &operator[](size_t i)const{return *p[i];}size_t size()const{return p.size();}bool empty()const{return p.empty();}};
static ConversationOrder byActivity(const std::vector<Conversation> &list){ConversationOrder o;o.p.reserve(list.size());for(auto &c:list)o.p.push_back(&c);std::stable_sort(o.p.begin(),o.p.end(),[](const Conversation *a,const Conversation *b){return a->activity>b->activity;});return o;}
// Full-height dialog card under the header; clear: wipe the page below the header first.
Rect Renderer::dialogPanel(bool clear){if(clear)g.fillRect(0,Top,W,H-Top,BG);Rect panel={Margin,Top+4,W-2*Margin,Bottom-Top-8};c.card(panel);return panel;}
void Renderer::messageBanner(const std::string &sender,const std::string &preview){
 auto single=[](std::string text){for(char &ch:text)if(static_cast<unsigned char>(ch)<32)ch=' ';return text;};
 auto name=single(senderDisplayName(sender)),body=single(preview);const auto r=MessageBannerArea;int available=W-2*Margin;
 auto ellipsis=[&](const std::string &text,Font f){if(c.width(text,f)<=available)return text;std::string head;uint32_t cp;for(size_t at=0;at<text.size();){size_t next=utf8Next(text,at,cp);auto unit=text.substr(at,next-at);if(c.width(head+unit+"...",f)>available)break;head+=unit;at=next;}return head+"...";};
 g.fillRect(r.x,r.y,r.w,r.h,BG);g.drawFastHLine(0,r.y+r.h-1,W,FG);
 // Two lines in the header slot: sender, then the message preview.
 int top=r.y+(r.h-1-c.lineHeight(Font::Bold)-c.lineHeight(Font::ChatMeta))/2;
 c.text(Margin,top,ellipsis(name,Font::Bold),Font::Bold,FG,available);
 c.text(Margin,top+c.lineHeight(Font::Bold),ellipsis(body,Font::ChatMeta),Font::ChatMeta,FG,available);
}
void Renderer::shell(const State &s,const std::string &title,bool back,Icon right){g.fillScreen(BG);c.statusBar(s.status);c.pageHeader(title,back,right);}
// Theme illustrations (raw deflate, ui/packed.h): the lock draws the active one, the Themes page 80x60 thumbnails
// (every 4th pixel of every 4th row) kept in PSRAM, 9.6 KB each.
static const uint16_t *lockPhoto(int theme){
 static const struct {const uint8_t *data;size_t size;} photos[ThemeCount]={{mm_lock_photo::packed,sizeof(mm_lock_photo::packed)},
  {mm_lock_night::packed,sizeof(mm_lock_night::packed)},{mm_lock_expedition::packed,sizeof(mm_lock_expedition::packed)}};
 return unpackPhoto(photos[theme].data,photos[theme].size,mm_lock_photo::width,mm_lock_photo::height);}
static const uint16_t *lockThumb(int theme){
 static uint16_t *thumbs[ThemeCount]={};
 if(!thumbs[theme]){const uint16_t *photo=lockPhoto(theme);if(!photo)return nullptr;
  auto *thumb=static_cast<uint16_t*>(heap_caps_malloc(80*60*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!thumb)return nullptr;
  for(int y=0;y<60;y++)for(int x=0;x<80;x++)thumb[y*80+x]=photo[(y*4)*mm_lock_photo::width+x*4];thumbs[theme]=thumb;}
 return thumbs[theme];}
// Settings row detail: the enabled languages (Alt + Space switches) or the one typing.
static std::string keyboardDetail(const State &s){
 if(mm_kbd::enabledCount(s.kbdMask)<2)return "Typing: "+std::string(mm_kbd::info(mm_kbd::Lang(s.kbdLang)).name);
 std::string codes;for(auto l:mm_kbd::Order)if(s.kbdMask&(1u<<int(l))){if(!codes.empty())codes+=" ";codes+=mm_kbd::info(l).code;}return codes+" (Alt + Space)";}
void Renderer::lock(const State &s){
 // T-Deck Plus lock: the theme's Matterhorn illustration, clock placed by the theme, notification card, unlock band.
 if(const uint16_t *photo=lockPhoto(themeIndex()))c.image(0,0,photo,mm_lock_photo::width,mm_lock_photo::height);// row copies on the colour frame
 else g.fillScreen(BG);
 c.statusBar(s.status);
 {auto layout=theme(themeIndex()).lockLayout;
  // Quiet typography over the picture: small brand line, Clock face, meta-size date.
  auto logo=[&](int x,int y,int w){g.drawBitmap(x+(w-mm_logo::size32)/2,y,mm_logo::logo32,mm_logo::size32,mm_logo::size32,LockInk);c.centered(x,y+35,w,"MATTERMESH OS",Font::MenuLabel,LockInk);};
  if(layout==LockLayout::RightColumn){int bx=W/2+20,bw=W-Margin-bx;logo(bx,StatusH+8,bw);
   c.centered(bx,StatusH+56,bw,s.status.clock,Font::Clock,LockInk);c.centered(bx,StatusH+94,bw,s.date,Font::ChatMeta,LockInk);}
  else if(layout==LockLayout::TopLeft){c.text(Margin+4,StatusH+8,s.status.clock,Font::Clock,LockInk);c.text(Margin+6,StatusH+46,s.date,Font::ChatMeta,LockInk,W/2);logo(W-Margin-110,StatusH+10,110);}
  else{c.text(Margin+4,LockMessageY-56,s.status.clock,Font::Clock,LockInk);c.text(Margin+6,LockMessageY-18,s.date,Font::ChatMeta,LockInk,W/2);}
  const auto r=LockMessage;c.fillRound(r.x,r.y,r.w,r.h,10,BG);
  if(!s.lockHasMessage){c.icon(Icon::Messages,r.x+14,r.y+(r.h-24)/2,24);c.text(r.x+50,r.y+(r.h-c.lineHeight(Font::ChatBody))/2,"No new messages",Font::ChatBody,FG,r.w-60);}
  else{Rect icon={r.x+8,r.y+(r.h-38)/2,38,38};c.fillRound(icon.x,icon.y,icon.w,icon.h,9,Mist);c.packIcon(PackIcon::Chat,icon.x+7,icon.y+7,24,FG);
   int tx=icon.x+icon.w+10,timeW=c.width(s.receivedTime,Font::ChatMeta);
   c.text(tx,r.y+9,senderDisplayName(s.receivedName),Font::Bold,FG,r.x+r.w-tx-timeW-14);c.text(r.x+r.w-10-timeW,r.y+10,s.receivedTime,Font::ChatMeta,Muted);
   c.text(tx,r.y+31,s.receivedPreview,Font::ChatBody,FG,r.x+r.w-tx-(s.unread?32:10));
   if(s.unread){int bx2=r.x+r.w-17,by=r.y+r.h-16;c.badge(bx2,by,9,s.unread,Accent,OnAccent);}}
  bool darkBand=layout==LockLayout::RightColumn;// Alpine: night-blue band; the other themes use their status-bar colours
  g.fillRect(0,H-24,W,24,darkBand?FG:StatusBg);
  c.centered(Margin,H-18,W-2*Margin,!s.lockRepeater.empty()?s.lockRepeater:std::string("Trackball x2: unlock  /  ENTER: quick reply"),Font::ChatMeta,darkBand?BG:StatusFg);
 }
 if(!s.quickReply&&s.quickCountdown>=0)quickCountdownBanner(s.quickCountdown);
 else if(!s.quickReply&&(s.lockHint||s.unlockTapCount||s.unlockCounting))unlockTapBanner(s.unlockTapCount);
 if(s.quickReply)quickReply(s);
}
void Renderer::quickReply(const State &s){
 auto panel=QuickReplyPanel;
 // Plus: the lock picture stays visible behind a translucent veil (no dark band), the panel slightly see-through.
 {auto &canvas=static_cast<FastCanvas16&>(g);canvas.tint(0,StatusH,W,H-StatusH,BG,9);canvas.tint(panel.x,panel.y,panel.w,panel.h,Surface,14);g.drawRoundRect(panel.x,panel.y,panel.w,panel.h,Radius,Line);}
 c.text(panel.x+8,panel.y+QuickTitleDY,"Quick Reply",Font::ChatTitle,FG,panel.w-16);if(s.accent&&!s.quickSent){int x=panel.x+8+c.width("Quick Reply  ",Font::ChatTitle);c.text(x,panel.y+QuickTitleDY+3,accentBarText(s),Font::ChatMeta,FG,panel.x+panel.w-60-x);}
 if(!s.quickSent&&s.quickInput.size()>MessageCounterThreshold){auto count=std::to_string(s.quickInput.size())+"/"+std::to_string(s.quickMessageLimit);int width=c.width(count,Font::ChatMeta);c.text(panel.x+panel.w-8-width,panel.y+QuickTitleDY+3,count,Font::ChatMeta);}
 c.text(panel.x+8,panel.y+QuickChannelDY,s.quickChannel,Font::ChatMeta,FG,panel.w-16);
 c.text(panel.x+8,panel.y+QuickSenderDY,senderDisplayName(s.quickSender),Font::ChatBody,FG,panel.w-16);
 auto preview=c.lines(s.quickPreview,panel.w-16,Font::ChatMeta);for(int i=0;i<std::min(QuickPreviewLines,int(preview.size()));i++)c.text(panel.x+8,panel.y+QuickPreviewDY+i*13,preview[i],Font::ChatMeta,FG,panel.w-16);
 if(s.quickSent){Rect confirmation={QuickReplyComposer.x,QuickReplyComposer.y+(QuickReplyComposer.h-24)/2,QuickReplyComposer.w,24};c.card(confirmation);c.centered(confirmation.x+7,confirmation.y+(confirmation.h-c.lineHeight(Font::ChatMeta))/2,confirmation.w-14,"Message sent",Font::ChatMeta);return;}
 c.card(QuickReplyComposer);auto rows=c.lines(s.quickInput,QuickReplyComposer.w-14,Font::ChatBody);int trailing=0;for(auto i=s.quickInput.rbegin();i!=s.quickInput.rend()&&*i==' ';++i)trailing++;int tailWidth=trailing*c.width(" ",Font::ChatBody);if(c.width(rows.back(),Font::ChatBody)+tailWidth>QuickReplyComposer.w-14){rows.push_back("");tailWidth=0;}int visible=(QuickReplyComposer.h-12)/c.lineHeight(Font::ChatBody),first=std::max(0,int(rows.size())-visible);
 for(int i=first;i<int(rows.size());i++)c.text(QuickReplyComposer.x+7,QuickReplyComposer.y+6+(i-first)*c.lineHeight(Font::ChatBody),rows[i],Font::ChatBody,FG,QuickReplyComposer.w-14);
 if(s.quickReplyEditable){int y=QuickReplyComposer.y+6+(int(rows.size())-first-1)*c.lineHeight(Font::ChatBody);g.drawFastVLine(QuickReplyComposer.x+7+c.width(rows.back(),Font::ChatBody)+tailWidth,y,12,FG);}
 if(s.quickError.empty()){c.text(panel.x+8,QuickReplySend.y,"Enter: send",Font::ChatMeta,FG,QuickReplyEmoji.x-panel.x-12);c.text(panel.x+8,QuickReplySend.y+14,"Back: cancel",Font::ChatMeta,FG,QuickReplyEmoji.x-panel.x-12);}
 else c.text(panel.x+8,(QuickReplySend.y+8),s.quickError.empty()?"Enter: send / Back: cancel":s.quickError,Font::ChatMeta,FG,(QuickReplySend.x-panel.x-14));
 c.button(QuickReplySend,"SEND",true);c.button(QuickReplyCancel,s.quickPending?"CLOSE":"CANCEL");
 if(s.quickReplyEditable){Rect e=QuickReplyEmoji;constexpr int B=mm_emoji_picker::ButtonSize;c.card(e,s.quickEmoji,6);g.drawBitmap(e.x+(e.w-B)/2,e.y+(e.h-B)/2,mm_emoji_picker::button,B,B,s.quickEmoji?OnAccent:FG);}
 if(s.quickEmoji)emojiPopup(s);
}
void Renderer::home(const State &s){
 g.fillScreen(BG);c.statusBar(s.status);
 {// T-Deck Plus "Alpine" Home: compact clock + weather header, a soft ridge, two message cards, four commands.
  // Clock block (left half): big clock, date under it; the right half is centred on the divider's box.
  const int top=StatusH+6;c.text(Margin,top,s.status.clock,Font::MenuClock);
  c.text(Margin+2,top+34,s.status.acquiringTime?"Acquiring time":s.menuDate,Font::ChatMeta,Muted,140);
  int split=W/2+6;g.drawFastVLine(split-10,top+2,40,Line);const int boxX=split-2,boxW=W-Margin-boxX;
  bool offline=s.weatherPhase==WeatherPhaseView::WifiOff||s.weatherPhase==WeatherPhaseView::NoNetwork;
  if(s.weatherValid&&!offline){// weather header: refreshed every 15 min while Home is shown
   std::string temperature=std::to_string(int(std::lround(s.temperature))),place=s.weatherPlace.empty()?std::to_string(s.nodeCount)+" contacts":s.weatherPlace;
   int tw=c.width(temperature,Font::ChatTitle),lineW=std::max(tw+16,c.width(c.fit(place,boxW-38,Font::ChatMeta),Font::ChatMeta)),total=30+8+lineW,x=boxX+std::max(0,(boxW-total)/2);
   c.weatherIcon(s.weatherCode,x,top+4,30);int tx=x+38;
   c.text(tx,top+2,temperature,Font::ChatTitle);g.drawCircle(tx+tw+4,top+5,2,FG);c.text(tx+tw+8,top+2,"C",Font::ChatMeta);
   c.text(tx,top+23,place,Font::ChatMeta,Muted,boxW-(tx-boxX));
  }else{// no Wi-Fi: the MatterMesh mark with the contact count and one rotating fact
   std::string count=std::to_string(s.nodeCount)+" contacts",fact=s.homeFact.empty()?"MatterMesh OS":s.homeFact;
   int lineW=std::max(c.width(count,Font::ChatTitle),c.width(c.fit(fact,boxW-40,Font::ChatMeta),Font::ChatMeta)),total=32+8+lineW,x=boxX+std::max(0,(boxW-total)/2);
   c.fillRound(x,top+3,32,32,8,FG);g.drawBitmap(x,top+3,mm_logo::logo32,mm_logo::size32,mm_logo::size32,BG);
   c.text(x+40,top+2,count,Font::ChatTitle,FG,boxW-(x+40-boxX));
   c.text(x+40,top+23,fact,Font::ChatMeta,Muted,boxW-(x+40-boxX));
  }
  // Ridge: two overlapping silhouettes in the mist colour under the header.
  const int base=MenuHomeRowY-3;const int ridge[][2]={{150,0},{176,7},{214,3},{244,8},{276,2},{W,6}};// right of the date only
  for(unsigned i=0;i+1<sizeof(ridge)/sizeof(ridge[0]);i++){int x0=ridge[i][0],x1=ridge[i+1][0],y0=base-ridge[i][1],y1=base-ridge[i+1][1];g.fillTriangle(x0,base,x1,base,x0,y0,Mist);g.fillTriangle(x1,base,x0,y0,x1,y1,Mist);}
  auto conversations=byActivity(s.conversations);
  int first=std::max(0,std::min(s.scroll,std::max(0,int(conversations.size())-MenuHomeRows)));
  for(int row=0;row<MenuHomeRows&&first+row<int(conversations.size());row++){
   auto &item=conversations[first+row];auto r=menuHomeRowRect(row);c.fillRound(r.x,r.y,r.w,r.h,10,item.direct?SurfaceAlt:Surface);
   if(item.direct)c.fillRound(r.x,r.y+10,4,r.h-20,2,Accent);// direct chats: a thin accent bar instead of a badge
   int textX=r.x+14,textW=r.x+r.w-textX-40;auto age=item.age=="--"?item.time:item.age;
   c.text(textX,r.y+9,item.name,Font::ChatTitle,FG,textW);c.text(r.x+r.w-8-c.width(age,Font::ChatMeta),r.y+11,age,Font::ChatMeta,Muted);
   std::string preview=item.sender.empty()?item.preview:senderDisplayName(item.sender)+": "+item.preview;
   c.text(textX,r.y+33,preview,Font::ChatBody,FG,r.x+r.w-textX-(item.unread?32:10));
   if(item.unread){int bx=r.x+r.w-17,by=r.y+r.h-17;c.badge(bx,by,9,item.unread,item.direct?FG:Accent,item.direct?BG:OnAccent);}
  }
  if(int(conversations.size())>MenuHomeRows){int height=menuHomeRowRect(MenuHomeRows-1).y+MenuHomeRowH-MenuHomeRowY,thumb=std::max(8,height*MenuHomeRows/int(conversations.size()));c.fillRound(W-4,MenuHomeRowY+first*(height-thumb)/(int(conversations.size())-MenuHomeRows),3,thumb,1,Muted);}
  if(conversations.empty())c.centered(6,MenuHomeRowY+40,W-12,"No conversations",Font::ChatBody,Muted);
  for(int tab=0;tab<MenuHomeTabCount;tab++){auto r=menuHomeTabRect(tab);bool selected=(s.focus<0?0:s.focus)==tab;auto ink=selected?OnAccent:FG;
   if(selected)c.fillRound(r.x,r.y,r.w,r.h,9,Accent);else{c.fillRound(r.x,r.y,r.w,r.h,9,BG);g.drawRoundRect(r.x,r.y,r.w,r.h,9,Line);}
   c.packIcon(HomeTabs[tab].icon,r.x+(r.w-24)/2,r.y+1,24,ink);c.centered(r.x+1,r.y+r.h-14,r.w-2,HomeTabs[tab].label,Font::NavLabel,ink);
   if(tab==0&&s.unread){int cx=r.x+r.w-10,cy=r.y+10;c.badge(cx,cy,7,s.unread,selected?OnAccent:Accent,selected?Accent:OnAccent);}
  }
  return;
 }}
void Renderer::launcher(const State &s){
 shell(s,"App",true);g.fillRect(ComposeButton.x,StatusH,ComposeButton.w,HeaderH-1,BG);
 for(int i=0;i<ApplicationCount;i++){auto r=appRect(i);bool focused=s.focus==i;auto ink=focused?BG:FG;c.card(r,focused,5);auto label=c.lines(Applications[i].label,r.w-6,Font::ChatMeta);if(label.size()>2)label.resize(2);int icon=32,top=((r.h-(icon+6+int(label.size())*13))/2),labelY=(icon+6);c.packIcon(Applications[i].icon,r.x+(r.w-icon)/2,r.y+top,icon,ink);for(unsigned l=0;l<label.size();l++)c.centered(r.x+2,r.y+top+labelY+l*(13),r.w-4,label[l],Font::ChatMeta,ink);}

}
// Thin track + 3 px thumb at the right edge (same as Home and Settings): first..first+visible of total.
static void scrollBar(Adafruit_GFX &g,int y,int h,int first,int visible,int total){
 if(total<=visible||h<=0)return;int thumb=std::max(12,h*visible/total),max=total-visible;first=std::max(0,std::min(first,max));
 g.drawFastVLine(W-3,y,h,FG);g.fillRect(W-4,y+first*(h-thumb)/max,3,thumb,FG);}
// Plus avatar colours: muted hues that carry white text on every theme; chosen from the chat name.
static uint16_t avatarColor(const std::string &name){static const uint16_t hues[]={rgb(0x2e,0x7d,0x6b),rgb(0x3b,0x6e,0xa8),rgb(0x8a,0x4f,0x9e),rgb(0xb0,0x5a,0x3c),rgb(0x4f,0x7f,0x2f),rgb(0x9a,0x3f,0x5c),rgb(0x2f,0x6f,0x8f),rgb(0x7a,0x63,0x2a)};uint32_t h=2166136261u;for(unsigned char ch:name)h=(h^ch)*16777619u;return hues[h%8];}
static uint16_t avatarInk(){return rgb(255,255,255);}
static std::string avatarLetter(const std::string &name){for(size_t i=0;i<name.size();i++){unsigned char ch=name[i];if(ch=='#')return "#";if(std::isalnum(ch))return std::string(1,char(std::toupper(ch)));}return "?";}
void Renderer::messages(const State &s){
 {
  shell(s,"",true,Icon::Menu);c.text(34,StatusH+7,"Messages",Font::ChatTitle,FG,W-150);
  int unread=0;for(auto &v:s.conversations)unread+=v.unread;
  {std::string info=unread?std::to_string(unread)+" unread":std::to_string(s.conversations.size())+(s.conversations.size()==1?" chat":" chats");int w=c.width(info,Font::ChatMeta)+14,x=MessageMenu.x-4-w;c.fillRound(x,StatusH+8,w,17,8,unread?Accent:Surface);c.text(x+7,StatusH+10,info,Font::ChatMeta,unread?OnAccent:Muted);}
  c.iconButton(MessageMenu,Icon::Menu,s.messageMenu||s.focus<0);
  int rows=ListRows;int focused=0;for(unsigned i=0;i<s.conversations.size();i++)if(s.conversations[i].id==s.focus)focused=i;
  int start=s.listStart>=0?s.listStart:(focused/rows)*rows;
  {Rect top=messageListRect(0),last=messageListRect(rows-1);scrollBar(g,top.y,last.y+last.h-top.y,start,rows,s.conversations.size());}
  for(int row=0;row<rows&&start+row<int(s.conversations.size());row++){
   const auto &v=s.conversations[start+row];Rect r=messageListRect(row);bool selected=!s.hideFocus&&v.id==s.focus;c.card(r,selected,8);
   auto ink=selected?OnAccent:FG,soft=selected?OnAccent:(v.unread?FG:Muted);
   int cx=r.x+20,cy=r.y+r.h/2;g.fillCircle(cx,cy,13,avatarColor(v.name));c.centered(cx-13,cy-7,26,avatarLetter(v.name),Font::Bold,avatarInk());
   if(v.direct){g.fillCircle(cx+10,cy+10,4,selected?Accent:Surface);g.fillCircle(cx+10,cy+10,2,selected?OnAccent:Accent);}// direct chat dot
   int textX=r.x+40,right=r.x+r.w-8;auto time=v.age=="--"?v.time:v.age;int tw=c.width(time,Font::ChatMeta);
   c.text(right-tw,r.y+4,time,Font::ChatMeta,selected?OnAccent:(v.unread?Accent:Muted));
   c.text(textX,r.y+2,v.name,Font::Title,ink,right-tw-6-textX);
   std::string preview=v.sender.empty()?v.preview:senderDisplayName(v.sender)+": "+v.preview;int badgeW=0;
   if(v.unread){std::string n=v.unread>99?"99+":std::to_string(v.unread);badgeW=std::max(18,c.width(n,Font::MenuLabel)+10);int bx=right-badgeW+2,by=r.y+r.h-17;c.fillRound(bx,by,badgeW,14,7,selected?OnAccent:Accent);c.centered(bx,by+2,badgeW,n,Font::MenuLabel,selected?Accent:OnAccent);}
   c.text(textX,r.y+20,c.fit(preview,right-textX-(badgeW?badgeW+6:0),Font::ChatMeta),Font::ChatMeta,soft);
  }
  if(s.conversations.empty()){int mid=(Top+ChatsSearch.y)/2;c.centered(Margin,mid-14,W-2*Margin,s.searching&&!s.input.empty()?"No chat matches":"No conversations yet",Font::ChatBody,FG);c.centered(Margin,mid+4,W-2*Margin,s.searching?"Try another name":"Tap New to start one",Font::ChatMeta,Muted);}
  // Bottom bar: search field (tap or '/') and New (direct chat, private or hashtag channel).
  {Rect f=ChatsSearch;c.fillRound(f.x,f.y,f.w,f.h,13,Surface);if(s.searching)g.drawRoundRect(f.x,f.y,f.w,f.h,13,Accent);c.icon(Icon::Search,f.x+8,f.y+5,16,s.searching?Accent:Muted);
   int tx=f.x+30,tw=f.w-40;if(s.searching&&!s.input.empty()){auto tail=c.inputTail(s.input,tw-4,Font::ChatBody);c.text(tx,f.y+5,tail,Font::ChatBody,FG);g.fillRect(tx+c.width(tail,Font::ChatBody)+1,f.y+5,2,16,Accent);}
   else{c.text(tx,f.y+5,s.searching?"Type a name...":"Search chats",Font::ChatBody,Muted,tw);if(s.searching)g.fillRect(tx-2,f.y+5,2,16,Accent);}}
  {Rect n=ChatsNew;c.fillRound(n.x,n.y,n.w,n.h,13,Accent);int w=16+4+c.width("New",Font::Bold),x=n.x+(n.w-w)/2;c.icon(Icon::Plus,x,n.y+5,16,OnAccent);c.text(x+20,n.y+7,"New",Font::Bold,OnAccent);}
 }
 if(s.messageMenu){g.fillRect(MessagesPopup.x,MessagesPopup.y,MessagesPopup.w,MessagesPopup.h,BG);c.card(MessagesPopup);
  const char *titles[]={"New direct chat","Create private channel","Join private channel","Join hashtag channel"},*details[]={"Message a saved contact","Random secret key to share","Name + 32-char secret key","Public, key from the #name"};
  for(int i=0;i<MessagesMenuRows;i++){Rect r={MessagesPopup.x+3,MessagesPopup.y+2+i*MessagesPopupRowH,MessagesPopup.w-6,MessagesPopupRowH-2};bool focused=s.messageMenuFocus==i;auto ink=focused?(OnAccent):FG;if(focused)c.fillRound(r.x,r.y,r.w,r.h,Radius,Accent);
   if(i==3)c.centered(r.x+2,r.y+9,22,"#",Font::ChatTitle,ink);else c.icon(i==0?Icon::Contacts:i==1?Icon::Plus:Icon::Lock,r.x+5,r.y+12,16,ink);
   c.text(r.x+28,r.y+6,titles[i],Font::ChatBody,ink,r.w-32);c.text(r.x+28,r.y+23,details[i],Font::ChatMeta,ink,r.w-32);
   if(i<MessagesMenuRows-1&&!focused&&s.messageMenuFocus!=i+1)g.drawFastHLine(r.x+26,r.y+r.h+1,r.w-30,FG);}}
}
void Renderer::chat(const State &s){
 bubbleHits.clear();senderHits.clear();linkHits.clear();chatShiftOut=0;chatPixelOut=s.chatPixel;// kept while the history loads
 const int bodyH=c.lineHeight(Font::ChatText),metaH=c.lineHeight(Font::ChatMeta);
 // The header is painted after the messages: on the Plus a bubble scrolled half out of view slides under it.
 auto header=[&]{g.fillRect(0,0,W,Top+ChatSubtitleH,BG);c.statusBar(s.status);c.pageHeader(s.chatName,true,Icon::Menu);g.fillRect(32,StatusH,W-64,HeaderH-1,BG);
  c.text(34,StatusH+7,s.chatName,Font::ChatTitle,FG,W-70);
  g.fillRect(ChatMenu.x,ChatMenu.y,ChatMenu.w,ChatMenu.h-1,BG);for(int i=0;i<3;i++)g.fillCircle(ChatMenu.x+8+i*8,ChatMenu.y+16,2,FG);};
 g.fillScreen(BG);
 bool showCounter=s.input.size()>MessageCounterThreshold;
 // Draft (typed or dictated): the composer grows upward (up to ComposerMaxLines wrapped rows) so the
 // whole text, mention included, stays readable; the caret marks where the next character lands and the
 // visible rows follow it when the draft is longer than the card.
 const int inputX=Margin+7,inputWidth=ComposerEmoji.x-inputX-5;std::vector<std::string> inputRows;CaretPlace caret;int caretRowShown=0;
 const size_t caretByte=std::min(s.inputCursor,s.input.size());const size_t caretOffset=Components::display(s.input.substr(0,caretByte)).size();
 if(s.composerExpanded&&!s.input.empty()&&!s.emojiPicker){inputRows=c.lines(s.input,inputWidth-3,Font::ChatText);caret=caretPlace(inputRows,Components::display(s.input),caretOffset);
  int start=caretWindowStart(int(inputRows.size()),ComposerMaxLines,caret.row);caretRowShown=caret.row-start;
  if(start>0)inputRows.erase(inputRows.begin(),inputRows.begin()+start);if(int(inputRows.size())>ComposerMaxLines)inputRows.resize(ComposerMaxLines);}
 const int composerH=ComposerH+(inputRows.empty()?0:(int(inputRows.size())-1)*bodyH),composerTop=H-composerH;chatComposerTop=composerTop;
 const int counterY=composerTop-FirstRepeaterH;
 int startY=Top+ChatSubtitleH;int messageBottom=composerTop-(!s.firstRepeater.empty()||showCounter?FirstRepeaterH:0);
 // Compact chat text (Plus) breathes a little more inside its bubble; the Max keeps BubblePadding.
 const bool compactChat=!chatTextLarge();const int padX=compactChat?8:BubblePadding;const int padY=compactChat?6:BubblePadding;
 // Plus: emoji in bubbles are 20 px on a 22 px row (16 px would be a blur on this panel); a message of 1..3 emoji alone is 30 px.
 auto emojiPx=[&](const Message &m){return ((c.emojiOnlyCount(m.body,Font::ChatText)?30:20));};
 auto bodyLines=[&](const Message &m){c.inlineEmoji(emojiPx(m));auto out=c.lines(m.body,BubbleMaxW-2*padX,Font::ChatText);c.inlineEmoji(0);return out;};
 auto lineH=[&](const std::string &line,int px){return px&&c.hasEmoji(line,Font::ChatText)?px+2:bodyH;};
 // Sender row: a name with emoji gets 16 px emoji (Plus) on an 18 px row, so they neither shrink to a blur nor overlap the text.
 const int SenderEmojiPx=16;auto senderRowH=[&](const Message &m){return c.hasEmoji(senderDisplayName(m.sender),Font::ChatMeta)?SenderEmojiPx+2:metaH;};
 auto linesH=[&](const std::vector<std::string> &lines,int px){int h=0;for(auto &line:lines)h+=lineH(line,px);return h;};
 // Wrapping text (emoji measured glyph by glyph) is the costly part of a chat frame: bodies (with their widest line)
 // and metadata rows are kept across frames, so a scrolling chat only wraps what it has not seen yet.
 struct Wrapped{std::vector<std::string> lines;int widest;};static std::unordered_map<uint64_t,Wrapped> wrapped;static std::unordered_map<std::string,std::vector<std::string>> metaWrapped;
 int latestOwn=s.latestOwnMessage;
 if(latestOwn==-2){latestOwn=-1;for(int i=int(s.messages.size())-1;i>=0;i--)if(s.messages[i].mine){latestOwn=i;break;}}
 auto metadata=[&](const Message &m){if(m.mine){if(latestOwn<0||latestOwn>=int(s.messages.size())||&m!=&s.messages[latestOwn])return std::vector<std::string>{};return std::vector<std::string>{m.repeaters<0?"Heard x--":"Heard x"+std::to_string(m.repeaters)};}std::string meta=m.time;if(!m.meta.empty())meta+=" | "+m.meta;if(!s.scope.empty()&&m.meta.find('|')==std::string::npos&&m.meta.find(s.scope)==std::string::npos)meta+=" | "+s.scope;
  std::string key=(chatTextLarge()?"L":"S")+meta;auto it=metaWrapped.find(key);if(it==metaWrapped.end()){if(metaWrapped.size()>512)metaWrapped.clear();it=metaWrapped.emplace(key,c.lines(meta,W-2*BubbleMargin,Font::ChatMeta)).first;}return it->second;};
 // Each message is laid out once per frame: the fit pass (height) and the draw pass share the wrapped lines.
 struct Lay{bool ok=false;int px=0,widest=-1;std::vector<std::string> lines,meta;};std::vector<Lay> lays(s.messages.size());
 // Body cache key: token, body and text size.
 auto wrapKey=[&](const Message &m){uint64_t h=1469598103934665603ull;for(unsigned char ch:m.body){h^=ch;h*=1099511628211ull;}h^=uint64_t(m.token)<<1;h^=uint64_t(chatTextLarge())<<63;h^=uint64_t(m.mine)<<62;return h;};
 auto layout=[&](int i)->const Lay&{auto &L=lays[i];if(!L.ok){const auto &m=s.messages[i];L.px=emojiPx(m);
   uint64_t key=wrapKey(m);auto it=wrapped.find(key);if(it==wrapped.end()){if(wrapped.size()>512)wrapped.clear();
     Wrapped w{bodyLines(m),0};c.inlineEmoji(L.px);for(auto &line:w.lines)w.widest=std::max(w.widest,c.width(line,Font::ChatText));c.inlineEmoji(0);it=wrapped.emplace(key,std::move(w)).first;}
   L.lines=it->second.lines;L.widest=it->second.widest;L.meta=metadata(m);L.ok=true;}return L;};
 auto height=[&](int i){const auto &m=s.messages[i];const auto &L=layout(i);return linesH(L.lines,L.px)+2*padY+(!s.chatDirect&&!m.mine?senderRowH(m):0)+int(L.meta.size())*metaH+2+BubbleGap;};
 int end=std::max(0,int(s.messages.size())-s.chatOffset),first=end,total=0;
 // Draws message i with its top at y (lines: the wrapped body, possibly cut); returns the y below its block.
 auto draw=[&](int i,int y,std::vector<std::string> lines)->int{
  const auto &m=s.messages[i];const auto &L=layout(i);const auto &meta=L.meta;int senderH=!s.chatDirect&&!m.mine?senderRowH(m):0;const int px=L.px;c.inlineEmoji(px);
  int widest=0;if(L.widest>=0&&lines.size()==L.lines.size())widest=L.widest;else for(auto &line:lines)widest=std::max(widest,c.width(line,Font::ChatText));/* full body: width cached */if(senderH){c.inlineEmoji(SenderEmojiPx);widest=std::max(widest,c.width(senderDisplayName(m.sender),Font::ChatMeta));c.inlineEmoji(px);}
  int width=std::min(BubbleMaxW,std::max(54,widest+2*padX));int h=linesH(lines,px)+2*padY+senderH;
  Rect r={m.mine?W-BubbleMargin-width:BubbleMargin,y,width,h};bubbleHits.push_back({r,i});c.fillRound(r.x,r.y,r.w,r.h,9,m.mine?Accent:SurfaceAlt);auto ink=m.mine?OnAccent:FG;// colour: green own bubbles, glacier-blue received
  int by=y+padY;if(senderH){senderHits.push_back({{r.x+padX,by,r.w-2*padX,senderH},i});c.inlineEmoji(SenderEmojiPx);c.text(r.x+padX,by+(senderH-metaH)/2,senderDisplayName(m.sender),Font::ChatMeta,Accent,r.w-2*padX);c.inlineEmoji(px);by+=senderH;}
  // A link in the text is drawn in the link colour and underlined; its rects (one per line) become tap targets.
  size_t linkAt=std::string::npos,linkLen=0;if(device::linkFindUrl(m.body,&linkAt,&linkLen).empty())linkAt=std::string::npos;
  uint16_t linkInk=m.mine?OnAccent:Accent;size_t cursor=0;
  for(auto &line:lines){int lh=lineH(line,px),ty=by+(lh-bodyH)/2;size_t at=linkAt==std::string::npos?std::string::npos:m.body.find(line,cursor);
   size_t a=0,b=0;if(at!=std::string::npos){cursor=at+line.size();size_t s0=std::max(at,linkAt),s1=std::min(at+line.size(),linkAt+linkLen);if(s0<s1){a=s0-at;b=s1-at;}}
   if(a<b){int x=r.x+padX;std::string pre=line.substr(0,a),url=line.substr(a,b-a),post=line.substr(b);
    if(!pre.empty()){c.text(x,ty,pre,Font::ChatText,ink);x+=c.width(pre,Font::ChatText);}
    int uw=c.width(url,Font::ChatText);c.text(x,ty,url,Font::ChatText,linkInk);g.drawFastHLine(x,ty+bodyH+1,uw,linkInk);linkHits.push_back({{x-2,by-2,uw+4,lh+4},i});x+=uw;
    if(!post.empty())c.text(x,ty,post,Font::ChatText,ink);}
   else c.text(r.x+padX,ty,line,Font::ChatText,ink);// already wrapped to the bubble: no width check per frame
   by+=lh;}c.inlineEmoji(0);
  by=y+h+2;for(auto &line:meta){int x=m.mine?W-BubbleMargin-c.width(line,Font::ChatMeta):BubbleMargin;c.text(x,by,line,Font::ChatMeta,Muted);by+=metaH;}
  return by+BubbleGap;};
 if(!s.messages.empty()){
  // Plus: continuous scroll. The anchor (newest message the offset points at) has its block bottom at the list
  // bottom + chatPixel; older messages stack above it, cut by the header. Pixels beyond the anchor move it.
  const int viewTop=startY+3,viewBottom=messageBottom-4,viewH=viewBottom-viewTop,n=int(s.messages.size());
  int anchor=std::max(0,std::min(n-1,end-1)),pix=s.chatPixel;
  while(anchor>0&&pix>=height(anchor)){pix-=height(anchor);anchor--;}
  while(pix<0&&anchor<n-1){anchor++;pix+=height(anchor);}
  // Newest loaded message reached: at the thread's newest stop there; otherwise the leftover goes back to the caller,
  // which moves past the window (a fling toward the newest keeps its distance).
  const bool newestLoaded=s.chatPosition-s.chatOffset<=0;if(pix<0&&newestLoaded)pix=0;
  // The thread's first message is loaded: stop when it reaches the top (a short thread starts at the top).
  const bool oldestLoaded=s.chatTotal<=0||s.chatPosition-s.chatOffset+n>=s.chatTotal;int above=-pix;for(int i=anchor;i>=0&&above<viewH;i--)above+=height(i);// only as far as one screen
  if(!oldestLoaded&&anchor==0)pix=std::min(pix,height(0));// older rows still loading (or unreadable): never scroll into emptiness
  if(oldestLoaded)while(above<viewH&&(pix>0||anchor<n-1)){if(pix>0){int d=std::min(pix,viewH-above);pix-=d;above+=d;}else{anchor++;int h=height(anchor);pix+=h;above+=h;}}
  chatShiftOut=(end-1)-anchor;chatPixelOut=pix;if(pix<0){pix=0;}
  int bottom=oldestLoaded&&above<viewH?viewTop+above+pix:viewBottom+pix;
  for(int i=anchor;i>=0&&bottom>viewTop;i--){int h=height(i);if(bottom-h<viewBottom){draw(i,bottom-h,layout(i).lines);first=i;}bottom-=h;}
  end=anchor+1;if(first>anchor)first=anchor;
  // Only the visible part of a bubble answers a tap.
  auto clip=[&](std::vector<std::pair<Rect,int>> &hits){for(size_t k=0;k<hits.size();){Rect &r=hits[k].first;int y0=std::max(r.y,viewTop-3),y1=std::min(r.y+r.h,messageBottom);if(y1<=y0){hits.erase(hits.begin()+k);continue;}r.y=y0;r.h=y1-y0;k++;}};
  clip(bubbleHits);clip(senderHits);clip(linkHits);
 }else{
 for(int i=end-1;i>=0;i--){int h=height(i);if(total+h>messageBottom-startY-4){if(i==end-1)first=i;break;}total+=h;first=i;}
 int y=startY+3;
 for(int i=first;i<end;i++){
  const auto &L=layout(i);auto lines=L.lines;const auto &m=s.messages[i];int senderH=!s.chatDirect&&!m.mine?senderRowH(m):0;const int px=L.px;c.inlineEmoji(px);
  int room=messageBottom-y-4-int(L.meta.size())*metaH-BubbleGap-2*padY-senderH;
  int maxLines=0;{int used=0;for(int k=int(lines.size())-1;k>=0;k--){used+=lineH(lines[k],px);if(used>room&&maxLines)break;maxLines++;}}maxLines=std::max(1,maxLines);bool clipped=int(lines.size())>maxLines;
  if(clipped){lines.erase(lines.begin(),lines.end()-maxLines);lines[0]=c.fit(".. "+lines[0],BubbleMaxW-2*padX,Font::ChatText);}
  y=draw(i,y,lines);
 }}
 {int shown=end-first,total=s.chatTotal>0?s.chatTotal:int(s.messages.size()),offset=s.chatTotal>0?s.chatPosition:s.chatOffset;// oldest at the top of the track
  scrollBar(g,startY+2,messageBottom-startY-4,total-offset-shown,shown,total);}
 if(s.messages.empty()){if(s.chatDirect&&s.historyError.empty()&&!s.historyLoading){int mid=(startY+messageBottom)/2;c.centered(Margin,mid-18,W-2*Margin,"Start a new conversation",Font::ChatText);c.centered(Margin,mid+2,W-2*Margin,c.fit(s.chatName,W-2*Margin,Font::ChatMeta),Font::ChatMeta);}
  else c.centered(Margin,startY+30,W-2*Margin,!s.historyError.empty()?s.historyError:s.historyLoading?"Loading SD history...":"No messages yet",Font::ChatMeta);}
 header();
 if(!s.firstRepeater.empty()||showCounter){
  g.fillRect(0,counterY,W,FirstRepeaterH,BG);
  auto count=showCounter?std::to_string(s.input.size())+"/"+std::to_string(s.messageLimit):std::string();
  int counterWidth=c.width(count,Font::ChatMeta);
  if(!s.firstRepeater.empty())c.text(Margin,counterY+2,s.firstRepeater,Font::ChatMeta,FG,W-2*Margin-(showCounter?counterWidth+Gap:0));
  if(showCounter)c.text(W-Margin-counterWidth,counterY+2,count,Font::ChatMeta);
 }
 g.fillRect(0,composerTop,W,composerH,BG);Rect composer={Margin,composerTop+1,W-2*Margin,composerH-3};{c.fillRound(composer.x,composer.y,composer.w,composer.h,9,BG);g.drawRoundRect(composer.x,composer.y,composer.w,composer.h,9,Line);}
 if(!inputRows.empty()){int y=composer.y+9;for(auto &row:inputRows){c.text(inputX,y,row,Font::ChatText);y+=bodyH;}
  const auto &caretRow=inputRows[std::min(caretRowShown,int(inputRows.size())-1)];int caretX=inputX+c.rawWidth(caretRow.substr(0,std::min(caret.column,caretRow.size())),Font::ChatText)+1;if(caret.space)caretX+=c.rawWidth(" ",Font::ChatText);/* a trailing space is dropped by the wrapping but the caret still advances */
  {int cx=std::min(caretX,inputX+inputWidth);g.fillRect(cx,composer.y+8+caretRowShown*bodyH,2,15,Accent);}}
 else if(s.input.empty())c.text(inputX,composer.y+9,"Write message...",Font::ChatText,Muted,inputWidth);
 else {auto tail=c.inputTail(s.input.substr(0,caretByte),inputWidth-3,Font::ChatText);int caretX=inputX+c.width(tail,Font::ChatText)+1;c.text(inputX,composer.y+9,tail,Font::ChatText);
  if(caretByte<s.input.size()&&caretX+4<inputX+inputWidth)c.text(caretX+3,composer.y+9,s.input.substr(caretByte),Font::ChatText,FG,inputX+inputWidth-caretX-3);g.fillRect(caretX,composer.y+8,2,15,Accent);}
 g.drawFastVLine(ComposerEmoji.x-2,composer.y+5,composer.h-10,Line);{constexpr int B=mm_emoji_picker::ButtonSize;if(s.emojiPicker)c.fillRound(ComposerEmoji.x+2,ComposerEmoji.y+2,ComposerEmoji.w-4,ComposerEmoji.h-5,5,FG);g.drawBitmap(ComposerEmoji.x+(ComposerEmoji.w-B)/2,ComposerEmoji.y+(ComposerEmoji.h-B)/2-1,mm_emoji_picker::button,B,B,s.emojiPicker?BG:FG);}
 // Accent key held or armed: the letters the next key gives (language and taps) show above the composer, and the
 // emoji button turns into an accent mark with the tap count.
 if(s.accent){auto letters=accentBarText(s);if(!letters.empty()){Rect bar={composer.x,composer.y-32,composer.w,28};c.fillRound(bar.x,bar.y,bar.w,bar.h,8,FG);c.centered(bar.x,bar.y+(bar.h-c.lineHeight(Font::ChatTitle))/2,bar.w,letters,Font::ChatTitle,BG);}
  c.fillRound(ComposerEmoji.x+2,ComposerEmoji.y+2,ComposerEmoji.w-4,ComposerEmoji.h-5,5,FG);c.centered(ComposerEmoji.x,ComposerEmoji.y+(ComposerEmoji.h-c.lineHeight(Font::ChatTitle))/2,ComposerEmoji.w,s.accentTaps>1?"\xc2\xb4"+std::to_string(s.accentTaps):std::string("\xc2\xb4"),Font::ChatTitle,BG);}
 if(s.emojiPicker)emojiPopup(s);
 if(s.messageMenu){Rect info=dialogPanel();c.text(info.x+8,info.y+8,"Chat options",Font::ChatTitle);const char *labels[]={"Region scope","Add region scope","Clear chat","Send GPS position"};for(int i=0;i<ChatOptionCount;i++)c.button(chatOptionRect(i),labels[i],s.chatMenuFocus==i);}
 if(s.scopePicker){
  Rect panel=dialogPanel();c.text(Margin+8,Top+12,s.notice.empty()?"Region scope":s.notice,s.notice.empty()?Font::ChatTitle:Font::ChatMeta,FG,panel.w-16);
  int offset=std::max(0,std::min(s.scopeOffset,std::max(0,int(s.regionScopes.size())-1)));
  for(int i=0;i<ScopeRows&&offset+i<int(s.regionScopes.size());i++){auto row=scopeRowRect(i);bool selected=offset+i==s.scopeFocus;c.button(row,s.regionScopes[offset+i]=="*"?"Global (*)":s.regionScopes[offset+i],selected);}
  c.button(ScopeAdd,"+ ADD");c.button(ScopeApply,"APPLY");c.button(ScopeCancel,"CANCEL");
  if(s.scopeEditing){g.fillRect(0,Top,W,H-Top,BG);c.card(panel);c.text(Margin+8,Top+12,"Add region scope",Font::ChatTitle);c.text(Margin+8,Top+46,"Example: it-lig",Font::ChatMeta);c.text(Margin+8,Top+62,s.notice.empty()?"Letters, numbers and hyphens":s.notice,Font::ChatMeta,s.notice.empty()?FG:Accent,panel.w-16);/* a rejected name replaces the hint, inside the panel */c.card(ScopeInput);c.text(ScopeInput.x+7,ScopeInput.y+10,s.scopeInput.empty()?"Type a region...":s.scopeInput,Font::ChatText,FG,ScopeInput.w-14);c.button(ScopeSave,"SAVE");c.button(ScopeEditCancel,"CANCEL");}
 }
 if(s.clearChatConfirm){Rect panel=dialogPanel();c.text(Margin+8,Top+15,"Clear all messages?",Font::ChatTitle,FG,panel.w-16);c.text(Margin+8,Top+56,"Channel and scope are kept.",Font::ChatMeta,FG,panel.w-16);c.button(ChatClearYes,"CLEAR CHAT");c.button(ChatClearNo,"CANCEL");if(!s.notice.empty())c.text(Margin+8,Top+215,s.notice,Font::ChatMeta,FG,panel.w-16);}

 if(s.positionShare){
  Rect panel=dialogPanel();c.text(Margin+8,Top+15,"Share position",Font::ChatTitle,FG,panel.w-16);
  c.text(Margin+8,Top+49,s.positionCoordinates.empty()?"Live GPS required":"Current GPS coordinates",Font::ChatMeta,FG,panel.w-16);
  auto lines=c.lines(s.positionCoordinates.empty()?s.positionMessage:s.positionCoordinates,panel.w-16,Font::ChatText);
  for(int i=0;i<std::min(4,int(lines.size()));i++)c.text(Margin+8,Top+84+i*c.lineHeight(Font::ChatText),lines[i],Font::ChatText,FG,panel.w-16);
  std::string label=!s.positionCoordinates.empty()?"SEND POSITION":!s.gpsAvailable?"GPS UNAVAILABLE":!s.gpsEnabled?"ENABLE GPS":"WAITING FOR GPS";
  c.button(PositionSend,label,!s.positionCoordinates.empty());c.button(PositionCancel,"CANCEL");
  if(!s.notice.empty())c.text(Margin+8,Bottom-15,s.notice,Font::ChatMeta,FG,panel.w-16);
 }
 if(s.messageActions){
 if(s.contextMessage.mine){
  Rect panel=dialogPanel();
  c.text(panel.x+8,Top+12,"Your message",Font::ChatTitle);
  c.button(ownMessageActionRect(0),"RESEND");
  c.text(panel.x+8,Top+76,"REPEATERS HEARD / "+(s.contextMessage.repeaters<0?std::string("--"):std::to_string(s.contextMessage.repeaters)),Font::ChatMeta);
  int start=std::max(0,std::min(s.heardOffset,std::max(0,int(s.heardPeers.size())-HeardRows)));
  for(int i=0;i<HeardRows&&start+i<int(s.heardPeers.size());i++)c.text(panel.x+8,HeardListY+i*HeardRowH,s.heardPeers[start+i],Font::ChatMeta,FG,panel.w-16);
  if(s.heardPeers.empty())c.text(panel.x+8,HeardListY,s.contextMessage.repeaters>0?"Names not recorded":"No repeater echoes recorded",Font::ChatMeta,FG,panel.w-16);
  if(!s.notice.empty())c.text(panel.x+8,HeardFooterY,s.notice,Font::ChatMeta,FG,panel.w-16);else c.text(panel.x+8,HeardFooterY,s.heardPeers.size()>HeardRows?"Swipe / J K: "+std::to_string(start+1)+"-"+std::to_string(std::min(start+HeardRows,int(s.heardPeers.size())))+" of "+std::to_string(s.heardPeers.size()):"Echoes / not a delivery ACK",Font::ChatMeta,FG,panel.w-16);
  c.button(ownMessageActionRect(4),"QUOTE");c.button(ownMessageActionRect(2),"PATH");c.button(ownMessageActionRect(1),s.channelMuted?"UNMUTE":"MUTE");c.button(ownMessageActionRect(3),"CLOSE");
 }else{
  Rect panel=dialogPanel();
  c.text(panel.x+8,panel.y+9,senderDisplayName(s.contextMessage.sender),Font::ChatTitle,FG,panel.w-16);
  const char *labels[]={"Mention / reply","View full path","Read message",s.channelMuted?"Unmute notifications":"Mute notifications","Close"};
  for(int row=0;row<ReceivedMessageActions;row++){Rect r=messageActionRect(row);if(row==1){r.w-=MessageMapW+Gap;c.button(messageMapRect(),"MAP");}if(row==2&&s.contextHasLink){r.w-=MessageMapW+Gap;c.button(messageLinkRect(),"LINK");}c.button(r,labels[row]);}

 }
 }
 if(s.messagePath&&s.messageRouteView){
  // Static packet route: low-detail map, numbered hops, legend in a band across the top of the map (its bottom edge
  // carries the scale and the OSM credit). No controls. Up to three legend lines.
  g.fillRect(0,Top,W,H-Top,BG);mapView(s);
  struct Placed {const std::string *text;int x,line;};std::vector<Placed> placed;int x=Margin,line=0;
  for(auto &entry:s.mapLegend){int w=c.width(entry,Font::Tiny);if(x>Margin&&x+w>W-Margin){if(++line>=3)break;x=Margin;}placed.push_back({&entry,x,line});x+=w+10;}
  if(!placed.empty()){int lines=placed.back().line+1,y=MapArea.y;g.fillRect(0,y,W,lines*12+5,BG);g.drawFastHLine(0,y+lines*12+5,W,FG);
   for(auto &p:placed)c.text(p.x,y+3+p.line*12,*p.text,Font::Tiny,FG,W-2*Margin);}
 }else if(s.messagePath){
  Rect panel=dialogPanel();
  c.text(Margin+8,Top+12,s.messageBodyView?"Message":s.contextMessage.mine?"Repeaters heard":"Received path",Font::ChatTitle);
  auto lines=c.lines(s.pathText,panel.w-16,Font::ChatText);int rows=(panel.h-70)/c.lineHeight(Font::ChatText);
  int start=std::min(s.pathOffset,std::max(0,int(lines.size())-rows));
  for(int i=0;i<rows&&start+i<int(lines.size());i++)c.text(Margin+8,Top+40+i*c.lineHeight(Font::ChatText),lines[start+i],Font::ChatText);
  c.button({Margin+8,Bottom-40,58,28},"UP");c.button({Margin+72,Bottom-40,58,28},"DOWN");c.button({Margin+136,Bottom-40,72,28},"CLOSE");
 }

}

void Renderer::mapView(const State &s){
 // Map view (colour OpenStreetMap), then pins (repeater triangle, companion dot), own position, scale, status.
 if(s.mapPixels)c.image(MapArea.x,MapArea.y,s.mapPixels,MapArea.w,MapArea.h);else{g.fillRect(MapArea.x,MapArea.y,MapArea.w,MapArea.h,BG);if(s.mapBits)g.drawBitmap(MapArea.x,MapArea.y,s.mapBits,MapArea.w,MapArea.h,FG);}
 auto label=[&](int x,int y,const std::string &text){int w=c.width(text,Font::Tiny)+4;x=std::max(MapArea.x,std::min(x,MapArea.x+MapArea.w-w));g.fillRect(x,y,w,11,BG);c.text(x+2,y+1,text,Font::Tiny);};
 // Packet route: white halo then a 2 px black line, dashed across hops with no known position.
 for(auto &seg:s.mapRoute){int dx=seg.x1-seg.x0,dy=seg.y1-seg.y0,len=std::max(std::abs(dx),std::abs(dy));
  for(int pass=0;pass<2;pass++)for(int i=0;i<=len;i++){if(seg.dashed&&(i/6)%2)continue;int x=seg.x0+(len?dx*i/len:0),y=seg.y0+(len?dy*i/len:0);if(pass==0)g.fillRect(x-2,y-2,5,5,BG);else g.fillRect(x-1,y-1,2,2,Accent);}}
 for(auto &pin:s.mapPins){if(pin.step>=0){if(pin.step==0){g.fillRect(pin.x-7,pin.y-7,15,15,BG);g.fillRect(pin.x-5,pin.y-5,11,11,FG);}else{g.fillCircle(pin.x,pin.y,9,BG);g.fillCircle(pin.x,pin.y,7,Accent);std::string n=std::to_string(pin.step);c.text(pin.x-c.width(n,Font::Tiny)/2+1,pin.y-4,n,Font::Tiny,OnAccent);}
   if(!pin.name.empty())label(pin.x+10,pin.y-5,pin.name);continue;}
  if(pin.repeater){g.fillTriangle(pin.x,pin.y-9,pin.x-8,pin.y+6,pin.x+8,pin.y+6,BG);g.fillTriangle(pin.x,pin.y-6,pin.x-5,pin.y+4,pin.x+5,pin.y+4,Accent);}else{g.fillCircle(pin.x,pin.y,6,BG);g.fillCircle(pin.x,pin.y,4,FG);}
  if(s.mapPins.size()<=8&&!pin.name.empty())label(pin.x+7,pin.y-5,pin.name);}
 if(s.mapSelfX>=0){int x=s.mapSelfX,y=s.mapSelfY;g.fillCircle(x,y,11,BG);for(int r=8;r<=9;r++)g.drawCircle(x,y,r,FG);g.fillCircle(x,y,4,Accent);for(int d:{-1,0,1}){g.drawFastHLine(x-14,y+d,4,FG);g.drawFastHLine(x+11,y+d,4,FG);g.drawFastVLine(x+d,y-14,4,FG);g.drawFastVLine(x+d,y+11,4,FG);}}
 if(!s.mapStatus.empty())label(MapArea.x+(40),MapArea.y+(12),s.mapStatus);
 {std::string credit="(c) OpenStreetMap contributors";label(MapArea.x+MapArea.w-44-c.width(credit,Font::Tiny)-4,MapArea.y+MapArea.h-14,credit);}
 if(s.mapScalePx>0){int x=MapArea.x+4,y=MapArea.y+MapArea.h-6;g.fillRect(x-2,y-13,std::max(s.mapScalePx,c.width(s.mapScaleLabel,Font::Tiny))+6,17,BG);g.drawFastHLine(x,y,s.mapScalePx,FG);g.drawFastHLine(x,y+1,s.mapScalePx,FG);g.drawFastVLine(x,y-3,5,FG);g.drawFastVLine(x+s.mapScalePx-1,y-3,5,FG);c.text(x,y-12,s.mapScaleLabel,Font::Tiny);}
}
// Link viewer: status bar, a bar with the back disc, host / page title and the PICTURE / PAGE switch, then the
// page rows (text, headings, link rows with a focus card) or the composed picture with floating zoom buttons.
void Renderer::linkView(const State &s){
 g.fillScreen(BG);c.statusBar(s.status);
 int barY=StatusH;g.fillRect(0,barY,W,LinkBarH,Surface);g.drawFastHLine(0,barY+LinkBarH-1,W,Line);
 g.fillCircle(LinkBack.x+LinkBack.w/2,LinkBack.y+LinkBack.h/2,LinkBack.w/2,BG);c.icon(Icon::Back,LinkBack.x+3,LinkBack.y+3,16);
 int titleX=LinkBack.x+LinkBack.w+6,titleW=(s.linkSwitch.empty()?W-Margin:LinkSwitch.x-6)-titleX;
 c.text(titleX,barY+(s.linkTitle.empty()?6:1),s.linkHost,s.linkTitle.empty()?Font::Bold:Font::ChatMeta,s.linkTitle.empty()?FG:Muted,titleW);
 if(!s.linkTitle.empty())c.text(titleX,barY+12,s.linkTitle,Font::Bold,FG,titleW);
 if(!s.linkSwitch.empty()){Rect r=LinkSwitch;c.fillRound(r.x,r.y,r.w,r.h,r.h/2,Accent);c.centered(r.x,r.y+3,r.w,s.linkSwitch,Font::Bold,OnAccent);}
 const Rect A=LinkArea;
 if(s.linkMode==0||s.linkMode==3){// loading / error: centred message
  int cy=A.y+A.h/2-30;c.icon(s.linkMode==3?Icon::Info:Icon::Wifi,W/2-12,cy,24,s.linkMode==3?FG:Accent);
  c.centered(Margin,cy+34,W-2*Margin,s.linkMessage,Font::ChatBody);
  if(!s.linkInfo.empty())c.centered(Margin,cy+54,W-2*Margin,s.linkInfo,Font::ChatMeta,Muted);
  if(s.linkMode==0&&s.linkProgress>=0){Rect bar={W/2-80,cy+74,160,6};c.fillRound(bar.x,bar.y,bar.w,bar.h,3,SurfaceAlt);c.fillRound(bar.x,bar.y,std::max(6,bar.w*s.linkProgress/100),bar.h,3,Accent);}
  return;}
 if(s.linkMode==2){
  if(s.linkPixels)c.image(A.x,A.y,s.linkPixels,A.w,A.h);
  if(!s.linkInfo.empty()){int w=c.width(s.linkInfo,Font::ChatMeta)+10;c.fillRound(A.x+4,A.y+A.h-18,w,15,7,Surface);c.text(A.x+9,A.y+A.h-17,s.linkInfo,Font::ChatMeta,FG);}
  if(s.linkZoomButtons)for(Rect r:{LinkZoomOut,LinkZoomIn}){int cx=r.x+r.w/2,cy=r.y+r.h/2;g.fillCircle(cx,cy,r.w/2,Surface);g.drawCircle(cx,cy,r.w/2,Line);g.fillRect(cx-6,cy-1,12,3,FG);if(r.y==LinkZoomIn.y)g.fillRect(cx-1,cy-6,3,12,FG);}
  return;}
 // Page rows.
 for(int i=0;i<int(s.linkRows.size());i++){Rect r=linkRowRect(i);const auto &row=s.linkRows[i];bool focus=i==s.linkFocus;
  if(row.second==-1){c.text(Margin,r.y+1,row.first,Font::ChatBody,FG,W-2*Margin-6);continue;}
  if(row.second==-2){c.text(Margin,r.y+4,row.first,Font::ChatMeta,Muted,W-2*Margin-6);g.drawFastHLine(Margin,r.y+r.h-2,W-2*Margin-6,Line);continue;}
  Rect card={Margin-3,r.y,W-2*Margin,r.h-1};if(focus)c.fillRound(card.x,card.y,card.w,card.h,6,Accent);
  uint16_t ink=focus?OnAccent:Accent;c.text(Margin+2,r.y+1,row.first,Font::ChatBody,ink,W-2*Margin-12);}
 if(s.linkScrollTotal>LinkRows){int track=A.h-6,thumb=std::max(10,track*LinkRows/s.linkScrollTotal),max=s.linkScrollTotal-LinkRows;c.fillRound(W-4,A.y+3+std::min(s.linkScrollTop,max)*(track-thumb)/max,3,thumb,1,Muted);}
}
void Renderer::channelOptions(const State &s){
 shell(s,s.channelOptionName,true,Icon::Menu);g.fillRect(ComposeButton.x,StatusH,ComposeButton.w,HeaderH-1,BG);
 if(s.channelKeyView){channelKey(s);return;}
 Rect panel=dialogPanel(false);
 c.text(Margin+8,Top+15,s.channelOptionDirect?"Chat options":"Channel options",Font::ChatTitle,FG,panel.w-16);
 if(s.deleteChannelConfirm){
  c.text(Margin+8,Top+55,s.channelOptionDirect?"Delete this chat?":"Delete this channel?",Font::ChatBody,FG,panel.w-16);
  c.text(Margin+8,Top+87,"Its messages will be removed.",Font::ChatMeta,FG,panel.w-16);
  c.button(ChatClearYes,"DELETE");c.button(ChatClearNo,"CANCEL");
 }else{
  c.text(Margin+8,Top+50,s.channelOptionMuted?"Notifications: OFF":"Notifications: ON",Font::ChatMeta,FG,panel.w-16);
  c.button(channelOptionRect(0),s.channelOptionMuted?"Enable notifications":"Mute notifications",s.channelOptionFocus==0);
  if(!s.channelOptionDirect)c.button(channelOptionRect(1),"Show key + QR",s.channelOptionFocus==1);
  int slot=s.channelOptionDirect?1:2;c.button(channelOptionRect(slot),s.channelOptionDirect?"Delete chat":"Delete channel",s.channelOptionFocus==slot);
 }
 if(!s.notice.empty())c.text(Margin+8,Bottom-28,s.notice,Font::ChatMeta,FG,panel.w-16);
}
// Share view: MeshCore-app QR (white quiet zone of 2 modules), name, key in 4-char groups, scope.
void Renderer::channelKey(const State &s){
 {// QR on the left, name / key / scope beside it
  int n=s.qrSize,scale=n?std::min(4,(Bottom-Top-8)/(n+4)):0,side=(n+4)*scale,x0=Margin,y0=Top+4;
  if(n&&int(s.qrModules.size())==n*n){g.fillRect(x0,y0,side,side,BG);for(int y=0;y<n;y++)for(int x=0;x<n;x++)if(s.qrModules[y*n+x])g.fillRect(x0+(x+2)*scale,y0+(y+2)*scale,scale,scale,FG);}
  else c.text(Margin,y0+60,"QR unavailable",Font::ChatMeta);
  int x=Margin+std::max(side,100)+8,w=W-Margin-x;
  c.text(x,Top+10,s.channelOptionName,Font::ChatTitle,FG,w);
  for(int l=0;l<2&&l*16<int(s.channelKeyHex.size());l++)c.text(x,Top+36+l*16,channelSecretGroups(s.channelKeyHex,l*16,16),Font::Body,FG,w);
  c.text(x,Top+80,"Scope "+(s.channelKeyScope.empty()||s.channelKeyScope=="*"?std::string("all regions"):s.channelKeyScope),Font::ChatMeta,FG,w);
  c.text(x,Top+96,"Scan with MeshCore",Font::ChatMeta,FG,w);c.text(x,Top+112,"Key holders can read and send",Font::ChatMeta,FG,w);
  return;
 }}
// First-boot welcome: radio band, network preset, companion name, path hash size, summary.
void Renderer::welcome(const State &s){
 g.fillScreen(BG);c.statusBar(s.status);
 const char *titles[]={"Welcome to MatterMesh","Wi-Fi and clock","Radio band","Network preset","Companion name","Path hash size","Ready"};
 const char *hints[]={"Off-grid chat over LoRa, on the MeshCore network.","Wi-Fi sets the clock, updates and weather.","Pick the LoRa band of your radio and country.","The settings your local mesh uses.","How other nodes will see you.","Bytes used for each repeater in a route.","Check and confirm. You can change it later in LoRa."};
 int step=std::max(0,std::min(6,s.welcomeStep));
 for(int i=0;i<7;i++){int x=W-Margin-7*12+i*12;g.fillCircle(x+4,StatusH+12,i==step?4:3,i<=step?Accent:Line);}
 c.text(Margin,StatusH+5,titles[step],Font::ChatTitle,FG,W-2*Margin-92);c.text(Margin,StatusH+27,hints[step],Font::ChatMeta,Muted,W-2*Margin);
 auto choice=[&](Rect r,const std::string &big,const std::string &small,bool on){c.card(r,on,10);auto ink=on?OnAccent:FG;c.centered(r.x,r.y+22,r.w,big,Font::ChatTitle,ink);
  auto lines=c.clampLines(small,r.w-16,Font::ChatMeta,2,"..");for(size_t k=0;k<lines.size();k++)c.centered(r.x+4,r.y+52+int(k)*14,r.w-8,lines[k],Font::ChatMeta,on?OnAccent:Muted);};
 if(step==0){
  c.icon(Icon::Radio,W/2-16,70,32,Accent);
  c.centered(Margin,112,W-2*Margin,"Let's set up your device",Font::ChatBody,FG);c.centered(Margin,132,W-2*Margin,"Wi-Fi, band, preset, name and route bytes.",Font::ChatMeta,Muted);
  c.centered(Margin,150,W-2*Margin,"Trackball or touch to choose, ENTER to continue.",Font::ChatMeta,Muted);
  if(!s.welcomeArchive.empty()){Rect r={Margin,163,W-2*Margin,34};c.fillRound(r.x,r.y,r.w,r.h,10,Surface);int ty=r.y+3;
   for(auto &l:c.clampLines("Old data moved to "+s.welcomeArchive+" on the microSD. Nothing was deleted.",r.w-16,Font::ChatMeta,2,".."))
    {c.text(r.x+8,ty,l,Font::ChatMeta,Muted,r.w-16);ty+=14;}}
 }else if(step==1){
  // Two short cards (connect / skip), then how the clock gets set.
  auto small=[&](int i,const std::string &big,const std::string &line,bool on){Rect r=welcomeCard(i,2);r.y=60;r.h=50;c.card(r,on,10);auto ink=on?OnAccent:FG;
   c.centered(r.x,r.y+7,r.w,big,Font::ChatBody,ink);c.centered(r.x+4,r.y+28,r.w-8,line,Font::ChatMeta,on?OnAccent:Muted);};
  small(0,s.wifiConnected?"Connected":"Connect to Wi-Fi",s.wifiConnected?s.wifiSSID:"Choose a network",s.welcomeWifiChoice==0);small(1,"Skip for now","Set it up later",s.welcomeWifiChoice==1);
  Rect r={Margin,116,W-2*Margin,WelcomeNext.y-122};c.fillRound(r.x,r.y,r.w,r.h,10,Surface);c.icon(Icon::Clock,r.x+8,r.y+7,16,Accent);
  c.text(r.x+30,r.y+6,"How the clock is set",Font::Bold,FG,r.w-38);
  const char *lines[]={"With Wi-Fi: from the internet, in a few seconds.","Without Wi-Fi: from GPS satellites, outdoors.","Or by hand: Settings > Date / time."};
  for(int i=0;i<3;i++)c.text(r.x+12,r.y+26+i*15,lines[i],Font::ChatMeta,Muted,r.w-20);
 }else if(step==2){choice(welcomeCard(0,2),"868 MHz","Europe, UK, Switzerland",s.welcomeBand==868);choice(welcomeCard(1,2),"900 MHz","Americas, Oceania, Asia",s.welcomeBand==900);}
 else if(step==3){
  for(int i=0;i<WelcomeRows&&s.welcomeScroll+i<int(s.welcomePresets.size());i++){int at=s.welcomeScroll+i;Rect r=welcomeRow(i);bool on=at==s.welcomeFocus;c.card(r,on,8);auto ink=on?OnAccent:FG;
   c.text(r.x+10,r.y+2,s.welcomePresets[at].first,Font::ChatBody,ink,r.w-20);c.text(r.x+10,r.y+17,s.welcomePresets[at].second,Font::ChatMeta,on?OnAccent:Muted,r.w-20);}
  int n=s.welcomePresets.size();if(n>WelcomeRows){int track=WelcomeRows*WelcomeRowH-3,thumb=std::max(12,track*WelcomeRows/n),max=n-WelcomeRows;c.fillRound(W-4,WelcomeRowY+std::min(s.welcomeScroll,max)*(track-thumb)/max,3,thumb,1,Muted);}
 }else if(step==4){
  Rect f=WelcomeNameField;c.fillRound(f.x,f.y,f.w,f.h,10,Surface);g.drawRoundRect(f.x,f.y,f.w,f.h,10,Accent);auto tail=c.inputTail(s.welcomeName,f.w-24,Font::ChatText);c.text(f.x+10,f.y+8,tail,Font::ChatText,FG);g.fillRect(f.x+11+c.width(tail,Font::ChatText),f.y+8,2,18,Accent);
  c.text(Margin+2,f.y+f.h+8,"Letters, digits and spaces; up to 31 bytes.",Font::ChatMeta,Muted,W-2*Margin);c.text(Margin+2,f.y+f.h+24,"Type on the keyboard, BACKSPACE deletes.",Font::ChatMeta,Muted,W-2*Margin);
 }else if(step==5){const char *small[]={"Most compatible","Balanced","Fewest clashes"};for(int b=1;b<=3;b++)choice(welcomeCard(b-1,3),std::to_string(b)+(b==1?" byte":" bytes"),small[b-1],s.welcomeBytes==b);}
 else{
  Rect r={Margin,56,W-2*Margin,124};c.card(r,false,10);
  const char *labels[]={"WI-FI","BAND","PRESET","NAME","PATH HASH"};std::string values[]={s.wifiConnected?s.wifiSSID:"Not connected",std::to_string(s.welcomeBand)+" MHz",s.welcomePreset,s.welcomeName,std::to_string(s.welcomeBytes)+(s.welcomeBytes==1?" byte":" bytes")};
  for(int i=0;i<5;i++){int y=r.y+5+i*23;c.text(r.x+10,y+2,labels[i],Font::ChatMeta,Muted);c.text(r.x+96,y,values[i],Font::ChatBody,FG,r.w-106);}
 }
 if(!s.notice.empty())c.centered(Margin,WelcomeNext.y-18,W-2*Margin,s.notice,Font::ChatMeta,Accent);
 if(step>0){c.fillRound(WelcomeBack.x,WelcomeBack.y,WelcomeBack.w,WelcomeBack.h,15,Surface);c.centered(WelcomeBack.x,WelcomeBack.y+8,WelcomeBack.w,"BACK",Font::Bold,FG);}
 c.fillRound(WelcomeNext.x,WelcomeNext.y,WelcomeNext.w,WelcomeNext.h,15,Accent);c.centered(WelcomeNext.x,WelcomeNext.y+8,WelcomeNext.w,step==0?"START":step==6?"FINISH":step==1&&s.welcomeWifiChoice==0&&!s.wifiConnected?"CONNECT":"NEXT",Font::Bold,OnAccent);
}
void Renderer::holdBanner(const std::string &label,int progress,int elapsedTenths,bool done,Icon icon){
 // Same shell as unlockTapBanner: the date/header band, a card with the icon, the label and dots on the right.
 // Six dots stand for the 2 s hold (one every third of a second); all filled when the action is done.
 (void)elapsedTenths;progress=done?HoldDots:std::max(0,std::min(HoldDots,progress));
 g.fillRect(0,HoldBannerY,W,HoldBannerH,BG);
 Rect r={Margin,HoldBannerY+3,W-2*Margin,HoldBannerH-6};c.card(r);
 c.icon(icon,r.x+8,r.y+(r.h-16)/2,16);
 c.text(r.x+30,r.y+(r.h-c.lineHeight(Font::ChatMeta))/2,label,Font::ChatMeta,FG,r.w-30-HoldDotsW-8);
 for(int i=0;i<HoldDots;i++){int cx=r.x+r.w-HoldDotsW+HoldDotR+i*HoldDotStep,cy=r.y+r.h/2;if(i<progress)g.fillCircle(cx,cy,HoldDotR,FG);else g.drawCircle(cx,cy,HoldDotR,FG);}
}

void Renderer::emojiPopup(const State &s){
  g.fillRect(0,EmojiPopup.y-3,W,ComposerY-EmojiPopup.y+3,BG);c.card(EmojiPopup,false,6);
  for(int t=0;t<EmojiTabs;t++){Rect r=emojiTabRect(t);bool on=s.emojiTab==t;c.card(r,on,4);auto ink=on?BG:FG;if(t){const int T=16;c.emoji(mm_emoji_picker::glyphs[mm_emoji_picker::categories[t-1].first].code,r.x+(r.w-T)/2,r.y+(r.h-T)/2,T,ink);}else c.icon(Icon::Clock,r.x+(r.w-16)/2,r.y+(r.h-16)/2,16,ink);}
  g.drawFastHLine(EmojiPopup.x+4,EmojiPopup.y+31,EmojiPopup.w-8,FG);
  std::vector<int> items;if(s.emojiTab==0)items=s.emojiRecent;else{auto &cat=mm_emoji_picker::categories[s.emojiTab-1];for(int i=0;i<cat.count;i++)items.push_back(cat.first+i);}
  for(int i=0;i<int(items.size())&&i<EmojiPageSize;i++){Rect r=emojiChoiceRect(i);bool on=s.emojiFocus==i;if(on)c.card(r,true,5);
   c.emoji(mm_emoji_picker::glyphs[items[i]].code,r.x+(r.w-30)/2,r.y+(r.h-30)/2,30,on?OnAccent:FG);
  }
  int footY=EmojiPopup.y+EmojiPopup.h-15;c.text(EmojiPopup.x+8,footY,s.emojiTab?mm_emoji_picker::categories[s.emojiTab-1].label:"Most used",Font::ChatMeta);std::string hint="N/M tab  ENTER add";c.text(EmojiPopup.x+EmojiPopup.w-8-c.width(hint,Font::ChatMeta),footY,hint,Font::ChatMeta);
}
void Renderer::quickCountdownBanner(int dots){
 // Same band as the unlock banner: message icon, what is about to happen and two dots that fill over 2 s.
 dots=std::max(0,std::min(2,dots));
 Rect r={Margin,HoldBannerY+3,W-2*Margin,HoldBannerH-6};c.fillRound(r.x,r.y,r.w,r.h,9,BG);
 c.icon(Icon::Messages,r.x+8,r.y+(r.h-16)/2,16);
 c.text(r.x+30,r.y+(r.h-c.lineHeight(Font::ChatMeta))/2,"Opening quick reply...",Font::ChatMeta,FG,r.w-74);
 for(int i=0;i<2;i++){int cx=r.x+r.w-30+i*16,cy=r.y+r.h/2;if(i<dots)g.fillCircle(cx,cy,4,Accent);else g.drawCircle(cx,cy,4,Muted);}
}
void Renderer::unlockTapBanner(int taps){
 // Takes the date row's band: a card with the lock icon, the gesture and one dot per SPACE tap.
 taps=std::max(0,std::min(2,taps));
 Rect r={Margin,HoldBannerY+3,W-2*Margin,HoldBannerH-6};c.fillRound(r.x,r.y,r.w,r.h,9,BG);
 c.icon(Icon::Lock,r.x+8,r.y+(r.h-16)/2,16);
 c.text(r.x+30,r.y+(r.h-c.lineHeight(Font::ChatMeta))/2,"Trackball twice to unlock",Font::ChatMeta,FG,r.w-74);
 for(int i=0;i<2;i++){int cx=r.x+r.w-30+i*16,cy=r.y+r.h/2;if(i<taps)g.fillCircle(cx,cy,4,Accent);else g.drawCircle(cx,cy,4,Muted);}
}

// New direct chat: saved companion contacts (no repeaters), search, tap or ENTER opens the chat.
void Renderer::contactPicker(const State &s){
 shell(s,"",true,Icon::Menu);g.fillRect(MessageMenu.x,StatusH,MessageMenu.w,HeaderH-1,BG);c.text(34,StatusH+7,s.pickerMention?"Mention":"New direct chat",Font::ChatTitle,FG,W-70);c.iconButton(MessageMenu,Icon::Search,s.pickerSearching);
 c.card(PickerSearch,s.pickerSearching);c.text(PickerSearch.x+8,PickerSearch.y+6,s.pickerQuery.empty()?(s.pickerSearching?"Type a name...":s.pickerMention?"Search people to mention":"Search contacts"):s.pickerQuery,Font::ChatMeta,s.pickerSearching?(OnAccent):FG,PickerSearch.w-16);
 for(int row=0;row<PickerRows&&row<int(s.pickerRows.size());row++){Rect r=pickerRowRect(row);bool focused=s.pickerStart+row==s.pickerFocus;auto ink=focused?(OnAccent):FG;c.card(r,focused);
  c.text(r.x+8,r.y+4,s.pickerRows[row].name,Font::ChatBody,ink,r.w-16);c.text(r.x+8,r.y+23,s.pickerRows[row].detail,Font::ChatMeta,ink,r.w-16);}
 if(s.pickerRows.empty())c.centered(Margin,Top+70,W-2*Margin,s.pickerQuery.empty()?"No saved contacts yet":"No contact matches",Font::ChatMeta);
 g.drawFastHLine(0,FooterY,W,FG);c.text(Margin,FooterY+4,s.notice.empty()?std::to_string(s.pickerTotal)+(s.pickerTotal==1?" CONTACT":" CONTACTS"):s.notice,Font::ChatMeta,FG,W-90);
 if(s.pickerStart>0)c.centered(PickerPrev.x,FooterY+4,PickerPrev.w,"<",Font::ChatMeta);if(s.pickerStart+PickerRows<s.pickerTotal)c.centered(PickerNext.x,FooterY+4,PickerNext.w,">",Font::ChatMeta);
}
void Renderer::form(const State &s){
 const auto mode=ChannelForm(s.newForm);const bool hashtag=mode==ChannelForm::JoinHashtag,create=mode==ChannelForm::CreatePrivate;
 shell(s,create?"New private channel":hashtag?"Join hashtag channel":"Join private channel");g.fillRect(W-32,StatusH,32,HeaderH-1,BG);
 auto name=formRect(0);c.text(name.x,name.y-16,hashtag?"HASHTAG CHANNEL":"CHANNEL NAME",Font::ChatMeta);c.card(name,s.field==0);auto ink=s.field==0?BG:FG;
 if(hashtag){c.text(name.x+8,name.y+8,"#",Font::Body,ink);c.text(name.x+20,name.y+8,s.newName.empty()?"channel-name":s.newName,Font::Body,ink,name.w-28);}
 else c.text(name.x+8,name.y+8,s.newName.empty()?"Write with keyboard...":s.newName,Font::Body,ink,name.w-18);
 auto key=formRect(1);
 if(hashtag){c.text(key.x,key.y-16,"ENCRYPTION",Font::ChatMeta);c.card(key);auto note=c.lines("The key comes from the #name: anyone who knows it can read.",key.w-16,Font::ChatMeta);for(unsigned l=0;l<note.size()&&l<2;l++)c.text(key.x+8,key.y+6+l*14,note[l],Font::ChatMeta);}
 else{c.text(key.x,key.y-16,create?"GENERATED SECRET KEY":"SECRET KEY (32 HEX)",Font::ChatMeta);c.card(key,s.field==1);auto keyInk=s.field==1?BG:FG;
  if(s.newKey.empty())c.text(key.x+8,key.y+13,"Write with keyboard...",Font::Body,keyInk,key.w-16);
  else for(int l=0;l<2;l++)c.text(key.x+8,key.y+5+l*17,channelSecretGroups(s.newKey,l*16,16),Font::Body,keyInk,key.w-16);}
 auto scope=formRect(2);c.text(scope.x,scope.y-16,"REGION SCOPE",Font::ChatMeta);c.card(scope,s.field==2);c.text(scope.x+8,scope.y+8,s.newScope.empty()?"* (all regions)":s.newScope,Font::Body,s.field==2?BG:FG,scope.w-18);
 c.button(SaveButton,create?"CREATE":"JOIN");c.button(CancelButton,"CANCEL");
 c.text(Margin,FormActionsY+34,s.notice.empty()?create?"Tap the key for a new one / Enter: next":"Enter: next / save":s.notice,Font::Small,FG,W-2*Margin);
 c.footer(s.symbols?"SYMBOLS":s.caps?"ABC":"abc","MESH CHAT");
}
void Renderer::settings(const State &s){
 shell(s,"Settings");
 const char *groups[]={"CONNECTIVITY","DEVICE","SYSTEM"};const int first[]={0,SettingsDeviceFirst,SettingsSystemFirst};
 // A group label is drawn only together with its first row, so it never sits alone at the bottom.
 for(int i=0;i<3;i++)if(settingsVisible(first[i],s.settingsOffset))c.text(Margin+2,settingsRect(first[i],s.settingsOffset).y-15,groups[i],Font::ChatMeta);
 for(int i=0;i<SettingsCount;i++){if(!settingsVisible(i,s.settingsOffset))continue;auto r=settingsRect(i,s.settingsOffset);
  // Detail line by destination section (the row list differs between the Max and the Plus).
  int to=SettingsItems[i].section;
  std::string look=theme(s.wallpaper).name;
  std::string detail=to==4?(s.wifiConnected?"Connected":s.wifiEnabled?"On, not connected":"Off"):to==14?(s.bleReady?"On":"Off"):to==2?(s.radioReady?"SX1262 ready":"Offline"):to==3?(s.gpsEnabled?"On":"Off"):to==6?("Screen / keyboard light"):to==22?look:to==31?(s.textSize?"Large chat text":"Compact chat text"):to==32?(s.accentMode==1?"Right Shift in chats":s.accentMode==2?"Left Shift in chats":"Off"):to==KeyboardSection?keyboardDetail(s):to==21?("Speaker / ringtones"):to==20?"Light timers / auto lock":to==5?(s.status.battery<0?"--%":std::to_string(s.status.battery)+"%"):to==16?s.timeSource:to==24?(s.sd?"SD available":"No SD"):to==17?"Update your firmware":to==SupportSection?"Support the project, Extra apps":to==11?"USB diagnostics":"Version "+s.version;
  c.menuRow(r,SettingsItems[i].icon,SettingsItems[i].label,detail,!s.hideFocus&&s.settingsFocus==i);
 }
 // Scroll indicator like the Home conversation list: thin track + 3 px thumb right of the cards.
 if(int max=settingsMaxScroll();max>0){int track=Bottom-SettingsListY,total=settingsContentY(SettingsCount-1)+SettingsCardH,thumb=std::max(12,track*track/total),offset=std::min(s.settingsOffset,max);
  g.drawFastVLine(W-3,SettingsListY,track,FG);g.fillRect(W-4,SettingsListY+settingsSnapY(offset)*(track-thumb)/settingsSnapY(max),3,thumb,FG);}
}
void Renderer::section(const State &s){
 if(s.page==Page::Link){linkView(s);return;}
 const char *title="MatterMesh OS";
 switch(s.page){case Page::Storage:title="Storage";break;case Page::Keyboard:title="Keyboard languages";break;case Page::Controls:title="Lights";break;case Page::Audio:title="Sound";break;case Page::Power:title="Power / timeouts";break;case Page::Wallpaper:title="Themes";break;case Page::Mesh:title="Contacts";break;case Page::Radio:title="LoRa";break;case Page::GPS:title="GPS";break;case Page::Maps:title="Maps";break;case Page::Weather:title="Weather";break;case Page::Info:title="About";break;case Page::Support:title="Donate / Unlock";break;case Page::Files:title="Files";break;case Page::Wifi:title="Wi-Fi";break;case Page::Bluetooth:title="Bluetooth";break;case Page::Terminal:title="Diagnostics";break;case Page::DateTime:title="Date / time";break;case Page::Updates:title="OTA update";break;case Page::Battery:title="Battery";break;case Page::AccentKey:title="Accent key";break;case Page::TextSize:title="Text size";break;default:break;}
 if(s.page==Page::Maps){g.fillScreen(BG);c.statusBar(s.status);}else{shell(s,title);g.fillRect(W-32,StatusH,32,HeaderH-1,BG);}// Plus Maps: no header, the map takes the page
 auto valueCard=[&](Rect r,const std::string &label,const std::string &value){c.card(r);c.text(r.x+7,r.y+4,label,Font::ChatMeta);c.text(r.x+7,r.y+20,value,Font::Bold,FG,r.w-14);};
 auto gb=[](uint64_t bytes){char text[16];if(bytes<1073741824ull)snprintf(text,sizeof(text),"%u MB",unsigned(bytes/1048576));else snprintf(text,sizeof(text),"%.1f GB",bytes/1073741824.0);return std::string(text);};
 if(s.page==Page::Controls){
  const char *names[]={"Display light","Keyboard light"};
  auto header=[&](int y,int row,const char *name,int percent,bool enabled){c.text(Margin+2,y,name,Font::ChatBody);auto value=std::to_string(percent)+"%";auto t=lightToggleRect(row);c.text(t.x-8-c.width(value,Font::ChatMeta),y+2,value,Font::ChatMeta);
   if(enabled)c.fillRound(t.x,t.y,t.w,t.h,11,FG);else g.drawRoundRect(t.x,t.y,t.w,t.h,11,FG);c.centered(t.x,t.y+5,t.w,enabled?"ON":"OFF",Font::Bold,enabled?BG:FG);};
  for(int row=0;row<2;row++){int percent=((row?s.keyboardLight:s.displayLight)*100+127)/255;header(ControlFirstY+row*ControlStep,row,names[row],percent,row?s.keyboardLightEnabled:s.displayLightEnabled);c.slider(lightSliderRect(row),percent);}
  {c.text(Margin+2,HapticY+4,"Lights wake on key or touch.",Font::ChatMeta,Muted,W-2*Margin);return;}}else if(s.page==Page::Mesh){
  // Contacts: ALL / COMP / RPT tabs opening into the panel (LoRa style), search, four rows, pager.
  // A companion row opens its chat.
  g.fillRect(34,StatusH,W-34,HeaderH-1,BG);g.drawFastHLine(0,Top-1,W,BG); // the tabs are the title
  auto tab=[&](Rect r,const char *label,bool selected){c.tab(r,label,selected,selected);};

  const char *filters[]={"ALL","COMP","RPT"};for(int i=0;i<ContactTabCount;i++)tab(contactTabRect(i),filters[i],s.filter==i);
  {
   // Plus: search + count row, full-width rows (type badge, name, detail line, distance, signal), scrollbar.
   c.card(ContactSearch);auto tail=c.inputTail(s.contactQuery,ContactSearch.w-40,Font::ChatBody);c.text(ContactSearch.x+7,ContactSearch.y+5,s.contactQuery.empty()?(s.contactSearching?"Type a name...":s.filter==2?"Search repeaters":s.filter==1?"Search companions":"Search contacts"):tail,Font::ChatBody,Muted,ContactSearch.w-36);
   if(s.contactSearching)g.drawFastVLine(ContactSearch.x+8+c.width(tail,Font::ChatBody),ContactSearch.y+5,16,FG);c.icon(Icon::Search,ContactSearch.x+ContactSearch.w-22,ContactSearch.y+5,16,Muted);
   std::string summary=!s.notice.empty()?s.notice:!s.radioReady?"Radio offline":std::to_string(s.filteredNodeCount)+" / "+std::to_string(s.contactLive)+" live";c.text(ContactCount.x+ContactCount.w-c.width(summary,Font::ChatMeta),ContactCount.y+7,summary,Font::ChatMeta,Muted,ContactCount.w);
   for(int i=0;i<ContactRows&&i<int(s.nodes.size());i++){auto row=contactRowRect(i);auto &n=s.nodes[i];bool selected=!s.hideFocus&&s.contactFocus==s.nodeScroll+i;c.card(row,selected,8);auto ink=selected?OnAccent:FG;auto soft=selected?OnAccent:Muted;
    Rect badge={row.x+5,row.y+(row.h-24)/2,24,24};c.fillRound(badge.x,badge.y,badge.w,badge.h,6,selected?OnAccent:n.type==2?Mist:SurfaceAlt);c.icon(n.type==2?Icon::Radio:Icon::Contacts,badge.x+4,badge.y+4,16,selected?Accent:FG);
    int right=row.x+row.w-6;int distW=n.distance.empty()?0:c.width(n.distance,Font::ChatMeta);if(distW)c.text(right-distW-(n.live?24:0),row.y+3,n.distance,Font::ChatMeta,soft);
    c.text(badge.x+badge.w+8,row.y+2,n.name.empty()?std::string("Unknown"):n.name,Font::ChatBody,ink,right-badge.x-badge.w-10-(distW?distW+6:0));
    std::string line=contactLine(n);
    if(n.live)c.signalBars(right-19,row.y+row.h-7,n.snr,ink);c.text(badge.x+badge.w+8,row.y+18,line,Font::ChatMeta,soft,right-badge.x-badge.w-10-(n.live?24:0));}
   if(s.nodes.empty()){c.centered(Margin,ContactRowY+40,W-2*Margin,s.contactQuery.empty()?"No contacts yet":"No contact matches",Font::Bold);c.centered(Margin,ContactRowY+60,W-2*Margin,!s.radioReady?"Radio offline / check LoRa":s.contactQuery.empty()?"Waiting for advertisements":"Try another name",Font::ChatMeta,Muted);}
   if(s.filteredNodeCount>ContactRows){int track=ContactRows*ContactRowH-3,thumb=std::max(12,track*ContactRows/s.filteredNodeCount),max=s.filteredNodeCount-ContactRows;c.fillRound(W-4,ContactRowY+std::min(s.nodeScroll,max)*(track-thumb)/max,3,thumb,1,Muted);}
  }
 }else if(s.page==Page::Radio){
  const char *labels[]={"Frequency","Bandwidth","Spread factor","Coding rate","TX power","Companion name"};
  g.drawFastHLine(0,Top-1,W,BG);
  auto tab=[&](Rect r,const char *label,bool selected){c.tab(r,label,selected,true);};// the radio tabs stay open at the bottom
  c.tabPanel(LoRaPanel);
  tab(LoRaRadio,"RADIO",!s.radioDevicePage);tab(LoRaDevice,"DEVICE",s.radioDevicePage);
  if(!s.radioPowerEditing){int tab=s.radioDevicePage?(s.radioDeviceFocus==5?0:s.radioDeviceFocus==6?1:-1):(s.radioFocus==8?0:s.radioFocus==9?1:-1);if(tab>=0){auto r=tab?LoRaDevice:LoRaRadio;g.drawFastHLine(r.x+8,r.y+r.h-5,r.w-16,FG);}}
  if(s.radioDevicePage){
   c.card(LoRaName,s.radioDeviceFocus==0,5);auto ink=s.radioDeviceFocus==0?BG:FG;
   c.text(LoRaName.x+8,LoRaName.y+7,"Companion name",Font::ChatMeta,ink);
   c.text(LoRaName.x+8,LoRaName.y+28,s.radioName,Font::ChatBody,ink,LoRaName.w-16);
   c.text(LoRaInset,LoRaBytesY-23,"Path hash bytes",Font::ChatBody);
   for(int bytes=1;bytes<=3;bytes++){auto r=loraBytesRect(bytes);c.button(r,std::to_string(bytes),s.radioPathBytes==bytes);if(s.radioDeviceFocus==bytes)g.drawRect(r.x-2,r.y-2,r.w+4,r.h+4,FG);}
   
   c.button(LoRaDeviceApply,s.radioDraftChanged?"APPLY *":"APPLY",s.radioDeviceFocus==4);
   c.text(LoRaInset,Top+238,s.notice.empty()?"W S: select / Enter: edit":s.notice,Font::ChatMeta,FG,LoRaContentW);
  }else{
  std::string radioLine=s.radioReady?(s.radioTX?"TX active":s.radioQueued?"TX queued / RX":"RX ready"):("Radio error "+std::to_string(s.radioError)),pathLine="paths "+std::to_string(s.radioPathBytes)+"B";
  
  c.button(LoRaBand868,"868 MHz",s.radioBand==868);c.button(LoRaBand900,"900 MHz",s.radioBand==900);
  c.card(LoRaPreset,false,5);if(s.radioFocus==0)g.drawRoundRect(LoRaPreset.x+2,LoRaPreset.y+2,LoRaPreset.w-4,LoRaPreset.h-4,4,FG);
  c.iconButton({LoRaPresetPrev.x,LoRaPreset.y+(LoRaPreset.h-30)/2,30,30},Icon::Back);c.iconButton({LoRaPresetNext.x,LoRaPreset.y+(LoRaPreset.h-30)/2,30,30},Icon::ChevronRight);
  std::string name=s.radioPreset>=0&&s.radioPreset<matter::RADIO_PRESET_COUNT?matter::RADIO_PRESETS[s.radioPreset].name:"Custom";
  auto names=c.clampLines(name,LoRaPreset.w-64,Font::ChatMeta,2,"..");
  int nameY=LoRaPreset.y+(LoRaPreset.h-int(names.size())*c.lineHeight(Font::ChatMeta))/2;
  for(int i=0;i<int(names.size());i++)c.centered(LoRaPreset.x+32,nameY+i*c.lineHeight(Font::ChatMeta),LoRaPreset.w-64,names[i],Font::ChatMeta);
  char frequency[24],bandwidth[24];snprintf(frequency,sizeof(frequency),"%.3f MHz",s.radioFrequency);snprintf(bandwidth,sizeof(bandwidth),"%.2f kHz",s.radioBW);
  const std::string values[]={frequency,bandwidth,"SF"+std::to_string(s.radioSF),"4/"+std::to_string(s.radioCR),std::to_string(s.radioPower)+" dBm"};
  for(int i=0;i<matter::RadioFieldCount;i++){auto row=loraFieldRect(i);bool focus=s.radioFocus==i+1;c.card(row,focus);auto ink=focus?BG:FG;c.text(row.x+6,row.y+(3),labels[i],Font::ChatMeta,ink,LoRaValueX-12);c.text(row.x+LoRaValueX,row.y+(2),values[i],Font::ChatBody,ink,row.w-LoRaValueX-6);}
  c.button(LoRaApply,s.radioDraftChanged?"APPLY *":"APPLY",s.radioFocus==6);c.button(LoRaAdvert,"ADVERTISE",s.radioFocus==7);
  c.text(LoRaInset,Bottom-17,!s.notice.empty()?s.notice:radioLine+" / "+pathLine,Font::ChatMeta,FG,LoRaContentW);
  }
  if(s.radioPowerEditing){
   Rect panel=dialogPanel();
   int field=std::max(0,std::min(int(matter::RadioFieldCount),s.radioEditField));
   c.text(Margin+8,Top+15,labels[field],Font::ChatTitle);
   if(field==matter::RadioFieldCount){c.text(Margin+8,Top+46,"1 to 31 bytes / name only",Font::ChatMeta);c.text(Margin+8,Top+62,"Public key stays unchanged",Font::ChatMeta);}
   else if(field==matter::Frequency){c.text(Margin+8,Top+46,"863-870 / 902-928 MHz",Font::ChatMeta);c.text(Margin+8,Top+62,"Enter frequency in MHz",Font::ChatMeta);}
   else if(field==matter::Bandwidth){c.text(Margin+8,Top+43,"7.8 / 10.4 / 15.6 / 20.8",Font::ChatMeta);c.text(Margin+8,Top+57,"31.25 / 41.7 / 62.5",Font::ChatMeta);c.text(Margin+8,Top+71,"125 / 250 / 500 kHz",Font::ChatMeta);}
   else c.text(Margin+8,Top+46,field==matter::SpreadFactor?"SF 5 to 12":field==matter::CodingRate?"5 to 8 (4/5 to 4/8)":"-9 to 22 dBm",Font::ChatMeta,FG,W-2*Margin-16);
   c.card(RadioPowerInput);auto tail=c.inputTail(s.radioPowerInput,RadioPowerInput.w-20,Font::ChatBody);c.text(RadioPowerInput.x+8,RadioPowerInput.y+10,s.radioPowerInput.empty()?"Type value...":tail,Font::ChatBody,FG,RadioPowerInput.w-16);g.drawFastVLine(RadioPowerInput.x+9+c.width(tail,Font::ChatBody),RadioPowerInput.y+8,16,FG);
   c.button(RadioPowerSave,"SAVE EDIT");c.button(RadioPowerCancel,"CANCEL");
   c.text(Margin+8,Top+120,s.notice.empty()?"Then APPLY to update LoRa":s.notice,Font::ChatMeta,FG,W-2*Margin-16);
  }
  
 }else if(s.page==Page::Storage){
  c.sectionLabel(Top+6,"SD CARD");valueCard({Margin,Top+22,W-2*Margin,40},"STATUS",s.sd?"Mounted":"Unavailable");
  if(s.sd&&s.storageTotal){c.sectionLabel(Top+72,"SPACE",gb(s.storageTotal)+" total");Rect r={Margin,Top+88,W-2*Margin,52};c.card(r);
   c.text(r.x+7,r.y+5,gb(s.storageUsed)+" used",Font::Bold);auto free=gb(s.storageTotal-s.storageUsed)+" free";c.text(r.x+r.w-7-c.width(free,Font::ChatMeta),r.y+6,free,Font::ChatMeta);
   Rect bar={r.x+7,r.y+30,r.w-14,12};g.drawRoundRect(bar.x,bar.y,bar.w,bar.h,4,FG);int fill=std::max(4,int((bar.w-4)*s.storageUsed/s.storageTotal));c.fillRound(bar.x+2,bar.y+2,fill,bar.h-4,2,FG);}
 }else if(s.page==Page::Maps){
  mapView(s);
  {// floating controls over the map
   auto round=[&](Rect r){c.fillRound(r.x,r.y,r.w,r.h,r.h/2,Surface);g.drawRoundRect(r.x,r.y,r.w,r.h,r.h/2,Line);};
   round(MapBackFloat);c.icon(Icon::Back,MapBackFloat.x+6,MapBackFloat.y+6,16);
   round(MapStyleButton);c.centered(MapStyleButton.x,MapStyleButton.y+5,MapStyleButton.w,s.mapStyle?"NIGHT":"LIGHT",Font::Bold);
   for(int i=0;i<3;i++){Rect r=mapButtonRect(i);bool on=i==1&&s.mapFollow;if(on)c.fillRound(r.x,r.y,r.w,r.h,r.h/2,Accent);else round(r);uint16_t ink=on?OnAccent:FG;int cx=r.x+r.w/2,cy=r.y+r.h/2;
    if(i==1)c.icon(Icon::GPS,cx-8,cy-8,16,ink);else{g.fillRect(cx-6,cy-1,12,2,ink);if(i==2)g.fillRect(cx-1,cy-6,2,12,ink);}}
  }
 }else if(s.page==Page::GPS){
  c.toggleRow(GpsToggle,Icon::GPS,"GPS receiver",!s.gpsAvailable?"Unavailable":s.gpsTemporary?"On for time sync":s.gpsEnabled?"Enabled by user":"Off, saved position kept",s.gpsEnabled&&!s.gpsTemporary,s.gpsTemporary?"KEEP ON":"");
  c.sectionLabel(SettingsSectionY,"STATUS");
  // Landscape: FIX / SATELLITES / RECEIVER on one row, then position + altitude.
  valueCard((settingsThird(0,SettingsBodyY)),"FIX",!s.gpsAvailable?"Unavailable":!s.gpsEnabled?"Off":s.gpsFix?"Position fix":"Searching...");
  valueCard((settingsThird(1,SettingsBodyY)),"SATELLITES",s.gpsEnabled?std::to_string(s.gpsUsed)+" used / "+std::to_string(s.gpsVisible):"--");
  int positionY=(Top+132);
  c.sectionLabel(positionY-16,s.gpsFix?"POSITION":"SAVED POSITION");
  char position[40]="-- / --";if(s.locationValid)snprintf(position,sizeof(position),"%.5f / %.5f",s.latitude,s.longitude);valueCard((settingsThird(0,positionY,2)),"LATITUDE / LONGITUDE",position);
  valueCard((settingsThird(2,positionY)),"ALTITUDE",s.gpsFix?std::to_string(int(s.altitude))+" m":"--");
  valueCard((settingsThird(2,SettingsBodyY)),"RECEIVER",!s.gpsEnabled?"Off":s.gpsNmeaFresh?"NMEA "+std::to_string(s.gpsBaud):s.gpsData?"No fresh NMEA":"No data");
  if(s.gpsTimeValid)c.text(Margin+2,(positionY+46),"GPS date / time available",Font::ChatMeta,FG,W-2*Margin);
 }else if(s.page==Page::Weather){
  {weatherTabs(s);if(s.weatherTab==1){stormsBody(s);return;}if(s.weatherTab==2){radarBody(s);return;}}
  bool data=s.weatherValid,offline=s.weatherPhase==WeatherPhaseView::WifiOff||s.weatherPhase==WeatherPhaseView::NoNetwork;
  auto number=[](float v){return std::to_string(int(std::lround(v)));};
  // Number with a drawn degree ring (the fonts have no degree glyph), centred in w when w>0.
  auto withDegree=[&](int x,int y,int w,const std::string &value,Font font,int ring){int tw=c.width(value,font),total=tw+ring*2+3;if(w>0)x+=(w-total)/2;c.text(x,y,value,font);g.drawCircle(x+tw+ring+2,y+ring+1,ring,FG);};
  std::string place=s.weatherPlace.empty()?(data?"Your area":"Weather"):s.weatherPlace,range=data?"H "+number(s.high)+"  L "+number(s.low):"";int rangeW=range.empty()?0:c.width(range,Font::ChatBody)+8;
  c.text(Margin,WeatherPlaceY,place,Font::ChatTitle,FG,W-2*Margin-rangeW);if(rangeW)c.text(W-Margin-rangeW+8,WeatherPlaceY+3,range,Font::ChatBody);
  c.weatherIcon(data?s.weatherCode:-1,WeatherIconX,WeatherHeroY,64);
  std::string temperature=data?number(s.temperature):"--";Font big=c.width(temperature,Font::LockClock)<=96?Font::LockClock:Font::Clock;
  int tx=WeatherTempX+8;c.text(tx,WeatherHeroY-2,temperature,big);int tw=c.width(temperature,big);if(data){g.drawCircle(tx+tw+6,WeatherHeroY+4,3,FG);g.drawCircle(tx+tw+6,WeatherHeroY+4,2,FG);c.text(tx+tw+12,WeatherHeroY,"C",Font::ChatBody);}
  c.text(WeatherTempX+8,WeatherHeroY+50,data?s.weatherText:"No weather data",Font::ChatBody,FG,WeatherSideX-WeatherTempX-12);// stops before the status / REFRESH column
  if(offline){
   bool wifiOff=s.weatherPhase==WeatherPhaseView::WifiOff;c.card(WeatherPrompt);c.icon(Icon::Wifi,W/2-12,WeatherPrompt.y+WeatherPromptIconDY,24);
   c.centered(Margin,WeatherPrompt.y+WeatherPromptTitleDY,W-2*Margin,wifiOff?"Wi-Fi is off":"No saved Wi-Fi",Font::ChatTitle);
   c.centered(Margin,WeatherPrompt.y+WeatherPromptTextDY,W-2*Margin,wifiOff?"Turn it on to update the weather":"Connect a network in Wi-Fi",Font::ChatMeta);
   c.button(WeatherWifiButton,wifiOff?"TURN ON WI-FI":"WI-FI SETTINGS",true);
  }else{
   Rect hours={Margin,WeatherHoursY,W-2*Margin,WeatherHoursH};c.card(hours);
   if(data&&s.weatherHours>0){int column=hours.w/WeatherHoursColumns;for(int i=0;i<s.weatherHours&&i<WeatherHoursColumns;i++){int x=hours.x+i*column;if(i)g.drawFastVLine(x,hours.y+6,hours.h-12,FG);c.centered(x,hours.y+4,column,s.weatherHourTime[i],Font::ChatMeta);c.weatherIcon(s.weatherHourCode[i],x+column/2-13,hours.y+16,26);withDegree(x,hours.y+43,column,number(s.weatherHourTemp[i]),Font::ChatBody,2);}}
   else c.centered(hours.x,hours.y+21,hours.w,"Next hours after update",Font::ChatMeta);
   const char *labels[]={"Feels like","Humidity","Wind","Pressure"};
   for(int i=0;i<4;i++){Rect cell=weatherCell(i);c.card(cell);c.text(cell.x+(4),cell.y+(2),labels[i],Font::ChatMeta,FG,cell.w-8);std::string value=!data?"--":i==1?number(s.humidity)+"%":i==2?number(s.wind)+" km/h":i==3?number(s.pressure):"";
    int valueY=cell.y+(14);// landscape cells: label above the value
    if(data&&i==0){int vw=c.width(number(s.apparent),Font::ChatBody)+7;withDegree(cell.x+cell.w-6-vw,valueY,0,number(s.apparent),Font::ChatBody,2);}else c.text(cell.x+cell.w-6-c.width(value,Font::ChatBody),valueY,value,Font::ChatBody);}
  }
  std::string updated=s.weatherUpdated;if(updated.size()>10&&updated[10]=='T')updated=updated.substr(11,5);
  std::string status=!s.notice.empty()?s.notice:s.weatherPhase==WeatherPhaseView::Ready?"Updated "+updated+(s.weatherSource.empty()?"":" / "+s.weatherSource):s.weatherPhase==WeatherPhaseView::Error&&data?s.weatherMessage+" / cached":s.weatherMessage.empty()&&data?"Cached "+updated:s.weatherMessage;
  {c.text(WeatherSideX,WeatherStatusY,status,Font::ChatMeta,FG,WeatherSideW);c.text(WeatherSideX,WeatherStatusY+14,"Open-Meteo",Font::ChatMeta);}
  if(!offline){bool busy=s.weatherBusy||s.weatherPhase==WeatherPhaseView::Locating;c.button(WeatherRefresh,busy?"UPDATING...":"REFRESH",busy);}
 }else if(s.page==Page::Wifi&&!s.wifiEditing){
  // Header switch, status line, network rows (lock, name, "Connected", signal), bottom action bar.
  {Rect t=WifiToggle;c.fillRound(t.x,t.y,t.w,t.h,11,s.wifiEnabled?Accent:Surface);c.centered(t.x,t.y+4,t.w,s.wifiEnabled?"ON":"OFF",Font::Bold,s.wifiEnabled?OnAccent:Muted);}
  std::string status=!s.wifiEnabled?"Wi-Fi is off":s.wifiConnected?"Connected to "+s.wifiSSID+(s.wifiIP.empty()?"":" / "+s.wifiIP):s.wifiConnecting?"Connecting...":s.wifiScanning?"Scanning...":s.wifiMessage.empty()?"Not connected":s.wifiMessage;
  int count=s.networks.size();std::string found=count?std::to_string(count)+(count==1?" network":" networks"):"";int fw=c.width(found,Font::ChatMeta);
  c.text(Margin+2,Top+8,status,s.wifiConnected?Font::Bold:Font::ChatMeta,s.wifiConnected?Accent:FG,W-2*Margin-fw-12);if(fw)c.text(W-Margin-fw,Top+9,found,Font::ChatMeta,Muted);
  for(int row=0;row<WifiRows&&s.wifiOffset+row<count;row++){auto &n=s.networks[s.wifiOffset+row];Rect r=wifiRowRect(row);bool on=!s.hideFocus&&s.wifiFocus==s.wifiOffset+row;bool current=s.wifiConnected&&n.ssid==s.wifiSSID;c.card(r,on,8);auto ink=on?OnAccent:FG;
   c.icon(n.secured?Icon::Lock:Icon::Wifi,r.x+8,r.y+(r.h-16)/2,16,on?OnAccent:current?Accent:Muted);
   float snr=n.rssi>=-55?12.f:n.rssi>=-67?6.f:n.rssi>=-78?0.f:-10.f;c.signalBars(r.x+r.w-25,r.y+r.h-9,snr,ink);
   std::string tag=current?"Connected":n.saved?"Saved":(n.secured?"":"Open");int tw=tag.empty()?0:c.width(tag,Font::ChatMeta);if(tw)c.text(r.x+r.w-32-tw,r.y+(r.h-13)/2,tag,Font::ChatMeta,on?OnAccent:current?Accent:Muted);
   c.text(r.x+32,r.y+(r.h-c.lineHeight(Font::ChatBody))/2,n.ssid.empty()?std::string("(hidden)"):n.ssid,Font::ChatBody,ink,r.w-72-tw);}
  if(!count)c.centered(Margin,WifiListY+44,W-2*Margin,!s.wifiEnabled?"Turn Wi-Fi on to search":s.wifiScanning?"Searching for networks...":"No networks yet / tap SCAN",Font::ChatMeta,Muted);
  if(count>WifiRows){int track=WifiRows*WifiRowH-3,thumb=std::max(12,track*WifiRows/count),max=count-WifiRows;c.fillRound(W-4,WifiListY+std::min(s.wifiOffset,max)*(track-thumb)/max,3,thumb,1,Muted);}
  auto pill=[&](Rect r,const char *label,bool accent){c.fillRound(r.x,r.y,r.w,r.h,13,accent?Accent:Surface);c.centered(r.x,r.y+6,r.w,label,Font::Bold,accent?OnAccent:FG);};
  pill(WifiScan,s.wifiScanning?"SCAN...":"SCAN",true);pill(WifiManual,"HIDDEN",false);pill(WifiDisconnect,s.wifiConnected||s.wifiConnecting?"DISCONNECT":"RECONNECT",false);
 }else if(s.page==Page::Wifi){
  
  if(s.wifiEditing){
   g.fillRect(0,Top,W,H-Top,BG);c.sectionLabel(Top+8,s.wifiHidden?"NETWORK NAME (SSID)":"SELECTED NETWORK");c.card(WifiSSIDInput);
   auto name=c.inputTail(s.wifiSSID,WifiSSIDInput.w-16,Font::ChatBody);c.text(WifiSSIDInput.x+7,WifiSSIDInput.y+9,name.empty()?"Type network name":name,Font::ChatBody,FG,WifiSSIDInput.w-14);
   if(s.wifiEditField==0)g.drawFastVLine(WifiSSIDInput.x+8+c.width(name,Font::ChatBody),WifiSSIDInput.y+7,16,FG);
   c.sectionLabel(WifiPassword.y-16,"PASSWORD / EMPTY IF OPEN");c.card(WifiPassword);
   auto tail=c.inputTail(s.wifiPasswordMask,WifiPassword.w-WifiEye.w-17,Font::ChatBody);c.text(WifiPassword.x+7,WifiPassword.y+9,tail,Font::ChatBody,FG,WifiPassword.w-WifiEye.w-14);
   if(s.wifiEditField==1)g.drawFastVLine(WifiPassword.x+8+c.width(tail,Font::ChatBody),WifiPassword.y+7,16,FG);
   g.drawFastVLine(WifiEye.x,WifiEye.y+4,WifiEye.h-8,FG);g.drawRoundRect(WifiEye.x+7,WifiEye.y+10,18,11,5,FG);g.drawCircle(WifiEye.x+16,WifiEye.y+15,2,FG);if(!s.wifiShowPassword)g.drawLine(WifiEye.x+8,WifiEye.y+24,WifiEye.x+24,WifiEye.y+7,FG);
   c.button(WifiConnect,"CONNECT");c.button(WifiCancel,"CANCEL");// one line fits under the buttons: the notice (connection result) or the keys
   c.text(Margin+2,WifiConnect.y+44,s.notice.empty()?"Enter: next field / connect  Alt+V: show password":s.notice,Font::ChatMeta,FG,W-2*Margin);
  }
 }else if(s.page==Page::Bluetooth){
  // MeshCore app link: Bluetooth (passkey below), Wi-Fi (TCP 5000, LAN only) and the USB cable all reach the same companion.
  c.toggleRow(BleToggle,Icon::Bluetooth,"Bluetooth",!s.bleEnabled?"Off":!s.bleReady?"Starting...":s.appBleLinked?"MeshCore app connected":"Visible to the MeshCore app",s.bleEnabled);
  // Bluetooth name: tap the card (or N) to edit the part after "MeshCore-"; ENTER saves, empty = the node name.
  if(s.appBleEditing){Rect r=AppNameCard;c.fillRound(r.x,r.y,r.w,r.h,10,Surface);g.drawRoundRect(r.x,r.y,r.w,r.h,10,Accent);g.drawRoundRect(r.x+1,r.y+1,r.w-2,r.h-2,9,Accent);
   c.text(r.x+7,r.y+4,"BLE NAME / ENTER saves",Font::ChatMeta,Accent,r.w-14);int px=r.x+7;c.text(px,r.y+20,"MeshCore-",Font::Bold,Muted);px+=c.width("MeshCore-",Font::Bold);
   auto tail=c.inputTail(s.appBleInput,r.x+r.w-8-px,Font::Bold);c.text(px,r.y+20,tail,Font::Bold,FG);g.fillRect(px+c.width(tail,Font::Bold)+1,r.y+19,2,15,Accent);}
  else{valueCard(AppNameCard,"BLE NAME / TAP TO EDIT",s.appBleName);}
  valueCard(AppPinCard,"PIN",s.bleEnabled?s.appPin:"-");
  c.toggleRow(AppWifiToggle,Icon::Wifi,"App over Wi-Fi",!s.appWifi?"Off":s.appWifiLinked?"MeshCore app connected":s.appWifiDetail,s.appWifi);
  c.text(Margin+2,AppWifiToggle.y+AppWifiToggle.h+8,s.appUsbLinked?"USB: MeshCore app connected":"USB cable: always available",Font::ChatMeta,FG,W-2*Margin);
  c.text(Margin+2,AppWifiToggle.y+AppWifiToggle.h+24,"MeshCore app: settings and contacts (messages soon).",Font::ChatMeta,Muted,W-2*Margin);
 }else if(s.page==Page::Audio){
  c.toggleRow(AudioNotify,Icon::Messages,"Notification sound",s.audioReady?"Speaker ready":s.audioMessage,s.audioNotifications);
  c.sectionLabel(SettingsSectionY,"VOLUME",std::to_string(s.audioVolume)+"%");for(auto r:{AudioMinus,AudioPlus}){c.card(r);int cx=r.x+r.w/2,cy=r.y+r.h/2;g.fillRect(cx-6,cy-1,12,2,FG);if(r.x==AudioPlus.x)g.fillRect(cx-1,cy-6,2,12,FG);}c.slider(AudioSlider,s.audioVolume);
  c.sectionLabel(AudioToneMessage.y-16,"RINGTONES","TAP TO PLAY");
  auto tone=[&](Rect r,Icon id,const char *label,const std::string &name){c.card(r);c.icon(id,r.x+6,r.y+(r.h-24)/2,24);c.text(r.x+36,r.y+3,label,Font::ChatMeta,FG,r.w-40);
   c.text(r.x+36,r.y+17,name,Font::Bold,FG,r.w-80);c.icon(Icon::Back,r.x+r.w-44,r.y+(r.h-12)/2,12);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12);};
  tone(AudioToneMessage,Icon::Messages,"New message",s.messageTone);tone(AudioToneMention,Icon::Contacts,"Mention @you",s.mentionTone);
  return;}else if(s.page==Page::Power){
  c.sectionLabel(Top+6,"LIGHT TIMERS");
  const char *names[]={"Display / front-light","Keyboard backlight"};for(int i=0;i<2;i++){Rect r=powerRowRect(i);bool focus=s.powerField==i;c.card(r,focus);auto ink=focus?BG:FG;if(i)c.icon(Icon::Keyboard,r.x+6,r.y+(r.h-16)/2,16,ink);else{g.drawRect(r.x+7,r.y+14,16,10,ink);g.drawFastHLine(r.x+12,r.y+27,6,ink);}c.text(r.x+30,r.y+5,names[i],Font::ChatBody,ink,r.w-50);uint32_t seconds=i?s.keyboardTimeout:s.displayTimeout;std::string value=seconds==0?"Never":seconds>=60?std::to_string(seconds/60)+" minute"+(seconds>=120?"s":""):std::to_string(seconds)+" seconds";c.text(r.x+30,r.y+23,value+(seconds==30?" / default":""),Font::ChatMeta,ink,r.w-50);c.icon(Icon::ChevronRight,r.x+r.w-16,r.y+(r.h-12)/2,12,ink);}
  c.sectionLabel(timeoutOptionRect(0).y-16,s.powerField?"KEYBOARD TIMEOUT":"DISPLAY TIMEOUT");const char *labels[]={"15s","30s","1m","2m","5m","Never"};const uint32_t values[]={15,30,60,120,300,0};uint32_t selected=s.powerField?s.keyboardTimeout:s.displayTimeout;
  for(int i=0;i<6;i++){Rect r=timeoutOptionRect(i);c.card(r,selected==values[i]);c.centered(r.x,r.y+(r.h-c.lineHeight(Font::Small))/2,r.w,labels[i],Font::Small,selected==values[i]?BG:FG);}
  int y=timeoutOptionRect(0).y+(36);c.text(Margin+2,y+(0),"Lights wake on key or touch.",Font::ChatMeta,FG,W-2*Margin);c.text(Margin+2,y+(14),"Locks after 30 s or a trackball hold.",Font::ChatMeta,FG,W-2*Margin);
 }else if(s.page==Page::AccentKey){
  // Three choice cards: the active one is ticked, the focused one outlined.
  const char *names[]={"Off","Right Shift","Left Shift"};const char *detail[]={"No accents","Accents in chats","Accents in chats"};
  for(int i=0;i<3;i++){Rect r=accentCardRect(i);bool focus=s.accentFocus==i,active=s.accentMode==i;c.fillRound(r.x,r.y,r.w,r.h,9,Surface);if(focus){g.drawRoundRect(r.x,r.y,r.w,r.h,9,Accent);g.drawRoundRect(r.x+1,r.y+1,r.w-2,r.h-2,8,Accent);}
   c.text(r.x+8,r.y+10,names[i],Font::Bold,FG,r.w-30);auto rows=c.lines(detail[i],r.w-16,Font::ChatMeta);for(int k=0;k<int(rows.size())&&k<2;k++)c.text(r.x+8,r.y+32+k*c.lineHeight(Font::ChatMeta),rows[k],Font::ChatMeta,Muted,r.w-16);
   if(active){g.fillCircle(r.x+r.w-12,r.y+12,7,Accent);c.centered(r.x+r.w-19,r.y+8,14,"v",Font::Bold,OnAccent);}}
  c.sectionLabel(AccentInfoY,"ACCENTS",!s.notice.empty()?upperAscii(s.notice):s.accentMode==s.accentFocus?"ACTIVE":"TAP OR ENTER TO APPLY");// the notice replaces the footer, which would cover the help
  // How the key works, then the letters of the language in use (they change with Settings > Keyboard languages).
  const int tw=W-2*Margin-4;int y=AccentInfoY+18;
  for(auto &line:c.lines(AccentKeyHowTo,tw,Font::ChatBody)){c.text(Margin+2,y,line,Font::ChatBody,FG,tw);y+=c.lineHeight(Font::ChatBody);}
  auto lang=mm_kbd::Lang(s.kbdLang);y+=3;
  const int lh=c.lineHeight(Font::ChatMeta);c.text(Margin+2,y,std::string(mm_kbd::info(lang).name)+" (Settings > Keyboard languages):",Font::ChatMeta,Muted,tw);y+=lh+1;
  int rows=(H-2-y)/lh;if(rows>0)for(auto &line:c.clampLines(mm_kbd::accentHelp(lang),tw,Font::ChatMeta,rows,"..")){c.text(Margin+2,y,line,Font::ChatMeta,FG,tw);y+=lh;}
 }else if(s.page==Page::TextSize){
  // Two sample cards drawn with their own face; the active one is ticked, the focused one outlined.
  const char *names[]={"Compact","Large"};const char *detail[]={"11 px / 16 px rows","12 px / 17 px rows"};bool large=chatTextLarge();
  for(int i=0;i<2;i++){Rect r=textSizeCardRect(i);bool focus=s.textSizeFocus==i,active=s.textSize==i;c.fillRound(r.x,r.y,r.w,r.h,9,Surface);if(focus){g.drawRoundRect(r.x,r.y,r.w,r.h,9,Accent);g.drawRoundRect(r.x+1,r.y+1,r.w-2,r.h-2,8,Accent);}
   c.text(r.x+10,r.y+8,names[i],Font::Bold);c.text(r.x+10,r.y+24,detail[i],Font::ChatMeta,Muted,r.w-20);
   setChatTextLarge(i==1);Rect bubble={r.x+10,r.y+42,r.w-20,r.h-50};c.fillRound(bubble.x,bubble.y,bubble.w,bubble.h,8,SurfaceAlt);auto rows=c.lines("Meet at the ridge at 10:30, bring the radio kit.",bubble.w-14,Font::ChatText);
   for(int k=0;k<int(rows.size())&&k<3;k++)c.text(bubble.x+7,bubble.y+6+k*c.lineHeight(Font::ChatText),rows[k],Font::ChatText);
   if(active){g.fillCircle(r.x+r.w-12,r.y+12,7,Accent);c.centered(r.x+r.w-19,r.y+8,14,"v",Font::Bold,OnAccent);}}
  setChatTextLarge(large);
  c.sectionLabel(TextSizeInfoY,"CHAT TEXT",s.textSize==s.textSizeFocus?"ACTIVE":"TAP OR ENTER TO APPLY");
  c.text(Margin+2,TextSizeInfoY+18,"Bubbles, composer and chat dialogs",Font::ChatBody,FG,W-2*Margin-4);c.text(Margin+2,TextSizeInfoY+36,"Lists and settings keep their size",Font::ChatMeta,Muted,W-2*Margin-4);
 }else if(s.page==Page::Wallpaper){
  {for(int i=0;i<ThemeCount;i++){Rect r=wallpaperCardRect(i);bool focus=s.wallpaperFocus==i,active=s.wallpaper==i;auto &t=theme(i);
    c.fillRound(r.x,r.y,r.w,r.h,9,Surface);if(focus){g.drawRoundRect(r.x,r.y,r.w,r.h,9,Accent);g.drawRoundRect(r.x+1,r.y+1,r.w-2,r.h-2,8,Accent);}
    int tw=80,th=60,tx=r.x+(r.w-tw)/2,ty=r.y+6;if(const uint16_t *thumb=lockThumb(i))c.image(tx,ty,thumb,tw,th);
    g.fillRect(tx,ty,tw,5,t.statusBg);
    c.centered(r.x+2,ty+th+6,r.w-4,t.name,Font::Bold,FG);
    const uint16_t swatch[]={t.bg,t.surface,t.accent,t.fg};for(int k=0;k<4;k++){int cx=r.x+r.w/2-27+k*18,cy=r.y+r.h-14;g.fillCircle(cx,cy,6,swatch[k]);g.drawCircle(cx,cy,6,Line);}
    if(active){g.fillCircle(r.x+r.w-12,r.y+12,7,Accent);c.centered(r.x+r.w-19,r.y+8,14,"v",Font::Bold,OnAccent);}}}
  {int sel=s.wallpaperFocus>=0&&s.wallpaperFocus<ThemeCount?s.wallpaperFocus:0;
   c.sectionLabel(WallpaperInfoY,"THEME",s.wallpaper==sel?"ACTIVE":"TAP OR ENTER TO APPLY");
   c.text(Margin+2,WallpaperInfoY+18,theme(sel).name,Font::ChatBody,FG,W-2*Margin-4);c.text(Margin+2,WallpaperInfoY+36,std::string(theme(sel).detail)+" / Matterhorn lock screen",Font::ChatMeta,Muted,W-2*Margin-4);}
 }else if(s.page==Page::Info){
  int systemY=Top+148;// landscape: Top+82, rows 13 px
  {// logo beside the name: the system card keeps its seven rows
   g.drawBitmap(Margin,Top+6,mm_logo::logo56,mm_logo::size56,mm_logo::size56,FG);int x=Margin+66,w=W-Margin-x;
   c.text(x,Top+8,"MATTERMESH OS",Font::Brand,FG,w);c.text(x,Top+34,"Version "+s.version,Font::ChatBody,FG,w);c.text(x,Top+50,"By Kinta",Font::Bold,Accent,w);systemY=Top+82;
  }
  c.sectionLabel(systemY-16,"SYSTEM");Rect r={Margin,systemY,W-2*Margin,Bottom-systemY};c.card(r);
  const char *labels[]={"MeshCore","Device","Radio","Build","Commit","Memory","Hardware"};const std::string values[]={s.meshCoreVersion,DeviceLabel,s.radioReady?"SX1262 / MeshCore":"SX1262 / offline",s.build,s.commit,std::to_string(s.flashMB)+" MB / "+std::to_string(s.heapKB)+" KB free",s.touch&&s.keyboard?("Touch, keys, trackball OK"):std::string(s.touch?"":"touch ")+(s.keyboard?"":"keys ")+"missing"};
  for(int i=0;i<7;i++){int y=r.y+4+i*(13);c.text(r.x+7,y,labels[i],Font::ChatMeta);c.text(r.x+72,y,values[i],Font::ChatMeta,FG,r.w-79);}
 }else if(s.page==Page::Support){
  // Donate / Extra apps tabs, then the QR of the selected one (black on a white quiet zone of 2 modules on every theme,
  // so any phone reads it) and its text: beside the QR on the Plus, below it on the Max. The URLs are constants: each
  // QR is encoded once.
  static std::vector<uint8_t> donateQr,unlockQr;static const int donateN=encodeQr(DonateUrl,donateQr),unlockN=encodeQr(UnlockUrl,unlockQr);
  const char *tabs[]={"DONATE","EXTRA APPS"};for(int i=0;i<2;i++)c.button(supportTabRect(i),tabs[i],s.supportTab==i);
  const bool unlock=s.supportTab==1;const std::vector<uint8_t> &modules=unlock?unlockQr:donateQr;
  const int n=unlock?unlockN:donateN,room=(Bottom-SupportQrY),scale=n?std::min(4,room/(n+4)):0,side=(n+4)*scale;
  const int x0=Margin,y0=SupportQrY;
  const uint16_t qrLight=rgb(255,255,255),qrDark=rgb(0,0,0);
  if(n){g.fillRect(x0,y0,side,side,qrLight);for(int y=0;y<n;y++)for(int x=0;x<n;x++)if(modules[y*n+x])g.fillRect(x0+(x+2)*scale,y0+(y+2)*scale,scale,scale,qrDark);}
  int x=n?x0+side+12:Margin,w=W-Margin-x,y=SupportQrY+2;
  auto para=[&](const std::string &text,Font f,uint16_t ink,int step){for(auto &line:c.lines(text,w,f)){c.text(x,y,line,f,ink,w);y+=step;}};
  if(unlock){para("RPT Scanner, RPT Diagnostic, RX Log, Noise Floor.",Font::ChatMeta,Muted,14);
   y+=6;c.text(x,y,"Your unlock code",Font::ChatMeta,FG,w);c.text(x,y+15,s.unlockCode,Font::ChatTitle,Accent,w);y+=40;para("Buy on Ko-fi, then redeem at mattermeshos.com/unlock",Font::ChatMeta,Muted,14);}
  else{{para("Support MatterMesh OS",Font::ChatTitle,FG,20);y+=2;}para("MatterMesh OS is free. A donation of any amount on Ko-fi helps hardware and field tests.",Font::ChatMeta,Muted,14);y+=6;para("ko-fi.com/mattermeshos",Font::ChatMeta,FG,14);}
 }else if(s.page==Page::Battery){
  // Landscape: icon and percentage on the left, the three detail cards stacked on the right.
  int half=(W/2);
  c.icon(Icon::Battery,(half-32)/2,(Top+40),32);c.centered(Margin,(Top+80),half-2*Margin,s.status.battery<0?"--%":std::to_string(s.status.battery)+"%",Font::Clock);
  
  valueCard((Rect{W/2+3,Top+8,W/2-3-Margin,40}),"VOLTAGE",s.voltage<0?"--":std::to_string(s.voltage)+" mV");
  valueCard((Rect{W/2+3,Top+56,W/2-3-Margin,40}),"STATE",s.batteryFull?"Full":s.batteryCharging||s.status.charging?"Charging":"On battery");
  valueCard((Rect{W/2+3,Top+104,W/2-3-Margin,40}),"SOURCE",s.batteryEstimated?"Voltage estimate":"BQ27220 fuel gauge");
 }else if(s.page==Page::Files){
  c.text(Margin,Top+4,s.sd?"SD CARD / ROOT":"NO SD CARD",Font::ChatMeta);
  if(s.files.empty())c.centered(Margin,Top+55,W-2*Margin,"No files available",Font::Body);
  int start=s.scroll;for(int row=0;row<FileRows&&start+row<int(s.files.size());row++){Rect r=fileRowRect(row);c.card(r);auto name=s.files[start+row];bool dir=name.rfind("[DIR] ",0)==0;if(dir)name=name.substr(6);c.icon(dir?Icon::Files:Icon::Notes,r.x+6,r.y+(r.h-24)/2,24);c.text(r.x+37,r.y+3,name,Font::ChatBody,FG,r.w-45);c.text(r.x+37,r.y+18,dir?"Folder / read-only":"File / read-only",Font::ChatMeta);}
 }else if(s.page==Page::Terminal){
  c.sectionLabel(Top+6,"SYSTEM");valueCard((settingsThird(0,Top+22)),"MEMORY / STACK",std::to_string(s.heapKB)+" KB / "+std::to_string(s.stackFree)+" B");
  valueCard((settingsThird(1,Top+22)),"CPU",std::to_string(s.cpuMHz)+" MHz");valueCard((settingsThird(2,Top+22)),"MESH RX / TX",std::to_string(s.radioPacketsRX)+" / "+std::to_string(s.radioPacketsTX));
  valueCard((Rect{Margin,Top+68,W-2*Margin,40}),"UI / LCD TIME",std::to_string(s.rasterMs)+" / "+std::to_string(s.paintMs)+" ms");
  int usbY=(Top+118);c.sectionLabel(usbY,"USB SERIAL");c.text(Margin+2,usbY+18,"ui stats / radio status / status",Font::ChatMeta,FG,W-2*Margin);
 }else if(s.page==Page::Keyboard){
  // Search on top, one switch per language, how to switch and how to reach the extra letters below.
  c.card(KeyboardSearch);auto tail=c.inputTail(s.kbdQuery,KeyboardSearch.w-40,Font::ChatBody);
  c.text(KeyboardSearch.x+7,KeyboardSearch.y+5,s.kbdQuery.empty()?(s.kbdSearching?"Type a language...":"Search languages"):tail,Font::ChatBody,s.kbdQuery.empty()?Muted:FG,KeyboardSearch.w-36);
  if(s.kbdSearching)g.drawFastVLine(KeyboardSearch.x+8+c.width(tail,Font::ChatBody),KeyboardSearch.y+5,16,FG);c.icon(Icon::Search,KeyboardSearch.x+KeyboardSearch.w-22,KeyboardSearch.y+5,16,Muted);
  auto rows=mm_kbd::rows(s.kbdQuery.c_str());
  int focus=rows.empty()?0:std::min(std::max(s.kbdFocus,0),int(rows.size())-1),first=mm_kbd::firstRow(focus,int(rows.size()),KeyboardRows);
  for(int i=0;i<KeyboardRows&&first+i<int(rows.size());i++){int id=rows[first+i];auto &info=mm_kbd::info(mm_kbd::Lang(id));Rect r=keyboardRowRect(i);bool on=s.kbdMask&(1u<<id);
   std::string detail=on&&id==s.kbdLang&&mm_kbd::enabledCount(s.kbdMask)>1?std::string("In use (")+info.code+")":std::string(info.code)+(std::string(info.english)!=info.name?std::string(" / ")+info.english:"");
   c.card(r);c.icon(Icon::Keyboard,r.x+6,r.y+(r.h-24)/2,24,FG);const int pw=44,px=r.x+r.w-pw-6,py=r.y+(r.h-22)/2,ty=r.y+(r.h-c.lineHeight(Font::ChatBody))/2+1;
   c.text(r.x+36,ty,info.name,Font::ChatBody,FG,px-r.x-44);int nx=r.x+36+c.width(info.name,Font::ChatBody)+8;if(nx<px-12)c.text(nx,ty+1,detail,Font::ChatMeta,Muted,px-nx-8);
   if(on)c.fillRound(px,py,pw,22,11,Accent);else g.drawRoundRect(px,py,pw,22,11,Muted);c.centered(px,py+5,pw,on?"ON":"OFF",Font::Bold,on?OnAccent:FG);
   if(first+i==focus&&!s.kbdSearching&&!s.hideFocus)g.drawRoundRect(r.x+1,r.y+1,r.w-2,r.h-2,Radius,Accent);}
  if(rows.empty())c.centered(Margin,Top+80,W-2*Margin,"No language matches",Font::ChatBody);
  // One line (the notice, or how Alt + Space works), then the accent help of the focused language (keyboardHelp).
  int y=KeyboardHelpY;const int lh=c.lineHeight(Font::ChatMeta),tw=W-2*Margin-4;
  if(!s.notice.empty())c.text(Margin+2,y,s.notice,Font::ChatMeta,Accent,tw);else c.text(Margin+2,y,keyboardHowto(s),Font::ChatMeta,FG,tw);y+=lh;
  int helpRows=(H-2-y)/lh;if(helpRows>0&&!rows.empty())for(auto &line:c.clampLines(keyboardHelp(s,rows[focus]),tw,Font::ChatMeta,helpRows,"..")){c.text(Margin+2,y,line,Font::ChatMeta,Muted);y+=lh;}
 }else if(s.page==Page::DateTime){
  valueCard({Margin,Top+6,W-2*Margin,40},"NOW",s.status.clock+"   "+s.date);
  c.text(Margin+2,Top+49,"Synchronised from: "+s.timeSource,Font::ChatMeta,Muted,W-2*Margin);// ends above the SET BY HAND label
  c.sectionLabel(DateTimeBoxY-16,"SET BY HAND","DD / MM / YYYY  HH:MM");
  for(int i=0;i<5;i++){Rect r=dateTimeBox(i);bool on=i==s.dtFocus;c.card(r,on,8);auto ink=on?OnAccent:FG;char v[8];snprintf(v,sizeof(v),i==2?"%04d":"%02d",s.dtField[i]);
   c.centered(r.x,r.y+(r.h-c.lineHeight(Font::ChatTitle))/2,r.w,v,Font::ChatTitle,ink);int cx=r.x+r.w/2;auto tri=on?OnAccent:Muted;
   g.fillTriangle(cx-5,r.y+9,cx+5,r.y+9,cx,r.y+4,tri);g.fillTriangle(cx-5,r.y+r.h-9,cx+5,r.y+r.h-9,cx,r.y+r.h-4,tri);
   if(i==0||i==1){Rect n=dateTimeBox(i+1);c.centered(r.x+r.w,r.y+r.h/2-8,n.x-r.x-r.w,"/",Font::ChatBody,Muted);}if(i==3){Rect n=dateTimeBox(4);c.centered(r.x+r.w,r.y+r.h/2-9,n.x-r.x-r.w,":",Font::ChatTitle,FG);}}
  c.fillRound(DateTimeSet.x,DateTimeSet.y,DateTimeSet.w,DateTimeSet.h,14,Accent);c.centered(DateTimeSet.x,DateTimeSet.y+7,DateTimeSet.w,"SET DATE AND TIME",Font::Bold,OnAccent);
 }else if(s.page==Page::Updates){
  // OTA update: installed build and server status side by side (Plus landscape) or stacked, progress, action.
  const int cardsY=(Top+6);// Plus: no section label, room for the release notes
  Rect installed=(settingsHalf(0,cardsY)),status=(settingsHalf(1,cardsY));
  valueCard(installed,"VERSION",s.version);valueCard(status,"STATUS",s.otaStatus);
  int y=((installed.y+installed.h))+8;
  if(s.otaProgress>=0){Rect bar={Margin,y,W-2*Margin,12};g.drawRoundRect(bar.x,bar.y,bar.w,bar.h,4,Line);int fill=std::max(4,(bar.w-4)*std::min(s.otaProgress,100)/100);c.fillRound(bar.x+2,bar.y+2,fill,bar.h-4,2,Accent);y+=18;}
  if(!s.otaNotes.empty()&&s.otaProgress<0){
   // "What's new": the release notes as a list, wrapped, as many lines as fit above the button.
   Rect card={Margin,y,W-2*Margin,UpdatesAction.y-6-y};c.card(card);c.text(card.x+8,card.y+5,s.otaNotesTitle,Font::ChatMeta,Accent);
   int ly=card.y+22,lh=c.lineHeight(Font::ChatMeta);
   for(auto &item:s.otaNotes){auto lines=c.lines(item,card.w-30,Font::ChatMeta);for(size_t k=0;k<lines.size();k++){if(ly+lh>card.y+card.h-2)break;if(!k)g.fillCircle(card.x+12,ly+lh/2,2,Accent);c.text(card.x+20,ly,lines[k],Font::ChatMeta,FG,card.w-28);ly+=lh;}}
  }else c.text(Margin+2,y,s.otaDetail,Font::ChatMeta,FG,W-2*Margin);
  if(!s.otaAction.empty())c.button(UpdatesAction,s.otaAction,s.otaConfirm);
 }else {
  Icon id=s.page==Page::Wifi?Icon::Wifi:s.page==Page::Bluetooth?Icon::Bluetooth:s.page==Page::Terminal?Icon::Terminal:Icon::Tools;
  c.icon(id,(W-32)/2,Top+24,32);
  std::string line=s.page==Page::Terminal?"USB diagnostics available":s.page==Page::Updates?"No update service configured":"Connection not configured";
  c.centered(Margin,Top+80,W-2*Margin,line,Font::Bold);
  if(s.page==Page::Terminal){c.text(Margin,Top+112,"status / shot / home / apps",Font::Body);c.text(Margin,Top+134,"Existing USB commands preserved",Font::Small);}
 }
 if(!s.notice.empty()&&s.page!=Page::Radio&&s.page!=Page::Weather&&s.page!=Page::Wifi&&s.page!=Page::Keyboard&&s.page!=Page::AccentKey){c.footer(s.notice,"");}// Keyboard and Accent key draw their notice above the help
}
#include "quick_settings_renderer.inc"
#include "cli_renderer.inc"
#include "weather_apps_renderer.inc"
void Renderer::draw(const State &s){switch(s.page){
 case Page::Cli:cliApp(s);break;
 case Page::Home:home(s);break;case Page::Launcher:launcher(s);break;case Page::Messages:messages(s);break;case Page::Chat:chat(s);break;case Page::NewChat:form(s);break;case Page::ContactPicker:contactPicker(s);break;case Page::Settings:settings(s);break;case Page::QuickSettings:quickSettings(s);break;default:section(s);}if(s.channelOptions)channelOptions(s);}
}
