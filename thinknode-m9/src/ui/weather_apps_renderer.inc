// Weather app tabs (Plus): NOW (the forecast page), STORMS, RADAR. State comes from mm::State.storm* / radar*.
static const uint16_t StormWarning=0xC0E3;// orange (230,120,0) in panel byte order: thunderstorm marks, readable on every theme
void Renderer::weatherTabs(const State &s){
 const char *labels[]={"NOW","STORMS","RADAR"};
 for(int i=0;i<3;i++){Rect r=weatherTabRect(i);bool on=s.weatherTab==i;g.fillRoundRect(r.x,r.y,r.w,r.h,10,on?Accent:Surface);c.centered(r.x,r.y+4,r.w,labels[i],Font::Bold,on?OnAccent:FG);
  if(i==1&&!s.stormRows.empty())g.fillCircle(r.x+r.w-6,r.y+5,4,StormWarning);}// a pending alert marks the STORMS tab
}
void Renderer::stormsBody(const State &s){
 // Status line: what the next 24 h hold (or what the service waits for), when it was checked.
 int subW=c.width(s.stormSub,Font::ChatMeta);bool alerting=!s.stormRows.empty();
 c.text(Margin+2,Top+7,s.stormHead,Font::Bold,alerting?StormWarning:s.stormReady?Accent:FG,W-2*Margin-subW-10);if(subW)c.text(W-Margin-subW,Top+8,s.stormSub,Font::ChatMeta,Muted);
 int count=s.stormRows.size();
 for(int row=0;row<StormRows&&s.stormOffset+row<count;row++){auto &e=s.stormRows[s.stormOffset+row];Rect r=stormRowRect(row);c.card(r,false,8);
  Rect badge={r.x+6,r.y+(r.h-26)/2,26,26};g.fillRoundRect(badge.x,badge.y,badge.w,badge.h,7,e.thunder?StormWarning:Mist);c.icon(Icon::Weather,badge.x+5,badge.y+5,16,e.thunder?OnAccent:FG);
  int ww=c.width(e.when,Font::Bold);c.text(r.x+r.w-8-ww,r.y+4,e.when,Font::Bold,FG);
  c.text(badge.x+badge.w+8,r.y+4,e.title,Font::Bold,e.thunder?StormWarning:FG,r.w-badge.w-ww-30);
  c.text(badge.x+badge.w+8,r.y+21,e.detail,Font::ChatMeta,Muted,r.w-badge.w-20);}
 if(!count){Rect r={Margin,StormRowY+4,W-2*Margin,StormRows*StormRowH-14};c.card(r,false,10);
  c.icon(Icon::Weather,W/2-16,r.y+12,32,s.stormReady?Accent:Muted);
  c.centered(Margin,r.y+52,W-2*Margin,s.stormReady?"No storms expected":"No forecast yet",Font::Bold,FG);
  c.centered(Margin,r.y+72,W-2*Margin,"Thunderstorms, gusts from 60 km/h, heavy rain",Font::ChatMeta,Muted);
  c.centered(Margin,r.y+87,W-2*Margin,"Checked every 30 min while Wi-Fi is on",Font::ChatMeta,Muted);}
 if(count>StormRows){int track=StormRows*StormRowH-3,thumb=std::max(12,track*StormRows/count),max=count-StormRows;g.fillRoundRect(W-4,StormRowY+std::min(s.stormOffset,max)*(track-thumb)/max,3,thumb,1,Muted);}
 c.pill(StormRefresh,s.stormBusy?"CHECKING":"REFRESH",true);c.pill(StormAlertsButton,s.stormAlerts?"ALERTS ON":"ALERTS OFF",false);
}
void Renderer::radarBody(const State &s){
 // The map (RGB565, MapArea from the status bar down) shown below the header, the radar already blended in.
 const int y0=Top,h=H-Top;
 if(s.mapPixels)c.image(0,y0,s.mapPixels+size_t(y0-MapArea.y)*MapArea.w,W,h);else g.fillRect(0,y0,W,h,Mist);
 auto label=[&](int x,int y,const std::string &text,Font f){int w=c.width(text,f)+8;g.fillRoundRect(x,y,w,c.lineHeight(f)+2,5,Surface);c.text(x+4,y+1,text,f,FG);};
 if(s.mapSelfX>=0&&s.mapSelfY>=y0+6){int x=s.mapSelfX,y=s.mapSelfY;g.fillCircle(x,y,7,BG);g.drawCircle(x,y,6,FG);g.fillCircle(x,y,3,Accent);}
 label(Margin,y0+6,s.radarTime.empty()?"Radar":s.radarTime,Font::Bold);
 if(!s.radarMessage.empty())label(Margin,y0+26,s.radarMessage,Font::ChatMeta);
 // Legend: light -> heavy rain in RainViewer's "universal blue" scheme.
 {auto panel=[](int r,int g,int b){uint16_t v=uint16_t(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3));return uint16_t(v<<8|v>>8);};
  const uint16_t steps[]={panel(136,221,238),panel(0,153,204),panel(0,85,170),panel(255,238,0),panel(255,136,0),panel(230,0,0)};int x=W-Margin-6*12-4,y=y0+6;g.fillRoundRect(x-4,y,6*12+12,22,5,Surface);for(int i=0;i<6;i++)g.fillRect(x+i*12,y+4,12,8,steps[i]);c.text(x,y+11,"light",Font::Tiny,Muted);c.text(x+6*12-c.width("heavy",Font::Tiny),y+11,"heavy",Font::Tiny,Muted);}
 if(s.mapScalePx>0){int x=Margin+4,y=H-10;g.fillRoundRect(x-4,y-14,std::max(s.mapScalePx,c.width(s.mapScaleLabel,Font::Tiny))+10,20,4,Surface);g.drawFastHLine(x,y,s.mapScalePx,FG);g.drawFastHLine(x,y+1,s.mapScalePx,FG);c.text(x,y-12,s.mapScaleLabel,Font::Tiny,FG);}
 std::string credit="RainViewer / (c) OpenStreetMap";int cw=c.width(credit,Font::Tiny)+6;g.fillRect(W-Margin-76-cw,H-14,cw,12,Surface);c.text(W-Margin-73-cw,H-13,credit,Font::Tiny,Muted);
 auto round=[&](Rect r,bool plus,bool enabled){g.fillRoundRect(r.x,r.y,r.w,r.h,r.h/2,Surface);g.drawRoundRect(r.x,r.y,r.w,r.h,r.h/2,Line);auto ink=enabled?FG:Muted;int cx=r.x+r.w/2,cy=r.y+r.h/2;g.fillRect(cx-6,cy-1,12,2,ink);if(plus)g.fillRect(cx-1,cy-6,2,12,ink);};
 round(RadarZoomOut,false,s.radarZoom>6);round(RadarZoomIn,true,s.radarZoom<7);
}
