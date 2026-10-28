#include "artwork.h"
#include "build_info.h"
#include "os_logo_bitmap.h"
namespace mm {
void splashScreen(Adafruit_GFX &g,int progress,const std::string &stage){
 g.fillScreen(BG);Components c(g);
 g.drawBitmap((W-mm_logo::size112)/2,34,mm_logo::logo112,mm_logo::size112,mm_logo::size112,FG);// OS logo (tools/make_os_logo.py)
 c.centered(Margin,160,W-2*Margin,"MATTERMESH OS",Font::Brand);
 c.centered(Margin,189,W-2*Margin,"T-DECK MAX",Font::Bold);
 c.centered(Margin,211,W-2*Margin,"Offline capable.",Font::Body);
 c.centered(Margin,225,W-2*Margin,"Built for the field.",Font::Body);
 Rect r={24,H-54,W-48,10};g.drawRoundRect(r.x,r.y,r.w,r.h,3,FG);
 int fill=(r.w-4)*progress/100;if(fill>0)g.fillRoundRect(r.x+2,r.y+2,fill,r.h-4,2,FG);
 c.centered(Margin,H-37,W-2*Margin,stage,Font::Small);
 c.centered(Margin,H-15,W-2*Margin,MM_RELEASE_LABEL,Font::Small);
}
}
