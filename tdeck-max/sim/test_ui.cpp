#include <algorithm>
#include <vector>
#include <cassert>
#include <iostream>
#include <cstring>
#include "ui/renderer.h"
#include "ui/refresh.h"
#include "ui/navigation_catalog.h"
#include "ui/utf8.h"
#include "ui/composer_caret.h"
#include "ui/emoji_choices.h"
#include "services/keyboard_layout.h"
int main(){
 GFXcanvas1 canvas(mm::W,mm::H);mm::Components c(canvas);
 assert(mm::senderDisplayName("Ridge_L1 🇮🇹")=="Ridge_L1");assert(mm::senderDisplayName("IT Field 🙂")=="IT Field 🙂");
 assert(mm::mentionDisplayText("@[🇮🇹 Oreste IW1QQG] Hi")=="@[Oreste IW1QQG] Hi");assert(mm::mentionDisplayText("@[Oreste 🇮🇹")=="@[Oreste");assert(mm::mentionDisplayText("Weather 🇮🇹") == "Weather 🇮🇹");
 for(int i=0;i<mm::ApplicationCount;i++){auto r=mm::appRect(i);assert(r.x>=0&&r.y>=mm::StatusH&&r.x+r.w<=mm::W&&r.y+r.h<=mm::Bottom);for(int j=i+1;j<mm::ApplicationCount;j++){auto q=mm::appRect(j);assert(r.x+r.w<=q.x||q.x+q.w<=r.x||r.y+r.h<=q.y||q.y+q.h<=r.y);}}
 assert(mm::MenuHomeTabCount==4&&mm::MenuHomeTabIcon==24&&mm::MenuHomeTabH==44);
 assert(mm::HomeTabs[2].section==19);for(auto app:mm::Applications)assert(app.section!=19&&app.section!=0&&app.section!=1);
 assert(mm::menuHomeRowRect(mm::MenuHomeRows-1).y+mm::MenuHomeRowH<mm::MenuHomeTabY);
 for(int i=0;i<mm::MenuHomeTabCount;i++){auto area=mm::menuHomeTabRect(i);assert(area.x>=mm::Margin&&area.x+area.w<=mm::W-mm::Margin&&area.y+area.h<=mm::H);assert(mm::HomeTabs[i].section!=9&&mm::HomeTabs[i].section!=10);if(i)assert(mm::menuHomeTabRect(i-1).x+area.w+mm::MenuHomeTabGap==area.x);}
 bool weather=false;for(auto app:mm::Applications)weather|=app.section==10;assert(weather&&mm::HomeTabs[3].section<0&&std::string(mm::HomeTabs[3].label)=="App");
 mm::Status status;status.mesh=false;c.statusBar(status);std::vector<uint8_t> noSignal(canvas.getBuffer(),canvas.getBuffer()+mm::W/8*mm::StatusH);status.mesh=true;c.statusBar(status);assert(!memcmp(noSignal.data(),canvas.getBuffer(),noSignal.size()));
 for(int i=0;i<3;i++){auto r=mm::formRect(i);assert(r.y>=mm::Top&&r.y+r.h<mm::SaveButton.y);}
 assert(mm::SaveButton.y+mm::SaveButton.h<mm::Bottom);
 assert(mm::RadioAdvert.y+mm::RadioAdvert.h<mm::Bottom);
 for(int i=0;i<mm::WifiRows;i++){auto r=mm::wifiRowRect(i);assert(r.y+r.h<=mm::Bottom);}
 for(int i=0;i<mm::FileRows;i++){auto r=mm::fileRowRect(i);assert(r.y+r.h<=mm::Bottom);}
 assert(mm::WeatherStatusY+mm::Components::lineHeight(mm::Font::ChatMeta)<=mm::WeatherRefresh.y&&mm::weatherCell(3).y+mm::weatherCell(3).h<mm::WeatherStatusY);
 assert(mm::WeatherHeroY+64<=mm::WeatherHoursY&&mm::WeatherHoursY+mm::WeatherHoursH<mm::WeatherGridY&&mm::WeatherPrompt.y+mm::WeatherPrompt.h<=mm::WeatherStatusY);
 assert(mm::WeatherWifiButton.y+mm::WeatherWifiButton.h<mm::WeatherPrompt.y+mm::WeatherPrompt.h&&mm::weatherCell(1).x+mm::weatherCell(1).w<=mm::W-mm::Margin);
 assert(mm::WeatherRefresh.y+mm::WeatherRefresh.h<=mm::Bottom);
 for(int i=0;i<5;i++){auto r=mm::ownMessageActionRect(i);assert(r.x>=mm::Margin&&r.y>=mm::Top&&r.x+r.w<=mm::W-mm::Margin&&r.y+r.h<=mm::Bottom);for(int j=i+1;j<5;j++){auto q=mm::ownMessageActionRect(j);assert(r.x+r.w<=q.x||q.x+q.w<=r.x||r.y+r.h<=q.y||q.y+q.h<=r.y);}}
 for(int i=0;i<mm::ContactRows;i++){auto r=mm::contactRowRect(i);assert(r.y>=mm::ContactSearch.y+mm::ContactSearch.h&&r.y+r.h<=mm::ContactPrevious.y);}
 for(int i=0;i<6;i++){auto r=mm::timeoutOptionRect(i);assert(r.x>=0&&r.y>=mm::Top&&r.x+r.w<=mm::W&&r.y+r.h<mm::Bottom);}
 std::string longWord(96,'W');auto lines=c.lines(longWord,176);std::string result;for(auto &line:lines){assert(c.width(line)<=176);result+=line;}assert(result==longWord);
 // The byte counter appears only after95 and never occupies composer pixels.
 {mm::State state;state.page=mm::Page::Chat;state.messageLimit=142;state.input=std::string(95,'x');mm::Renderer renderer(canvas);renderer.chat(state);
  for(int y=mm::FirstRepeaterY;y<mm::ComposerY;y++)for(int x=0;x<mm::W;x++)assert(!canvas.getPixel(x,y));
  state.input+='x';renderer.chat(state);bool ink=false;for(int y=mm::FirstRepeaterY;y<mm::ComposerY;y++)for(int x=mm::W-70;x<mm::W;x++)ink=ink||canvas.getPixel(x,y);assert(ink);
  std::vector<uint8_t> pixels(canvas.getBuffer(),canvas.getBuffer()+mm::W/8*mm::H);state.messageLimit=160;renderer.chat(state);
  assert(!memcmp(pixels.data()+mm::ComposerY*(mm::W/8),canvas.getBuffer()+mm::ComposerY*(mm::W/8),(mm::H-mm::ComposerY)*(mm::W/8)));
  state.input.pop_back();renderer.chat(state);for(int y=mm::FirstRepeaterY;y<mm::ComposerY;y++)for(int x=0;x<mm::W;x++)assert(!canvas.getPixel(x,y));
 }
 // Dictated draft: the composer expands upward, shows the mention typed first and every row; typing alone stays single-line.
 {mm::State state;state.page=mm::Page::Chat;state.messageLimit=160;std::string mention="@[Aria] ";state.input=mention+"questo e un messaggio dettato abbastanza lungo da occupare parecchie righe del compositore per controllare gli errori";
  mm::Renderer renderer(canvas);renderer.chat(state);assert(renderer.chatComposerTop==mm::ComposerY);
  auto tail=c.inputTail(state.input,mm::ComposerEmoji.x-(mm::Margin+7)-5-3,mm::Font::ChatBody);assert(tail.find(mention)==std::string::npos);// single line shows only the end
  state.composerExpanded=true;renderer.chat(state);auto rows=c.lines(state.input,mm::ComposerEmoji.x-(mm::Margin+7)-5-3,mm::Font::ChatBody);int bodyH=c.lineHeight(mm::Font::ChatBody);
  assert(rows.size()>=3&&rows.size()<=size_t(mm::ComposerMaxLines)&&rows[0].rfind(mention,0)==0);
  assert(renderer.chatComposerTop==mm::H-(mm::ComposerH+(int(rows.size())-1)*bodyH));
  bool ink=false;for(int y=renderer.chatComposerTop+9;y<renderer.chatComposerTop+9+bodyH;y++)for(int x=mm::Margin+7;x<mm::Margin+60;x++)ink=ink||canvas.getPixel(x,y);assert(ink);// first row (mention) painted
  ink=false;for(int y=renderer.chatComposerTop-mm::FirstRepeaterH;y<renderer.chatComposerTop;y++)for(int x=mm::W-70;x<mm::W;x++)ink=ink||canvas.getPixel(x,y);assert(ink);// byte counter moved up with the card
  state.input=std::string(400,'a');renderer.chat(state);assert(renderer.chatComposerTop==mm::H-(mm::ComposerH+(mm::ComposerMaxLines-1)*bodyH));// capped
  state.audioRecording=true;renderer.chat(state);assert(renderer.chatComposerTop==mm::ComposerY);// recording banner is single-line again
  state.audioRecording=false;state.emojiPicker=true;renderer.chat(state);assert(renderer.chatComposerTop==mm::ComposerY);// picker needs the fixed composer
 }
 // Composer caret: typed drafts expand too; the caret follows the cursor byte offset across wrapped rows and the visible window follows it.
 {using namespace mm;
  {std::vector<std::string> rows={"ciao","mondo"};std::string value="ciao mondo";
   auto p=caretPlace(rows,value,0);assert(p.row==0&&p.column==0);p=caretPlace(rows,value,4);assert(p.row==0&&p.column==4);
   p=caretPlace(rows,value,5);assert(p.row==1&&p.column==0);p=caretPlace(rows,value,7);assert(p.row==1&&p.column==2);p=caretPlace(rows,value,10);assert(p.row==1&&p.column==5);p=caretPlace(rows,value,99);assert(p.row==1&&p.column==5);}
  {std::vector<std::string> rows={"a b","c"};std::string value="a  b \nc";// collapsed double space, dropped separators
   auto p=caretPlace(rows,value,2);assert(p.row==0&&p.column==2);p=caretPlace(rows,value,3);assert(p.row==0&&p.column==2);p=caretPlace(rows,value,4);assert(p.row==0&&p.column==3);p=caretPlace(rows,value,6);assert(p.row==1&&p.column==0);}
  {std::vector<std::string> none;auto p=caretPlace(none,"",0);assert(p.row==0&&p.column==0);}
  assert(caretWindowStart(3,6,0)==0&&caretWindowStart(9,6,8)==3&&caretWindowStart(9,6,3)==3&&caretWindowStart(9,6,1)==1&&caretWindowStart(9,6,0)==0);
  mm::State state;state.page=mm::Page::Chat;state.messageLimit=160;state.input="ciao mondo";state.composerExpanded=true;state.inputCursor=std::string::npos;
  mm::Renderer renderer(canvas);renderer.chat(state);assert(renderer.chatComposerTop==mm::ComposerY);// one row: the card keeps its height
  auto caretColumn=[&](int y0,int y1){int found=-1;for(int x=mm::Margin+7;x<mm::ComposerEmoji.x-2&&found<0;x++){bool full=true;for(int y=y0;y<y1;y++)full=full&&canvas.getPixel(x,y);if(full)found=x;}return found;};
  int endX=caretColumn(mm::ComposerY+9,mm::ComposerY+9+15);assert(endX>mm::Margin+7+c.width("ciao mond",mm::Font::ChatBody));
  state.inputCursor=4;renderer.chat(state);int midX=caretColumn(mm::ComposerY+9,mm::ComposerY+9+15);assert(midX==mm::Margin+7+c.width("ciao",mm::Font::ChatBody)+1&&midX<endX);
  state.inputCursor=0;renderer.chat(state);assert(caretColumn(mm::ComposerY+9,mm::ComposerY+9+15)==mm::Margin+8);
  // Long typed draft: 9 rows, caret on the second row -> the window starts there and the caret is on the first visible row.
  std::string longText;for(int i=0;i<40;i++)longText+="parola ";state.input=longText;state.inputCursor=std::string::npos;renderer.chat(state);int bodyH=c.lineHeight(mm::Font::ChatBody);
  auto rows=c.lines(state.input,mm::ComposerEmoji.x-(mm::Margin+7)-5-3,mm::Font::ChatBody);assert(int(rows.size())>mm::ComposerMaxLines);assert(renderer.chatComposerTop==mm::H-(mm::ComposerH+(mm::ComposerMaxLines-1)*bodyH));
  int top=renderer.chatComposerTop;assert(caretColumn(top+9+(mm::ComposerMaxLines-1)*bodyH,top+9+(mm::ComposerMaxLines-1)*bodyH+15)>0);// caret on the last visible row
  size_t secondRow=rows[0].size()+1;state.inputCursor=secondRow;renderer.chat(state);assert(renderer.chatComposerTop==top);assert(caretColumn(top+9,top+9+15)==mm::Margin+8);// caret at the start of the first visible row (row 1 of the draft)
  bool firstRowInk=false;for(int y=top+9;y<top+9+bodyH;y++)for(int x=mm::Margin+7;x<mm::Margin+40;x++)firstRowInk=firstRowInk||canvas.getPixel(x,y);assert(firstRowInk);
  // Emoji picker open: single line shows the text up to the caret with the caret after it, then the rest.
  state.input="ciao mondo";state.inputCursor=4;state.emojiPicker=true;renderer.chat(state);assert(caretColumn(mm::ComposerY+9,mm::ComposerY+9+15)==mm::Margin+7+c.width("ciao",mm::Font::ChatBody)+1);
  std::cout<<"PASS: composer caret placement, typed draft expansion, caret window, picker single line\n";
 }
 mm::RefreshTracker tracker;canvas.fillScreen(0);auto plan=tracker.plan(canvas.getBuffer(),false,0);assert(plan.changed&&plan.full);tracker.commit(canvas.getBuffer());assert(!tracker.plan(canvas.getBuffer(),false,0).changed);
 canvas.drawPixel(83,100,1);plan=tracker.plan(canvas.getBuffer(),false,0);assert(plan.changed&&!plan.full&&plan.area.x%8==0&&plan.area.contains(83,100));tracker.commit(canvas.getBuffer());assert(tracker.plan(canvas.getBuffer(),true,0).full);assert(!tracker.plan(canvas.getBuffer(),false,mm::PartialLimit).changed);
 canvas.fillScreen(1);{auto big=tracker.plan(canvas.getBuffer(),false,0);assert(big.changed&&!big.full);} // whole-screen change: fast partial, the full refresh is deferred to idle
 assert(tracker.plan(canvas.getBuffer(),true,0).full); // marked transition (lock / unlock / idle maintenance)
 mm::State s;s.page=mm::Page::Chat;s.chatName="Field Team";s.scope="it-vda";s.messages.push_back({"River","10:18",longWord,"3 hop | it-vda",false});mm::Renderer r(canvas);r.draw(s);
 {
  mm::State all;for(int i=0;i<4;i++){mm::Conversation item;item.id=i;item.name=i==3?"Cervinia":"Chat "+std::to_string(i);all.conversations.push_back(item);}
  all.scroll=1;r.home(all);std::vector<uint8_t> scrolled(canvas.getBuffer(),canvas.getBuffer()+mm::W/8*mm::H);
  auto expected=all;expected.scroll=0;expected.conversations.erase(expected.conversations.begin());r.home(expected);
  assert(all.conversations.size()==4&&all.conversations[3].name=="Cervinia");
  for(int row=0;row<mm::MenuHomeRows;row++){auto box=mm::menuHomeRowRect(row);for(int y=box.y;y<box.y+box.h;y++)for(int x=box.x;x<box.x+box.w;x++)assert(bool(scrolled[y*(mm::W/8)+x/8]&(128>>(x%8)))==bool(canvas.getPixel(x,y)));}
 }
 for(auto page:{mm::Page::Weather,mm::Page::Files,mm::Page::Mesh,mm::Page::Settings}){s.page=page;r.draw(s);assert(!canvas.getPixel(0,mm::FooterY));}
 s.lockHasMessage=true;s.receivedName="Ridge_L1 🇮🇹";r.lock(s);std::vector<uint8_t> lockPixels(canvas.getBuffer(),canvas.getBuffer()+((mm::W+7)/8)*mm::H);assert(s.receivedName=="Ridge_L1 🇮🇹");s.receivedName="Ridge_L1";r.lock(s);assert(!memcmp(lockPixels.data(),canvas.getBuffer(),lockPixels.size()));
 for(auto clock:{"22:51","11:11","00:00"})for(auto date:{"Wed, Oct 14","Mon, Apr 28","Sat, Sep 30"})for(bool acquiring:{false,true}){
  s.status.clock=clock;s.menuDate=date;s.status.acquiringTime=acquiring;s.nodeCount=303;
  // Widget layout: the smaller clock and its date stay left of the divider (HomeWidgetBox.x-5), clear of it.
  s.homeWidget=mm::WidgetSunset;r.home(s);for(int y=mm::HomeWidgetBox.y-4;y<72;y++)for(int x=mm::HomeWidgetBox.x-8;x<mm::HomeWidgetBox.x-5;x++)assert(!canvas.getPixel(x,y));
  s.homeWidget=mm::WidgetClassic;r.home(s);
  for(auto area:{mm::MenuHomeClock,mm::MenuHomeDate,mm::MenuHomeStatus}){int left=area.x+area.w,right=-1,top=area.y+area.h,bottom=-1;for(int y=area.y;y<area.y+area.h;y++)for(int x=area.x;x<area.x+area.w;x++)if(canvas.getPixel(x,y)){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}assert(right>=left&&bottom>=top);assert(std::abs(left+right+1-2*area.x-area.w)<=1&&std::abs(top+bottom+1-2*area.y-area.h)<=1);}
 }

 s=mm::State{};s.lockHasMessage=true;s.receivedPreview="Selected";r.lock(s);std::vector<uint8_t> base(canvas.getBuffer(),canvas.getBuffer()+mm::W/8*mm::H);
 s.quickReply=true;s.quickReplyEditable=true;s.quickSender="River";s.quickChannel="Cervinia / it-vda";s.quickPreview="Can you bring the radio kit to the ridge?";s.quickInput=std::string(96,'W');s.lockRepeater="Received via: Mongioie";r.lock(s);
 assert(!memcmp(base.data(),canvas.getBuffer(),mm::W/8*mm::QuickReplyPanel.y));assert(memcmp(base.data(),canvas.getBuffer(),base.size()));
 for(int y=mm::QuickReplyPanel.y+mm::QuickReplyPanel.h;y<mm::H;y++)for(int x=0;x<mm::W;x++)assert(!canvas.getPixel(x,y));
 for(auto rect:{mm::QuickReplyPanel,mm::QuickReplyComposer,mm::QuickReplySend,mm::QuickReplyCancel})assert(rect.x>=mm::Margin&&rect.y>=mm::StatusH&&rect.x+rect.w<=mm::W-mm::Margin&&rect.y+rect.h<=mm::H);
 s=mm::State{};s.page=mm::Page::Chat;s.chatName="Italia";s.scope="it";s.firstRepeater="Received via: Mongioie";
 for(int i=0;i<10;i++){mm::Message m{"River","10:18","Hello","5 hop | it",false};m.token=i+1;s.messages.push_back(m);}
 r.chat(s);assert(r.messageBounds().size()>=4);for(auto &hit:r.messageBounds()){assert(hit.first.y>=mm::Top&&hit.first.y+hit.first.h<=mm::FirstRepeaterY);assert(r.senderHit(hit.first.x+mm::BubblePadding,hit.first.y+mm::BubblePadding));}
 assert(mm::ComposerEmoji.y>=mm::ComposerY&&mm::ComposerEmoji.x>=mm::Margin&&mm::ComposerEmoji.x+mm::ComposerEmoji.w<=mm::W-mm::Margin);
 assert(mm::EmojiPopup.y>mm::Top&&mm::EmojiPopup.y+mm::EmojiPopup.h<mm::ComposerY&&mm::emojiTabRect(mm::EmojiTabs-1).x+mm::emojiTabRect(mm::EmojiTabs-1).w<=mm::EmojiPopup.x+mm::EmojiPopup.w-4);
 {auto last=mm::emojiChoiceRect(mm::EmojiPageSize-1);assert(last.x+last.w<=mm::EmojiPopup.x+mm::EmojiPopup.w-4&&last.y+last.h<=mm::EmojiPopup.y+mm::EmojiPopup.h-16&&mm::emojiChoiceRect(0).y>mm::emojiTabRect(0).y+mm::emojiTabRect(0).h);}
 for(int t=0;t<mm_emoji_picker::categoryCount;t++)assert(mm_emoji_picker::categories[t].count<=mm::EmojiPageSize&&mm_emoji_picker::categories[t].first+mm_emoji_picker::categories[t].count<=mm_emoji_picker::count);
 for(auto code:mm::DefaultRecentEmoji)assert(mm::emojiIndex(code)>=0);
 r.chat(s);std::vector<uint8_t> beforeBanner(canvas.getBuffer(),canvas.getBuffer()+mm::W/8*mm::H);
 // Notification card: long, multi-line text stays inside the card and the page above Top-1 it clears (Home: the clock band).
 r.notificationBanner(mm::NotificationBanner,mm::Top-1,mm::Icon::Messages,"River\nname",std::string(300,'W')+"\nmore",std::string(300,'W')+"\nmore");bool bannerChanged=false;
 for(int y=0;y<mm::H;y++)for(int x=0;x<mm::W;x++){bool old=beforeBanner[y*(mm::W/8)+x/8]&(128>>(x%8));if(old!=bool(canvas.getPixel(x,y))){assert(y<mm::Top-1);bannerChanged=true;}}
 assert(bannerChanged);
 r.home(s);std::vector<uint8_t> beforeHome(canvas.getBuffer(),canvas.getBuffer()+mm::W/8*mm::H);
 r.notificationBanner(mm::NotificationBannerHome,mm::MenuHomeRowY-2,mm::Icon::Messages,"River","Direct",std::string(300,'W'));
 for(int y=0;y<mm::H;y++)for(int x=0;x<mm::W;x++){bool old=beforeHome[y*(mm::W/8)+x/8]&(128>>(x%8));if(old!=bool(canvas.getPixel(x,y)))assert(y<mm::MenuHomeRowY-2);}
 for(int i=0;i<mm::ReceivedMessageActions;i++){auto box=mm::messageActionRect(i);assert(box.y+box.h<=mm::Bottom);}
 s=mm::State{};s.quickReply=true;s.quickReplyEditable=true;s.quickInput="@[River] ";r.lock(s);int caret=mm::QuickReplyComposer.x+7+c.width("@[River] ",mm::Font::ChatBody);assert(canvas.getPixel(caret,mm::QuickReplyComposer.y+9));
 s=mm::State{};s.quickReply=true;s.quickReplyEditable=true;s.quickInput="@[River] Copy";r.lock(s);std::vector<uint8_t> beforeConfirmation(canvas.getBuffer(),canvas.getBuffer()+mm::W/8*mm::H);s.quickSent=true;s.quickReplyEditable=false;r.lock(s);bool confirmationInk=false;for(int y=0;y<mm::H;y++)for(int x=0;x<mm::W;x++){bool old=beforeConfirmation[y*(mm::W/8)+x/8]&(128>>(x%8));if(old!=bool(canvas.getPixel(x,y))){assert(mm::QuickReplyPanel.contains(x,y));confirmationInk=true;}}assert(confirmationInk);bool messageInk=false;for(int y=mm::QuickReplyComposer.y+4;y<mm::QuickReplyComposer.y+mm::QuickReplyComposer.h-4;y++)for(int x=mm::QuickReplyComposer.x+4;x<mm::QuickReplyComposer.x+mm::QuickReplyComposer.w-4;x++)messageInk=messageInk||canvas.getPixel(x,y);assert(messageInk);

 s=mm::State{};s.messages.push_back({"River","10:18","Painted original","5 hop | it",false});s.messages[0].token=77;s.messages[0].scope="it";s.messages[0].hops=5;r.chat(s);auto frozen=r.snapshotMessages(s);s.messages[0].scope="it-vda";s.messages[0].token=88;s.messages.push_back({"Other","10:19","New RX","1 hop | it-vda",false});assert(frozen.size()==1&&frozen[0].message.token==77&&frozen[0].message.scope=="it"&&frozen[0].message.hops==5&&frozen[0].sender.h>0);
 {// Maps page: the map fills the page under the status bar; pins/self at the map corners stay on screen;
  // back and zoom / centre buttons float over the map as white discs with a black ring.
  const auto &A=mm::MapPageArea;std::vector<uint8_t> black((A.w+7)/8*A.h,0xff);mm::State m;m.page=mm::Page::Maps;m.mapFull=true;m.mapBits=black.data();m.mapStatus="Z13 / LOADING 2";m.mapScalePx=60;m.mapScaleLabel="500 m";
  m.mapPins={{A.x,A.y,true,"Edge"},{A.x+A.w-1,A.y+A.h-1,false,"Corner"}};m.mapSelfX=A.x+A.w-1;m.mapSelfY=A.y;
  mm::Renderer r(canvas);r.draw(m);auto px=[&](int x,int y){return canvas.getPixel(x,y)!=0;};
  assert(px(A.w/2,A.y+A.h/2)==true);
  for(int i=0;i<3;i++){auto b=mm::mapButtonRect(i);assert(b.y>=A.y&&b.y+b.h<=mm::H&&b.x+b.w<=mm::W);int cx=b.x+b.w/2,cy=b.y+b.h/2;assert(!px(cx-10,cy-6)&&px(b.x,cy));}
  {auto b=mm::MapBackFloat;assert(!px(b.x+b.w/2+9,b.y+b.h/2+6)&&px(b.x,b.y+b.h/2));}
  {// Static route picture inside the chat: route over the map, numbered stop, legend below, no map buttons.
   std::vector<uint8_t> white((mm::MapArea.w+7)/8*mm::MapArea.h,0);mm::State route;route.page=mm::Page::Chat;route.messagePath=route.messageRouteView=true;route.mapBits=white.data();route.mapStatus="2 HOPS / 2 ON MAP";
   mm::MapSegment solid;solid.x0=20;solid.y0=120;solid.x1=120;solid.y1=120;mm::MapSegment dashed=solid;dashed.y0=dashed.y1=200;dashed.dashed=true;route.mapRoute={solid,dashed};
   mm::MapPin stop;stop.x=120;stop.y=160;stop.step=1;stop.repeater=true;route.mapPins={stop};route.mapLegend={"S Marta","1 Rpt Righi","2 [a3]?","> this T-Deck"};mm::Renderer rr(canvas);rr.draw(route);
   for(int x=24;x<=116;x+=8)assert(px(x,120));int gaps=0;for(int x=22;x<=118;x++)gaps+=!px(x,200);assert(gaps>20&&gaps<70);assert(px(114,160)&&px(120,154)&&!px(120,151));
   int legend=0;for(int y=mm::MapArea.y+mm::MapArea.h+2;y<mm::Bottom;y++)for(int x=0;x<mm::W;x++)legend+=px(x,y);assert(legend>40);
   bool button=false;auto b=mm::mapButtonRect(1);for(int x=b.x;x<b.x+b.w;x++)button|=px(x,b.y+b.h-1);assert(!button);}
  r.draw(m);bool inked=false;for(int x=mm::mapButtonRect(1).x;x<mm::mapButtonRect(1).x+mm::mapButtonRect(1).w;x++)inked|=px(x,mm::mapButtonRect(1).y+14);assert(inked);}
 // Keyboard languages: the toggleRow detail line stays inside the focus outline, the rows end above the help, and the
 // help fits for every language with the two-line how-to and a notice line (one language on, focus on that row).
 {const int line=mm::Components::lineHeight(mm::Font::ChatMeta),wrap=mm::W-2*mm::Margin-4;assert(22+line<=mm::KeyboardRowH-3);
  auto last=mm::keyboardRowRect(mm::KeyboardRows-1);assert(last.y+last.h<mm::KeyboardHelpY&&mm::KeyboardSearch.y+mm::KeyboardSearch.h<mm::keyboardRowRect(0).y);
  int howto=int(c.lines("Turn on two languages, then Alt + Space switches between them",wrap,mm::Font::ChatMeta).size());
  for(int i=0;i<mm_kbd::LangCount;i++){std::string help=mm_kbd::accentHelp(mm_kbd::Lang(i));int n=1+howto+(help.empty()?0:std::max({int(c.lines("Left Shift is the accent key. "+help,wrap,mm::Font::ChatMeta).size()),int(c.lines("Right Shift is the accent key. "+help,wrap,mm::Font::ChatMeta).size()),int(c.lines("The accent key is off (Settings > Accent key).",wrap,mm::Font::ChatMeta).size())}));
   assert(mm::KeyboardHelpY+(n-1)*line<=mm::Bottom-12);}}
 // Accent key page: three rows inside the screen above the help; for every choice the help (with a notice line) fits,
 // the saved choice is marked, and the notice is not copied into the footer.
 {for(int i=0;i<3;i++){auto r=mm::accentRowRect(i);assert(r.y>=mm::Top&&r.y+r.h<mm::AccentInfoY);}
  mm::State a;a.page=mm::Page::AccentKey;mm::Renderer renderer(canvas);
  for(int mode=0;mode<3;mode++){a.accentMode=a.accentFocus=mode;a.notice="Saved";renderer.draw(a);
   std::vector<uint8_t> withNotice(canvas.getBuffer()+mm::W/8*mm::FooterY,canvas.getBuffer()+mm::W/8*mm::H);a.notice="";renderer.draw(a);
   assert(!memcmp(withNotice.data(),canvas.getBuffer()+mm::W/8*mm::FooterY,withNotice.size()));
   auto r=mm::accentRowRect(mode);assert(canvas.getPixel(r.x+r.w-20,r.y+r.h/2));// filled dot on the saved choice
   for(int other=0;other<3;other++)if(other!=mode){auto o=mm::accentRowRect(other);assert(!canvas.getPixel(o.x+o.w-20,o.y+o.h/2));}}
  const int line=mm::Components::lineHeight(mm::Font::ChatMeta),wrap=mm::W-2*mm::Margin-4;
  for(int l=0;l<mm_kbd::LangCount;l++){int n=1+int(c.lines(std::string(mm_kbd::info(mm_kbd::Lang(l)).name)+" (Settings > Keyboard languages):",wrap,mm::Font::ChatMeta).size())+int(c.lines("In a chat, tap it before a letter: once for the first accent, twice for the second.",wrap,mm::Font::ChatMeta).size())+int(c.lines(mm_kbd::accentHelp(mm_kbd::Lang(l))[0]?mm_kbd::accentHelp(mm_kbd::Lang(l)):"No accented letters in this language",wrap,mm::Font::ChatMeta).size());
   assert(22+line<=mm::AccentRowH-3&&mm::AccentInfoY+(n-1)*line<=mm::Bottom-12);}
  }
 // The Keyboard page shows its notice once, above the help: no footer copy at the bottom.
 {mm::State k;k.page=mm::Page::Keyboard;mm::Renderer renderer(canvas);renderer.draw(k);
  std::vector<uint8_t> plain(canvas.getBuffer()+mm::W/8*mm::FooterY,canvas.getBuffer()+mm::W/8*mm::H);
  k.notice="Save failed";renderer.draw(k);assert(!memcmp(plain.data(),canvas.getBuffer()+mm::W/8*mm::FooterY,plain.size()));}
 std::cout<<"PASS: panel bounds, keyboard-language rows and help, accent key page, grid separation, form/control bounds, unbroken message wrap, no-op/partial/full refresh planning, maximum-length message rendering, Home clock/date/status visible-ink centering.\n";
}
