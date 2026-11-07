// M9 D-pad focus. The M9 has no touch panel: the arrows walk every element a finger could press on the T-Deck page
// (buttons, tabs, pills, switches, list rows - recorded while the frame is drawn, components.cpp focusTarget), the
// chosen one gets a ring, and OK presses it with the page's own tap handler (m9Tap, the Plus touch code). Lists that
// keep their own selection (Home tiles and rows, the chat list, Settings, Apps) are tagged: the arrows set that
// selection, the page paints it, and OK runs the page's own ENTER.
static bool focusOn=false;static mm::Rect focusRect{};static int focusKind=0,focusIndex=0;
static std::vector<mm::FocusTarget> shownTargets;// the elements of the frame on screen
static int focusPage=-1;// screen, section and open dialog of the frame the focus belongs to
static bool focusWantOn=false;static mm::Rect focusWant{};
// Back on a list from one of its items: the next frame rings the element nearest r (the item just left), not the top.
void focusPrefer(const mm::Rect &r){focusWant=r;focusWantOn=true;}

static int focusPageId(){
 int dialogs=emojiPicker|messagePath<<1;// the other dialogs are row pages (m9_pages.inc), outside the focus
 return int(screen)*100000+section*1000+dialogs;}
// Arrows on a text field: the chat composer keeps them (up / down scroll the chat, left / right move the caret);
// other fields keep left / right for the caret and give up / down to the focus.
static bool focusTyping(int dx,int dy){auto field=editableField();if(!field)return false;return field==&input||dx!=0;}

// The page's own selection, as a tagged element: where the arrows start from on a page opened without a focus.
static int taggedSelection(){
 for(int i=0;i<int(shownTargets.size());i++){auto &t=shownTargets[i];
  switch(t.kind){
   case mm::FocusHomeTile:if(homeRow<0&&t.index==(homeSelection<0?0:homeSelection))return i;break;
   case mm::FocusHomeRow:if(homeRow==t.index)return i;break;
   case mm::FocusLauncher:if(t.index==(launcherSelection<0?0:launcherSelection))return i;break;
   case mm::FocusSettingsRow:if(t.index==settingsFocus)return i;break;
   case mm::FocusChatsItem:{auto list=conversationList();int at=chatsSearchRow?0:int(list.size())+1;for(unsigned k=0;k<list.size();k++)if(!chatsSearchRow&&list[k].id==selected)at=k+1;if(t.index==at)return i;break;}
   default:break;}}
 return -1;}
// A tagged element chosen: the page's selection follows it (the page paints the highlight).
static void taggedSelect(const mm::FocusTarget &t){
 switch(t.kind){
  case mm::FocusHomeTile:homeRow=-1;homeSelection=t.index;break;
  case mm::FocusHomeRow:homeRow=t.index;break;
  case mm::FocusLauncher:launcherSelection=t.index;break;
  case mm::FocusSettingsRow:settingsFocus=t.index;settingsScroll=mm::settingsReveal(settingsFocus,settingsScroll);break;
  case mm::FocusChatsItem:{auto list=conversationList();chatsSearchRow=t.index==0;selected=t.index>=1&&t.index<=int(list.size())?list[t.index-1].id:chatCount;break;}
  default:break;}}

// The element in direction (dx, dy) from r: the nearest centre that lies that way, sideways offset weighed 3x so the
// same row / column wins over a diagonal; left / right only reach elements sharing the row (no jump to a far corner).
static int focusNeighbour(const mm::Rect &r,int dx,int dy){
 int cx=r.x+r.w/2,cy=r.y+r.h/2,best=-1;long bestScore=0;
 for(int i=0;i<int(shownTargets.size());i++){auto &t=shownTargets[i].r;if(t.x==r.x&&t.y==r.y&&t.w==r.w&&t.h==r.h)continue;
  int tx=t.x+t.w/2,ty=t.y+t.h/2;long along,side;
  if(dy){along=(ty-cy)*dy;if(along<=2)continue;side=std::max(0,std::max(t.x-(r.x+r.w),r.x-(t.x+t.w)));}
  else{along=(tx-cx)*dx;if(along<=2||std::min(t.y+t.h,r.y+r.h)-std::max(t.y,r.y)<=0)continue;side=std::abs(ty-cy);}// sideways: only the same row
  long score=along+3*side;if(best<0||score<bestScore){best=i;bestScore=score;}}
 return best;}
static void focusSet(int i){auto &t=shownTargets[i];focusOn=true;focusRect=t.r;focusKind=t.kind;focusIndex=t.index;focusPage=focusPageId();if(t.kind)taggedSelect(t);}

// After a frame is drawn: keep the focus on the same element (or the nearest one, the list may have moved) and ring it.
void focusAfterRender(){
 shownTargets=mm::focusTargets();
 if(focusOn&&focusPage!=focusPageId())focusOn=false;// another page or dialog: start again from its own selection
 if(focusWantOn){focusWantOn=false;focusOn=true;focusRect=focusWant;focusKind=focusIndex=0;focusPage=focusPageId();}
 if(!focusOn||deviceLocked)return;
 int best=-1;long bestD=0;for(int i=0;i<int(shownTargets.size());i++){auto &t=shownTargets[i];
  if(focusKind&&t.kind==focusKind&&t.index==focusIndex){best=i;break;}
  long dx=(t.r.x+t.r.w/2)-(focusRect.x+focusRect.w/2),dy=(t.r.y+t.r.h/2)-(focusRect.y+focusRect.h/2),d=dx*dx+dy*dy;if(best<0||d<bestD){best=i;bestD=d;}}
 if(best<0){focusOn=false;return;}
 auto &t=shownTargets[best];focusRect=t.r;focusKind=t.kind;focusIndex=t.index;
 if(!t.kind){mm::Components c(frame);// a 3 px ring in the text colour: seen on light cards and on accent buttons alike
  c.pressRing({t.r.x-3,t.r.y-3,t.r.w+6,t.r.h+6},10,mm::FG);frame.drawRoundRect(t.r.x-1,t.r.y-1,t.r.w+2,t.r.h+2,8,mm::FG);}// tagged rows: the page paints its selection
}

// Pages whose arrows already do the job (the row pages are not here: rowArrow takes them first): the Note, the CLI,
// Maps (the arrows pan the map), the weather Radar (left / right view, up / down zoom), Donate (two tabs).
static bool focusOwnArrows(){return (screen==Screen::Chat&&messagePath&&!messageRouteView)/* Read the whole message / View path: up / down scroll the text */||(screen==Screen::Section&&(section==mm::NoteSection||section==mm::CliSection||section==9||section==10||section==mm::SupportSection));}
// Arrows: true when the focus took them.
bool focusMove(int dx,int dy){
 if(deviceLocked||screen==Screen::Welcome||rowPageOn()||focusOwnArrows()||focusTyping(dx,dy)||shownTargets.empty())return false;
 dx=dx>0?1:dx<0?-1:0;dy=dy>0?1:dy<0?-1:0;if(dx&&dy)dy=0;
 if(!focusOn||focusPage!=focusPageId()){// first arrow on this page: start on its selection, or on the first element
  int at=taggedSelection();
  if(at<0){at=0;for(int i=1;i<int(shownTargets.size());i++){auto &a=shownTargets[i].r,&b=shownTargets[at].r;if(a.y<b.y||(a.y==b.y&&a.x<b.x))at=i;}focusSet(at);render();return true;}
  focusSet(at);}
 int next=focusNeighbour(focusRect,dx,dy);
 if(next>=0){focusSet(next);render();return true;}
 // Nothing that way: a list scrolls (the page's own up / down), then the focus takes the element now there.
 if(dy){mm::Rect was=focusRect;int kind=focusKind;
  if(kind==mm::FocusLauncher)key(dy<0?'w':'s');/* the app grid: the tile above / below or nothing (no walk along the row) */
  else if(kind)key(dy<0?'k':'j');else{auto field=editableField();if(!field)key(dy<0?'w':'s');}
  if(kind){int at=taggedSelection();if(at>=0)focusSet(at);}else{int n=focusNeighbour(was,0,dy);if(n>=0)focusSet(n);}
  render();}
 return true;}

// OK: true when it pressed the focused element (a tagged row leaves OK to the page, its selection already follows).
bool focusPress(){
 if(!focusOn||focusPage!=focusPageId()||deviceLocked||screen==Screen::Welcome||focusKind)return false;
 m9Tap(focusRect.x+focusRect.w/2,focusRect.y+focusRect.h/2);return true;}
