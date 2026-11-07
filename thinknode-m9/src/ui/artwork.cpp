#include "artwork.h"
#include "build_info.h"
#include "os_logo_bitmap.h"
#include <cmath>
#include "fast_canvas.h"
#include "splash_background.h"
#include "packed.h"
namespace mm {
void splashScreen(Adafruit_GFX &g,int progress,const std::string &stage){
 Components c(g);
 {// T-Deck Plus (the user's mock-up): a night valley between snowy peaks, the logo as a dark badge with a cyan rim and
  // glow and an ivory mountain, light-blue titles, a rounded cyan progress bar. Fixed colours: the splash runs
  // before the theme is loaded.
  auto &canvas=static_cast<FastCanvas16&>(g);/* the FastCanvas16 path copies rows */
  if(const uint16_t *photo=unpackPhoto(mm_splash_bg::packed,sizeof(mm_splash_bg::packed),mm_splash_bg::width,mm_splash_bg::height))Components(canvas).image(0,0,photo,mm_splash_bg::width,mm_splash_bg::height);else g.fillScreen(0);
  const int S=mm_logo::size112,lx=(W-S)/2,ly=18;
  // Badge of the 112 px logo: rounded square 2..62 of a 64 box, radius 14 (tools/make_os_logo.py); signed distance.
  const float half=S*30.f/64,radius=S*14.f/64,cx=lx+S/2.f,cy=ly+S/2.f;
  auto badge=[&](float x,float y){float qx=std::fabs(x-cx)-(half-radius),qy=std::fabs(y-cy)-(half-radius);
   float ox=std::max(qx,0.f),oy=std::max(qy,0.f);return std::sqrt(ox*ox+oy*oy)+std::min(std::max(qx,qy),0.f)-radius;};
  const uint16_t glow=rgb(80,190,255),rim=rgb(126,214,252),dark=rgb(8,14,28),ivory=rgb(246,243,234);
  const int G=14;
  for(int y=ly-G;y<ly+S+G;y++)for(int x=lx-G;x<lx+S+G;x++){if(x<0||y<0||x>=W||y>=H)continue;float d=badge(x+.5f,y+.5f);
   if(d>0&&d<G){float t=1-d/G;canvas.tint(x,y,1,1,glow,int(10*t*t+.5f));}
   else if(d<=0){int bx=x-lx,by=y-ly;bool ink=bx>=0&&by>=0&&bx<S&&by<S&&(mm_logo::logo112[by*(S/8)+bx/8]&(0x80>>(bx&7)));
    g.drawPixel(x,y,d>-1.6f?rim:ink?dark:ivory);}}
  c.centered(Margin,136,W-2*Margin,"MATTERMESH OS",Font::Brand,rgb(214,228,244));
  c.centered(Margin,164,W-2*Margin,"THINKNODE M9",Font::Bold,rgb(126,196,236));
  Rect r={24,H-56,W-48,12};g.fillRoundRect(r.x,r.y,r.w,r.h,6,rgb(8,16,32));g.drawRoundRect(r.x,r.y,r.w,r.h,6,rgb(150,214,240));
  int fill=(r.w-6)*progress/100;if(fill>3)g.fillRoundRect(r.x+3,r.y+3,fill,r.h-6,3,rgb(110,214,242));
  c.centered(Margin,H-37,W-2*Margin,stage,Font::Small,rgb(168,196,226));
  c.centered(Margin,H-15,W-2*Margin,MM_RELEASE_LABEL " / By Kinta",Font::Small,rgb(200,210,224));
  return;}
 g.fillScreen(BG);
 {// T-Deck Plus: logo, name and model, then the same progress footer
  g.drawBitmap((W-mm_logo::size112)/2,18,mm_logo::logo112,mm_logo::size112,mm_logo::size112,FG);
  c.centered(Margin,136,W-2*Margin,"MATTERMESH OS",Font::Brand);
  c.centered(Margin,164,W-2*Margin,"THINKNODE M9",Font::Bold);
 }
 Rect r={24,H-54,W-48,10};g.drawRoundRect(r.x,r.y,r.w,r.h,3,FG);
 int fill=(r.w-4)*progress/100;if(fill>0)g.fillRoundRect(r.x+2,r.y+2,fill,r.h-4,2,FG);
 c.centered(Margin,H-37,W-2*Margin,stage,Font::Small);
 c.centered(Margin,H-15,W-2*Margin,(MM_RELEASE_LABEL " / By Kinta"),Font::Small);
}
}
