// Protocol test peers communicate in memory. No RF transmission or application callbacks.
#include "protocol_test.h"
#include "discovery_limits.h"
#include "flood_policy.h"
#include <esp_heap_caps.h>
#include <new>
#include <helpers/BaseChatMesh.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/TransportKeyStore.h>
#include <esp_system.h>
#include <deque>
#include <vector>
namespace matter { namespace {
class RNG:public mesh::RNG{void random(uint8_t *d,size_t n)override{esp_fill_random(d,n);}};
class Clock:public mesh::RTCClock{uint32_t t=1700000000;public:uint32_t getCurrentTime()override{return t;}void setCurrentTime(uint32_t v)override{t=v;}};
class Link:public mesh::Radio{
public:Link *peer=nullptr;std::deque<std::vector<uint8_t>> incoming;std::vector<uint8_t> last;bool sent=false;
 int recvRaw(uint8_t *d,int n)override{if(incoming.empty())return 0;auto b=incoming.front();incoming.pop_front();if(int(b.size())>n)return 0;memcpy(d,b.data(),b.size());return b.size();}
 uint32_t getEstAirtimeFor(int)override{return 1;}
 float packetScore(float,int)override{return 1;}
 bool startSendRaw(const uint8_t *b,int n)override{last.assign(b,b+n);peer->incoming.push_back(last);sent=true;return true;}
 bool isSendComplete()override{return sent;}
 void onSendFinished()override{sent=false;}
 bool isInRecvMode()const override{return true;}
};
class TestPool:public mesh::PacketManager {
 mesh::Packet packets[12];bool used[12]={};struct Queued{mesh::Packet *p;uint32_t at;uint8_t pri;};std::vector<Queued> out,in;
 mesh::Packet *take(std::vector<Queued> &q,uint32_t now){int choice=-1;for(unsigned i=0;i<q.size();i++)if(int32_t(now-q[i].at)>=0&&(choice<0||q[i].pri<q[choice].pri))choice=i;if(choice<0)return nullptr;auto p=q[choice].p;q.erase(q.begin()+choice);return p;}
public:
 mesh::Packet *allocNew()override{for(int i=0;i<12;i++)if(!used[i]){used[i]=true;return &packets[i];}return nullptr;}
 void free(mesh::Packet *p)override{for(int i=0;i<12;i++)if(p==&packets[i])used[i]=false;}
 void queueOutbound(mesh::Packet *p,uint8_t pri,uint32_t at)override{out.push_back({p,at,pri});}
 mesh::Packet *getNextOutbound(uint32_t now)override{return take(out,now);}
 int getOutboundCount(uint32_t now)const override{int count=0;for(auto &v:out)if(now==UINT32_MAX||int32_t(now-v.at)>=0)count++;return count;}
 int getOutboundTotal()const override{return out.size();}
 int getFreeCount()const override{int count=0;for(bool v:used)if(!v)count++;return count;}
 mesh::Packet *getOutboundByIdx(int i)override{return i>=0&&i<int(out.size())?out[i].p:nullptr;}
 mesh::Packet *removeOutboundByIdx(int i)override{if(i<0||i>=int(out.size()))return nullptr;auto p=out[i].p;out.erase(out.begin()+i);return p;}
 void queueInbound(mesh::Packet *p,uint32_t at)override{in.push_back({p,at,0});}
 mesh::Packet *getNextInbound(uint32_t now)override{return take(in,now);}
};
class Peer:public BaseChatMesh{
public:Link link;ArduinoMillis ms;RNG rng;Clock clock;TestPool pool;SimpleMeshTables tables;uint32_t expected=0;uint8_t remote[32]={};int groups=0,directs=0,acks=0,discoveries=0,hops=-1,pathBytes=0,loginBytes=-1,loginOffset=4;bool loginExact=false;String text;
 Peer():BaseChatMesh(link,ms,rng,clock,pool,tables){do{self_id=mesh::LocalIdentity(&rng);}while(self_id.pub_key[0]==0||self_id.pub_key[0]==255);begin();addChannel("Public","izOH6cXN6mrJ5e26oRXNcg==");}
protected:
 void sendFloodScoped(const ContactInfo&,mesh::Packet *p,uint32_t delay=0)override{sendMatterFlood(*this,p,delay);}
 void sendFloodScoped(const mesh::GroupChannel&,mesh::Packet *p,uint32_t delay=0)override{sendMatterFlood(*this,p,delay);}
 float getAirtimeBudgetFactor()const override{return 0;}
 int calcRxDelay(float,uint32_t)const override{return 0;}
 bool allowPacketForward(const mesh::Packet*)override{return false;}
 void onDiscoveredContact(ContactInfo&,bool,uint8_t,const uint8_t*)override{discoveries++;}
 void onContactPathUpdated(const ContactInfo&)override{}
 ContactInfo *processAck(const uint8_t *d)override{if(expected&&!memcmp(d,&expected,4)){expected=0;acks++;return lookupContactByPubKey(remote,32);}return nullptr;}
 void onMessageRecv(const ContactInfo&,mesh::Packet *p,uint32_t,const char *t)override{directs++;text=t;hops=p->isRouteFlood()?p->getPathHashCount():-1;pathBytes=p->getPathHashSize();}
 void onAnonDataRecv(mesh::Packet *p,const uint8_t*,const mesh::Identity&,uint8_t *data,size_t len)override{if(p->getPayloadType()!=PAYLOAD_TYPE_ANON_REQ||len<size_t(loginOffset))return;loginBytes=0;loginExact=true;for(size_t i=loginOffset;i<len&&data[i];i++){loginBytes++;if(data[i]!=uint8_t('X'))loginExact=false;}}
 void onCommandDataRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*)override{}
 void onCLICommandRecv(const ContactInfo&,mesh::Packet*,uint32_t,const char*,char *reply)override{reply[0]=0;}
 void onSignedMessageRecv(const ContactInfo&,mesh::Packet*,uint32_t,const uint8_t*,const char*)override{}
 void onChannelMessageRecv(const mesh::GroupChannel&,mesh::Packet *p,uint32_t,const char *t)override{groups++;text=t;hops=p->isRouteFlood()?p->getPathHashCount():-1;pathBytes=p->getPathHashSize();}
 uint32_t calcFloodTimeoutMillisFor(uint32_t)const override{return 1500;}
 uint32_t calcDirectTimeoutMillisFor(uint32_t,uint8_t)const override{return 1500;}
 void onSendTimeout()override{}
 uint8_t onContactRequest(const ContactInfo&,uint32_t,const uint8_t*,uint8_t,uint8_t*)override{return 0;}
 void onContactResponse(const ContactInfo&,const uint8_t*,uint8_t)override{}
};
static void pump(Peer &a,Peer &b,int ms){uint32_t end=millis()+ms;while(int32_t(end-millis())>0){a.loop();b.loop();delay(1);}}
}
String protocolSelfTest(){
 // Allocate peers on heap because packet/contact/crypto stacks exceed Arduino loop stack.
 void *ma=heap_caps_malloc(sizeof(Peer),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),*mb=heap_caps_malloc(sizeof(Peer),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!ma||!mb){heap_caps_free(ma);heap_caps_free(mb);return "FAIL: test PSRAM allocation";}auto a=new(ma) Peer();auto b=new(mb) Peer();a->link.peer=&b->link;b->link.peer=&a->link;String failure;
 auto advert=a->createSelfAdvert("TEST_A");sendMatterFlood(*a,advert);advert=b->createSelfAdvert("TEST_B");sendMatterFlood(*b,advert);pump(*a,*b,250);
 if(a->getNumContacts()!=1||b->getNumContacts()!=1)failure="signed advertisements";
 ChannelDetails channel;a->getChannel(0,channel);if(failure.isEmpty()&&!a->sendGroupMessage(1700000001,channel.channel,"TEST_A","group protocol test",19))failure="group queue";pump(*a,*b,80);
 if(failure.isEmpty()&&(b->groups!=1||b->text!="TEST_A: group protocol test"||b->hops!=0||b->pathBytes!=outgoingPathHashBytes()))failure="group decrypt/hops";
 auto duplicate=a->link.last;b->link.incoming.push_back(duplicate);pump(*a,*b,50);if(failure.isEmpty()&&b->groups!=1)failure="duplicate suppression";
 mesh::Packet scoped;scoped.header=(PAYLOAD_TYPE_GRP_TXT<<PH_TYPE_SHIFT)|ROUTE_TYPE_TRANSPORT_FLOOD;scoped.payload_len=8;memset(scoped.payload,7,8);scoped.setPathHashSizeAndCount(2,3);TransportKeyStore keys;TransportKey key;keys.getAutoKeyFor(1,"#it-lig",key);scoped.transport_codes[0]=key.calcTransportCode(&scoped);uint8_t raw[MAX_TRANS_UNIT];uint8_t n=scoped.writeTo(raw);mesh::Packet decoded;
 if(failure.isEmpty()&&(!decoded.readFrom(raw,n)||decoded.getPathHashCount()!=3||decoded.getPathByteLen()!=6||!decoded.hasTransportCodes()||key.calcTransportCode(&decoded)!=decoded.transport_codes[0]))failure="scope/multibyte hops";
 auto contact=a->lookupContactByPubKey(b->self_id.pub_key,32);uint32_t timeout=0;memcpy(a->remote,b->self_id.pub_key,32);if(failure.isEmpty()&&(!contact||a->sendMessage(*contact,1700000002,0,"direct protocol test",a->expected,timeout)==0))failure="direct queue";pump(*a,*b,700);
 if(failure.isEmpty()&&(b->directs!=1||b->text!="direct protocol test"||a->acks!=1))failure="direct decrypt/ACK";
 if(failure.isEmpty()&&contact){auto target=*contact;for(int kind=0;kind<2&&failure.isEmpty();kind++){target.type=kind?ADV_TYPE_ROOM:ADV_TYPE_REPEATER;b->loginOffset=kind?8:4;for(int length:{0,15,16,64,MAX_LOGIN_PASSWORD_LEN}){String password;for(int i=0;i<length;i++)password+='X';b->loginBytes=-1;b->loginExact=false;if(!a->sendLogin(target,password.c_str(),timeout)){failure="login packet queue";break;}pump(*a,*b,90);if(b->loginBytes!=length||!b->loginExact){failure="login password truncation/format";break;}}}String oversized;for(int i=0;i<=MAX_LOGIN_PASSWORD_LEN;i++)oversized+='X';if(failure.isEmpty()&&a->sendLogin(target,oversized.c_str(),timeout)!=MSG_SEND_FAILED)failure="oversized login accepted";}
 if(failure.isEmpty()){while(b->getNumContacts()<MAX_CONTACTS){ContactInfo c={};int index=b->getNumContacts();memset(c.id.pub_key,0x5a,32);memcpy(c.id.pub_key,&index,sizeof(index));c.type=index%2?ADV_TYPE_CHAT:ADV_TYPE_REPEATER;c.out_path_len=OUT_PATH_UNKNOWN;snprintf(c.name,sizeof(c.name),"CAPACITY_TEST_%d",index);if(!b->addContact(c)){failure="contact table capacity";break;}}ContactInfo extra={};extra.type=ADV_TYPE_CHAT;if(failure.isEmpty()&&(b->getNumContacts()!=MAX_CONTACTS||b->addContact(extra)))failure="contact table overflow";int count=0;ContactInfo c;auto iterator=b->startContactsIterator();while(iterator.hasNext(b,c))count++;if(failure.isEmpty()&&count!=MAX_CONTACTS)failure="contact iterator capacity";}
 a->~Peer();b->~Peer();heap_caps_free(ma);heap_caps_free(mb);
 return failure.length()?"FAIL: "+failure:"PASS: signed adverts, group decrypt ("+String(outgoingPathHashBytes())+"-byte paths), dedup, scope/hops, direct decrypt and ACK, login 0/15/16/64/128 bytes intact (RPT + room), "+String(MAX_CONTACTS)+" contacts/capacity/overflow (in memory, no RF)";
}
}
