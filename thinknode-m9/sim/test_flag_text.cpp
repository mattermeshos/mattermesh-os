#include <cassert>
#include <iostream>
#include <vector>
#include "ui/components.h"
// Flag emoji: a regional-indicator pair renders as one compact badge, never as two oversized letters, and is never split.
int main(){GFXcanvas1 canvas(mm::W,mm::H);mm::Components c(canvas);const std::string it="\xF0\x9F\x87\xAE\xF0\x9F\x87\xB9",name="Aria "+it+" ok";
 int flag=c.width(it,mm::Font::ChatBody);assert(flag<2*14&&flag>8);assert(c.width(name,mm::Font::ChatBody)==c.width("Aria ",mm::Font::ChatBody)+flag+c.width(" ok",mm::Font::ChatBody));
 canvas.fillScreen(0);c.text(4,10,it,mm::Font::ChatBody);int inked=0;for(int y=10;y<25;y++)for(int x=4;x<4+flag;x++)inked+=canvas.getPixel(x,y);assert(inked>20&&inked<flag*15/2); // box + two small letters, not a filled block
 for(int y=10;y<25;y++)for(int x=4+flag;x<60;x++)assert(!canvas.getPixel(x,y)); // nothing drawn past the badge
 auto cut=c.fit(name,c.width("Aria ",mm::Font::ChatBody)+flag/2+c.width("..",mm::Font::ChatBody),mm::Font::ChatBody);assert(cut.find("\xF0\x9F\x87")==std::string::npos); // too narrow for the pair: whole pair dropped
 auto lines=c.lines("abc "+it+"def",40,mm::Font::ChatBody);for(auto &l:lines){size_t n=0;for(size_t i=0;i+2<l.size();i++)if(l.compare(i,3,"\xF0\x9F\x87")==0)n++;assert(n==0||n==2);}
 std::cout<<"PASS: flag emoji badge (compact width, inked box, pair never split by fit/lines)"<<std::endl;return 0;}
