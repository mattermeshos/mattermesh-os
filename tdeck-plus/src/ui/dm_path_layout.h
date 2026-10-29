#pragma once
#include "theme.h"
namespace mm {
// Chat options > Path (direct chats): the route of the direct messages to that contact.
constexpr int DmPathSection=43;
constexpr Rect DmPathCard={Margin,Top+4,W-2*Margin,54};
// RESET, FLOOD ONLY, SET PATH under the card.
inline Rect dmPathButton(int i){int w=(W-2*Margin-2*Gap)/3;return {Margin+i*(w+Gap),Top+64,w,32};}
constexpr int DmPathHintY=Top+106;
// SET PATH: the hops chosen so far and the hex being typed, three rows of known repeaters (a tap adds one), actions.
constexpr Rect DmPathDraft={Margin,Top+4,W-2*Margin,28};
constexpr int DmPathRows=3,DmPathRowH=30,DmPathRowY=Top+36;
inline Rect dmPathRow(int i){return {Margin,DmPathRowY+i*DmPathRowH,W-2*Margin-6,DmPathRowH-3};}
constexpr int DmPathEditHintY=DmPathRowY+DmPathRows*DmPathRowH+3;
// UNDO, DIRECT (no hops), CANCEL, SAVE.
inline Rect dmPathAction(int i){int w=(W-2*Margin-3*Gap)/4;return {Margin+i*(w+Gap),Bottom-30,w,30};}
}
