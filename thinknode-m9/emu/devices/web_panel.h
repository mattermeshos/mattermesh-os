#pragma once
#include <string>
// Web panel (--web PORT): the M9 face in a browser on the PC or a phone on the same network, keys by mouse or touch.
// With --web-key SECRET every address below is under /SECRET/ (for a public tunnel).
//   GET /                 emu/web/m9.html
//   GET /skin.png         the --skin picture (the M9 photo; not in git)
//   GET /frame?since=N    the LCD, RGB565 little endian: X-Enc raw, or rle = (count, colour) uint16 pairs;
//                         headers X-Seq / X-W / X-H / X-Light / X-Kbd;
//                         waits up to 1 s for a frame newer than N, 204 if none
//   GET /key?c=HH         one byte from the M9 keyboard chip (as the real one sends it, D-pad with its ghost OK)
namespace webpanel {
bool start(int port,const std::string &pageDir,const std::string &skin,const std::string &secret);// secret: "" or the /SECRET/ prefix of every address
}
