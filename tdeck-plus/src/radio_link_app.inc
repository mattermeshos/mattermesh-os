// NVS "mmrlink": on, addr, type, name, pin. Pages: radio_link_layout.h / ui/radio_link_renderer.inc.
// Companion link: the T-Deck uses the radio of another MeshCore companion over BLE. With "Use companion
// radio" on, the SX1262 sleeps, chat sends go to the companion (channels matched by their secret, DMs by the
// contact key prefix) and the messages it receives land in the T-Deck chats. Included by main.cpp after the chat code.
// Limits by design: the companion sends with its own identity and its default region (chat scopes are not used), and
// new DM contacts are discovered by the companion, not by the sleeping T-Deck radio.
#include <SHA256.h>
#include "services/companion_client.h"
#include "services/companion_ble.h"
#include "ui/radio_link_layout.h"
static remote::Client rlClient;
static bool rlOn=false;static String rlAddress,rlName,rlInput,rlPinText="123456";static uint8_t rlType=0;static uint32_t rlPin=123456;
static int rlEditing=0,rlFocus=0;static device::RadioLinkPeer rlPending;static uint32_t rlPendingPin=0;static bool rlPairing=false;
static std::vector<device::RadioLinkPeer> rlShown;/* the companions on screen: a tap pairs the row it hit */static int rlAuthFails=0;
static uint32_t rlRetryAt=0,rlSeen=0,rlBatteryAt=0,rlForgetArmedAt=0;static device::RadioLinkPhase rlPhase=device::RadioLinkPhase::Off;static bool rlTimeSent=false;
static uint32_t rlIn=0,rlDropped=0,rlSent=0,rlDelivered=0,rlFailed=0;/* counters for the serial status (tests) */
// Messages waiting on the companion: -1 not asked since the link went on, 0 they stay there, 1 downloaded. A reconnection
// keeps the answer; turning the link off and on asks again.
static int rlSyncChoice=-1;
static void rlAskAgain(){rlSyncChoice=-1;rlClient.syncAllowed=false;}
constexpr uint32_t RadioLinkRetryMs=20000,RadioLinkScanMs=6000,RadioLinkBatteryMs=60000,RadioLinkForgetMs=4000;
static void rlWrite(void*,const uint8_t *f,size_t n){device::radioLinkSend(f,n);}
static bool rlSaved(){return rlAddress.length()>0;}
static bool rlPage(){return screen==Screen::Section&&(section==mm::RadioLinkSection||section==mm::RadioLinkSettingsSection);}
bool radioLinkActive(){return rlOn;}
bool radioLinkConnected(){return rlClient.ready();}
static bool rlAskDemo=false;// serial "rlink sync demo": the question on screen with no companion (tests)
bool radioLinkAsking(){return (rlAskDemo||(rlOn&&rlClient.ready()&&rlSyncChoice<0))&&!deviceLocked&&screen!=Screen::Welcome;}
void radioLinkAnswer(bool yes){if(rlAskDemo){rlAskDemo=false;Serial.printf("RLINK_DEMO answer=%d\n",yes);markRadioDirty();return;}rlSyncChoice=yes;rlClient.allowSync(yes,millis());notice=yes?"Downloading messages":"Messages stay on the companion";markRadioDirty();}
// Text limit while the link is on: the companion prefixes channel texts with its own name.
size_t radioLinkTextLimit(bool direct){size_t name=rlClient.info.name.size();return matter::messageBodyLimit(direct,name?name:matter::snapshot().config.name.length());}
static void rlSave(){Preferences p;if(!p.begin("mmrlink",false))return;p.putBool("on",rlOn);p.putString("addr",rlAddress);p.putUChar("type",rlType);p.putString("name",rlName);p.putUInt("pin",rlPin);p.end();}
static void rlConnectSaved(){if(!rlSaved())return;device::RadioLinkPeer p;p.name=rlName;p.address=rlAddress;p.addressType=rlType;device::radioLinkConnect(p,rlPin);rlRetryAt=millis()+RadioLinkRetryMs;}
// On: the T-Deck radio sleeps and the companion is (re)connected; off: the link closes and the radio wakes.
static void rlSetOn(bool on){
 if(on&&!rlSaved()){notice="Pair a companion first";if(rlOn){rlOn=false;rlSave();matter::radioSuspend(false);markRadioDirty();}return;}
 rlOn=on;rlSave();rlAskAgain();bool radio=matter::radioSuspend(on)||!matter::snapshot().identity;/* no radio to restart without an SD identity */
 if(on)rlConnectSaved();else{device::radioLinkDisconnect();rlClient.disconnected();}
 notice=on?"Using the companion radio":radio?"T-Deck radio back on":"T-Deck radio did not restart";markRadioDirty();}
void radioLinkBegin(){
 rlClient.begin(rlWrite,nullptr);rlAskAgain();
 Preferences p;if(p.begin("mmrlink",true)){rlOn=p.getBool("on",false);rlAddress=p.getString("addr","");rlType=p.getUChar("type",0);rlName=p.getString("name","");rlPin=p.getUInt("pin",123456);p.end();}
 // The connection starts from radioLinkLoop 8 s after boot (SD and UI start done): the contact listing that follows arrives as a
 // burst the main loop must be running to take.
 rlPinText=String(rlPin);if(rlOn&&rlSaved()){matter::radioSuspend(true);rlRetryAt=millis()+8000;}else rlOn=false;}
// A message the companion received, into the T-Deck chats: a channel by its secret (a channel the T-Deck does not
// have is dropped with a notice), a DM by the sender's full key (the prefix only when the companion does not know it).
static void rlDeliverIncoming(const remote::Message &m){
 matter::Received r;r.timestamp=m.timestamp;r.hops=m.hops;r.sender=m.sender.c_str();r.body=m.body.c_str();
 if(m.direct){r.direct=true;r.key=m.keyKnown?mm::hexString(m.key,remote::KeySize).c_str():mm::hexString(m.prefix,6).c_str();if(!r.sender.length())r.sender="Unknown contact";}
 else{const remote::Channel *ch=nullptr;for(auto &c:rlClient.channels)if(c.index==m.channel){ch=&c;break;}
  int id=-1;if(ch){String secret=mm::hexString(ch->secret,16).c_str();for(int i=0;i<chatCount;i++){String mine;if(!chats[i].direct&&channelReady[i]&&matter::channelSecret(i,mine)&&mine.equalsIgnoreCase(secret)){id=i;break;}}}
  if(id<0){rlDropped++;notice=String("Companion: message on ")+(ch?ch->name.c_str():"an unknown channel")+" (not on the T-Deck)";markRadioDirty();return;}
  r.channelId=id;r.channel=chats[id].name;}
 rlIn++;incomingRadio(r);}
static void rlDeliverOutcome(const remote::Delivery &d){
 if(d.outcome==remote::Outcome::Delivered)rlDelivered++;else if(d.outcome==remote::Outcome::Sent)rlSent++;else rlFailed++;
 matter::Delivery out;out.token=d.token;out.txComplete=true;out.txSuccess=d.outcome==remote::Outcome::Sent||d.outcome==remote::Outcome::Delivered;
 out.state=d.outcome==remote::Outcome::Delivered?"Delivered":d.outcome==remote::Outcome::Sent?"Sent via companion":d.outcome==remote::Outcome::NoAck?"No ACK":"Failed (companion)";deliveredRadio(out);}
// A chat send while the link is on (queueRadioMessage): false with a reason when the companion cannot take it.
// Without a T-Deck clock the message carries the companion's time (a 0 stamp would make equal texts duplicates).
bool radioLinkChatSend(int chat,const String &body,uint32_t token,String &reason){
 if(!rlClient.ready()){reason="Companion not connected";return false;}
 time_t now=time(nullptr);uint32_t ts=now>=MM_EPOCH_MIN?uint32_t(now):rlClient.companionTime(millis());std::string text(body.c_str());
 if(chats[chat].direct){uint8_t prefix[6];if(peerKeys[chat].length()<12||!mm::parseHex(peerKeys[chat].c_str(),12,prefix,6)){reason="Contact key missing";return false;}
  if(!rlClient.contactByPrefix(prefix)){reason="Contact not on the companion";return false;}
  if(!rlClient.sendDirect(prefix,text,ts,token,millis())){reason="Companion refused";return false;}}
 else{String secret;uint8_t raw[16];if(!matter::channelSecret(chat,secret)||!mm::parseHex(secret.c_str(),secret.length(),raw,16)){reason="Channel key missing";return false;}
  auto *ch=rlClient.channelBySecret(raw);if(!ch){reason="Channel not on the companion";return false;}
  if(!rlClient.sendChannel(ch->index,text,ts,token,millis())){reason="Companion refused";return false;}
  matter::remoteEchoTrack(token,raw,ts,body);}
 reason="Sent to companion";return true;}
void radioLinkLoop(){
 uint32_t now=millis();
 if(device::radioLinkRevision()!=rlSeen){auto st=device::radioLinkStatus();rlSeen=st.revision;
  static uint32_t connectsSeen=0;if(st.phase==device::RadioLinkPhase::Connected&&rlPhase==device::RadioLinkPhase::Connected&&st.connects!=connectsSeen){rlClient.disconnected();rlClient.connected(now);rlTimeSent=false;}connectsSeen=st.connects;
  if(st.phase!=rlPhase){auto was=rlPhase;rlPhase=st.phase;
   if(st.phase==device::RadioLinkPhase::Connected){rlClient.connected(now);rlTimeSent=false;rlAuthFails=0;
    if(rlPairing){rlPairing=false;rlAddress=rlPending.address;rlType=rlPending.addressType;rlName=rlPending.name;rlPin=rlPendingPin;rlOn=true;rlSave();rlAskAgain();matter::radioSuspend(true);notice="Companion paired: its radio is in use";}}
   else if(was==device::RadioLinkPhase::Connected){rlClient.disconnected();if(st.phase==device::RadioLinkPhase::Failed)notice="Companion disconnected";}
   if(st.phase==device::RadioLinkPhase::Failed&&rlPairing){rlPairing=false;notice=st.message;if(!rlOn&&!rlPage())device::radioLinkDisconnect();/* BLE back to the phone */}
   // A saved companion that refuses our keys twice has dropped its bond (reflashed, reset): ours goes too, so the
   // next retry pairs again with the saved PIN.
   else if(st.phase==device::RadioLinkPhase::Failed&&rlOn&&st.message.startsWith("Pairing failed")&&++rlAuthFails==2){device::radioLinkForgetBond(rlAddress,rlType);notice="Pairing again with the companion";}}
  if(rlPage())markRadioDirty();}
 uint8_t frame[200];size_t n;int budget=512;bool changed=false;while(budget--&&device::radioLinkTake(frame,n)){if(frame[0]==0x88&&n>3)matter::remoteRaw(frame+3,int(n-3));/* RX log: SNR, RSSI, the raw packet */else rlClient.frame(frame,n,now);changed=true;}
 rlClient.tick(now);
 {static bool shownReady=false;if(rlClient.ready()!=shownReady){shownReady=rlClient.ready();markRadioDirty();}}/* the status bar antenna changes colour on every page */
 {static bool asked=false;if(radioLinkAsking()!=asked){asked=!asked;if(asked){device::activity();render();}else markRadioDirty();}}/* at once: it takes the input */
 if(rlClient.reconnectWanted){rlClient.reconnectWanted=false;device::radioLinkDisconnect(false);rlClient.disconnected();rlRetryAt=now+2000;notice="Companion did not answer: reconnecting";}
 remote::Message m;while(rlClient.takeMessage(m))rlDeliverIncoming(m);
 remote::Delivery d;while(rlClient.takeDelivery(d))rlDeliverOutcome(d);
 if(rlClient.ready()&&!rlTimeSent){rlTimeSent=true;time_t t=time(nullptr);if(t>=MM_EPOCH_MIN)rlClient.setTime(uint32_t(t),now);rlClient.refreshBattery(now);rlBatteryAt=now;}
 if(rlClient.ready()&&now-rlBatteryAt>=RadioLinkBatteryMs){rlBatteryAt=now;rlClient.refreshBattery(now);rlClient.poll(now);}
 if(changed&&rlPage())markRadioDirty();
 // Auto reconnect while on: every RadioLinkRetryMs after a failure or a drop.
 if(rlOn&&rlSaved()&&!rlPairing&&(rlPhase==device::RadioLinkPhase::Off||rlPhase==device::RadioLinkPhase::Failed)&&int32_t(now-rlRetryAt)>=0)rlConnectSaved();}
void radioLinkCompose(mm::State &v){
 auto st=device::radioLinkStatus();v.rlOn=rlOn;
 v.rlSaved=rlSaved();v.rlSavedName=rlName.c_str();v.rlConnected=st.phase==device::RadioLinkPhase::Connected;v.rlReady=rlClient.ready();
 v.rlScanning=st.phase==device::RadioLinkPhase::Scanning;v.rlFailed=st.phase==device::RadioLinkPhase::Failed;
 v.rlStatus=rlForgetArmedAt&&millis()-rlForgetArmedAt<RadioLinkForgetMs?"Tap FORGET again to forget this companion":v.rlReady?std::string("Connected / ")+(rlClient.info.version.empty()?"MeshCore":rlClient.info.version):v.rlConnected?"Connected / syncing contacts and channels...":st.message.length()?st.message.c_str():(rlSaved()?(rlOn?"Waiting":"Off"):"");
 v.rlEditing=rlEditing;v.rlFocus=rlFocus;v.rlInput=rlInput.c_str();v.rlPin=rlPinText.c_str();
 rlShown.clear();for(auto &p:device::radioLinkPeers()){if(v.rlPeers.size()>=size_t(mm::RadioLinkPeerRows))break;mm::State::RadioLinkPeerRow row;row.name=p.name.c_str();row.rssi=p.rssi;v.rlPeers.push_back(row);rlShown.push_back(p);}
 auto &i=rlClient.info;v.rlName=i.name;v.rlRegion=i.scopeKnown?i.scope:"(not supported)";v.rlPathBytes=i.pathModeKnown?i.pathMode+1:0;v.rlContacts=int(rlClient.contacts.size());v.rlChannels=int(rlClient.channels.size());
 v.rlVersion=i.model.empty()?i.version:i.model+" / "+i.version;
 if(i.frequencyKhz){char b[80];snprintf(b,sizeof b,"%.3f MHz / BW %.1f / SF%u / CR%u / %d dBm",i.frequencyKhz/1000.0,i.bandwidthHz/1000.0,i.sf,i.cr,i.power);v.rlRadio=b;}
 if(i.batteryMv){char b[32];snprintf(b,sizeof b,"battery %.2f V",i.batteryMv/1000.0);v.rlBattery=b;}}
String *radioLinkField(){return rlEditing?&rlInput:nullptr;}
// Editing: 1 PIN, 2 companion name, 3 region.
static void rlStartEdit(int what){if((what==2||what==3)&&!rlClient.ready()){notice="Connect the companion first";render();return;}
 rlEditing=what;rlInput=what==1?rlPinText:what==2?String(rlClient.info.name.c_str()):String(rlClient.info.scopeKnown?rlClient.info.scope.c_str():"");render();}
static void rlFinishEdit(){
 String text=rlInput;text.trim();int what=rlEditing;rlEditing=0;rlInput="";
 if(what==1){long pin=text.toInt();if(text.length()==6&&pin>=100000&&pin<=999999){rlPinText=text;notice="PIN set";}else notice="PIN: 6 digits";}
 else if(what==2){if(text.length()&&rlClient.setName(text.c_str(),millis()))notice="Companion name sent";else notice="Name not sent";}
 else if(what==3){String name=text;if(name.startsWith("#"))name=name.substring(1);uint8_t key[16]={};
  if(name.length()){String tag="#"+name;SHA256 sha;sha.update(tag.c_str(),tag.length());uint8_t h[32];sha.finalize(h,32);memcpy(key,h,16);}
  notice=rlClient.setRegion(name.c_str(),key,millis())?(name.length()?"Companion region set":"Companion region cleared"):"Region not sent";}
 render();}
// FORGET asks for a second tap within RadioLinkForgetMs; it drops the pairing keys too (NimBLE keeps only three).
static void rlForget(){
 if(!rlForgetArmedAt||millis()-rlForgetArmedAt>=RadioLinkForgetMs){rlForgetArmedAt=millis();notice="Tap FORGET again to confirm";render();return;}
 rlForgetArmedAt=0;device::radioLinkDisconnect();rlClient.disconnected();device::radioLinkForgetBond(rlAddress,rlType);
 bool wasOn=rlOn;if(wasOn)rlOn=false;rlAddress="";rlName="";rlType=0;rlSave();if(wasOn)matter::radioSuspend(false);rlFocus=0;notice="Companion forgotten";render();}
static void rlPair(int index){if(rlPairing||device::radioLinkStatus().phase==device::RadioLinkPhase::Connecting)return;/* one pairing at a time */
 if(index<0||index>=int(rlShown.size()))return;rlPending=rlShown[index];rlPendingPin=uint32_t(rlPinText.toInt());rlPairing=true;
 device::radioLinkConnect(rlPending,rlPendingPin);notice="Pairing with "+rlPending.name+"...";render();}
// SETTINGS / CONNECT or DISCONNECT (the same as the switch: it stays off until turned on) / FORGET.
static void rlScan(){if(rlPairing){notice="Pairing in progress";render();return;}device::radioLinkScan(RadioLinkScanMs);rlFocus=0;render();}
static void rlAction(int i){if(i==0){openSection(mm::RadioLinkSettingsSection);return;}if(i==1){rlSetOn(!rlOn);render();return;}rlForget();}
// Leaving the link pages ends an edit and an armed FORGET; with the link off and no pairing running, BLE goes back
// to the phone companion server (after a scan or a failed pairing).
void radioLinkLeft(){rlEditing=0;rlInput="";rlFocus=0;rlForgetArmedAt=0;
 auto phase=device::radioLinkStatus().phase;if(!rlOn&&!rlPairing&&phase!=device::RadioLinkPhase::Connected&&phase!=device::RadioLinkPhase::Connecting)device::radioLinkDisconnect();}
bool radioLinkKey(char c){
 if(c=='\b')return false;// Backspace: the generic field edit / Back
 if(!rlPage())return false;
 if(rlEditing){if(c=='\n'){rlFinishEdit();return true;}size_t cap=rlEditing==1?6:rlEditing==2?31:30;if(c>=32&&c<=126&&rlInput.length()<cap&&(rlEditing!=1||(c>='0'&&c<='9'))){rlInput+=c;repaintSettled();}return true;}
 if(section==mm::RadioLinkSection){
  if(rlSaved()){if(c==' '||c=='t'){rlSetOn(!rlOn);render();return true;}if(c=='j'||c=='l'){rlFocus=std::min(3,rlFocus+1);render();return true;}if(c=='k'||c=='h'){rlFocus=std::max(0,rlFocus-1);render();return true;}
   if(c=='\n'){if(rlFocus==0){rlSetOn(!rlOn);}else rlAction(rlFocus-1);render();return true;}return false;}
  int peers=int(device::radioLinkPeers().size());
  if(c=='f'||(c=='\n'&&rlFocus==0)){rlScan();return true;}
  if(c=='p'){rlStartEdit(1);return true;}
  if(c=='j'||c=='l'){rlFocus=std::min(std::min(peers,mm::RadioLinkPeerRows),rlFocus+1);render();return true;}if(c=='k'||c=='h'){rlFocus=std::max(0,rlFocus-1);render();return true;}
  if(c=='\n'&&rlFocus>0){rlPair(rlFocus-1);return true;}return false;}
 if(c=='n'){rlStartEdit(2);return true;}if(c=='r'){rlStartEdit(3);return true;}if(c=='a'){notice=rlClient.advertise(millis())?"Advert sent by the companion":"Companion not ready";render();return true;}
 if(c>='1'&&c<='3'){notice=rlClient.setPathBytes(c-'0',millis())?"Path bytes sent":"Companion not ready";render();return true;}
 return false;}
bool radioLinkTap(int x,int y){
 if(!rlPage())return false;
 if(mm::BackButton.contains(x,y))return false;
 if(section==mm::RadioLinkSection){
  if(rlEditing&&!mm::RadioLinkPin.contains(x,y)){rlFinishEdit();return true;}
  if(rlSaved()){if(mm::RadioLinkToggle.contains(x,y)){rlSetOn(!rlOn);render();return true;}for(int i=0;i<3;i++)if(mm::radioLinkAction(i).contains(x,y)){rlAction(i);return true;}return true;}
  if(mm::RadioLinkPin.contains(x,y)){if(rlEditing==1)rlFinishEdit();else rlStartEdit(1);return true;}
  if(mm::RadioLinkSearch.contains(x,y)){rlScan();return true;}
  for(int i=0;i<mm::RadioLinkPeerRows;i++)if(mm::radioLinkPeerRow(i).contains(x,y)){rlPair(i);return true;}return true;}
 if(rlEditing&&!(mm::RadioLinkName.contains(x,y)||mm::RadioLinkRegion.contains(x,y))){rlFinishEdit();return true;}
 if(mm::RadioLinkName.contains(x,y)){if(rlEditing==2)rlFinishEdit();else rlStartEdit(2);return true;}
 if(mm::RadioLinkRegion.contains(x,y)){if(rlEditing==3)rlFinishEdit();else rlStartEdit(3);return true;}
 if(mm::RadioLinkAdvert.contains(x,y)){notice=rlClient.advertise(millis())?"Advert sent by the companion":"Companion not ready";render();return true;}
 for(int i=0;i<3;i++)if(mm::radioLinkPathChip(i).contains(x,y)){notice=rlClient.setPathBytes(i+1,millis())?"Path bytes sent":"Companion not ready";render();return true;}
 return true;}
// Serial (tests): "rlink status|last|peers|scan|pair N [PIN]|on|off|forget|info|sync yes|sync no|sync demo" (forget: twice, as the button).
bool radioLinkSerial(const String &s){
 if(!s.startsWith("rlink"))return false;auto st=device::radioLinkStatus();
 if(s=="rlink status"){Serial.printf("RLINK_STATUS source=0 on=%d saved=%d phase=%d ready=%d name=%s message=\"%s\" contacts=%d channels=%d peers=%d radio_suspended=%d connects=%lu drops=%lu lost=%lu timeouts=%lu errors=%lu skipped=%lu stray=%lu\n",rlOn,rlSaved(),int(st.phase),rlClient.ready(),rlName.c_str(),st.message.c_str(),int(rlClient.contacts.size()),int(rlClient.channels.size()),int(device::radioLinkPeers().size()),matter::snapshot().suspended,(unsigned long)st.connects,(unsigned long)st.drops,(unsigned long)st.lost,(unsigned long)rlClient.timeouts,(unsigned long)rlClient.errors,(unsigned long)rlClient.skipped,(unsigned long)rlClient.stray);return true;}
 if(s=="rlink page"){auto v=uiState();Serial.printf("RLINK_PAGE saved=%d name=%s status=%s\n",v.rlSaved,v.rlSavedName.c_str(),v.rlStatus.c_str());return true;}/* what the page shows */
 if(s=="rlink sync demo"){rlAskDemo=true;markRadioDirty();Serial.println("RLINK_DEMO shown");return true;}
 if(s=="rlink sync yes"||s=="rlink sync no"){const bool asking=radioLinkAsking();if(asking)radioLinkAnswer(s.endsWith("yes"));Serial.printf("RLINK_SYNC asked=%d choice=%d\n",asking,rlSyncChoice);return true;}
 if(s=="rlink last"){Serial.printf("RLINK_LAST chat=%d sender=%s body=%s in=%lu dropped=%lu sent=%lu delivered=%lu failed=%lu\n",lastReceivedChat,lastReceivedSender.c_str(),lastReceivedBody.c_str(),(unsigned long)rlIn,(unsigned long)rlDropped,(unsigned long)rlSent,(unsigned long)rlDelivered,(unsigned long)rlFailed);return true;}
 if(s=="rlink peers"){int i=0;for(auto &p:device::radioLinkPeers())Serial.printf("RLINK_PEER index=%d name=%s address=%s type=%u rssi=%d\n",i++,p.name.c_str(),p.address.c_str(),p.addressType,p.rssi);Serial.printf("RLINK_PEERS count=%d\n",i);return true;}
 if(s=="rlink scan"){rlScan();Serial.println("RLINK_SCAN");return true;}
 if(s.startsWith("rlink pair ")){rlShown=device::radioLinkPeers();/* serial: pairs by the current list order */int sp=s.indexOf(' ',11);int idx=s.substring(11,sp<0?s.length():sp).toInt();if(sp>0)rlPinText=s.substring(sp+1);rlPair(idx);Serial.printf("RLINK_PAIR index=%d\n",idx);return true;}
 if(s=="rlink on"||s=="rlink off"){rlSetOn(s=="rlink on");Serial.printf("RLINK_ON on=%d\n",rlOn);render();return true;}
 if(s=="rlink forget"){rlForget();Serial.printf("RLINK_FORGET saved=%d\n",rlSaved());return true;}
 if(s=="rlink info"){auto &i=rlClient.info;Serial.printf("RLINK_INFO name=%s model=%s version=%s freq=%lu bw=%lu sf=%u cr=%u power=%d path=%d region=%s battery=%u clock=%lu\n",i.name.c_str(),i.model.c_str(),i.version.c_str(),(unsigned long)i.frequencyKhz,(unsigned long)i.bandwidthHz,i.sf,i.cr,i.power,i.pathModeKnown?i.pathMode+1:0,i.scopeKnown?i.scope.c_str():"-",i.batteryMv,(unsigned long)rlClient.companionTime(millis()));
  for(auto &c:rlClient.channels)Serial.printf("RLINK_CHANNEL index=%d name=%s\n",c.index,c.name.c_str());return true;}
 return false;}
