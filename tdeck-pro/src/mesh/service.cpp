#include "../services/clock_limits.h"
#include "../board/device_model.h"
#include "chat_limits.h"
#include <memory>
#include "../ui/build_info.h"
#include "../services/direct_chat.h"
#include "services/hex_util.h"
#include "../services/sd_replace.h"
#include "../services/tsv_field.h"
#include "../services/psram.h"
#include "service.h"
#include "radio_parameters.h"
#include "companion_settings.h"
#include "companion_air.h"
#include <CayenneLPP.h>
#include <helpers/SensorManager.h>
#include <ArduinoJson.h>
#include "antenna_policy.h"
#include <Preferences.h>
#include "repeater_observer.h"
#include <SPI.h>
#include <esp_heap_caps.h>
#include <new>
#include "../board/board.h"
#include <esp_system.h>
#include <esp_timer.h>
#include <helpers/BaseChatMesh.h>
static_assert(MAX_TEXT_LEN==matter::MESSAGE_MAX_TEXT_BYTES,"Message limits must match the compiled MeshCore");
#include <helpers/ArduinoHelpers.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/TransportKeyStore.h>
#include <SHA256.h>
#include <ed_25519.h>
#include <time.h>
namespace matter {
static Preferences antennaPrefs;
static RxLog rxPackets; // last received packets (CLI log); slots in PSRAM (begin)
static fs::FS *storage=nullptr;
static Receiver receivedCallback;static Delivered deliveredCallback;
static Snapshot state;static bool contactsDirty=false;static uint32_t saveAt=0,lastContactsSave=0,lastContactsAttempt=0;
struct CachedNode {char key[65]={},name[32]={};uint8_t type=0;uint32_t seen=0;int32_t lat=0,lon=0;int hops=-1;float rssi=0,snr=0;bool live=false,verified=false,advertVerified=false;uint8_t advertDigest[32]={};
};
static CachedNode *discovered=nullptr;static int discoveredCount=0;
static uint32_t nodeChanges=0;// any cached node heard or changed: the Contacts page repaints
// Returns true when the node is new or its saved fields (name, type, position) changed: only then the contact book on SD is dirty.
static bool cacheNode(const CachedNode &node){int index=discoveredCount;for(int i=0;i<discoveredCount;i++)if(!strcmp(discovered[i].key,node.key)){index=i;break;}bool changed=true;if(index==discoveredCount){if(discoveredCount>=NODE_CAPACITY)return false;new(&discovered[discoveredCount++]) CachedNode(node);}else{auto old=discovered[index];changed=strcmp(old.name,node.name)!=0||old.type!=node.type||old.lat!=node.lat||old.lon!=node.lon;discovered[index]=node;auto &n=discovered[index];if(!node.lat&&!node.lon){n.lat=old.lat;n.lon=old.lon;}}
  nodeChanges++;return changed;}

static String channelScopes[CHAT_SLOTS],currentScope="*";static bool explicitGroupScope=false;
static std::vector<String> knownScopes;// filled by registerScope (saved and channel scopes)
static String hex(const uint8_t *p,size_t n){return String(mm::hexString(p,n).c_str());}
static bool unhex(const String &s,uint8_t *p,size_t n){return mm::parseHex(s.c_str(),s.length(),p,n);}
static String safe(String s,size_t max=160){String out;for(size_t i=0;i<s.length()&&out.length()<max;i++){char c=s[i];out+=(c=='\t'||c=='\r'||c=='\n')?' ':c;}return out;}
static String field(const String &s,int index){return mm::tsvField(s,index);}
class HardwareRNG:public mesh::RNG {void random(uint8_t *dest,size_t n)override{esp_fill_random(dest,n);}};
// Persisted protocol sequence never pretends to be a valid wall clock.
class Clock:public mesh::RTCClock {
 uint32_t base=1;uint64_t start=0;bool wall=false;
public:
 void init(fs::FS &fs){File f=fs.open("/mattermesh/protocol-clock");uint32_t last=f?f.parseInt():0;f.close();base=last+86400;start=esp_timer_get_time();File out=fs.open("/mattermesh/protocol-clock","w");if(out){out.println(base+86400);out.close();}}
 uint32_t getCurrentTime()override{time_t t=time(nullptr);return t>=MM_EPOCH_MIN?uint32_t(t):base+uint32_t((esp_timer_get_time()-start)/1000000);}
 void setCurrentTime(uint32_t t)override{if(t>=MM_EPOCH_MIN){timeval tv={time_t(t),0};settimeofday(&tv,nullptr);wall=true;}else{base=t;start=esp_timer_get_time();}}
};
class Board:public mesh::MainBoard {
 uint16_t getBattMilliVolts()override{return 0;}
 const char *getManufacturerName()const override{return "LilyGO " MM_DEVICE_MODEL " / MATTERMESH OS";}
 void reboot()override{ESP.restart();}
 uint8_t getStartupReason()const override{return BD_STARTUP_NORMAL;}
 void onBeforeTransmit()override{state.tx=true;}
 void onAfterTransmit()override{state.tx=false;}
};
static Board board;static HardwareRNG rng;static ArduinoMillis ms;static Clock clock;
static SimpleMeshTables tables;static StaticPoolPacketManager pool(24);
static CustomSX1262 radio(new Module(BOARD_LORA_CS,BOARD_LORA_INT,BOARD_LORA_RST,BOARD_LORA_BUSY,SPI));
static CustomSX1262Wrapper driver(radio,board);
static TransportKeyStore scopeStore;
static String packetScope(const mesh::Packet *pkt){
 if(!pkt->hasTransportCodes())return "*";
 // One key per known scope, derived once (knownScopes only grows); the store's own cache holds 16 of up to 64.
 static std::vector<TransportKey> scopeKeys;while(scopeKeys.size()<knownScopes.size()){TransportKey k;String hashtag="#"+knownScopes[scopeKeys.size()];scopeStore.getAutoKeyFor(scopeKeys.size()+1,hashtag.c_str(),k);scopeKeys.push_back(k);}
 String found;for(unsigned i=0;i<knownScopes.size();i++){TransportKey &k=scopeKeys[i];uint16_t code=k.calcTransportCode(pkt);if(code==pkt->transport_codes[0]||code==pkt->transport_codes[1]){if(found.length()&&found!=knownScopes[i])return "unknown";found=knownScopes[i];}}
 return found.length()?found:"unknown";
}
struct Pending {bool active=false;uint32_t token=0,ack=0,deadline=0,timeout=0;bool transmitted=false;uint8_t key[32]={0};};
static Pending pending;
struct TrackedTX {mesh::Packet *packet=nullptr;uint32_t token=0;bool direct=false;};
static TrackedTX outgoing[16];
struct EchoTrack {bool active=false;uint32_t token=0,seenAt=0;uint8_t hash[MAX_HASH_SIZE]={};RepeaterObserver repeaters;};
static EchoTrack echoes[16];static unsigned nextEcho=0;
static String receivedPath(const mesh::Packet *p){if(!p->isRouteFlood())return "direct";String out="flood:";for(unsigned i=0;i<p->getPathHashCount();i++){if(i)out+=",";out+=hex(p->path+i*p->getPathHashSize(),p->getPathHashSize());}return out;}
static void observeEcho(mesh::Packet *p){if(!p->isRouteFlood())return;uint8_t hash[MAX_HASH_SIZE];p->calculatePacketHash(hash);for(auto &e:echoes)if(e.active&&millis()-e.seenAt<600000&&!memcmp(hash,e.hash,MAX_HASH_SIZE)){if(e.repeaters.observe(p->path,p->getPathHashCount(),p->getPathHashSize())){Delivery d;d.token=e.token;d.repeaters=e.repeaters.count;if(p->getPathHashCount())d.firstRepeater=hex(p->path,p->getPathHashSize());for(int i=0;i<e.repeaters.count;i++){if(i)d.heard+=",";d.heard+=hex(e.repeaters.hashes[i],e.repeaters.sizes[i]);}deliveredCallback(d);}break;}}

static void trackTX(uint32_t token,bool direct){for(auto &t:outgoing)if(!t.packet){t.packet=pool.getOutboundByIdx(pool.getOutboundTotal()-1);t.token=token;t.direct=direct;if(t.packet&&t.packet->isRouteFlood()){auto &e=echoes[nextEcho++%16];e=EchoTrack();e.active=true;e.token=token;e.seenAt=millis();t.packet->calculatePacketHash(e.hash);}break;}}
static void txFinished(mesh::Packet *packet,bool success){for(auto &t:outgoing)if(t.packet==packet){Delivery d;d.txComplete=true;d.txSuccess=success;d.token=t.token;d.state=success?(t.direct?"Sent / awaiting ACK":"Sent / no group ACK"):"TX failed";deliveredCallback(d);if(t.direct&&pending.active&&pending.token==t.token){pending.transmitted=success;pending.deadline=millis()+pending.timeout;if(!success)pending.active=false;}t.packet=nullptr;break;}}

// Companion app link (companion.inc, end of file): messages for the app's offline queue, ACKs of the app's sends.
namespace companion {void queueContact(const ContactInfo &from,mesh::Packet *pkt,uint32_t ts,const char *text);void queueChannel(int idx,mesh::Packet *pkt,uint32_t ts,const char *text);void ackReceived(const uint8_t *data);void logRaw(float snr,float rssi,const uint8_t *raw,int len);void contactResponse(const uint8_t *data,uint8_t len);void controlData(const mesh::Packet *packet,float snr,float rssi);}
class ChatMesh:public BaseChatMesh {
 int activeChannelIdx(const mesh::GroupChannel &ch){for(int i=0;i<CHAT_SLOTS;i++){ChannelDetails detail;if(getChannel(i,detail)&&detail.name[0]&&!memcmp(ch.secret,detail.channel.secret,sizeof(ch.secret)))return i;}return -1;}
 uint8_t lastHash[64][MAX_HASH_SIZE]={};unsigned nextHash=0;
 bool fresh(mesh::Packet *pkt){uint8_t h[MAX_HASH_SIZE];pkt->calculatePacketHash(h);for(auto &v:lastHash)if(!memcmp(h,v,MAX_HASH_SIZE))return false;memcpy(lastHash[nextHash++%64],h,MAX_HASH_SIZE);return true;}
 void deliver(const ContactInfo &from,mesh::Packet *pkt,uint32_t ts,const char *text){if(!fresh(pkt))return;companion::queueContact(from,pkt,ts,text);Received m;m.direct=true;m.key=hex(from.id.pub_key,32);m.sender=safe(from.name,31);m.body=safe(text);m.timestamp=ts;m.hops=pkt->isRouteFlood()?pkt->getPathHashCount():-1;m.scope=packetScope(pkt);m.path=receivedPath(pkt);receivedCallback(m);}
 void flood(mesh::Packet *pkt,uint32_t delay){if(currentScope.length()&&currentScope!="*"){TransportKey k;String tag="#"+currentScope;SHA256 sha;sha.update(tag.c_str(),tag.length());sha.finalize(k.key,sizeof(k.key));uint16_t codes[]={k.calcTransportCode(pkt),0};sendMatterFlood(*this,pkt,delay,codes);}else sendMatterFlood(*this,pkt,delay);}
protected:
 int searchChannelsByHash(const uint8_t *hash,mesh::GroupChannel dest[],int maxMatches)override{int n=0;for(int i=0;i<CHAT_SLOTS&&n<maxMatches;i++){ChannelDetails detail;if(getChannel(i,detail)&&detail.name[0]&&detail.channel.hash[0]==hash[0])dest[n++]=detail.channel;}return n;}
 void onPeerDataRecv(mesh::Packet *packet,uint8_t type,int sender,const uint8_t *secret,uint8_t *data,size_t len)override{String old=currentScope;currentScope=packetScope(packet);if(currentScope=="unknown")currentScope="*";BaseChatMesh::onPeerDataRecv(packet,type,sender,secret,data,len);currentScope=old;}
 void logRxRaw(float snr,float rssi,const uint8_t raw[],int len)override{companion::logRaw(snr,rssi,raw,len);}
 void logRx(mesh::Packet *packet,int len,float)override{state.rssi=driver.getLastRSSI();state.snr=driver.getLastSNR();observeEcho(packet);recordRx(packet,len);}
 void recordRx(mesh::Packet *packet,int len);
 void logTx(mesh::Packet *packet,int)override{txFinished(packet,true);}
 void logTxFail(mesh::Packet *packet,int)override{txFinished(packet,false);}
 bool allowPacketForward(const mesh::Packet*)override{return false;} // Companion, never a repeater.
 float getAirtimeBudgetFactor()const override{return 9.0f;} // TX <=10% airtime, conservative queue pacing.
 int calcRxDelay(float,uint32_t)const override{return 0;}
 void sendFloodScoped(const ContactInfo&,mesh::Packet *pkt,uint32_t delay=0)override{flood(pkt,delay);}
 void sendFloodScoped(const mesh::GroupChannel &ch,mesh::Packet *pkt,uint32_t delay=0)override{int idx=activeChannelIdx(ch);String old=currentScope;if(!explicitGroupScope)currentScope=idx>=0?channelScopes[idx]:"*";flood(pkt,delay);currentScope=old;}
 void onAdvertRecv(mesh::Packet *packet,const mesh::Identity &id,uint32_t timestamp,const uint8_t *data,size_t length)override{BaseChatMesh::onAdvertRecv(packet,id,timestamp,data,length);}
 void onDiscoveredContact(ContactInfo &c,bool,uint8_t pathLen,const uint8_t*)override{
  String key=hex(c.id.pub_key,32);CachedNode n;key.toCharArray(n.key,sizeof(n.key));safe(c.name,31).toCharArray(n.name,sizeof(n.name));n.type=c.type;n.seen=millis();n.hops=pathLen&63;n.rssi=driver.getLastRSSI();n.snr=driver.getLastSNR();n.live=true;n.verified=true;n.lat=c.gps_lat;n.lon=c.gps_lon;if(cacheNode(n)){contactsDirty=true;saveAt=millis();}
 }
 void onContactPathUpdated(const ContactInfo&)override{contactsDirty=true;saveAt=millis();}
 ContactInfo *processAck(const uint8_t *data)override{companion::ackReceived(data);uint32_t ack;memcpy(&ack,data,4);if(pending.active&&pending.ack==ack){auto contact=lookupContactByPubKey(pending.key,32);Delivery d;d.token=pending.token;d.state="Delivered";deliveredCallback(d);pending.active=false;return contact;}return checkConnectionsAck(data);}
 void onMessageRecv(const ContactInfo &c,mesh::Packet *p,uint32_t ts,const char *t)override{if(c.type==ADV_TYPE_REPEATER)return;deliver(c,p,ts,t);}// repeater CLI replies are not chat messages
 void onCommandDataRecv(const ContactInfo &c,mesh::Packet *p,uint32_t ts,const char *text)override{}
 void onCLICommandRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*,char *reply)override{reply[0]=0;} // Companion does not expose a remote CLI.
 void onSignedMessageRecv(const ContactInfo &c,mesh::Packet *p,uint32_t ts,const uint8_t*,const char *t)override{deliver(c,p,ts,t);}
 void onChannelMessageRecv(const mesh::GroupChannel &ch,mesh::Packet *p,uint32_t ts,const char *t)override{
  if(!fresh(p))return;int idx=activeChannelIdx(ch);ChannelDetails detail;if(idx<0||!getChannel(idx,detail)||!detail.name[0])return;
  companion::queueChannel(idx,p,ts,t);
  Received m;m.channelId=idx;m.channel=detail.name;m.scope=packetScope(p);m.timestamp=ts;m.hops=p->isRouteFlood()?p->getPathHashCount():-1;m.path=receivedPath(p);String content=safe(t);int colon=content.indexOf(": ");m.sender=colon>=0?content.substring(0,colon):"Unknown";m.body=colon>=0?content.substring(colon+2):content;receivedCallback(m);
 }
 uint32_t calcFloodTimeoutMillisFor(uint32_t air)const override{return 500+16*air;}
 uint32_t calcDirectTimeoutMillisFor(uint32_t air,uint8_t path)const override{return 500+(6*air+250)*((path&63)+1);}
 void onSendTimeout()override{if(pending.active&&pending.transmitted){Delivery d;d.token=pending.token;d.state="No ACK";deliveredCallback(d);pending.active=false;}}
 uint8_t onContactRequest(const ContactInfo&,uint32_t,const uint8_t*,uint8_t,uint8_t*)override{return 0;}
 void onContactResponse(const ContactInfo&,const uint8_t *data,uint8_t len)override{companion::contactResponse(data,len);}
 bool onPeerPathRecv(mesh::Packet *packet,int sender,const uint8_t *secret,uint8_t *path,uint8_t pathLen,uint8_t type,uint8_t *extra,uint8_t len)override{return BaseChatMesh::onPeerPathRecv(packet,sender,secret,path,pathLen,type,extra,len);}
 bool onContactPathRecv(ContactInfo &c,uint8_t *in,uint8_t inLen,uint8_t *out,uint8_t outLen,uint8_t type,uint8_t *extra,uint8_t len)override{if(c.type==ADV_TYPE_REPEATER){if(type==PAYLOAD_TYPE_ACK&&len>=4)processAck(extra);return false;}return BaseChatMesh::onContactPathRecv(c,in,inLen,out,outLen,type,extra,len);}// repeaters: keep the saved path, no automatic reciprocal path
 void onControlDataRecv(mesh::Packet *packet)override{companion::controlData(packet,driver.getLastSNR(),driver.getLastRSSI());}
public:
 ChatMesh():BaseChatMesh(driver,ms,rng,clock,pool,tables){}
 // Rows formatted into a 4 KB buffer and written in blocks (same TSV as before; was ~10 String concatenations per contact).
 bool saveContacts(){File f=storage->open("/mattermesh/contacts.tmp","w");if(!f)return false;
  // Buffers live in PSRAM only while saving (static ones cost 4.6 KB of internal RAM for good).
  std::unique_ptr<char,void(*)(void*)> buffers((char*)ps_malloc(4096+512),free);if(!buffers){f.close();return false;}char *block=buffers.get(),*row=block+4096;constexpr size_t blockSize=4096,rowSize=512;size_t used=0;bool ok=true;
  auto flush=[&](){if(used&&f.write(reinterpret_cast<const uint8_t*>(block),used)!=used)ok=false;used=0;};
  ContactInfo c;auto it=startContactsIterator();
  while(ok&&it.hasNext(this,c)){if(c.type==ADV_TYPE_NONE&&!c.name[0])continue;// nameless node added for an app anonymous request: not kept
   size_t n=0;
   for(int k=0;k<32;k++){row[n++]=mm::hexDigit(c.id.pub_key[k]>>4);row[n++]=mm::hexDigit(c.id.pub_key[k]);}row[n++]='\t';
   for(int k=0;k<31&&c.name[k];k++){char ch=c.name[k];row[n++]=(ch=='\t'||ch=='\r'||ch=='\n')?' ':ch;}row[n++]='\t';
   n+=snprintf(row+n,rowSize-n,"%u\t%u\t",unsigned(c.type),unsigned(c.out_path_len));
   if(c.out_path_len!=OUT_PATH_UNKNOWN){size_t len=(c.out_path_len&63)*((c.out_path_len>>6)+1);for(size_t k=0;k<len&&k<sizeof(c.out_path);k++){row[n++]=mm::hexDigit(c.out_path[k]>>4);row[n++]=mm::hexDigit(c.out_path[k]);}}
   n+=snprintf(row+n,rowSize-n,"\t%lu\t%ld\t%ld\n",(unsigned long)c.last_advert_timestamp,(long)c.gps_lat,(long)c.gps_lon);
   if(used+n>blockSize)flush();memcpy(block+used,row,n);used+=n;}
  flush();f.flush();f.close();if(!ok){storage->remove("/mattermesh/contacts.tmp");return false;}
  return device::sdReplace(*storage,"/mattermesh/contacts.tmp","/mattermesh/contacts.tsv","/mattermesh/contacts.bak");}
 void loadContacts(){File f=storage->open(device::sdSavedPath(*storage,"/mattermesh/contacts.tsv","/mattermesh/contacts.bak"));if(!f)return;while(f.available()){String row=f.readStringUntil('\n');row.trim();ContactInfo c={};if(!unhex(field(row,0),c.id.pub_key,32))continue;String name=field(row,1);name.toCharArray(c.name,sizeof(c.name));c.type=field(row,2).toInt();c.out_path_len=field(row,3).toInt();if(c.out_path_len!=OUT_PATH_UNKNOWN&&(!mesh::Packet::isValidPathLen(c.out_path_len)||!unhex(field(row,4),c.out_path,(c.out_path_len&63)*((c.out_path_len>>6)+1))))continue;c.last_advert_timestamp=field(row,5).toInt();c.gps_lat=field(row,6).toInt();c.gps_lon=field(row,7).toInt();c.shared_secret_valid=false;if(addContact(c)){CachedNode n;hex(c.id.pub_key,32).toCharArray(n.key,sizeof(n.key));String(c.name).toCharArray(n.name,sizeof(n.name));n.type=c.type;n.lat=c.gps_lat;n.lon=c.gps_lon;cacheNode(n);}}f.close();}
};
static ChatMesh *meshPtr=nullptr;
static ChatMesh &chatMesh(){return *meshPtr;}
static bool valid(const Config &c){return validRadioParameters(c.frequency,c.bandwidth,c.sf,c.cr,c.power)&&validCompanionName(c.name.c_str())&&validPathHashBytes(c.pathBytes);}
static bool verifyIdentity(){const uint8_t challenge[]={0x4d,0x61,0x74,0x74,0x65,0x72};uint8_t signature[64];chatMesh().self_id.sign(signature,challenge,sizeof(challenge));return chatMesh().self_id.verify(signature,challenge,sizeof(challenge));}
// RF power path as the stock LilyGO T-Deck Max firmware (hardware/examples/factory/peri_lora.cpp):
// TCXO 2.4 V, current limit 140 mA, DIO2 RF switch, DC-DC regulator (user request 2026-10-19, "usa lo
// stock tdeck max per la potenza"). Default RX gain and no AGC reset are kept: the 0.4.1.14/16 combination
// of 2.4 V + AGC reset + boosted gain left the companion "mute" to repeaters; the 1.6 V MeshCore value was
// an interim choice. Sync word, preamble and CRC stay MeshCore (protocol, not power).
static constexpr float tcxoVoltage=2.4f;
static bool initRadio(){state.error=radio.begin(state.config.frequency,state.config.bandwidth,state.config.sf,state.config.cr,RADIOLIB_SX126X_SYNC_WORD_PRIVATE,state.config.power,16,tcxoVoltage,false);if(state.error)return false;state.error=radio.setCRC(1);if(state.error)return false;state.error=radio.setCurrentLimit(140);if(state.error)return false;state.error=radio.setDio2AsRfSwitch(true);if(state.error)return false;driver.setParams(state.config.frequency,state.config.bandwidth,state.config.sf,state.config.cr);driver.begin();state.error=radio.startReceive();return state.error==0;}
bool begin(fs::FS &fs,Receiver receiver,Delivered delivered){storage=&fs;receivedCallback=receiver;deliveredCallback=delivered;if(!meshPtr){void *memory=heap_caps_malloc(sizeof(ChatMesh),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);void *cache=heap_caps_malloc(sizeof(CachedNode)*NODE_CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!memory||!cache){heap_caps_free(memory);heap_caps_free(cache);state.error=-1004;return false;}meshPtr=new(memory) ChatMesh();discovered=static_cast<CachedNode*>(cache);discoveredCount=0;if(!rxPackets.ready()){void *slots=heap_caps_malloc(sizeof(RxPacket)*RX_LOG_CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(slots)rxPackets.attach(static_cast<RxPacket*>(slots),RX_LOG_CAPACITY);}}
 // Single fixed antenna on this board (no XL9555 antenna switch, unlike the T-Deck Max).
 antennaPrefs.begin("mmantenna",false);state.internal=true;
 pinMode(BOARD_LORA_EN,OUTPUT);digitalWrite(BOARD_LORA_EN,HIGH);delay(30);
 fs.mkdir("/mattermesh");File f=fs.open(device::sdSavedPath(fs,"/mattermesh/radio.conf","/mattermesh/radio.bak"));if(f){Config cfg;String row=f.readStringUntil('\n');cfg.frequency=field(row,0).toFloat();cfg.bandwidth=field(row,1).toFloat();cfg.sf=field(row,2).toInt();cfg.cr=field(row,3).toInt();cfg.power=field(row,4).toInt();cfg.name=field(row,5);cfg.name.trim();int bytes;bool widthValid=parseSavedPathBytes(field(row,6).c_str(),bytes);if(widthValid)cfg.pathBytes=bytes;if(widthValid&&valid(cfg))state.config=cfg;f.close();}setOutgoingPathHashBytes(state.config.pathBytes);
 clock.init(fs);File id=fs.open("/mattermesh/identity.id");if(id){state.identity=id.size()==96&&chatMesh().self_id.readFrom(id)&&verifyIdentity();id.close();if(!state.identity){state.error=-1002;return false;}}else{for(int n=0;n<10;n++){chatMesh().self_id=mesh::LocalIdentity(&rng);if(chatMesh().self_id.pub_key[0]!=0&&chatMesh().self_id.pub_key[0]!=255)break;}File out=fs.open("/mattermesh/identity.tmp","w");state.identity=out&&chatMesh().self_id.writeTo(out)&&verifyIdentity();if(out){out.flush();out.close();}state.identity=state.identity&&fs.rename("/mattermesh/identity.tmp","/mattermesh/identity.id");if(!state.identity){state.error=-1003;return false;}}
 if(!initRadio())return false;chatMesh().begin();chatMesh().loadContacts();state.ready=true;return true;
}
uint32_t nodesRevision(){return nodeChanges;}
void loop(){if(!state.ready)return;chatMesh().loop();driver.loop();if(pending.active&&pending.transmitted&&int32_t(millis()-pending.deadline)>0){Delivery d;d.token=pending.token;d.state="No ACK";deliveredCallback(d);pending.active=false;}if(contactsDirty&&millis()-lastContactsAttempt>=3000&&(millis()-saveAt>3000||millis()-lastContactsSave>=120000)&&!busy()){lastContactsAttempt=millis();if(chatMesh().saveContacts()){lastContactsSave=millis();contactsDirty=false;}}}
RadioLevels radioLevels(){RadioLevels l;if(!state.ready)return l;l.noiseFloor=driver.getNoiseFloor();if(auto p=rxPackets.newest(0)){l.received=true;l.rssi=p->rssi();l.snr=p->snr();}return l;}
RadioDiag radioDiag(){RadioDiag d;d.tcxo=tcxoVoltage;if(state.ready){d.boosted=radio.getRxBoostedGainMode();d.deviceErrors=radio.getDeviceErrors();}d.noiseFloor=driver.getNoiseFloor();d.received=driver.getPacketsRecv();d.recvErrors=driver.getPacketsRecvErrors();if(d.received){d.rssi=driver.getLastRSSI();d.snr=driver.getLastSNR();}return d;}
bool radioClearErrors(){return state.ready&&radio.clearDeviceErrors()==RADIOLIB_ERR_NONE;}
RadioCounters radioCounters(){RadioCounters c;c.ready=state.ready;c.received=driver.getPacketsRecv();c.sent=driver.getPacketsSent();return c;}// no Config copy (the node name String)
Snapshot snapshot(){state.nodes=meshPtr?chatMesh().getNumContacts():0;state.received=driver.getPacketsRecv();state.sent=driver.getPacketsSent();state.errors=driver.getPacketsRecvErrors();state.rx=state.ready&&driver.isInRecvMode();state.queued=pool.getOutboundTotal();return state;} // rssi/snr are captured in logRx, no SPI reads per poll
static bool matchesFilter(const CachedNode &n,int filter){return filter==0||(filter==1&&n.type==ADV_TYPE_CHAT)||(filter==2&&n.type==ADV_TYPE_REPEATER);}
int nodeCount(int filter){int count=0;for(int i=0;i<discoveredCount;i++)if(matchesFilter(discovered[i],filter))count++;return count;}
ContactPage contacts(int filter,const std::string &query,int offset,int limit){ContactPage page;std::vector<int> idx;idx.reserve(discoveredCount);for(int i=0;i<discoveredCount;i++){auto &n=discovered[i];if(!matchesFilter(n,filter)||!mm::contactMatches(n.name,query))continue;idx.push_back(i);if(n.live)page.live++;}
 uint32_t now=millis();std::stable_sort(idx.begin(),idx.end(),[&](int a,int b){auto &x=discovered[a],&y=discovered[b];if(x.live!=y.live)return x.live;if(x.live)return uint32_t(now-x.seen)<uint32_t(now-y.seen);return strcasecmp(x.name,y.name)<0;});
 page.total=idx.size();for(int i=std::max(0,offset);i<int(idx.size())&&int(page.rows.size())<limit;i++){auto &n=discovered[idx[i]];Node node;node.key=n.key;node.name=n.name;node.type=n.type;node.seen=n.seen;node.hops=n.hops;node.rssi=n.rssi;node.snr=n.snr;node.live=n.live;node.lat=n.lat;node.lon=n.lon;page.rows.push_back(node);}return page;}
int contactsCount(int filter,const std::string &query){int n=0;for(int i=0;i<discoveredCount;i++){auto &c=discovered[i];if(matchesFilter(c,filter)&&mm::contactMatches(c.name,query))n++;}return n;}
std::vector<Node> nodes(int filter,int offset,int limit){std::vector<Node> out;if(offset<0||limit<=0)return out;int index=0;for(int i=0;i<discoveredCount&&int(out.size())<limit;i++){auto &n=discovered[i];if(!matchesFilter(n,filter)||index++<offset)continue;Node node;node.key=n.key;node.name=n.name;node.type=n.type;node.seen=n.seen;node.hops=n.hops;node.rssi=n.rssi;node.snr=n.snr;node.live=n.live;node.lat=n.lat;node.lon=n.lon;out.push_back(node);}return out;}
// Map pins: nodes whose advert carried a position (MeshCore 1e-6 degrees) inside the box.
int nodePins(double south,double west,double north,double east,NodePin *out,int capacity){int count=0;for(int i=0;i<discoveredCount&&count<capacity;i++){auto &n=discovered[i];if(!n.lat&&!n.lon)continue;double lat=n.lat/1e6,lon=n.lon/1e6;if(lat<south||lat>north||lon<west||lon>east)continue;auto &p=out[count++];p.lat=lat;p.lon=lon;p.repeater=n.type==ADV_TYPE_REPEATER;strlcpy(p.name,n.name,sizeof(p.name));}return count;}
// Route on map: repeaters matching a path hash prefix, or nodes with an exact name.
// `matches` counts every candidate; only those with a position are returned.
static int nodePinsWhere(bool (*match)(const CachedNode&,const String&),const String &value,NodePin *out,int capacity,int &matches){matches=0;int count=0;for(int i=0;i<discoveredCount;i++){auto &n=discovered[i];if(!match(n,value))continue;matches++;if((!n.lat&&!n.lon)||count>=capacity)continue;auto &p=out[count++];p.lat=n.lat/1e6;p.lon=n.lon/1e6;p.repeater=n.type==ADV_TYPE_REPEATER;strlcpy(p.name,n.name,sizeof(p.name));}return count;}
int repeaterPinsForPrefix(const String &prefix,NodePin *out,int capacity,int &matches){if(!prefix.length()||prefix.length()>64){matches=0;return 0;}return nodePinsWhere([](const CachedNode &n,const String &v){return n.type==ADV_TYPE_REPEATER&&!strncasecmp(n.key,v.c_str(),v.length());},prefix,out,capacity,matches);}
int nodePinsForName(const String &name,NodePin *out,int capacity,int &matches){if(!name.length()){matches=0;return 0;}return nodePinsWhere([](const CachedNode &n,const String &v){return n.type!=ADV_TYPE_REPEATER&&v==n.name;},name,out,capacity,matches);}
String nodeNameForPrefix(const String &prefix,int &matches,int type){matches=0;String name;if(!prefix.length()||prefix.length()>64)return name;for(int i=0;i<discoveredCount;i++)if((!type||discovered[i].type==type)&&!strncmp(discovered[i].key,prefix.c_str(),prefix.length())){name=discovered[i].name;matches++;}return name;}

bool busy(){return state.ready&&(state.tx||pool.getOutboundTotal()>0||driver.isReceiving());}
// This board has a single fixed antenna (no XL9555 antenna switch, unlike the T-Deck Max): always "internal".
bool antennaSavedInternal(){return true;}
int antennaSelectLevel(){return -1;}
bool antenna(bool internal,String &reason,bool){
 if(!internal){reason="External antenna not available on this board";return false;}
 reason="Internal antenna (fixed)";return true;
}

bool configure(const Config &cfg,String &reason){
 if(!valid(cfg)){reason="Invalid radio settings";return false;}
 if(!state.ready||busy()){reason="Radio unavailable or busy";return false;}
 Config old=state.config;bool rfChanged=old.frequency!=cfg.frequency||old.bandwidth!=cfg.bandwidth||old.sf!=cfg.sf||old.cr!=cfg.cr||old.power!=cfg.power;
 if(rfChanged){radio.standby();state.config=cfg;if(!initRadio()){state.config=old;state.ready=initRadio();reason="Radio settings rejected";return false;}}
 File f=storage->open("/mattermesh/radio.tmp","w");
 String row=String(cfg.frequency,3)+"\t"+String(cfg.bandwidth,2)+"\t"+String(cfg.sf)+"\t"+String(cfg.cr)+"\t"+String(cfg.power)+"\t"+cfg.name+"\t"+String(cfg.pathBytes)+"\n";
 bool ok=f&&f.print(row)==row.length();if(f){f.flush();f.close();}
 ok=ok&&device::sdReplace(*storage,"/mattermesh/radio.tmp","/mattermesh/radio.conf","/mattermesh/radio.bak");
 if(!ok){storage->remove("/mattermesh/radio.tmp");state.config=old;if(rfChanged)state.ready=initRadio();reason="SD settings save failed";return false;}
 state.config=cfg;setOutgoingPathHashBytes(cfg.pathBytes);reason="LoRa settings saved";return true;
}
bool registerScope(const String &scope){if(!scope.length()||scope=="*"||scope=="unknown")return true;for(auto &value:knownScopes)if(value==scope)return true;if(knownScopes.size()>=64)return false;knownScopes.push_back(scope);return true;}
bool contactPath(const String &key,uint8_t &encoded,uint8_t *bytes,size_t capacity){uint8_t pub[32];if(!meshPtr||!unhex(key,pub,32))return false;auto c=chatMesh().lookupContactByPubKey(pub,32);if(!c)return false;encoded=c->out_path_len;size_t n=encoded==OUT_PATH_UNKNOWN?0:(encoded&63)*((encoded>>6)+1);if(n>capacity||n>sizeof(c->out_path))return false;memcpy(bytes,c->out_path,n);return true;}
bool channelSecret(int id,String &out){ChannelDetails ch;if(!meshPtr||id<0||id>=CHAT_SLOTS||!chatMesh().getChannel(id,ch)||!ch.name[0])return false;out=hex(ch.channel.secret,16);return true;}
void clearChannels(){if(!meshPtr)return;ChannelDetails empty={};for(int i=0;i<CHAT_SLOTS;i++){chatMesh().setChannel(i,empty);channelScopes[i]="*";}}
bool channel(int id,const String &name,const String &key,const String &scope,String &reason){if(!meshPtr){reason="Mesh memory unavailable";return false;}if(id<0||id>=CHAT_SLOTS){reason="Channel index invalid";return false;}ChannelDetails ch={};if(name.length()>31){reason="Channel name too long";return false;}name.toCharArray(ch.name,sizeof(ch.name));if(key.length()){size_t n=key.length()/2;if((n!=16&&n!=32)||!unhex(key,ch.channel.secret,n)){reason="Use a 16/32-byte hex key";return false;}}else if(name=="Public"){const uint8_t publicKey[]={0x8b,0x33,0x87,0xe9,0xc5,0xcd,0xea,0x6a,0xc9,0xe5,0xed,0xba,0xa1,0x15,0xcd,0x72};memcpy(ch.channel.secret,publicKey,16);}else if(name.startsWith("#")){SHA256 sha;sha.update(name.c_str(),name.length());sha.finalize(ch.channel.secret,16);}else{reason="Channel key required";return false;}chatMesh().setChannel(id,ch);channelScopes[id]=scope;registerScope(scope);reason="Channel ready";return true;}
size_t textLimit(bool direct){return messageBodyLimit(direct,state.config.name.length());}
bool send(int id,const String &peer,const String &scope,const String &text,uint32_t token,String &reason){if(text.length()>textLimit(peer.length()!=0)){reason=messageLimitError(textLimit(peer.length()!=0)).c_str();return false;}if(!state.ready){reason="Radio offline";return false;}if(pool.getOutboundTotal()>8){reason="TX queue full";return false;}uint32_t timestamp=clock.getCurrentTimeUnique();
 // The scope applies to this send only: app (companion) sends and later floods must not inherit it.
 struct ScopeGuard {String old;~ScopeGuard(){currentScope=old;}} guard{currentScope};currentScope=scope;if(peer.length()){if(pending.active){reason="Waiting for ACK";return false;}uint8_t key[32];if(!unhex(peer,key,32)){reason="Invalid contact key";return false;}auto c=chatMesh().lookupContactByPubKey(key,32);if(!c){reason="Contact not discovered";return false;}uint32_t ack,timeout;int result=chatMesh().sendMessage(*c,timestamp,0,text.c_str(),ack,timeout);if(!result){reason="TX queue full";return false;}pending.active=true;pending.token=token;pending.ack=ack;pending.timeout=timeout;pending.transmitted=false;trackTX(token,true);memcpy(pending.key,key,32);reason="Queued / awaiting ACK";}
 else{ChannelDetails ch;if(id<0||!chatMesh().getChannel(id,ch)||!ch.name[0]){reason="Channel key required";return false;}explicitGroupScope=true;bool queued=chatMesh().sendGroupMessage(timestamp,ch.channel,state.config.name.c_str(),text.c_str(),text.length());explicitGroupScope=false;if(!queued){reason="TX queue full";return false;}trackTX(token,false);reason="Queued / no group ACK";}
 return true;
}
bool advertise(String &reason){if(!state.ready){reason="Radio offline";return false;}if(pool.getOutboundTotal()>8){reason="TX queue full";return false;}auto pkt=chatMesh().createSelfAdvert(state.config.name.c_str());if(!pkt){reason="TX queue full";return false;}sendMatterFlood(chatMesh(),pkt);reason="Advert queued";return true;}
bool syncClock(uint32_t epoch,String &reason){if(epoch<MM_EPOCH_MIN||epoch>MM_EPOCH_END){reason="Invalid UTC epoch";return false;}setenv("TZ",MM_TZ,1);tzset();clock.setCurrentTime(epoch);File f=storage?storage->open("/mattermesh/protocol-clock","w"):File();if(f){f.println(epoch);f.flush();f.close();}reason="Clock synchronized / USB";return true;}
String ownKey(){return state.identity?hex(chatMesh().self_id.pub_key,32):"";}
void ChatMesh::recordRx(mesh::Packet *packet,int len){
 RxPacket p;time_t now=time(nullptr);p.epoch=now>=MM_EPOCH_MIN?uint32_t(now):0;p.uptimeMs=millis();p.header=packet->header;p.pathLen=uint8_t(packet->path_len);
 memcpy(p.path,packet->path,std::min<int>(packet->getPathByteLen(),RX_LOG_PATH_BYTES));p.length=len;p.payloadLength=packet->payload_len;
 memcpy(p.payload,packet->payload,std::min<int>(packet->payload_len,RX_LOG_PAYLOAD_HEAD));if(packet->hasTransportCodes()){p.transport[0]=packet->transport_codes[0];p.transport[1]=packet->transport_codes[1];}
 uint8_t hash[MAX_HASH_SIZE];packet->calculatePacketHash(hash);memcpy(p.hash,hash,sizeof(p.hash));
 p.rssiTenths=int16_t(lroundf(driver.getLastRSSI()*10));p.snrTenths=int16_t(lroundf(packet->getSNR()*10));rxPackets.push(p);
}
const RxLog &rxLog(){return rxPackets;}
#include "companion.inc"
}
