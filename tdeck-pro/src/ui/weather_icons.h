#pragma once
#include <Adafruit_GFX.h>
namespace mm {
// MatterMesh Weather: editable vector primitives, 64px master, pure 1-bit ink.
enum class WeatherGlyph { Clear,PartlyCloudy,Cloudy,Fog,Drizzle,Rain,Snow,Storm,Unknown };
inline WeatherGlyph weatherGlyph(int code){
 if(code==0)return WeatherGlyph::Clear;if(code==1||code==2)return WeatherGlyph::PartlyCloudy;
 if(code==3)return WeatherGlyph::Cloudy;if(code==45||code==48)return WeatherGlyph::Fog;
 if(code>=51&&code<=57)return WeatherGlyph::Drizzle;
 if((code>=61&&code<=67)||(code>=80&&code<=82))return WeatherGlyph::Rain;
 if((code>=71&&code<=77)||code==85||code==86)return WeatherGlyph::Snow;
 if(code>=95&&code<=99)return WeatherGlyph::Storm;return WeatherGlyph::Unknown;
}
inline void drawWeatherGlyph(Adafruit_GFX &target,WeatherGlyph kind,int x,int y,int size,uint16_t ink=1){
 GFXcanvas1 tile(64,64);tile.fillScreen(0);
 auto line=[&](int a,int b,int c,int d){tile.drawLine(a,b,c,d,1);tile.drawLine(a+1,b,c+1,d,1);tile.drawLine(a,b+1,c,d+1,1);};
 auto sun=[&](int cx,int cy,int radius){tile.fillCircle(cx,cy,radius,1);tile.fillCircle(cx,cy,radius-3,0);int r=radius+5,e=radius+11;line(cx,cy-r,cx,cy-e);line(cx,cy+r,cx,cy+e);line(cx-r,cy,cx-e,cy);line(cx+r,cy,cx+e,cy);int q=r*7/10,t=e*7/10;for(int dx:{-1,1})for(int dy:{-1,1})line(cx+dx*q,cy+dy*q,cx+dx*t,cy+dy*t);};
 auto cloud=[&](int shift){
  GFXcanvas1 mask(64,64);mask.fillScreen(0);mask.fillCircle(18,31+shift,10,1);mask.fillCircle(31,25+shift,13,1);mask.fillCircle(45,33+shift,10,1);mask.fillRect(17,31+shift,29,13,1);
  for(int py=0;py<64;py++)for(int px=0;px<64;px++)if(mask.getPixel(px,py)){
   bool edge=false;for(int delta=-2;delta<=2;delta++)if(!mask.getPixel(px+delta,py)||!mask.getPixel(px,py+delta))edge=true;
   tile.drawPixel(px,py,edge?1:0);
  }
 };
 if(kind==WeatherGlyph::Clear)sun(31,31,12);
 else if(kind==WeatherGlyph::Unknown){cloud(0);line(27,21,35,21);line(35,21,35,28);line(35,28,31,32);tile.fillCircle(31,37,2,1);}
 else {
  if(kind==WeatherGlyph::PartlyCloudy)sun(21,20,8);
  cloud(kind==WeatherGlyph::PartlyCloudy?5:0);
  if(kind==WeatherGlyph::Fog){line(9,49,54,49);line(15,56,48,56);}
  if(kind==WeatherGlyph::Drizzle)for(int px:{18,31,44}){tile.fillCircle(px,50,2,1);tile.fillCircle(px-3,58,2,1);}
  if(kind==WeatherGlyph::Rain)for(int px:{18,31,44})line(px,49,px-4,58);
  if(kind==WeatherGlyph::Snow)for(int px:{19,43}){line(px,48,px,59);line(px-5,51,px+5,57);line(px-5,57,px+5,51);}
  if(kind==WeatherGlyph::Storm){tile.fillTriangle(32,42,23,54,35,54,1);tile.fillTriangle(29,52,39,52,27,63,1);}
 }
 for(int py=0;py<size;py++)for(int px=0;px<size;px++)if(tile.getPixel(px*64/size,py*64/size))target.drawPixel(x+px,y+py,ink);
}
}
