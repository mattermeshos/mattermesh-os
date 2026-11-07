#include <cassert>
#include <iostream>
#include "mesh/journal.h"
int main(){
 fs::FS storage;matter::Journal journal;journal.begin(storage);
 matter::Record a;a.chat=0;a.token=1;a.body="Old incoming";a.scope="it";a.path="flood:ab,cd";assert(journal.append(a));
 matter::Record b=a;b.chat=1;b.token=2;b.body="Keep other channel";assert(journal.append(b));
 a.mine=true;a.token=3;a.body="Old outgoing";assert(journal.append(a));assert(journal.repeats(3,2,"ab,cd"));assert(journal.markRead(1,2));
 storage.failWrites=true;assert(!journal.clearChat(0,128));assert(journal.records.size()==3&&journal.clearedChats==0);
 storage.failWrites=false;assert(!journal.clearChat(-1));assert(!journal.clearChat(matter::CHAT_SLOTS));assert(journal.clearChat(0,128));
 assert(journal.records.size()==1&&journal.records[0].token==2&&journal.localClearThrough[0]==128);
 matter::Journal boot;boot.begin(storage);assert(boot.records.size()==1&&boot.records[0].body==b.body&&boot.localClearThrough[0]==128&&(boot.clearedChats&1)&&boot.readTokens[1]==2&&boot.readTokens[0]==0);
 a.token=4;a.mine=false;a.body="New message after clear";assert(boot.append(a));
 matter::Journal again;again.begin(storage);assert(again.records.size()==2&&again.records[1].token==4&&again.readTokens[0]==0);assert(again.markRead(0,4));matter::Journal seen;seen.begin(storage);assert(seen.readTokens[0]==4);
 auto file=storage.open("/mattermesh/messages.tsv",FILE_APPEND);file.print("D1\t0\t128\tCLEA\nD1\tbogus\t128\tCLEAR\n");
 matter::Journal malformed;malformed.begin(storage);assert(malformed.records.size()==2);
 assert(malformed.clearChat(0,180));matter::Journal final;final.begin(storage);assert(final.records.size()==1&&final.localClearThrough[0]==180);
 std::cout<<"PASS: per-channel clear persisted/replayed, other messages and paths retained, failed writes/invalid or truncated markers ignored, new messages after clear retained, legacy boundary and repeated clear\n";
}
