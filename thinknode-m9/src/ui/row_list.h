#pragma once
// M9 D-pad pages: one column of rows drawn with the T-Deck cards, icons and ON / OFF pills, one row selected (filled
// with the accent, like the Settings list). Up / down select, left / right change a value, OK opens or runs.
#include "theme.h"
#include "components.h"
#include <algorithm>
#include <string>
#include <vector>
namespace mm {
enum class RowKind:uint8_t{
 Section,// group label, not selectable
 Nav,    // opens a page: value text and a chevron on the right
 Toggle, // ON / OFF pill; left / right set it, OK flips it
 Choice, // "< value >": left / right step through the choices
 Level,  // bar and percent: left / right step it
 Action, // runs on OK: label centred in the card
 Info,   // label and value, not selectable
 Field}; // text being typed: label above, the text and (selected) the caret; letters go into it
struct ListRow{RowKind kind=RowKind::Nav;Icon icon=Icon::Settings;bool hasIcon=false;std::string label,detail,value;int percent=0;bool on=false;};// Field: value = the text (empty: detail is the hint)
constexpr int ListTop=Top+4,ListCardH=34,ListPitch=36,ListSectionH=18;
inline bool rowSelectable(const ListRow &r){return r.kind!=RowKind::Section&&r.kind!=RowKind::Info;}
inline int rowHeight(const ListRow &r){return r.kind==RowKind::Section?ListSectionH:ListPitch;}
inline int rowOffset(const std::vector<ListRow> &rows,int index){int y=0;for(int i=0;i<index&&i<int(rows.size());i++)y+=rowHeight(rows[i]);return y;}
inline int rowsHeight(const std::vector<ListRow> &rows){return rowOffset(rows,int(rows.size()));}
// The scroll (pixels) that keeps row `focus` in view, its group label too when it opens a group.
inline int rowReveal(const std::vector<ListRow> &rows,int focus,int scroll){
 const int view=Bottom-ListTop,total=rowsHeight(rows);if(focus<0||focus>=int(rows.size()))return 0;
 int top=rowOffset(rows,focus),bottom=top+ListCardH;if(focus>0&&rows[focus-1].kind==RowKind::Section)top-=ListSectionH;
 if(top<scroll)scroll=top;if(bottom>scroll+view)scroll=bottom-view;
 return std::max(0,std::min(scroll,std::max(0,total-view)));}
}
