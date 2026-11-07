#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include "ui/components.h"
#include "ui/fast_canvas.h"
int main(){
 mm::FastCanvas16 frame(mm::W,mm::H);mm::Components c(frame);auto shot=[&]{return std::vector<uint16_t>(frame.getBuffer(),frame.getBuffer()+mm::W*mm::H);};
 for(auto font:{mm::Font::ChatBody,mm::Font::ChatMeta,mm::Font::LockText,mm::Font::LockClock}){
  frame.fillScreen(mm::BG);c.text(10,10,"°",font);int r=std::max(2,c.ascent(font)/5);
  assert(frame.getPixel(10+r,10)==mm::FG);assert(frame.getPixel(10+r,10+r)==mm::BG);// ring drawn in FG, centre hollow
  auto degree=shot();
  frame.fillScreen(mm::BG);c.text(10,10,"⁰",font);assert(shot()==degree);
  assert(c.width("12°",font)==c.width("12⁰",font));
  assert(c.width("18℃",font)==c.width("18°C",font));
  auto lines=c.lines("Plateau Rosa 12⁰ windy today.",80,font);for(auto &line:lines)assert(c.width(line,font)<=80||line=="°"||c.width("R",font)>80);
 }
 {int r=std::max(2,c.ascent(mm::Font::ChatBody)/5);frame.fillScreen(mm::FG);c.text(10,10,"°",mm::Font::ChatBody,mm::BG);assert(frame.getPixel(10+r,10)==mm::BG);assert(frame.getPixel(10+r,10+r)==mm::FG);}
 std::cout<<"PASS: degree/superscript-zero hollow glyph, shared width/wrapping, Celsius normalization, BG-on-FG\n";
}
