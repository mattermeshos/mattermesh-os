#pragma once
namespace window {
bool open(int scale,bool headless);   // SDL window (headless: SDL dummy driver, for tests)
void screenshotNow();                 // writes dataDir()/screenshot-N.ppm
}
