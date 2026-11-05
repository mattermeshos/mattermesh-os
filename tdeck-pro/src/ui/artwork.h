#pragma once
#include "components.h"
namespace mm {
void splashScreen(Adafruit_GFX &g,int progress,const std::string &stage);
constexpr Rect SplashProgress={24,H-54,W-48,34};
}
