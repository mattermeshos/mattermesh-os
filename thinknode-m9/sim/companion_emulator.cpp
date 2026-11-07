// PC emulator for the MeshCore app's "Discover regions" against MatterMesh OS's companion code (src/mesh/companion_air.h).
// Real MeshCore core (Mesh, BaseChatMesh, packets, Ed25519/AES) on an in-memory radio: one companion node and two
// repeaters that answer like simple_repeater (zero-hop NODE_DISCOVER_RESP, ANON_REQ_TYPE_REGIONS reply), plus one
// repeater too far away to hear. The test plays the app: frame 55 (discover repeaters), collects the 0x8E pushes,
// frame 57 (regions request) per repeater, and checks the 0x8C pushes carry each repeater's region list.
#include <Arduino.h>
#include <helpers/BaseChatMesh.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/SimpleMeshTables.h>
#include "mesh/companion_air.h"
#include <cassert>
#include <deque>
#include <random>
#include <string>
#include <vector>
static unsigned long simMs=1000;unsigned long millis(){return simMs;}
namespace {
struct Ms:mesh::MillisecondClock {unsigned long getMillis()override{return simMs;}};
struct Rng:mesh::RNG {std::mt19937 g{12345};void random(uint8_t *d,size_t n)override{for(size_t i=0;i<n;i++)d[i]=uint8_t(g());}};
struct Clock:mesh::RTCClock {uint32_t t=1790000000;uint32_t getCurrentTime()override{return t+simMs/1000;}void setCurrentTime(uint32_t v)override{t=v;}};
// Broadcast radio: what a node sends reaches the radios it can hear.
struct Air;struct Radio:mesh::Radio {Air *air=nullptr;std::deque<std::vector<uint8_t>> in;std::vector<Radio*> hears;bool busy=false;
 int recvRaw(uint8_t *d,int n)override{if(in.empty())return 0;auto b=in.front();in.pop_front();if(int(b.size())>n)return 0;memcpy(d,b.data(),b.size());return int(b.size());}
 uint32_t getEstAirtimeFor(int len)override{return 20+len;}float packetScore(float,int)override{return 1;}
 bool startSendRaw(const uint8_t *b,int n)override{for(auto *r:hears)r->in.emplace_back(b,b+n);busy=true;return true;}
 bool isSendComplete()override{return busy;}void onSendFinished()override{busy=false;}bool isInRecvMode()const override{return true;}
 float getLastSNR()const override{return 9.5f;}float getLastRSSI()const override{return -71;}};
void link(Radio &a,Radio &b){a.hears.push_back(&b);b.hears.push_back(&a);}

// The phone app's side of the link: frames written by the companion.
struct App {std::vector<std::vector<uint8_t>> frames;void write(const uint8_t *f,size_t n){frames.emplace_back(f,f+n);}};

// Companion node: the same request / push code as the firmware (companion.inc calls these functions).
// Radio, clocks and tables built before the mesh base class, whose constructor already uses them.
struct Parts {Radio radio;Ms ms;Rng rng;Clock clock;StaticPoolPacketManager pool{16};SimpleMeshTables tables;};
class Companion:public Parts,public BaseChatMesh {
public:App app;companion_air::Pending pending;
 Companion():BaseChatMesh(radio,ms,rng,clock,pool,tables){rng.g.seed(1);self_id=mesh::LocalIdentity(&rng);begin();}
 // One app frame in, the direct reply frame out.
 void frame(const std::vector<uint8_t> &in){uint8_t out[16];size_t n=0;
  if(in[0]==companion_air::CMD_SEND_CONTROL_DATA)n=companion_air::sendControl(*this,in.data(),in.size(),out);
  else if(in[0]==companion_air::CMD_SEND_ANON_REQ)n=companion_air::sendAnon(*this,clock.getCurrentTime(),in.data(),in.size(),pending,out);
  app.write(out,n);}
protected:
 void onControlDataRecv(mesh::Packet *p)override{uint8_t f[200];size_t n=companion_air::controlFrame(p,radio.getLastSNR(),radio.getLastRSSI(),f,sizeof(f));if(n)app.write(f,n);}
 void onContactResponse(const ContactInfo&,const uint8_t *data,uint8_t len)override{uint8_t f[200];size_t n=companion_air::responseFrame(pending,data,len,f,sizeof(f));if(n)app.write(f,n);}
 void sendFloodScoped(const ContactInfo&,mesh::Packet *p,uint32_t delay=0)override{sendFlood(p,delay);}
 void sendFloodScoped(const mesh::GroupChannel&,mesh::Packet *p,uint32_t delay=0)override{sendFlood(p,delay);}
 float getAirtimeBudgetFactor()const override{return 0;}int calcRxDelay(float,uint32_t)const override{return 0;}bool allowPacketForward(const mesh::Packet*)override{return false;}
 void onDiscoveredContact(ContactInfo&,bool,uint8_t,const uint8_t*)override{}void onContactPathUpdated(const ContactInfo&)override{}ContactInfo *processAck(const uint8_t*)override{return nullptr;}
 void onMessageRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*)override{}void onCommandDataRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*)override{}
 void onCLICommandRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*,char *reply)override{reply[0]=0;}void onSignedMessageRecv(const ContactInfo&,mesh::Packet*,uint32_t,const uint8_t*,const char*)override{}
 void onChannelMessageRecv(const mesh::GroupChannel&,mesh::Packet*,uint32_t,const char*)override{}
 uint32_t calcFloodTimeoutMillisFor(uint32_t)const override{return 3000;}uint32_t calcDirectTimeoutMillisFor(uint32_t,uint8_t)const override{return 3000;}void onSendTimeout()override{}
 uint8_t onContactRequest(const ContactInfo&,uint32_t,const uint8_t*,uint8_t,uint8_t*)override{return 0;}
};

// Repeater: the discovery and anonymous regions handlers of meshcore-base/examples/simple_repeater/MyMesh.cpp.
class Repeater:public Parts,public mesh::Mesh {
public:std::string regions;
 Repeater(uint32_t seed,const char *names):mesh::Mesh(radio,ms,rng,clock,pool,tables),regions(names){rng.g.seed(seed);self_id=mesh::LocalIdentity(&rng);begin();}
protected:
 void onControlDataRecv(mesh::Packet *p)override{
  if((p->payload[0]&0xF0)!=0x80||p->payload_len<6)return;uint8_t filter=p->payload[1];uint32_t tag;memcpy(&tag,p->payload+2,4);
  if(!(filter&(1<<ADV_TYPE_REPEATER)))return;bool prefixOnly=p->payload[0]&1;
  uint8_t d[6+PUB_KEY_SIZE];d[0]=0x90|ADV_TYPE_REPEATER;d[1]=uint8_t(p->_snr);memcpy(d+2,&tag,4);memcpy(d+6,self_id.pub_key,PUB_KEY_SIZE);
  if(auto *r=createControlData(d,prefixOnly?6+8:6+PUB_KEY_SIZE))sendZeroHop(r,getRetransmitDelay(r)*4);}
 void onAnonDataRecv(mesh::Packet *p,const uint8_t *secret,const mesh::Identity &sender,uint8_t *data,size_t len)override{
  if(p->getPayloadType()!=PAYLOAD_TYPE_ANON_REQ||len<6||data[4]!=0x01||!p->isRouteDirect())return;// ANON_REQ_TYPE_REGIONS, direct only
  uint32_t ts;memcpy(&ts,data,4);uint8_t pathLen=data[5];if(!mesh::Packet::isValidPathLen(pathLen))return;uint8_t path[MAX_PATH_SIZE];mesh::Packet::writePath(path,data+6,pathLen);
  uint8_t reply[MAX_PACKET_PAYLOAD];memcpy(reply,&ts,4);uint32_t now=clock.getCurrentTime();memcpy(reply+4,&now,4);memcpy(reply+8,regions.data(),regions.size());
  if(auto *r=createDatagram(PAYLOAD_TYPE_RESPONSE,sender,secret,reply,8+regions.size()))sendDirect(r,path,pathLen,300);}
};
template<class... N> void run(int ms,N&... nodes){for(int t=0;t<ms;t+=5){simMs+=5;(nodes.loop(),...);}}
}

int main(){
 setvbuf(stdout,nullptr,_IONBF,0);
 auto *phone=new Companion();auto *north=new Repeater(2,"*,IT-VDA,IT-PIE,");auto *south=new Repeater(3,"*,IT-TOS,");auto *far=new Repeater(4,"*,CH-TI,");
 link(phone->radio,north->radio);link(phone->radio,south->radio);link(north->radio,far->radio);// far: two hops away, not zero-hop
 // 1. The app asks who is around: control NODE_DISCOVER_REQ for repeaters, full keys.
 std::vector<uint8_t> discover={companion_air::CMD_SEND_CONTROL_DATA,0x80,uint8_t(1<<ADV_TYPE_REPEATER),0x11,0x22,0x33,0x44};
 phone->frame(discover);assert(phone->app.frames.back()==std::vector<uint8_t>{companion_air::RESP_OK});
 run(4000,*phone,*north,*south,*far);
 std::vector<std::vector<uint8_t>> keys;
 for(auto &f:phone->app.frames)if(f[0]==companion_air::PUSH_CONTROL_DATA){assert(f.size()==4+6+PUB_KEY_SIZE&&(f[4]&0xF0)==0x90&&!memcmp(&f[6],&discover[3],4));keys.emplace_back(f.begin()+10,f.end());}
 printf("discover: %zu repeaters answered (expected 2)\n",keys.size());assert(keys.size()==2);
 // 2. The app asks each one for its regions: ANON_REQ {type 0x01}{reply path length 0}.
 std::vector<std::string> lists;
 for(auto &k:keys){size_t before=phone->app.frames.size();std::vector<uint8_t> req={companion_air::CMD_SEND_ANON_REQ};req.insert(req.end(),k.begin(),k.end());req.push_back(0x01);req.push_back(0);
  phone->frame(req);auto sent=phone->app.frames.back();assert(sent.size()==10&&sent[0]==companion_air::RESP_SENT&&sent[1]==0);uint32_t tag;memcpy(&tag,&sent[2],4);
  run(3000,*phone,*north,*south,*far);bool got=false;
  for(size_t i=before;i<phone->app.frames.size();i++){auto &f=phone->app.frames[i];if(f[0]!=companion_air::PUSH_BINARY_RESPONSE)continue;uint32_t t;memcpy(&t,&f[2],4);assert(t==tag);std::string names(f.begin()+10,f.end());names.erase(names.find_last_not_of('\0')+1);/* AES block padding, as from MeshCore */lists.push_back(names);got=true;}
  assert(got);printf("regions from %02x%02x...: %s\n",k[0],k[1],lists.back().c_str());}
 std::sort(lists.begin(),lists.end());assert(lists[0]=="*,IT-TOS,"&&lists[1]=="*,IT-VDA,IT-PIE,");
 // 3. Malformed frames answer an error instead of "unsupported".
 phone->frame({companion_air::CMD_SEND_CONTROL_DATA,0x10});assert(phone->app.frames.back()[0]==companion_air::RESP_ERR&&phone->app.frames.back()[1]==companion_air::ERR_ILLEGAL_ARG);
 phone->frame({companion_air::CMD_SEND_ANON_REQ,1,2,3});assert(phone->app.frames.back()[0]==companion_air::RESP_ERR&&phone->app.frames.back()[1]==companion_air::ERR_ILLEGAL_ARG);
 delete phone;delete north;delete south;delete far;
 printf("PASS: companion emulator - Discover regions: zero-hop discovery (2 of 3 repeaters in range), anonymous regions requests to unknown repeaters, 0x8C replies matched by tag, malformed frames rejected\n");
}
