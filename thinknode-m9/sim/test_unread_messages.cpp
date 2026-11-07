#include <cassert>
#include <iostream>
#include "services/lock_messages.h"
#include "services/received_repeater.h"
struct Record {uint32_t token;int chat;bool mine,readLocally;Record(uint32_t t,int c,bool own):token(t),chat(c),mine(own),readLocally(false){}};
int main(){
 std::vector<Record> records={{1,0,false},{2,0,true},{3,1,false},{4,0,false},{5,1,false},{6,-1,false},{7,1,false},{8,1,true}};
 uint16_t unread[2]={1,2};
 auto eligible=device::unreadMessageTokens(records,unread,2);assert((eligible==std::vector<uint32_t>{4,5,7}));
 device::LockMessages messages;messages.reset(eligible);assert(messages.current==7);
 assert(messages.next(eligible)==5);assert(messages.next(eligible)==4);assert(!messages.next(eligible)&&messages.ended);for(int i=0;i<30;i++)assert(!messages.next(eligible));
 // A new incoming packet interrupts browsing, shows itself and its actual RPT.
 records.push_back({9,0,false});unread[0]++;eligible=device::unreadMessageTokens(records,unread,2);
 device::ReceivedRepeater rx;rx.receive(9,0,"flood:1234,abcd",1000);messages.receive(eligible,9);
 assert(messages.current==9&&!messages.browsing&&rx.token==messages.current&&rx.hash=="abcd"&&rx.visible(1100,15000));
 assert(messages.next(eligible)==7&&messages.current!=rx.token);assert(messages.next(eligible)==5);
 // New RX must win in the middle of the list, not only at its end.
 records.push_back({10,1,false});unread[1]++;messages.receive(device::unreadMessageTokens(records,unread,2),10);assert(messages.current==10);
 // Reading a chat removes all of its items; TX and other chat history stay out.
 unread[0]=0;eligible=device::unreadMessageTokens(records,unread,2);assert((eligible==std::vector<uint32_t>{5,7,10}));
 for(int i=0;i<9;i++)assert(messages.next(eligible)!=9);
 unread[1]=0;eligible=device::unreadMessageTokens(records,unread,2);assert(eligible.empty());assert(!messages.next(eligible)&&messages.ended);
 // Journal eviction cannot reintroduce an older read message or grow the list.
 unread[1]=65535;records.erase(records.begin(),records.begin()+6);eligible=device::unreadMessageTokens(records,unread,2);assert((eligible==std::vector<uint32_t>{7,10}));messages.reset(eligible);
 assert(messages.next(eligible)==7);for(int i=0;i<100;i++)assert(!messages.next(eligible));
 std::cout<<"PASS: unread-only RX finite browser/count, no old/TX/invalid chat, new RX preempts browsing with matching RPT, read/eviction/empty handling\n";
}
