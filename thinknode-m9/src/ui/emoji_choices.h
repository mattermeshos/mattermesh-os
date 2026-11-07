#pragma once
#include <stdint.h>
#include <algorithm>
#include <vector>
#include "emoji_picker.h"
#include "theme.h"
namespace mm {
// Composer emoji picker: tab 0 = most used, then one tab per category, EmojiCols x EmojiRows glyphs per tab (theme.h).
constexpr int EmojiPageSize=EmojiCols*EmojiRows,EmojiTabs=1+mm_emoji_picker::categoryCount;
constexpr int EmojiTabPitch=((EmojiPopup.w-8)/EmojiTabs);
constexpr Rect emojiTabRect(int index){return {EmojiPopup.x+4+index*EmojiTabPitch,EmojiPopup.y+4,EmojiTabPitch-2,26};}
constexpr Rect emojiChoiceRect(int index){return {EmojiPopup.x+(((EmojiPopup.w-EmojiCols*36)/2+1))+(index%EmojiCols)*36,EmojiPopup.y+34+(index/EmojiCols)*36,34,34};}
static constexpr uint32_t DefaultRecentEmoji[]={0x1f642,0x1f602,0x1f609,0x1f44d,0x2764,0x2705,0x26a1,0x1f4cd,0x1f64f,0x1f44b,0x26a0,0x1f198
 ,0x1f60d,0x1f923,0x1f60e,0x1f44c,0x1f525,0x1f389,0x1f914,0x1f44f,0x1f4aa,0x2600,0x1f327,0x1f4e1// the Plus page holds 24: fill it from the start
};
inline int emojiIndex(uint32_t code){for(int i=0;i<mm_emoji_picker::count;i++)if(mm_emoji_picker::glyphs[i].code==code)return i;return -1;}
// Count one use; on saturation halve every count so the ranking keeps adapting.
inline void countEmoji(uint16_t *counts,int index){if(index<0||index>=mm_emoji_picker::count)return;if(counts[index]==UINT16_MAX)for(int i=0;i<mm_emoji_picker::count;i++)counts[i]/=2;counts[index]++;}
// Most used first (ties keep picker order), then the defaults, one page at most.
inline std::vector<int> frequentEmoji(const uint16_t *counts){std::vector<int> out;for(int i=0;i<mm_emoji_picker::count;i++)if(counts[i])out.push_back(i);
 std::stable_sort(out.begin(),out.end(),[&](int a,int b){return counts[a]>counts[b];});if(int(out.size())>EmojiPageSize)out.resize(EmojiPageSize);
 for(auto code:DefaultRecentEmoji){int i=emojiIndex(code);if(int(out.size())<EmojiPageSize&&std::find(out.begin(),out.end(),i)==out.end())out.push_back(i);}return out;}
}
