// Link viewer app (section 33): opened from a received chat message that contains a link (tap the underlined
// link, or message panel > LINK, or L in the panel). Pages show as text rows with their links at the end; a
// picture (or a photo page's og:image) shows fitted to the screen and can be zoomed (fit, 1:1, 2x of the decoded
// picture) and dragged. Back walks the pages opened from links in this session, then returns to the chat.
// Included by main.cpp (after services/link_viewer.h).
static std::vector<std::string> linkHistory;// pages to go back to (previous URL first-in-last-out), max 8
static std::vector<std::pair<std::string,int>> linkAllRows;static uint32_t linkRowsRev=0;
static int linkScroll=0,linkFocusRow=-1,linkZoom=0,linkDragRest=0;
static bool linkShowPage=false;// photo page: its text shown instead of the picture (PAGE)
static double linkCenterX=0.5,linkCenterY=0.5;
static std::string linkError;// shown when a link could not even be started (e.g. too long), or after the page was closed
static uint16_t *linkView=nullptr;
static struct {uint32_t revision=0;int zoom=-1;double cx=-1,cy=-1;} linkViewKey;// what linkView holds
static int linkMaxScroll(){return max(0,int(linkAllRows.size())-mm::LinkRows);}
static void linkFreeView(){if(linkView){heap_caps_free(linkView);linkView=nullptr;}linkViewKey.zoom=-1;}
static void linkReset(){linkShowPage=false;linkAllRows.clear();linkRowsRev=0;linkScroll=0;linkFocusRow=-1;linkZoom=0;linkDragRest=0;linkCenterX=linkCenterY=0.5;linkError="";linkFreeView();}
static bool linkPageView(const device::LinkMeta &m){return m.phase==device::LinkPhase::Page||(m.phase==device::LinkPhase::Image&&linkShowPage&&m.page);}
static std::string linkShortUrl(const std::string &url){return url.size()>48?url.substr(0,46)+"..":url;}
// Opens a URL; `push`: remember the page on screen so Back returns to it (only once the new link has started).
void linkGo(const std::string &url,bool push){
 std::string previous=device::linkMeta().url;linkReset();
 if(!device::linkOpen(url))linkError=url.size()>=500?"Link too long to open":"Not a web link";
 else if(push&&!previous.empty()){if(linkHistory.size()>=8)linkHistory.erase(linkHistory.begin());linkHistory.push_back(previous);}
 if(!(screen==Screen::Section&&section==mm::LinkSection))openSection(mm::LinkSection);else render();
}
void linkOpenFromChat(const std::string &text){std::string url=device::linkFind(text);if(url.empty()){showChatError("No link in this message",2500);return;}
 linkHistory.clear();messageActions=messagePath=false;linkGo(url,false);}
void linkLeft(){device::linkClose();linkReset();linkHistory.clear();linkError="Link closed";}// section left (Back to the chat, Home, Quick settings)
bool linkBack(){if(linkHistory.empty())return false;std::string url=linkHistory.back();linkHistory.pop_back();linkReset();device::linkOpen(url);render();return true;}

// Page rows: optional picture error / "view picture" rows, wrapped text, then a LINKS heading and one row per link.
static void linkBuildRows(const device::LinkState &st){
 linkAllRows.clear();mm::Components c(frame);int w=mm::W-2*mm::Margin-8;
 if(!st.pictureError.empty())linkAllRows.push_back({"Picture: "+st.pictureError,-1});
 if(!st.imageUrl.empty())linkAllRows.push_back({"View picture",-3});
 size_t start=0;while(start<=st.text.size()){size_t end=st.text.find('\n',start);if(end==std::string::npos)end=st.text.size();
  std::string para=st.text.substr(start,end-start);if(para.empty())linkAllRows.push_back({"",-1});else for(auto &line:c.lines(para,w,mm::Font::ChatBody))linkAllRows.push_back({line,-1});
  if(linkAllRows.size()>1500)break;start=end+1;}
 if(st.text.empty())linkAllRows.push_back({"(no readable text)",-1});
 if(!st.links.empty()){linkAllRows.push_back({"LINKS ("+std::to_string(st.links.size())+")",-2});for(int i=0;i<int(st.links.size());i++)linkAllRows.push_back({c.fit(st.links[i].text,w-4,mm::Font::ChatBody),i});}
 linkScroll=min(linkScroll,linkMaxScroll());
}
static bool linkSelectable(int row){return row>=0&&row<int(linkAllRows.size())&&linkAllRows[row].second!=-1&&linkAllRows[row].second!=-2;}
static void linkActivate(int row){
 if(!linkSelectable(row))return;int target=linkAllRows[row].second;auto m=device::linkMeta();
 if(target==-3&&m.phase==device::LinkPhase::Image){linkShowPage=false;render();return;}// the picture is already here
 if(target==-3&&!m.imageUrl.empty()){linkGo(m.imageUrl,true);return;}
 if(target>=0&&target<m.links){auto st=device::linkState();if(target<int(st.links.size()))linkGo(st.links[target].url,true);}}
// Picture: scale per zoom level (0 fit, 1 decoded 1:1, 2 decoded 2x); levels that would look the same are skipped.
static double linkScaleFor(int zoom,int w,int h){const auto &a=mm::LinkArea;double fit=min(double(a.w)/w,double(a.h)/h);if(fit>1)fit=min(fit,2.0);return zoom==0?fit:zoom==1?max(fit,1.0):max(fit,2.0);}
static void linkComposePicture(mm::State &v,uint32_t revision){
 const auto &a=mm::LinkArea;int iw=0,ih=0;const uint16_t *src=device::linkPicture(iw,ih);if(!src||iw<=0)return;
 if(!linkView)linkView=static_cast<uint16_t*>(heap_caps_malloc(size_t(a.w)*a.h*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(!linkView)return;
 if(linkViewKey.revision!=revision||linkViewKey.zoom!=linkZoom||linkViewKey.cx!=linkCenterX||linkViewKey.cy!=linkCenterY){
  linkViewKey.revision=revision;linkViewKey.zoom=linkZoom;linkViewKey.cx=linkCenterX;linkViewKey.cy=linkCenterY;
  double s=linkScaleFor(linkZoom,iw,ih);double vw=a.w/s,vh=a.h/s;// view size in picture pixels
  double left=linkCenterX*iw-vw/2,top=linkCenterY*ih-vh/2;if(vw<iw)left=constrain(left,0.0,double(iw)-vw);else left=(iw-vw)/2;if(vh<ih)top=constrain(top,0.0,double(ih)-vh);else top=(ih-vh)/2;
  uint16_t paper=mm::BG;for(int y=0;y<a.h;y++){int sy=int(floor(top+(y+0.5)/s));uint16_t *out=linkView+size_t(y)*a.w;
   if(sy<0||sy>=ih){for(int x=0;x<a.w;x++)out[x]=paper;continue;}const uint16_t *row=src+size_t(sy)*iw;
   for(int x=0;x<a.w;x++){int sx=int(floor(left+(x+0.5)/s));out[x]=sx<0||sx>=iw?paper:row[sx];}}}
 v.linkPixels=linkView;
}
static void linkPan(int dx,int dy){auto m=device::linkMeta();if(m.phase!=device::LinkPhase::Image||m.imageW<=0||linkZoom==0)return;double s=linkScaleFor(linkZoom,m.imageW,m.imageH);
 linkCenterX=constrain(linkCenterX+dx/s/m.imageW,0.0,1.0);linkCenterY=constrain(linkCenterY+dy/s/m.imageH,0.0,1.0);render();}
static void linkZoomStep(int delta){auto m=device::linkMeta();if(m.imageW<=0)return;double now=linkScaleFor(linkZoom,m.imageW,m.imageH);int z=linkZoom;
 do z+=delta;while(z>0&&z<2&&linkScaleFor(z,m.imageW,m.imageH)==now);
 z=constrain(z,0,2);if(linkScaleFor(z,m.imageW,m.imageH)==now)return;
 linkZoom=z;if(z==0)linkCenterX=linkCenterY=0.5;render();}
static void linkScrollBy(int rows){int next=constrain(linkScroll+rows,0,linkMaxScroll());if(next==linkScroll)return;linkScroll=next;if(linkFocusRow>=0&&(linkFocusRow<linkScroll||linkFocusRow>=linkScroll+mm::LinkRows))linkFocusRow=-1;render();}
// Focus walks the selectable rows (picture, links); the list scrolls to keep it visible.
static void linkMoveFocus(int dir){int row=linkFocusRow<0?(dir>0?linkScroll-1:linkScroll+mm::LinkRows):linkFocusRow;
 for(row+=dir;row>=0&&row<int(linkAllRows.size());row+=dir)if(linkSelectable(row)){linkFocusRow=row;if(row<linkScroll)linkScroll=row;else if(row>=linkScroll+mm::LinkRows)linkScroll=row-mm::LinkRows+1;render();return;}
 linkScrollBy(dir*(mm::LinkRows-1));}

static void linkFillRows(mm::State &v){
 for(int i=0;i<mm::LinkRows&&linkScroll+i<int(linkAllRows.size());i++)v.linkRows.push_back(linkAllRows[linkScroll+i]);
 v.linkFocus=linkFocusRow>=0?linkFocusRow-linkScroll:-1;v.linkScrollTop=linkScroll;v.linkScrollTotal=linkAllRows.size();}
void linkFill(mm::State &v){
 auto m=device::linkMeta();v.linkHost=m.host;
 if(m.phase==device::LinkPhase::Idle){v.linkMode=3;v.linkMessage=linkError.empty()?"Link closed":linkError;v.linkInfo="Open it again from the chat";return;}
 if(m.phase==device::LinkPhase::Loading){v.linkMode=0;v.linkMessage=m.message.empty()||m.message=="Loading..."?"Opening link":m.message;v.linkTitle=m.title;v.linkInfo=linkShortUrl(m.url);
  if(m.received){v.linkInfo=std::to_string(m.received/1024)+" KB"+(m.total?" of "+std::to_string(m.total/1024)+" KB":"");v.linkProgress=m.total?int(min<uint64_t>(100,uint64_t(m.received)*100/m.total)):-1;}
  if(!device::wifiConnected())v.linkInfo="Waiting for Wi-Fi";return;}
 if(m.phase==device::LinkPhase::Error){v.linkMode=3;v.linkMessage=m.message;v.linkInfo=linkShortUrl(m.url);return;}
 v.linkTitle=m.title;
 if(linkPageView(m)){// page text and links are copied only when the page changed (rows rebuilt), not every frame
  if(m.revision!=linkRowsRev){auto st=device::linkState();linkRowsRev=st.revision;linkBuildRows(st);}
  v.linkMode=1;if(m.phase==device::LinkPhase::Image||!m.imageUrl.empty())v.linkSwitch="PICTURE";linkFillRows(v);return;}
 v.linkMode=2;linkComposePicture(v,m.revision);v.linkZoomButtons=true;
 v.linkInfo=std::to_string(m.sourceW)+" x "+std::to_string(m.sourceH)+(linkZoom==0?"  FIT":linkZoom==1?"  1:1":"  2x");if(m.page||!linkHistory.empty())v.linkSwitch="PAGE";
}
// Keys: BACK / Backspace back, J K or W S scroll / move focus, ENTER opens the focused row, P picture <-> page,
// + - (I O) zoom, W A S D pan a zoomed picture, R reload.
bool linkKey(char c){
 auto m=device::linkMeta();
 if(c=='\b'||c==3){if(!linkBack())back();return true;}
 if(c=='r'&&!m.url.empty()){std::string url=m.url;linkReset();device::linkOpen(url);render();return true;}
 if(c=='p'){if(m.phase==device::LinkPhase::Image&&m.page){linkShowPage=!linkShowPage;render();}// photo page: picture <-> text, no reload
  else if(m.phase==device::LinkPhase::Page&&!m.imageUrl.empty())linkGo(m.imageUrl,true);else if(m.phase==device::LinkPhase::Image&&!linkHistory.empty())linkBack();return true;}
 bool pageView=linkPageView(m);
 if(m.phase==device::LinkPhase::Image&&!pageView){
  if(c=='+'||c=='='||c=='i'){linkZoomStep(1);return true;}if(c=='-'||c=='o'){linkZoomStep(-1);return true;}
  if(c=='w'||c=='k'){linkPan(0,-mm::LinkArea.h/3);return true;}if(c=='s'||c=='j'){linkPan(0,mm::LinkArea.h/3);return true;}
  if(c=='a'||c=='h'){linkPan(-mm::LinkArea.w/3,0);return true;}if(c=='d'||c=='l'){linkPan(mm::LinkArea.w/3,0);return true;}return true;}
 if(pageView){
  if(c=='j'||c=='s'){linkMoveFocus(1);return true;}if(c=='k'||c=='w'){linkMoveFocus(-1);return true;}
  if(c==' '){linkScrollBy(mm::LinkRows-1);return true;}if(c=='\n'){linkActivate(linkFocusRow);return true;}}
 return true;// the viewer swallows every other key
}
// Finger moves (touch pipeline Move/Drag/Swipe): scroll the page by whole rows (the remainder carries over between
// moves) or pan a zoomed picture.
// M9: OK on the element chosen with the D-pad presses it like a finger did on the T-Deck (main.cpp focusPress).
bool linkTap(int x,int y){
 if(mm::LinkBack.contains(x,y)||(y<mm::StatusH+mm::LinkBarH&&x<60)){if(!linkBack())back();return true;}
 auto m=device::linkMeta();
 if(mm::LinkSwitch.contains(x,y)){linkKey('p');return true;}
 bool pageView=linkPageView(m);
 if(m.phase==device::LinkPhase::Image&&!pageView){if(mm::LinkZoomIn.contains(x,y)){linkZoomStep(1);return true;}if(mm::LinkZoomOut.contains(x,y)){linkZoomStep(-1);return true;}
  if(mm::LinkArea.contains(x,y))linkPan(x-(mm::LinkArea.x+mm::LinkArea.w/2),y-(mm::LinkArea.y+mm::LinkArea.h/2));return true;}
 if(m.phase==device::LinkPhase::Error&&mm::LinkArea.contains(x,y)){linkKey('r');return true;}
 if(pageView)for(int i=0;i<mm::LinkRows;i++)if(mm::linkRowRect(i).contains(x,y)){int row=linkScroll+i;if(linkSelectable(row)){linkFocusRow=row;linkActivate(row);}return true;}
 return true;
}
