#include <cassert>
#include <iostream>
#include "mesh/history_page.h"
#include "services/lock_messages.h"
// SD history beyond the 64-record RAM cache: index, summaries, random reads,
// pages, legacy echo replay, clear and token uniqueness.
static matter::Record message(uint32_t token,int chat,bool mine=false){matter::Record r;r.token=token;r.chat=chat;r.mine=mine;r.sender=mine?"YOU":"Peer";r.body="m"+std::to_string(token);r.status=mine?"Pending send":"Received";r.scope="it-vda";return r;}
static void drain(matter::HistoryPager &pager,matter::Journal &journal){for(int i=0;i<100&&pager.busy();i++)pager.poll(journal);assert(!pager.busy());}
int main(){
 fs::FS storage;matter::Journal journal;journal.begin(storage);
 // 113 Cervinia (chat 2) records, then 90 Public (chat 0): none of Cervinia stays in the 64 cache.
 for(uint32_t t=1;t<=113;t++)assert(journal.append(message(t,2,t%10==0)));
 for(uint32_t t=1001;t<=1090;t++)assert(journal.append(message(t,0)));
 matter::Journal reboot;reboot.begin(storage);
 assert(reboot.records.size()==matter::JOURNAL_CACHE_RECORDS&&reboot.index.size()==203);
 int cachedCervinia=0;for(auto &r:reboot.records)if(r.chat==2)cachedCervinia++;assert(cachedCervinia==0);
 auto s=reboot.summary(2);assert(s&&s->total==113&&s->last.token==113&&s->lastOwn==110&&s->lastRX==113);
 matter::Record r;assert(reboot.readRecord(1,r,false)&&r.body=="m1"&&r.chat==2);
 assert(reboot.readRecord(110,r,false)&&r.mine&&r.status=="Not confirmed / reboot");

 // Newest page, then a deep page; offsets count from the newest record.
 matter::HistoryPager pager;assert(pager.ensure(reboot,2,0));drain(pager,reboot);
 assert(pager.page.ready&&pager.page.chat==2&&pager.page.offset==0&&pager.page.records.size()==24);
 assert(pager.page.records.back().token==113&&pager.page.records.front().token==90);
 assert(!pager.ensure(reboot,2,10));
 assert(pager.ensure(reboot,2,100));drain(pager,reboot);
 assert(pager.page.offset==88&&pager.page.records.size()==24&&pager.page.records.front().token==2&&pager.page.records.back().token==25);
 assert(pager.ensure(reboot,2,112));drain(pager,reboot);
 assert(pager.page.offset==100&&pager.page.records.size()==13&&pager.page.records.front().token==1);
 assert(!pager.ensure(reboot,2,112));

 // New RX shifts the open page; revision change forces a reload of the same window.
 assert(reboot.append(message(114,2)));pager.appended(2);
 assert(pager.page.offset==101&&pager.page.total==114);
 assert(pager.ensure(reboot,2,113));drain(pager,reboot);assert(pager.page.records.front().token==1);

 // Legacy echo replay: a later C1 with no heard list keeps the earlier heard list.
 auto file=storage.open("/mattermesh/messages.tsv",FILE_APPEND);
 file.print("C1\t1e\t2\taa,bb\nC1\t1e\t3\t\n");
 matter::Journal legacy;legacy.begin(storage);
 assert(legacy.readRecord(0x1e,r,false)&&r.repeaters==3&&r.heard=="aa,bb");

 // Lock browsing reads unread tokens from the full index, bounded to 64.
 uint16_t unread[matter::CHAT_SLOTS]={};device::restoreUnreadCounts(legacy.index,legacy.readTokens,unread,matter::CHAT_SLOTS);
 assert(unread[2]==103);// first append marked read at token 0; 114 rows, 11 own
 auto tokens=device::unreadMessageTokens(legacy.index,unread,matter::CHAT_SLOTS,64);assert(tokens.size()==64&&tokens.back()==114);

 // Duplicate tokens are rejected, clear hides archived rows from reads and pages.
 assert(!legacy.append(message(5,2)));
 assert(legacy.clearChat(2));assert(!legacy.readRecord(1,r,false));
 matter::HistoryPager cleared;cleared.ensure(legacy,2,0);drain(cleared,legacy);assert(cleared.page.ready&&cleared.page.records.empty());
 matter::Journal afterClear;afterClear.begin(storage);assert(afterClear.summary(2)->total==0&&afterClear.summary(0)->total==90);

 // SD read failure surfaces an error instead of a partial page.
 matter::HistoryPager failing;failing.ensure(afterClear,0,40);storage.files["/mattermesh/messages.tsv"]->clear();drain(failing,afterClear);
 assert(!failing.page.ready&&!failing.error.empty());
 std::cout<<"PASS: SD history beyond the RAM cache (index/summaries/readRecord/pages/new RX shift/legacy heard/lock 64 bound/duplicates/clear/read error)\n";
}
