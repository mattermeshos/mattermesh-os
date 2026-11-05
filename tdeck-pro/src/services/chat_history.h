#pragma once
#include <algorithm>
#include <stdint.h>
#include <vector>
namespace device {
inline int historyStep(int offset,int delta,int count){return std::max(0,std::min(std::max(0,count-1),offset+(delta>0?1:delta<0?-1:0)));}
}
