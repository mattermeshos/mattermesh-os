#pragma once
#include <Adafruit_GFX.h>
#include <string>
#include <vector>
#include "theme.h"
namespace mm {
// HomeClock and LockText are no longer drawn (their faces left the build); the values stay for the ascent/lineHeight tables.
enum class Font { Small,Body,Bold,Title,Brand,Clock,Tiny,HomeClock,LockText,ChatBody,ChatMeta,ChatTitle,MenuClock,MenuLabel,LockClock,NavLabel };
enum class Icon { Messages,Mesh,Weather,Radio,Files,Terminal,Notes,Tools,Settings,GPS,Info,Battery,Signal,Wifi,Bluetooth,Contacts,Keyboard,Clock,Lock,Power,Back,Search,Menu,Plus,ChevronRight,Mountain,Vibration };
// MatterMesh OS e-ink icon pack (tools/import_icon_pack.py); order = mm_assets::pack_icons_*.
enum class PackIcon { Chat,Contacts,Settings,Apps,Map,Files,Weather,Signal,Tools,List,Radio,Keyboard };
struct Status { std::string clock="--:--";int battery=-1,unread=0;bool mesh=false,wifi=false,bt=false,gps=false,acquiringTime=false,charging=false,antennaInternal=true;std::string keyboard;/* language code, only with 2+ keyboard languages */ };
struct StatusLayout {std::string percent;int percentX,percentWidth,batteryX,iconsRight;};
class Components {
 Adafruit_GFX &g;
public:
 explicit Components(Adafruit_GFX &canvas):g(canvas){}
 static const GFXfont *font(Font f);
 static int ascent(Font f),lineHeight(Font f);
 void emoji(uint32_t code,int x,int y,int size=14,uint16_t color=FG);
 // Flag emoji (two regional indicators) as one compact badge: rounded box with the country code in the Tiny font.
 void flag(int x,int y,Font f,uint32_t first,uint32_t second,uint16_t color=FG);int flagAdvance(Font f,bool pair=true);
 std::string inputTail(const std::string &s,int width,Font f=Font::ChatBody);
 static std::string display(const std::string &s);// the text as drawn (mentions cleaned, accents folded): caret offsets refer to it
 int unitAdvance(uint32_t code,Font f);
 int width(const std::string &s,Font f=Font::Body);int rawWidth(const std::string &value,Font f);
 std::string fit(const std::string &s,int w,Font f=Font::Body);std::string fitDisplayed(const std::string &value,int w,Font f);
 void drawDisplayed(int x,int y,const std::string &value,Font f,uint16_t color);
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
 void iconButton(Rect r,Icon id,bool selected=false);
 void statusBar(const Status &status);
 StatusLayout statusLayout(const Status &status);
 void pageHeader(const std::string &title,bool back=true,Icon right=Icon::Menu);
 void footer(const std::string &left,const std::string &right="OFFLINE");
 void menuRow(Rect r,Icon id,const std::string &label,const std::string &detail,bool selected=false);
 // Settings pages: switch card (icon, name, state line, ON/OFF pill) and uppercase group label.
 void toggleRow(Rect r,Icon id,const std::string &label,const std::string &detail,bool on,const std::string &pill="");
 void sectionLabel(int y,const std::string &label,const std::string &right="");
};
}
