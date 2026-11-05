#pragma once
#include "theme.h"
// CLI app (section 30).
namespace mm {
constexpr int CliSection=30;
// Sections served by the tools hooks (main.cpp): the CLI.
inline bool toolsSection(int section){return section==CliSection;}
// CLI: terminal text above a toolbar (TAB, up, down, left, right, PASTE | CLEAR, STOP).
constexpr int CliToolCount=8,CliLineH=13;
constexpr Rect CliBar={Margin-4,H-36,W-2*Margin+8,32};
constexpr Rect CliOutput={Margin,Top+4,W-2*Margin,CliBar.y-Top-8};
constexpr int CliToolGap=6,CliToolW=(CliBar.w-8-CliToolGap)/CliToolCount;
// A divider separates the editing tools (0..5) from CLEAR and STOP (6, 7).
inline Rect cliToolRect(int index){return {CliBar.x+4+index*CliToolW+(index>=6?CliToolGap:0),CliBar.y+2,CliToolW,CliBar.h-4};}
}
