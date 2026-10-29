// Chat options > Path (direct chats): the route of the DMs to the contact (matter::dmRoute*, dm_path_layout.h).
// Included by main.cpp.
static int dmPathChat=-1,dmPathFocus=0,dmPathOffset=0;static bool dmPathEditing=false,dmPathKeys=false;// dmPathKeys: keys used here, the focus shows
static String dmPathInput;static std::vector<uint8_t> dmPathHops;// hashes chosen in SET PATH, dmPathHashWidth() each
static bool dmPathDirect=false;// SET PATH: DIRECT chosen (a save with no hops needs it: a stray click never saves an empty path)
static std::vector<matter::Node> dmPathNodes;// known repeaters (live first, then most recently heard), read when SET PATH opens
static int dmPathHashWidth(){/* Path bytes (Settings > LoRa): bytes per hop */return matter::snapshot().config.pathBytes;}
static String dmPathContact(){return dmPathChat>=0&&dmPathChat<chatCount&&chats[dmPathChat].direct?peerKeys[dmPathChat]:String();}
static bool onDmPathPage(){return screen==Screen::Section&&section==mm::DmPathSection;}
void dmPathOpen(int chat){
 if(chat<0||chat>=chatCount||!chats[chat].direct||peerKeys[chat].length()!=64){notice="Contact key missing";render();return;}
 dmPathChat=chat;dmPathEditing=dmPathKeys=false;dmPathFocus=0;openSection(mm::DmPathSection);}
// A hop as shown: the repeater name when one known repeater has that prefix, else the hex.
static std::string dmPathHopName(const uint8_t *hash,int width){
 String prefix=mm::hexString(hash,size_t(width)).c_str();int matches=0;String name=matter::nodeNameForPrefix(prefix,matches,2);/* 2: repeaters */
 return matches==1&&name.length()?std::string(name.c_str()):std::string(prefix.c_str());}
static std::string dmPathRouteText(){
 uint8_t encoded=mm::DirectPathUnknown,bytes[64];if(!matter::contactPath(dmPathContact(),encoded,bytes,sizeof(bytes)))return "Contact not known to the radio";
 if(matter::dmRouteMode(dmPathContact())==matter::DmRouteMode::Flood)return "Flood";
 int hops=mm::directPathHops(encoded);if(hops<0)return "Flood (no path learned yet)";if(hops==0)return "Direct (in radio range)";
 int width=(encoded>>6)+1;std::string out=std::to_string(hops)+(hops==1?" hop: ":" hops: ");
 for(int i=0;i<hops;i++){if(i)out+=" > ";out+=dmPathHopName(bytes+i*width,width);}return out;}
void dmPathCompose(mm::State &v){
 if(dmPathChat<0||dmPathChat>=chatCount)return;
 v.dmPathName=chats[dmPathChat].name.c_str();auto mode=matter::dmRouteMode(dmPathContact());v.dmPathMode=int(mode);v.dmPathHashBytes=dmPathHashWidth();v.dmPathEditing=dmPathEditing;
 v.dmPathRoute=dmPathRouteText();
 uint8_t encoded=mm::DirectPathUnknown,bytes[64];bool learned=matter::contactPath(dmPathContact(),encoded,bytes,sizeof(bytes))&&encoded!=mm::DirectPathUnknown;
 v.dmPathOrigin=mode==matter::DmRouteMode::Flood?"Flood only: a learned path is not used":mode==matter::DmRouteMode::Manual?"Set by hand: kept until RESET":learned?"Learned from replies (RESET forgets it)":"No path yet: the first reply teaches it";
 v.dmPathFocus=dmPathKeys&&!dmPathEditing?dmPathFocus:-1;// SET PATH: rows are picked by touch
 if(!dmPathEditing)return;
 const int width=dmPathHashWidth();for(size_t i=0;i+width<=dmPathHops.size();i+=width){if(i)v.dmPathDraft+=" > ";v.dmPathDraft+=dmPathHopName(dmPathHops.data()+i,width);}
 v.dmPathInput=dmPathInput.c_str();
 for(int i=dmPathOffset;i<int(dmPathNodes.size())&&i<dmPathOffset+mm::DmPathRows;i++){auto &n=dmPathNodes[i];mm::State::DmPathRepeater row;row.name=n.name.length()?n.name.c_str():"Repeater";
  row.prefix=std::string(n.key.substring(0,width*2).c_str());v.dmPathRepeaters.push_back(row);}}
static void dmPathStartEdit(){dmPathEditing=true;dmPathDirect=false;dmPathInput="";dmPathHops.clear();dmPathOffset=0;notice="";
 dmPathNodes=matter::recentRepeaters(48);render();}
static void dmPathAdd(const uint8_t *hash){const int width=dmPathHashWidth();if(int(dmPathHops.size())/width>=std::min(63,64/width)){notice="Path full";return;}dmPathHops.insert(dmPathHops.end(),hash,hash+width);}
static void dmPathAddNode(int index){if(index<0||index>=int(dmPathNodes.size()))return;uint8_t key[32];if(!mm::parseHex(dmPathNodes[index].key.c_str(),64,key,32))return;dmPathAdd(key);dmPathDirect=false;notice="";}
static void dmPathSave(){
 if(dmPathInput.length()){notice="Finish the hex of the last hop";render();return;}
 if(dmPathHops.empty()&&!dmPathDirect){notice="Add hops, or DIRECT for a contact in range";render();return;}
 const int width=dmPathHashWidth();bool ok=matter::dmRouteSet(dmPathContact(),dmPathHops.data(),int(dmPathHops.size())/width,width);
 notice=ok?"Path saved: used for the next message":"Path not saved";if(ok){dmPathEditing=false;dmPathNodes=std::vector<matter::Node>();}render();}
static void dmPathRun(int index){
 auto key=dmPathContact();if(!key.length())return;
 if(index==0){notice=matter::dmRouteReset(key)?"Path forgotten: next message by flood":"Reset failed";render();}
 else if(index==1){bool on=matter::dmRouteMode(key)!=matter::DmRouteMode::Flood;notice=matter::dmRouteFlood(key,on)?(on?"Flood only: on":"Flood only: off"):"Save failed";render();}
 else dmPathStartEdit();}
static void dmPathUndo(){if(dmPathInput.length())dmPathInput.remove(dmPathInput.length()-1);else if(dmPathHops.size())dmPathHops.resize(dmPathHops.size()-dmPathHashWidth());render();}
// BACK on the page: SET PATH closes first.
bool dmPathBack(){if(!onDmPathPage()||!dmPathEditing)return false;dmPathEditing=false;dmPathNodes=std::vector<matter::Node>();notice="";render();return true;}
String *dmPathField(){return onDmPathPage()&&dmPathEditing?&dmPathInput:nullptr;}
bool dmPathKeyPress(char c){
 if(!onDmPathPage())return false;
 if(dmPathEditing){
  if(c=='\n'){dmPathSave();return true;}if(c=='\b'){dmPathUndo();return true;}
  char h=c>='A'&&c<='F'?char(c-'A'+'a'):c;
  if((h>='0'&&h<='9')||(h>='a'&&h<='f')){dmPathInput+=h;const int width=dmPathHashWidth();
   if(int(dmPathInput.length())==width*2){uint8_t hash[3];if(mm::parseHex(dmPathInput.c_str(),width*2,hash,width))dmPathAdd(hash);dmPathInput="";dmPathDirect=false;}notice="";repaintSettled();}
  return true;}
 if(c=='w'||c=='k'||c=='a'||c=='h'){dmPathKeys=true;dmPathFocus=std::max(0,dmPathFocus-1);render();return true;}
 if(c=='s'||c=='j'||c=='d'||c=='l'){dmPathKeys=true;dmPathFocus=std::min(2,dmPathFocus+1);render();return true;}
 if(c=='\n'){if(!dmPathKeys){dmPathKeys=true;render();}/* the first click shows the focus: no unseen RESET */else dmPathRun(dmPathFocus);return true;}
 return false;}
bool dmPathTap(int x,int y){
 if(!onDmPathPage())return false;
 if(mm::BackButton.contains(x,y)){back();return true;}
 if(!dmPathEditing){for(int i=0;i<3;i++)if(mm::dmPathButton(i).contains(x,y)){dmPathFocus=i;dmPathRun(i);return true;}return true;}
 for(int i=0;i<mm::DmPathRows;i++)if(mm::dmPathRow(i).contains(x,y)){dmPathAddNode(dmPathOffset+i);render();return true;}
 if(mm::dmPathAction(0).contains(x,y))dmPathUndo();
 else if(mm::dmPathAction(1).contains(x,y)){dmPathHops.clear();dmPathInput="";dmPathDirect=true;notice="Direct: SAVE to use it";render();}
 else if(mm::dmPathAction(2).contains(x,y))dmPathBack();
 else if(mm::dmPathAction(3).contains(x,y))dmPathSave();
 return true;}
// Touch scroll of the repeater rows in SET PATH (rows of DmPathRowH).
bool dmPathScroll(int d){if(!onDmPathPage()||!dmPathEditing)return false;dmPathOffset=constrain(dmPathOffset+d,0,std::max(0,int(dmPathNodes.size())-mm::DmPathRows));render();return true;}
// USB (chat -1: the open chat): dm path <chat> | dm path reset <chat> | dm path set <chat> <hex,hex,...> (empty: direct) |
// dm flood <chat> on|off | dm path learn <chat> <hex,...> (tests: the path as if learned, Auto mode).
bool dmPathSerial(const String &s){
 if(!s.startsWith("dm path")&&!s.startsWith("dm flood"))return false;
 auto idOf=[&](const String &arg){String a=arg;a.trim();char *end=nullptr;long id=a.length()?strtol(a.c_str(),&end,10):-2;if(!a.length()||(end&&*end&&*end!=' '))return -2;return id==-1?activeChat:int(id);};// -2: not a chat id
 auto chatOf=[&](const String &arg)->String{int id=idOf(arg);return id>=0&&id<chatCount&&chats[id].direct?peerKeys[id]:String();};
 auto report=[&](int id){int was=dmPathChat;dmPathChat=id;auto mode=matter::dmRouteMode(dmPathContact());Serial.printf("DM_PATH chat=%d mode=%d route=\"%s\"\n",id,int(mode),dmPathRouteText().c_str());dmPathChat=was;};
 if(s.startsWith("dm path reset ")){int id=idOf(s.substring(14));bool ok=matter::dmRouteReset(chatOf(s.substring(14)));Serial.printf("DM_PATH_RESET ok=%d\n",ok);if(ok)report(id);return true;}
 if(s.startsWith("dm path set ")||s.startsWith("dm path learn ")){const bool learn=s.startsWith("dm path learn ");String rest=s.substring(learn?14:12);int sp=rest.indexOf(' ');int id=idOf(sp<0?rest:rest.substring(0,sp));String list=sp<0?String():rest.substring(sp+1);list.trim();
  const int width=dmPathHashWidth();std::vector<uint8_t> hops;bool ok=true;int at=0;
  while(ok&&at<int(list.length())){int comma=list.indexOf(',',at);String h=list.substring(at,comma<0?list.length():comma);h.trim();at=comma<0?list.length():comma+1;
   uint8_t hash[3];if(int(h.length())!=width*2||!mm::parseHex(h.c_str(),width*2,hash,width))ok=false;else hops.insert(hops.end(),hash,hash+width);}
  ok=ok&&matter::dmRouteSet(chatOf(String(id)),hops.data(),int(hops.size())/width,width)&&(!learn||matter::dmRouteFlood(chatOf(String(id)),false));Serial.printf("DM_PATH_SET ok=%d\n",ok);if(ok)report(id);return true;}
 if(s.startsWith("dm flood ")){String rest=s.substring(9);int sp=rest.indexOf(' ');String arg=sp>0?rest.substring(sp+1):String();if(arg!="on"&&arg!="off"){Serial.println("DM_FLOOD error=on or off");return true;}bool on=arg=="on";int id=idOf(sp>0?rest.substring(0,sp):rest);bool ok=matter::dmRouteFlood(chatOf(String(id)),on);Serial.printf("DM_FLOOD ok=%d on=%d\n",ok,on);if(ok)report(id);return true;}
 if(s.startsWith("dm path ")){int id=idOf(s.substring(8));if(!chatOf(s.substring(8)).length()){Serial.println("DM_PATH error=not a direct chat");return true;}report(id);return true;}
 return false;}
