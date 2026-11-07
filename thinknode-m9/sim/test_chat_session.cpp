#include <cassert>
#include <iostream>
#include <string>
#include "services/chat_session.h"
#include "services/lock_messages.h"
#include "mesh/journal.h"
struct Message {bool mine=false;uint32_t token=0;std::string body,scope;};
int main(){
 using namespace device;
 ChatDrafts<std::string,24> drafts;std::string editor="Draft A è";
 drafts.leave(0,editor);assert(editor.empty());drafts.take(1,editor);editor="Draft B";drafts.leave(1,editor);
 drafts.take(0,editor);assert(editor=="Draft A è");editor.clear();drafts.recoverIfEmpty(0,editor);assert(editor.empty());
 drafts.take(1,editor);assert(editor=="Draft B");drafts.leave(1,editor);drafts.clear(1);drafts.take(1,editor);assert(editor.empty());
 drafts.save(2,"original");editor="edited";drafts.recoverIfEmpty(2,editor);assert(editor=="edited");drafts.leave(2,editor);drafts.take(2,editor);assert(editor=="edited");
 drafts.save(-1,"bad");drafts.save(24,"bad");drafts.take(24,editor);assert(editor.empty());
 fs::FS storage;matter::Journal journal;journal.begin(storage);matter::Record own;own.mine=true;own.chat=3;own.token=77;own.body="Scoped reply";own.scope="it-vda";assert(journal.append(own));
 Message selected;selected.mine=true;selected.token=77;selected.body="old snapshot";selected.scope="it";
 auto resend=resendPayload(selected,3,3,journal.records);assert(resend.valid&&resend.scoped&&resend.scope=="it-vda"&&resend.body=="Scoped reply");
 assert(!resendPayload(selected,3,4,journal.records).valid);selected.mine=false;assert(!resendPayload(selected,3,3,journal.records).valid);
 selected.mine=true;selected.token=0;selected.body="Legacy draft";selected.scope="";resend=resendPayload(selected,3,3,journal.records);assert(resend.valid&&!resend.scoped);
 selected.scope="*";resend=resendPayload(selected,3,3,journal.records);assert(resend.valid&&resend.scoped&&resend.scope=="*");
 selected.scope="unknown";assert(!resendPayload(selected,3,3,journal.records).valid);
 matter::Record rx;rx.chat=0;rx.token=1;rx.body="First";assert(journal.append(rx));rx.token=2;rx.body="Later arrival";assert(journal.append(rx));
 uint16_t unread=2;storage.failWrites=true;assert(!commitPaintedRead(journal,0,1,2,1,unread)&&unread==2&&journal.readTokens[0]==0);
 storage.failWrites=false;assert(commitPaintedRead(journal,0,1,2,1,unread)&&unread==1);
 matter::Journal reboot;reboot.begin(storage);uint16_t counts[matter::CHAT_SLOTS]={};restoreUnreadCounts(reboot.records,reboot.readTokens,counts,matter::CHAT_SLOTS);assert(counts[0]==1);
 assert(commitPaintedRead(reboot,0,2,2,1,counts[0])&&counts[0]==0);
 // Crash between saving Pending send and queueing radio TX: never leave it
 // looking live forever after reboot, and never transmit automatically.
 matter::Record pending=own;pending.token=88;pending.status="Pending send";assert(journal.append(pending));
 reboot.begin(storage);bool found=false;for(auto &r:reboot.records)if(r.token==88){found=true;assert(r.status=="Not confirmed / reboot");}assert(found);
 std::cout<<"PASS: per-chat draft ownership/navigation/clear, resend preserves original scope and identity, transactional SD read state and crash recovery\n";
}
