// M9: what a finger on (x, y) did on the T-Deck Plus page (its touch code as of Plus 0.4.0.13), reached by OK on the
// element chosen with the D-pad (m9_focus.inc). Only the pages not drawn as rows (m9_pages.inc) still need it: the
// link viewer, the chat Path page, the emoji picker and a message's path.
void m9Tap(int x,int y){
 if(x<0||x>=mm::W||y<0||y>=mm::H||deviceLocked)return;device::activity();
 Serial.printf("FOCUS_PRESS x=%d y=%d screen=%d\n",x,y,int(screen));
 if(screen==Screen::Section){
  if(dmPathTap(x,y))return;
  if(section==mm::LinkSection){linkTap(x,y);return;}
  return;}
 if(screen!=Screen::Chat)return;
 if(emojiPicker){for(int t=0;t<mm::EmojiTabs;t++)if(mm::emojiTabRect(t).contains(x,y)){emojiTab=t;emojiFocus=0;render();return;}int count=emojiItems().size();for(int i=0;i<count&&i<mm::EmojiPageSize;i++)if(mm::emojiChoiceRect(i).contains(x,y)){insertEmoji(i);return;}return;}
 if(messagePath&&!messageRouteView&&y>=mm::Bottom-40){if(x<mm::Margin+70)scrollMessagePath(-5);else if(x<mm::Margin+134)scrollMessagePath(5);else messagePath=false;render();}
}
