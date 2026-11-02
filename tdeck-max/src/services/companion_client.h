#pragma once
// Companion link, protocol side: the T-Deck acts as the app of another MeshCore companion (e.g. over BLE) and uses
// its radio. Pure logic over byte frames (one frame = one BLE write / notify), no transport and no Arduino types, so
// the host tests drive it with a scripted companion. Frame layouts follow meshcore-base/examples/companion_radio.
//
// Flow after the transport connects: DEVICE_QUERY -> DEVICE_INFO, APP_START -> SELF_INFO, GET_CONTACTS -> listing,
// GET_CHANNEL 0..n-1, then Ready: messages waiting are pulled with SYNC_NEXT_MESSAGE (also on PUSH_MSG_WAITING).
// One request is in flight at a time (the companion answers in order); pushes may arrive in between.
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <deque>
namespace remote {
enum : uint8_t {
 CMD_APP_START=1,CMD_SEND_TXT_MSG=2,CMD_SEND_CHANNEL_TXT_MSG=3,CMD_GET_CONTACTS=4,CMD_GET_DEVICE_TIME=5,CMD_SET_DEVICE_TIME=6,CMD_SEND_SELF_ADVERT=7,
 CMD_SET_ADVERT_NAME=8,CMD_SYNC_NEXT_MESSAGE=10,CMD_GET_BATT_AND_STORAGE=20,CMD_DEVICE_QUERY=22,CMD_GET_CHANNEL=31,
 CMD_SET_PATH_HASH_MODE=61,CMD_SET_DEFAULT_FLOOD_SCOPE=63,CMD_GET_DEFAULT_FLOOD_SCOPE=64,
 RESP_OK=0,RESP_ERR=1,RESP_CONTACTS_START=2,RESP_CONTACT=3,RESP_END_OF_CONTACTS=4,RESP_SELF_INFO=5,RESP_SENT=6,
 RESP_CONTACT_MSG_RECV=7,RESP_CHANNEL_MSG_RECV=8,RESP_CURR_TIME=9,RESP_NO_MORE_MESSAGES=10,RESP_BATT_AND_STORAGE=12,RESP_DEVICE_INFO=13,
 RESP_CONTACT_MSG_RECV_V3=16,RESP_CHANNEL_MSG_RECV_V3=17,RESP_CHANNEL_INFO=18,RESP_CHANNEL_DATA_RECV=27,RESP_DEFAULT_FLOOD_SCOPE=28,
 PUSH_ADVERT=0x80,PUSH_SEND_CONFIRMED=0x82,PUSH_MSG_WAITING=0x83,PUSH_NEW_ADVERT=0x8A,PUSH_CONTACT_DELETED=0x8F,
 TXT_PLAIN=0,TXT_SIGNED=2,AppVersion=3};
constexpr size_t KeySize=32,PathMax=64,FrameMax=176;// MeshCore MAX_FRAME_SIZE
constexpr uint32_t RequestTimeoutMs=8000,AckMarginMs=5000,AckDefaultMs=30000;
enum class Phase {Idle,Query,Start,Contacts,Channels,Ready};
struct Contact {uint8_t key[KeySize]={};std::string name;uint8_t type=0;};
struct Channel {int index=-1;std::string name;uint8_t secret[16]={};};
// A text the companion received, already split for the chat: channel messages carry "sender: body"; a DM carries the
// sender's full key when the companion knows the contact (keyKnown), else only its 6-byte prefix.
struct Message {bool direct=false,keyKnown=false;int channel=-1;uint8_t prefix[6]={},key[KeySize]={};std::string sender,body;uint32_t timestamp=0;int hops=-1;float snr=0;};
// Outcome of a send: Sent (taken by the companion / flooded), Delivered (DM acknowledged), NoAck (a DM's ACK never
// came within the companion's estimate), Failed (refused, request timeout, link lost).
enum class Outcome {Sent,Delivered,NoAck,Failed};
struct Delivery {uint32_t token=0;Outcome outcome=Outcome::Sent;};
inline Delivery delivery(uint32_t token,Outcome outcome){Delivery d;d.token=token;d.outcome=outcome;return d;}/* C++11: no brace init of this struct */
struct Info {std::string name,model,version;uint8_t protocol=0,maxChannels=0,pathMode=0;bool pathModeKnown=false;
 int8_t power=0,maxPower=0;uint32_t frequencyKhz=0,bandwidthHz=0;uint8_t sf=0,cr=0;uint8_t key[KeySize]={};
 uint16_t batteryMv=0;std::string scope;bool scopeKnown=false;uint32_t clock=0,clockAt=0;};
using Writer=void(*)(void *ctx,const uint8_t *frame,size_t len);
class Client {
 struct Request {std::vector<uint8_t> frame;uint32_t token=0;bool dm=false;std::string text;uint32_t value=0;};/* text / value: a setting shown on its OK */
 struct Ack {uint32_t ack=0,token=0,deadline=0;};
 Writer write=nullptr;void *ctx=nullptr;std::deque<Request> pending;bool inFlight=false;Request current;uint32_t sentAt=0;
 std::vector<Ack> acks;// companion ACK code -> our token, until PUSH_SEND_CONFIRMED or the deadline
 bool syncing=false,syncAgain=false,refreshing=false,refreshAgain=false,lateErr=false;int nextChannel=0,timeoutsInRow=0;
 uint32_t contactsSince=0,listingMax=0;/* lastmod of the newest contact of the last complete listing / of the one running */
 std::vector<Message> inbox,unresolved;std::vector<Delivery> outcomes;
 static uint32_t u32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
 static std::string text(const uint8_t *p,size_t n){size_t k=0;while(k<n&&p[k])k++;return std::string(reinterpret_cast<const char*>(p),k);}
 void transmit(const std::vector<uint8_t> &f,uint32_t now){if(write)write(ctx,f.data(),f.size());sentAt=now;inFlight=true;}
 void queue(std::vector<uint8_t> f,uint32_t now,uint32_t token=0,bool dm=false,const std::string &text=std::string(),uint32_t value=0){Request r;r.frame=std::move(f);r.token=token;r.dm=dm;r.text=text;r.value=value;pending.push_back(r);pump(now);}
 void pump(uint32_t now){if(inFlight||pending.empty())return;current=pending.front();pending.pop_front();transmit(current.frame,now);}
 void done(uint32_t now){inFlight=false;current=Request();pump(now);}
 void fail(Request &r){if(r.token)outcomes.push_back(delivery(r.token,Outcome::Failed));}
 void sync(uint32_t now){if(phase!=Phase::Ready||!syncAllowed)return;if(syncing){syncAgain=true;return;}syncing=true;queue({CMD_SYNC_NEXT_MESSAGE},now);}
 // Whether code answers the request sent (a late answer to a timed-out request must not complete the next one).
 static bool answers(uint8_t sent,uint8_t code){
  if(code==RESP_ERR)return true;
  switch(sent){case CMD_DEVICE_QUERY:return code==RESP_DEVICE_INFO;case CMD_APP_START:return code==RESP_SELF_INFO;
  case CMD_GET_CONTACTS:return code==RESP_CONTACTS_START||code==RESP_CONTACT||code==RESP_END_OF_CONTACTS;case CMD_GET_CHANNEL:return code==RESP_CHANNEL_INFO;
  case CMD_SEND_TXT_MSG:return code==RESP_SENT;case CMD_GET_DEVICE_TIME:return code==RESP_CURR_TIME;case CMD_GET_BATT_AND_STORAGE:return code==RESP_BATT_AND_STORAGE;
  case CMD_GET_DEFAULT_FLOOD_SCOPE:return code==RESP_DEFAULT_FLOOD_SCOPE;
  case CMD_SYNC_NEXT_MESSAGE:return code==RESP_CONTACT_MSG_RECV||code==RESP_CONTACT_MSG_RECV_V3||code==RESP_CHANNEL_MSG_RECV||code==RESP_CHANNEL_MSG_RECV_V3||code==RESP_NO_MORE_MESSAGES||code==RESP_CHANNEL_DATA_RECV;
  default:return code==RESP_OK;}}
 // MeshCore lists contacts with lastmod > since: one second back so changes in the same second are not missed (the list
 // de-duplicates). A refresh asked while one runs (or during the handshake) runs once that one ends.
 void refreshContacts(uint32_t now){if(refreshing||phase!=Phase::Ready){refreshAgain=true;return;}refreshing=true;listingMax=contactsSince;uint32_t since=contactsSince?contactsSince-1:0;
  std::vector<uint8_t> f={CMD_GET_CONTACTS};f.resize(5);memcpy(f.data()+1,&since,4);queue(f,now);}
 // Whatever answers a SYNC (a message, a datagram, an error): the next one is asked for, so the sync never stalls.
 void answered(uint8_t sent,uint32_t now){done(now);if(sent==CMD_SYNC_NEXT_MESSAGE){syncing=false;sync(now);}}
 void requestChannel(uint32_t now){if(nextChannel>=info.maxChannels){phase=Phase::Ready;readyCount++;queue({CMD_GET_DEFAULT_FLOOD_SCOPE},now);queue({CMD_GET_DEVICE_TIME},now);sync(now);becameReady(now);return;}
  queue({CMD_GET_CHANNEL,uint8_t(nextChannel)},now);}
 void becameReady(uint32_t now){if(refreshAgain){refreshAgain=false;refreshContacts(now);}}
 bool contactFrame(const uint8_t *f,size_t n,Contact &c){if(n<1+KeySize+3+PathMax+32)return false;memcpy(c.key,f+1,KeySize);c.type=f[1+KeySize];c.name=text(f+1+KeySize+3+PathMax,32);return true;}
 void upsert(const Contact &c){for(auto &k:contacts)if(!memcmp(k.key,c.key,KeySize)){k=c;return;}contacts.push_back(c);}
 bool resolve(Message &m){for(auto &c:contacts)if(!memcmp(c.key,m.prefix,6)){memcpy(m.key,c.key,KeySize);m.keyKnown=true;if(m.sender.empty())m.sender=c.name;return true;}return false;}
 // DMs from a sender the companion added after the listing wait for a contact refresh (or a new advert push).
 void releaseUnresolved(bool all){for(size_t i=0;i<unresolved.size();){if(resolve(unresolved[i])||all){inbox.push_back(unresolved[i]);unresolved.erase(unresolved.begin()+i);}else i++;}}
 void endRefresh(uint32_t now,bool complete){refreshing=false;if(complete)contactsSince=listingMax;releaseUnresolved(true);if(refreshAgain){refreshAgain=false;refreshContacts(now);}}
 void message(const uint8_t *f,size_t n,bool v3,bool direct,uint32_t now){
  size_t i=1;Message m;m.direct=direct;if(v3){if(n<4)return;m.snr=int8_t(f[1])/4.0f;i=4;}
  if(direct){if(n<i+6+2+4)return;memcpy(m.prefix,f+i,6);i+=6;}else{if(n<i+1+2+4)return;m.channel=f[i++];}
  uint8_t path=f[i++],type=f[i++];m.hops=path==0xFF?-1:(path&63);m.timestamp=u32(f+i);i+=4;// path_len: hash size in the top bits
  std::string author;
  if(type==TXT_SIGNED&&direct){if(n<i+4)return;for(auto &c:contacts)if(!memcmp(c.key,f+i,4)){author=c.name;break;}i+=4;}// room post: the author's key prefix first
  else if(type!=TXT_PLAIN){skipped++;return;}
  std::string t(reinterpret_cast<const char*>(f+i),n-i);
  if(direct){m.body=t;bool known=resolve(m);if(!author.empty())m.sender=m.sender.empty()?author:m.sender+" / "+author;
   if(!known){unresolved.push_back(m);refreshContacts(now);return;}}
  else{size_t colon=t.find(": ");if(colon!=std::string::npos&&colon<32){m.sender=t.substr(0,colon);m.body=t.substr(colon+2);}else m.body=t;}
  inbox.push_back(std::move(m));}
public:
 Phase phase=Phase::Idle;Info info;std::vector<Contact> contacts;std::vector<Channel> channels;uint32_t readyCount=0,errors=0,timeouts=0,skipped=0,stray=0;/* stray: frames that answered no request in flight */
 bool reconnectWanted=false;
 void begin(Writer w,void *c){write=w;ctx=c;}
 // The transport is up: start the handshake. Down: drop everything in flight (queued sends fail, waiting DMs too).
 void connected(uint32_t now){reset();phase=Phase::Query;queue({CMD_DEVICE_QUERY,AppVersion},now);}
 void disconnected(){for(auto &r:pending)fail(r);if(inFlight)fail(current);for(auto &a:acks)outcomes.push_back(delivery(a.token,Outcome::NoAck));releaseUnresolved(true);reset();}
 void reset(){contactsSince=listingMax=0;timeoutsInRow=0;lateErr=refreshAgain=false;reconnectWanted=false;pending.clear();inFlight=false;current=Request();acks.clear();syncing=syncAgain=refreshing=false;nextChannel=0;phase=Phase::Idle;contacts.clear();channels.clear();info=Info();}
 bool ready()const{return phase==Phase::Ready;}
 // The companion's clock (epoch) now, 0 when unknown: used to stamp messages when the T-Deck has no time.
 uint32_t companionTime(uint32_t now)const{return info.clock?info.clock+(now-info.clockAt)/1000:0;}
 // A request unanswered for RequestTimeoutMs fails (its send is reported Failed) and the next one goes out.
 // During the handshake a timeout moves on (a contact listing cut short still leads to the channels; a channel slot
 // that never answers is skipped); a device query or app start that never answers asks for a reconnect.
 // A DM whose ACK did not come by its deadline is reported NoAck.
 void tick(uint32_t now){
  for(size_t i=0;i<acks.size();)if(int32_t(now-acks[i].deadline)>=0){outcomes.push_back(delivery(acks[i].token,Outcome::NoAck));acks.erase(acks.begin()+i);}else i++;
  if(!inFlight||now-sentAt<RequestTimeoutMs)return;timeouts++;lateErr=true;
  if(phase==Phase::Ready&&++timeoutsInRow>=3)reconnectWanted=true;/* the link looks up but nothing answers */
  const uint8_t cmd=current.frame.empty()?0:current.frame[0];fail(current);
  done(now);if(cmd==CMD_SYNC_NEXT_MESSAGE){syncing=false;sync(now);}/* a lost answer or MSG_WAITING: ask again */
  if(cmd==CMD_GET_CONTACTS&&phase==Phase::Contacts){phase=Phase::Channels;channels.clear();nextChannel=0;requestChannel(now);}
  else if(cmd==CMD_GET_CONTACTS)endRefresh(now,false);
  else if(cmd==CMD_GET_CHANNEL&&phase==Phase::Channels){nextChannel++;requestChannel(now);}
  else if((cmd==CMD_DEVICE_QUERY||cmd==CMD_APP_START)&&phase!=Phase::Ready)reconnectWanted=true;}
 // Periodic poll (the caller, every minute): pulls messages a dropped MSG_WAITING would leave waiting.
 void poll(uint32_t now){sync(now);}
 // Off: messages stay on the companion, nothing is pulled until allowSync(true).
 bool syncAllowed=true;void allowSync(bool on,uint32_t now){syncAllowed=on;if(on)sync(now);}
 // Taken by the caller after each frame / tick.
 bool takeMessage(Message &m){if(inbox.empty())return false;m=inbox.front();inbox.erase(inbox.begin());return true;}
 bool takeDelivery(Delivery &d){if(outcomes.empty())return false;d=outcomes.front();outcomes.erase(outcomes.begin());return true;}
 // Sends: the channel index is the companion's (see channels); a DM goes to a contact by its 6-byte key prefix.
 bool sendChannel(int index,const std::string &body,uint32_t timestamp,uint32_t token,uint32_t now){
  if(!ready()||index<0||index>255||body.empty()||body.size()>FrameMax-7)return false;
  std::vector<uint8_t> f={CMD_SEND_CHANNEL_TXT_MSG,TXT_PLAIN,uint8_t(index)};f.resize(7);memcpy(f.data()+3,&timestamp,4);f.insert(f.end(),body.begin(),body.end());queue(f,now,token);return true;}
 bool sendDirect(const uint8_t prefix[6],const std::string &body,uint32_t timestamp,uint32_t token,uint32_t now){
  if(!ready()||body.empty()||body.size()>FrameMax-13)return false;
  std::vector<uint8_t> f={CMD_SEND_TXT_MSG,TXT_PLAIN,0};f.resize(7);memcpy(f.data()+3,&timestamp,4);f.insert(f.end(),prefix,prefix+6);f.insert(f.end(),body.begin(),body.end());queue(f,now,token,true);return true;}
 // Settings of the companion (minimal management): name, path hash bytes 1..3, default region, advert, clock, battery.
 bool setName(const std::string &name,uint32_t now){if(!ready()||name.empty()||name.size()>31)return false;std::vector<uint8_t> f={CMD_SET_ADVERT_NAME};f.insert(f.end(),name.begin(),name.end());queue(f,now,0,false,name);return true;}
 bool setPathBytes(int bytes,uint32_t now){if(!ready()||bytes<1||bytes>3)return false;queue({CMD_SET_PATH_HASH_MODE,0,uint8_t(bytes-1)},now,0,false,std::string(),uint32_t(bytes-1));return true;}
 // Default flood scope: the region name without '#' and its 16-byte key (SHA-256 of "#name"); empty name clears it.
 bool setRegion(const std::string &name,const uint8_t key[16],uint32_t now){if(!ready()||name.size()>30)return false;std::vector<uint8_t> f={CMD_SET_DEFAULT_FLOOD_SCOPE};
  if(!name.empty()){f.resize(1+31+16,0);memcpy(f.data()+1,name.data(),name.size());memcpy(f.data()+32,key,16);}queue(f,now,0,false,name);return true;}
 bool advertise(uint32_t now){if(!ready())return false;queue({CMD_SEND_SELF_ADVERT,1},now);return true;}/* 1: flood */
 bool setTime(uint32_t epoch,uint32_t now){if(phase==Phase::Idle)return false;std::vector<uint8_t> f={CMD_SET_DEVICE_TIME};f.resize(5);memcpy(f.data()+1,&epoch,4);queue(f,now,0,false,std::string(),epoch);return true;}/* the clock counts once the companion takes it */
 bool refreshBattery(uint32_t now){if(!ready())return false;queue({CMD_GET_BATT_AND_STORAGE},now);return true;}
 const Channel *channelBySecret(const uint8_t secret[16])const{for(auto &c:channels)if(!memcmp(c.secret,secret,16))return &c;return nullptr;}
 const Contact *contactByPrefix(const uint8_t prefix[6])const{for(auto &c:contacts)if(!memcmp(c.key,prefix,6))return &c;return nullptr;}
 // One frame from the companion (a response to the request in flight, or a push).
 void frame(const uint8_t *f,size_t n,uint32_t now){
  if(!n)return;
  const uint8_t code=f[0];
  if(code==PUSH_MSG_WAITING){sync(now);return;}
  if(code==PUSH_SEND_CONFIRMED){if(n>=5){uint32_t ack=u32(f+1);for(size_t i=0;i<acks.size();i++)if(acks[i].ack==ack){outcomes.push_back(delivery(acks[i].token,Outcome::Delivered));acks.erase(acks.begin()+i);break;}}return;}
  // A new / changed advert: the companion may or may not have stored it (manual add, full storage), so the list is
  // read again from the last change instead of trusting the push.
  if(code==PUSH_NEW_ADVERT||code==PUSH_ADVERT){refreshContacts(now);return;}
  if(code==PUSH_CONTACT_DELETED){if(n>=1+KeySize)for(size_t i=0;i<contacts.size();i++)if(!memcmp(contacts[i].key,f+1,KeySize)){contacts.erase(contacts.begin()+i);break;}return;}
  if(code>=0x80)return;// other pushes (RX log, adverts, path updates): not used here
  const uint8_t sent=current.frame.empty()?0:current.frame[0];
  const bool isMessage=code==RESP_CONTACT_MSG_RECV||code==RESP_CONTACT_MSG_RECV_V3||code==RESP_CHANNEL_MSG_RECV||code==RESP_CHANNEL_MSG_RECV_V3;
  // An ERR right after a timeout belongs to the timed-out request, not to the one now in flight.
  if(code==RESP_ERR&&lateErr&&inFlight&&now-sentAt<200){lateErr=false;stray++;return;}
  if(!inFlight||!answers(sent,code)){stray++;/* a late answer: a message still reaches the chat, nothing else is taken */
   if(isMessage)message(f,n,code==RESP_CONTACT_MSG_RECV_V3||code==RESP_CHANNEL_MSG_RECV_V3,code==RESP_CONTACT_MSG_RECV||code==RESP_CONTACT_MSG_RECV_V3,now);return;}
  timeoutsInRow=0;lateErr=false;
  switch(code){
  case RESP_DEVICE_INFO:
   info.protocol=n>1?f[1]:0;info.maxChannels=n>3?f[3]:0;if(n>=60)info.model=text(f+20,40);if(n>=80)info.version=text(f+60,20);if(n>=82){info.pathMode=f[81];info.pathModeKnown=true;}
   done(now);phase=Phase::Start;{std::vector<uint8_t> s={CMD_APP_START,AppVersion,0,0,0,0,0,0};const char *app="MatterMesh";s.insert(s.end(),app,app+strlen(app));queue(s,now);}return;
  case RESP_SELF_INFO:
   if(n>=1+3+KeySize+8+4+8+2){size_t i=1;i++;info.power=int8_t(f[i++]);info.maxPower=int8_t(f[i++]);memcpy(info.key,f+i,KeySize);i+=KeySize;i+=8+4;
    info.frequencyKhz=u32(f+i);i+=4;info.bandwidthHz=u32(f+i);i+=4;info.sf=f[i++];info.cr=f[i++];info.name=std::string(reinterpret_cast<const char*>(f+i),n-i);}
   done(now);phase=Phase::Contacts;queue({CMD_GET_CONTACTS},now);return;
  case RESP_CONTACTS_START:if(phase==Phase::Contacts)contacts.clear();return;// the listing follows (a refresh only updates); in flight until END
  case RESP_CONTACT:{sentAt=now;/* a long listing keeps the request alive */Contact c;if(contactFrame(f,n,c))upsert(c);if(n>=148){uint32_t mod=u32(f+144);if(mod>listingMax)listingMax=mod;}return;}
  case RESP_END_OF_CONTACTS:if(n>=5&&u32(f+1)>listingMax)listingMax=u32(f+1);done(now);if(phase==Phase::Contacts){contactsSince=listingMax;phase=Phase::Channels;channels.clear();nextChannel=0;requestChannel(now);}else endRefresh(now,true);return;
  case RESP_CHANNEL_INFO:if(n>=2+32+16){Channel c;c.index=f[1];c.name=text(f+2,32);memcpy(c.secret,f+34,16);if(!c.name.empty())channels.push_back(c);}
   done(now);nextChannel++;requestChannel(now);return;
  case RESP_DEFAULT_FLOOD_SCOPE:info.scope=n>=32?text(f+1,31):"";info.scopeKnown=true;done(now);return;
  case RESP_CURR_TIME:if(n>=5&&!info.clock){info.clock=u32(f+1);info.clockAt=now;}done(now);return;
  case RESP_BATT_AND_STORAGE:if(n>=3){uint16_t mv;memcpy(&mv,f+1,2);info.batteryMv=mv;}done(now);return;
  case RESP_SENT:if(current.token){outcomes.push_back(delivery(current.token,Outcome::Sent));uint32_t ack=n>=6?u32(f+2):0,estimate=n>=10?u32(f+6):0;
    if(current.dm&&ack){Ack a;a.ack=ack;a.token=current.token;a.deadline=now+(estimate?estimate+AckMarginMs:AckDefaultMs);acks.push_back(a);if(acks.size()>16){outcomes.push_back(delivery(acks.front().token,Outcome::NoAck));acks.erase(acks.begin());}}}
   done(now);return;
  case RESP_CONTACT_MSG_RECV:case RESP_CONTACT_MSG_RECV_V3:case RESP_CHANNEL_MSG_RECV:case RESP_CHANNEL_MSG_RECV_V3:
   message(f,n,code==RESP_CONTACT_MSG_RECV_V3||code==RESP_CHANNEL_MSG_RECV_V3,code==RESP_CONTACT_MSG_RECV||code==RESP_CONTACT_MSG_RECV_V3,now);
   answered(sent,now);return;// one message per SYNC: ask for the next
  case RESP_NO_MORE_MESSAGES:done(now);syncing=false;if(syncAgain){syncAgain=false;sync(now);}return;
  case RESP_OK:if(current.token)outcomes.push_back(delivery(current.token,Outcome::Sent));
   if(sent==CMD_SET_ADVERT_NAME)info.name=current.text;else if(sent==CMD_SET_PATH_HASH_MODE){info.pathMode=uint8_t(current.value);info.pathModeKnown=true;}
   else if(sent==CMD_SET_DEFAULT_FLOOD_SCOPE){info.scope=current.text;info.scopeKnown=true;}else if(sent==CMD_SET_DEVICE_TIME){info.clock=current.value;info.clockAt=now;}
   done(now);return;
  case RESP_ERR:errors++;
   if(sent==CMD_GET_CHANNEL){done(now);nextChannel=info.maxChannels;requestChannel(now);return;}// past the companion's last slot
   if(sent==CMD_GET_DEFAULT_FLOOD_SCOPE){info.scopeKnown=false;done(now);return;}// older firmware: no default region
   if(sent==CMD_GET_CONTACTS){done(now);if(phase==Phase::Contacts){phase=Phase::Channels;channels.clear();nextChannel=0;requestChannel(now);}else endRefresh(now,false);return;}
   if(sent==CMD_DEVICE_QUERY||sent==CMD_APP_START){done(now);reconnectWanted=true;return;}
   fail(current);answered(sent,now);return;
  default:skipped++;answered(sent,now);return;// a channel datagram from the queue
  }
 }
};
}
