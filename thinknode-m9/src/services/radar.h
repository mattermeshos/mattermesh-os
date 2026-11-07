#pragma once
#include <Arduino.h>
namespace device {
// Rain radar (Plus, Weather > RADAR): RainViewer's latest radar frame (public API, zoom 6..7), PNG tiles with
// alpha decoded on core 0 into RGB565 (panel order) + 8-bit alpha, six kept in PSRAM. Runs only while the tab
// is open; the frame list is re-read every 10 min.
constexpr int RadarMinZoom=6,RadarMaxZoom=7;
constexpr uint32_t RADAR_FRAMES_MS=600000UL,RADAR_RETRY_MS=60000UL;
struct RadarState {bool open=false,busy=false;String message;uint32_t frameTime=0,revision=0;};
void radarBegin();void radarSetOpen(bool open);void radarRefresh();
RadarState radarState();uint32_t radarRevision();
// Main loop only: the tile of the current frame, or nullptr (then queued). rgb: 256x256 RGB565, alpha: 256x256.
bool radarTile(int z,uint32_t x,uint32_t y,const uint16_t *&rgb,const uint8_t *&alpha);
}
