#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <new>
#include <type_traits>
#ifdef ESP32
#include <esp_heap_caps.h>
#endif
namespace matter {
// Volatile metadata only. Message bodies and updates remain in the SD journal.
// heardPosition: latest C1 whose heard list replaces the previous one (legacy replay rule).
struct MessageIndexEntry {uint32_t token=0,position=0,statusPosition=0,echoPosition=0,heardPosition=0;int chat=-1;bool mine=false,readLocally=false,restored=false;};
class MessageIndex {
 static constexpr size_t BlockSize=128;
 struct Block {Block *previous=nullptr,*next=nullptr;size_t used=0;MessageIndexEntry entries[BlockSize];};
 Block *head=nullptr,*tail=nullptr,*spare=nullptr;size_t count=0;
public:
 MessageIndex()=default;MessageIndex(const MessageIndex&)=delete;MessageIndex&operator=(const MessageIndex&)=delete;
 ~MessageIndex(){clear();}
 void clear(){while(head){auto next=head->next;head->~Block();free(head);head=next;}if(spare){spare->~Block();free(spare);spare=nullptr;}tail=nullptr;count=0;}
 bool prepare(){
  if((tail&&tail->used<BlockSize)||spare)return true;
#ifdef ESP32
  void *memory=heap_caps_malloc(sizeof(Block),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
  void *memory=malloc(sizeof(Block));
#endif
  if(!memory)return false;spare=new(memory)Block;return true;
 }
 bool push(const MessageIndexEntry &entry){if(!prepare())return false;if(!tail||tail->used==BlockSize){auto block=spare;spare=nullptr;block->previous=tail;if(tail)tail->next=block;else head=block;tail=block;}tail->entries[tail->used++]=entry;count++;return true;}
 size_t size()const{return count;}
 template<bool Reverse,bool Const>class Iterator {
  using Pointer=typename std::conditional<Const,const Block*,Block*>::type;Pointer block;size_t slot;
 public:
  Iterator(Pointer b=nullptr,size_t s=0):block(b),slot(s){}
  using Reference=typename std::conditional<Const,const MessageIndexEntry&,MessageIndexEntry&>::type;
  Reference operator*()const{return block->entries[slot];}
  auto operator->()const -> typename std::conditional<Const,const MessageIndexEntry*,MessageIndexEntry*>::type{return &block->entries[slot];}
  bool operator!=(const Iterator &other)const{return block!=other.block||(block&&slot!=other.slot);}
  Iterator&operator++(){if(Reverse){if(slot)slot--;else{block=block->previous;slot=block?block->used-1:0;}}else if(++slot>=block->used){block=block->next;slot=0;}return *this;}
 };
 Iterator<false,false>begin(){return {head};}Iterator<false,false>end(){return {};}
 Iterator<false,true>begin()const{return {head};}Iterator<false,true>end()const{return {};}
 Iterator<true,false>rbegin(){return {tail,tail?tail->used-1:0};}Iterator<true,false>rend(){return {};}
 Iterator<true,true>rbegin()const{return {tail,tail?tail->used-1:0};}Iterator<true,true>rend()const{return {};}
 // Newest first: lookups are almost always for a recent message (append, delivery, repeat echo).
 MessageIndexEntry *find(uint32_t token){for(auto it=rbegin();it!=rend();++it){auto &e=*it;if(e.token==token&&e.chat>=0)return &e;}return nullptr;}
};
}
