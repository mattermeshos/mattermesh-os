#include <cassert>
#include <cstring>
#include <iostream>
#include "services/quick_reply.h"
#include "services/lock_messages.h"
#include "services/channel_catalog_policy.h"
#include "mesh/journal.h"
int main(){using namespace device;

 LockEnter enter;assert(enter.press(100));assert(enter.tick(2099)==EnterAction::None);assert(enter.tick(2100)==EnterAction::Hold);assert(enter.tick(2101)==EnterAction::None);assert(enter.release(2200)==EnterAction::None);assert(enter.release(2201)==EnterAction::None);
 enter.reset();assert(enter.press(100));assert(enter.release(1099)==EnterAction::Short);
 enter.reset();enter.press(100);assert(enter.release(2100)==EnterAction::Hold);enter.reset();enter.press(100,true);assert(enter.release(100,true)==EnterAction::Short);
 enter.reset();enter.press(0xfffffff0,true);assert(enter.tick(2000)==EnterAction::Hold);assert(enter.recover(10000));
 ReplyOriginal selected;selected.chat=2;selected.token=42;selected.sender="River";selected.body="Original message";selected.scope="it-vda";selected.hops=5;selected.channel="Cervinia";selected.path="flood:1234,abcd";
 QuickReply reply;assert(reply.open(selected)&&reply.editable());int calls=0;
 assert(!reply.send([&](int,const std::string&,const std::string&,std::string&){calls++;return true;}));assert(!calls&&reply.active);
 for(char c:std::string("Copy 12"))reply.append(c);auto draft=reply.text;
 assert(!reply.send([&](int chat,const std::string &scope,const std::string &body,std::string &error){calls++;assert(chat==2&&scope=="it-vda"&&body==draft);error="Radio busy";return false;}));assert(reply.active&&reply.text==draft);
 selected.chat=0;selected.token=99;selected.scope="it"; // Later global RX cannot retarget the frozen session.
 assert(reply.original.chat==2&&reply.original.token==42&&reply.original.hops==5);
 assert(reply.send([&](int chat,const std::string &scope,const std::string &body,std::string&){calls++;assert(chat==2&&scope=="it-vda"&&body=="@[River] Copy 12");return true;}));assert(reply.active&&reply.pending&&!reply.editable()&&reply.text==draft);reply.txToken=50;int queuedCalls=calls;assert(!reply.send([&](int,const std::string&,const std::string&,std::string&){calls++;return true;}));assert(calls==queuedCalls);assert(!reply.complete(51,true));assert(reply.complete(50,false));assert(reply.editable()&&reply.text==draft);assert(reply.send([](int,const std::string&,const std::string&,std::string&){return true;}));reply.txToken=51;assert(reply.complete(51,true)&&reply.active&&reply.sent&&!reply.pending&&!reply.editable()&&reply.text==draft);
 assert(!reply.confirmationExpired(10000));reply.didPaint(10000);assert(!reply.confirmationExpired(11499)&&reply.confirmationExpired(11500));reply.didPaint(11000);assert(reply.confirmationExpired(11500));
 assert(!reply.complete(51,true));assert(!reply.send([&](int,const std::string&,const std::string&,std::string&){calls++;return true;}));assert(calls==queuedCalls);
 reply.append('x');reply.erase();assert(reply.text==draft);reply.close();assert(!reply.active&&!reply.sent&&reply.text.empty());
 reply.open(selected);reply.append('x');assert(reply.send([](int,const std::string&,const std::string&,std::string&){return true;}));reply.txToken=52;assert(reply.complete(52,true));reply.didPaint(0xfffffff0);assert(!reply.confirmationExpired(100)&&reply.confirmationExpired(1600));reply.close();
 for(auto scope:{"*","unknown",""}){selected.scope=scope;reply.open(selected);assert(reply.editable()&&reply.text=="@[River] ");int sent=0;assert(!reply.send([&](int,const std::string&,const std::string&,std::string&){sent++;return true;}));assert(!sent);reply.append('x');assert(reply.send([&](int chat,const std::string &region,const std::string &body,std::string&){sent++;assert(body=="@[River] x");return true;}));assert(sent==1);reply.txToken=1;assert(reply.complete(1,true));}
 selected.direct=true;selected.scope="";reply.open(selected);assert(reply.editable());reply.append('x');assert(reply.send([&](int chat,const std::string &scope,const std::string &body,std::string&){assert(chat==0&&scope.empty()&&body=="@[River] x");return true;}));selected.direct=false;
 fs::FS fs;matter::Journal journal;journal.begin(fs);matter::Record record;record.chat=0;record.body="Unread";
 for(uint32_t token:{1,2,3}){record.token=token;assert(journal.append(record));}
 uint16_t counts[matter::CHAT_SLOTS]={};restoreUnreadCounts(journal.records,journal.readTokens,counts,matter::CHAT_SLOTS);assert(counts[0]==3);
 assert(journal.markOneRead(0,3));restoreUnreadCounts(journal.records,journal.readTokens,counts,matter::CHAT_SLOTS);assert(counts[0]==2);
 matter::Journal boot;boot.begin(fs);restoreUnreadCounts(boot.records,boot.readTokens,counts,matter::CHAT_SLOTS);assert(counts[0]==2&&boot.records.back().readLocally);
 assert((unreadMessageTokens(boot.records,counts,matter::CHAT_SLOTS)==std::vector<uint32_t>{1,2}));
 fs.failWrites=true;assert(!boot.markOneRead(0,2));restoreUnreadCounts(boot.records,boot.readTokens,counts,matter::CHAT_SLOTS);assert(counts[0]==2);
 fs.failWrites=false;assert(boot.markOneRead(0,2)&&boot.markOneRead(0,1));restoreUnreadCounts(boot.records,boot.readTokens,counts,matter::CHAT_SLOTS);assert(counts[0]==0);
 matter::Journal seen;seen.begin(fs);restoreUnreadCounts(seen.records,seen.readTokens,counts,matter::CHAT_SLOTS);assert(counts[0]==0);
 record.token=4;assert(seen.append(record));restoreUnreadCounts(seen.records,seen.readTokens,counts,matter::CHAT_SLOTS);assert(counts[0]==1);
 assert(seen.markRead(0,4));restoreUnreadCounts(seen.records,seen.readTokens,counts,matter::CHAT_SLOTS);assert(counts[0]==0);
 int mapping[2]={1,0};uint32_t boundaries[2]={};std::string row="I1\t0\t3\tREAD_ONE";assert(remapChatRow(row,true,mapping,2,boundaries)==RemapRow::Keep&&row=="I1\t1\t3\tREAD_ONE");
 std::cout<<"PASS: ENTER short/hold/debounce/recovery; frozen reply chat/token/scope/hops; async dispatch/token success/failure/duplicate suppression, free unscoped mention, explicit Path; persistent per-message unread decrement/no collateral reads/write failure/remap; partial overlay and long draft\n";
}
