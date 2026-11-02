// Companion link protocol (services/companion_client.h) against a scripted MeshCore companion: handshake (device query,
// app start, contacts, channels, default region), a channel send (OK -> Sent), a DM (SENT -> Sent, then the push
// SEND_CONFIRMED -> Delivered), messages pulled after MSG_WAITING (V3 channel and DM frames, split "sender: body"),
// settings frames (name, path bytes, region, advert), a refused send, a request timeout and a disconnect.
#include "services/companion_client.h"
#include <cassert>
#include <cstdio>
#include <iostream>
using namespace remote;
static std::vector<std::vector<uint8_t>> written;
static void writer(void*,const uint8_t *f,size_t n){written.emplace_back(f,f+n);}
static const std::vector<uint8_t> &last(){assert(!written.empty());return written.back();}
static void put32(std::vector<uint8_t> &f,uint32_t v){uint8_t b[4];memcpy(b,&v,4);f.insert(f.end(),b,b+4);}
static void feed(Client &c,const std::vector<uint8_t> &f,uint32_t now){c.frame(f.data(),f.size(),now);}
static std::vector<uint8_t> deviceInfo(){std::vector<uint8_t> f(82,0);f[0]=RESP_DEVICE_INFO;f[1]=9;f[2]=175;f[3]=3;memcpy(&f[20],"Seeed Wio Tracker L1",20);memcpy(&f[60],"v1.10.0",7);f[80]=0;f[81]=1;return f;}
static std::vector<uint8_t> selfInfo(){std::vector<uint8_t> f={RESP_SELF_INFO,1,22,22};for(int i=0;i<32;i++)f.push_back(uint8_t(0xA0+i));f.resize(f.size()+8+4,0);put32(f,869618);put32(f,62500);f.push_back(8);f.push_back(8);const char *n="Wio-Kinta";f.insert(f.end(),n,n+strlen(n));return f;}
static std::vector<uint8_t> contact(uint8_t first,const char *name){std::vector<uint8_t> f={RESP_CONTACT};for(int i=0;i<32;i++)f.push_back(uint8_t(first+i));f.push_back(1);f.push_back(0);f.push_back(0xFF);f.resize(f.size()+PathMax,0);std::vector<uint8_t> nm(32,0);memcpy(nm.data(),name,strlen(name));f.insert(f.end(),nm.begin(),nm.end());put32(f,0);put32(f,0);put32(f,0);put32(f,0);return f;}
static std::vector<uint8_t> channelInfo(int idx,const char *name,uint8_t fill){std::vector<uint8_t> f={RESP_CHANNEL_INFO,uint8_t(idx)};std::vector<uint8_t> nm(32,0);memcpy(nm.data(),name,strlen(name));f.insert(f.end(),nm.begin(),nm.end());for(int i=0;i<16;i++)f.push_back(fill);return f;}
int main(){
 Client c;c.begin(writer,nullptr);uint32_t t=1000;
 // Handshake
 c.connected(t);assert(c.phase==Phase::Query&&last()==std::vector<uint8_t>({CMD_DEVICE_QUERY,AppVersion}));
 feed(c,deviceInfo(),t+=10);assert(c.phase==Phase::Start&&last()[0]==CMD_APP_START&&last().size()==8+10&&c.info.maxChannels==3&&c.info.model=="Seeed Wio Tracker L1"&&c.info.version=="v1.10.0"&&c.info.pathModeKnown&&c.info.pathMode==1);
 feed(c,selfInfo(),t+=10);assert(c.phase==Phase::Contacts&&last()==std::vector<uint8_t>({CMD_GET_CONTACTS})&&c.info.name=="Wio-Kinta"&&c.info.frequencyKhz==869618&&c.info.bandwidthHz==62500&&c.info.sf==8&&c.info.power==22&&c.info.key[0]==0xA0);
 size_t before=written.size();feed(c,{RESP_CONTACTS_START,2,0,0,0},t+=10);feed(c,contact(0x10,"Alice"),t+=10);feed(c,contact(0x40,"Bob"),t+=RequestTimeoutMs-100);
 c.tick(t+=200);assert(c.timeouts==0);// each contact frame keeps the listing alive
 assert(written.size()==before);feed(c,{RESP_END_OF_CONTACTS,0,0,0,0},t+=10);assert(c.contacts.size()==2&&c.contacts[1].name=="Bob");
 assert(c.phase==Phase::Channels&&last()==std::vector<uint8_t>({CMD_GET_CHANNEL,0}));
 feed(c,channelInfo(0,"Public",0x8B),t+=10);assert(last()==std::vector<uint8_t>({CMD_GET_CHANNEL,1}));
 feed(c,channelInfo(1,"",0),t+=10);assert(last()==std::vector<uint8_t>({CMD_GET_CHANNEL,2}));// empty slot: skipped, listing goes on
 feed(c,channelInfo(2,"#meteo",0x33),t+=10);assert(c.ready()&&c.readyCount==1&&c.channels.size()==2&&c.channels[1].index==2&&c.channels[1].name=="#meteo");
 assert(last()==std::vector<uint8_t>({CMD_GET_DEFAULT_FLOOD_SCOPE}));
 {uint8_t s[16];memset(s,0x33,16);assert(c.channelBySecret(s)&&c.channelBySecret(s)->index==2);}
 {std::vector<uint8_t> f(1+31+16,0);f[0]=RESP_DEFAULT_FLOOD_SCOPE;memcpy(&f[1],"it-lig",6);feed(c,f,t+=10);}assert(c.info.scopeKnown&&c.info.scope=="it-lig");
 assert(last()==std::vector<uint8_t>({CMD_GET_DEVICE_TIME})&&c.companionTime(t)==0);{std::vector<uint8_t> f={RESP_CURR_TIME};put32(f,1700000000);feed(c,f,t+=10);}assert(c.companionTime(t+5000)==1700000005);
 assert(last()==std::vector<uint8_t>({CMD_SYNC_NEXT_MESSAGE}));feed(c,{RESP_NO_MORE_MESSAGES},t+=10);
 // Channel send: OK -> Sent
 assert(c.sendChannel(2,"Ciao meteo",1700000000,11,t+=10));{auto f=last();assert(f[0]==CMD_SEND_CHANNEL_TXT_MSG&&f[1]==TXT_PLAIN&&f[2]==2&&std::string(f.begin()+7,f.end())=="Ciao meteo");uint32_t ts;memcpy(&ts,&f[3],4);assert(ts==1700000000);}
 feed(c,{RESP_OK},t+=10);Delivery d;assert(c.takeDelivery(d)&&d.token==11&&d.outcome==Outcome::Sent&&!c.takeDelivery(d));
 // DM: SENT (ack 0x01020304) -> Sent, then SEND_CONFIRMED -> Delivered; a confirm for another ack changes nothing
 uint8_t bob[6];memcpy(bob,c.contacts[1].key,6);assert(c.sendDirect(bob,"Ciao Bob",1700000001,12,t+=10));{auto f=last();assert(f[0]==CMD_SEND_TXT_MSG&&!memcmp(&f[7],bob,6)&&std::string(f.begin()+13,f.end())=="Ciao Bob");}
 feed(c,{RESP_SENT,0,4,3,2,1,0x10,0x27,0,0},t+=10);/* ack 0x01020304, estimate 10000 ms */assert(c.takeDelivery(d)&&d.token==12&&d.outcome==Outcome::Sent);
 feed(c,{PUSH_SEND_CONFIRMED,9,9,9,9,0,0,0,0},t+=10);assert(!c.takeDelivery(d));
 feed(c,{PUSH_SEND_CONFIRMED,4,3,2,1,0x20,0,0,0},t+=10);assert(c.takeDelivery(d)&&d.token==12&&d.outcome==Outcome::Delivered);
 // Messages: MSG_WAITING -> SYNC; a V3 channel message, a V3 DM from Alice, then none left
 feed(c,{PUSH_MSG_WAITING},t+=10);assert(last()==std::vector<uint8_t>({CMD_SYNC_NEXT_MESSAGE}));
 {std::vector<uint8_t> f={RESP_CHANNEL_MSG_RECV_V3,uint8_t(int8_t(-20)),0,0,0,0x42,TXT_PLAIN};/* 2 hops, 2-byte hashes */put32(f,1700000100);const char *t2="Mario: ciao a tutti";f.insert(f.end(),t2,t2+strlen(t2));feed(c,f,t+=10);}
 assert(last()==std::vector<uint8_t>({CMD_SYNC_NEXT_MESSAGE}));
 {std::vector<uint8_t> f={RESP_CONTACT_MSG_RECV_V3,40,0,0};for(int i=0;i<6;i++)f.push_back(uint8_t(0x10+i));f.push_back(0xFF);f.push_back(TXT_PLAIN);put32(f,1700000200);const char *t3="ciao da Alice";f.insert(f.end(),t3,t3+strlen(t3));feed(c,f,t+=10);}
 feed(c,{RESP_NO_MORE_MESSAGES},t+=10);
 Message m;assert(c.takeMessage(m)&&!m.direct&&m.channel==0&&m.sender=="Mario"&&m.body=="ciao a tutti"&&m.hops==2&&m.timestamp==1700000100&&m.snr==-5.0f);
 assert(c.takeMessage(m)&&m.direct&&m.keyKnown&&m.key[31]==0x10+31&&m.sender=="Alice"&&m.body=="ciao da Alice"&&m.hops==-1&&m.snr==10.0f&&!c.takeMessage(m));
 // A DM from a contact added after the listing waits for a contact refresh, then arrives with its full key
 feed(c,{PUSH_MSG_WAITING},t+=10);
 {std::vector<uint8_t> f={RESP_CONTACT_MSG_RECV_V3,0,0,0};for(int i=0;i<6;i++)f.push_back(uint8_t(0x70+i));f.push_back(0);f.push_back(TXT_PLAIN);put32(f,1700000300);const char *x="sono nuovo";f.insert(f.end(),x,x+strlen(x));feed(c,f,t+=10);}
 assert(!c.takeMessage(m));// held
 assert(last()[0]==CMD_GET_CONTACTS&&last().size()==5);// a refresh from the last change goes out first, the next SYNC after it
 feed(c,{RESP_CONTACTS_START,3,0,0,0},t+=10);feed(c,contact(0x10,"Alice"),t+=10);feed(c,contact(0x40,"Bob"),t+=10);feed(c,contact(0x70,"Nuovo"),t+=10);feed(c,{RESP_END_OF_CONTACTS,0,0,0,0},t+=10);
 assert(c.contacts.size()==3&&c.takeMessage(m)&&m.direct&&m.keyKnown&&m.sender=="Nuovo"&&m.body=="sono nuovo"&&m.hops==0);
 assert(last()==std::vector<uint8_t>({CMD_SYNC_NEXT_MESSAGE}));feed(c,{RESP_NO_MORE_MESSAGES},t+=10);
 // The user said no: messages stay on the companion, even with MSG_WAITING and the periodic poll, until allowed again.
 {c.allowSync(false,t+=10);const size_t sent=written.size();feed(c,{PUSH_MSG_WAITING},t+=10);c.poll(t+=10);assert(written.size()==sent);
  c.allowSync(true,t+=10);assert(written.size()==sent+1&&last()==std::vector<uint8_t>({CMD_SYNC_NEXT_MESSAGE}));feed(c,{RESP_NO_MORE_MESSAGES},t+=10);}
 // Room post (signed): the author prefix is skipped, the author named after the room
 feed(c,{PUSH_MSG_WAITING},t+=10);
 {std::vector<uint8_t> f={RESP_CONTACT_MSG_RECV_V3,0,0,0};for(int i=0;i<6;i++)f.push_back(uint8_t(0x40+i));f.push_back(1);f.push_back(TXT_SIGNED);put32(f,1700000400);for(int i=0;i<4;i++)f.push_back(uint8_t(0x10+i));const char *x="post nella room";f.insert(f.end(),x,x+strlen(x));feed(c,f,t+=10);}
 feed(c,{RESP_NO_MORE_MESSAGES},t+=10);assert(c.takeMessage(m)&&m.sender=="Bob / Alice"&&m.body=="post nella room"&&m.hops==1);
 // An unexpected answer to SYNC (a channel datagram) does not stall the sync; other text types are skipped
 feed(c,{PUSH_MSG_WAITING},t+=10);feed(c,{RESP_CHANNEL_DATA_RECV,1,2,3},t+=10);assert(last()==std::vector<uint8_t>({CMD_SYNC_NEXT_MESSAGE})&&c.skipped==1);
 {std::vector<uint8_t> f={RESP_CHANNEL_MSG_RECV_V3,0,0,0,0,0,1};put32(f,1);f.push_back('x');feed(c,f,t+=10);}assert(c.skipped==2&&!c.takeMessage(m));feed(c,{RESP_NO_MORE_MESSAGES},t+=10);
 // New advert / deleted contact pushes keep the list current
 feed(c,[]{auto f=contact(0x90,"Advert");f[0]=PUSH_NEW_ADVERT;return f;}(),t+=10);assert(c.contacts.size()==3&&last()[0]==CMD_GET_CONTACTS);// not trusted: the list is read again
 feed(c,{RESP_CONTACTS_START,1,0,0,0},t+=10);assert(c.contacts.size()==3);feed(c,contact(0x90,"Advert"),t+=10);feed(c,{RESP_END_OF_CONTACTS,0,0,0,0},t+=10);assert(c.contacts.size()==4&&c.contacts[3].name=="Advert");
 {std::vector<uint8_t> f={PUSH_CONTACT_DELETED};for(int i=0;i<32;i++)f.push_back(uint8_t(0x40+i));feed(c,f,t+=10);}assert(c.contacts.size()==3&&!c.contactByPrefix(bob));
 // A DM whose ACK never comes turns NoAck after the companion's estimate plus a margin
 assert(c.sendDirect(c.contacts[0].key,"ack?",1700000500,13,t+=10));feed(c,{RESP_SENT,0,9,9,9,9,0xE8,3,0,0},t+=10);assert(c.takeDelivery(d)&&d.outcome==Outcome::Sent);
 c.tick(t+=1000+AckMarginMs-10);assert(!c.takeDelivery(d));c.tick(t+=20);assert(c.takeDelivery(d)&&d.token==13&&d.outcome==Outcome::NoAck);
 // A late answer (END_OF_CONTACTS of a timed-out listing) does not complete the SYNC in flight; a SYNC timeout asks again
 feed(c,{PUSH_MSG_WAITING},t+=10);assert(last()[0]==CMD_SYNC_NEXT_MESSAGE);{uint32_t s0=c.stray;feed(c,{RESP_END_OF_CONTACTS,0,0,0,0},t+=10);assert(c.stray==s0+1);}
 size_t w0=written.size();c.tick(t+=RequestTimeoutMs);assert(written.size()==w0+1&&last()[0]==CMD_SYNC_NEXT_MESSAGE);feed(c,{RESP_NO_MORE_MESSAGES},t+=10);
 // A name the companion refuses is not shown; an accepted one is
 assert(c.setName("Rifiutato",t+=10));feed(c,{RESP_ERR,6},t+=10);assert(c.info.name!="Rifiutato");
 assert(c.setName("Accettato",t+=10));feed(c,{RESP_OK},t+=10);assert(c.info.name=="Accettato");
 // A MSG_WAITING while a sync runs asks again once that one ends
 feed(c,{PUSH_MSG_WAITING},t+=10);size_t n1=written.size();feed(c,{PUSH_MSG_WAITING},t+=10);assert(written.size()==n1);feed(c,{RESP_NO_MORE_MESSAGES},t+=10);assert(written.size()==n1+1&&last()[0]==CMD_SYNC_NEXT_MESSAGE);feed(c,{RESP_NO_MORE_MESSAGES},t+=10);
 // Settings frames
 assert(c.setName("Wio-Plus",t+=10)&&last()==std::vector<uint8_t>({CMD_SET_ADVERT_NAME,'W','i','o','-','P','l','u','s'}));feed(c,{RESP_OK},t+=10);
 assert(c.setPathBytes(2,t+=10)&&last()==std::vector<uint8_t>({CMD_SET_PATH_HASH_MODE,0,1}));feed(c,{RESP_OK},t+=10);assert(!c.setPathBytes(4,t));
 {uint8_t key[16];for(int i=0;i<16;i++)key[i]=uint8_t(i);assert(c.setRegion("it",key,t+=10));auto f=last();assert(f.size()==48&&f[0]==CMD_SET_DEFAULT_FLOOD_SCOPE&&f[1]=='i'&&f[2]=='t'&&f[3]==0&&f[32]==0&&f[47]==15);feed(c,{RESP_OK},t+=10);
  assert(c.setRegion("",key,t+=10)&&last()==std::vector<uint8_t>({CMD_SET_DEFAULT_FLOOD_SCOPE}));feed(c,{RESP_OK},t+=10);}
 assert(c.advertise(t+=10)&&last()==std::vector<uint8_t>({CMD_SEND_SELF_ADVERT,1}));feed(c,{RESP_OK},t+=10);
 assert(!c.takeDelivery(d));// settings carry no token
 // Requests queue while one is in flight; a refused send fails, the next goes out
 assert(c.sendChannel(0,"uno",1,21,t+=10)&&c.sendChannel(0,"due",2,22,t));assert(std::string(last().begin()+7,last().end())=="uno");
 feed(c,{RESP_ERR,2},t+=10);assert(c.takeDelivery(d)&&d.token==21&&d.outcome==Outcome::Failed&&std::string(last().begin()+7,last().end())=="due");
 // Timeout: no answer -> Failed, then a disconnect fails what is still queued
 {uint32_t t0=c.timeouts;c.tick(t+=RequestTimeoutMs);assert(c.timeouts==t0+1);}assert(c.takeDelivery(d)&&d.token==22&&d.outcome==Outcome::Failed);
 // A refresh asked while one runs goes out once it ends; three timeouts in a row while Ready ask for a reconnect
 feed(c,{PUSH_ADVERT,1},t+=10);assert(last()[0]==CMD_GET_CONTACTS);feed(c,{RESP_CONTACTS_START,0,0,0,0},t+=300);
 {size_t w=written.size();feed(c,{PUSH_ADVERT,1},t+=10);assert(written.size()==w);feed(c,{RESP_END_OF_CONTACTS,0,0,0,0},t+=10);assert(written.size()==w+1&&last()[0]==CMD_GET_CONTACTS);}
 feed(c,{RESP_CONTACTS_START,0,0,0,0},t+=10);feed(c,{RESP_END_OF_CONTACTS,0,0,0,0},t+=10);
 assert(c.advertise(t+=10)&&c.advertise(t)&&c.advertise(t));assert(!c.reconnectWanted);for(int i=0;i<3;i++)c.tick(t+=RequestTimeoutMs);assert(c.reconnectWanted);c.reconnectWanted=false;
 assert(c.sendChannel(0,"tre",3,23,t+=10)&&c.sendChannel(0,"quattro",4,24,t));c.disconnected();
 assert(c.takeDelivery(d)&&d.outcome==Outcome::Failed&&c.takeDelivery(d)&&d.outcome==Outcome::Failed&&!c.ready()&&c.phase==Phase::Idle&&c.contacts.empty());
 assert(!c.sendChannel(0,"cinque",5,25,t));
 // Older firmware: GET_CHANNEL error past the last slot and no default region still end Ready
 c.connected(t+=10);feed(c,deviceInfo(),t+=10);feed(c,selfInfo(),t+=10);feed(c,{RESP_CONTACTS_START,0,0,0,0},t+=10);feed(c,{RESP_END_OF_CONTACTS,0,0,0,0},t+=10);
 feed(c,channelInfo(0,"Public",1),t+=10);feed(c,{RESP_ERR,2},t+=10);assert(c.ready()&&c.channels.size()==1&&c.readyCount==2);
 feed(c,{RESP_ERR,1},t+=10);assert(!c.info.scopeKnown&&last()[0]==CMD_GET_DEVICE_TIME);feed(c,{RESP_ERR,1},t+=10);assert(last()[0]==CMD_SYNC_NEXT_MESSAGE&&c.companionTime(t)==0);
 // A contact listing cut short (END lost) times out and still moves on to the channels; a device query that never
 // answers asks for a reconnect
 c.connected(t+=10);feed(c,deviceInfo(),t+=10);feed(c,selfInfo(),t+=10);feed(c,{RESP_CONTACTS_START,9,0,0,0},t+=10);feed(c,contact(0x10,"Alice"),t+=10);
 c.tick(t+=RequestTimeoutMs);assert(c.phase==Phase::Channels&&c.contacts.size()==1&&last()==std::vector<uint8_t>({CMD_GET_CHANNEL,0}));
 c.tick(t+=RequestTimeoutMs);assert(last()==std::vector<uint8_t>({CMD_GET_CHANNEL,1}));// a slot that never answers is skipped
 feed(c,channelInfo(1,"Public",7),t+=10);feed(c,channelInfo(2,"",0),t+=10);assert(c.ready()&&c.channels.size()==1);
 c.connected(t+=10);assert(!c.reconnectWanted);c.tick(t+=RequestTimeoutMs);assert(c.reconnectWanted&&!c.ready());
 std::cout<<"PASS: companion client - handshake (info, self, contacts, channels, region), channel send, DM sent and delivered, messages pulled after MSG_WAITING (V3 channel / DM, sender split), settings frames, queue, refusal, timeout, disconnect, older firmware, handshake timeouts, hops with 2-byte hashes, room posts, unknown DM sender resolved by a contact refresh, advert / deleted pushes, no-ACK deadline, sync past unexpected frames, companion clock, late answers ignored, sync retried after a timeout, settings shown on OK\n";
}
