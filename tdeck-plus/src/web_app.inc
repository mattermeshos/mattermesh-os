// MatterMesh Web (test line): Settings > Companion link > MatterMesh Web. The T-Deck serves a desktop page
// (web_page.h) on port 80 of its Wi-Fi address: the browser logs in with the 4-digit code shown on the T-Deck and
// gets a session token (one browser at a time, 5 wrong codes make a new code). The page reads the chats, the
// messages kept in RAM and the network facts, sends messages and adds channels through the same functions the
// keyboard uses. Non-blocking: one request at a time, 25 ms of work per loop. Included by main.cpp.
#include "web_page.h"
static WiFiServer mwServer(80);static WiFiClient mwClient;
static bool mwOn=false;static char mwCode[5]="";static String mwToken,mwPeer;static uint32_t mwSeenAt=0;static int mwFails=0,mwFocus=0;
enum class MwStep {Idle,Header,Body};static MwStep mwStep=MwStep::Idle;static std::string mwHead,mwBody,mwMethod,mwTarget;static size_t mwLength=0;static uint32_t mwLast=0;
static constexpr uint32_t MwHeaderTimeoutMs=5000,MwBudgetMs=25,MwSessionIdleMs=120000;static constexpr size_t MwMaxBody=2048;

static void mwNewCode(){snprintf(mwCode,sizeof mwCode,"%04u",unsigned(esp_random()%10000));mwFails=0;}
static bool mwLinked(){return mwToken.length()&&millis()-mwSeenAt<MwSessionIdleMs;}
static String mwAddress(){auto c=device::connectivityStatus();return c.connected&&c.address.length()?"http://"+c.address:String();}
// Only the switch on its page turns it on (never at boot); a restart leaves it off.
static bool mwPaused=false;// the Trails upload page took port 80: back on when it closes
static void webStart(){if(mwOn)return;mwNewCode();mwToken="";mwPeer="";mwServer.begin();mwOn=true;mwPaused=false;mwStep=MwStep::Idle;Serial.println("WEB_ON");}
static void webStop(){if(!mwOn)return;if(mwClient)mwClient.stop();mwServer.end();mwOn=false;mwStep=MwStep::Idle;mwHead.clear();mwBody.clear();mwToken="";mwPeer="";Serial.println("WEB_OFF");}
bool webActive(){return mwOn;}
void webPause(){if(mwOn){webStop();mwPaused=true;}}
void webResume(){if(mwPaused)webStart();}
static void mwCloseSession(){mwToken="";mwPeer="";mwNewCode();}

// ---- HTTP
static void mwReply(int status,const char *type,const char *body,size_t len){
 const char *text=status==200?"OK":status==400?"Bad Request":status==401?"Unauthorized":status==404?"Not Found":status==409?"Conflict":"Error";
 mwClient.printf("HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",status,text,type,unsigned(len));
 for(size_t at=0;at<len;){size_t n=mwClient.write((const uint8_t*)body+at,std::min<size_t>(1460,len-at));if(!n)break;at+=n;}
 mwClient.flush();mwClient.stop();mwStep=MwStep::Idle;mwHead.clear();mwBody.clear();}
static void mwJson(int status,const std::string &j){mwReply(status,"application/json",j.data(),j.size());}
// Names cut at a fixed byte length can end mid-character: an incomplete or invalid UTF-8 sequence is dropped,
// otherwise the browser shows it as a replacement mark.
static std::string mwEsc(const String &v){std::string o;o.reserve(v.length()+8);const uint8_t *p=(const uint8_t*)v.c_str();size_t len=v.length();
 for(size_t i=0;i<len;){uint8_t c=p[i];
  if(c<0x80){if(c=='"'||c=='\\'){o+='\\';o+=char(c);}else if(c=='\n')o+="\\n";else if(c<32)o+=' ';else o+=char(c);i++;continue;}
  size_t n=c>=0xC2&&c<=0xDF?2:c>=0xE0&&c<=0xEF?3:c>=0xF0&&c<=0xF4?4:0;bool ok=n&&i+n<=len;
  for(size_t k=1;ok&&k<n;k++)ok=(p[i+k]&0xC0)==0x80;
  if(ok&&n==3)ok=!(c==0xE0&&p[i+1]<0xA0)&&!(c==0xED&&p[i+1]>=0xA0);
  if(ok&&n==4)ok=!(c==0xF0&&p[i+1]<0x90)&&!(c==0xF4&&p[i+1]>=0x90);
  if(ok){o.append((const char*)p+i,n);i+=n;}else i++;}
 return o;}
static std::string mwStr(const String &v){return "\""+mwEsc(v)+"\"";}
static void mwError(int status,const String &message){mwJson(status,"{\"error\":"+mwStr(message)+"}");}
static String mwQuery(const char *key){size_t q=mwTarget.find('?');if(q==std::string::npos)return "";std::string k=std::string(key)+"=";size_t at=mwTarget.find("?"+k,q);if(at==std::string::npos)at=mwTarget.find("&"+k,q);if(at==std::string::npos)return "";at+=1+k.size();
 size_t e=mwTarget.find('&',at);std::string v=mwTarget.substr(at,e==std::string::npos?std::string::npos:e-at),o;
 for(size_t i=0;i<v.size();i++){if(v[i]=='%'&&i+2<v.size()){o+=char(strtol(v.substr(i+1,2).c_str(),nullptr,16));i+=2;}else o+=v[i]=='+'?' ':v[i];}return o.c_str();}
static String mwAge(uint32_t seconds){if(seconds<60)return String(seconds)+" s";if(seconds<3600)return String(seconds/60)+" min";if(seconds<86400)return String(seconds/3600)+" h";return String(seconds/86400)+" d";}

// ---- API
static std::string mwDevice(){auto b=device::batteryState();auto c=device::connectivityStatus();time_t now=time(nullptr);String clock=now>=MM_EPOCH_MIN?messageTime(uint32_t(now)):String("--:--");
 return "{\"name\":"+mwStr(matter::snapshot().config.name)+",\"version\":\"" MM_RELEASE_LABEL "\",\"battery\":"+std::to_string(b.percent)+",\"wifiRssi\":"+std::to_string(c.rssi)+",\"clock\":"+mwStr(clock)+",\"theme\":"+std::to_string(lockWallpaper)+"}";}
static std::string mwState(){std::string o="{\"device\":"+mwDevice()+",\"chats\":[";time_t now=time(nullptr);
 for(int i=0;i<chatCount;i++){if(i)o+=",";String preview="",age="";uint32_t total=0;
  if(auto s=radioJournal.summary(i))if(s->total){total=s->total;auto &r=s->last;preview=(r.mine?String("You: "):chats[i].direct?String():r.sender+": ")+r.body;
   if(now>=MM_EPOCH_MIN&&r.timestamp>=MM_EPOCH_MIN&&r.timestamp<=uint32_t(now)){uint32_t m=(uint32_t(now)-r.timestamp)/60;age=m<60?String(m)+"m":m<1440?String(m/60)+"h":String(m/1440)+"d";}}
  o+="{\"id\":"+std::to_string(i)+",\"name\":"+mwStr(chats[i].name)+",\"scope\":"+mwStr(chats[i].scope)+",\"direct\":"+(chats[i].direct?"true":"false")+",\"unread\":"+std::to_string(unreadMessages[i])+
   ",\"muted\":"+((mutedChannels>>i)&1?"true":"false")+",\"total\":"+std::to_string(total)+",\"limit\":"+std::to_string(messageByteLimit(i))+",\"preview\":"+mwStr(preview)+",\"age\":"+mwStr(age)+"}";}
 return o+"]}";}
static std::string mwChat(int chat){std::string o="{\"messages\":[";bool first=true;std::vector<const matter::Record*> list;
 for(auto it=radioJournal.records.rbegin();it!=radioJournal.records.rend()&&list.size()<60;++it)if(it->chat==chat)list.push_back(&*it);
 const bool dm=chats[chat].direct;
 for(auto it=list.rbegin();it!=list.rend();++it){auto &r=**it;if(!first)o+=",";first=false;
  // As the T-Deck chat: own channel message = repeats heard ("Heard x3"), own direct message = delivery status.
  String meta=r.mine?(dm||r.status.startsWith("Not sent")||r.status.startsWith("Pending")?r.status:String("Heard x")+(r.repeaters<0?String("--"):String(r.repeaters))):(r.hops<0?String(dm?"direct":"? hop"):String(r.hops)+" hop");if(!r.mine&&!dm)meta+=" | "+r.scope;
  o+="{\"token\":"+std::to_string(r.token)+",\"mine\":"+(r.mine?"true":"false")+",\"sender\":"+mwStr(r.sender)+",\"body\":"+mwStr(r.body)+",\"time\":"+mwStr(messageTime(r.timestamp))+",\"meta\":"+mwStr(meta)+",\"heard\":"+mwStr(r.heard)+",\"repeaters\":"+std::to_string(r.repeaters)+"}";}
 return o+"]}";}
static std::string mwNetwork(){auto s=matter::snapshot();auto c=device::connectivityStatus();auto b=device::batteryState();uint32_t up=millis()/1000;char f[24];
 snprintf(f,sizeof f,"%.3f",s.config.frequency);std::string bw=String(s.config.bandwidth,s.config.bandwidth==int(s.config.bandwidth)?0:1).c_str();
 std::vector<matter::Node> nodes=matter::nodes(0,0,96);std::stable_sort(nodes.begin(),nodes.end(),[](const matter::Node &a,const matter::Node &b){if(a.live!=b.live)return a.live;return int32_t(a.seen-b.seen)>0;});
 int live=0;for(auto &n:nodes)if(n.live)live++;
 std::string o="{\"radio\":{\"ready\":"+std::string(s.ready?"true":"false")+",\"freq\":\""+f+"\",\"bw\":\""+bw+"\",\"sf\":"+std::to_string(s.config.sf)+",\"cr\":"+std::to_string(s.config.cr)+",\"power\":"+std::to_string(s.config.power)+"}"+
  ",\"mesh\":{\"received\":"+std::to_string(s.received)+",\"sent\":"+std::to_string(s.sent)+",\"contacts\":"+std::to_string(matter::nodeCount(0))+",\"live\":"+std::to_string(live)+",\"rssi\":"+std::to_string(int(s.rssi))+",\"snr\":"+std::to_string(int(s.snr))+"}"+
  ",\"wifi\":{\"ssid\":"+mwStr(c.ssid)+",\"ip\":"+mwStr(c.address)+",\"rssi\":"+std::to_string(c.rssi)+"}"+
  ",\"device\":{\"battery\":"+std::to_string(b.percent)+",\"mv\":"+std::to_string(b.voltage)+",\"uptime\":"+mwStr(mwAge(up))+",\"version\":\"" MM_RELEASE_LABEL "\"}";
 auto gps=device::locationState();if(gps.fix){char p[64];snprintf(p,sizeof p,",\"self\":{\"lat\":%.5f,\"lon\":%.5f}",gps.latitude,gps.longitude);o+=p;}
 o+=",\"nodes\":[";bool first=true;
 for(auto &n:nodes){if(!first)o+=",";first=false;char p[48];snprintf(p,sizeof p,"\"lat\":%.5f,\"lon\":%.5f",n.lat/1e6,n.lon/1e6);
  uint32_t age=n.live?(millis()-n.seen)/1000:0;
  o+="{\"name\":"+mwStr(n.name)+",\"type\":"+std::to_string(n.type)+",\"hops\":"+std::to_string(n.hops)+",\"live\":"+(n.live?"true":"false")+",\"rssi\":"+std::to_string(int(n.rssi))+",\"snr\":"+std::to_string(int(n.snr))+",\"age\":"+mwStr(n.live?mwAge(age):String("saved"))+","+p+"}";}
 return o+"]}";}
// Message info (the T-Deck's hold on a sender / own message): route with repeater names, repeats heard, status.
static std::string mwHashRows(String list,int type){std::string o="[";bool first=true;list.trim();
 while(list.length()){int split=list.indexOf(',');String hash=split<0?list:list.substring(0,split);hash.trim();hash.toLowerCase();
  if(hash.length()>=2&&hash.length()<=8){int matches=0;String name=matter::nodeNameForPrefix(hash,matches,type);
   if(!first)o+=",";first=false;o+="{\"hash\":"+mwStr(hash)+",\"name\":"+mwStr(matches==1?String(mm::senderDisplayName(name.c_str()).c_str()):matches>1?String("ambiguous"):String(""))+"}";}
  if(split<0)break;list.remove(0,split+1);}
 return o+"]";}
static void mwInfo(int chat,uint32_t token){matter::Record r;bool found=false;
 for(auto &x:radioJournal.records)if(x.token==token&&x.chat==chat){r=x;found=true;break;}
 if(!found){mwError(404,"Message not in memory");return;}
 String path=r.path,route="unknown";if(path=="direct")route="direct";else if(path.startsWith("flood:")){path.remove(0,6);route=path.length()?"flood":"heard directly";}else path="";
 char date[24]="";if(r.timestamp>=MM_EPOCH_MIN){time_t t=r.timestamp;struct tm tm;localtime_r(&t,&tm);strftime(date,sizeof date,"%d/%m/%Y %H:%M:%S",&tm);}
 mwJson(200,"{\"sender\":"+mwStr(r.sender)+",\"mine\":"+(r.mine?"true":"false")+",\"direct\":"+(chats[chat].direct?"true":"false")+",\"date\":\""+date+"\",\"hops\":"+std::to_string(r.hops)+
  ",\"scope\":"+mwStr(r.scope)+",\"status\":"+mwStr(r.status)+",\"repeaters\":"+std::to_string(r.repeaters)+",\"route\":"+mwStr(route)+",\"path\":"+mwHashRows(route=="flood"?path:String(),0)+
  ",\"heard\":"+mwHashRows(r.heard,2)+",\"body\":"+mwStr(r.body)+"}");}
// @ in the composer: people who wrote in this chat lately first, then the saved companions (no repeaters).
static std::string mwMention(int chat){std::vector<String> names;
 for(auto it=radioJournal.records.rbegin();it!=radioJournal.records.rend()&&names.size()<12;++it)if(it->chat==chat&&!it->mine&&it->sender.length()&&std::find(names.begin(),names.end(),it->sender)==names.end())names.push_back(it->sender);
 auto nodes=matter::nodes(1,0,256);std::stable_sort(nodes.begin(),nodes.end(),[](const matter::Node &a,const matter::Node &b){if(a.live!=b.live)return a.live;return strcasecmp(a.name.c_str(),b.name.c_str())<0;});
 for(auto &n:nodes)if(n.type==1&&n.name.length()&&names.size()<120&&std::find(names.begin(),names.end(),n.name)==names.end())names.push_back(n.name);
 std::string o="{\"names\":[";for(size_t i=0;i<names.size();i++){if(i)o+=",";o+=mwStr(names[i]);}return o+"]}";}
// The New channel form of the T-Deck, filled from the page: the device screen comes back as it was.
static void mwChannel(DynamicJsonDocument &in){int form=in["form"]|1;if(form<1||form>3){mwError(400,"Unknown form");return;}
 auto shown=screen;int shownSection=section,shownSelected=selected;int before=chatCount;
 newForm=form==1?int(mm::ChannelForm::CreatePrivate):form==2?int(mm::ChannelForm::JoinPrivate):int(mm::ChannelForm::JoinHashtag);
 String name=in["name"]|"",key=in["key"]|"",scope=in["scope"]|"";name.trim();key.trim();key.toLowerCase();scope.trim();if(form==3&&name.startsWith("#"))name.remove(0,1);
 newName=name;newKey=form==3?String():key;newScope=scope.length()?scope:String("*");notice="";
 saveNewChannel();bool ok=chatCount>before;String message=notice;
 screen=shown;section=shownSection;selected=ok&&shown==Screen::Chats?selected:shownSelected;newKey="";newName="";newScope="";notice="";render();
 if(!ok){mwError(409,message.length()?message:String("Channel not saved"));return;}
 mwJson(200,"{\"id\":"+std::to_string(chatCount-1)+",\"message\":"+mwStr(message)+"}");}
static void mwSend(DynamicJsonDocument &in){int chat=in["chat"]|-1;String text=in["text"]|"";
 if(chat<0||chat>=chatCount){mwError(400,"Conversation unavailable");return;}
 uint32_t token=0;bool ok=queueRadioMessage(text,nullptr,chat,&token);String message=notice;notice="";if(!deviceLocked)render();
 if(!ok){mwError(409,message.length()?message:String("Not sent"));return;}mwJson(200,"{\"ok\":true,\"message\":"+mwStr(message)+"}");}

static void mwHandle(){
 size_t q=mwTarget.find('?');std::string route=q==std::string::npos?mwTarget:mwTarget.substr(0,q);
 if(mwMethod=="GET"&&(route=="/"||route=="/index.html")){mwReply(200,"text/html; charset=utf-8",WebPage,sizeof(WebPage)-1);return;}
 if(mwMethod=="GET"&&route=="/favicon.ico"){mwReply(404,"text/plain","",0);return;}
 if(route.compare(0,5,"/api/")){mwReply(404,"text/plain","Not found",9);return;}
 DynamicJsonDocument in(mwMethod=="POST"&&mwBody.size()?MwMaxBody+512:16);/* GETs never read it: no 2.5 KB internal alloc per poll */if(mwMethod=="POST"&&mwBody.size()&&deserializeJson(in,mwBody)){mwError(400,"Bad JSON");return;}
 if(route=="/api/login"&&mwMethod=="POST"){String pin=in["pin"]|"";
  if(pin!=mwCode){if(++mwFails>=5){mwNewCode();render();mwError(401,"Too many tries: read the new code on the T-Deck");}else mwError(401,"Wrong code");return;}
  char t[33];for(int i=0;i<4;i++)snprintf(t+i*8,9,"%08lx",(unsigned long)esp_random());mwToken=t;mwFails=0;mwSeenAt=millis();mwPeer=mwClient.remoteIP().toString();
  Serial.printf("WEB_LOGIN peer=%s\n",mwPeer.c_str());render();mwJson(200,std::string("{\"token\":\"")+t+"\"}");return;}
 if(!mwToken.length()||mwQuery("t")!=mwToken||!mwLinked()){mwError(401,"Session closed");return;}
 mwSeenAt=millis();
 if(route=="/api/state"){mwJson(200,mwState());return;}
 if(route=="/api/chat"){int id=mwQuery("id").toInt();if(id<0||id>=chatCount){mwError(404,"No such chat");return;}mwJson(200,mwChat(id));return;}
 if(route=="/api/network"){mwJson(200,mwNetwork());return;}
 if(route=="/api/info"){int id=mwQuery("chat").toInt();if(id<0||id>=chatCount){mwError(404,"No such chat");return;}mwInfo(id,strtoul(mwQuery("token").c_str(),nullptr,10));return;}
 if(route=="/api/mention"){int id=mwQuery("chat").toInt();if(id<0||id>=chatCount){mwError(404,"No such chat");return;}mwJson(200,mwMention(id));return;}
 if(route=="/api/key"){mwJson(200,"{\"key\":"+mwStr(randomChannelSecret())+"}");return;}
 if(mwMethod!="POST"){mwError(404,"Unknown request");return;}
 if(route=="/api/send"){mwSend(in);return;}
 if(route=="/api/channel"){mwChannel(in);return;}
 if(route=="/api/logout"){mwCloseSession();render();mwJson(200,"{\"ok\":true}");return;}
 mwError(404,"Unknown request");}
static void mwReadHeader(){uint32_t t0=millis();
 while(mwClient.available()&&millis()-t0<MwBudgetMs){int c=mwClient.read();if(c<0)break;mwLast=millis();mwHead+=char(c);
  if(mwHead.size()>4096){mwReply(400,"text/plain","Header too large",16);return;}
  if(mwHead.size()>=4&&!mwHead.compare(mwHead.size()-4,4,"\r\n\r\n")){
   size_t sp=mwHead.find(' '),sp2=mwHead.find(' ',sp+1);if(sp==std::string::npos||sp2==std::string::npos){mwReply(400,"text/plain","Bad request",11);return;}
   mwMethod=mwHead.substr(0,sp);mwTarget=mwHead.substr(sp+1,sp2-sp-1);for(auto &ch:mwHead)ch=char(tolower(uint8_t(ch)));
   size_t cl=mwHead.find("\r\ncontent-length:");mwLength=cl==std::string::npos?0:strtoul(mwHead.c_str()+cl+17,nullptr,10);
   if(mwLength>MwMaxBody){mwReply(400,"text/plain","Body too large",14);return;}
   mwBody.clear();if(mwLength){mwStep=MwStep::Body;return;}mwHandle();return;}}
 if(millis()-mwLast>MwHeaderTimeoutMs){mwClient.stop();mwStep=MwStep::Idle;mwHead.clear();}}
static void mwReadBody(){uint32_t t0=millis();
 while(mwBody.size()<mwLength&&mwClient.available()&&millis()-t0<MwBudgetMs){int c=mwClient.read();if(c<0)break;mwLast=millis();mwBody+=char(c);}
 if(mwBody.size()>=mwLength){mwHandle();return;}
 if(!mwClient.connected()||millis()-mwLast>MwHeaderTimeoutMs){mwClient.stop();mwStep=MwStep::Idle;mwHead.clear();mwBody.clear();}}
void webLoop(){
 if(!mwOn)return;
 static bool shownLinked=false;bool linked=mwLinked();if(linked!=shownLinked){shownLinked=linked;if(screen==Screen::Section&&section==mm::WebAppSection)render();}
 if(mwStep==MwStep::Idle){WiFiClient next=mwServer.available();if(!next)return;mwClient=next;mwStep=MwStep::Header;mwHead.clear();mwLast=millis();}
 if(mwStep==MwStep::Header)mwReadHeader();else if(mwStep==MwStep::Body)mwReadBody();}

// ---- the T-Deck page (section 47)
void webCompose(mm::State &v){v.webOn=mwOn;v.webAddress=mwAddress().c_str();v.webCode=mwCode;v.webLinked=mwLinked();v.webPeer=mwPeer.c_str();v.webFocus=mwFocus;}
static void mwToggle(){if(mwOn)webStop();else webStart();notice=mwOn?(mwAddress().length()?"Open the address in a browser":"Connect to Wi-Fi first"):"MatterMesh Web off";render();}
static void mwAction(int i){if(i==0){mwToggle();return;}if(!mwOn)return;
 if(i==1){mwCloseSession();notice="Session closed / new code";render();return;}}
bool webKey(char c){if(!(screen==Screen::Section&&section==mm::WebAppSection))return false;
 if(c==' '||c=='t'){mwToggle();return true;}
 if(c=='j'||c=='s'||c=='l'||c=='d'){mwFocus=std::min(1,mwFocus+1);render();return true;}if(c=='k'||c=='w'||c=='h'||c=='a'){mwFocus=std::max(0,mwFocus-1);render();return true;}
 if(c=='\n'){mwAction(mwFocus);return true;}return false;}
bool webTap(int x,int y){if(!(screen==Screen::Section&&section==mm::WebAppSection))return false;if(mm::BackButton.contains(x,y))return false;
 if(mm::WebAppToggle.contains(x,y)){mwToggle();return true;}if(mm::WebAppClose.contains(x,y)){mwAction(1);return true;}return true;}
// Serial (tests): "web status|on|off|code|close".
bool webSerial(const String &s){if(!s.startsWith("web ")&&s!="web")return false;
 if(s=="web on"){webStart();render();}else if(s=="web off"){webStop();render();}else if(s=="web close"){mwCloseSession();render();}
 Serial.printf("WEB_STATUS on=%d address=%s code=%s linked=%d peer=%s fails=%d\n",mwOn,mwAddress().c_str(),s=="web code"?mwCode:"****",mwLinked(),mwPeer.c_str(),mwFails);return true;}
