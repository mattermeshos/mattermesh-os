#include "components.h"
#include "weather_icons.h"
#include "icon_pack_bitmap.h"
#include "fonts_bitmap.h"
#include "v2_font.h"
#include "chat_body_font.h"
#include "chat_meta_font.h"
#include "chat_title_font.h"
#include "menu_clock_font.h"
#include "menu_label_font.h"
#include "icons_bitmap.h"
#include "lock_clock_font.h"
#include "nav_label_font.h"
#include "emoji_bitmap.h"
#include "utf8.h"
#include <algorithm>
namespace mm {
// Text some senders encode twice (UTF-8 read as Latin-1 and encoded again): "Però" arrives as "PerÃ²". A Latin-1
// lead (U+00C2..U+00F4) followed by exactly the continuations it needs (U+0080..U+00BF) is put back together.
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
// Preserve UTF-8 in storage/protocol. Latin-1 (à è é ...) is drawn by the chat faces; faces without it fold it to
// ASCII when drawing (latinFold). Typography beyond Latin-1 that phone keyboards insert is mapped to ASCII here.
static std::string displayText(const std::string &in){
 // Fast path (most strings): every rewrite below needs a mention "@[", a 0xC3 lead (double-encoded UTF-8) or a 0xE2
 // lead (the typography table), so text without them comes back unchanged with a single copy.
 if(in.find('\xc3')==std::string::npos&&in.find('\xe2')==std::string::npos&&in.find("@[")==std::string::npos)return in;
 std::string value=mentionDisplayText(repairDoubleUtf8(in));
 for(auto unit:{std::make_pair("℃","°C"),std::make_pair("℉","°F")}){size_t at=0;while((at=value.find(unit.first,at))!=std::string::npos){value.replace(at,std::string(unit.first).size(),unit.second);at+=std::string(unit.second).size();}}
 static const char *from[]={"’","‘","“","”","„","…","–","—"};
 static const char *to[]={"'","'","\"","\"","\"","...","-","-"};
 for(size_t i=0;i<sizeof(from)/sizeof(from[0]);i++){size_t at=0;while((at=value.find(from[i],at))!=std::string::npos){value.replace(at,std::string(from[i]).size(),to[i]);at++;}}return value;
}
// ASCII stand-in for a Latin-1 letter in a face that stops at 126 (menus, clock, labels); 0 = not Latin-1.
static char latinFold(uint32_t cp){static const char fold[]=" !cL$Y|S\"ca\"--R-o+23'uP.,1o\"????AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";return cp>=0xa0&&cp<=0xff&&cp!=0xad?fold[cp-0xa0]:0;}
const GFXfont *Components::font(Font f){switch(f){
 case Font::Small:return &mm_fonts::Small;case Font::Body:return &mm_fonts::Body;
 case Font::Bold:return &mm_fonts::Bold;case Font::Title:return &mm_fonts::Title;
 case Font::Brand:return &mm_fonts::Brand;case Font::Clock:return &mm_fonts::Clock;case Font::Tiny:return &mm_v2::Tiny;case Font::ChatBody:return &mm_chat_body::Text;case Font::ChatMeta:return &mm_chat_meta::Text;case Font::ChatTitle:return &mm_chat_title::Text;case Font::MenuClock:return &mm_menu_clock::Text;case Font::MenuLabel:return &mm_menu_label::Text;case Font::LockClock:return &mm_lock_clock::Text;case Font::NavLabel:return &mm_nav_label::Text;}return &mm_fonts::Body;}
int Components::ascent(Font f){const int a[]={10,10,10,12,17,28,7,48,11,10,9,13,30,7,47,9};return a[int(f)];}
int Components::lineHeight(Font f){const int a[]={11,13,13,16,21,34,9,64,17,15,13,20,44,11,64,13};return a[int(f)];}
static const mm_emoji::Glyph *emojiGlyph(uint32_t cp){int lo=0,hi=mm_emoji::count;while(lo<hi){int mid=(lo+hi)/2;if(mm_emoji::glyphs[mid].code<cp)lo=mid+1;else hi=mid;}return lo<mm_emoji::count&&mm_emoji::glyphs[lo].code==cp?&mm_emoji::glyphs[lo]:nullptr;}
// Regional indicators (U+1F1E6..U+1F1FF) come in pairs as flags; one text unit keeps the pair together.
static bool regionalIndicator(uint32_t cp){return cp>=0x1f1e6&&cp<=0x1f1ff;}
static size_t unitNext(const std::string &s,size_t at,uint32_t &cp){size_t next=utf8Next(s,at,cp);if(regionalIndicator(cp)){uint32_t second;size_t after=utf8Next(s,next,second);if(regionalIndicator(second))return after;}return next;}
int Components::flagAdvance(Font f,bool pair){return (pair?2:1)*unitAdvance('A',Font::Tiny)+7;}
void Components::flag(int x,int y,Font f,uint32_t first,uint32_t second,uint16_t color){bool pair=regionalIndicator(second);int h=std::min(14,lineHeight(f)-1),w=flagAdvance(f,pair)-2,top=y+(lineHeight(f)-1-h)/2;g.drawRoundRect(x,top,w,h,3,color);g.setFont(font(Font::Tiny));g.setTextSize(1);g.setTextWrap(false);g.setTextColor(color);g.setCursor(x+3,top+(h+ascent(Font::Tiny))/2-1);g.write(uint8_t('A'+first-0x1f1e6));if(pair)g.write(uint8_t('A'+second-0x1f1e6));g.setFont(font(f));}
void Components::emoji(uint32_t cp,int x,int y,int size,uint16_t color){auto sprite=emojiGlyph(cp);if(!sprite){g.drawRect(x+1,y+1,size-2,size-2,color);return;}
 if(size==14){g.drawBitmap(x,y,sprite->pixels,14,14,color);return;}// native size: same 2-byte MSB-first rows, no per-pixel scaling
 for(int yy=0;yy<size;yy++)for(int xx=0;xx<size;xx++){int sx=xx*14/size,sy=yy*14/size;if(sprite->pixels[sy*2+sx/8]&(128>>(sx%8)))g.drawPixel(x+xx,y+yy,color);}}
static int degreeRadius(Font f){return std::max(2,Components::ascent(f)/5);}
// Cyrillic (U+0400..U+0491) lives in a second table of the chat fonts (code 32 = U+0400, tools/build_chat_fonts.py);
// the text faces borrow the chat table of a similar size, so Russian / Ukrainian names and messages never show boxes
// (the tiny label and clock faces only draw fixed English text and have none).
constexpr uint32_t CyrFirst=0x400,CyrLast=0x491;
static const GFXfont *cyrillicFont(Font f){switch(f){
 case Font::ChatBody:case Font::Body:case Font::Bold:case Font::LockText:return &mm_chat_body::TextCyr;
 case Font::ChatTitle:case Font::Title:case Font::Brand:return &mm_chat_title::TextCyr;
 case Font::ChatMeta:case Font::Small:return &mm_chat_meta::TextCyr;
 default:return nullptr;}}
static const GFXfont *cyrillicFor(uint32_t cp,Font f){return cp>=CyrFirst&&cp<=CyrLast?cyrillicFont(f):nullptr;}
int Components::unitAdvance(uint32_t cp,Font f){if(invisibleCodepoint(cp))return 0;if(cp==0xb0||cp==0x2070)return degreeRadius(f)*2+3;auto face=font(f);if(cp>=face->first&&cp<=face->last)return face->glyph[cp-face->first].xAdvance;if(auto cyr=cyrillicFor(cp,f))return cyr->glyph[cp-CyrFirst].xAdvance;if(char c=latinFold(cp))return face->glyph[uint8_t(c)-face->first].xAdvance;if(cp==0xad)return 0;return std::min(14,lineHeight(f)-1)+2;}
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
std::string Components::fit(const std::string &s,int w,Font f){auto value=displayText(s);if(rawWidth(value,f)<=w)return s;return fitDisplayed(value,w,f);}
// fit() on text already passed through displayText(); the result is displayed text too.
std::string Components::fitDisplayed(const std::string &value,int w,Font f){if(rawWidth(value,f)<=w)return value;static const std::string dots="..";
 TextMeter m(this,f);std::string out;uint32_t cp;
 for(size_t at=0;at<value.size();){size_t next=unitNext(value,at,cp);TextMeter t=m;t.add(value,at,next,cp);t.addAll(dots);if(t.width()>w)break;m.add(value,at,next,cp);out.append(value,at,next-at);at=next;}
 return out+"..";}
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
// displayText() runs once per call: text() and centered() fit and measure the displayed string itself.
void Components::text(int x,int y,const std::string &s,Font f,uint16_t color,int maxWidth){auto value=displayText(s);if(maxWidth)value=fitDisplayed(value,maxWidth,f);drawDisplayed(x,y,value,f,color);}
void Components::drawDisplayed(int x,int y,const std::string &value,Font f,uint16_t color){g.setFont(font(f));g.setTextSize(1);g.setTextWrap(false);g.setTextColor(color);uint32_t cp;for(size_t at=0;at<value.size();){size_t next=unitNext(value,at,cp);if(regionalIndicator(cp)){uint32_t second=0;if(next-at>4)utf8Next(value,at+4,second);flag(x,y,f,cp,second,color);x+=flagAdvance(f,next-at>4);at=next;continue;}at=next;if(invisibleCodepoint(cp))continue;auto face=font(f);if(cp==0xb0||cp==0x2070){int r=degreeRadius(f);g.drawCircle(x+r,y+r,r,color);if(r>=4)g.drawCircle(x+r,y+r,r-1,color);}else if(cp>=face->first&&cp<=face->last){g.setCursor(x,y+ascent(f));g.write(uint8_t(cp));}else if(auto cyr=cyrillicFor(cp,f)){g.setFont(cyr);g.setCursor(x,y+ascent(f));g.write(uint8_t(cp-CyrFirst+32));g.setFont(face);}else if(char c=latinFold(cp)){g.setCursor(x,y+ascent(f));g.write(uint8_t(c));}else if(cp!=0xad)emoji(cp,x,y,std::min(14,lineHeight(f)-1),color);x+=unitAdvance(cp,f);}}
void Components::centered(int x,int y,int w,const std::string &s,Font f,uint16_t color){auto v=fitDisplayed(displayText(s),w,f);drawDisplayed(x+(w-rawWidth(v,f))/2,y,v,f,color);}
std::vector<std::string> Components::lines(const std::string &s,int maxW,Font f){
 std::vector<std::string> out;std::string line,word;auto value=displayText(s);g.setTextWrap(false);
 TextMeter lm(this,f);static const std::string space=" ";
 auto emit=[&](){out.push_back(line);line.clear();lm=TextMeter(this,f);};
 auto add=[&](){if(word.empty())return;if(!line.empty()){TextMeter t=lm;t.addAll(space);t.addAll(word);if(t.width()>maxW)emit();}if(!line.empty()){line+=' ';lm.addAll(space);}
  uint32_t cp;for(size_t at=0;at<word.size();){size_t next=unitNext(word,at,cp);TextMeter t=lm;t.add(word,at,next,cp);if(t.width()>maxW&&!line.empty()){emit();t=lm;t.add(word,at,next,cp);}line.append(word,at,next-at);lm=t;at=next;}word.clear();};
 uint32_t cp;for(size_t at=0;at<value.size();){size_t next=utf8Next(value,at,cp);if(cp==' '||cp=='\n'){add();if(cp=='\n')emit();}else word.append(value,at,next-at);at=next;}add();if(!line.empty())emit();if(out.empty())out.push_back("");return out;
}
void Components::icon(Icon id,int x,int y,int size,uint16_t color){const uint8_t *bmp=nullptr;
 switch(id){
 case Icon::Messages:bmp=size==12?mm_assets::icon_messages_12:size==16?mm_assets::icon_messages_16:size==24?mm_assets::icon_messages_24:mm_assets::icon_messages_32;break;
 case Icon::Mesh:bmp=size==12?mm_assets::icon_mesh_12:size==16?mm_assets::icon_mesh_16:size==24?mm_assets::icon_mesh_24:mm_assets::icon_mesh_32;break;

 case Icon::Weather:bmp=size==12?mm_assets::icon_weather_12:size==16?mm_assets::icon_weather_16:size==24?mm_assets::icon_weather_24:mm_assets::icon_weather_32;break;
 case Icon::Radio:bmp=size==12?mm_assets::icon_radio_12:size==16?mm_assets::icon_radio_16:size==24?mm_assets::icon_radio_24:mm_assets::icon_radio_32;break;
 case Icon::Files:bmp=size==12?mm_assets::icon_files_12:size==16?mm_assets::icon_files_16:size==24?mm_assets::icon_files_24:mm_assets::icon_files_32;break;
 case Icon::Terminal:bmp=size==12?mm_assets::icon_terminal_12:size==16?mm_assets::icon_terminal_16:size==24?mm_assets::icon_terminal_24:mm_assets::icon_terminal_32;break;
 case Icon::Notes:bmp=size==12?mm_assets::icon_notes_12:size==16?mm_assets::icon_notes_16:size==24?mm_assets::icon_notes_24:mm_assets::icon_notes_32;break;
 case Icon::Tools:bmp=size==12?mm_assets::icon_tools_12:size==16?mm_assets::icon_tools_16:size==24?mm_assets::icon_tools_24:mm_assets::icon_tools_32;break;
 case Icon::Settings:bmp=size==12?mm_assets::icon_settings_12:size==16?mm_assets::icon_settings_16:size==24?mm_assets::icon_settings_24:mm_assets::icon_settings_32;break;
 case Icon::GPS:bmp=size==12?mm_assets::icon_gps_12:size==16?mm_assets::icon_gps_16:size==24?mm_assets::icon_gps_24:mm_assets::icon_gps_32;break;
 case Icon::Info:bmp=size==12?mm_assets::icon_info_12:size==16?mm_assets::icon_info_16:size==24?mm_assets::icon_info_24:mm_assets::icon_info_32;break;
 case Icon::Battery:bmp=size==12?mm_assets::icon_battery_12:size==16?mm_assets::icon_battery_16:size==24?mm_assets::icon_battery_24:mm_assets::icon_battery_32;break;
 case Icon::Signal:bmp=size==12?mm_assets::icon_signal_12:size==16?mm_assets::icon_signal_16:size==24?mm_assets::icon_signal_24:mm_assets::icon_signal_32;break;
 case Icon::Wifi:bmp=size==12?mm_assets::icon_wifi_12:size==16?mm_assets::icon_wifi_16:size==24?mm_assets::icon_wifi_24:mm_assets::icon_wifi_32;break;
 case Icon::Bluetooth:bmp=size==12?mm_assets::icon_bluetooth_12:size==16?mm_assets::icon_bluetooth_16:size==24?mm_assets::icon_bluetooth_24:mm_assets::icon_bluetooth_32;break;
 case Icon::Contacts:bmp=size==12?mm_assets::icon_contacts_12:size==16?mm_assets::icon_contacts_16:size==24?mm_assets::icon_contacts_24:mm_assets::icon_contacts_32;break;
 case Icon::Keyboard:bmp=size==12?mm_assets::icon_keyboard_12:size==16?mm_assets::icon_keyboard_16:size==24?mm_assets::icon_keyboard_24:mm_assets::icon_keyboard_32;break;

 case Icon::Clock:bmp=size==12?mm_assets::icon_clock_12:size==16?mm_assets::icon_clock_16:size==24?mm_assets::icon_clock_24:mm_assets::icon_clock_32;break;
 case Icon::Lock:bmp=size==12?mm_assets::icon_lock_12:size==16?mm_assets::icon_lock_16:size==24?mm_assets::icon_lock_24:mm_assets::icon_lock_32;break;
 case Icon::Power:bmp=size==12?mm_assets::icon_power_12:size==16?mm_assets::icon_power_16:size==24?mm_assets::icon_power_24:mm_assets::icon_power_32;break;
 case Icon::Back:bmp=size==12?mm_assets::icon_back_12:size==16?mm_assets::icon_back_16:size==24?mm_assets::icon_back_24:mm_assets::icon_back_32;break;
 case Icon::Search:bmp=size==12?mm_assets::icon_search_12:size==16?mm_assets::icon_search_16:size==24?mm_assets::icon_search_24:mm_assets::icon_search_32;break;

 case Icon::Menu:bmp=size==12?mm_assets::icon_menu_12:size==16?mm_assets::icon_menu_16:size==24?mm_assets::icon_menu_24:mm_assets::icon_menu_32;break;

 case Icon::Plus:bmp=size==12?mm_assets::icon_plus_12:size==16?mm_assets::icon_plus_16:size==24?mm_assets::icon_plus_24:mm_assets::icon_plus_32;break;
 case Icon::ChevronRight:bmp=size==12?mm_assets::icon_chevron_right_12:size==16?mm_assets::icon_chevron_right_16:size==24?mm_assets::icon_chevron_right_24:mm_assets::icon_chevron_right_32;break;

 case Icon::Mountain:bmp=size==12?mm_assets::icon_mountain_12:size==16?mm_assets::icon_mountain_16:size==24?mm_assets::icon_mountain_24:mm_assets::icon_mountain_32;break;


 case Icon::Vibration:bmp=size==12?mm_assets::icon_vibration_12:size==16?mm_assets::icon_vibration_16:size==24?mm_assets::icon_vibration_24:mm_assets::icon_vibration_32;break;
 }if(id==Icon::Weather){drawWeatherGlyph(g,WeatherGlyph::PartlyCloudy,x,y,size,color);return;}
 if(id==Icon::GPS){
  // Native crosshair: clean 1-bit ink, also legible in the 12px status slot.
  int center=size/2,radius=std::max(2,size/3),arm=std::max(2,size/4);
  g.drawCircle(x+center,y+center,radius,color);
  g.fillCircle(x+center,y+center,std::max(1,size/12),color);
  g.drawFastHLine(x+1,y+center,arm,color);g.drawFastHLine(x+size-arm,y+center,arm,color);
  g.drawFastVLine(x+center,y+1,arm,color);g.drawFastVLine(x+center,y+size-arm,arm,color);return;
 }
 if(bmp)g.drawBitmap(x,y,bmp,size,size,color);
}
void Components::packIcon(PackIcon id,int x,int y,int size,uint16_t color){g.drawBitmap(x,y,(size==24?mm_assets::pack_icons_24:mm_assets::pack_icons_32)[int(id)],size==24?24:32,size==24?24:32,color);}
void Components::weatherIcon(int code,int x,int y,int size){drawWeatherGlyph(g,weatherGlyph(code),x,y,size,FG);}
void Components::card(Rect r,bool selected,int radius){if(selected)g.fillRoundRect(r.x,r.y,r.w,r.h,radius,FG);else g.drawRoundRect(r.x,r.y,r.w,r.h,radius,FG);}
void Components::slider(Rect r,int percent){percent=std::max(0,std::min(100,percent));card(r,false,4);int x=r.x+8,w=r.w-16,y=r.y+(r.h-6)/2,fill=w*percent/100;g.drawRect(x,y,w+1,6,FG);g.fillRect(x+1,y+1,fill,4,FG);g.fillCircle(x+fill,y+2,4,FG);}
// Four-bar link quality from SNR (SF8 decodes down to about -10 dB).
void Components::signalBars(int x,int bottom,float snr,uint16_t ink,int scale){int level=snr>=5?4:snr>=0?3:snr>=-5?2:snr>=-10?1:0;for(int i=0;i<4;i++){int h=(5+i*4)*scale,w=4*scale,bx=x+i*(w+scale);if(i<level)g.fillRect(bx,bottom-h,w,h,ink);else g.drawRect(bx,bottom-h,w,h,ink);}}
void Components::button(Rect r,const std::string &label,bool selected){card(r,selected);centered(r.x,r.y+(r.h-13)/2,r.w,label,Font::Bold,selected?BG:FG);}
void Components::iconButton(Rect r,Icon id,bool selected){if(selected)card(r,true);icon(id,r.x+(r.w-16)/2,r.y+(r.h-16)/2,16,selected?BG:FG);}
StatusLayout Components::statusLayout(const Status &s){
 StatusLayout layout;layout.percent=s.battery<0?"--%":std::to_string(std::min(100,s.battery))+"%";
 layout.percentWidth=width(layout.percent,Font::ChatBody);layout.percentX=W-StatusRightMargin-layout.percentWidth;
 layout.batteryX=layout.percentX-StatusBatteryGap-StatusBatteryW;layout.iconsRight=layout.batteryX-StatusIndicatorGap;
 return layout;
}
void Components::statusBar(const Status &s){
 g.fillRect(0,0,W,StatusH,BG);
 text(Margin,StatusTextY,s.clock,Font::ChatBody,FG,StatusClockW);
 text(StatusAntennaX,StatusAntennaY,s.antennaInternal?"INT":"EXT",Font::ChatMeta,FG,StatusAntennaW);
 if(!s.keyboard.empty())text(StatusAntennaX+StatusAntennaW+2,StatusAntennaY,s.keyboard,Font::ChatMeta,FG,22);// keyboard language (Alt + Space)
 const Icon indicators[]={Icon::Messages,Icon::GPS,Icon::Wifi,Icon::Bluetooth};
 const bool active[]={s.unread>0,s.gps,s.wifi,s.bt};
 int count=0;for(bool on:active)if(on)count++;
 auto layout=statusLayout(s);
 int first=layout.iconsRight-(count?(count-1)*StatusIconStep+IconStatus:0),visible=0;
 // Center the actual ink bounds, not each asset's transparent padding.
 GFXcanvas1 glyph(IconStatus,IconStatus);Components glyphComponents(glyph);
 for(int i=0;i<4;i++){
  if(!active[i])continue;
  glyph.fillScreen(BG);if(i==0){glyph.drawRect(0,2,12,8,FG);glyph.drawLine(0,2,5,6,FG);glyph.drawLine(11,2,6,6,FG);}else glyphComponents.icon(indicators[i],0,0,IconStatus);
  int left=IconStatus,right=-1,top=IconStatus,bottom=-1;
  for(int y=0;y<IconStatus;y++)for(int x=0;x<IconStatus;x++)if(glyph.getPixel(x,y)){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}
  if(right<left)continue;
  int dx=first+visible++*StatusIconStep+(IconStatus-(right-left+1))/2-left;
  int dy=StatusIconY+(IconStatus-(bottom-top+1))/2-top;
  for(int y=top;y<=bottom;y++)for(int x=left;x<=right;x++)if(glyph.getPixel(x,y))g.drawPixel(dx+x,dy+y,FG);
 }
 text(layout.percentX,StatusTextY,layout.percent,Font::ChatBody);
 if(s.charging){int x=layout.batteryX+1;g.fillTriangle(x+7,StatusIconY,x+1,StatusIconY+7,x+7,StatusIconY+7,FG);g.fillTriangle(x+5,StatusIconY+4,x+5,StatusIconY+12,x+11,StatusIconY+4,FG);}
 else {int x=layout.batteryX,y=StatusBatteryY;g.fillRect(x,y,12,StatusBatteryH,FG);g.fillRect(x+2,y+2,8,5,BG);g.fillRect(x+12,y+3,2,3,FG);int level=std::max(0,std::min(100,s.battery));int fill=(level*6+50)/100;if(fill)g.fillRect(x+3,y+3,fill,3,FG);}
 g.drawFastHLine(0,StatusH-1,W,FG);
}
void Components::pageHeader(const std::string &title,bool back,Icon right){if(back)iconButton(BackButton,Icon::Back);else iconButton(BackButton,Icon::Mesh);text(34,StatusH+8,title,Font::Title,FG,W-72);iconButton(ComposeButton,right);g.drawFastHLine(0,Top-1,W,FG);}
void Components::footer(const std::string &left,const std::string &right){g.fillRect(0,FooterY,W,FooterH,BG);g.drawFastHLine(0,FooterY,W,FG);text(Margin,FooterY+4,left,Font::Small,FG,W/2-12);auto label=fit(right,W/2-Margin-4,Font::Small);int ww=width(label,Font::Small);text(W-Margin-ww,FooterY+4,label,Font::Small);}
void Components::menuRow(Rect r,Icon id,const std::string &label,const std::string &detail,bool selected){card(r,selected);auto color=selected?BG:FG;icon(id,r.x+6,r.y+(r.h-24)/2,24,color);text(r.x+36,r.y+4,label,Font::ChatBody,color,r.w-56);text(r.x+36,r.y+20,detail,Font::ChatMeta,color,r.w-56);icon(Icon::ChevronRight,r.x+r.w-16,r.y+(r.h-12)/2,12,color);}
void Components::toggleRow(Rect r,Icon id,const std::string &label,const std::string &detail,bool on,const std::string &pill){
 card(r);icon(id,r.x+6,r.y+(r.h-24)/2,24);auto state=pill.empty()?std::string(on?"ON":"OFF"):pill;int pw=std::max(44,width(state,Font::Bold)+16),px=r.x+r.w-pw-6,py=r.y+(r.h-22)/2;
 text(r.x+36,r.y+5,label,Font::ChatBody,FG,px-r.x-40);text(r.x+36,r.y+22,detail,Font::ChatMeta,FG,px-r.x-40);
 if(on)g.fillRoundRect(px,py,pw,22,11,FG);else g.drawRoundRect(px,py,pw,22,11,FG);centered(px,py+5,pw,state,Font::Bold,on?BG:FG);}
void Components::sectionLabel(int y,const std::string &label,const std::string &right){text(Margin+2,y,label,Font::ChatMeta);if(!right.empty())text(W-Margin-2-width(right,Font::ChatMeta),y,right,Font::ChatMeta);}
}
