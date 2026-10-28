#pragma once
// Companion app requests that go on air and the frames pushed back to the app, as MeshCore's companion_radio:
// CMD_SEND_CONTROL_DATA (zero-hop control packet, e.g. "discover nearby repeaters") answered by
// PUSH_CODE_CONTROL_DATA frames, and CMD_SEND_ANON_REQ (anonymous request to a node that need not be a contact,
// e.g. a repeater's region list) answered by RESP_SENT and later PUSH_CODE_BINARY_RESPONSE. The app's
// "Discover regions" uses both. Header-only and generic over the mesh so the host emulator
// (sim/companion_emulator.cpp) runs this same code against real MeshCore nodes.
#include <Mesh.h>
#include <helpers/BaseChatMesh.h>
#include <string.h>
namespace companion_air {
enum : uint8_t {CMD_SEND_CONTROL_DATA=55,CMD_SEND_ANON_REQ=57,RESP_OK=0,RESP_ERR=1,RESP_SENT=6,
 PUSH_BINARY_RESPONSE=0x8C,PUSH_CONTROL_DATA=0x8E,ERR_TABLE_FULL=3,ERR_ILLEGAL_ARG=6};
// Tag of the app's last anonymous request (MeshCore keeps one pending request too).
struct Pending {uint32_t tag=0;};
inline size_t error(uint8_t *out,uint8_t code){out[0]=RESP_ERR;out[1]=code;return 2;}
// {55}{control payload, first byte with the top bit set}: sent zero-hop. Returns the reply frame length.
template<class M> size_t sendControl(M &node,const uint8_t *in,size_t len,uint8_t *out){
 if(len<2||!(in[1]&0x80))return error(out,ERR_ILLEGAL_ARG);
 mesh::Packet *packet=node.createControlData(in+1,len-1);if(!packet)return error(out,ERR_TABLE_FULL);
 node.sendZeroHop(packet);out[0]=RESP_OK;return 1;
}
// {57}{public key 32}{request}: a node missing from the contacts is added as an unnamed contact reached zero-hop
// direct (MeshCore firmware 13+), so its encrypted reply can be matched.
template<class M> size_t sendAnon(M &node,uint32_t now,const uint8_t *in,size_t len,Pending &pending,uint8_t *out){
 if(len<=1+PUB_KEY_SIZE||len-1-PUB_KEY_SIZE>MAX_PACKET_PAYLOAD-4)return error(out,ERR_ILLEGAL_ARG);
 const uint8_t *key=in+1;ContactInfo *to=node.lookupContactByPubKey(key,PUB_KEY_SIZE);ContactInfo anon;
 if(!to){memset(&anon,0,sizeof(anon));memcpy(anon.id.pub_key,key,PUB_KEY_SIZE);anon.out_path_len=0;anon.type=ADV_TYPE_NONE;anon.lastmod=now;if(node.addContact(anon))to=&anon;}
 if(!to)return error(out,ERR_TABLE_FULL);// contacts full
 uint32_t tag=0,timeout=0;int result=node.sendAnonReq(*to,in+1+PUB_KEY_SIZE,uint8_t(len-1-PUB_KEY_SIZE),tag,timeout);
 if(result==MSG_SEND_FAILED)return error(out,ERR_TABLE_FULL);
 pending.tag=tag;out[0]=RESP_SENT;out[1]=result==MSG_SEND_SENT_FLOOD?1:0;memcpy(out+2,&tag,4);memcpy(out+6,&timeout,4);return 10;
}
// A response {tag 4}{data} from a contact: the push for the app when it answers the pending request, else 0.
inline size_t responseFrame(Pending &pending,const uint8_t *data,uint8_t len,uint8_t *out,size_t cap){
 uint32_t tag;if(len<=4||!pending.tag)return 0;memcpy(&tag,data,4);if(tag!=pending.tag||size_t(len)+2>cap)return 0;
 pending.tag=0;out[0]=PUSH_BINARY_RESPONSE;out[1]=0;memcpy(out+2,data,len);return size_t(len)+2;
}
// Every control packet heard: {0x8E}{snr x4}{rssi}{path length}{payload}.
inline size_t controlFrame(const mesh::Packet *packet,float snr,float rssi,uint8_t *out,size_t cap){
 if(size_t(packet->payload_len)+4>cap)return 0;
 out[0]=PUSH_CONTROL_DATA;out[1]=uint8_t(int8_t(snr*4));out[2]=uint8_t(int8_t(rssi));out[3]=packet->path_len;memcpy(out+4,packet->payload,packet->payload_len);return size_t(packet->payload_len)+4;
}
}
