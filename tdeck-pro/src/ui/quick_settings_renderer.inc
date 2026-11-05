// Quick settings panel (status-bar tap). Included by renderer.cpp.
// Geometry lives in theme.h so main.cpp hit-testing uses the same rects.
namespace {
// Vector glyphs missing from the bitmap set; 2 px strokes for e-ink.
void quickSun(Adafruit_GFX &g,int x,int y,int size,uint16_t ink){
 int cx=x+size/2,cy=y+size/2,r=size/5;g.fillCircle(cx,cy,r,ink);
 for(int i=0;i<8;i++){float a=i*3.14159265f/4;int x0=cx+int(std::cos(a)*(r+3)),y0=cy+int(std::sin(a)*(r+3)),x1=cx+int(std::cos(a)*(size/2-1)),y1=cy+int(std::sin(a)*(size/2-1));
  g.drawLine(x0,y0,x1,y1,ink);g.drawLine(x0+1,y0,x1+1,y1,ink);}
}
void quickSpeaker(Adafruit_GFX &g,int x,int y,int size,bool muted,uint16_t ink){
 int cy=y+size/2,body=size/4;g.fillRect(x+1,cy-body/2,body,body+1,ink);g.fillTriangle(x+1+body,cy-body/2,x+1+body,cy+body/2,x+size/2,y+2,ink);g.fillTriangle(x+1+body,cy+body/2,x+size/2,y+2,x+size/2,y+size-2,ink);
 if(muted){int a=x+size*5/8,b=x+size-2;g.drawLine(a,cy-size/5,b,cy+size/5,ink);g.drawLine(a+1,cy-size/5,b+1,cy+size/5,ink);g.drawLine(a,cy+size/5,b,cy-size/5,ink);g.drawLine(a+1,cy+size/5,b+1,cy-size/5,ink);return;}
 for(int r=size/4;r<=size/2-1;r+=size/6){g.drawCircleHelper(x+size/2-2,cy,r,2,ink);g.drawCircleHelper(x+size/2-2,cy,r,4,ink);g.drawCircleHelper(x+size/2-1,cy,r,2,ink);g.drawCircleHelper(x+size/2-1,cy,r,4,ink);}
}
// Dashed outline: an inactive slot reserved for a future tile.
void quickPlaceholder(Adafruit_GFX &g,Rect r){
 for(int x=r.x+6;x<r.x+r.w-6;x+=8){g.drawFastHLine(x,r.y,4,FG);g.drawFastHLine(x,r.y+r.h-1,4,FG);}
 for(int y=r.y+6;y<r.y+r.h-6;y+=8){g.drawFastVLine(r.x,y,4,FG);g.drawFastVLine(r.x+r.w-1,y,4,FG);}
}
}
void Renderer::quickSlider(int row,bool sun,const std::string &label,int step,bool off){
 int y=quickSliderLabelY(row);
 if(sun)quickSun(g,Margin,y-1,15,FG);else quickSpeaker(g,Margin,y-1,15,off,FG);
 c.text(Margin+22,y,label,Font::Bold,FG,W/2);
 std::string value=off?"Off":std::to_string(step)+" / "+std::to_string(QuickSteps);
 c.text(W-Margin-c.width(value,Font::Bold),y,value,Font::Bold);
 c.slider(quickSliderRect(row),step*100/QuickSteps);
}
void Renderer::quickTile(int index,const std::string &label,bool on,int glyph){
 Rect r=quickTileRect(index);c.card(r,on,8);uint16_t ink=on?BG:FG;int ix=r.x+(r.w-24)/2,iy=r.y+4;
 if(glyph==0)c.icon(Icon::Wifi,ix,iy,24,ink);else if(glyph==1)c.icon(Icon::Bluetooth,ix,iy,24,ink);else if(glyph==2)c.icon(Icon::GPS,ix,iy,24,ink);
 else if(glyph==3)quickSpeaker(g,ix,iy,24,!on,ink);else c.icon(Icon::Power,ix,iy,24,ink);
 c.centered(r.x,r.y+r.h-17,r.w,label,Font::Bold,ink);
}
void Renderer::quickSettings(const State &s){
 shell(s,"Quick settings");g.fillRect(W-32,StatusH,32,HeaderH-1,BG);
 // Brightness = front light level; Off when the display light is switched off.
 quickSlider(0,true,"Brightness",(s.displayLight*QuickSteps+127)/255,!s.displayLightEnabled);
 quickSlider(1,false,"Volume",(s.audioVolume*QuickSteps+50)/100,!s.audioNotifications||s.audioVolume==0);
 quickTile(QuickWifi,"Wi-Fi",s.wifiEnabled,0);
 quickTile(QuickBluetooth,"Bluetooth",s.bleEnabled,1);
 quickTile(QuickGps,"GPS",s.gpsEnabled&&!s.gpsTemporary,2);
 quickTile(QuickSound,"Sound",s.audioNotifications,3);
 quickTile(QuickPowerOff,"Power Off",false,4);
 quickPlaceholder(g,quickTileRect(QuickReserved));
 if(s.shutdownConfirm){
  auto box=QuickConfirmBox;g.fillRoundRect(box.x-2,box.y-2,box.w+4,box.h+4,10,BG);g.drawRoundRect(box.x,box.y,box.w,box.h,8,FG);g.drawRoundRect(box.x+1,box.y+1,box.w-2,box.h-2,7,FG);
  c.centered(box.x,box.y+14,box.w,"Turn off T-Deck?",Font::ChatTitle);
  c.centered(box.x,box.y+44,box.w,"The device will power off.",Font::ChatMeta);
  if(s.usbPower||!s.powerSourceKnown)c.centered(box.x,box.y+62,box.w,s.usbPower?"Unplug USB first.":"Power controller unavailable",Font::ChatMeta);
  c.button(QuickConfirmCancel,"CANCEL");c.button(QuickConfirmPowerOff,"POWER OFF",true);
 }
}
