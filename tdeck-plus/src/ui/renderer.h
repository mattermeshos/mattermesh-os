#pragma once
#include "model.h"
#include "artwork.h"
namespace mm {
// Text of the accent bar, the Keyboard languages how-to line and accent help (pure, tested in sim/test_keyboard_pages.cpp).
std::string accentBarText(const State &s);std::string keyboardHowto(const State &s);std::string keyboardHelp(const State &s,int lang);std::string upperAscii(std::string text);
struct MessageHit {Rect area,sender;Message message;int index;std::vector<Rect> links;};// links: the underlined link, one rect per line
class Renderer {
 Adafruit_GFX &g;Components c;
 Rect dialogPanel(bool clear=true);
 std::vector<std::pair<Rect,int>> bubbleHits;std::vector<std::pair<Rect,int>> senderHits,linkHits;
public:
 // Plus continuous chat scroll: chat() normalises State::chatOffset/chatPixel; the caller adds chatShiftOut to its
 // offset (older messages) and keeps chatPixelOut.
 int chatShiftOut=0,chatPixelOut=0;
 int chatComposerTop=ComposerY;// top of the composer card painted by chat(): ComposerY unless a draft expanded it
private:
 void shell(const State &s,const std::string &title,bool back=true,Icon right=Icon::Menu);
 void hideHeaderAction();// blanks the header's right button (pages without one) and keeps the press ring off it
 void mapView(const State &s); // MapArea: tiles, route, pins, own position, status, scale
 void linkView(const State &s); // link viewer: bar, page rows or picture (section 33)
 void homeHeader(const State &s,int dy,bool widgetOnly);// Home clock, date and the box beside it, moved down by dy (Beside clock preview)
 void homeWidget(const State &s,int id,int x,int top,int w);// one Home widget in the box right of the clock
 void homeChats(const State &s,int dy);// Home chat cards, moved down by dy
 void mainWidget(const State &s,int id,Rect r);// one Main area widget in r
public:
 explicit Renderer(Adafruit_GFX &canvas):g(canvas),c(canvas){}
 explicit Renderer(FastCanvas16 &canvas):g(canvas),c(canvas){}
 void lock(const State &s);
 void quickReply(const State &s);
 void home(const State &s);
 void launcher(const State &s);
 void messages(const State &s);
 void chat(const State &s);
 void channelOptions(const State &s);
 void channelKey(const State &s);
 void contactPicker(const State &s);
 void weatherTabs(const State &s);void stormsBody(const State &s);void radarBody(const State &s); // Weather tabs (Plus)
 void cliApp(const State &s);
 int cliLineAt(const State &s,int y); // raw CLI line under y, -1 if none
 int cliMaxScroll(const State &s);
 const std::vector<std::pair<Rect,int>> &messageBounds()const{return bubbleHits;}
 bool senderHit(int x,int y)const{for(auto &r:senderHits)if(r.first.contains(x,y))return true;return false;}
 std::vector<MessageHit> snapshotMessages(const State &s)const{std::vector<MessageHit> out;for(auto &hit:bubbleHits){if(hit.second<0||hit.second>=int(s.messages.size()))continue;Rect sender={0,0,0,0};for(auto &name:senderHits)if(name.second==hit.second){sender=name.first;break;}MessageHit m{hit.first,sender,s.messages[hit.second],hit.second,{}};for(auto &l:linkHits)if(l.second==hit.second)m.links.push_back(l.first);out.push_back(m);}return out;}
 void form(const State &s);
 void settings(const State &s);
 void section(const State &s);
 void quickSettings(const State &s);
 void quickSlider(int row,bool sun,const std::string &label,int step,bool off);
 void quickTile(int index,const std::string &label,bool on,bool pressed=false);
 void draw(const State &s);
 void welcome(const State &s);// first-boot setup (Plus)
 void holdBanner(const std::string &label,int progress);
 void unlockTapBanner(int taps);
 void quickCountdownBanner(int dots);// lock screen: ENTER held, the quick reply opens when both dots are full
 void emojiPopup(const State &s);// emoji picker panel above the composer (chat) or over the quick reply
 void notificationBanner(Rect r,int clearTo,Icon icon,const std::string &title,const std::string &tag,const std::string &body);
};
}
