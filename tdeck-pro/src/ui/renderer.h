#pragma once
#include "model.h"
#include "artwork.h"
namespace mm {
struct MessageHit {Rect area,sender;Message message;int index;};
class Renderer {
 Adafruit_GFX &g;Components c;uint8_t *bits=nullptr;// the canvas pixels, when drawing on a GFXcanvas1 (fast paths)
 std::vector<std::pair<Rect,int>> bubbleHits;std::vector<std::pair<Rect,int>> senderHits;
public:
 int chatComposerTop=ComposerY;// top of the composer card painted by chat(): ComposerY unless a draft expanded it
private:
 void shell(const State &s,const std::string &title,bool back=true,Icon right=Icon::Menu);
 Rect overlayPanel();// clears below the header and draws the card shared by the chat overlays
 void mapView(const State &s); // MapArea: tiles, route, pins, own position, status, scale
 void heroClock(const std::string &text,int y);
 void wallpaperThumb(int index,int x,int y); // lock-screen clock glyphs, centered
public:
 explicit Renderer(Adafruit_GFX &canvas):g(canvas),c(canvas){}
 explicit Renderer(GFXcanvas1 &canvas):g(canvas),c(canvas),bits(canvas.getBuffer()){}
 void lock(const State &s);
 void quickReply(const State &s);
 void home(const State &s);
 void launcher(const State &s);
 void messages(const State &s);
 void chat(const State &s);
 void channelOptions(const State &s);
 void channelKey(const State &s);
 void contactPicker(const State &s);
 void cliApp(const State &s);
 int cliLineAt(const State &s,int y); // raw CLI line under y, -1 if none
 int cliMaxScroll(const State &s);
 const std::vector<std::pair<Rect,int>> &messageBounds()const{return bubbleHits;}
 bool senderHit(int x,int y)const{for(auto &r:senderHits)if(r.first.contains(x,y))return true;return false;}
 std::vector<MessageHit> snapshotMessages(const State &s)const{std::vector<MessageHit> out;for(auto &hit:bubbleHits){if(hit.second<0||hit.second>=int(s.messages.size()))continue;Rect sender={0,0,0,0};for(auto &name:senderHits)if(name.second==hit.second){sender=name.first;break;}out.push_back({hit.first,sender,s.messages[hit.second],hit.second});}return out;}
 void form(const State &s);
 void settings(const State &s);
 void section(const State &s);
 void quickSettings(const State &s);
 void quickSlider(int row,bool sun,const std::string &label,int step,bool off);
 void quickTile(int index,const std::string &label,bool on,int glyph);
 void draw(const State &s);
 void welcome(const State &s);// first-boot setup
 void holdBanner(const std::string &label);// one-line notice card under the status bar
 void unlockTapBanner(int taps);
 void messageBanner(const std::string &sender,const std::string &preview);
};
}
