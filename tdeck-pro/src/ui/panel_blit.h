#pragma once
#include "refresh.h"
namespace mm {
// Same current/previous-buffer sequence as GxEPD2_BW::nextPage, without
// rasterizing the already-rendered 1-bit canvas into a second GFX buffer.
template<class Panel> void paintFramebuffer(Panel &panel,const uint8_t *pixels,const RefreshPlan &plan){
 if(!plan.changed)return;
 if(plan.full){panel.writeImageForFullRefresh(pixels,0,0,W,H,true,false,false);panel.refresh(false);panel.writeImageAgain(pixels,0,0,W,H,true,false,false);panel.powerOff();}
 else{auto r=plan.area;panel.writeImagePart(pixels,r.x,r.y,W,H,r.x,r.y,r.w,r.h,true,false,false);if(plan.typing)panel.refreshTyping(r.x,r.y,r.w,r.h);else panel.refresh(r.x,r.y,r.w,r.h);panel.writeImagePartAgain(pixels,r.x,r.y,W,H,r.x,r.y,r.w,r.h,true,false,false);panel.powerOff();} // panel booster off between updates too
}
// Clean-up after typing: the OTP partial only drives pixels whose old/new data differ, so the controller's previous
// buffer is first loaded with the inverse of the area; every pixel is then rewritten at full contrast, removing the
// lighter ink and any residue of the short waveform.
template<class Panel> void repaintArea(Panel &panel,const uint8_t *pixels,const Rect &r){
 panel.writeImagePartAgain(pixels,r.x,r.y,W,H,r.x,r.y,r.w,r.h,false,false,false);
 panel.writeImagePart(pixels,r.x,r.y,W,H,r.x,r.y,r.w,r.h,true,false,false);panel.refresh(r.x,r.y,r.w,r.h);
 panel.writeImagePartAgain(pixels,r.x,r.y,W,H,r.x,r.y,r.w,r.h,true,false,false);panel.powerOff();
}
}
