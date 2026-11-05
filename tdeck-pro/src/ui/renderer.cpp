#include "renderer.h"
#include "composer_caret.h"
#include "utf8.h"
#include "mesh/radio_parameters.h"
#include "mesh/radio_presets.h"
#include "emoji_choices.h"
#include "navigation_catalog.h"
#include "services/channel_qr.h"
#include "cli_layout.h"
#include "mesh/rx_log.h"
#include "lock_wallpapers.h"
#include "lock_clock_glyphs.h"
#include "services/dictation_policy.h"
#include "services/channel_form.h"
#include "os_logo_bitmap.h"
#include "services/keyboard_layout.h"
#include <cmath>
#include <algorithm>
namespace mm {
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
Rect Renderer::overlayPanel(){g.fillRect(0,Top,W,H-Top,BG);Rect panel={Margin,Top+4,W-2*Margin,Bottom-Top-8};c.card(panel);return panel;}
void Renderer::shell(const State &s,const std::string &title,bool back,Icon right){g.fillScreen(BG);c.statusBar(s.status);c.pageHeader(title,back,right);}
// Lock clock: centered digits from the generated glyph set (Home clock face, larger).
void Renderer::heroClock(const std::string &text,int y){
 auto glyph=[](char ch)->const mm_hero_clock::Glyph*{for(int i=0;i<mm_hero_clock::count;i++)if(mm_hero_clock::glyphs[i].ch==ch)return &mm_hero_clock::glyphs[i];return nullptr;};
 int total=0;const mm_hero_clock::Glyph *last=nullptr;for(char ch:text)if(auto item=glyph(ch)){total+=item->advance;last=item;}
 if(last)total-=last->advance-last->w;
 int x=(W-total)/2;for(char ch:text){auto item=glyph(ch);if(!item)continue;g.drawBitmap(x,y,mm_hero_clock::bitmap+item->offset,item->w,item->h,FG);x+=item->advance;}
}
// Settings thumbnail: the 240x278 wallpaper at 1/4 scale (60x70). A block is ink when at least a quarter of its
// mountain pixels are ink, so 1-2 px outlines survive while white snow stays white.
void Renderer::wallpaperThumb(int index,int x0,int y0){
 if(index<0||index>=mm_wallpaper::count)return;const auto &wp=mm_wallpaper::list[index];constexpr int stride=(mm_wallpaper::width+7)/8,S=WallpaperThumbScale;
 PackBitsReader image(wp.image);uint8_t rows[S][stride];int y=0;
 for(int ty=0;ty<WallpaperThumbH;ty++){int band=0;for(;band<S&&y<mm_wallpaper::height;band++,y++)image.read(rows[band],stride);
  for(int tx=0;tx<WallpaperThumbW;tx++){int ink=0;
   for(int dy=0;dy<band;dy++){const uint8_t *row=rows[dy];for(int dx=0;dx<S;dx++){int x=tx*S+dx;if(row[x>>3]&(0x80>>(x&7)))ink++;}}
   if(ink*4>=S*S)g.drawPixel(x0+tx,y0+ty,FG);}}
}
void Renderer::lock(const State &s){
 g.fillScreen(BG);c.statusBar(s.status);
 c.centered(Margin,LockDateY,W-2*Margin,s.date,Font::ChatBody);
 heroClock(s.status.clock,LockClockY);
 // Mountain after the clock: sky pixels are transparent, the mountain is opaque, so the summit stands in front of the digits.
 constexpr int stride=(mm_wallpaper::width+7)/8;const auto &wp=mm_wallpaper::list[s.wallpaper>=0&&s.wallpaper<mm_wallpaper::count?s.wallpaper:0];
 PackBitsReader maskData(wp.mask),imageData(wp.image);uint8_t maskRow[stride],imageRow[stride];
 for(int y=0;y<mm_wallpaper::height;y++){maskData.read(maskRow,stride);imageData.read(imageRow,stride);
  // A full-width unrotated 1 bpp canvas takes whole bytes (72k drawPixel calls per lock frame otherwise).
  if(bits&&g.getRotation()==0&&g.width()==mm_wallpaper::width&&stride*8==W){if(LockHeroY+y<H){uint8_t *row=bits+size_t(LockHeroY+y)*stride;for(int i=0;i<stride;i++)row[i]=uint8_t((row[i]&~maskRow[i])|(imageRow[i]&maskRow[i]));}continue;}
  for(int x=0;x<mm_wallpaper::width;x++){uint8_t bit=0x80>>(x&7);if(!(maskRow[x>>3]&bit))continue;g.drawPixel(x,LockHeroY+y,(imageRow[x>>3]&bit)?FG:BG);}}
 const auto r=LockMessage;g.fillRoundRect(r.x,r.y,r.w,r.h,Radius,BG);c.card(r);
 if(!s.lockHasMessage){
  int block=24+6+c.lineHeight(Font::ChatBody),top=r.y+(r.h-block)/2;
  c.icon(Icon::Messages,r.x+(r.w-24)/2,top,24);c.centered(r.x+6,top+30,r.w-12,s.lockEnd?"No more messages":"No new messages",Font::ChatBody);
 }else{
  // Same anatomy as a Home conversation row: name + time, meta line, preview, unread badge.
  int textX=r.x+8,timeW=c.width(s.receivedTime,Font::ChatMeta);
  c.text(textX,r.y+7,senderDisplayName(s.receivedName),Font::ChatBody,FG,r.w-22-timeW);
  c.text(r.x+r.w-8-timeW,r.y+9,s.receivedTime,Font::ChatMeta,FG,timeW);
  std::string route=s.receivedChannel;if(s.receivedHops>=0)route+=" / "+std::to_string(s.receivedHops)+(s.receivedHops==1?" hop":" hops");
  c.text(textX,r.y+24,route,Font::ChatMeta,FG,r.w-16);
  int width=r.w-16-(s.unread?24:0);auto preview=c.lines(s.receivedPreview,width,Font::ChatBody);int limit=(r.h-LockCardHeader-6)/c.lineHeight(Font::ChatBody);
  if(int(preview.size())>limit){preview.resize(limit);preview.back()=c.fit(preview.back()+"...",width,Font::ChatBody);}
  int previewY=r.y+LockCardHeader+(r.h-LockCardHeader-6-int(preview.size())*c.lineHeight(Font::ChatBody))/2; // centered in the body like the previous lock card
  for(int i=0;i<int(preview.size());i++)c.text(textX,previewY+i*c.lineHeight(Font::ChatBody),preview[i],Font::ChatBody);
  if(s.unread){int bx=r.x+r.w-13,by=r.y+r.h-14;g.fillCircle(bx,by,9,FG);c.centered(bx-9,by-4,18,s.unread>99?"99+":std::to_string(s.unread),Font::MenuLabel,BG);}
 }
 // Repeater line over the wallpaper's lower slopes: a white plate with a thin frame keeps it readable.
 if(!s.lockRepeater.empty()){auto text=c.fit(s.lockRepeater,LockRepeater.w-16,Font::ChatMeta);int tw=c.width(text,Font::ChatMeta)+14,th=c.lineHeight(Font::ChatMeta)+4;
  Rect plate={(W-tw)/2,LockRepeater.y+(LockRepeater.h-th)/2,tw,th};g.fillRoundRect(plate.x,plate.y,plate.w,plate.h,Radius,BG);g.drawRoundRect(plate.x,plate.y,plate.w,plate.h,Radius,FG);
  c.centered(plate.x,plate.y+2,plate.w,text,Font::ChatMeta);}
 if(!s.quickReply&&(s.lockHint||s.unlockTapCount||s.unlockCounting))unlockTapBanner(s.unlockTapCount);
 if(s.quickReply)quickReply(s);
}
void Renderer::quickReply(const State &s){
 auto panel=QuickReplyPanel;g.fillRect(0,panel.y,W,H-panel.y,BG);c.card(panel); // full width: also hides the hero card edges
 c.text(panel.x+8,panel.y+8,"Quick Reply",Font::ChatTitle,FG,panel.w-16);if(s.accent&&!s.quickSent){int x=panel.x+8+c.width("Quick Reply  ",Font::ChatTitle);c.text(x,panel.y+8,mm_kbd::accentBar(mm_kbd::Lang(s.kbdLang),s.accentTaps," "),Font::ChatTitle,FG,panel.x+panel.w-8-x);}
 if(!s.quickSent&&s.quickInput.size()>MessageCounterThreshold){auto count=std::to_string(s.quickInput.size())+"/"+std::to_string(s.quickMessageLimit);int width=c.width(count,Font::ChatMeta);c.text(panel.x+panel.w-8-width,panel.y+11,count,Font::ChatMeta);}
 c.text(panel.x+8,panel.y+31,s.quickChannel,Font::ChatMeta,FG,panel.w-16);
 c.text(panel.x+8,panel.y+49,senderDisplayName(s.quickSender),Font::ChatBody,FG,panel.w-16);
 auto preview=c.lines(s.quickPreview,panel.w-16,Font::ChatMeta);for(int i=0;i<std::min(2,int(preview.size()));i++)c.text(panel.x+8,panel.y+69+i*13,preview[i],Font::ChatMeta,FG,panel.w-16);
 if(s.quickSent){Rect confirmation={QuickReplyComposer.x,QuickReplyComposer.y+(QuickReplyComposer.h-24)/2,QuickReplyComposer.w,24};c.card(confirmation);c.centered(confirmation.x+7,confirmation.y+(confirmation.h-c.lineHeight(Font::ChatMeta))/2,confirmation.w-14,"Message sent",Font::ChatMeta);return;}
 c.card(QuickReplyComposer);auto rows=c.lines(s.quickInput,QuickReplyComposer.w-14,Font::ChatBody);int trailing=0;for(auto i=s.quickInput.rbegin();i!=s.quickInput.rend()&&*i==' ';++i)trailing++;int tailWidth=trailing*c.width(" ",Font::ChatBody);if(c.width(rows.back(),Font::ChatBody)+tailWidth>QuickReplyComposer.w-14){rows.push_back("");tailWidth=0;}int visible=(QuickReplyComposer.h-12)/c.lineHeight(Font::ChatBody),first=std::max(0,int(rows.size())-visible);
 for(int i=first;i<int(rows.size());i++)c.text(QuickReplyComposer.x+7,QuickReplyComposer.y+6+(i-first)*c.lineHeight(Font::ChatBody),rows[i],Font::ChatBody,FG,QuickReplyComposer.w-14);
 if(s.quickReplyEditable){int y=QuickReplyComposer.y+6+(int(rows.size())-first-1)*c.lineHeight(Font::ChatBody);g.drawFastVLine(QuickReplyComposer.x+7+c.width(rows.back(),Font::ChatBody)+tailWidth,y,12,FG);}
 c.text(panel.x+8,QuickReplySend.y-16,s.quickError.empty()?"Enter: send / Back: cancel":s.quickError,Font::ChatMeta,FG,panel.w-16);
 c.button(QuickReplySend,"SEND",true);c.button(QuickReplyCancel,s.quickPending?"CLOSE":"CANCEL");
}
void Renderer::home(const State &s){
 g.fillScreen(BG);c.statusBar(s.status);
 // These header strings are ASCII. Some glyph boxes contain blank edge pixels;
 // use the font bitmap itself so dates and narrow digits center by visible ink.
 auto centeredInk=[&](Rect area,const std::string &text,Font face){
  auto value=c.fit(text,area.w,face);auto f=Components::font(face);int pen=0,left=0,right=0,top=0,bottom=0;bool ink=false;
  for(unsigned char ch:value){if(ch<f->first||ch>f->last)continue;const auto &glyph=f->glyph[ch-f->first];for(int y=0;y<glyph.height;y++)for(int x=0;x<glyph.width;x++){int bit=y*glyph.width+x;if(!(f->bitmap[glyph.bitmapOffset+(bit>>3)]&(128>>(bit&7))))continue;int px=pen+glyph.xOffset+x,py=glyph.yOffset+y;if(!ink){left=right=px;top=bottom=py;ink=true;}else{left=std::min(left,px);right=std::max(right,px);top=std::min(top,py);bottom=std::max(bottom,py);}}pen+=glyph.xAdvance;}
  if(ink)c.text(area.x+(area.w-(right-left+1))/2-left,area.y+(area.h-(bottom-top+1))/2-top-Components::ascent(face),value,face);
 };
 centeredInk(MenuHomeClock,s.status.clock,Font::MenuClock);
 centeredInk(MenuHomeDate,s.menuDate,Font::ChatMeta);
 centeredInk(MenuHomeStatus,s.status.acquiringTime?"Acquiring time":std::to_string(s.nodeCount)+" contacts",Font::ChatMeta);
 g.drawFastHLine(6,73,W-12,FG);
 auto conversations=s.conversations;std::stable_sort(conversations.begin(),conversations.end(),[](const Conversation &a,const Conversation &b){return a.activity>b.activity;});
 int first=std::max(0,std::min(s.scroll,std::max(0,int(conversations.size())-MenuHomeRows)));
 for(int row=0;row<MenuHomeRows&&first+row<int(conversations.size());row++){
  auto &item=conversations[first+row];auto r=menuHomeRowRect(row);c.card(r,false,5);
  int textX=r.x+8;c.text(textX,r.y+7,item.name,Font::ChatBody,FG,r.w-50);
  auto age=item.age=="--"?item.time:item.age;c.text(r.x+r.w-36,r.y+7,age,Font::ChatMeta,FG,32);
  std::string preview=item.sender.empty()?item.preview:senderDisplayName(item.sender)+": "+item.preview;
  auto lines=c.lines(preview,r.w-16-(item.unread?24:0),Font::ChatMeta);if(lines.size()>2){lines.resize(2);lines.back()=c.fit(lines.back()+"...",r.w-16-(item.unread?24:0),Font::ChatMeta);}
  for(unsigned i=0;i<lines.size();i++)c.text(textX,r.y+26+i*13,lines[i],Font::ChatMeta);
  if(item.unread){int bx=r.x+r.w-13,by=r.y+r.h-14;g.fillCircle(bx,by,9,FG);c.centered(bx-9,by-4,18,item.unread>99?"99+":std::to_string(item.unread),Font::MenuLabel,BG);}
 }
 if(int(conversations.size())>MenuHomeRows){int height=menuHomeRowRect(MenuHomeRows-1).y+MenuHomeRowH-MenuHomeRowY,thumb=std::max(8,height*MenuHomeRows/int(conversations.size()));g.drawFastVLine(W-3,MenuHomeRowY,height,FG);g.fillRect(W-4,MenuHomeRowY+first*(height-thumb)/(int(conversations.size())-MenuHomeRows),3,thumb,FG);}
 if(conversations.empty())c.centered(6,MenuHomeRowY+40,W-12,"No conversations",Font::ChatBody);
 for(int tab=0;tab<MenuHomeTabCount;tab++){auto r=menuHomeTabRect(tab);bool selected=(s.focus<0?0:s.focus)==tab;c.card(r,selected,3);auto ink=selected?BG:FG;c.packIcon(HomeTabs[tab].icon,r.x+(r.w-MenuHomeTabIcon)/2,r.y+3,MenuHomeTabIcon,ink);c.centered(r.x+1,r.y+MenuHomeTabLabelY,r.w-2,HomeTabs[tab].label,Font::NavLabel,ink);
  if(tab==0&&s.unread){int cx=r.x+r.w-9,cy=r.y+9;g.fillCircle(cx,cy,7,ink);c.centered(cx-7,cy-4,14,s.unread>99?"99+":std::to_string(s.unread),Font::MenuLabel,selected?FG:BG);}
 }
}
void Renderer::launcher(const State &s){
 shell(s,"App",true);g.fillRect(ComposeButton.x,StatusH,ComposeButton.w,HeaderH-1,BG);
 for(int i=0;i<ApplicationCount;i++){auto r=appRect(i);bool focused=s.focus==i;auto ink=focused?BG:FG;c.card(r,focused,5);auto label=c.lines(Applications[i].label,r.w-6,Font::ChatMeta);if(label.size()>2)label.resize(2);int top=label.size()>1?6:12;c.packIcon(Applications[i].icon,r.x+(r.w-32)/2,r.y+top,32,ink);for(unsigned l=0;l<label.size();l++)c.centered(r.x+2,r.y+top+44+l*14,r.w-4,label[l],Font::ChatMeta,ink);}

}
// Thin track + 3 px thumb at the right edge (same as Home and Settings): first..first+visible of total.
static void scrollBar(Adafruit_GFX &g,int y,int h,int first,int visible,int total){
 if(total<=visible||h<=0)return;int thumb=std::max(12,h*visible/total),max=total-visible;first=std::max(0,std::min(first,max));
 g.drawFastVLine(W-3,y,h,FG);g.fillRect(W-4,y+first*(h-thumb)/max,3,thumb,FG);}
void Renderer::messages(const State &s){
 shell(s,"",true,Icon::Menu);c.text(34,StatusH+7,"Mesh Messages",Font::ChatTitle,FG,W-70);
 c.iconButton(MessageMenu,Icon::Menu,s.messageMenu||s.focus<0);
 int rows=ListRows,startY=Top+4;if(s.searching){c.card({Margin,startY,W-2*Margin,26});c.text(Margin+6,startY+6,s.input.empty()?"Search conversations...":s.input,Font::ChatMeta,FG,W-2*Margin-12);rows--;}
 int focused=0;for(unsigned i=0;i<s.conversations.size();i++)if(s.conversations[i].id==s.focus)focused=i;
 int start=(focused/rows)*rows;
 {Rect top=messageListRect(0,s.searching),last=messageListRect(rows-1,s.searching);scrollBar(g,top.y,last.y+last.h-top.y,start,rows,s.conversations.size());}
 for(int row=0;row<rows&&start+row<int(s.conversations.size());row++){
  const auto &v=s.conversations[start+row];Rect r=messageListRect(row,s.searching);bool selected=v.id==s.focus;c.card(r,selected,5);auto ink=selected?BG:FG;
  c.text(r.x+8,r.y+6,v.name,Font::ChatTitle,ink,r.w-54);
  auto time=v.age=="--"?v.time:v.age;c.text(r.x+r.w-38,r.y+8,time,Font::ChatMeta,ink,32);
  std::string preview=v.sender.empty()?v.preview:senderDisplayName(v.sender)+": "+v.preview;int width=r.w-16-(v.unread?24:0);auto lines=c.lines(preview,width,Font::ChatMeta);
  if(lines.size()>2){lines.resize(2);lines.back()=c.fit(lines.back()+"...",width,Font::ChatMeta);}
  for(unsigned i=0;i<lines.size();i++)c.text(r.x+8,r.y+27+i*13,lines[i],Font::ChatMeta,ink);
  if(v.unread){int cx=r.x+r.w-13,cy=r.y+r.h-14;g.fillCircle(cx,cy,9,ink);c.centered(cx-9,cy-4,18,v.unread>99?"99+":std::to_string(v.unread),Font::MenuLabel,selected?FG:BG);}
 }
 if(s.conversations.empty())c.centered(Margin,startY+40,W-2*Margin,"No matching conversations",Font::ChatMeta);
 g.drawFastHLine(0,FooterY,W,FG);c.text(Margin,FooterY+4,"ALL MESSAGES",Font::ChatMeta);auto status=std::to_string(s.conversations.size())+" CHATS";c.text(W-Margin-c.width(status,Font::ChatMeta),FooterY+4,status,Font::ChatMeta);
 if(s.messageMenu){g.fillRect(MessagesPopup.x,MessagesPopup.y,MessagesPopup.w,MessagesPopup.h,BG);c.card(MessagesPopup);
  const char *titles[]={"New direct chat","Create private channel","Join private channel","Join hashtag channel"},*details[]={"Message a saved contact","Random secret key to share","Name + 32-char secret key","Public, key from the #name"};
  for(int i=0;i<MessagesMenuRows;i++){Rect r={MessagesPopup.x+3,MessagesPopup.y+2+i*MessagesPopupRowH,MessagesPopup.w-6,MessagesPopupRowH-2};bool focused=s.messageMenuFocus==i;auto ink=focused?BG:FG;if(focused)g.fillRoundRect(r.x,r.y,r.w,r.h,Radius,FG);
   if(i==3)c.centered(r.x+2,r.y+9,22,"#",Font::ChatTitle,ink);else c.icon(i==0?Icon::Contacts:i==1?Icon::Plus:Icon::Lock,r.x+5,r.y+12,16,ink);
   c.text(r.x+28,r.y+6,titles[i],Font::ChatBody,ink,r.w-32);c.text(r.x+28,r.y+23,details[i],Font::ChatMeta,ink,r.w-32);
   if(i<MessagesMenuRows-1&&!focused&&s.messageMenuFocus!=i+1)g.drawFastHLine(r.x+26,r.y+r.h+1,r.w-30,FG);}}
}
void Renderer::chat(const State &s){
 bubbleHits.clear();senderHits.clear();
 const int bodyH=c.lineHeight(Font::ChatBody),metaH=c.lineHeight(Font::ChatMeta);
 shell(s,"",true,Icon::Menu);g.fillRect(32,StatusH,W-64,HeaderH-1,BG);// the chat title is drawn below in ChatTitle
 c.text(34,StatusH+7,s.chatName,Font::ChatTitle,FG,W-70);
 g.fillRect(ChatMenu.x,ChatMenu.y,ChatMenu.w,ChatMenu.h-1,BG);for(int i=0;i<3;i++)g.fillCircle(ChatMenu.x+8+i*8,ChatMenu.y+16,2,FG);
 bool showCounter=s.input.size()>MessageCounterThreshold&&!s.audioRecording&&!s.audioTranscribing;
 // Draft (typed or dictated): the composer grows upward (up to ComposerMaxLines wrapped rows) so the
 // whole text, mention included, stays readable; the caret marks where the next character lands and the
 // visible rows follow it when the draft is longer than the card.
 const int inputX=Margin+7,inputWidth=ComposerEmoji.x-inputX-5;std::vector<std::string> inputRows;CaretPlace caret;int caretRowShown=0;
 const size_t caretByte=std::min(s.inputCursor,s.input.size());const size_t caretOffset=Components::display(s.input.substr(0,caretByte)).size();
 if(s.composerExpanded&&!s.input.empty()&&!s.audioRecording&&!s.audioTranscribing&&!s.emojiPicker){inputRows=c.lines(s.input,inputWidth-3,Font::ChatBody);caret=caretPlace(inputRows,Components::display(s.input),caretOffset);
  int start=caretWindowStart(int(inputRows.size()),ComposerMaxLines,caret.row);caretRowShown=caret.row-start;
  if(start>0)inputRows.erase(inputRows.begin(),inputRows.begin()+start);if(int(inputRows.size())>ComposerMaxLines)inputRows.resize(ComposerMaxLines);}
 const int composerH=ComposerH+(inputRows.empty()?0:(int(inputRows.size())-1)*bodyH),composerTop=H-composerH;chatComposerTop=composerTop;
 const int counterY=composerTop-FirstRepeaterH;
 int startY=Top+ChatSubtitleH;int messageBottom=composerTop-(!s.firstRepeater.empty()||showCounter?FirstRepeaterH:0);
 // Each body is wrapped once per frame: the page fit (height) and the drawing reuse it.
 std::vector<std::vector<std::string>> wrapped(s.messages.size());std::vector<char> wrappedReady(s.messages.size(),0);
 auto bodyLines=[&](const Message &m)->const std::vector<std::string>&{size_t i=size_t(&m-s.messages.data());if(!wrappedReady[i]){wrapped[i]=c.lines(m.body,BubbleMaxW-2*BubblePadding,Font::ChatBody);wrappedReady[i]=1;}return wrapped[i];};
 int latestOwn=s.latestOwnMessage;
 if(latestOwn==-2){latestOwn=-1;for(int i=int(s.messages.size())-1;i>=0;i--)if(s.messages[i].mine){latestOwn=i;break;}}
 auto metadata=[&](const Message &m){if(m.mine){if(latestOwn<0||latestOwn>=int(s.messages.size())||&m!=&s.messages[latestOwn])return std::vector<std::string>{};return std::vector<std::string>{m.repeaters<0?"Heard x--":"Heard x"+std::to_string(m.repeaters)};}std::string meta=m.time;if(!m.meta.empty())meta+=" | "+m.meta;if(!s.scope.empty()&&m.meta.find('|')==std::string::npos&&m.meta.find(s.scope)==std::string::npos)meta+=" | "+s.scope;return c.lines(meta,W-2*BubbleMargin,Font::ChatMeta);};
 auto height=[&](const Message &m){return int(bodyLines(m).size())*bodyH+2*BubblePadding+(!s.chatDirect&&!m.mine?metaH:0)+int(metadata(m).size())*metaH+2+BubbleGap;};
 int end=std::max(0,int(s.messages.size())-s.chatOffset),first=end,total=0;
 for(int i=end-1;i>=0;i--){int h=height(s.messages[i]);if(total+h>messageBottom-startY-4){if(i==end-1)first=i;break;}total+=h;first=i;}
 int y=startY+3;
 for(int i=first;i<end;i++){
  const auto &m=s.messages[i];auto lines=bodyLines(m);/* a copy: clipped below */auto meta=metadata(m);int senderH=!s.chatDirect&&!m.mine?metaH:0;
  int room=messageBottom-y-4-int(meta.size())*metaH-BubbleGap-2*BubblePadding-senderH;
  int maxLines=std::max(1,room/bodyH);bool clipped=int(lines.size())>maxLines;
  if(clipped){lines.erase(lines.begin(),lines.end()-maxLines);lines[0]=c.fit(".. "+lines[0],BubbleMaxW-2*BubblePadding,Font::ChatBody);}
  int widest=0;for(auto &line:lines)widest=std::max(widest,c.width(line,Font::ChatBody));if(senderH)widest=std::max(widest,c.width(senderDisplayName(m.sender),Font::ChatMeta));
  int width=std::min(BubbleMaxW,std::max(54,widest+2*BubblePadding));int h=int(lines.size())*bodyH+2*BubblePadding+senderH;
  Rect r={m.mine?W-BubbleMargin-width:BubbleMargin,y,width,h};bubbleHits.push_back({r,i});c.card(r,m.mine,7);auto ink=m.mine?BG:FG;
  int by=y+BubblePadding;if(senderH){senderHits.push_back({{r.x+BubblePadding,by,r.w-2*BubblePadding,senderH},i});c.text(r.x+BubblePadding,by,senderDisplayName(m.sender),Font::ChatMeta,ink,r.w-2*BubblePadding);by+=senderH;}
  for(auto &line:lines){c.text(r.x+BubblePadding,by,line,Font::ChatBody,ink,r.w-2*BubblePadding);by+=bodyH;}
  by=y+h+2;for(auto &line:meta){int x=m.mine?W-BubbleMargin-c.width(line,Font::ChatMeta):BubbleMargin;c.text(x,by,line,Font::ChatMeta,FG);by+=metaH;}
  y=by+BubbleGap;
 }
 {int shown=end-first,total=s.chatTotal>0?s.chatTotal:int(s.messages.size()),offset=s.chatTotal>0?s.chatPosition:s.chatOffset;// oldest at the top of the track
  scrollBar(g,startY+2,messageBottom-startY-4,total-offset-shown,shown,total);}
 if(s.messages.empty()){if(s.chatDirect&&s.historyError.empty()&&!s.historyLoading){int mid=(startY+messageBottom)/2;c.centered(Margin,mid-18,W-2*Margin,"Start a new conversation",Font::ChatBody);c.centered(Margin,mid+2,W-2*Margin,c.fit(s.chatName,W-2*Margin,Font::ChatMeta),Font::ChatMeta);}
  else c.centered(Margin,startY+30,W-2*Margin,!s.historyError.empty()?s.historyError:s.historyLoading?"Loading SD history...":"No messages yet",Font::ChatMeta);}
 if(!s.firstRepeater.empty()||showCounter){
  g.fillRect(0,counterY,W,FirstRepeaterH,BG);
  auto count=showCounter?std::to_string(s.input.size())+"/"+std::to_string(s.messageLimit):std::string();
  int counterWidth=c.width(count,Font::ChatMeta);
  if(!s.firstRepeater.empty())c.text(Margin,counterY+2,s.firstRepeater,Font::ChatMeta,FG,W-2*Margin-(showCounter?counterWidth+Gap:0));
  if(showCounter)c.text(W-Margin-counterWidth,counterY+2,count,Font::ChatMeta);
 }
 g.fillRect(0,composerTop,W,composerH,BG);Rect composer={Margin,composerTop+1,W-2*Margin,composerH-3};c.card(composer,false,6);
 if(!inputRows.empty()){int y=composer.y+9;for(auto &row:inputRows){c.text(inputX,y,row,Font::ChatBody);y+=bodyH;}
  const auto &caretRow=inputRows[std::min(caretRowShown,int(inputRows.size())-1)];g.drawFastVLine(inputX+c.rawWidth(caretRow.substr(0,std::min(caret.column,caretRow.size())),Font::ChatBody)+1,composer.y+8+caretRowShown*bodyH,15,FG);}
 else if(s.audioRecording)c.text(inputX,composer.y+9,s.audioWarming?std::string("Get ready..."):"Recording... "+std::to_string(s.audioSeconds)+"/"+std::to_string(device::DICTATION_MAX_SECONDS)+"s",Font::ChatBody,FG,inputWidth);
 else if(s.audioTranscribing)c.text(inputX,composer.y+9,"Transcribing...",Font::ChatBody,FG,inputWidth);
 else if(s.input.empty())c.text(inputX,composer.y+9,"Write message...",Font::ChatBody,FG,inputWidth);
 else {auto tail=c.inputTail(s.input.substr(0,caretByte),inputWidth-3,Font::ChatBody);int caretX=inputX+c.width(tail,Font::ChatBody)+1;c.text(inputX,composer.y+9,tail,Font::ChatBody);
  if(caretByte<s.input.size()&&caretX+4<inputX+inputWidth)c.text(caretX+3,composer.y+9,s.input.substr(caretByte),Font::ChatBody,FG,inputX+inputWidth-caretX-3);g.drawFastVLine(caretX,composer.y+8,15,FG);}
 g.drawFastVLine(ComposerEmoji.x-2,composer.y+5,composer.h-10,FG);{constexpr int B=mm_emoji_picker::ButtonSize;if(s.emojiPicker)g.fillRoundRect(ComposerEmoji.x+2,ComposerEmoji.y+2,ComposerEmoji.w-4,ComposerEmoji.h-5,5,FG);g.drawBitmap(ComposerEmoji.x+(ComposerEmoji.w-B)/2,ComposerEmoji.y+(ComposerEmoji.h-B)/2-1,mm_emoji_picker::button,B,B,s.emojiPicker?BG:FG);}
 // Accent key held or armed (left Shift): the accented letters show above the composer, the emoji button shows "\xc3\xa0".
 // Accent armed or held: the letters the next key gives in the active keyboard language (none: no bar).
 auto accentLetters=s.accent?mm_kbd::accentBar(mm_kbd::Lang(s.kbdLang),s.accentTaps,"  "):std::string();
 if(!accentLetters.empty()){Rect bar={composer.x,composer.y-32,composer.w,28};g.fillRoundRect(bar.x,bar.y,bar.w,bar.h,8,FG);c.centered(bar.x,bar.y+(bar.h-c.lineHeight(Font::ChatTitle))/2,bar.w,accentLetters,Font::ChatTitle,BG);
  g.fillRoundRect(ComposerEmoji.x+2,ComposerEmoji.y+2,ComposerEmoji.w-4,ComposerEmoji.h-5,5,FG);c.centered(ComposerEmoji.x,ComposerEmoji.y+(ComposerEmoji.h-c.lineHeight(Font::ChatTitle))/2,ComposerEmoji.w,"\xc3\xa0",Font::ChatTitle,BG);}
 if(s.emojiPicker){
  g.fillRect(0,EmojiPopup.y-3,W,ComposerY-EmojiPopup.y+3,BG);c.card(EmojiPopup,false,6);
  for(int t=0;t<EmojiTabs;t++){Rect r=emojiTabRect(t);bool on=s.emojiTab==t;c.card(r,on,4);auto ink=on?BG:FG;if(t)c.emoji(mm_emoji_picker::glyphs[mm_emoji_picker::categories[t-1].first].code,r.x+(r.w-14)/2,r.y+(r.h-14)/2,14,ink);else c.icon(Icon::Clock,r.x+(r.w-16)/2,r.y+(r.h-16)/2,16,ink);}
  g.drawFastHLine(EmojiPopup.x+4,EmojiPopup.y+31,EmojiPopup.w-8,FG);
  std::vector<int> items;if(s.emojiTab==0)items=s.emojiRecent;else{auto &cat=mm_emoji_picker::categories[s.emojiTab-1];for(int i=0;i<cat.count;i++)items.push_back(cat.first+i);}
  for(int i=0;i<int(items.size())&&i<EmojiPageSize;i++){Rect r=emojiChoiceRect(i);bool on=s.emojiFocus==i;if(on)c.card(r,true,5);g.drawBitmap(r.x+(r.w-24)/2,r.y+(r.h-24)/2,mm_emoji_picker::glyphs[items[i]].pixels,24,24,on?BG:FG);}
  int footY=EmojiPopup.y+EmojiPopup.h-15;c.text(EmojiPopup.x+8,footY,s.emojiTab?mm_emoji_picker::categories[s.emojiTab-1].label:"Most used",Font::ChatMeta);std::string hint="N/M tab  ENTER add";c.text(EmojiPopup.x+EmojiPopup.w-8-c.width(hint,Font::ChatMeta),footY,hint,Font::ChatMeta);
 }
 if(s.messageMenu){g.fillRect(0,Top,W,H-Top,BG);Rect info={Margin,Top+4,W-2*Margin,Bottom-Top-8};g.fillRect(info.x,info.y,info.w,info.h,BG);c.card(info);c.text(info.x+8,info.y+8,"Chat options",Font::ChatTitle);const char *labels[]={"Region scope","Add region scope","Clear chat","Send GPS position"};for(int i=0;i<ChatOptionCount;i++)c.button(chatOptionRect(i),labels[i],s.chatMenuFocus==i);}
 if(s.scopePicker){
  Rect panel=overlayPanel();c.text(Margin+8,Top+12,s.notice.empty()?"Region scope":s.notice,s.notice.empty()?Font::ChatTitle:Font::ChatMeta,FG,panel.w-16);
  int offset=std::max(0,std::min(s.scopeOffset,std::max(0,int(s.regionScopes.size())-1)));
  for(int i=0;i<ScopeRows&&offset+i<int(s.regionScopes.size());i++){auto row=scopeRowRect(i);bool selected=offset+i==s.scopeFocus;c.button(row,s.regionScopes[offset+i]=="*"?"Global (*)":s.regionScopes[offset+i],selected);}
  c.button(ScopeAdd,"+ Add new scope");c.button(ScopeApply,"APPLY");c.button(ScopeCancel,"CANCEL");
  if(s.scopeEditing){g.fillRect(0,Top,W,H-Top,BG);c.card(panel);c.text(Margin+8,Top+12,"Add region scope",Font::ChatTitle);c.text(Margin+8,Top+46,"Example: it-lig",Font::ChatMeta);c.text(Margin+8,Top+62,"Letters, numbers and hyphens",Font::ChatMeta,FG,panel.w-16);c.card(ScopeInput);c.text(ScopeInput.x+7,ScopeInput.y+10,s.scopeInput.empty()?"Type a region...":s.scopeInput,Font::ChatBody,FG,ScopeInput.w-14);c.button(ScopeSave,"SAVE");c.button(ScopeEditCancel,"CANCEL");if(!s.notice.empty())c.text(Margin+8,Top+185,s.notice,Font::ChatMeta,FG,panel.w-16);}
 }
 if(s.clearChatConfirm){Rect panel=overlayPanel();c.text(Margin+8,Top+15,"Clear all messages?",Font::ChatTitle,FG,panel.w-16);c.text(Margin+8,Top+56,"Channel and scope are kept.",Font::ChatMeta,FG,panel.w-16);c.button(ChatClearYes,"CLEAR CHAT");c.button(ChatClearNo,"CANCEL");if(!s.notice.empty())c.text(Margin+8,Top+215,s.notice,Font::ChatMeta,FG,panel.w-16);}

 if(s.positionShare){
  Rect panel=overlayPanel();c.text(Margin+8,Top+15,"Share position",Font::ChatTitle,FG,panel.w-16);
  c.text(Margin+8,Top+49,s.positionCoordinates.empty()?"Live GPS required":"Current GPS coordinates",Font::ChatMeta,FG,panel.w-16);
  auto lines=c.lines(s.positionCoordinates.empty()?s.positionMessage:s.positionCoordinates,panel.w-16,Font::ChatBody);
  for(int i=0;i<std::min(4,int(lines.size()));i++)c.text(Margin+8,Top+84+i*c.lineHeight(Font::ChatBody),lines[i],Font::ChatBody,FG,panel.w-16);
  std::string label=!s.positionCoordinates.empty()?"SEND POSITION":!s.gpsAvailable?"GPS UNAVAILABLE":!s.gpsEnabled?"ENABLE GPS":"WAITING FOR GPS";
  c.button(PositionSend,label,!s.positionCoordinates.empty());c.button(PositionCancel,"CANCEL");
  if(!s.notice.empty())c.text(Margin+8,Bottom-15,s.notice,Font::ChatMeta,FG,panel.w-16);
 }
 if(s.messageActions){
 if(s.contextMessage.mine){
  g.fillRect(0,Top,W,H-Top,BG);
  Rect panel={Margin,Top+4,W-2*Margin,Bottom-Top-8};g.fillRect(panel.x,panel.y,panel.w,panel.h,BG);c.card(panel);
  c.text(panel.x+8,Top+12,"Your message",Font::ChatTitle);
  c.button(ownMessageActionRect(0),"RESEND");
  c.text(panel.x+8,Top+76,"REPEATERS HEARD / "+(s.contextMessage.repeaters<0?std::string("--"):std::to_string(s.contextMessage.repeaters)),Font::ChatMeta);
  int start=std::max(0,std::min(s.heardOffset,std::max(0,int(s.heardPeers.size())-HeardRows)));
  for(int i=0;i<HeardRows&&start+i<int(s.heardPeers.size());i++)c.text(panel.x+8,HeardListY+i*HeardRowH,s.heardPeers[start+i],Font::ChatMeta,FG,panel.w-16);
  if(s.heardPeers.empty())c.text(panel.x+8,HeardListY,s.contextMessage.repeaters>0?"Names not recorded":"No repeater echoes recorded",Font::ChatMeta,FG,panel.w-16);
  c.text(panel.x+8,Top+150,s.heardPeers.size()>HeardRows?"Swipe / J K: "+std::to_string(start+1)+"-"+std::to_string(std::min(start+HeardRows,int(s.heardPeers.size())))+" of "+std::to_string(s.heardPeers.size()):"Echoes / not a delivery ACK",Font::ChatMeta,FG,panel.w-16);
  c.button(ownMessageActionRect(4),"QUOTE");c.button(ownMessageActionRect(2),"PATH");c.button(ownMessageActionRect(1),s.channelMuted?"UNMUTE":"MUTE");c.button(ownMessageActionRect(3),"CLOSE");
  if(!s.notice.empty())c.text(panel.x+8,Bottom-18,s.notice,Font::ChatMeta,FG,panel.w-16);
 }else{
  Rect panel=overlayPanel();
  c.text(panel.x+8,panel.y+9,senderDisplayName(s.contextMessage.sender),Font::ChatTitle,FG,panel.w-16);
  const char *labels[]={"Mention / reply","View full path","Read message",s.channelMuted?"Unmute notifications":"Mute notifications","Close"};
  for(int row=0;row<ReceivedMessageActions;row++){Rect r=messageActionRect(row);if(row==1){r.w-=MessageMapW+Gap;c.button(messageMapRect(),"MAP");}c.button(r,labels[row]);}

 }
 }
 if(s.messagePath&&s.messageRouteView){
  // Static packet route: low-detail map, numbered hops, legend underneath. No controls.
  g.fillRect(0,Top,W,H-Top,BG);mapView(s);int x=Margin,y=MapArea.y+MapArea.h+3,line=0;
  for(auto &entry:s.mapLegend){int w=c.width(entry,Font::Tiny);if(x>Margin&&x+w>W-Margin){if(++line>=3)break;x=Margin;y+=12;}c.text(x,y,entry,Font::Tiny,FG,W-2*Margin);x+=w+10;}
 }else if(s.messagePath){
  g.fillRect(0,Top,W,H-Top,BG);
  Rect panel={Margin,Top+4,W-2*Margin,Bottom-Top-8};g.fillRect(panel.x,panel.y,panel.w,panel.h,BG);c.card(panel);
  c.text(Margin+8,Top+12,s.messageBodyView?"Message":s.contextMessage.mine?"Repeaters heard":"Received path",Font::ChatTitle);
  auto lines=c.lines(s.pathText,panel.w-16,Font::ChatBody);int rows=(panel.h-70)/c.lineHeight(Font::ChatBody);
  int start=std::min(s.pathOffset,std::max(0,int(lines.size())-rows));
  for(int i=0;i<rows&&start+i<int(lines.size());i++)c.text(Margin+8,Top+40+i*c.lineHeight(Font::ChatBody),lines[start+i],Font::ChatBody);
  c.button({Margin+8,Bottom-40,58,28},"UP");c.button({Margin+72,Bottom-40,58,28},"DOWN");c.button({Margin+136,Bottom-40,72,28},"CLOSE");
 }

}

void Renderer::mapView(const State &s){
 // 1 bpp OpenStreetMap view, then pins (repeater triangle, companion dot), own position, scale, status, credit.
 // Maps app: the whole page under the status bar; packet route: MapArea under the header.
 const Rect A=s.mapFull?MapPageArea:MapArea;
 g.fillRect(A.x,A.y,A.w,A.h,BG);if(s.mapBits)g.drawBitmap(A.x,A.y,s.mapBits,A.w,A.h,FG);
 auto label=[&](int x,int y,const std::string &text){int w=c.width(text,Font::Tiny)+4;x=std::max(A.x,std::min(x,A.x+A.w-w));g.fillRect(x,y,w,11,BG);c.text(x+2,y+1,text,Font::Tiny);};
 // Packet route: white halo then a 2 px black line, dashed across hops with no known position.
 for(auto &seg:s.mapRoute){int dx=seg.x1-seg.x0,dy=seg.y1-seg.y0,len=std::max(std::abs(dx),std::abs(dy));
  for(int pass=0;pass<2;pass++)for(int i=0;i<=len;i++){if(seg.dashed&&(i/6)%2)continue;int x=seg.x0+(len?dx*i/len:0),y=seg.y0+(len?dy*i/len:0);if(pass==0)g.fillRect(x-2,y-2,5,5,BG);else g.fillRect(x-1,y-1,2,2,FG);}}
 for(auto &pin:s.mapPins){if(pin.step>=0){if(pin.step==0){g.fillRect(pin.x-7,pin.y-7,15,15,BG);g.fillRect(pin.x-5,pin.y-5,11,11,FG);}else{g.fillCircle(pin.x,pin.y,9,BG);g.fillCircle(pin.x,pin.y,7,FG);std::string n=std::to_string(pin.step);c.text(pin.x-c.width(n,Font::Tiny)/2+1,pin.y-4,n,Font::Tiny,BG);}
   if(!pin.name.empty())label(pin.x+10,pin.y-5,pin.name);continue;}
  if(pin.repeater){g.fillTriangle(pin.x,pin.y-9,pin.x-8,pin.y+6,pin.x+8,pin.y+6,BG);g.fillTriangle(pin.x,pin.y-6,pin.x-5,pin.y+4,pin.x+5,pin.y+4,FG);}else{g.fillCircle(pin.x,pin.y,6,BG);g.fillCircle(pin.x,pin.y,4,FG);}
  if(s.mapPins.size()<=8&&!pin.name.empty())label(pin.x+7,pin.y-5,pin.name);}
 if(s.mapSelfX>=0){int x=s.mapSelfX,y=s.mapSelfY;g.fillCircle(x,y,11,BG);for(int r=8;r<=9;r++)g.drawCircle(x,y,r,FG);g.fillCircle(x,y,4,FG);for(int d:{-1,0,1}){g.drawFastHLine(x-14,y+d,4,FG);g.drawFastHLine(x+11,y+d,4,FG);g.drawFastVLine(x+d,y-14,4,FG);g.drawFastVLine(x+d,y+11,4,FG);}}
 if(!s.mapStatus.empty())label(A.x+(s.mapFull?42:2),A.y+(s.mapFull?15:2),s.mapStatus);
 if(s.mapFull&&!s.notice.empty())label(A.x+42,A.y+28,s.notice);// e.g. "No GPS position": no footer on the full-page map
 // OpenStreetMap license: credit in the bottom-left corner, the scale bar just above it.
 label(A.x+2,A.y+A.h-12,"(c) OpenStreetMap");
 if(s.mapScalePx>0){int x=A.x+4,y=A.y+A.h-18;g.fillRect(x-2,y-13,std::max(s.mapScalePx,c.width(s.mapScaleLabel,Font::Tiny))+6,17,BG);g.drawFastHLine(x,y,s.mapScalePx,FG);g.drawFastHLine(x,y+1,s.mapScalePx,FG);g.drawFastVLine(x,y-3,5,FG);g.drawFastVLine(x+s.mapScalePx-1,y-3,5,FG);c.text(x,y-12,s.mapScaleLabel,Font::Tiny);}
 if(!s.mapFull)g.drawFastHLine(0,A.y+A.h,W,FG);
}
void Renderer::channelOptions(const State &s){
 shell(s,s.channelOptionName,true,Icon::Menu);g.fillRect(ComposeButton.x,StatusH,ComposeButton.w,HeaderH-1,BG);
 if(s.channelKeyView){channelKey(s);return;}
 Rect panel={Margin,Top+4,W-2*Margin,Bottom-Top-8};c.card(panel);
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
 int n=s.qrSize,scale=n?std::min(4,170/(n+4)):0,side=(n+4)*scale,x0=(W-side)/2,y0=Top+4;
 if(n&&int(s.qrModules.size())==n*n){g.fillRect(x0,y0,side,side,BG);for(int y=0;y<n;y++)for(int x=0;x<n;x++)if(s.qrModules[y*n+x])g.fillRect(x0+(x+2)*scale,y0+(y+2)*scale,scale,scale,FG);}
 else c.centered(Margin,y0+60,W-2*Margin,"QR unavailable",Font::ChatMeta);
 int y=y0+std::max(side,120)+4;c.centered(Margin,y,W-2*Margin,c.fit(s.channelOptionName,W-2*Margin,Font::ChatTitle),Font::ChatTitle);
 for(int l=0;l<2&&l*16<int(s.channelKeyHex.size());l++)c.centered(Margin,y+22+l*16,W-2*Margin,channelSecretGroups(s.channelKeyHex,l*16,16),Font::Body);
 c.centered(Margin,y+58,W-2*Margin,"Scope "+(s.channelKeyScope.empty()||s.channelKeyScope=="*"?std::string("all regions"):s.channelKeyScope)+" / scan with MeshCore",Font::ChatMeta);
 c.centered(Margin,y+74,W-2*Margin,"Key holders can read and send",Font::ChatMeta);
}
// First-boot welcome (e-paper portrait): Wi-Fi and clock, radio band, network preset, companion name, path hash size, summary.
void Renderer::welcome(const State &s){
 g.fillScreen(BG);c.statusBar(s.status);
 const char *titles[]={"Welcome to MatterMesh","Wi-Fi and clock","Radio band","Network preset","Companion name","Path hash size","Ready"};
 const char *hints[]={"Off-grid chat over LoRa, on the MeshCore network.","Wi-Fi sets the clock, updates and weather.","Pick the LoRa band of your radio and country.","The settings your local mesh uses.","How other nodes will see you.","Bytes used for each repeater in a route.","Check and confirm. You can change it later in LoRa."};
 int step=std::max(0,std::min(6,s.welcomeStep));
 // Word wrap into at most `rows` lines of width w (Components::lines also splits a word wider than w).
 auto wrap=[&](const std::string &text,int w,Font f,int rows){auto out=c.lines(text,w,f);if(int(out.size())>rows)out.resize(rows);return out;};
 c.text(Margin,StatusH+8,titles[step],Font::ChatTitle,FG,W-2*Margin);
 {auto lines=wrap(hints[step],W-2*Margin,Font::ChatMeta,2);for(size_t i=0;i<lines.size();i++)c.text(Margin,StatusH+32+int(i)*14,lines[i],Font::ChatMeta);}
 auto choice=[&](Rect r,const std::string &big,const std::string &small,bool on){c.card(r,on,8);uint16_t ink=on?BG:FG;c.text(r.x+12,r.y+(r.h>56?12:8),big,Font::ChatTitle,ink,r.w-24);
  auto lines=wrap(small,r.w-24,Font::ChatMeta,r.h>56?2:1);for(size_t k=0;k<lines.size();k++)c.text(r.x+12,r.y+(r.h>56?36:30)+int(k)*13,lines[k],Font::ChatMeta,ink,r.w-24);};
 if(step==0){
  c.icon(Icon::Radio,W/2-16,WelcomeBodyY+6,32);
  c.centered(Margin,WelcomeBodyY+50,W-2*Margin,"Let's set up your device",Font::ChatBody);
  int y=WelcomeBodyY+74;for(auto &l:wrap("Wi-Fi, band, preset, name and route bytes. Arrows or touch to choose, ENTER to continue.",W-2*Margin-8,Font::ChatMeta,3)){c.centered(Margin,y,W-2*Margin,l,Font::ChatMeta);y+=14;}
  if(!s.welcomeArchive.empty()){Rect r={Margin,y+6,W-2*Margin,50};c.card(r,false,8);int ty=r.y+5;
   for(auto &l:wrap("Data of a previous installation was moved to "+s.welcomeArchive+" on the microSD. Nothing was deleted.",r.w-16,Font::ChatMeta,3)){c.text(r.x+8,ty,l,Font::ChatMeta);ty+=13;}}
 }else if(step==1){
  choice(welcomeWifiCard(0),s.wifiConnected?"Connected":"Connect to Wi-Fi",s.wifiConnected?s.wifiSSID:"Choose a network",s.welcomeWifiChoice==0);
  choice(welcomeWifiCard(1),"Skip for now","Set it up later",s.welcomeWifiChoice==1);
  Rect r=welcomeWifiCard(2);r.h=WelcomeNext.y-34-r.y;c.card(r,false,8);c.icon(Icon::Clock,r.x+8,r.y+7,16);c.text(r.x+30,r.y+7,"How the clock is set",Font::Bold,FG,r.w-38);
  const char *lines[]={"Wi-Fi: from the internet.","No Wi-Fi: GPS, outdoors.","Or by hand: Settings > Date / time."};
  for(int i=0;i<3;i++)c.text(r.x+10,r.y+27+i*14,lines[i],Font::ChatMeta,FG,r.w-20);
 }else if(step==2){choice(welcomeCard(0,2),"868 MHz","Europe, UK, Switzerland",s.welcomeBand==868);choice(welcomeCard(1,2),"900 MHz","Americas, Oceania, Asia",s.welcomeBand==900);}
 else if(step==3){
  for(int i=0;i<WelcomeRows&&s.welcomeScroll+i<int(s.welcomePresets.size());i++){int at=s.welcomeScroll+i;Rect r=welcomeRow(i);bool on=at==s.welcomeFocus;c.card(r,on,6);uint16_t ink=on?BG:FG;
   c.text(r.x+8,r.y+3,s.welcomePresets[at].first,Font::ChatBody,ink,r.w-16);c.text(r.x+8,r.y+20,s.welcomePresets[at].second,Font::ChatMeta,ink,r.w-16);}
  int n=s.welcomePresets.size();if(n>WelcomeRows){int track=WelcomeRows*WelcomeRowH-4,thumb=std::max(12,track*WelcomeRows/n),max=n-WelcomeRows;g.fillRect(W-4,WelcomeBodyY+std::min(s.welcomeScroll,max)*(track-thumb)/max,3,thumb,FG);}
 }else if(step==4){
  Rect f=WelcomeNameField;g.drawRoundRect(f.x,f.y,f.w,f.h,8,FG);g.drawRoundRect(f.x+1,f.y+1,f.w-2,f.h-2,7,FG);auto tail=c.inputTail(s.welcomeName,f.w-24,Font::ChatBody);c.text(f.x+10,f.y+9,tail,Font::ChatBody);g.fillRect(f.x+11+c.width(tail,Font::ChatBody),f.y+8,2,18,FG);
  int y=f.y+f.h+10;for(auto &l:wrap("Letters, digits and spaces; up to 31 bytes. Type on the keyboard, BACKSPACE deletes.",W-2*Margin-4,Font::ChatMeta,3)){c.text(Margin+2,y,l,Font::ChatMeta);y+=14;}
 }else if(step==5){const char *small[]={"Most compatible","Balanced","Fewest clashes"};for(int b=1;b<=3;b++)choice(welcomeCard(b-1,3),std::to_string(b)+(b==1?" byte":" bytes"),small[b-1],s.welcomeBytes==b);}
 else{
  Rect r={Margin,WelcomeBodyY,W-2*Margin,5*32+6};c.card(r,false,8);
  const char *labels[]={"WI-FI","BAND","PRESET","NAME","PATH HASH"};std::string values[]={s.wifiConnected?s.wifiSSID:"Not connected",std::to_string(s.welcomeBand)+" MHz",s.welcomePreset,s.welcomeName,std::to_string(s.welcomeBytes)+(s.welcomeBytes==1?" byte":" bytes")};
  for(int i=0;i<5;i++){int y=r.y+4+i*32;c.text(r.x+10,y,labels[i],Font::ChatMeta);c.text(r.x+10,y+14,values[i],Font::ChatBody,FG,r.w-20);}
 }
 for(int i=0;i<7;i++){int x=W/2-3*12+i*12;if(i==step)g.fillCircle(x,WelcomeNext.y-12,4,FG);else if(i<step)g.fillCircle(x,WelcomeNext.y-12,3,FG);else g.drawCircle(x,WelcomeNext.y-12,3,FG);}
 if(!s.notice.empty())c.centered(Margin,WelcomeNext.y-26,W-2*Margin,s.notice,Font::ChatMeta);
 if(step>0)c.button(WelcomeBack,"BACK",false);
 c.button(WelcomeNext,step==0?"START":step==6?"FINISH":step==1&&s.welcomeWifiChoice==0&&!s.wifiConnected?"CONNECT":"NEXT",true);
}
void Renderer::holdBanner(const std::string &label){
 // Same shell as unlockTapBanner: the date/header band, a card with the lock icon, the label and six filled dots.
 g.fillRect(0,HoldBannerY,W,HoldBannerH,BG);
 Rect r={Margin,HoldBannerY+3,W-2*Margin,HoldBannerH-6};c.card(r);
 c.icon(Icon::Lock,r.x+8,r.y+(r.h-16)/2,16);
 c.text(r.x+30,r.y+(r.h-c.lineHeight(Font::ChatMeta))/2,label,Font::ChatMeta,FG,r.w-30-HoldDotsW-8);
 for(int i=0;i<HoldDots;i++)g.fillCircle(r.x+r.w-HoldDotsW+HoldDotR+i*HoldDotStep,r.y+r.h/2,HoldDotR,FG);
}

void Renderer::unlockTapBanner(int taps){
 // Takes the date row's band: a card with the lock icon, the gesture and one dot per SPACE tap.
 taps=std::max(0,std::min(2,taps));g.fillRect(0,HoldBannerY,W,HoldBannerH,BG);
 Rect r={Margin,HoldBannerY+3,W-2*Margin,HoldBannerH-6};c.card(r);
 c.icon(Icon::Lock,r.x+8,r.y+(r.h-16)/2,16);
 c.text(r.x+30,r.y+(r.h-c.lineHeight(Font::ChatMeta))/2,"SPACE twice to unlock",Font::ChatMeta,FG,r.w-74);
 for(int i=0;i<2;i++){int cx=r.x+r.w-30+i*16,cy=r.y+r.h/2;if(i<taps)g.fillCircle(cx,cy,4,FG);else g.drawCircle(cx,cy,4,FG);}
}

// New direct chat: saved companion contacts (no repeaters), search, tap or ENTER opens the chat.
void Renderer::contactPicker(const State &s){
 shell(s,"",true,Icon::Menu);g.fillRect(MessageMenu.x,StatusH,MessageMenu.w,HeaderH-1,BG);c.text(34,StatusH+7,"New direct chat",Font::ChatTitle,FG,W-70);c.iconButton(MessageMenu,Icon::Search,s.pickerSearching);
 c.card(PickerSearch,s.pickerSearching);c.text(PickerSearch.x+8,PickerSearch.y+6,s.pickerQuery.empty()?(s.pickerSearching?"Type a name...":"Search contacts"):s.pickerQuery,Font::ChatMeta,s.pickerSearching?BG:FG,PickerSearch.w-16);
 for(int row=0;row<PickerRows&&row<int(s.pickerRows.size());row++){Rect r=pickerRowRect(row);bool focused=s.pickerStart+row==s.pickerFocus;auto ink=focused?BG:FG;c.card(r,focused);
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
  // Detail by section (not by row), so rows can be added or moved.
  auto detailFor=[&](int section)->std::string{switch(section){
   case 4:return s.wifiConnected?"Connected":s.wifiEnabled?"On, not connected":"Off";case 14:return s.bleReady?"On":"Off";
   case 2:return s.radioReady?"SX1262 ready":"Offline";case 23:return s.radioInternal?"Internal antenna":"External antenna";
   case 3:return s.gpsEnabled?"On":"Off";case 6:return "Lights / haptics";
   case 22:return std::string("Lock screen: ")+mm_wallpaper::list[s.wallpaper>=0&&s.wallpaper<mm_wallpaper::count?s.wallpaper:0].name;
   case KeyboardSection:{std::string codes;for(auto l:mm_kbd::Order)if(s.kbdMask&(1u<<int(l))){if(!codes.empty())codes+=" ";codes+=mm_kbd::info(l).code;}return mm_kbd::enabledCount(s.kbdMask)>1?codes+" (Alt + Space)":"Typing: "+std::string(mm_kbd::info(mm_kbd::Lang(s.kbdLang)).name);}
   case AccentKeySection:return s.accentMode==1?"Right Shift":s.accentMode==2?"Left Shift":"Off";
   case 21:return "Speaker / microphone";case 20:return "Light timers / auto lock";case 5:return s.status.battery<0?"--%":std::to_string(s.status.battery)+"%";
   case 16:return s.timeSource;case 24:return s.sd?"SD available":"No SD";case 11:return "USB diagnostics";case 17:return "Update your firmware";
   case SupportSection:return s.extraAppsUnlocked?"Support the project":"Support the project, Extra apps";default:return "Version "+s.version;}};
  std::string detail=detailFor(SettingsItems[i].section);
  // Full image with the Extra apps unlocked: the entry is only for donations.
  const char *label=SettingsItems[i].section==SupportSection&&s.extraAppsUnlocked?"Donate":SettingsItems[i].label;
  c.menuRow(r,SettingsItems[i].icon,label,detail,s.settingsFocus==i);
 }
 // Scroll indicator like the Home conversation list: thin track + 3 px thumb right of the cards.
 if(int max=settingsMaxScroll();max>0){int track=Bottom-SettingsListY,total=settingsContentY(SettingsCount-1)+SettingsCardH,thumb=std::max(12,track*track/total),offset=std::min(s.settingsOffset,max);
  g.drawFastVLine(W-3,SettingsListY,track,FG);g.fillRect(W-4,SettingsListY+settingsSnapY(offset)*(track-thumb)/settingsSnapY(max),3,thumb,FG);}
}
void Renderer::section(const State &s){
 const char *title="MatterMesh OS";
 switch(s.page){case Page::Antenna:title="Antenna";break;case Page::Storage:title="Storage";break;case Page::Controls:title="Lights / vibration";break;case Page::Audio:title="Sound / dictation";break;case Page::Power:title="Power / timeouts";break;case Page::Wallpaper:title="Wallpaper";break;case Page::Keyboard:title="Keyboard languages";break;case Page::AccentKey:title="Accent key";break;case Page::Mesh:title="Contacts";break;case Page::Radio:title="LoRa";break;case Page::GPS:title="GPS";break;case Page::Maps:title="Maps";break;case Page::Weather:title="Weather";break;case Page::Info:title="About";break;case Page::Support:title=s.extraAppsUnlocked?"Donate":"Donate / Unlock";break;case Page::Files:title="Files";break;case Page::Wifi:title="Wi-Fi";break;case Page::Bluetooth:title="Bluetooth";break;case Page::Terminal:title="Diagnostics";break;case Page::DateTime:title="Date / time";break;case Page::Updates:title="OTA update";break;case Page::Battery:title="Battery";break;default:break;}
 if(s.page==Page::Maps){g.fillScreen(BG);c.statusBar(s.status);}else{shell(s,title);g.fillRect(W-32,StatusH,32,HeaderH-1,BG);}// Maps: no header, the map takes the page
 auto valueCard=[&](Rect r,const std::string &label,const std::string &value){c.card(r);c.text(r.x+7,r.y+4,label,Font::ChatMeta);c.text(r.x+7,r.y+20,value,Font::Bold,FG,r.w-14);};
 auto gb=[](uint64_t bytes){char text[16];if(bytes<1073741824ull)snprintf(text,sizeof(text),"%u MB",unsigned(bytes/1048576));else snprintf(text,sizeof(text),"%.1f GB",bytes/1073741824.0);return std::string(text);};
 if(s.page==Page::Controls){
  const char *names[]={"Display light","Keyboard light"};
  auto header=[&](int y,int row,const char *name,int percent,bool enabled){c.text(Margin+2,y,name,Font::ChatBody);auto value=std::to_string(percent)+"%";auto t=lightToggleRect(row);c.text(t.x-8-c.width(value,Font::ChatMeta),y+2,value,Font::ChatMeta);
   if(enabled)g.fillRoundRect(t.x,t.y,t.w,t.h,11,FG);else g.drawRoundRect(t.x,t.y,t.w,t.h,11,FG);c.centered(t.x,t.y+5,t.w,enabled?"ON":"OFF",Font::Bold,enabled?BG:FG);};
  for(int row=0;row<2;row++){int percent=((row?s.keyboardLight:s.displayLight)*100+127)/255;header(ControlFirstY+row*ControlStep,row,names[row],percent,row?s.keyboardLightEnabled:s.displayLightEnabled);c.slider(lightSliderRect(row),percent);}
  header(HapticY,2,"Vibration",s.vibrationPercent,s.haptic);c.slider(HapticSlider,s.vibrationPercent);
  c.sectionLabel(VibrationMessage.y-16,"PATTERNS","TAP TO FEEL");
  auto pattern=[&](Rect r,Icon id,const char *label,const std::string &name){c.card(r);c.icon(id,r.x+6,r.y+(r.h-16)/2,16);c.text(r.x+28,r.y+(r.h-15)/2,label,Font::ChatBody,FG,96);
   int right=r.x+r.w-56;c.text(right-c.width(name,Font::Bold),r.y+(r.h-13)/2,name,Font::Bold);c.icon(Icon::Back,r.x+r.w-44,r.y+(r.h-12)/2,12);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12);};
  pattern(VibrationMessage,Icon::Messages,"Message",s.messageVibration);pattern(VibrationMention,Icon::Contacts,"Mention",s.mentionVibration);
 }else if(s.page==Page::Mesh){
  // Contacts: ALL / COMP / RPT tabs opening into the panel (LoRa style), search, four rows, pager.
  // A companion row opens its chat.
  g.fillRect(34,StatusH,W-34,HeaderH-1,BG);g.drawFastHLine(0,Top-1,W,BG); // the tabs are the title
  auto tab=[&](Rect r,const char *label,bool selected){c.card(r,false,5);if(selected){g.fillRect(r.x+1,r.y+r.h-5,r.w-2,5,BG);g.drawFastVLine(r.x,r.y+r.h-5,5,FG);g.drawFastVLine(r.x+r.w-1,r.y+r.h-5,5,FG);}c.centered(r.x,r.y+(r.h-13)/2,r.w,label,selected?Font::Bold:Font::Body,FG);};
  auto ago=[](uint32_t sec){return sec<60?std::to_string(sec)+"s":sec<3600?std::to_string(sec/60)+"m":std::to_string(sec/3600)+"h";};
  const char *filters[]={"ALL","COMP","RPT"};for(int i=0;i<ContactTabCount;i++)tab(contactTabRect(i),filters[i],s.filter==i);
  c.card(LoRaPanel);Rect active=contactTabRect(s.filter);g.drawFastHLine(active.x+1,LoRaPanel.y,active.w-2,BG);
  c.card(ContactSearch);auto tail=c.inputTail(s.contactQuery,ContactSearch.w-40,Font::ChatBody);c.text(ContactSearch.x+7,ContactSearch.y+5,s.contactQuery.empty()?(s.contactSearching?"Type a name...":s.filter==2?"Search repeaters":s.filter==1?"Search companions":"Search contacts"):tail,Font::ChatBody,FG,ContactSearch.w-36);
  if(s.contactSearching)g.drawFastVLine(ContactSearch.x+8+c.width(tail,Font::ChatBody),ContactSearch.y+5,16,FG);c.icon(Icon::Search,ContactSearch.x+ContactSearch.w-22,ContactSearch.y+5,16);
  for(int i=0;i<ContactRows&&i<int(s.nodes.size());i++){auto row=contactRowRect(i);auto &n=s.nodes[i];bool selected=s.contactFocus==s.nodeScroll+i;c.card(row,selected);auto ink=selected?BG:FG;
   c.icon(n.type==2?Icon::Radio:Icon::Contacts,row.x+6,row.y+(row.h-16)/2,16,ink);int right=row.x+row.w-6;
   int distW=n.distance.empty()?0:c.width(n.distance,Font::ChatMeta);if(distW)c.text(right-distW,row.y+6,n.distance,Font::ChatMeta,ink);
   c.text(row.x+28,row.y+4,n.name.empty()?std::string("Unknown"):n.name,Font::ChatBody,ink,right-row.x-30-(distW?distW+6:0));
   std::string line;if(n.live){std::string route=n.hops==0?"direct":n.hops>0?std::to_string(n.hops)+(n.hops==1?" hop":" hops"):"via mesh";char text[48];snprintf(text,sizeof(text),"%.0f dBm / %s ago / %s",n.rssi,ago(n.age).c_str(),route.c_str());line=text;}else line="Saved / not heard since boot";
   if(n.live)c.signalBars(right-19,row.y+row.h-9,n.snr,ink);c.text(row.x+28,row.y+22,line,Font::ChatMeta,ink,right-row.x-30-(n.live?24:0));}
  if(s.nodes.empty()){c.centered(LoRaInset,ContactRowY+30,LoRaContentW,s.contactQuery.empty()?"No contacts yet":"No contact matches",Font::Bold);c.centered(LoRaInset,ContactRowY+50,LoRaContentW,!s.radioReady?"Radio offline / check LoRa":s.contactQuery.empty()?"Waiting for advertisements":"Try another name",Font::ChatMeta);}
  if(!s.notice.empty())c.text(LoRaInset,ContactNoticeY,s.notice,Font::ChatMeta,FG,LoRaContentW);
  c.button(ContactPrevious,"<",s.nodeScroll>0);c.button(ContactNext,">",s.nodeScroll+ContactRows<s.filteredNodeCount);
  std::string summary=!s.radioReady?"Radio offline":std::to_string(s.filteredNodeCount)+(s.filteredNodeCount==1?" contact / ":" contacts / ")+std::to_string(s.contactLive)+" live";c.centered(ContactPrevious.x+ContactPrevious.w,ContactPrevious.y+5,ContactNext.x-ContactPrevious.x-ContactPrevious.w,summary,Font::ChatMeta);
 }else if(s.page==Page::Radio){
  const char *labels[]={"Frequency","Bandwidth","Spread factor","Coding rate","TX power","Companion name"};
  g.drawFastHLine(0,Top-1,W,BG);
  auto tab=[&](Rect r,const char *label,bool selected){c.card(r,false,5);g.fillRect(r.x,r.y+r.h-5,r.w,5,BG);g.drawFastVLine(r.x,r.y+r.h-5,5,FG);g.drawFastVLine(r.x+r.w-1,r.y+r.h-5,5,FG);c.centered(r.x,r.y+(r.h-13)/2,r.w,label,selected?Font::Bold:Font::Body,FG);};
  tab(LoRaRadio,"RADIO",!s.radioDevicePage);tab(LoRaDevice,"DEVICE",s.radioDevicePage);
  if(!s.radioPowerEditing){int tab=s.radioDevicePage?(s.radioDeviceFocus==5?0:s.radioDeviceFocus==6?1:-1):(s.radioFocus==8?0:s.radioFocus==9?1:-1);if(tab>=0){auto r=tab?LoRaDevice:LoRaRadio;g.drawFastHLine(r.x+8,r.y+r.h-5,r.w-16,FG);}}
  if(s.radioDevicePage){
   c.card(LoRaName,s.radioDeviceFocus==0,5);auto ink=s.radioDeviceFocus==0?BG:FG;
   c.text(LoRaName.x+8,LoRaName.y+7,"Companion name",Font::ChatMeta,ink);
   c.text(LoRaName.x+8,LoRaName.y+28,s.radioName,Font::ChatBody,ink,LoRaName.w-16);
   c.text(LoRaInset,LoRaBytesY-23,"Path hash bytes",Font::ChatBody);
   for(int bytes=1;bytes<=3;bytes++){auto r=loraBytesRect(bytes);c.button(r,std::to_string(bytes),s.radioPathBytes==bytes);if(s.radioDeviceFocus==bytes)g.drawRect(r.x-2,r.y-2,r.w+4,r.h+4,FG);}
   c.text(LoRaInset,LoRaBytesY+47,"Bytes per repeater / outgoing flood",Font::ChatMeta,FG,LoRaContentW);
   c.text(LoRaInset,LoRaBytesY+63,"Saved direct paths stay unchanged",Font::ChatMeta,FG,LoRaContentW);
   c.button(LoRaDeviceApply,s.radioDraftChanged?"APPLY *":"APPLY",s.radioDeviceFocus==4);
   c.text(LoRaInset,Top+238,s.notice.empty()?"W S: select / Enter: edit":s.notice,Font::ChatMeta,FG,LoRaContentW);
  }else{
  c.text(LoRaInset,Top+2,s.radioReady?(s.radioTX?"TX active":s.radioQueued?"TX queued / RX":"RX ready"):("Radio error "+std::to_string(s.radioError)),Font::ChatMeta,FG,110);
  c.text(W-LoRaInset-100,Top+2,std::string(s.radioInternal?"INT":"EXT")+" / paths "+std::to_string(s.radioPathBytes)+"B",Font::ChatMeta,FG,100);
  c.button(LoRaBand868,"868 MHz",s.radioBand==868);c.button(LoRaBand900,"900 MHz",s.radioBand==900);
  c.card(LoRaPreset,false,5);if(s.radioFocus==0)g.drawRoundRect(LoRaPreset.x+2,LoRaPreset.y+2,LoRaPreset.w-4,LoRaPreset.h-4,4,FG);
  c.iconButton({LoRaPresetPrev.x,LoRaPreset.y+10,30,30},Icon::Back);c.iconButton({LoRaPresetNext.x,LoRaPreset.y+10,30,30},Icon::ChevronRight);
  std::string name=s.radioPreset>=0&&s.radioPreset<matter::RADIO_PRESET_COUNT?matter::RADIO_PRESETS[s.radioPreset].name:"Custom";
  auto names=c.lines(name,LoRaPreset.w-64,Font::ChatMeta);if(names.size()>2){names.resize(2);names.back()=c.fit(names.back()+"..",LoRaPreset.w-64,Font::ChatMeta);}
  int nameY=LoRaPreset.y+(LoRaPreset.h-int(names.size())*c.lineHeight(Font::ChatMeta))/2;
  for(int i=0;i<int(names.size());i++)c.centered(LoRaPreset.x+32,nameY+i*c.lineHeight(Font::ChatMeta),LoRaPreset.w-64,names[i],Font::ChatMeta);
  char frequency[24],bandwidth[24];snprintf(frequency,sizeof(frequency),"%.3f MHz",s.radioFrequency);snprintf(bandwidth,sizeof(bandwidth),"%.2f kHz",s.radioBW);
  const std::string values[]={frequency,bandwidth,"SF"+std::to_string(s.radioSF),"4/"+std::to_string(s.radioCR),std::to_string(s.radioPower)+" dBm"};
  for(int i=0;i<matter::RadioFieldCount;i++){auto row=loraFieldRect(i);bool focus=s.radioFocus==i+1;c.card(row,focus);auto ink=focus?BG:FG;c.text(row.x+6,row.y+4,labels[i],Font::ChatMeta,ink,100);c.text(row.x+112,row.y+3,values[i],Font::ChatBody,ink,row.w-118);}
  c.button(LoRaApply,s.radioDraftChanged?"APPLY *":"APPLY",s.radioFocus==6);c.button(LoRaAdvert,"ADVERTISE",s.radioFocus==7);
  c.text(LoRaInset,Bottom-17,s.notice.empty()?"B: band / A D: preset":s.notice,Font::ChatMeta,FG,LoRaContentW);
  }
  if(s.radioPowerEditing){
   g.fillRect(0,Top,W,H-Top,BG);
   Rect panel={Margin,Top+4,W-2*Margin,Bottom-Top-8};g.fillRect(panel.x,panel.y,panel.w,panel.h,BG);c.card(panel);
   int field=std::max(0,std::min(int(matter::RadioFieldCount),s.radioEditField));
   c.text(Margin+8,Top+15,labels[field],Font::ChatTitle);
   if(field==matter::RadioFieldCount){c.text(Margin+8,Top+46,"1 to 31 bytes / name only",Font::ChatMeta);c.text(Margin+8,Top+62,"Public key stays unchanged",Font::ChatMeta);}
   else if(field==matter::Frequency){c.text(Margin+8,Top+46,"863-870 / 902-928 MHz",Font::ChatMeta);c.text(Margin+8,Top+62,"Enter frequency in MHz",Font::ChatMeta);}
   else if(field==matter::Bandwidth){c.text(Margin+8,Top+43,"7.8 / 10.4 / 15.6 / 20.8",Font::ChatMeta);c.text(Margin+8,Top+57,"31.25 / 41.7 / 62.5",Font::ChatMeta);c.text(Margin+8,Top+71,"125 / 250 / 500 kHz",Font::ChatMeta);}
   else c.text(Margin+8,Top+46,field==matter::SpreadFactor?"SF 5 to 12":field==matter::CodingRate?"5 to 8 (4/5 to 4/8)":"-9 to 22 dBm / both antennas",Font::ChatMeta,FG,W-2*Margin-16);
   c.card(RadioPowerInput);auto tail=c.inputTail(s.radioPowerInput,RadioPowerInput.w-20,Font::ChatBody);c.text(RadioPowerInput.x+8,RadioPowerInput.y+10,s.radioPowerInput.empty()?"Type value...":tail,Font::ChatBody,FG,RadioPowerInput.w-16);g.drawFastVLine(RadioPowerInput.x+9+c.width(tail,Font::ChatBody),RadioPowerInput.y+8,16,FG);
   c.button(RadioPowerSave,"SAVE EDIT");c.button(RadioPowerCancel,"CANCEL");
   c.text(Margin+8,Top+185,s.notice.empty()?"Then APPLY to update LoRa":s.notice,Font::ChatMeta,FG,W-2*Margin-16);
  }
  g.drawRect(LoRaPanel.x,LoRaPanel.y,LoRaPanel.w,LoRaPanel.h,FG);
  auto activeTab=s.radioDevicePage?LoRaDevice:LoRaRadio;g.drawFastHLine(activeTab.x+1,LoRaPanel.y,activeTab.w-2,BG);
 }else if(s.page==Page::Antenna){
  if(s.antennaConfirm){c.text(Margin,Top+16,"External antenna",Font::ChatTitle,FG,W-2*Margin);c.text(Margin,Top+52,"Have you connected an",Font::ChatBody,FG,W-2*Margin);c.text(Margin,Top+72,"external LoRa antenna?",Font::ChatBody,FG,W-2*Margin);c.text(Margin,Top+108,"YES enables it. NO keeps",Font::ChatMeta,FG,W-2*Margin);c.text(Margin,Top+123,"the current antenna.",Font::ChatMeta,FG,W-2*Margin);c.button(ConfirmYes,"YES",true);c.button(ConfirmNo,"NO");}
  else{c.sectionLabel(Top+6,"LORA ANTENNA");
   auto option=[&](Rect r,const char *label,const char *detail,bool selected){c.card(r,selected);auto ink=selected?BG:FG;c.icon(Icon::Signal,r.x+6,r.y+(r.h-24)/2,24,ink);c.text(r.x+36,r.y+7,label,Font::ChatBody,ink,r.w-70);c.text(r.x+36,r.y+25,detail,Font::ChatMeta,ink,r.w-70);
    int cx=r.x+r.w-18,cy=r.y+r.h/2;g.drawCircle(cx,cy,7,ink);if(selected)g.fillCircle(cx,cy,4,ink);};
   option(AntennaInternal,"Internal antenna","Built in, always available",s.radioInternal);option(AntennaExternal,"External antenna","IPEX connector, asks first",!s.radioInternal);
   c.text(Margin+2,AntennaExternal.y+AntennaExternal.h+12,"Only one antenna is active at a time.",Font::ChatMeta,FG,W-2*Margin);}
 }else if(s.page==Page::Storage){
  c.sectionLabel(Top+6,"SD CARD");valueCard({Margin,Top+22,W-2*Margin,40},"STATUS",s.sd?"Mounted":"Unavailable");
  if(s.sd&&s.storageTotal){c.sectionLabel(Top+72,"SPACE",gb(s.storageTotal)+" total");Rect r={Margin,Top+88,W-2*Margin,52};c.card(r);
   c.text(r.x+7,r.y+5,gb(s.storageUsed)+" used",Font::Bold);auto free=gb(s.storageTotal-s.storageUsed)+" free";c.text(r.x+r.w-7-c.width(free,Font::ChatMeta),r.y+6,free,Font::ChatMeta);
   Rect bar={r.x+7,r.y+30,r.w-14,12};g.drawRoundRect(bar.x,bar.y,bar.w,bar.h,4,FG);int fill=std::max(4,int((bar.w-4)*s.storageUsed/s.storageTotal));g.fillRoundRect(bar.x+2,bar.y+2,fill,bar.h-4,2,FG);}
 }else if(s.page==Page::Maps){
  mapView(s);
  // Floating controls: white discs with a black ring (black when following the GPS).
  auto disc=[&](Rect r,bool on){int cx=r.x+r.w/2,cy=r.y+r.h/2,rad=(r.w-1)/2;g.fillCircle(cx,cy,rad,on?FG:BG);g.drawCircle(cx,cy,rad,FG);g.drawCircle(cx,cy,rad-1,FG);};
  disc(MapBackFloat,false);c.icon(Icon::Back,MapBackFloat.x+7,MapBackFloat.y+7,16);
  for(int i=0;i<3;i++){Rect r=mapButtonRect(i);bool on=i==1&&s.mapFollow;disc(r,on);uint16_t ink=on?BG:FG;int cx=r.x+r.w/2,cy=r.y+r.h/2;
   if(i==1)c.icon(Icon::GPS,cx-8,cy-8,16,ink);else{g.fillRect(cx-7,cy-1,14,3,ink);if(i==2)g.fillRect(cx-1,cy-7,3,14,ink);}}
 }else if(s.page==Page::GPS){
  c.toggleRow(GpsToggle,Icon::GPS,"GPS receiver",!s.gpsAvailable?"Unavailable":s.gpsTemporary?"On for time sync":s.gpsEnabled?"Enabled by user":"Off, saved position kept",s.gpsEnabled&&!s.gpsTemporary,s.gpsTemporary?"KEEP ON":"");
  c.sectionLabel(SettingsSectionY,"STATUS");
  valueCard(settingsHalf(0,SettingsBodyY),"FIX",!s.gpsAvailable?"Unavailable":!s.gpsEnabled?"Off":s.gpsFix?"Position fix":"Searching...");
  valueCard(settingsHalf(1,SettingsBodyY),"SATELLITES",s.gpsEnabled?std::to_string(s.gpsUsed)+" used / "+std::to_string(s.gpsVisible):"--");
  c.sectionLabel(Top+118,s.gpsFix?"POSITION":"SAVED POSITION");
  char position[40]="-- / --";if(s.locationValid)snprintf(position,sizeof(position),"%.5f / %.5f",s.latitude,s.longitude);valueCard({Margin,Top+134,W-2*Margin,40},"LATITUDE / LONGITUDE",position);
  valueCard(settingsHalf(0,Top+180),"ALTITUDE",s.gpsFix?std::to_string(int(s.altitude))+" m":"--");
  valueCard(settingsHalf(1,Top+180),"RECEIVER",!s.gpsEnabled?"Off":s.gpsNmeaFresh?"NMEA "+std::to_string(s.gpsBaud):s.gpsData?"No fresh NMEA":"No data");
  if(s.gpsTimeValid)c.text(Margin+2,Top+228,"GPS date / time available",Font::ChatMeta,FG,W-2*Margin);
 }else if(s.page==Page::Weather){
  bool data=s.weatherValid,offline=s.weatherPhase==WeatherPhaseView::WifiOff||s.weatherPhase==WeatherPhaseView::NoNetwork;
  auto number=[](float v){return std::to_string(int(std::lround(v)));};
  // Number with a drawn degree ring (the fonts have no degree glyph), centered in w when w>0.
  auto withDegree=[&](int x,int y,int w,const std::string &value,Font font,int ring){int tw=c.width(value,font),total=tw+ring*2+3;if(w>0)x+=(w-total)/2;c.text(x,y,value,font);g.drawCircle(x+tw+ring+2,y+ring+1,ring,FG);};
  std::string place=s.weatherPlace.empty()?(data?"Your area":"Weather"):s.weatherPlace,range=data?"H "+number(s.high)+"  L "+number(s.low):"";int rangeW=range.empty()?0:c.width(range,Font::ChatBody)+8;
  c.text(Margin,WeatherPlaceY,place,Font::ChatTitle,FG,W-2*Margin-rangeW);if(rangeW)c.text(W-Margin-rangeW+8,WeatherPlaceY+3,range,Font::ChatBody);
  c.weatherIcon(data?s.weatherCode:-1,WeatherIconX,WeatherHeroY,64);
  std::string temperature=data?number(s.temperature):"--";Font big=c.width(temperature,Font::LockClock)<=96?Font::LockClock:Font::Clock;
  int tx=WeatherTempX+8;c.text(tx,WeatherHeroY-2,temperature,big);int tw=c.width(temperature,big);if(data){g.drawCircle(tx+tw+6,WeatherHeroY+4,3,FG);g.drawCircle(tx+tw+6,WeatherHeroY+4,2,FG);c.text(tx+tw+12,WeatherHeroY,"C",Font::ChatBody);}
  c.text(WeatherTempX+8,WeatherHeroY+50,data?s.weatherText:"No weather data",Font::ChatBody,FG,W-Margin-WeatherTempX-8);
  if(offline){
   bool wifiOff=s.weatherPhase==WeatherPhaseView::WifiOff;c.card(WeatherPrompt);c.icon(Icon::Wifi,W/2-12,WeatherPrompt.y+8,24);
   c.centered(Margin,WeatherPrompt.y+36,W-2*Margin,wifiOff?"Wi-Fi is off":"No saved Wi-Fi",Font::ChatTitle);
   c.centered(Margin,WeatherPrompt.y+56,W-2*Margin,wifiOff?"Turn it on to update the weather":"Connect a network in Wi-Fi",Font::ChatMeta);
   c.button(WeatherWifiButton,wifiOff?"TURN ON WI-FI":"WI-FI SETTINGS",true);
  }else{
   Rect hours={Margin,WeatherHoursY,W-2*Margin,WeatherHoursH};c.card(hours);
   if(data&&s.weatherHours>0){int column=hours.w/WeatherHoursColumns;for(int i=0;i<s.weatherHours&&i<WeatherHoursColumns;i++){int x=hours.x+i*column;if(i)g.drawFastVLine(x,hours.y+6,hours.h-12,FG);c.centered(x,hours.y+4,column,s.weatherHourTime[i],Font::ChatMeta);c.weatherIcon(s.weatherHourCode[i],x+column/2-13,hours.y+16,26);withDegree(x,hours.y+43,column,number(s.weatherHourTemp[i]),Font::ChatBody,2);}}
   else c.centered(hours.x,hours.y+21,hours.w,"Next hours after update",Font::ChatMeta);
   const char *labels[]={"Feels like","Humidity","Wind","Pressure"};
   for(int i=0;i<4;i++){Rect cell=weatherCell(i);c.card(cell);c.text(cell.x+6,cell.y+9,labels[i],Font::ChatMeta);std::string value=!data?"--":i==1?number(s.humidity)+"%":i==2?number(s.wind)+" km/h":i==3?number(s.pressure):"";
    if(data&&i==0){int vw=c.width(number(s.apparent),Font::ChatBody)+7;withDegree(cell.x+cell.w-6-vw,cell.y+7,0,number(s.apparent),Font::ChatBody,2);}else c.text(cell.x+cell.w-6-c.width(value,Font::ChatBody),cell.y+7,value,Font::ChatBody);}
  }
  std::string updated=s.weatherUpdated;if(updated.size()>10&&updated[10]=='T')updated=updated.substr(11,5);
  std::string status=!s.notice.empty()?s.notice:s.weatherPhase==WeatherPhaseView::Ready?"Updated "+updated+(s.weatherSource.empty()?"":" / "+s.weatherSource):s.weatherPhase==WeatherPhaseView::Error&&data?s.weatherMessage+" / cached":s.weatherMessage.empty()&&data?"Cached "+updated:s.weatherMessage;
  c.text(Margin,WeatherStatusY,status,Font::ChatMeta,FG,W-2*Margin-70);c.text(W-Margin-c.width("Open-Meteo",Font::ChatMeta),WeatherStatusY,"Open-Meteo",Font::ChatMeta);
  if(!offline){bool busy=s.weatherBusy||s.weatherPhase==WeatherPhaseView::Locating;c.button(WeatherRefresh,busy?"UPDATING...":"REFRESH",busy);}
 }else if(s.page==Page::Wifi){
  std::string joined=s.wifiSSID+(s.wifiIP.empty()?"":" / "+s.wifiIP);// network name first, IP only when both fit
  c.toggleRow(WifiToggle,Icon::Wifi,"Wi-Fi",s.wifiConnected?(c.width(joined,Font::ChatMeta)<=120?joined:s.wifiSSID):s.wifiConnecting?"Connecting...":!s.wifiEnabled?"Off":s.wifiMessage.empty()?"Not connected":s.wifiMessage,s.wifiEnabled);
  c.button(WifiScan,s.wifiScanning?"SCAN...":"SCAN",s.wifiScanning);c.button(WifiManual,"HIDDEN");c.button(WifiDisconnect,s.wifiConnected||s.wifiConnecting?"DISCONNECT":"RECONNECT");
  int count=s.networks.size();c.sectionLabel(WifiListY-16,"NETWORKS",count?std::to_string(count)+" FOUND":"");
  for(int row=0;row<WifiRows&&s.wifiOffset+row<count;row++){auto &n=s.networks[s.wifiOffset+row];c.menuRow(wifiRowRect(row),n.secured?Icon::Lock:Icon::Wifi,n.ssid,std::to_string(n.rssi)+" dBm / "+(n.saved?"Saved":n.secured?"Protected":"Open"),s.wifiFocus==s.wifiOffset+row);}
  if(!count)c.centered(Margin,WifiListY+30,W-2*Margin,s.wifiEnabled?"Tap SCAN to search":"Turn Wi-Fi on to search",Font::ChatMeta);
  if(count>WifiRows){int track=WifiRows*WifiRowH-2,thumb=std::max(12,track*WifiRows/count),max=count-WifiRows;g.drawFastVLine(W-3,WifiListY,track,FG);g.fillRect(W-4,WifiListY+std::min(s.wifiOffset,max)*(track-thumb)/max,3,thumb,FG);}
  if(s.wifiEditing){
   g.fillRect(0,Top,W,H-Top,BG);c.sectionLabel(Top+8,s.wifiHidden?"NETWORK NAME (SSID)":"SELECTED NETWORK");c.card(WifiSSIDInput);
   auto name=c.inputTail(s.wifiSSID,WifiSSIDInput.w-16,Font::ChatBody);c.text(WifiSSIDInput.x+7,WifiSSIDInput.y+9,name.empty()?"Type network name":name,Font::ChatBody,FG,WifiSSIDInput.w-14);
   if(s.wifiEditField==0)g.drawFastVLine(WifiSSIDInput.x+8+c.width(name,Font::ChatBody),WifiSSIDInput.y+7,16,FG);
   c.sectionLabel(WifiPassword.y-16,"PASSWORD / EMPTY IF OPEN");c.card(WifiPassword);
   auto tail=c.inputTail(s.wifiPasswordMask,WifiPassword.w-WifiEye.w-17,Font::ChatBody);c.text(WifiPassword.x+7,WifiPassword.y+9,tail,Font::ChatBody,FG,WifiPassword.w-WifiEye.w-14);
   if(s.wifiEditField==1)g.drawFastVLine(WifiPassword.x+8+c.width(tail,Font::ChatBody),WifiPassword.y+7,16,FG);
   g.drawFastVLine(WifiEye.x,WifiEye.y+4,WifiEye.h-8,FG);g.drawRoundRect(WifiEye.x+7,WifiEye.y+10,18,11,5,FG);g.drawCircle(WifiEye.x+16,WifiEye.y+15,2,FG);if(!s.wifiShowPassword)g.drawLine(WifiEye.x+8,WifiEye.y+24,WifiEye.x+24,WifiEye.y+7,FG);
   c.button(WifiConnect,"CONNECT");c.button(WifiCancel,"CANCEL");c.text(Margin+2,WifiConnect.y+44,"Alt+V: show / hide password",Font::ChatMeta,FG,W-2*Margin);c.text(Margin+2,WifiConnect.y+62,"Enter: next field / connect",Font::ChatMeta,FG,W-2*Margin);
   c.text(Margin+2,WifiConnect.y+86,s.notice.empty()?"Review before connecting":s.notice,Font::ChatMeta,FG,W-2*Margin);
  }
 }else if(s.page==Page::Bluetooth){
  // MeshCore app link: Bluetooth (passkey below), Wi-Fi (TCP 5000, LAN only) and the USB cable all reach the same companion.
  c.toggleRow(BleToggle,Icon::Bluetooth,"Bluetooth",!s.bleEnabled?"Off":!s.bleReady?"Starting...":s.appBleLinked?"MeshCore app connected":"Visible to the MeshCore app",s.bleEnabled);
  c.sectionLabel(SettingsSectionY,"MESHCORE APP");
  // Bluetooth name: tap the card (or N) to edit the part after "MeshCore-"; ENTER saves, empty = the node name.
  if(s.appBleEditing){Rect r=AppNameCard;g.drawRoundRect(r.x,r.y,r.w,r.h,Radius,FG);g.drawRoundRect(r.x+1,r.y+1,r.w-2,r.h-2,Radius,FG);
   c.text(r.x+7,r.y+4,"ENTER saves",Font::ChatMeta,FG,r.w-14);int px=r.x+7;c.text(px,r.y+20,"MeshCore-",Font::ChatMeta);px+=c.width("MeshCore-",Font::ChatMeta);
   auto tail=c.inputTail(s.appBleInput,r.x+r.w-8-px,Font::Bold);c.text(px,r.y+20,tail,Font::Bold);g.fillRect(px+c.width(tail,Font::Bold)+1,r.y+19,2,15,FG);}
  else valueCard(AppNameCard,"BLE NAME / EDIT",s.appBleName);
  valueCard(AppPinCard,"PIN",s.bleEnabled?s.appPin:"-");
  c.toggleRow(AppWifiToggle,Icon::Wifi,"App over Wi-Fi",!s.appWifi?"Off":s.appWifiLinked?"MeshCore app connected":s.appWifiDetail,s.appWifi);
  c.text(Margin+2,AppWifiToggle.y+AppWifiToggle.h+8,s.appUsbLinked?"USB: MeshCore app connected":"USB cable: always available",Font::ChatMeta,FG,W-2*Margin);
 }else if(s.page==Page::Audio){
  c.toggleRow(AudioNotify,Icon::Messages,"Notification sound",s.audioReady?"Speaker ready":s.audioMessage,s.audioNotifications);
  c.sectionLabel(SettingsSectionY,"VOLUME",std::to_string(s.audioVolume)+"%");for(auto r:{AudioMinus,AudioPlus}){c.card(r);int cx=r.x+r.w/2,cy=r.y+r.h/2;g.fillRect(cx-6,cy-1,12,2,FG);if(r.x==AudioPlus.x)g.fillRect(cx-1,cy-6,2,12,FG);}c.slider(AudioSlider,s.audioVolume);
  c.sectionLabel(AudioToneMessage.y-16,"RINGTONES","TAP TO PLAY");
  auto tone=[&](Rect r,Icon id,const char *label,const std::string &name){c.card(r);c.icon(id,r.x+6,r.y+(r.h-24)/2,24);c.text(r.x+36,r.y+3,label,Font::ChatMeta,FG,r.w-40);
   c.text(r.x+36,r.y+17,name,Font::Bold,FG,r.w-80);c.icon(Icon::Back,r.x+r.w-44,r.y+(r.h-12)/2,12);c.icon(Icon::ChevronRight,r.x+r.w-20,r.y+(r.h-12)/2,12);};
  tone(AudioToneMessage,Icon::Messages,"New message",s.messageTone);tone(AudioToneMention,Icon::Contacts,"Mention @you",s.mentionTone);
  c.sectionLabel(AudioBackend.y-16,"DICTATION");c.card(AudioBackend);c.text(AudioBackend.x+7,AudioBackend.y+3,"Offline, Italian",Font::ChatBody,FG,AudioBackend.w-14);
  c.text(AudioBackend.x+7,AudioBackend.y+19,s.offlineEngineCompiled?"Hold MIC, release to transcribe":"Model support not compiled",Font::ChatMeta,FG,AudioBackend.w-14);
 }else if(s.page==Page::Power){
  c.sectionLabel(Top+6,"LIGHT TIMERS");
  const char *names[]={"Display / front-light","Keyboard backlight"};for(int i=0;i<2;i++){Rect r=powerRowRect(i);bool focus=s.powerField==i;c.card(r,focus);auto ink=focus?BG:FG;if(i)c.icon(Icon::Keyboard,r.x+6,r.y+(r.h-16)/2,16,ink);else{g.drawRect(r.x+7,r.y+14,16,10,ink);g.drawFastHLine(r.x+12,r.y+27,6,ink);}c.text(r.x+30,r.y+5,names[i],Font::ChatBody,ink,r.w-50);uint32_t seconds=i?s.keyboardTimeout:s.displayTimeout;std::string value=seconds==0?"Never":seconds>=60?std::to_string(seconds/60)+" minute"+(seconds>=120?"s":""):std::to_string(seconds)+" seconds";c.text(r.x+30,r.y+23,value+(seconds==30?" / default":""),Font::ChatMeta,ink,r.w-50);c.icon(Icon::ChevronRight,r.x+r.w-16,r.y+(r.h-12)/2,12,ink);}
  c.sectionLabel(timeoutOptionRect(0).y-16,s.powerField?"KEYBOARD TIMEOUT":"DISPLAY TIMEOUT");const char *labels[]={"15s","30s","1m","2m","5m","Never"};const uint32_t values[]={15,30,60,120,300,0};uint32_t selected=s.powerField?s.keyboardTimeout:s.displayTimeout;
  for(int i=0;i<6;i++){Rect r=timeoutOptionRect(i);c.card(r,selected==values[i]);c.centered(r.x,r.y+(r.h-c.lineHeight(Font::Small))/2,r.w,labels[i],Font::Small,selected==values[i]?BG:FG);}
  int y=timeoutOptionRect(0).y+44;c.sectionLabel(y,"AUTO LOCK");c.text(Margin+2,y+18,"Lights wake on key or touch.",Font::ChatMeta,FG,W-2*Margin);c.text(Margin+2,y+34,"Locks after 60 s or double SPACE.",Font::ChatMeta,FG,W-2*Margin);
 }else if(s.page==Page::Keyboard){
  // Search on top, one switch per language, and how to switch / reach the extra letters below.
  c.card(KeyboardSearch);auto tail=c.inputTail(s.kbdQuery,KeyboardSearch.w-40,Font::ChatBody);
  c.text(KeyboardSearch.x+7,KeyboardSearch.y+5,s.kbdQuery.empty()?(s.kbdSearching?"Type a language...":"Search languages"):tail,Font::ChatBody,FG,KeyboardSearch.w-36);
  if(s.kbdSearching)g.drawFastVLine(KeyboardSearch.x+8+c.width(tail,Font::ChatBody),KeyboardSearch.y+5,16,FG);c.icon(Icon::Search,KeyboardSearch.x+KeyboardSearch.w-22,KeyboardSearch.y+5,16);
  std::vector<int> rows;for(auto l:mm_kbd::Order)if(mm_kbd::matches(l,s.kbdQuery))rows.push_back(int(l));
  int focus=rows.empty()?0:std::min(std::max(s.kbdFocus,0),int(rows.size())-1),first=std::max(0,focus-KeyboardRows+1);
  for(int i=0;i<KeyboardRows&&first+i<int(rows.size());i++){int id=rows[first+i];auto &info=mm_kbd::info(mm_kbd::Lang(id));Rect r=keyboardRowRect(i);bool on=s.kbdMask&(1u<<id);
   std::string detail=on&&id==s.kbdLang&&mm_kbd::enabledCount(s.kbdMask)>1?std::string("In use (")+info.code+")":std::string(info.english)+"  "+info.code;
   c.toggleRow(r,Icon::Keyboard,info.name,detail,on);if(first+i==focus&&!s.kbdSearching)g.drawRoundRect(r.x+2,r.y+2,r.w-4,r.h-4,Radius-1,FG);}
  if(rows.empty())c.centered(Margin,Top+80,W-2*Margin,"No language matches",Font::ChatBody);
  int y=KeyboardHelpY;
  if(!s.notice.empty()){c.text(Margin+2,y,s.notice,Font::ChatMeta,FG,W-2*Margin-4);y+=13;}
  std::string howto=mm_kbd::enabledCount(s.kbdMask)>1?"Alt + Space: next language (now "+std::string(mm_kbd::info(mm_kbd::Lang(s.kbdLang)).code)+")":"Turn on two languages, then Alt + Space switches between them";
  for(auto &line:c.lines(howto,W-2*Margin-4,Font::ChatMeta)){c.text(Margin+2,y,line,Font::ChatMeta);y+=13;}std::string help=rows.empty()?"":mm_kbd::accentHelp(mm_kbd::Lang(rows[focus]));
  if(!help.empty())help=s.accentMode==0?"The accent key is off (Settings > Accent key).":std::string(s.accentMode==1?"Right":"Left")+" Shift is the accent key. "+help;
  if(!help.empty())for(auto &line:c.lines(help,W-2*Margin-4,Font::ChatMeta)){if(y>Bottom-12)break;c.text(Margin+2,y,line,Font::ChatMeta);y+=13;}
 }else if(s.page==Page::AccentKey){
  // One row per choice: the saved one has a filled dot, the focused one a double outline.
  const char *names[]={"Off","Right Shift","Left Shift"};
  const char *details[]={"No accents","Accents in chats","Accents in chats"};
  for(int i=0;i<3;i++){Rect r=accentRowRect(i);c.card(r);c.text(r.x+10,r.y+5,names[i],Font::ChatBody,FG,r.w-50);c.text(r.x+10,r.y+22,details[i],Font::ChatMeta,FG,r.w-50);
   int cx=r.x+r.w-20,cy=r.y+r.h/2;g.drawCircle(cx,cy,8,FG);if(s.accentMode==i)g.fillCircle(cx,cy,5,FG);
   if(s.accentFocus==i)g.drawRoundRect(r.x+2,r.y+2,r.w-4,r.h-4,Radius-1,FG);}
  // How it works (same text as the Plus): tap before a letter, then the letters of the active keyboard language.
  int y=AccentInfoY,w=W-2*Margin-4;if(!s.notice.empty()){c.text(Margin+2,y,s.notice,Font::ChatMeta,FG,w);y+=13;}
  for(auto &line:c.lines("In a chat, tap it before a letter: once for the first accent, twice for the second.",w,Font::ChatMeta)){c.text(Margin+2,y,line,Font::ChatMeta);y+=13;}
  auto lang=mm_kbd::Lang(s.kbdLang>=0&&s.kbdLang<mm_kbd::LangCount?s.kbdLang:0);
  for(auto &line:c.lines(std::string(mm_kbd::info(lang).name)+" (Settings > Keyboard languages):",w,Font::ChatMeta)){c.text(Margin+2,y,line,Font::ChatMeta);y+=13;}
  std::string help=mm_kbd::accentHelp(lang);if(help.empty())help="No accented letters in this language";
  for(auto &line:c.lines(help,w,Font::ChatMeta)){if(y>Bottom-12)break;c.text(Margin+2,y,line,Font::ChatMeta);y+=13;}
 }else if(s.page==Page::Wallpaper){
  c.sectionLabel(Top+6,"LOCK SCREEN");
  for(int i=0;i<WallpaperCardCount&&i<mm_wallpaper::count;i++){Rect r=wallpaperCardRect(i);bool focus=s.wallpaperFocus==i,active=s.wallpaper==i;c.card(r,focus);
   int tx=r.x+(r.w-WallpaperThumbW)/2,ty=r.y+(r.h-WallpaperThumbH)/2;g.fillRect(tx-1,ty-1,WallpaperThumbW+2,WallpaperThumbH+2,BG);wallpaperThumb(i,tx,ty);
   if(active){g.drawRect(tx-2,ty-2,WallpaperThumbW+4,WallpaperThumbH+4,focus?BG:FG);g.fillCircle(r.x+r.w-6,r.y+6,3,focus?BG:FG);}}
  int sel=s.wallpaperFocus>=0&&s.wallpaperFocus<mm_wallpaper::count?s.wallpaperFocus:0;
  c.sectionLabel(WallpaperInfoY,"SELECTED",s.wallpaper==sel?"ACTIVE":"TAP OR ENTER TO APPLY");
  c.text(Margin+2,WallpaperInfoY+18,mm_wallpaper::list[sel].name,Font::ChatBody,FG,W-2*Margin-4);c.text(Margin+2,WallpaperInfoY+36,mm_wallpaper::list[sel].detail,Font::ChatMeta,FG,W-2*Margin-4);
  c.text(Margin+2,WallpaperInfoY+58,"Shown behind the lock clock, summit",Font::ChatMeta,FG,W-2*Margin-4);c.text(Margin+2,WallpaperInfoY+72,"in front of the digits. Sky is clear.",Font::ChatMeta,FG,W-2*Margin-4);
 }else if(s.page==Page::Info){
  g.drawBitmap((W-mm_logo::size56)/2,Top+6,mm_logo::logo56,mm_logo::size56,mm_logo::size56,FG);// OS logo: Matterhorn + mesh route
  c.centered(Margin,Top+68,W-2*Margin,"MATTERMESH OS",Font::Brand);c.centered(Margin,Top+94,W-2*Margin,"Version "+s.version,Font::ChatBody);
  c.centered(Margin,Top+111,W-2*Margin,"Offline capable. Built for the field.",Font::ChatMeta);
  c.sectionLabel(Top+132,"SYSTEM");Rect r={Margin,Top+148,W-2*Margin,Bottom-Top-148};c.card(r);
  const char *labels[]={"MeshCore","Device","Radio","Build","Commit","Memory","Hardware"};const std::string values[]={s.meshCoreVersion,"T-Deck Max 240x320",s.radioReady?"SX1262 / MeshCore":"SX1262 / offline",s.build,s.commit,std::to_string(s.flashMB)+" MB / "+std::to_string(s.heapKB)+" KB free",s.touch&&s.keyboard&&s.hapticReady?"Touch, keys, haptic OK":std::string(s.touch?"":"touch ")+(s.keyboard?"":"keys ")+(s.hapticReady?"":"haptic ")+"missing"};
  for(int i=0;i<7;i++){int y=r.y+4+i*15;c.text(r.x+7,y,labels[i],Font::ChatMeta);c.text(r.x+72,y,values[i],Font::ChatMeta,FG,r.w-79);}
 }else if(s.page==Page::Support){
  // Donate / Extra apps tabs, then the QR of the selected one on a quiet zone of 2 modules and its text below. The
  // URLs are constants: each QR is encoded once. With the Extra apps unlocked (the usual Full image) there is nothing to buy: no tabs, Donate only.
  static std::vector<uint8_t> donateQr,unlockQr;static const int donateN=encodeQr(DonateUrl,donateQr),unlockN=encodeQr(UnlockUrl,unlockQr);
  const bool tabs=!s.extraAppsUnlocked;const int qrY=tabs?SupportQrY:Top+8;
  if(tabs){const char *names[]={"DONATE","EXTRA APPS"};for(int i=0;i<2;i++)c.button(supportTabRect(i),names[i],s.supportTab==i);}
  const bool unlock=tabs&&s.supportTab==1;const std::vector<uint8_t> &modules=unlock?unlockQr:donateQr;
  const int n=unlock?unlockN:donateN,scale=n?std::min(3,(W-2*Margin)/(n+4)):0,side=(n+4)*scale,x0=(W-side)/2,y0=qrY;
  if(n){g.fillRect(x0,y0,side,side,BG);for(int y=0;y<n;y++)for(int x=0;x<n;x++)if(modules[y*n+x])g.fillRect(x0+(x+2)*scale,y0+(y+2)*scale,scale,scale,FG);}
  const int w=W-2*Margin;int y=n?y0+side+8:qrY+8;
  auto para=[&](const std::string &text,Font f,int step){for(auto &line:c.lines(text,w,f)){c.centered(Margin,y,w,line,f);y+=step;}};
  if(unlock){para("RPT Scanner, RPT Diagnostic, RX Log, Noise Floor.",Font::ChatMeta,14);y+=4;c.centered(Margin,y,w,"Your unlock code",Font::ChatMeta);c.centered(Margin,y+15,w,s.unlockCode,Font::ChatTitle);y+=40;para("Buy on Ko-fi, then redeem at mattermeshos.com/unlock",Font::ChatMeta,14);}
  else{para("MatterMesh OS is free. A donation of any amount on Ko-fi helps hardware and field tests.",Font::ChatMeta,14);y+=4;para("ko-fi.com/mattermeshos",Font::ChatMeta,14);}
 }else if(s.page==Page::Battery){
  c.icon(Icon::Battery,(W-32)/2,Top+10,32);c.centered(Margin,Top+50,W-2*Margin,s.status.battery<0?"--%":std::to_string(s.status.battery)+"%",Font::Clock);
  c.sectionLabel(Top+108,"DETAILS");valueCard(settingsHalf(0,Top+124),"VOLTAGE",s.voltage<0?"--":std::to_string(s.voltage)+" mV");
  valueCard(settingsHalf(1,Top+124),"STATE",s.batteryFull?"Full":s.batteryCharging||s.status.charging?"Charging":"On battery");
  valueCard({Margin,Top+170,W-2*Margin,40},"SOURCE",s.batteryEstimated?"Voltage estimate":"BQ27220 fuel gauge");
 }else if(s.page==Page::Files){
  c.text(Margin,Top+4,s.sd?"SD CARD / ROOT":"NO SD CARD",Font::ChatMeta);
  if(s.files.empty())c.centered(Margin,Top+55,W-2*Margin,"No files available",Font::Body);
  int start=s.scroll;for(int row=0;row<FileRows&&start+row<int(s.files.size());row++){Rect r=fileRowRect(row);c.card(r);auto name=s.files[start+row];bool dir=name.rfind("[DIR] ",0)==0;if(dir)name=name.substr(6);c.icon(dir?Icon::Files:Icon::Notes,r.x+6,r.y+(r.h-24)/2,24);c.text(r.x+37,r.y+3,name,Font::ChatBody,FG,r.w-45);c.text(r.x+37,r.y+18,dir?"Folder / read-only":"File / read-only",Font::ChatMeta);}
 }else if(s.page==Page::Terminal){
  c.sectionLabel(Top+6,"SYSTEM");valueCard({Margin,Top+22,W-2*Margin,40},"FREE MEMORY / STACK",std::to_string(s.heapKB)+" KB / "+std::to_string(s.stackFree)+" B");
  valueCard(settingsHalf(0,Top+68),"CPU",std::to_string(s.cpuMHz)+" MHz");valueCard(settingsHalf(1,Top+68),"MESH RX / TX",std::to_string(s.radioPacketsRX)+" / "+std::to_string(s.radioPacketsTX));
  valueCard({Margin,Top+114,W-2*Margin,40},"UI / E-PAPER TIME",std::to_string(s.rasterMs)+" / "+std::to_string(s.paintMs)+" ms");
  c.sectionLabel(Top+164,"USB SERIAL");c.text(Margin+2,Top+182,"ui stats / radio status / status",Font::ChatMeta,FG,W-2*Margin);
 }else if(s.page==Page::DateTime&&s.dateEditing){
  auto two=[](int v){char t[8];snprintf(t,sizeof(t),"%02d",v);return std::string(t);};
  c.sectionLabel(Top+6,"DATE","DD / MM / YYYY");c.sectionLabel(Top+66,"TIME","HH : MM");
  for(int i=0;i<DateFieldCount;i++)c.button(dateFieldRect(i),i==2?std::to_string(s.dateFields[2]):two(s.dateFields[i]),s.dateFocus==i);
  c.button(DateMinus,"-");c.button(DatePlus,"+");
  c.text(Margin+2,Top+172,"h/l field, k/j +/-, type digits",Font::ChatMeta,FG,W-2*Margin);c.text(Margin+2,Top+188,"Enter saves, Back cancels",Font::ChatMeta,FG,W-2*Margin);
  c.button(DateSave,"SAVE",true);c.button(DateCancel,"CANCEL");
 }else if(s.page==Page::DateTime){
  c.sectionLabel(Top+6,"CLOCK");valueCard({Margin,Top+22,W-2*Margin,40},"TIME",s.status.clock);valueCard({Margin,Top+68,W-2*Margin,40},"DATE",s.date);
  c.sectionLabel(Top+118,"SOURCE");valueCard({Margin,Top+134,W-2*Margin,40},"SYNCHRONIZED FROM",s.timeSource);
  c.text(Margin+2,Top+184,"Set automatically from Wi-Fi or GPS.",Font::ChatMeta,FG,W-2*Margin);
  c.button(DateManual,"SET MANUALLY");
 }else if(s.page==Page::Updates){
  c.sectionLabel(Top+6,"INSTALLED");valueCard({Margin,Top+22,W-2*Margin,40},"VERSION",s.version+" / "+s.commit);
  c.sectionLabel(Top+72,"UPDATE SERVER");valueCard({Margin,Top+88,W-2*Margin,40},"STATUS",s.otaStatus);
  if(s.otaProgress>=0){Rect bar={Margin,Top+134,W-2*Margin,12};g.drawRoundRect(bar.x,bar.y,bar.w,bar.h,4,FG);int fill=std::max(4,(bar.w-4)*std::min(s.otaProgress,100)/100);g.fillRoundRect(bar.x+2,bar.y+2,fill,bar.h-4,2,FG);}
  c.text(Margin+2,Top+152,s.otaDetail,Font::ChatMeta,FG,W-2*Margin);
  if(!s.otaAction.empty())c.button(UpdatesAction,s.otaAction,s.otaConfirm);
 }else {
  // Every section has its own page above; anything else (a section number typed on the serial) is a blank card.
  c.icon(Icon::Tools,(W-32)/2,Top+24,32);c.centered(Margin,Top+80,W-2*Margin,"Not available",Font::Bold);
 }
 if(!s.notice.empty()&&s.page!=Page::Radio&&s.page!=Page::Weather&&s.page!=Page::Wifi&&s.page!=Page::Mesh&&s.page!=Page::Maps&&s.page!=Page::Keyboard&&s.page!=Page::AccentKey){c.footer(s.notice,"");}// Keyboard draws its notice above the help
}
#include "quick_settings_renderer.inc"
#include "cli_renderer.inc"
void Renderer::draw(const State &s){switch(s.page){
 case Page::Cli:cliApp(s);break;
 case Page::Home:home(s);break;case Page::Launcher:launcher(s);break;case Page::Messages:messages(s);break;case Page::Chat:chat(s);break;case Page::NewChat:form(s);break;case Page::ContactPicker:contactPicker(s);break;case Page::Settings:settings(s);break;case Page::QuickSettings:quickSettings(s);break;default:section(s);}if(s.channelOptions)channelOptions(s);}
}
