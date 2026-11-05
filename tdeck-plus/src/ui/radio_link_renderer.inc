// Settings > Companion link (41) and Companion settings (42).
// Included by renderer.cpp.
#include "radio_link_layout.h"
void Renderer::radioLinkPages(const State &s){
 if(s.page==Page::WebApp){
  shell(s,"MatterMesh Web");hideHeaderAction();
  c.toggleRow(WebAppToggle,Icon::Desktop,"MatterMesh Web",s.webOn?"Serving your chats on Wi-Fi":"Off",s.webOn);
  {Rect r=WebAppCard;c.card(r,false,8);
   if(!s.webOn){c.text(r.x+10,r.y+10,"Turn it on, then open the address",Font::Bold,FG,r.w-20);c.text(r.x+10,r.y+30,"shown here in a browser on the same",Font::ChatMeta,Muted,r.w-20);c.text(r.x+10,r.y+46,"Wi-Fi (computer, tablet, phone).",Font::ChatMeta,Muted,r.w-20);}
   else if(s.webAddress.empty()){c.text(r.x+10,r.y+10,"No Wi-Fi connection",Font::Bold,FG,r.w-20);c.text(r.x+10,r.y+30,"Settings > Wi-Fi: join a network first.",Font::ChatMeta,Muted,r.w-20);}
   else{c.text(r.x+10,r.y+6,"OPEN IN A BROWSER",Font::ChatMeta,Muted,r.w-20);c.text(r.x+10,r.y+21,s.webAddress,Font::Bold,Accent,r.w-20);
    c.text(r.x+10,r.y+44,s.webLinked?"CONNECTED":"CODE",Font::ChatMeta,Muted,r.w-20);
    if(s.webLinked)c.text(r.x+10,r.y+58,"Browser "+s.webPeer,Font::Bold,FG,r.w-20);else c.text(r.x+10,r.y+56,s.webCode,Font::Title,FG,r.w-20);}}
  c.button(WebAppClose,s.webLinked?"CLOSE SESSION":"NEW CODE",s.webFocus==1&&s.webOn);
  c.text(Margin,WebAppHintY,"One browser at a time, sending via this radio.",Font::ChatMeta,Muted,W-2*Margin);
  return;}
 if(s.page==Page::RadioLink){
  shell(s,"Companion link");hideHeaderAction();
  {Rect r=RadioLinkStatus;c.card(r,false,8);touchableBlock(r);c.icon(Icon::Antenna,r.x+8,r.y+(r.h-16)/2,16,s.rlReady?Accent:Muted);
   c.text(r.x+32,r.y+4,s.rlSaved?s.rlSavedName:"No companion paired",Font::Bold,FG,r.w-40);c.text(r.x+32,r.y+21,s.rlStatus,Font::ChatMeta,s.rlFailed?Accent:Muted,r.w-40);}
  if(s.rlSaved){
   c.toggleRow(RadioLinkToggle,Icon::Radio,"Use companion radio",s.rlOn?"T-Deck radio off":"T-Deck radio on",s.rlOn);
   const char *labels[]={"SETTINGS",s.rlOn?"DISCONNECT":"CONNECT","FORGET"};/* the button is the switch */for(int i=0;i<3;i++)c.button(radioLinkAction(i),labels[i],s.rlFocus==1+i);
   int y=RadioLinkInfoY;if(s.rlReady){c.text(Margin,y,s.rlRadio,Font::ChatMeta,Muted,W-2*Margin);c.text(Margin,y+16,std::to_string(s.rlContacts)+" contacts / "+std::to_string(s.rlChannels)+" channels"+(s.rlBattery.empty()?"":" / "+s.rlBattery),Font::ChatMeta,Muted,W-2*Margin);}
   else c.text(Margin,y,s.rlOn?"Companion on, phone app closed: one link at a time.":"Link off: the T-Deck uses its own radio.",Font::ChatMeta,Muted,W-2*Margin);
   return;}
  {Rect r=RadioLinkPin;c.card(r,s.rlEditing==1,6);auto ink=s.rlEditing==1?OnAccent:FG;std::string pin=s.rlEditing==1?s.rlInput+"_":s.rlPin;
   c.text(r.x+8,r.y+8,"PIN "+pin,Font::Bold,ink,r.w-16);}
  c.button(RadioLinkSearch,s.rlScanning?"...":"SEARCH",s.rlFocus==0);
  if(s.rlPeers.empty())c.text(Margin,radioLinkPeerRow(0).y+8,s.rlScanning?"Searching for companions...":"SEARCH, then tap a companion to pair.",Font::ChatMeta,Muted,W-2*Margin);
  for(int i=0;i<RadioLinkPeerRows&&i<int(s.rlPeers.size());i++){Rect r=radioLinkPeerRow(i);bool on=s.rlFocus==1+i;c.card(r,on,6);auto ink=on?OnAccent:FG;
   c.text(r.x+8,r.y+8,s.rlPeers[i].name,Font::Bold,ink,r.w-70);std::string db=std::to_string(s.rlPeers[i].rssi)+" dBm";c.text(r.x+r.w-8-c.width(db,Font::ChatMeta),r.y+9,db,Font::ChatMeta,on?OnAccent:Muted);}
  return;}
 shell(s,"Companion settings");hideHeaderAction();
 if(!s.rlReady){c.text(Margin,Top+10,"Connect the companion first.",Font::ChatBody,Muted,W-2*Margin);return;}
 {Rect r=RadioLinkName;c.card(r,s.rlEditing==2,6);auto ink=s.rlEditing==2?OnAccent:FG;c.text(r.x+8,r.y+8,"Name",Font::ChatMeta,s.rlEditing==2?OnAccent:Muted,60);
  c.text(r.x+64,r.y+7,s.rlEditing==2?s.rlInput+"_":s.rlName,Font::Bold,ink,r.w-72);}
 c.text(Margin,RadioLinkPathY+8,"Path bytes",Font::ChatMeta,Muted,90);
 for(int i=0;i<3;i++)c.button(radioLinkPathChip(i),std::to_string(i+1),s.rlPathBytes==i+1);
 {Rect r=RadioLinkRegion;c.card(r,s.rlEditing==3,6);auto ink=s.rlEditing==3?OnAccent:FG;c.text(r.x+8,r.y+8,"Region",Font::ChatMeta,s.rlEditing==3?OnAccent:Muted,60);
  c.text(r.x+64,r.y+7,s.rlEditing==3?s.rlInput+"_":(s.rlRegion.empty()?"none":s.rlRegion),Font::Bold,ink,r.w-72);}
 c.button(RadioLinkAdvert,"ADVERT",false);
 // Three fact lines and the preset hint fit above the footer band (FooterY).
 int y=RadioLinkFactsY;std::string counts=std::to_string(s.rlContacts)+" contacts / "+std::to_string(s.rlChannels)+" channels"+(s.rlBattery.empty()?"":" / "+s.rlBattery);
 for(auto &line:{s.rlRadio,s.rlVersion,counts}){if(line.empty())continue;c.text(Margin,y,line,Font::ChatMeta,Muted,W-2*Margin);y+=16;}
 c.text(Margin,y+2,"Radio preset: change it in the companion app.",Font::ChatMeta,Muted,W-2*Margin);
}
