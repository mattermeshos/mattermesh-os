// Chat options > Path (dm_path_layout.h). Included by renderer.cpp.
void Renderer::dmPathPage(const State &s){
 shell(s,"Path: "+s.dmPathName);hideHeaderAction();
 if(!s.dmPathEditing){
  {Rect r=DmPathCard;c.card(r,false,8);touchableBlock(r);
   c.text(r.x+8,r.y+7,s.dmPathRoute,Font::Bold,FG,r.w-16);c.text(r.x+8,r.y+31,s.dmPathOrigin,Font::ChatMeta,Muted,r.w-16);}
  const char *labels[]={"RESET","FLOOD ONLY","SET PATH"};
  for(int i=0;i<3;i++)c.button(dmPathButton(i),labels[i],s.dmPathFocus==i||(i==1&&s.dmPathMode==1));
  int y=DmPathHintY;
  for(auto line:{std::string(s.notice.empty()?"A learned path with no ACK: one retry by flood.":s.notice),
                 "Path bytes: "+std::to_string(s.dmPathHashBytes)+" (Settings > LoRa)."}){c.text(Margin,y,line,Font::ChatMeta,Muted,W-2*Margin);y+=16;}
  return;}
 {Rect r=DmPathDraft;c.card(r,true,6);std::string path=s.dmPathDraft.empty()&&s.dmPathInput.empty()?std::string("direct (0 hops)"):s.dmPathDraft;
  if(!s.dmPathInput.empty()||!s.dmPathDraft.empty())path+=(s.dmPathDraft.empty()?"":" > ")+s.dmPathInput+"_";
  std::string line="Path: "+path;while(line.size()>8&&c.width(line,Font::Bold)>r.w-16){line.erase(0,1);while(!line.empty()&&(uint8_t(line[0])&0xC0)==0x80)line.erase(0,1);}/* the end (typing) stays visible */
  if(line.compare(0,6,"Path: "))line=".."+line;c.text(r.x+8,r.y+7,line,Font::Bold,OnAccent,r.w-16);}
 if(s.dmPathRepeaters.empty())c.text(Margin,DmPathRowY+8,"No repeater heard yet: type the hex.",Font::ChatMeta,Muted,W-2*Margin);
 for(int i=0;i<DmPathRows&&i<int(s.dmPathRepeaters.size());i++){Rect r=dmPathRow(i);c.card(r,false,6);/* picked by touch: no focus */
  auto &row=s.dmPathRepeaters[i];c.text(r.x+8,r.y+6,row.name,Font::Bold,FG,r.w-80);c.text(r.x+r.w-8-c.width(row.prefix,Font::ChatMeta),r.y+8,row.prefix,Font::ChatMeta,Muted);}
 c.text(Margin,DmPathEditHintY,s.notice.empty()?"Tap a repeater or type "+std::to_string(s.dmPathHashBytes*2)+" hex per hop.":s.notice,Font::ChatMeta,Muted,W-2*Margin);
 const char *actions[]={"UNDO","DIRECT","CANCEL","SAVE"};for(int i=0;i<4;i++)c.button(dmPathAction(i),actions[i],i==3);
}
