// MeshCore companion protocol (the MeshCore phone / web app), shared by BLE, USB and Wi-Fi (TCP 5000).
// Included by service.cpp inside namespace matter. Frames follow meshcore-base/examples/companion_radio/MyMesh.cpp
// (protocol version 14). Stage 1: app hand-shake, node / device info, contact list, channels (read only), clock,
// self advert, name, radio parameters, TX power, path hash size, battery and storage, reboot, BLE PIN.
// Stage 2: messages. Received direct and channel messages wait in an offline queue (newest 32) that the app drains
// with SYNC_NEXT_MESSAGE, a PUSH_CODE_MSG_WAITING tells a connected app; the app sends direct (ACK pushed back as
// SEND_CONFIRMED) and channel texts. SET_CHANNEL adds or edits a channel conversation (128-bit keys, as MeshCore);
// channel deletes still answer "unsupported".
// Contacts (as MyMesh.cpp): add/update, import, export (self or a contact), share zero-hop, reset path, remove, get by
// key. Changes are saved to the SD like a path update (contactsDirty).
namespace companion {
enum : uint8_t {
 CMD_APP_START=1,CMD_SEND_TXT_MSG=2,CMD_SEND_CHANNEL_TXT_MSG=3,CMD_GET_CONTACTS=4,CMD_GET_DEVICE_TIME=5,CMD_SET_DEVICE_TIME=6,
 CMD_SEND_SELF_ADVERT=7,CMD_SET_ADVERT_NAME=8,CMD_ADD_UPDATE_CONTACT=9,CMD_SYNC_NEXT_MESSAGE=10,CMD_SET_RADIO_PARAMS=11,CMD_SET_RADIO_TX_POWER=12,
 CMD_RESET_PATH=13,CMD_REMOVE_CONTACT=15,CMD_SHARE_CONTACT=16,CMD_EXPORT_CONTACT=17,CMD_IMPORT_CONTACT=18,CMD_GET_CONTACT_BY_KEY=30,
 CMD_REBOOT=19,CMD_GET_BATT_AND_STORAGE=20,CMD_DEVICE_QUERY=22,CMD_HAS_CONNECTION=28,CMD_GET_CHANNEL=31,CMD_SET_CHANNEL=32,CMD_SET_DEVICE_PIN=37,
 CMD_GET_CUSTOM_VARS=40,CMD_GET_TUNING_PARAMS=43,CMD_SET_PATH_HASH_MODE=61,
 RESP_SENT=6,RESP_CONTACT_MSG_RECV=7,RESP_CHANNEL_MSG_RECV=8,RESP_CONTACT_MSG_RECV_V3=16,RESP_CHANNEL_MSG_RECV_V3=17,PUSH_SEND_CONFIRMED=0x82,PUSH_MSG_WAITING=0x83,PUSH_LOG_RX_DATA=0x88,
 TXT_PLAIN=0,ERR_TABLE_FULL=3,
 RESP_OK=0,RESP_ERR=1,RESP_CONTACTS_START=2,RESP_CONTACT=3,RESP_END_OF_CONTACTS=4,RESP_SELF_INFO=5,RESP_CURR_TIME=9,
 RESP_EXPORT_CONTACT=11,RESP_NO_MORE_MESSAGES=10,RESP_BATT_AND_STORAGE=12,RESP_DEVICE_INFO=13,RESP_CHANNEL_INFO=18,RESP_CUSTOM_VARS=21,RESP_TUNING_PARAMS=23,
 ERR_UNSUPPORTED=1,ERR_NOT_FOUND=2,ERR_BAD_STATE=4,ERR_ILLEGAL_ARG=6};
constexpr uint8_t ProtocolVersion=14;
static CompanionHooks hooks;
struct Out {uint8_t b[COMPANION_FRAME_MAX];size_t n=0;void u8(uint8_t v){if(n<sizeof(b))b[n++]=v;}void put(const void *p,size_t k){k=std::min(k,sizeof(b)-n);memcpy(b+n,p,k);n+=k;}
 void u32(uint32_t v){put(&v,4);}void i32(int32_t v){put(&v,4);}void u16(uint16_t v){put(&v,2);}void text(const char *s,size_t field){uint8_t z[64]={};size_t k=std::min(strlen(s),field-1);memcpy(z,s,k);put(z,field);}};
static void send(const Out &o,CompanionWriter w,void *ctx){if(w)w(ctx,o.b,o.n);}
static void ok(CompanionWriter w,void *ctx){Out o;o.u8(RESP_OK);send(o,w,ctx);}
static void err(uint8_t code,CompanionWriter w,void *ctx){Out o;o.u8(RESP_ERR);o.u8(code);send(o,w,ctx);}
// Apply a changed radio / identity setting through the normal path (saved to the SD, radio reconfigured).
static void apply(Config c,CompanionWriter w,void *ctx){String reason;if(configure(c,reason))ok(w,ctx);else err(ERR_ILLEGAL_ARG,w,ctx);}
// Offline queue for the app (MeshCore's frames: V3 when the app announced protocol >= 3) and the pushes every
// active link sends on its next pump (message waiting, send confirmed).
constexpr int QueueMax=32,PushMax=8,AckMax=8;
struct Queued {uint8_t len=0;uint8_t b[COMPANION_FRAME_MAX];};
static Queued *queue=nullptr;static int queueHead=0,queueCount=0;static uint8_t appVersion=3;
struct Push {uint8_t len=0;uint8_t b[12];};static Push pushes[PushMax];static uint32_t pushSeq=0;
struct Ack {uint32_t ack=0,sentMs=0;};static Ack acks[AckMax];static int nextAck=0;
// Raw RX log (PUSH_CODE_LOG_RX_DATA, MeshCore's logRxRaw: snr*4, rssi, packet bytes). The app finds the repeater
// echoes of its own channel messages in it; without it a sent channel message stays on "Sent". Kept only once an
// app has connected (a ring of the newest LogMax packets, each link sends what it has not sent yet).
constexpr int LogMax=16;struct LogFrame {uint8_t len=0;uint8_t b[COMPANION_FRAME_MAX];};
static LogFrame *logs=nullptr;
// Queue and RX log: about 7 KB, in PSRAM when there is some (internal RAM otherwise), allocated on first use
// (an app connecting, or the first message to queue). Each buffer is checked on its own.
template<class T> static T *appAlloc(int n){return static_cast<T*>(device::psramCalloc(n,sizeof(T)));}
static bool queueBuffer(){if(!queue)queue=appAlloc<Queued>(QueueMax);return queue!=nullptr;}
static bool logBuffer(){if(!logs)logs=appAlloc<LogFrame>(LogMax);return logs!=nullptr;}
static uint32_t logSeq=0;static bool appSeen=false;
void logRaw(float snr,float rssi,const uint8_t *raw,int len){if(!appSeen||!logs||len<=0||size_t(len)+3>COMPANION_FRAME_MAX)return;auto &l=logs[logSeq%LogMax];l.b[0]=PUSH_LOG_RX_DATA;l.b[1]=uint8_t(int8_t(snr*4));l.b[2]=uint8_t(int8_t(rssi));memcpy(l.b+3,raw,len);l.len=uint8_t(len+3);logSeq++;}
// Stage 3: zero-hop control data and anonymous requests (companion_air.h), used by the app's "Discover regions".
// Their pushes are longer than the 1-byte ones, so they go through the RX log ring.
static void pushFrame(const uint8_t *frame,size_t n){if(!appSeen||!logs||!n||n>COMPANION_FRAME_MAX)return;auto &l=logs[logSeq%LogMax];memcpy(l.b,frame,n);l.len=uint8_t(n);logSeq++;}
static companion_air::Pending anonPending;
void contactResponse(const uint8_t *data,uint8_t len){uint8_t f[COMPANION_FRAME_MAX];pushFrame(f,companion_air::responseFrame(anonPending,data,len,f,sizeof(f)));}
void controlData(const mesh::Packet *packet,float snr,float rssi){uint8_t f[COMPANION_FRAME_MAX];pushFrame(f,companion_air::controlFrame(packet,snr,rssi,f,sizeof(f)));}
static void push(const uint8_t *frame,size_t n){auto &p=pushes[pushSeq%PushMax];p.len=uint8_t(std::min(n,sizeof(p.b)));memcpy(p.b,frame,p.len);pushSeq++;}
static void enqueue(const Out &o){if(!queueBuffer())return;if(queueCount==QueueMax){queueHead=(queueHead+1)%QueueMax;queueCount--;}auto &q=queue[(queueHead+queueCount)%QueueMax];q.len=uint8_t(o.n);memcpy(q.b,o.b,o.n);queueCount++;uint8_t tickle=PUSH_MSG_WAITING;push(&tickle,1);}
static void text(Out &o,const char *t){size_t n=strlen(t);o.put(t,std::min(n,sizeof(o.b)-o.n));}
void queueContact(const ContactInfo &from,mesh::Packet *pkt,uint32_t ts,const char *t){Out o;
 if(appVersion>=3){o.u8(RESP_CONTACT_MSG_RECV_V3);o.u8(uint8_t(int8_t(pkt->getSNR()*4)));o.u8(0);o.u8(0);}else o.u8(RESP_CONTACT_MSG_RECV);
 o.put(from.id.pub_key,6);o.u8(pkt->isRouteFlood()?pkt->path_len:0xFF);o.u8(TXT_PLAIN);o.u32(ts);text(o,t);enqueue(o);}
// The app numbers channels 0..n-1 without holes; our mesh slots follow the conversation list, where direct chats leave
// empty slots. App index k is the k-th configured mesh slot.
static int meshSlot(int app){ChannelDetails ch;for(int i=0,k=0;i<MAX_GROUP_CHANNELS;i++)if(chatMesh().getChannel(i,ch)&&ch.name[0]){if(k==app)return i;k++;}return -1;}
static int appSlot(int mesh){ChannelDetails ch;int k=0;for(int i=0;i<mesh&&i<MAX_GROUP_CHANNELS;i++)if(chatMesh().getChannel(i,ch)&&ch.name[0])k++;return k;}
void queueChannel(int idx,mesh::Packet *pkt,uint32_t ts,const char *t){idx=appSlot(idx);Out o;
 if(appVersion>=3){o.u8(RESP_CHANNEL_MSG_RECV_V3);o.u8(uint8_t(int8_t(pkt->getSNR()*4)));o.u8(0);o.u8(0);}else o.u8(RESP_CHANNEL_MSG_RECV);
 o.u8(uint8_t(idx));o.u8(pkt->isRouteFlood()?pkt->path_len:0xFF);o.u8(TXT_PLAIN);o.u32(ts);text(o,t);enqueue(o);}
void ackReceived(const uint8_t *data){for(auto &a:acks)if(a.ack&&!memcmp(data,&a.ack,4)){uint8_t f[9];f[0]=PUSH_SEND_CONFIRMED;memcpy(f+1,data,4);uint32_t trip=millis()-a.sentMs;memcpy(f+5,&trip,4);push(f,9);a.ack=0;}}
static void contactFrame(Out &o,const ContactInfo &c){o.u8(RESP_CONTACT);o.put(c.id.pub_key,PUB_KEY_SIZE);o.u8(c.type);o.u8(c.flags);o.u8(c.out_path_len);o.put(c.out_path,MAX_PATH_SIZE);
 o.text(c.name,32);o.u32(c.last_advert_timestamp);o.i32(c.gps_lat);o.i32(c.gps_lon);o.u32(c.lastmod);}
}
namespace companion {
// MyMesh::updateContactFromFrame: key, type, flags, path, name, last advert, then optional lat/lon and lastmod.
static void contactFromFrame(ContactInfo &c,uint32_t &lastmod,const uint8_t *in,size_t len){
 size_t i=1;memcpy(c.id.pub_key,in+i,PUB_KEY_SIZE);i+=PUB_KEY_SIZE;c.type=in[i++];c.flags=in[i++];c.out_path_len=in[i++];
 if(len<i+MAX_PATH_SIZE+32+4)return;memcpy(c.out_path,in+i,MAX_PATH_SIZE);i+=MAX_PATH_SIZE;memcpy(c.name,in+i,32);c.name[31]=0;i+=32;
 memcpy(&c.last_advert_timestamp,in+i,4);i+=4;if(len>=i+8){memcpy(&c.gps_lat,in+i,4);memcpy(&c.gps_lon,in+i+4,4);i+=8;if(len>=i+4)memcpy(&lastmod,in+i,4);}}
static void contactsChanged(){contactsDirty=true;saveAt=millis();}
}
void companionHooks(const CompanionHooks &h){companion::hooks=h;}
void companionFrame(CompanionSession &s,const uint8_t *in,size_t len,CompanionWriter w,void *ctx){
 using namespace companion;if(!len)return;
 if(!meshPtr){err(ERR_BAD_STATE,w,ctx);return;}
 const uint8_t cmd=in[0];auto cfg=snapshot().config;Out o;
 auto read32=[&](size_t at){uint32_t v=0;if(at+4<=len)memcpy(&v,in+at,4);return v;};
 switch(cmd){
 case CMD_DEVICE_QUERY:{
  appVersion=len>=2?in[1]:0;s.active=true;s.pushSeq=pushSeq;s.logSeq=logSeq;appSeen=true;queueBuffer();logBuffer();// pushes from before this connection are not replayed
  o.u8(RESP_DEVICE_INFO);o.u8(ProtocolVersion);o.u8(uint8_t(std::min(255,MAX_CONTACTS/2)));o.u8(MAX_GROUP_CHANNELS);o.u32(hooks.pin?hooks.pin():0);
  o.text(__DATE__,12);o.text("LilyGO " MM_DEVICE_MODEL " / MatterMesh OS",40);o.text("MatterMesh " MM_BUILD_VERSION,20);o.u8(0);o.u8(uint8_t(cfg.pathBytes?cfg.pathBytes-1:0));
  send(o,w,ctx);break;}
 case CMD_APP_START:{
  s.listing=false;int32_t lat=0,lon=0;if(hooks.location)hooks.location(lat,lon);
  o.u8(RESP_SELF_INFO);o.u8(ADV_TYPE_CHAT);o.u8(uint8_t(cfg.power));o.u8(uint8_t(MAX_TX_POWER));o.put(chatMesh().self_id.pub_key,PUB_KEY_SIZE);o.i32(lat);o.i32(lon);
  o.u8(0);o.u8(0);o.u8(0);o.u8(0);// multi acks, advert location policy, telemetry modes, manual add contacts
  o.u32(uint32_t(lroundf(cfg.frequency*1000)));o.u32(uint32_t(lroundf(cfg.bandwidth*1000)));o.u8(cfg.sf);o.u8(cfg.cr);o.put(cfg.name.c_str(),cfg.name.length());
  send(o,w,ctx);break;}
 case CMD_GET_CONTACTS:{
  // A new request restarts a running listing (MeshCore answers BAD_STATE): with ~1000 contacts the first listing
  // after connecting lasts long, and the app's refresh after a delete would otherwise wait on a spinner.
  s.since=len>=5?read32(1):0;s.mostRecent=0;s.next=MAX_ANON_CONTACTS;s.listing=true;
  o.u8(RESP_CONTACTS_START);o.u32(uint32_t(chatMesh().getNumContacts()));send(o,w,ctx);break;}
 case CMD_GET_DEVICE_TIME:o.u8(RESP_CURR_TIME);o.u32(clock.getCurrentTime());send(o,w,ctx);break;
 case CMD_SET_DEVICE_TIME:{if(len<5){err(ERR_ILLEGAL_ARG,w,ctx);break;}String reason;if(syncClock(read32(1),reason))ok(w,ctx);else err(ERR_ILLEGAL_ARG,w,ctx);break;}
 case CMD_SEND_SELF_ADVERT:{String reason;if(advertise(reason))ok(w,ctx);else err(ERR_BAD_STATE,w,ctx);break;}
 case CMD_SET_ADVERT_NAME:{if(len<2){err(ERR_ILLEGAL_ARG,w,ctx);break;}String name;for(size_t i=1;i<len&&i<=31;i++)name+=char(in[i]);name.trim();cfg.name=name;apply(cfg,w,ctx);break;}
 case CMD_SET_RADIO_PARAMS:{
  if(len<11){err(ERR_ILLEGAL_ARG,w,ctx);break;}uint32_t f=read32(1),bw=read32(5);uint8_t sf=in[9],cr=in[10],repeat=len>11?in[11]:0;
  if(repeat){err(ERR_ILLEGAL_ARG,w,ctx);break;}// the client repeat mode is not part of MatterMesh OS
  cfg.frequency=f/1000.0f;cfg.bandwidth=bw/1000.0f;cfg.sf=sf;cfg.cr=cr;apply(cfg,w,ctx);break;}
 case CMD_SET_RADIO_TX_POWER:{if(len<2){err(ERR_ILLEGAL_ARG,w,ctx);break;}cfg.power=int8_t(in[1]);apply(cfg,w,ctx);break;}
 case CMD_SET_PATH_HASH_MODE:{if(len<3||in[1]!=0||in[2]>=3){err(ERR_ILLEGAL_ARG,w,ctx);break;}cfg.pathBytes=uint8_t(in[2]+1);apply(cfg,w,ctx);break;}
 case CMD_ADD_UPDATE_CONTACT:{
  if(len<1+PUB_KEY_SIZE+3){err(ERR_ILLEGAL_ARG,w,ctx);break;}
  uint32_t lastmod=clock.getCurrentTime();ContactInfo *known=chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE);
  if(known){contactFromFrame(*known,lastmod,in,len);known->lastmod=lastmod;contactsChanged();ok(w,ctx);break;}
  ContactInfo c={};contactFromFrame(c,lastmod,in,len);c.lastmod=lastmod;c.sync_since=0;
  if(chatMesh().addContact(c)){contactsChanged();ok(w,ctx);}else err(ERR_TABLE_FULL,w,ctx);break;}
 case CMD_RESET_PATH:{ContactInfo *c=len>=1+PUB_KEY_SIZE?chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE):nullptr;
  if(!c){err(ERR_NOT_FOUND,w,ctx);break;}c->out_path_len=OUT_PATH_UNKNOWN;contactsChanged();ok(w,ctx);break;}
 case CMD_REMOVE_CONTACT:{ContactInfo *c=len>=1+PUB_KEY_SIZE?chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE):nullptr;
  if(c&&chatMesh().removeContact(*c)){contactsChanged();ok(w,ctx);}else err(ERR_NOT_FOUND,w,ctx);break;}
 case CMD_SHARE_CONTACT:{ContactInfo *c=len>=1+PUB_KEY_SIZE?chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE):nullptr;
  if(!c)err(ERR_NOT_FOUND,w,ctx);else if(chatMesh().shareContactZeroHop(*c))ok(w,ctx);else err(ERR_TABLE_FULL,w,ctx);break;}
 case CMD_GET_CONTACT_BY_KEY:{ContactInfo *c=len>=1+PUB_KEY_SIZE?chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE):nullptr;
  if(!c){err(ERR_NOT_FOUND,w,ctx);break;}contactFrame(o,*c);send(o,w,ctx);break;}
 case CMD_EXPORT_CONTACT:{
  uint8_t raw[MAX_TRANS_UNIT+1];uint8_t n=0;
  if(len<1+PUB_KEY_SIZE){// the app's "share my contact": our own advert, flood route as MyMesh
   int32_t lat=0,lon=0;if(hooks.location)hooks.location(lat,lon);
   mesh::Packet *pkt=lat||lon?chatMesh().createSelfAdvert(cfg.name.c_str(),lat/1e6,lon/1e6):chatMesh().createSelfAdvert(cfg.name.c_str());
   if(pkt){pkt->header|=ROUTE_TYPE_FLOOD;n=pkt->writeTo(raw);chatMesh().releasePacket(pkt);}
  }else{ContactInfo *c=chatMesh().lookupContactByPubKey(in+1,PUB_KEY_SIZE);if(c)n=chatMesh().exportContact(*c,raw);}
  if(!n||size_t(n)+1>sizeof(o.b)){err(len<1+PUB_KEY_SIZE?ERR_TABLE_FULL:ERR_NOT_FOUND,w,ctx);break;}
  o.u8(RESP_EXPORT_CONTACT);o.put(raw,n);send(o,w,ctx);break;}
 case CMD_IMPORT_CONTACT:{
  if(len>2+PUB_KEY_SIZE+64&&len<=256&&chatMesh().importContact(in+1,uint8_t(len-1))){contactsChanged();ok(w,ctx);}else err(ERR_ILLEGAL_ARG,w,ctx);break;}
 case CMD_SYNC_NEXT_MESSAGE:
  if(queueCount){auto &q=queue[queueHead];queueHead=(queueHead+1)%QueueMax;queueCount--;if(w)w(ctx,q.b,q.len);}else{o.u8(RESP_NO_MORE_MESSAGES);send(o,w,ctx);}break;
 case CMD_SEND_TXT_MSG:{
  if(len<14){err(ERR_ILLEGAL_ARG,w,ctx);break;}uint8_t type=in[1],attempt=in[2];uint32_t ts=read32(3);ContactInfo *to=chatMesh().lookupContactByPubKey(in+7,6);
  if(!to){err(ERR_NOT_FOUND,w,ctx);break;}if(type!=TXT_PLAIN){err(ERR_UNSUPPORTED,w,ctx);break;}
  char body[COMPANION_FRAME_MAX];size_t n=std::min(len-13,sizeof(body)-1);memcpy(body,in+13,n);body[n]=0;uint32_t ack=0,timeout=0;
  int result=chatMesh().sendMessage(*to,ts,attempt,body,ack,timeout);if(result==MSG_SEND_FAILED){err(ERR_TABLE_FULL,w,ctx);break;}
  if(ack){acks[nextAck].ack=ack;acks[nextAck].sentMs=millis();nextAck=(nextAck+1)%AckMax;}
  o.u8(RESP_SENT);o.u8(result==MSG_SEND_SENT_FLOOD?1:0);o.u32(ack);o.u32(timeout);send(o,w,ctx);break;}
 case CMD_SEND_CHANNEL_TXT_MSG:{
  if(len<8){err(ERR_ILLEGAL_ARG,w,ctx);break;}uint8_t type=in[1],idx=in[2];uint32_t ts=read32(3);if(type!=TXT_PLAIN){err(ERR_UNSUPPORTED,w,ctx);break;}
  ChannelDetails ch;int slot=meshSlot(idx);if(slot<0||!chatMesh().getChannel(slot,ch)||!ch.name[0]){err(ERR_NOT_FOUND,w,ctx);break;}
  if(chatMesh().sendGroupMessage(ts,ch.channel,cfg.name.c_str(),reinterpret_cast<const char*>(in+7),int(len-7)))ok(w,ctx);else err(ERR_NOT_FOUND,w,ctx);break;}
 case CMD_GET_BATT_AND_STORAGE:o.u8(RESP_BATT_AND_STORAGE);o.u16(hooks.batteryMv?hooks.batteryMv():0);o.u32(hooks.storageUsedKb?hooks.storageUsedKb():0);o.u32(hooks.storageTotalKb?hooks.storageTotalKb():0);send(o,w,ctx);break;
 case CMD_GET_CHANNEL:{
  if(len<2||in[1]>=MAX_GROUP_CHANNELS){err(ERR_NOT_FOUND,w,ctx);break;}ChannelDetails ch={};int slot=meshSlot(in[1]);if(slot>=0)chatMesh().getChannel(slot,ch);
  o.u8(RESP_CHANNEL_INFO);o.u8(in[1]);o.text(ch.name,32);o.put(ch.channel.secret,16);send(o,w,ctx);break;}/* unused app slots read as empty, as in MeshCore */
 case CMD_SET_CHANNEL:{
  if(len>=2+32+32){err(ERR_UNSUPPORTED,w,ctx);break;}// 256-bit secrets: not in MeshCore either
  if(len<2+32+16||!hooks.setChannel){err(ERR_ILLEGAL_ARG,w,ctx);break;}
  char name[33]={};memcpy(name,in+2,32);int slot=meshSlot(in[1]);uint8_t code=hooks.setChannel(slot<0?0xFF:uint8_t(slot),name,in+2+32);if(code)err(code,w,ctx);else ok(w,ctx);break;}
 case CMD_REBOOT:if(len>=7&&!memcmp(in+1,"reboot",6)&&hooks.reboot){ok(w,ctx);hooks.reboot();}else err(ERR_ILLEGAL_ARG,w,ctx);break;
 case CMD_SET_DEVICE_PIN:{uint32_t pin=read32(1);if(len<5||(pin&&(pin<100000||pin>999999))||!hooks.setPin||!hooks.setPin(pin))err(ERR_ILLEGAL_ARG,w,ctx);else ok(w,ctx);break;}
 case CMD_GET_CUSTOM_VARS:o.u8(RESP_CUSTOM_VARS);send(o,w,ctx);break;
 case CMD_GET_TUNING_PARAMS:o.u8(RESP_TUNING_PARAMS);o.u32(0);o.u32(1000);send(o,w,ctx);break;// MeshCore defaults: rx delay 0, airtime factor 1.0
 case CMD_HAS_CONNECTION:err(ERR_NOT_FOUND,w,ctx);break;
 case companion_air::CMD_SEND_CONTROL_DATA:{uint8_t r[2];size_t n=companion_air::sendControl(chatMesh(),in,len,r);if(w)w(ctx,r,n);break;}
 case companion_air::CMD_SEND_ANON_REQ:{uint8_t r[10];size_t n=companion_air::sendAnon(chatMesh(),clock.getCurrentTime(),in,len,anonPending,r);if(w)w(ctx,r,n);break;}
 default:err(ERR_UNSUPPORTED,w,ctx);break;
 }
}
// One contact per call while a GET_CONTACTS listing runs (the transport calls it when it can send again).
bool companionPump(CompanionSession &s,CompanionWriter w,void *ctx){
 using namespace companion;
 // Pushes first (message waiting, send confirmed), one per call; a link that fell behind skips to the last PushMax.
 if(s.active&&s.pushSeq!=pushSeq){if(pushSeq-s.pushSeq>PushMax)s.pushSeq=pushSeq-PushMax;auto &p=pushes[s.pushSeq%PushMax];s.pushSeq++;if(w)w(ctx,p.b,p.len);return true;}
 if(s.active&&logs&&s.logSeq!=logSeq){if(logSeq-s.logSeq>LogMax)s.logSeq=logSeq-LogMax;auto &l=logs[s.logSeq%LogMax];s.logSeq++;if(w)w(ctx,l.b,l.len);return true;}
 if(!s.listing||!meshPtr)return false;
 ContactInfo c;while(chatMesh().getContactByIdx(uint32_t(s.next),c)){s.next++;if(!s.since||c.lastmod>s.since){Out o;/* contacts loaded from the SD have lastmod 0: a full listing sends them all */contactFrame(o,c);send(o,w,ctx);if(c.lastmod>s.mostRecent)s.mostRecent=c.lastmod;return true;}}
 Out o;o.u8(RESP_END_OF_CONTACTS);o.u32(s.mostRecent);send(o,w,ctx);s.listing=false;return true;
}
