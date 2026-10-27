#pragma once
#include <Adafruit_GFX.h>
#include <string>
#include <vector>
#include "theme.h"
#include "fast_canvas.h"
namespace mm {
enum class Font { Small,Body,Bold,Title,Brand,Clock,Tiny,HomeClock,LockText,ChatBody,ChatMeta,ChatTitle,MenuClock,MenuLabel,LockClock,NavLabel,ChatText };// ChatText: chat bubbles and composer (larger on the T-Deck Plus LCD, = ChatBody on e-paper)
enum class Icon { Messages,Mesh,Weather,Radio,Files,Terminal,Notes,Tools,Settings,GPS,Info,Battery,Signal,Wifi,Bluetooth,Contacts,Keyboard,Flashlight,Clock,Lock,Power,Back,Search,Compose,Menu,Plus,ChevronRight,Mountain };// order = tools/make_aa_icons.py ICONS
// MatterMesh OS icon pack: sprites mm_aa_icons::PackFirst+id (tools/make_aa_icons.py).
enum class PackIcon { Chat,Contacts,Settings,Apps,Map,Files,Weather,Signal,Tools,List,Radio,Keyboard };
struct Status { std::string clock="--:--";int battery=-1,unread=0;bool mesh=false,wifi=false,bt=false,gps=false,acquiringTime=false,charging=false;std::string keyboard;/* keyboard language code, only with 2+ languages on */ };
struct StatusLayout {std::string percent;int percentX,percentWidth,batteryX,iconsRight;};
// Touchables: what the components draw that a finger can press (cards, buttons, icon buttons, tabs, the Home cards
// and tabs), recorded during a render; render() shows a press ring on the one under a resting finger.
void touchablesClear();void touchableAdd(Rect r,int radius);
void touchableBlock(Rect r);// something painted over a registered element (a blanked header button): no ring there
bool touchableAt(int x,int y,Rect &r,int &radius);// the last drawn (topmost) one under x,y; a blocker or a panel over half the screen stops the search
class Components {
 Adafruit_GFX &g;
 FastCanvas16 *fast=nullptr;// the LCD frame: anti-aliased icons and colour emoji; null for 1-bit scratch canvases
public:
 explicit Components(FastCanvas16 &canvas):g(canvas),fast(&canvas){}
public:
 explicit Components(Adafruit_GFX &canvas):g(canvas){}
 // Rounded fill: row by row on the LCD frame (FastCanvas16::fillRoundRectRows, same pixels), Adafruit_GFX elsewhere.
 void fillRound(int x,int y,int w,int h,int r,uint16_t color){
  if(fast){fast->fillRoundRectRows(x,y,w,h,r,color);return;}
  g.fillRoundRect(x,y,w,h,r,color);}
 static const GFXfont *font(Font f);
 static int ascent(Font f),lineHeight(Font f);
 void emoji(uint32_t code,int x,int y,int size=14,uint16_t color=FG);
 void badge(int cx,int cy,int r,int count,uint16_t fill,uint16_t ink);
 std::vector<std::string> clampLines(const std::string &s,int w,Font f,int maxRows,const char *ellipsis);// wrapped, at most maxRows, the last one ending in `ellipsis`
 void tab(Rect r,const std::string &label,bool selected,bool open);// page tab: accent chip (`open` is kept for the callers, the chip has no opening)// unread count in a filled circle ("99+" above 99)
 void inlineEmoji(int px);// 0 = default size for the font; chat bubbles (Plus) pass 20 or 30
 bool hasEmoji(const std::string &s,Font f);int emojiOnlyCount(const std::string &s,Font f);// 1..3 emoji and nothing else, 0 otherwise
 // Flag emoji (two regional indicators) as one compact badge: rounded box with the country code in the Tiny font.
 void flag(int x,int y,Font f,uint32_t first,uint32_t second,uint16_t color=FG);int flagAdvance(Font f,bool pair=true);
 std::string inputTail(const std::string &s,int width,Font f=Font::ChatBody);
 static std::string display(const std::string &s);// the text as drawn (mentions cleaned, accents folded): caret offsets refer to it
 int unitAdvance(uint32_t code,Font f);
 int width(const std::string &s,Font f=Font::Body);int rawWidth(const std::string &value,Font f);
 std::string fit(const std::string &s,int w,Font f=Font::Body);
 // Already displayText()-ed strings: fit with ".." and draw without another conversion pass.
 std::string fitDisplayed(const std::string &value,int w,Font f);void drawDisplayed(int x,int y,const std::string &value,Font f,uint16_t color);
 void text(int x,int y,const std::string &s,Font f=Font::Body,uint16_t color=FG,int maxWidth=0);
 void centered(int x,int y,int w,const std::string &s,Font f=Font::Body,uint16_t color=FG);
 std::vector<std::string> lines(const std::string &s,int w,Font f=Font::Body);
 void icon(Icon id,int x,int y,int size=IconRow,uint16_t color=FG);
 void packIcon(PackIcon id,int x,int y,int size,uint16_t color=FG); // user icon pack, 24 or 32 px
 void weatherIcon(int code,int x,int y,int size=32);
 void card(Rect r,bool selected=false,int radius=Radius);
 void slider(Rect r,int percent);
 void signalBars(int x,int bottom,float snr,uint16_t ink=FG,int scale=1); // four bars from SNR, 19*scale px wide
 void button(Rect r,const std::string &label,bool selected=false);
 void pressRing(Rect r,int radius,uint16_t ink);// 2 px ring on a pressed element
 void iconButton(Rect r,Icon id,bool selected=false);
 void statusBar(const Status &status);
 StatusLayout statusLayout(const Status &status);
 void pageHeader(const std::string &title,bool back=true,Icon right=Icon::Menu);
 void footer(const std::string &left,const std::string &right="OFFLINE");
 void menuRow(Rect r,Icon id,const std::string &label,const std::string &detail,bool selected=false);
 // Settings pages: switch card (icon, name, state line, ON/OFF pill) and uppercase group label.
 void toggleRow(Rect r,Icon id,const std::string &label,const std::string &detail,bool on,const std::string &pill="");
 void sectionLabel(int y,const std::string &label,const std::string &right="");
 // Colour LCD tabbed pages (Contacts, LoRa, RPT): accent chip for the open tab, outlined panel (drawn first) it opens into.
 void tabChip(Rect r,const std::string &label,bool selected);
 void tabPanel(Rect r);
 void image(int x,int y,const uint16_t *pixels,int w,int h);// opaque RGB565 (panel byte order) picture, e.g. the colour map view
};
}
