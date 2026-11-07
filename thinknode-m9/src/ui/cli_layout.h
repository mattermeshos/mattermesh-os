#pragma once
#include "theme.h"
// CLI app (section 30).
namespace mm {
constexpr int CliSection=30;
// Sections served by the tools hooks (main.cpp): the CLI.
inline bool toolsSection(int section){return section==CliSection;}
// CLI: terminal text above a toolbar (TAB, up, down, left, right, PASTE | CLEAR, STOP).
constexpr int CliToolCount=8,CliLineH=13;
constexpr Rect CliOutput={Margin,Top+4,W-2*Margin,H-Top-8};
// A divider separates the editing tools (0..5) from CLEAR and STOP (6, 7).
}
