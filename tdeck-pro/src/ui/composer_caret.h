#pragma once
#include <string>
#include <vector>
#include <algorithm>
// Caret inside the wrapped composer: rows come from Components::lines() on the display text, which drops the
// separators between rows and collapses runs of spaces inside a row. Walking rows and text together maps a
// byte offset of the display text to (row, bytes into that row), so the caret can be drawn where typing lands.
namespace mm {
struct CaretPlace{int row=0;size_t column=0;};
inline bool caretSeparator(char c){return c==' '||c=='\n';}
inline CaretPlace caretPlace(const std::vector<std::string> &rows,const std::string &value,size_t offset){
 offset=std::min(offset,value.size());CaretPlace place;size_t pos=0;
 for(int r=0;r<int(rows.size());r++){const auto &row=rows[r];place.row=r;place.column=0;
  while(pos<value.size()&&caretSeparator(value[pos]))pos++;
  for(size_t i=0;i<row.size();i++){if(pos>=offset){place.column=i;return place;}
   if(row[i]==' '){while(pos<value.size()&&caretSeparator(value[pos]))pos++;}else pos++;}
  place.column=row.size();if(pos>=offset)return place;
 }
 if(rows.empty()){place.row=0;place.column=0;}return place;
}
// First visible row when only `visible` rows fit: the newest rows unless the caret sits above them.
inline int caretWindowStart(int rows,int visible,int caretRow){if(rows<=visible)return 0;int start=rows-visible;return caretRow<start?caretRow:start;}
}
