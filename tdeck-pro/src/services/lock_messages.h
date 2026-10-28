#pragma once
#include <stdint.h>
#include <vector>
#include <algorithm>
namespace device {
// Take exactly the latest counted unread RX records per chat, never old history
// or local TX. Keep arrival order even when senders have incorrect timestamps.
template<class Records,size_t N>
std::vector<uint32_t> unreadMessageTokens(const Records &records,const uint16_t (&counts)[N],size_t chatCount,size_t limit=SIZE_MAX){
 chatCount=std::min(chatCount,N);std::vector<uint16_t> remaining(counts,counts+chatCount);std::vector<uint32_t> tokens;
 for(auto it=records.rbegin();it!=records.rend();++it)if(!it->mine&&!it->readLocally&&it->token&&it->chat>=0&&size_t(it->chat)<chatCount&&remaining[it->chat]){if(tokens.size()==limit)break;tokens.push_back(it->token);remaining[it->chat]--;}
 std::reverse(tokens.begin(),tokens.end());return tokens;
}
// Tokens, not journal indices: eviction cannot move the selection to a read RX.
// Browse newest -> oldest, then a terminal empty state. New RX takes priority.
struct LockMessages {
 std::vector<uint32_t> ids;int position=-1;uint32_t current=0;bool browsing=false,ended=false;
 void reset(const std::vector<uint32_t> &tokens){receive(tokens,tokens.empty()?0:tokens.back());}
 void receive(const std::vector<uint32_t> &tokens,uint32_t token){
  ids=tokens;browsing=false;ended=ids.empty();auto found=std::find(ids.begin(),ids.end(),token);
  position=ended?-1:found==ids.end()?int(ids.size())-1:int(found-ids.begin());current=ended?0:ids[position];
 }
 uint32_t next(const std::vector<uint32_t> &tokens){
  bool unchanged=tokens==ids;ids=tokens;browsing=true;if(ids.empty()||(ended&&unchanged)){ended=true;position=-1;return current=0;}
  auto found=std::find(ids.begin(),ids.end(),current);
  position=found==ids.end()?int(ids.size())-1:int(found-ids.begin())-1;
  ended=position<0;return current=ended?0:ids[position];
 }
};
template<class Records,size_t N>
void restoreUnreadCounts(const Records &records,const uint32_t (&readTokens)[N],uint16_t (&counts)[N],size_t chats){
 chats=std::min(chats,N);for(size_t i=0;i<N;i++)counts[i]=0;
 for(auto &r:records)if(!r.mine&&r.chat>=0&&size_t(r.chat)<chats){if(r.token==readTokens[r.chat])counts[r.chat]=0;else if(!r.readLocally&&counts[r.chat]<65535)counts[r.chat]++;}
}
}
