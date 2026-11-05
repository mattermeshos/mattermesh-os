#pragma once
#include "journal.h"
#include <algorithm>
#include <string>
namespace matter {
constexpr int HISTORY_PAGE_RECORDS=24;
class HistoryPager {
public:
 struct Page {int chat=-1,offset=0;uint32_t total=0,revision=0;bool ready=false;std::vector<Record> records;};
 Page page;
 std::string error;
private:
 Page pending;std::vector<uint32_t> tokens;size_t next=0;bool loading=false;
 int attemptedChat=-1,attemptedOffset=-1;uint32_t attemptedRevision=0;
public:
 bool busy()const{return loading;}
 void close(){page=Page{};pending=Page{};tokens.clear();next=0;loading=false;error.clear();attemptedChat=attemptedOffset=-1;}
 bool ensure(Journal &journal,int chat,int offset){
  auto s=journal.summary(chat);if(!s)return false;offset=std::max(0,offset);
  bool fits=page.ready&&page.chat==chat&&page.revision==s->revision&&offset>=page.offset&&
    (offset<page.offset+int(page.records.size())-5||page.offset+int(page.records.size())>=int(page.total));
  if(fits)return false;
  int skip=std::max(0,offset-HISTORY_PAGE_RECORDS/2);skip=std::min(skip,std::max(0,int(s->total)-1));
  if(loading&&pending.chat==chat&&offset>=pending.offset&&offset<pending.offset+HISTORY_PAGE_RECORDS)return false;
  if(!loading&&attemptedChat==chat&&attemptedOffset==skip&&attemptedRevision==s->revision&&!error.empty())return false;
  attemptedChat=chat;attemptedOffset=skip;attemptedRevision=s->revision;
  pending=Page{};pending.chat=chat;pending.offset=skip;pending.total=s->total;pending.revision=s->revision;tokens.clear();next=0;loading=true;error.clear();
  int ignored=0;for(auto it=journal.index.rbegin();it!=journal.index.rend();++it)if(it->chat==chat){if(ignored++<skip)continue;tokens.push_back(it->token);if(tokens.size()==HISTORY_PAGE_RECORDS)break;}
  std::reverse(tokens.begin(),tokens.end());return true;
 }
 // Each UI-loop tick reads at most two records; no worker owns the shared SPI bus.
 bool poll(Journal &journal){
  if(!loading)return false;
  File f=journal.openForRead();// one open for both records of this tick
  for(int budget=0;budget<2&&next<tokens.size();budget++,next++){
   Record r;if(!journal.readRecordFrom(f,tokens[next],r)||r.chat!=pending.chat){if(f)f.close();loading=false;pending.records.clear();error="SD read failed / reopen chat";return true;}pending.records.push_back(r);
  }
  if(f)f.close();
  if(next<tokens.size())return false;
  pending.ready=true;page=std::move(pending);pending=Page{};loading=false;tokens.clear();return true;
 }
 void appended(int chat){
  if(page.ready&&page.chat==chat){page.offset++;page.total++;}
  if(loading&&pending.chat==chat){pending.offset++;pending.total++;}
 }
};
}
