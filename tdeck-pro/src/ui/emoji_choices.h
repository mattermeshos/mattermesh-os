#pragma once
#include <stdint.h>
#include <algorithm>
#include <vector>
#include "emoji_picker.h"
namespace mm {
// Composer emoji picker: tab 0 = most used, then one tab per category, 6x4 glyphs per tab.
constexpr int EmojiCols=6,EmojiRows=4,EmojiPageSize=EmojiCols*EmojiRows,EmojiTabs=1+mm_emoji_picker::categoryCount;
static constexpr uint32_t DefaultRecentEmoji[]={0x1f642,0x1f602,0x1f609,0x1f44d,0x2764,0x2705,0x26a1,0x1f4cd,0x1f64f,0x1f44b,0x26a0,0x1f198};
inline int emojiIndex(uint32_t code){for(int i=0;i<mm_emoji_picker::count;i++)if(mm_emoji_picker::glyphs[i].code==code)return i;return -1;}
// Count one use; on saturation halve every count so the ranking keeps adapting.
inline void countEmoji(uint16_t *counts,int index){if(index<0||index>=mm_emoji_picker::count)return;if(counts[index]==UINT16_MAX)for(int i=0;i<mm_emoji_picker::count;i++)counts[i]/=2;counts[index]++;}
// Most used first (ties keep picker order), then the defaults, one page at most.
inline std::vector<int> frequentEmoji(const uint16_t *counts){std::vector<int> out;for(int i=0;i<mm_emoji_picker::count;i++)if(counts[i])out.push_back(i);
 std::stable_sort(out.begin(),out.end(),[&](int a,int b){return counts[a]>counts[b];});if(int(out.size())>EmojiPageSize)out.resize(EmojiPageSize);
 for(auto code:DefaultRecentEmoji){int i=emojiIndex(code);if(int(out.size())<EmojiPageSize&&std::find(out.begin(),out.end(),i)==out.end())out.push_back(i);}return out;}
}
