#pragma once
#include "theme.h"
// Settings > Companion link (41) pairs another companion over Bluetooth and switches the T-Deck to its radio; Companion
// settings (42) is its minimal management.
namespace mm {
constexpr int RadioLinkSection=41,RadioLinkSettingsSection=42;
// Bluetooth companion: the status card on top; paired: the "Use companion radio" switch and three actions;
// not paired: the PIN card, SEARCH and up to three companions found.
constexpr Rect RadioLinkStatus={Margin,Top+2,W-2*Margin,40};
constexpr Rect RadioLinkAskNo={Margin+8,DialogButtonY,101,30},RadioLinkAskYes={W-Margin-109,DialogButtonY,101,30};// download the companion's messages?
constexpr Rect RadioLinkToggle={Margin,Top+46,W-2*Margin,34};
inline Rect radioLinkAction(int i){int w=(W-2*Margin-2*Gap)/3;return {Margin+i*(w+Gap),Top+86,w,30};}
constexpr int RadioLinkInfoY=Top+122,RadioLinkPeerRows=3;
constexpr Rect RadioLinkPin={Margin,Top+46,W-2*Margin-96,30},RadioLinkSearch={W-Margin-90,Top+46,90,30};
inline Rect radioLinkPeerRow(int i){return {Margin,Top+82+i*34,W-2*Margin,30};}
// Companion settings: name, path bytes (three chips), region, ADVERT; read-only facts below.
constexpr Rect RadioLinkName={Margin,Top+4,W-2*Margin,30},RadioLinkRegion={Margin,Top+72,W-2*Margin-96,30},RadioLinkAdvert={W-Margin-90,Top+72,90,30};
constexpr int RadioLinkPathY=Top+38,RadioLinkFactsY=Top+110;
inline Rect radioLinkPathChip(int i){int x0=Margin+96,w=(W-Margin-x0-2*Gap)/3;return {x0+i*(w+Gap),RadioLinkPathY,w,30};}
}
