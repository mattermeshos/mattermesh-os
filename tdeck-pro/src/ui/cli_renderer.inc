// CLI app (section 30), part of the OS. Included by renderer.cpp.
namespace {
// Arrow centered on (cx,cy): 0 up, 1 down, 2 left, 3 right; 12 px shaft, 2 px stroke.
void toolArrow(Adafruit_GFX &g,int cx,int cy,int dir,uint16_t ink){
 int dx=dir==3?1:dir==2?-1:0,dy=dir==1?1:dir==0?-1:0;
 for(int t=-6;t<=6;t++){int x=cx+dx*t,y=cy+dy*t;g.fillRect(x-(dy?1:0),y-(dx?1:0),dy?2:1,dx?2:1,ink);}
 int tx=cx+dx*6,ty=cy+dy*6;for(int k=0;k<5;k++){int ax=tx-dx*k,ay=ty-dy*k;if(dx){g.drawFastVLine(ax,ay-k,2*k+1,ink);}else g.drawFastHLine(ax-k,ay,2*k+1,ink);}
}
}
void Renderer::cliApp(const State &s){
 // CLI: scrolling output, prompt pinned above the toolbar.
 shell(s,"CLI",true,Icon::Menu);g.fillRect(ComposeButton.x,StatusH,ComposeButton.w,HeaderH-1,BG);
 std::vector<std::pair<std::string,int>> wrapped;for(int i=0;i<int(s.cliLines.size());i++)for(auto &part:c.lines(s.cliLines[i],CliOutput.w-6,Font::ChatMeta))wrapped.push_back({part,i});
 int visible=(CliOutput.h-CliLineH)/CliLineH,maxScroll=std::max(0,int(wrapped.size())-visible),scroll=std::min(s.cliScroll,maxScroll),first=std::max(0,int(wrapped.size())-visible-scroll);
 for(int i=0;i<visible&&first+i<int(wrapped.size());i++)c.text(CliOutput.x,CliOutput.y+i*CliLineH,wrapped[first+i].first,Font::ChatMeta,FG,CliOutput.w-6);
 if(maxScroll){int height=visible*CliLineH,thumb=std::max(10,height*visible/int(wrapped.size()));g.drawFastVLine(W-3,CliOutput.y,height,FG);g.fillRect(W-4,CliOutput.y+(height-thumb)*(maxScroll-scroll)/maxScroll,3,thumb,FG);}
 int promptY=CliOutput.y+visible*CliLineH;
 if(s.cliStreaming)c.text(CliOutput.x,promptY,"Logging packets / STOP to end",Font::ChatMeta,FG,CliOutput.w);
 else{
  // Keep the cursor visible: drop leading characters until prompt + text before the cursor fits.
  std::string before=s.cliPrompt+s.cliInput.substr(0,std::min<size_t>(s.cliCursor,s.cliInput.size())),after=s.cliInput.substr(std::min<size_t>(s.cliCursor,s.cliInput.size()));
  int room=CliOutput.w-6;std::string head=c.width(before,Font::ChatMeta)>room?c.inputTail(before,room,Font::ChatMeta):before;
  int cx=CliOutput.x+c.width(head,Font::ChatMeta);c.text(CliOutput.x,promptY,head,Font::ChatMeta);c.text(cx+2,promptY,after,Font::ChatMeta,FG,std::max(0,CliOutput.x+CliOutput.w-cx-2));
  g.fillRect(cx,promptY,2,CliLineH-1,FG);
 }
 c.card(CliBar,false,8);
 for(int i=0;i<CliToolCount;i++){
  Rect r=cliToolRect(i);bool on=i==7&&s.cliStreaming;if(on)c.card(r,true);uint16_t ink=on?BG:FG;int cx=r.x+r.w/2,cy=r.y+r.h/2;
  switch(i){
  case 0:toolArrow(g,cx-2,cy,3,ink);g.fillRect(cx+5,cy-6,2,13,ink);break; // TAB: arrow to a bar
  case 1:toolArrow(g,cx,cy,0,ink);break;case 2:toolArrow(g,cx,cy,1,ink);break;case 3:toolArrow(g,cx,cy,2,ink);break;case 4:toolArrow(g,cx,cy,3,ink);break;
  case 5:{bool can=s.cliCanPaste;g.drawRoundRect(cx-6,cy-6,12,14,2,ink);g.fillRect(cx-3,cy-8,6,4,ink);if(can){g.drawFastHLine(cx-3,cy,6,ink);g.drawFastHLine(cx-3,cy+3,6,ink);}break;} // PASTE: lines when the clipboard holds text
  case 6:g.drawRect(cx-7,cy-3,14,10,ink);g.drawFastHLine(cx-5,cy-5,10,ink);g.drawFastHLine(cx-3,cy-7,6,ink);break; // CLEAR screen
  case 7:g.drawCircle(cx,cy,7,ink);g.drawCircle(cx,cy,6,ink);g.fillRect(cx-2,cy-2,5,5,ink);break; // STOP
  }
 }
 Rect divider=cliToolRect(6);g.drawFastVLine(divider.x-CliToolGap/2-1,CliBar.y+6,CliBar.h-12,FG);
}
int Renderer::cliLineAt(const State &s,int y){
 if(y<CliOutput.y)return -1;
 std::vector<int> owners;for(int i=0;i<int(s.cliLines.size());i++)for(size_t k=0;k<c.lines(s.cliLines[i],CliOutput.w-6,Font::ChatMeta).size();k++)owners.push_back(i);
 int visible=(CliOutput.h-CliLineH)/CliLineH,maxScroll=std::max(0,int(owners.size())-visible),scroll=std::min(s.cliScroll,maxScroll),first=std::max(0,int(owners.size())-visible-scroll),row=(y-CliOutput.y)/CliLineH;
 if(row>=visible||first+row>=int(owners.size()))return -1;
 return owners[first+row];
}
int Renderer::cliMaxScroll(const State &s){int total=0;for(auto &line:s.cliLines)total+=c.lines(line,CliOutput.w-6,Font::ChatMeta).size();return std::max(0,total-(CliOutput.h-CliLineH)/CliLineH);}
