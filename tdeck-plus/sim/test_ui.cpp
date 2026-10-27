#include <vector>
#include <cassert>
#include <iostream>
#include <cstring>
#include "ui/renderer.h"
#include "ui/navigation_catalog.h"
#include "ui/utf8.h"
#include "ui/composer_caret.h"
#include "ui/emoji_choices.h"
#include "ui/fast_canvas.h"
int main(){
 mm::FastCanvas16 canvas(mm::W,mm::H);mm::Components c(canvas);
 auto shot=[&]{return std::vector<uint16_t>(canvas.getBuffer(),canvas.getBuffer()+mm::W*mm::H);};auto px=[&](int x,int y){return canvas.getPixel(x,y);};
 assert(mm::senderDisplayName("Ridge_L1 🇮🇹")=="Ridge_L1");assert(mm::senderDisplayName("IT Field 🙂")=="IT Field 🙂");
 assert(mm::mentionDisplayText("@[🇮🇹 Oreste IW1QQG] Hi")=="@[Oreste IW1QQG] Hi");assert(mm::mentionDisplayText("@[Oreste 🇮🇹")=="@[Oreste");assert(mm::mentionDisplayText("Weather 🇮🇹") == "Weather 🇮🇹");
 for(int i=0;i<mm::ApplicationCount;i++){auto r=mm::appRect(i);assert(r.x>=0&&r.y>=mm::StatusH&&r.x+r.w<=mm::W&&r.y+r.h<=mm::Bottom);for(int j=i+1;j<mm::ApplicationCount;j++){auto q=mm::appRect(j);assert(r.x+r.w<=q.x||q.x+q.w<=r.x||r.y+r.h<=q.y||q.y+q.h<=r.y);}}
 assert(mm::MenuHomeTabCount==4&&mm::MenuHomeTabIcon==24&&mm::MenuHomeTabH==38);// Plus: compact 38 px tabs, 24 px icon over the label
 assert(mm::HomeTabs[2].section==19);for(auto app:mm::Applications)assert(app.section!=19&&app.section!=0&&app.section!=1);
 assert(mm::menuHomeRowRect(mm::MenuHomeRows-1).y+mm::MenuHomeRowH<mm::MenuHomeTabY);
 for(int i=0;i<mm::MenuHomeTabCount;i++){auto area=mm::menuHomeTabRect(i);assert(area.x>=mm::Margin&&area.x+area.w<=mm::W-mm::Margin&&area.y+area.h<=mm::H);assert(mm::HomeTabs[i].section!=9&&mm::HomeTabs[i].section!=10);if(i)assert(mm::menuHomeTabRect(i-1).x+area.w+mm::MenuHomeTabGap==area.x);}
 bool weather=false;for(auto app:mm::Applications)weather|=app.section==10;assert(weather&&mm::HomeTabs[3].section<0&&std::string(mm::HomeTabs[3].label)=="Apps");// Plus label
 mm::Status status;status.mesh=false;c.statusBar(status);auto noSignal=shot();status.mesh=true;c.statusBar(status);assert(shot()==noSignal);
 for(int i=0;i<3;i++){auto r=mm::formRect(i);assert(r.y>=mm::Top&&r.y+r.h<mm::SaveButton.y);}
 assert(mm::SaveButton.y+mm::SaveButton.h<mm::Bottom);
 assert(mm::LoRaAdvert.y+mm::LoRaAdvert.h<mm::Bottom&&mm::LoRaApply.y+mm::LoRaApply.h<=mm::LoRaAdvert.y&&mm::LoRaAdvert.x+mm::LoRaAdvert.w<=mm::W-mm::Margin);// the Plus advert button (LoRa page)
 for(int i=0;i<mm::WifiRows;i++){auto r=mm::wifiRowRect(i);assert(r.y+r.h<=mm::Bottom);}
 for(int i=0;i<mm::FileRows;i++){auto r=mm::fileRowRect(i);assert(r.y+r.h<=mm::Bottom);}
 // Plus Weather: status + source + REFRESH in the side column beside the temperature, hours strip, the four cells on one row at the bottom;
 // the no-data prompt replaces hours + cells (the Max status line under the grid does not exist here).
 assert(mm::WeatherStatusY+2*mm::Components::lineHeight(mm::Font::ChatMeta)<=mm::WeatherRefresh.y&&mm::WeatherRefresh.x==mm::WeatherSideX&&mm::WeatherSideX+mm::WeatherSideW<=mm::W-mm::Margin&&mm::WeatherRefresh.y+mm::WeatherRefresh.h<=mm::WeatherHoursY);
 assert(mm::WeatherHeroY+64<=mm::WeatherHoursY&&mm::WeatherHoursY+mm::WeatherHoursH<mm::WeatherGridY&&mm::WeatherPrompt.y>=mm::WeatherHoursY&&mm::WeatherPrompt.y+mm::WeatherPrompt.h<=mm::Bottom);
 assert(mm::WeatherWifiButton.y+mm::WeatherWifiButton.h<mm::WeatherPrompt.y+mm::WeatherPrompt.h&&mm::WeatherWifiButton.x>=mm::WeatherPrompt.x&&mm::WeatherWifiButton.x+mm::WeatherWifiButton.w<=mm::WeatherPrompt.x+mm::WeatherPrompt.w);
 for(int i=0;i<4;i++){auto r=mm::weatherCell(i);assert(r.x>=mm::Margin&&r.x+r.w<=mm::W-mm::Margin&&r.y+r.h<=mm::Bottom);if(i)assert(mm::weatherCell(i-1).x+mm::weatherCell(i-1).w<r.x);}
 for(int i=0;i<5;i++){auto r=mm::ownMessageActionRect(i);assert(r.x>=mm::Margin&&r.y>=mm::Top&&r.x+r.w<=mm::W-mm::Margin&&r.y+r.h<=mm::Bottom);for(int j=i+1;j<5;j++){auto q=mm::ownMessageActionRect(j);assert(r.x+r.w<=q.x||q.x+q.w<=r.x||r.y+r.h<=q.y||q.y+q.h<=r.y);}}
 for(int i=0;i<mm::ContactRows;i++){auto r=mm::contactRowRect(i);assert(r.y>=mm::ContactSearch.y+mm::ContactSearch.h&&r.y+r.h<=mm::Bottom);}// Plus: no pager row below the list
 for(int i=0;i<6;i++){auto r=mm::timeoutOptionRect(i);assert(r.x>=0&&r.y>=mm::Top&&r.x+r.w<=mm::W&&r.y+r.h<mm::Bottom);}
 std::string longWord(96,'W');auto lines=c.lines(longWord,176);std::string result;for(auto &line:lines){assert(c.width(line)<=176);result+=line;}assert(result==longWord);
 // The byte counter appears only after95 and never occupies composer pixels.
 {mm::State state;state.page=mm::Page::Chat;state.messageLimit=142;state.input=std::string(95,'x');mm::Renderer renderer(canvas);renderer.chat(state);
  for(int y=mm::FirstRepeaterY;y<mm::ComposerY;y++)for(int x=0;x<mm::W;x++)assert(px(x,y)==mm::BG);
  state.input+='x';renderer.chat(state);bool ink=false;for(int y=mm::FirstRepeaterY;y<mm::ComposerY;y++)for(int x=mm::W-70;x<mm::W;x++)ink=ink||px(x,y)!=mm::BG;assert(ink);
  auto pixels=shot();state.messageLimit=160;renderer.chat(state);
  assert(std::equal(pixels.begin()+mm::ComposerY*mm::W,pixels.end(),canvas.getBuffer()+mm::ComposerY*mm::W));
  state.input.pop_back();renderer.chat(state);for(int y=mm::FirstRepeaterY;y<mm::ComposerY;y++)for(int x=0;x<mm::W;x++)assert(px(x,y)==mm::BG);
 }
 // Dictated draft (the Plus composer wraps and draws drafts in Font::ChatText, caret = 2 px Accent bar): the composer expands upward, shows the mention typed first and every row; typing alone stays single-line.
 {mm::State state;state.page=mm::Page::Chat;state.messageLimit=160;std::string mention="@[Aria] ";state.input=mention+"questo e un messaggio dettato abbastanza lungo da occupare parecchie righe del compositore per controllare gli errori";
  mm::Renderer renderer(canvas);renderer.chat(state);assert(renderer.chatComposerTop==mm::ComposerY);
  auto tail=c.inputTail(state.input,mm::ComposerEmoji.x-(mm::Margin+7)-5-3,mm::Font::ChatText);assert(tail.find(mention)==std::string::npos);// single line shows only the end
  state.composerExpanded=true;renderer.chat(state);auto rows=c.lines(state.input,mm::ComposerEmoji.x-(mm::Margin+7)-5-3,mm::Font::ChatText);int bodyH=c.lineHeight(mm::Font::ChatText);
  assert(rows.size()>=3&&rows.size()<=size_t(mm::ComposerMaxLines)&&rows[0].rfind(mention,0)==0);
  assert(renderer.chatComposerTop==mm::H-(mm::ComposerH+(int(rows.size())-1)*bodyH));
  bool ink=false;for(int y=renderer.chatComposerTop+9;y<renderer.chatComposerTop+9+bodyH;y++)for(int x=mm::Margin+7;x<mm::Margin+60;x++)ink=ink||px(x,y)!=mm::BG;assert(ink);// first row (mention) painted
  ink=false;for(int y=renderer.chatComposerTop-mm::FirstRepeaterH;y<renderer.chatComposerTop;y++)for(int x=mm::W-70;x<mm::W;x++)ink=ink||px(x,y)!=mm::BG;assert(ink);// byte counter moved up with the card
  state.input=std::string(400,'a');renderer.chat(state);assert(renderer.chatComposerTop==mm::H-(mm::ComposerH+(mm::ComposerMaxLines-1)*bodyH));// capped
  state.emojiPicker=true;renderer.chat(state);assert(renderer.chatComposerTop==mm::ComposerY);// picker needs the fixed composer
 }
 // Composer caret: typed drafts expand too; the caret follows the cursor byte offset across wrapped rows and the visible window follows it.
 {using namespace mm;
  {std::vector<std::string> rows={"ciao","mondo"};std::string value="ciao mondo";
   auto p=caretPlace(rows,value,0);assert(p.row==0&&p.column==0);p=caretPlace(rows,value,4);assert(p.row==0&&p.column==4);
   p=caretPlace(rows,value,5);assert(p.row==1&&p.column==0);p=caretPlace(rows,value,7);assert(p.row==1&&p.column==2);p=caretPlace(rows,value,10);assert(p.row==1&&p.column==5);p=caretPlace(rows,value,99);assert(p.row==1&&p.column==5);}
  {std::vector<std::string> rows={"a b","c"};std::string value="a  b \nc";// collapsed double space, dropped separators
   auto p=caretPlace(rows,value,2);assert(p.row==0&&p.column==2);p=caretPlace(rows,value,3);assert(p.row==0&&p.column==2);p=caretPlace(rows,value,4);assert(p.row==0&&p.column==3);p=caretPlace(rows,value,6);assert(p.row==1&&p.column==0);}
  {std::vector<std::string> none;auto p=caretPlace(none,"",0);assert(p.row==0&&p.column==0);}
  {std::vector<std::string> rows={"ciao"};auto p=caretPlace(rows,"ciao ",5);assert(p.row==0&&p.column==4&&p.space);p=caretPlace(rows,"ciao  ",6);assert(p.column==4&&p.space);p=caretPlace(rows,"ciao ",4);assert(p.column==4&&!p.space);// trailing space: caret one blank further
   std::vector<std::string> two={"ciao","mondo"};p=caretPlace(two,"ciao mondo",5);assert(p.row==1&&p.column==0&&!p.space);}
  assert(caretWindowStart(3,6,0)==0&&caretWindowStart(9,6,8)==3&&caretWindowStart(9,6,3)==3&&caretWindowStart(9,6,1)==1&&caretWindowStart(9,6,0)==0);
  mm::State state;state.page=mm::Page::Chat;state.messageLimit=160;state.input="ciao mondo";state.composerExpanded=true;state.inputCursor=std::string::npos;
  mm::Renderer renderer(canvas);renderer.chat(state);assert(renderer.chatComposerTop==mm::ComposerY);// one row: the card keeps its height
  auto caretColumn=[&](int y0,int y1){int found=-1;for(int x=mm::Margin+7;x<mm::ComposerEmoji.x-2&&found<0;x++){bool full=true;for(int y=y0;y<y1;y++)full=full&&px(x,y)==mm::Accent;if(full)found=x;}return found;};
  int endX=caretColumn(mm::ComposerY+9,mm::ComposerY+9+15);assert(endX>mm::Margin+7+c.width("ciao mond",mm::Font::ChatText));
  state.inputCursor=4;renderer.chat(state);int midX=caretColumn(mm::ComposerY+9,mm::ComposerY+9+15);assert(midX==mm::Margin+7+c.width("ciao",mm::Font::ChatText)+1&&midX<endX);
  state.inputCursor=0;renderer.chat(state);assert(caretColumn(mm::ComposerY+9,mm::ComposerY+9+15)==mm::Margin+8);
  // Long typed draft: 9 rows, caret on the second row -> the window starts there and the caret is on the first visible row.
  std::string longText;for(int i=0;i<40;i++)longText+="parola ";state.input=longText;state.inputCursor=std::string::npos;renderer.chat(state);int bodyH=c.lineHeight(mm::Font::ChatText);
  auto rows=c.lines(state.input,mm::ComposerEmoji.x-(mm::Margin+7)-5-3,mm::Font::ChatText);assert(int(rows.size())>mm::ComposerMaxLines);assert(renderer.chatComposerTop==mm::H-(mm::ComposerH+(mm::ComposerMaxLines-1)*bodyH));
  int top=renderer.chatComposerTop;assert(caretColumn(top+9+(mm::ComposerMaxLines-1)*bodyH,top+9+(mm::ComposerMaxLines-1)*bodyH+15)>0);// caret on the last visible row
  size_t secondRow=rows[0].size()+1;state.inputCursor=secondRow;renderer.chat(state);assert(renderer.chatComposerTop==top);assert(caretColumn(top+9,top+9+15)==mm::Margin+8);// caret at the start of the first visible row (row 1 of the draft)
  bool firstRowInk=false;for(int y=top+9;y<top+9+bodyH;y++)for(int x=mm::Margin+7;x<mm::Margin+40;x++)firstRowInk=firstRowInk||px(x,y)!=mm::BG;assert(firstRowInk);
  // Emoji picker open: single line shows the text up to the caret with the caret after it, then the rest.
  state.input="ciao mondo";state.inputCursor=4;state.emojiPicker=true;renderer.chat(state);assert(caretColumn(mm::ComposerY+9,mm::ComposerY+9+15)==mm::Margin+7+c.width("ciao",mm::Font::ChatText)+1);
  std::cout<<"PASS: composer caret placement, typed draft expansion, caret window, picker single line\n";
 }
 // Removed: RefreshTracker no-op/partial/full planning (Max e-paper 1-bit refresh tiles and waveforms; the Plus LCD blits whole frames).
 mm::State s;s.page=mm::Page::Chat;s.chatName="Field Team";s.scope="it-vda";s.messages.push_back({"River","10:18",longWord,"3 hop | it-vda",false});mm::Renderer r(canvas);r.draw(s);
 {
  mm::State all;for(int i=0;i<4;i++){mm::Conversation item;item.id=i;item.name=i==3?"Cervinia":"Chat "+std::to_string(i);all.conversations.push_back(item);}
  all.scroll=1;r.home(all);auto scrolled=shot();
  auto expected=all;expected.scroll=0;expected.conversations.erase(expected.conversations.begin());r.home(expected);
  assert(all.conversations.size()==4&&all.conversations[3].name=="Cervinia");
  for(int row=0;row<mm::MenuHomeRows;row++){auto box=mm::menuHomeRowRect(row);for(int y=box.y;y<box.y+box.h;y++)for(int x=box.x;x<box.x+box.w;x++)assert(scrolled[y*mm::W+x]==px(x,y));}
 }
 for(auto page:{mm::Page::Weather,mm::Page::Files,mm::Page::Mesh,mm::Page::Settings}){s.page=page;r.draw(s);assert(px(0,mm::FooterY)==mm::BG);}// no footer rule across the bottom
 s.lockHasMessage=true;s.receivedName="Ridge_L1 🇮🇹";r.lock(s);auto lockPixels=shot();assert(s.receivedName=="Ridge_L1 🇮🇹");s.receivedName="Ridge_L1";r.lock(s);assert(shot()==lockPixels);
 // Plus Home header: clock (MenuClock) and date left-aligned in MenuHomeClock's column, left of the divider; the right box
 // (here: logo + contact count + fact, no weather) centred between the divider and the right margin. The Max centred
 // clock/date/status in MenuHomeClock/MenuHomeDate/MenuHomeStatus; the Plus does not use those two info rects.
 for(auto clock:{"22:51","11:11","00:00"})for(auto date:{"Wed, Oct 14","Mon, Apr 28","Sat, Sep 30"})for(bool acquiring:{false,true}){
  s.status.clock=clock;s.menuDate=date;s.status.acquiringTime=acquiring;s.nodeCount=303;r.home(s);
  const int divider=mm::W/2+6-10,boxX=mm::W/2+4,boxR=mm::W-mm::Margin;
  auto bounds=[&](int x0,int y0,int x1,int y1,int b[4]){b[0]=x1;b[1]=-1;b[2]=y1;b[3]=-1;for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++)if(px(x,y)!=mm::BG&&px(x,y)!=mm::Mist){b[0]=std::min(b[0],x);b[1]=std::max(b[1],x);b[2]=std::min(b[2],y);b[3]=std::max(b[3],y);}return b[1]>=b[0];};
  int left[4],right[4];assert(bounds(0,mm::StatusH,divider,mm::MenuHomeRowY-4,left));assert(left[0]>=mm::Margin&&left[1]<divider-2&&left[2]>=mm::StatusH);
  assert(bounds(divider+1,mm::StatusH,mm::W,mm::MenuHomeRowY-4,right));assert(right[0]>=boxX&&right[1]<boxR&&std::abs(right[0]+right[1]+1-boxX-boxR)<=4);
 }

 s=mm::State{};s.lockHasMessage=true;s.receivedPreview="Selected";s.lockRepeater="Received via: Mongioie";r.lock(s);auto base=shot();
 s.quickReply=true;s.quickReplyEditable=true;s.quickSender="River";s.quickChannel="Cervinia / it-vda";s.quickPreview="Can you bring the radio kit to the ridge?";s.quickInput=std::string(96,'W');s.lockRepeater="Received via: Mongioie";r.lock(s);
 // Plus quick reply: the status bar stays as it is, the rest of the lock is veiled (BG at 9/15) and only the panel carries content.
 assert(std::equal(base.begin(),base.begin()+mm::W*mm::StatusH,canvas.getBuffer()));assert(shot()!=base);
 for(int y=mm::StatusH;y<mm::H;y++)for(int x=0;x<mm::W;x++)if(!mm::QuickReplyPanel.contains(x,y))assert(px(x,y)==mm::FastCanvas16::blend(base[y*mm::W+x],mm::BG,9));
 for(auto rect:{mm::QuickReplyPanel,mm::QuickReplyComposer,mm::QuickReplySend,mm::QuickReplyCancel,mm::QuickReplyEmoji})assert(rect.x>=mm::Margin&&rect.y>=mm::StatusH&&rect.x+rect.w<=mm::W-mm::Margin&&rect.y+rect.h<=mm::H);
 for(auto rect:{mm::QuickReplyComposer,mm::QuickReplySend,mm::QuickReplyCancel,mm::QuickReplyEmoji}){auto p=mm::QuickReplyPanel;assert(rect.x>=p.x&&rect.x+rect.w<=p.x+p.w&&rect.y>=p.y&&rect.y+rect.h<=p.y+p.h);}
 assert(mm::QuickReplyEmoji.x+mm::QuickReplyEmoji.w<=mm::QuickReplySend.x&&mm::QuickReplySend.x+mm::QuickReplySend.w<=mm::QuickReplyCancel.x&&mm::QuickReplyComposer.y+mm::QuickReplyComposer.h<=mm::QuickReplySend.y);
 s=mm::State{};s.page=mm::Page::Chat;s.chatName="Italia";s.scope="it";s.firstRepeater="Received via: Mongioie";
 for(int i=0;i<10;i++){mm::Message m{"River","10:18","Hello","5 hop | it",false};m.token=i+1;s.messages.push_back(m);}
 r.chat(s);assert(r.messageBounds().size()>=3);// Plus: ChatText 12/17 bubbles + meta rows, ~2.5 short messages fit the 139 px thread (the Max showed 4+)
 {int named=0;for(auto &hit:r.messageBounds()){assert(hit.first.y>=mm::Top&&hit.first.y+hit.first.h<=mm::FirstRepeaterY);if(hit.first.y>mm::Top){assert(r.senderHit(hit.first.x+mm::BubblePadding,hit.first.y+mm::BubblePadding));named++;}}assert(named>=2);}// a bubble scrolled half under the header (Plus continuous scroll) has its sender row hidden
 assert(mm::ComposerEmoji.y>=mm::ComposerY&&mm::ComposerEmoji.x>=mm::Margin&&mm::ComposerEmoji.x+mm::ComposerEmoji.w<=mm::W-mm::Margin);
 // Plus: the 160 px picker overlaps the header rule (y=42 < Top) but stays below the chat title baseline.
 assert(mm::EmojiPopup.y>mm::StatusH+7+mm::Components::ascent(mm::Font::ChatTitle)&&mm::EmojiPopup.y+mm::EmojiPopup.h<mm::ComposerY&&mm::emojiTabRect(mm::EmojiTabs-1).x+mm::emojiTabRect(mm::EmojiTabs-1).w<=mm::EmojiPopup.x+mm::EmojiPopup.w-4);
 {auto last=mm::emojiChoiceRect(mm::EmojiPageSize-1);assert(last.x+last.w<=mm::EmojiPopup.x+mm::EmojiPopup.w-4&&last.y+last.h<=mm::EmojiPopup.y+mm::EmojiPopup.h-16&&mm::emojiChoiceRect(0).y>mm::emojiTabRect(0).y+mm::emojiTabRect(0).h);}
 for(int t=0;t<mm_emoji_picker::categoryCount;t++)assert(mm_emoji_picker::categories[t].count<=mm::EmojiPageSize&&mm_emoji_picker::categories[t].first+mm_emoji_picker::categories[t].count<=mm_emoji_picker::count);
 for(auto code:mm::DefaultRecentEmoji)assert(mm::emojiIndex(code)>=0);
 r.chat(s);auto beforeBanner=shot();
 // Notification card: drawn only above the header rule (that stays); the tall Home card and its clear stay above the
 // first Home conversation row (painted over a chat frame: only the bounds are checked).
 r.notificationBanner(mm::NotificationBanner,mm::Top-1,mm::Icon::Messages,"River\nname","Public",std::string(300,'W')+"\nmore");bool bannerChanged=false;
 for(int y=0;y<mm::H;y++)for(int x=0;x<mm::W;x++)if(beforeBanner[y*mm::W+x]!=px(x,y)){assert(y<mm::Top-1);bannerChanged=true;}
 assert(bannerChanged);
 r.chat(s);beforeBanner=shot();r.notificationBanner(mm::NotificationBannerHome,mm::MenuHomeRowY,mm::Icon::Weather,"Storm alert","",std::string(300,'W'));
 for(int y=0;y<mm::H;y++)for(int x=0;x<mm::W;x++)if(beforeBanner[y*mm::W+x]!=px(x,y))assert(y<mm::MenuHomeRowY);
 r.chat(s);
 for(int i=0;i<mm::ReceivedMessageActions;i++){auto box=mm::messageActionRect(i);assert(box.y+box.h<=mm::Bottom);}
 s=mm::State{};s.quickReply=true;s.quickReplyEditable=true;s.quickInput="@[River] ";r.lock(s);int caret=mm::QuickReplyComposer.x+7+c.width("@[River] ",mm::Font::ChatBody);assert(px(caret,mm::QuickReplyComposer.y+9)==mm::FG);
 s=mm::State{};s.quickReply=true;s.quickReplyEditable=true;s.quickInput="@[River] Copy";r.lock(s);auto beforeConfirmation=shot();s.quickSent=true;s.quickReplyEditable=false;r.lock(s);bool confirmationInk=false;for(int y=0;y<mm::H;y++)for(int x=0;x<mm::W;x++)if(beforeConfirmation[y*mm::W+x]!=px(x,y)){assert(mm::QuickReplyPanel.contains(x,y));confirmationInk=true;}assert(confirmationInk);bool messageInk=false;for(int y=mm::QuickReplyComposer.y+4;y<mm::QuickReplyComposer.y+mm::QuickReplyComposer.h-4;y++)for(int x=mm::QuickReplyComposer.x+4;x<mm::QuickReplyComposer.x+mm::QuickReplyComposer.w-4;x++)messageInk=messageInk||px(x,y)==mm::FG;assert(messageInk);

 s=mm::State{};s.messages.push_back({"River","10:18","Painted original","5 hop | it",false});s.messages[0].token=77;s.messages[0].scope="it";s.messages[0].hops=5;r.chat(s);auto frozen=r.snapshotMessages(s);s.messages[0].scope="it-vda";s.messages[0].token=88;s.messages.push_back({"Other","10:19","New RX","1 hop | it-vda",false});assert(frozen.size()==1&&frozen[0].message.token==77&&frozen[0].message.scope=="it"&&frozen[0].message.hops==5&&frozen[0].sender.h>0);
 {// Maps page (Plus): the map fills the page under the status bar, the back / style pills and the round -, centre, + buttons float over it.
  std::vector<uint8_t> black((mm::MapArea.w+7)/8*mm::MapArea.h,0xff);mm::State m;m.page=mm::Page::Maps;m.mapBits=black.data();m.mapStatus="Z13 / LOADING 2";m.mapScalePx=60;m.mapScaleLabel="500 m";
  m.mapPins={{mm::MapArea.x,mm::MapArea.y,true,"Edge"},{mm::MapArea.x+mm::MapArea.w-1,mm::MapArea.y+mm::MapArea.h-1,false,"Corner"}};m.mapSelfX=mm::MapArea.x+mm::MapArea.w-1;m.mapSelfY=mm::MapArea.y;
  mm::Renderer r(canvas);r.draw(m);
  assert(px(mm::MapArea.w/2,mm::MapArea.y+mm::MapArea.h/2)==mm::FG);
  for(int i=0;i<3;i++){auto b=mm::mapButtonRect(i);assert(b.x>=mm::MapArea.x&&b.y>=mm::MapArea.y&&b.x+b.w<=mm::W-mm::Margin&&b.y+b.h<=mm::H-mm::Margin);for(int j=0;j<i;j++){auto q=mm::mapButtonRect(j);assert(b.y+b.h<=q.y||q.y+q.h<=b.y);}}
  for(auto b:{mm::MapBackFloat,mm::MapStyleButton})assert(b.y>=mm::MapArea.y&&b.x>=0&&b.x+b.w<=mm::W&&b.x+b.w<=mm::mapButtonRect(0).x+mm::mapButtonRect(0).w);
  {// Static route picture inside the chat: route over the map, numbered stop, legend band across the top, no map buttons.
   std::vector<uint8_t> white((mm::MapArea.w+7)/8*mm::MapArea.h,0);mm::State route;route.page=mm::Page::Chat;route.messagePath=route.messageRouteView=true;route.mapBits=white.data();route.mapStatus="2 HOPS / 2 ON MAP";
   mm::MapSegment solid;solid.x0=20;solid.y0=120;solid.x1=120;solid.y1=120;mm::MapSegment dashed=solid;dashed.y0=dashed.y1=200;dashed.dashed=true;route.mapRoute={solid,dashed};
   mm::MapPin stop;stop.x=120;stop.y=160;stop.step=1;stop.repeater=true;route.mapPins={stop};route.mapLegend={"S Marta","1 Rpt Righi","2 [a3]?","> this T-Deck"};mm::Renderer rr(canvas);rr.draw(route);
   for(int x=24;x<=116;x+=8)assert(px(x,120)==mm::Accent);int gaps=0;for(int x=22;x<=118;x++)gaps+=px(x,200)!=mm::Accent;assert(gaps>20&&gaps<70);assert(px(114,160)==mm::Accent&&px(120,154)==mm::Accent&&px(120,151)==mm::BG);
   bool button=false;auto b=mm::mapButtonRect(1);for(int x=b.x;x<b.x+b.w;x++)button|=px(x,b.y+b.h-1)!=mm::BG;assert(!button);
   int legend=0;for(int y=mm::MapArea.y+2;y<mm::MapArea.y+15;y++)for(int x=0;x<mm::W;x++)legend+=px(x,y)!=mm::BG;assert(legend>40&&px(mm::W/2,mm::MapArea.y+17)==mm::FG);}
  r.draw(m);bool inked=false;for(int x=mm::mapButtonRect(1).x;x<mm::mapButtonRect(1).x+mm::mapButtonRect(1).w;x++)inked|=px(x,mm::mapButtonRect(1).y+14)==mm::Surface;assert(inked);}
 std::cout<<"PASS: panel bounds, grid separation, form/control bounds, unbroken message wrap, maximum-length message rendering, Home header ink inside its columns, quick reply veil, Maps floating controls.\n";
}
