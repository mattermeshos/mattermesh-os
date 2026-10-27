// Quick settings panel (status-bar tap). Included by renderer.cpp.
// Geometry lives in theme.h so main.cpp hit-testing uses the same rects.
namespace {
// Vector glyphs missing from the bitmap set; 2 px strokes.
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
}
void Renderer::quickSlider(int row,bool sun,const std::string &label,int step,bool off){
 int y=quickSliderLabelY(row);
 if(sun)quickSun(g,Margin,y-1,15,FG);else quickSpeaker(g,Margin,y-1,15,off,FG);
 c.text(Margin+22,y,label,Font::Bold,FG,W/2);
 std::string value=off?"Off":std::to_string(step)+" / "+std::to_string(QuickSteps);
 c.text(W-Margin-c.width(value,Font::Bold),y,value,Font::Bold);
 c.slider(quickSliderRect(row),step*100/QuickSteps);
}
void Renderer::quickTile(int index,const std::string &label,bool on,bool pressed){
 Rect r=quickTileRect(index);c.card(r,on,8);uint16_t ink=on?BG:FG;
 // Pressed (finger resting on it): a 2 px ring and the icon 1 px lower.
 if(pressed){c.pressRing(r,8,on?FG:Accent);r.y++;}int ix=r.x+(r.w-24)/2,iy=r.y+4;
 if(index==QuickWifi)c.icon(Icon::Wifi,ix,iy,24,ink);else if(index==QuickBluetooth)c.icon(Icon::Bluetooth,ix,iy,24,ink);else if(index==QuickGps)c.icon(Icon::GPS,ix,iy,24,ink);
 else quickSpeaker(g,ix,iy,24,!on,ink);
 c.centered(r.x,r.y+r.h-(15),r.w,label,Font::Bold,ink);
}
void Renderer::quickSettings(const State &s){
 shell(s,"Quick settings");hideHeaderAction();
 // Brightness = front light level; Off when the display light is switched off.
 quickSlider(0,true,"Brightness",(s.displayLight*QuickSteps+127)/255,!s.displayLightEnabled);
 quickSlider(1,false,"Volume",(s.audioVolume*QuickSteps+50)/100,!s.audioNotifications||s.audioVolume==0);
 auto press=[&](int tile){return s.quickPressed==tile;};
 quickTile(QuickWifi,"Wi-Fi",s.wifiEnabled,press(QuickWifi));
 quickTile(QuickBluetooth,"Bluetooth",s.bleEnabled,press(QuickBluetooth));
 quickTile(QuickGps,"GPS",s.gpsEnabled&&!s.gpsTemporary,press(QuickGps));
 quickTile(QuickSound,"Sound",s.audioNotifications,press(QuickSound));
}
