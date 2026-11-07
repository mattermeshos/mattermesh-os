#include <cassert>
#include <iostream>
#include "ui/emoji_choices.h"
int main(){uint16_t counts[mm_emoji_picker::count]={};auto first=mm::frequentEmoji(counts);assert(first.size()==sizeof(mm::DefaultRecentEmoji)/4&&first[0]==mm::emojiIndex(0x1f642));
 int fire=mm::emojiIndex(0x1f525),sos=mm::emojiIndex(0x1f198);for(int i=0;i<3;i++)mm::countEmoji(counts,fire);mm::countEmoji(counts,sos);auto list=mm::frequentEmoji(counts);
 assert(list[0]==fire&&list[1]==sos&&list.size()==size_t(mm::EmojiPageSize));// both are defaults too: one full pagefor(size_t i=0;i<list.size();i++)for(size_t j=i+1;j<list.size();j++)assert(list[i]!=list[j]);
 for(int i=0;i<mm_emoji_picker::count;i++)counts[i]=1;assert(mm::frequentEmoji(counts).size()==size_t(mm::EmojiPageSize));
 counts[fire]=UINT16_MAX;counts[sos]=40000;mm::countEmoji(counts,fire);assert(counts[fire]==32768&&counts[sos]==20000&&mm::frequentEmoji(counts)[0]==fire);mm::countEmoji(counts,-1);mm::countEmoji(counts,9999);
 std::cout<<"PASS: most-used emoji ranking, defaults fill without duplicates, page cap, saturation halving, bounds\n";}
