#include "components.h"
#include <climits>
#include "weather_icons.h"
#include "aa_icons.h"
#include "emoji_color.h"
#include "packed.h"
#include <cstring>
#include <esp_heap_caps.h>
#include "utf8.h"
#include <algorithm>
namespace mm {
// Themes: same components, different palette (illustrations follow the theme index).
// Themes: same components, different palette and lock illustration (renderer.cpp picks the picture by index).
static const Palette palettes[]={
 // Alpine Light: paper, night-blue ink, alpine green (contrast raised: darker ink, lines and surfaces)
 {"Alpine Light","Paper, alpine green",rgb(0xF4,0xF1,0xEA),rgb(0x10,0x1B,0x22),rgb(0x22,0x5E,0x4A),rgb(0xFF,0xFF,0xFF),rgb(0xDA,0xE2,0xD9),rgb(0xD0,0xDB,0xE5),rgb(0x45,0x54,0x5B),rgb(0xAE,0xB8,0xB2),rgb(0xBF,0xCC,0xD5),rgb(0xF4,0xF1,0xEA),rgb(0x10,0x1B,0x22),rgb(0x10,0x1B,0x22),LockLayout::RightColumn},
 // Night Ridge: deep navy, ice-blue ink, glacier cyan accent with dark text on it
 {"Night Ridge","Night, glacier cyan",rgb(0x0D,0x18,0x23),rgb(0xEA,0xF2,0xF7),rgb(0x3E,0xCD,0xE6),rgb(0x0A,0x15,0x1E),rgb(0x17,0x29,0x3B),rgb(0x1B,0x31,0x45),rgb(0x93,0xA8,0xB8),rgb(0x2E,0x46,0x5C),rgb(0x23,0x39,0x4E),rgb(0x08,0x11,0x19),rgb(0xEA,0xF2,0xF7),rgb(0xF2,0xF7,0xFA),LockLayout::BottomLeft},
 // Expedition: parchment, burnt orange, olive status bar
 {"Expedition","Parchment, burnt orange",rgb(0xEC,0xE4,0xD3),rgb(0x1E,0x1D,0x17),rgb(0xC0,0x50,0x28),rgb(0xFF,0xF6,0xE8),rgb(0xF7,0xF2,0xE6),rgb(0xE2,0xD8,0xC6),rgb(0x5A,0x56,0x48),rgb(0x8E,0x87,0x72),rgb(0xD5,0xC8,0xAE),rgb(0x48,0x51,0x33),rgb(0xF2,0xEB,0xDA),rgb(0xF6,0xEE,0xDC),LockLayout::TopLeft},
};
static_assert(sizeof(palettes)/sizeof(palettes[0])==ThemeCount,"theme count");
static int currentTheme=0;
uint16_t BG=palettes[0].bg,FG=palettes[0].fg,Accent=palettes[0].accent,OnAccent=palettes[0].onAccent,Surface=palettes[0].surface,SurfaceAlt=palettes[0].surfaceAlt,Muted=palettes[0].muted,Line=palettes[0].line,Mist=palettes[0].mist,StatusBg=palettes[0].statusBg,StatusFg=palettes[0].statusFg,LockInk=palettes[0].lockInk;

int themeIndex(){return currentTheme;}
const Palette &theme(int index){return palettes[index>=0&&index<ThemeCount?index:0];}
void setTheme(int index){auto &p=theme(index);currentTheme=index>=0&&index<ThemeCount?index:0;BG=p.bg;FG=p.fg;Accent=p.accent;OnAccent=p.onAccent;Surface=p.surface;SurfaceAlt=p.surfaceAlt;Muted=p.muted;Line=p.line;Mist=p.mist;StatusBg=p.statusBg;StatusFg=p.statusFg;LockInk=p.lockInk;}
// Preserve UTF-8 in storage/protocol; display Latin accents with the ASCII fonts.
// Text some senders encode twice (UTF-8 read as Latin-1 and encoded again): "Però" arrives as "PerÃ²", two glyphs the
// fonts lack. A Latin-1 lead (U+00C2..U+00F4) followed by exactly the continuations it needs (U+0080..U+00BF) is put
// back together; anything else stays as it is.
static std::string repairDoubleUtf8(const std::string &in){
 if(in.find('\xc3')==std::string::npos)return in;
 std::string out;out.reserve(in.size());
 for(size_t i=0;i<in.size();){
  auto c=(unsigned char)in[i];
  if(c==0xc3&&i+1<in.size()&&(unsigned char)in[i+1]>=0x82&&(unsigned char)in[i+1]<=0xb4){
   unsigned char lead=(unsigned char)in[i+1]+0x40;int need=lead<0xe0?1:lead<0xf0?2:3;size_t j=i+2;std::string seq(1,char(lead));
   while(int(seq.size())<=need&&j+1<in.size()&&(unsigned char)in[j]==0xc2&&(unsigned char)in[j+1]>=0x80&&(unsigned char)in[j+1]<=0xbf){seq+=in[j+1];j+=2;}
   if(int(seq.size())==need+1){out+=seq;i=j;continue;}}
  out+=in[i++];}
 return out;}
static std::string displayText(const std::string &input){
 // Every pattern below is non-ASCII (or a "@[" mention): plain ASCII text passes through untouched.
 if(input.find("@[")==std::string::npos&&std::all_of(input.begin(),input.end(),[](unsigned char c){return c<128;}))return input;
 std::string value=mentionDisplayText(repairDoubleUtf8(input));
 for(auto unit:{std::make_pair("℃","°C"),std::make_pair("℉","°F")}){size_t at=0;while((at=value.find(unit.first,at))!=std::string::npos){value.replace(at,std::string(unit.first).size(),unit.second);at+=std::string(unit.second).size();}}
 // The Plus faces carry Latin-1 (à è é ì ò ù ç ñ « » ...): only the typography beyond it is mapped to ASCII.
 static const char *from[]={"’","‘","“","”","„","…","–","—"};
 static const char *to[]={"'","'","\"","\"","\"","...","-","-"};
 static_assert(sizeof(from)==sizeof(to),"displayText map");
 for(size_t i=0;i<sizeof(from)/sizeof(from[0]);i++){size_t at=0;while((at=value.find(from[i],at))!=std::string::npos){value.replace(at,std::string(from[i]).size(),to[i]);at++;}}return value;
}
#include "aa_fonts.h"
static bool chatLarge=true;
void setChatTextLarge(bool large){chatLarge=large;}bool chatTextLarge(){return chatLarge;}
// T-Deck Plus: anti-aliased Inter faces.
const GFXfont *Components::font(Font f){switch(f){
 case Font::Small:return &mm_aa::Small;case Font::Body:return &mm_aa::Body;case Font::Bold:return &mm_aa::Bold;case Font::Title:return &mm_aa::Title;
 case Font::Brand:return &mm_aa::Brand;case Font::Clock:return &mm_aa::Clock;case Font::Tiny:return &mm_aa::Tiny;case Font::HomeClock:return &mm_aa::LockClock;/* same Inter Bold 47/64 face: one copy in flash */
 case Font::LockText:return &mm_aa::Small;/* never drawn (its index only bounds the ASCII fast path): no 10 KB face linked */case Font::ChatBody:return &mm_aa::ChatBody;case Font::ChatMeta:return &mm_aa::ChatMeta;case Font::ChatTitle:return &mm_aa::ChatTitle;
 case Font::MenuClock:return &mm_aa::MenuClock;case Font::MenuLabel:return &mm_aa::MenuLabel;case Font::LockClock:return &mm_aa::LockClock;case Font::NavLabel:return &mm_aa::NavLabel;case Font::ChatText:return chatLarge?&mm_aa::ChatText:&mm_aa::ChatTextSmall;}return &mm_aa::Body;}
// Cyrillic U+0400..U+0491 (Russian / Ukrainian): a second table of each text face (ui/aa_fonts.h <Face>Cyr, code 32 =
// U+0400); clock faces have none.
constexpr uint32_t CyrFirst=0x400,CyrLast=0x491;
static const GFXfont *cyrillicFont(Font f){switch(f){
 case Font::Small:case Font::LockText:return &mm_aa::SmallCyr;case Font::Body:return &mm_aa::BodyCyr;case Font::Bold:return &mm_aa::BoldCyr;
 case Font::Title:return &mm_aa::TitleCyr;case Font::Brand:return &mm_aa::BrandCyr;case Font::Tiny:return &mm_aa::TinyCyr;
 case Font::ChatBody:return &mm_aa::ChatBodyCyr;case Font::ChatMeta:return &mm_aa::ChatMetaCyr;case Font::ChatTitle:return &mm_aa::ChatTitleCyr;
 case Font::MenuLabel:return &mm_aa::MenuLabelCyr;case Font::NavLabel:return &mm_aa::NavLabelCyr;case Font::ChatText:return chatLarge?&mm_aa::ChatTextCyr:&mm_aa::ChatTextSmallCyr;
 default:return nullptr;}}
static const GFXfont *cyrillicFor(uint32_t cp,Font f){return cp>=CyrFirst&&cp<=CyrLast?cyrillicFont(f):nullptr;}
// Constant tables (built once); only ChatText depends on the runtime text size.
int Components::ascent(Font f){static constexpr int a[]={10,10,10,12,17,28,7,48,11,10,9,13,30,7,47,9};return f==Font::ChatText?(((chatLarge?12:11))):a[int(f)];}
int Components::lineHeight(Font f){static constexpr int a[]={11,13,13,16,21,34,9,64,17,15,13,20,44,11,64,13};return f==Font::ChatText?(((chatLarge?17:16))):a[int(f)];}
// Regional indicators (U+1F1E6..U+1F1FF) come in pairs as flags; one text unit keeps the pair together.
static bool regionalIndicator(uint32_t cp){return cp>=0x1f1e6&&cp<=0x1f1ff;}
static size_t unitNext(const std::string &s,size_t at,uint32_t &cp){size_t next=utf8Next(s,at,cp);if(regionalIndicator(cp)){uint32_t second;size_t after=utf8Next(s,next,second);if(regionalIndicator(second))return after;}return next;}
int Components::flagAdvance(Font f,bool pair){return (pair?2:1)*unitAdvance('A',Font::Tiny)+7;}
void Components::flag(int x,int y,Font f,uint32_t first,uint32_t second,uint16_t color){bool pair=regionalIndicator(second);int h=std::min(14,lineHeight(f)-1),w=flagAdvance(f,pair)-2,top=y+(lineHeight(f)-1-h)/2;g.drawRoundRect(x,top,w,h,3,color);g.setFont(font(Font::Tiny));g.setTextSize(1);g.setTextWrap(false);g.setTextColor(color);g.setCursor(x+3,top+(h+ascent(Font::Tiny))/2-1);g.write(uint8_t('A'+first-0x1f1e6));if(pair)g.write(uint8_t('A'+second-0x1f1e6));g.setFont(font(f));}
// Colour emoji (Noto Color Emoji): the 14 px set for text, the 24 px set for the picker; other sizes scale the nearest set.
static int emojiColorIndex(const mm_emoji_color::Entry *table,int count,uint32_t cp){int lo=0,hi=count;while(lo<hi){int mid=(lo+hi)/2;if(table[mid].code<cp)lo=mid+1;else hi=mid;}return lo<count&&table[lo].code==cp?table[lo].index:-1;}
// Emoji smaller than their sprite set (16 px names and previews) are averaged down once per emoji and size into PSRAM
// (rgb + 4-bit alpha, ~640 bytes at 16 px); later frames draw the cached copy as fast as a pre-rendered sprite.
// Sprites are raw-deflated one by one (ui/packed.h): inflated on first use into 64 direct-mapped PSRAM slots of the
// 30 px size (2250 bytes, 144 KB), keyed by set and index. Layout: side*side RGB565 words, then the 4-bit alpha.
static const uint8_t *emojiSprite(int side,int index){
 using namespace mm_emoji_color;constexpr int Slots=64;constexpr size_t SlotBytes=size_t(PickerPixels)*5/2;
 static uint8_t *slots=nullptr;static int32_t keys[Slots];
 if(!slots){slots=static_cast<uint8_t*>(heap_caps_malloc(Slots*SlotBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!slots)return nullptr;for(auto &k:keys)k=-1;}
 const bool picker=side==PickerSize;const int32_t key=index*2+picker;const int slot=int((uint32_t(key)*2654435761u)>>26);
 uint8_t *out=slots+slot*SlotBytes;if(keys[slot]==key)return out;
 const uint8_t *data=picker?PickerData:ChatBigData;const uint32_t *offset=picker?PickerOffset:ChatBigOffset;
 keys[slot]=-1;if(!inflateRaw(data+offset[index],offset[index+1]-offset[index],out,size_t(side)*side*5/2))return nullptr;
 keys[slot]=key;return out;}
struct SmallEmoji {uint16_t key;uint8_t size;uint16_t *rgb;uint8_t *alpha;};
static const SmallEmoji *smallEmoji(int side,int index,int size){
 static SmallEmoji *cache=nullptr;static int used=0;// open addressing on (set, index, size); full = draw uncached
 if(!cache){cache=static_cast<SmallEmoji*>(heap_caps_calloc(512,sizeof(SmallEmoji),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!cache)return nullptr;}// 6 KB, off the internal RAM
 const uint16_t key=uint16_t(index*2+(side==mm_emoji_color::PickerSize));uint32_t slot=(uint32_t(key)*2654435761u+size)%512;
 for(int probe=0;probe<512;probe++,slot=(slot+1)%512){SmallEmoji &e=cache[slot];
  if(e.rgb&&e.key==key&&e.size==size)return &e;
  if(!e.rgb){if(used>=448)return nullptr;const int n=size*size;
   const uint8_t *sprite=emojiSprite(side,index);if(!sprite)return nullptr;
   const uint16_t *rgb=reinterpret_cast<const uint16_t*>(sprite);const uint8_t *alpha=sprite+size_t(side)*side*2;
   auto *block=static_cast<uint8_t*>(heap_caps_malloc(size_t(n)*2+(n+1)/2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!block)return nullptr;
   auto *px=reinterpret_cast<uint16_t*>(block);uint8_t *a=block+size_t(n)*2;memset(block,0,size_t(n)*2+(n+1)/2);
   if(!FastCanvas16::areaReduce(rgb,alpha,side,side,size,0,size,[&](int xx,int yy,uint16_t ink,int v){int i=yy*size+xx;px[i]=ink;a[i>>1]|=uint8_t(v<<((i&1)?0:4));})){heap_caps_free(block);return nullptr;}
   e={key,uint8_t(size),px,a};used++;return &e;}}
 return nullptr;}
static bool emojiColor(FastCanvas16 *fast,uint32_t cp,int x,int y,int size){
 if(!fast)return false;using namespace mm_emoji_color;
 // Pre-rendered sizes: 30 px (picker set) for jumbo emoji, 20 px for chat bubbles. Smaller emoji (names, previews:
 // 16 px) are averaged down from the 20 px sprite; larger ones are scaled from the nearest set.
 int index=-1,side=0;
 if(size>=25&&(index=emojiColorIndex(PickerTable,PickerCount,cp))>=0)side=PickerSize;
 else if((index=emojiColorIndex(ChatBigTable,ChatBigCount,cp))>=0)side=ChatBigSize;
 if(index<0)return false;
 if(size<side)if(const SmallEmoji *small=smallEmoji(side,index,size)){fast->drawRGBA(x,y,small->rgb,small->alpha,size,size);return true;}
 const uint8_t *sprite=emojiSprite(side,index);if(!sprite)return false;
 const uint16_t *rgb=reinterpret_cast<const uint16_t*>(sprite);const uint8_t *alpha=sprite+size_t(side)*side*2;
 if(size<side)fast->drawRGBAArea(x,y,rgb,alpha,side,side,size);
 else fast->drawRGBAScaled(x,y,rgb,alpha,side,side,size);return true;}
void Components::emoji(uint32_t cp,int x,int y,int size,uint16_t color){
 // Every chat emoji has a colour sprite (same 1441 codepoints as the 1-bit table): a miss is drawn as a box, so the
 // 1-bit fallback table (46 KB) is not linked on the Plus.
 if(!emojiColor(fast,cp,x,y,size))g.drawRect(x+1,y+1,size-2,size-2,color);}
static int degreeRadius(Font f){return std::max(2,Components::ascent(f)/5);}
// Inline emoji: 16 px colour sprites on the Plus (chat rows are 16/17 px), 14 px 1-bit glyphs on the Max.
static int emojiOverride=0;// chat bubbles on the Plus draw bigger emoji (20 px inline, 30 px emoji-only messages)
void Components::inlineEmoji(int px){emojiOverride=px;}
static int emojiSize(Font f){if(emojiOverride)return emojiOverride;return (std::min(16,Components::lineHeight(f)));}
// A unit text() hands to emoji(): not a font glyph, not a degree sign or an invisible joiner; flags are drawn apart.
static bool emojiUnit(uint32_t cp,const GFXfont *face){return !invisibleCodepoint(cp)&&cp!=0xb0&&cp!=0x2070&&!(cp>=face->first&&cp<=face->last)&&!(cp>=CyrFirst&&cp<=CyrLast)&&!regionalIndicator(cp);}
bool Components::hasEmoji(const std::string &s,Font f){auto value=displayText(s);auto face=font(f);uint32_t cp;for(size_t at=0;at<value.size();){size_t next=unitNext(value,at,cp);if(emojiUnit(cp,face))return true;at=next;}return false;}
int Components::emojiOnlyCount(const std::string &s,Font f){auto value=displayText(s);auto face=font(f);uint32_t cp;int n=0;for(size_t at=0;at<value.size();){size_t next=unitNext(value,at,cp);if(emojiUnit(cp,face))n++;else if(cp!=' '&&!invisibleCodepoint(cp))return 0;at=next;}return n<=3?n:0;}
int Components::unitAdvance(uint32_t cp,Font f){if(invisibleCodepoint(cp))return 0;if(cp==0xb0||cp==0x2070)return degreeRadius(f)*2+3;auto face=font(f);if(cp>=face->first&&cp<=face->last)return face->glyph[cp-face->first].xAdvance;if(auto cyr=cyrillicFor(cp,f))return cyr->glyph[cp-CyrFirst+32-cyr->first].xAdvance;return emojiSize(f)+2;}
std::string Components::display(const std::string &s){return displayText(s);}
int Components::width(const std::string &s,Font f){return rawWidth(displayText(s),f);}
// Width of text already passed through displayText(): fit/lines/inputTail measure prefixes many times and must not repeat that pass.
int Components::rawWidth(const std::string &value,Font f){if(int(f)<=int(Font::LockText)&&std::all_of(value.begin(),value.end(),[](unsigned char c){return c<128;})){g.setFont(font(f));g.setTextSize(1);g.setTextWrap(false);int16_t x,y;uint16_t w,h;g.getTextBounds(value.c_str(),0,0,&x,&y,&w,&h);return w;}int total=0;uint32_t cp;for(size_t at=0;at<value.size();){size_t next=unitNext(value,at,cp);total+=regionalIndicator(cp)?flagAdvance(f,next-at>4):unitAdvance(cp,f);at=next;}return total;}
// Incremental width of a displayed string, unit by unit: exactly rawWidth() of the concatenation (Adafruit
// getTextBounds with wrap off for plain ASCII in the bitmap faces, the sum of unit advances otherwise), without
// re-measuring the whole prefix for every unit as fit()/lines() used to.
struct TextMeter{Components *c;Font f;const GFXfont *face;bool bounds;int16_t x=0,minx=0x7FFF,maxx=-1;int adv=0;bool ascii=true;
 TextMeter(Components *comp,Font font):c(comp),f(font),face(Components::font(font)),bounds(int(font)<=int(Font::LockText)){}
 void byte(unsigned char ch){if(ch=='\n'){x=0;return;}if(ch=='\r'||ch<face->first||ch>face->last)return;const GFXglyph &gl=face->glyph[ch-face->first];
  int16_t x1=int16_t(x+int8_t(gl.xOffset)),x2=int16_t(x1+gl.width-1);if(x1<minx)minx=x1;if(x2>maxx)maxx=x2;x=int16_t(x+gl.xAdvance);}
 void add(const std::string &value,size_t at,size_t next,uint32_t cp){for(size_t i=at;i<next;i++){unsigned char ch=value[i];if(ch>=128)ascii=false;else if(bounds)byte(ch);}
  adv+=regionalIndicator(cp)?c->flagAdvance(f,next-at>4):c->unitAdvance(cp,f);}
 void addAll(const std::string &value){uint32_t cp;for(size_t at=0;at<value.size();){size_t next=unitNext(value,at,cp);add(value,at,next,cp);at=next;}}
 int width()const{return bounds&&ascii?(maxx>=minx?maxx-minx+1:0):adv;}
};
std::string Components::fitDisplayed(const std::string &value,int w,Font f){
 if(rawWidth(value,f)<=w)return value;g.setTextWrap(false);static const std::string dots="..";
 TextMeter m(this,f);std::string out;uint32_t cp;
 for(size_t at=0;at<value.size();){size_t next=unitNext(value,at,cp);TextMeter t=m;t.add(value,at,next,cp);t.addAll(dots);if(t.width()>w)break;m.add(value,at,next,cp);out.append(value,at,next-at);at=next;}
 return out+"..";}
std::string Components::fit(const std::string &s,int w,Font f){auto value=displayText(s);if(rawWidth(value,f)<=w)return s;return fitDisplayed(value,w,f);}
// Tail of the input that fits in w: suffix widths come from one backward pass (glyph bounds shift with the start).
std::string Components::inputTail(const std::string &s,int w,Font f){
 auto value=displayText(s);if(rawWidth(value,f)<=w)return value;
 if(value.find_first_of("\n\r")!=std::string::npos){size_t start=0;uint32_t cp;while(start<value.size()&&rawWidth(value.substr(start),f)>w)start=unitNext(value,start,cp);return value.substr(start);}
 const GFXfont *face=font(f);const bool bounds=int(f)<=int(Font::LockText);const size_t n=value.size();
 std::vector<int> X(n+1,0),lo(n+1,INT32_MAX),hi(n+1,INT32_MIN),advFrom(n+1,0);std::vector<char> asciiFrom(n+1,1);
 for(size_t b=0;b<n;b++){unsigned char ch=value[b];X[b+1]=X[b];if(ch<128&&ch>=face->first&&ch<=face->last){const GFXglyph &gl=face->glyph[ch-face->first];X[b+1]=X[b]+gl.xAdvance;}}
 for(size_t b=n;b-->0;){unsigned char ch=value[b];lo[b]=lo[b+1];hi[b]=hi[b+1];asciiFrom[b]=asciiFrom[b+1]&&ch<128;
  if(ch<128&&ch>=face->first&&ch<=face->last){const GFXglyph &gl=face->glyph[ch-face->first];int a=X[b]+int8_t(gl.xOffset);lo[b]=std::min(lo[b],a);hi[b]=std::max(hi[b],a+gl.width-1);}}
 std::vector<size_t> starts;uint32_t cp;for(size_t at=0;at<n;){starts.push_back(at);at=unitNext(value,at,cp);}
 for(size_t u=starts.size();u-->0;){size_t at=starts[u],next=u+1<starts.size()?starts[u+1]:n;utf8Next(value,at,cp);advFrom[at]=advFrom[next]+(regionalIndicator(cp)?flagAdvance(f,next-at>4):unitAdvance(cp,f));}
 for(size_t at:starts){int width;if(bounds&&asciiFrom[at]){int mn=lo[at]==INT32_MAX?0x7FFF:std::min(0x7FFF,lo[at]-X[at]),mx=hi[at]==INT32_MIN?-1:std::max(-1,hi[at]-X[at]);width=mx>=mn?mx-mn+1:0;}else width=advFrom[at];
  if(width<=w)return value.substr(at);}
 return std::string();
}
void Components::drawDisplayed(int x,int y,const std::string &value,Font f,uint16_t color){g.setFont(font(f));g.setTextSize(1);g.setTextWrap(false);g.setTextColor(color);uint32_t cp;for(size_t at=0;at<value.size();){size_t next=unitNext(value,at,cp);if(regionalIndicator(cp)){uint32_t second=0;if(next-at>4)utf8Next(value,at+4,second);flag(x,y,f,cp,second,color);x+=flagAdvance(f,next-at>4);at=next;continue;}at=next;if(invisibleCodepoint(cp))continue;auto face=font(f);if(cp==0xb0||cp==0x2070){int r=degreeRadius(f);g.drawCircle(x+r,y+r,r,color);if(r>=4)g.drawCircle(x+r,y+r,r-1,color);}else if(cp>=face->first&&cp<=face->last){g.setCursor(x,y+ascent(f));g.write(uint8_t(cp));}else if(auto cyr=cyrillicFor(cp,f)){g.setFont(cyr);g.setCursor(x,y+ascent(f));g.write(uint8_t(cp-CyrFirst+32));g.setFont(face);}else{int e=emojiSize(f);emoji(cp,x,y+(lineHeight(f)-e)/2,e,color);}x+=unitAdvance(cp,f);}}
void Components::text(int x,int y,const std::string &s,Font f,uint16_t color,int maxWidth){auto value=displayText(s);if(maxWidth)value=fitDisplayed(value,maxWidth,f);drawDisplayed(x,y,value,f,color);}
void Components::centered(int x,int y,int w,const std::string &s,Font f,uint16_t color){auto v=fitDisplayed(displayText(s),w,f);drawDisplayed(x+(w-rawWidth(v,f))/2,y,v,f,color);}
std::vector<std::string> Components::lines(const std::string &s,int maxW,Font f){
 std::vector<std::string> out;std::string line,word;auto value=displayText(s);g.setTextWrap(false);
 TextMeter lm(this,f);static const std::string space=" ";
 auto emit=[&](){out.push_back(line);line.clear();lm=TextMeter(this,f);};
 auto add=[&](){if(word.empty())return;if(!line.empty()){TextMeter t=lm;t.addAll(space);t.addAll(word);if(t.width()>maxW)emit();}if(!line.empty()){line+=' ';lm.addAll(space);}
  uint32_t cp;for(size_t at=0;at<word.size();){size_t next=unitNext(word,at,cp);TextMeter t=lm;t.add(word,at,next,cp);if(t.width()>maxW&&!line.empty()){emit();t=lm;t.add(word,at,next,cp);}line.append(word,at,next-at);lm=t;at=next;}word.clear();};
 uint32_t cp;for(size_t at=0;at<value.size();){size_t next=utf8Next(value,at,cp);if(cp==' '||cp=='\n'){add();if(cp=='\n')emit();}else word.append(value,at,next-at);at=next;}add();if(!line.empty())emit();if(out.empty())out.push_back("");return out;
}
void Components::badge(int cx,int cy,int r,int count,uint16_t fill,uint16_t ink){g.fillCircle(cx,cy,r,fill);centered(cx-r,cy-4,2*r,count>99?"99+":std::to_string(count),Font::MenuLabel,ink);}
namespace {struct Touchable{int16_t x,y,w,h,radius;};Touchable touchables[160];int touchableCount=0;// radius -1: blocker
 void record(Rect r,int radius){int x0=std::max(0,r.x),y0=std::max(0,r.y),x1=std::min(W,r.x+r.w),y1=std::min(H,r.y+r.h);// clipped to the screen
  if(x1-x0<16||y1-y0<14||touchableCount>=int(sizeof(touchables)/sizeof(touchables[0])))return;touchables[touchableCount++]={int16_t(x0),int16_t(y0),int16_t(x1-x0),int16_t(y1-y0),int16_t(radius)};}}
void touchablesClear(){touchableCount=0;}
void touchableAdd(Rect r,int radius){record(r,std::max(0,radius));}
void touchableBlock(Rect r){record(r,-1);}
bool touchableAt(int x,int y,Rect &r,int &radius){
 for(int i=touchableCount-1;i>=0;i--){const auto &t=touchables[i];if(x<t.x||y<t.y||x>=t.x+t.w||y>=t.y+t.h)continue;
  if(t.radius<0||t.w*t.h>W*H/2)return false;r={t.x,t.y,t.w,t.h};radius=t.radius;return true;}
 return false;}
void Components::pressRing(Rect r,int radius,uint16_t ink){g.drawRoundRect(r.x,r.y,r.w,r.h,radius,ink);g.drawRoundRect(r.x+1,r.y+1,r.w-2,r.h-2,std::max(0,radius-1),ink);}
void Components::tab(Rect r,const std::string &label,bool selected,bool open){{tabChip(r,label,selected);return;}}
std::vector<std::string> Components::clampLines(const std::string &s,int w,Font f,int maxRows,const char *ellipsis){auto out=lines(s,w,f);if(maxRows>0&&int(out.size())>maxRows){out.resize(maxRows);out.back()=fit(out.back()+ellipsis,w,f);}return out;}
// Anti-aliased Material Design Icons (ui/aa_icons.h) at 12, 16, 24 and 32 px, drawn on the LCD frame only.
void Components::icon(Icon id,int x,int y,int size,uint16_t color){
 if(fast&&(size==12||size==16||size==24||size==32))fast->drawAlpha(x,y,mm_aa_icons::sprite(size,int(id)),size,size,color);
}
void Components::packIcon(PackIcon id,int x,int y,int size,uint16_t color){
 if(fast&&(size==12||size==16||size==24||size==32))fast->drawAlpha(x,y,mm_aa_icons::sprite(size,mm_aa_icons::PackFirst+int(id)),size,size,color);}
void Components::weatherIcon(int code,int x,int y,int size){
 if(fast){if(auto bits=mm_aa_icons::weather(size,int(weatherGlyph(code)))){fast->drawAlpha(x,y,bits,size,size,FG);return;}}
 drawWeatherGlyph(g,weatherGlyph(code),x,y,size,FG);}
// Filled cards: accent when selected, soft surface otherwise.
void Components::card(Rect r,bool selected,int radius){{touchableAdd(r,std::max(radius,6));fillRound(r.x,r.y,r.w,r.h,std::max(radius,6),selected?Accent:Surface);return;}}
void Components::slider(Rect r,int percent){percent=std::max(0,std::min(100,percent));
 {int x=r.x+10,w=r.w-20,y=r.y+r.h/2-3,fill=w*percent/100;fillRound(x,y,w,6,3,Line);if(fill)fillRound(x,y,fill,6,3,Accent);g.fillCircle(x+fill,y+3,7,Accent);g.fillCircle(x+fill,y+3,3,OnAccent);return;}}
// Four-bar link quality from SNR (SF8 decodes down to about -10 dB).
void Components::signalBars(int x,int bottom,float snr,uint16_t ink,int scale){int level=snr>=5?4:snr>=0?3:snr>=-5?2:snr>=-10?1:0;for(int i=0;i<4;i++){int h=(5+i*4)*scale,w=4*scale,bx=x+i*(w+scale);if(i<level)g.fillRect(bx,bottom-h,w,h,ink);else g.drawRect(bx,bottom-h,w,h,ink);}}
void Components::button(Rect r,const std::string &label,bool selected){touchableAdd(r,8);{if(selected)fillRound(r.x,r.y,r.w,r.h,8,Accent);else{fillRound(r.x,r.y,r.w,r.h,8,BG);g.drawRoundRect(r.x,r.y,r.w,r.h,8,Line);}}centered(r.x,r.y+(r.h-13)/2,r.w,label,Font::Bold,selected?OnAccent:FG);}
void Components::iconButton(Rect r,Icon id,bool selected){if(selected)card(r,true);else touchableAdd(r,8);icon(id,r.x+(r.w-16)/2,r.y+(r.h-16)/2,16,selected?OnAccent:FG);}
StatusLayout Components::statusLayout(const Status &s){
 StatusLayout layout;layout.percent=s.battery<0?"--%":std::to_string(std::min(100,s.battery))+"%";
 layout.percentWidth=width(layout.percent,Font::ChatBody);layout.percentX=W-StatusRightMargin-layout.percentWidth;
 layout.batteryX=layout.percentX-StatusBatteryGap-StatusBatteryW;layout.iconsRight=layout.batteryX-StatusIndicatorGap;
 return layout;
}
void Components::statusBar(const Status &s){
 g.fillRect(0,0,W,StatusH,StatusBg);
 text(Margin,StatusTextY,s.clock,Font::ChatBody,StatusFg,StatusClockW);
 if(!s.keyboard.empty())text(StatusAntennaX,StatusTextY,s.keyboard,Font::ChatMeta,StatusFg,StatusAntennaW);// keyboard language (Alt + Space)
 // the T-Deck Plus has one LoRa antenna
 const Icon indicators[]={Icon::Messages,Icon::GPS,Icon::Wifi,Icon::Bluetooth};
 const bool active[]={s.unread>0,s.gps,s.wifi,s.bt};
 int count=0;for(bool on:active)if(on)count++;
 auto layout=statusLayout(s);
 int first=layout.iconsRight-(count?(count-1)*StatusIconStep+IconStatus:0),visible=0;
 for(int i=0;i<4;i++)if(active[i])icon(indicators[i],first+visible++*StatusIconStep,StatusIconY,IconStatus,StatusFg);// MDI sprites share one grid
 text(layout.percentX,StatusTextY,layout.percent,Font::ChatBody,StatusFg);
 // iOS-style battery: rounded body, soft outline, level filled in the theme accent, rounded terminal; bolt when charging.
 {int x=layout.batteryX,y=StatusBatteryY,bw=StatusBatteryW-3,bh=StatusBatteryH;int level=std::max(0,std::min(100,s.battery));
  g.drawRoundRect(x,y,bw,bh,4,StatusFg);fillRound(x+bw+1,y+bh/2-2,2,4,1,StatusFg);
  int fill=(bw-4)*level/100;if(level>0&&fill<3)fill=3;if(fill)fillRound(x+2,y+2,fill,bh-4,2,level<=20?rgb(0xE0,0x45,0x3A):Accent);
  if(s.charging){int cx=x+bw/2;g.fillTriangle(cx+1,y+1,cx-4,y+bh/2+1,cx,y+bh/2+1,StatusFg);g.fillTriangle(cx-1,y+bh-1,cx+4,y+bh/2-1,cx,y+bh/2-1,StatusFg);}}
 g.drawFastHLine(0,StatusH-1,W,Line);
}
void Components::pageHeader(const std::string &title,bool back,Icon right){if(back)iconButton(BackButton,Icon::Back);else iconButton(BackButton,Icon::Mesh);text(34,StatusH+8,title,Font::Title,FG,W-72);iconButton(ComposeButton,right);g.drawFastHLine(0,Top-1,W,Line);}
void Components::footer(const std::string &left,const std::string &right){g.fillRect(0,FooterY,W,FooterH,BG);g.drawFastHLine(0,FooterY,W,Line);text(Margin,FooterY+4,left,Font::Small,Muted,W/2-12);auto label=fit(right,W/2-Margin-4,Font::Small);int ww=width(label,Font::Small);text(W-Margin-ww,FooterY+4,label,Font::Small,Muted);}
void Components::menuRow(Rect r,Icon id,const std::string &label,const std::string &detail,bool selected){card(r,selected);auto color=selected?OnAccent:FG;icon(id,r.x+6,r.y+(r.h-24)/2,24,color);text(r.x+36,r.y+4,label,Font::ChatBody,color,r.w-56);text(r.x+36,r.y+20,detail,Font::ChatMeta,color,r.w-56);icon(Icon::ChevronRight,r.x+r.w-16,r.y+(r.h-12)/2,12,color);}
void Components::toggleRow(Rect r,Icon id,const std::string &label,const std::string &detail,bool on,const std::string &pill){
 card(r);icon(id,r.x+6,r.y+(r.h-24)/2,24);auto state=pill.empty()?std::string(on?"ON":"OFF"):pill;int pw=std::max(44,width(state,Font::Bold)+16),px=r.x+r.w-pw-6,py=r.y+(r.h-22)/2;
 text(r.x+36,r.y+5,label,Font::ChatBody,FG,px-r.x-40);text(r.x+36,r.y+22,detail,Font::ChatMeta,FG,px-r.x-40);
 if(on)fillRound(px,py,pw,22,11,Accent);else g.drawRoundRect(px,py,pw,22,11,Muted);centered(px,py+5,pw,state,Font::Bold,on?OnAccent:FG);}
void Components::tabChip(Rect r,const std::string &label,bool selected){touchableAdd(r,5);fillRound(r.x,r.y,r.w,r.h,5,selected?Accent:Surface);centered(r.x,r.y+(r.h-13)/2,r.w,label,selected?Font::Bold:Font::Body,selected?OnAccent:FG);}
void Components::image(int x,int y,const uint16_t *pixels,int w,int h){
 if(fast){fast->blit(x,y,pixels,w,h);return;}
 g.drawRGBBitmap(x,y,pixels,w,h);}
void Components::tabPanel(Rect r){g.drawRoundRect(r.x,r.y,r.w,r.h,Radius,Line);}
void Components::sectionLabel(int y,const std::string &label,const std::string &right){text(Margin+2,y,label,Font::ChatMeta,Muted);if(!right.empty())text(W-Margin-2-width(right,Font::ChatMeta),y,right,Font::ChatMeta,Muted);}
}
