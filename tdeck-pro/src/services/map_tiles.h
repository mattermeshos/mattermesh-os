#pragma once
#include <stdint.h>
#include <FS.h>
namespace device {
// OpenStreetMap tiles for the Maps app: 1 bpp, cached on SD (/maps/osm1/...),
// 20 kept in PSRAM once the app has asked for one. Only tiles the user looks at (plus a one-tile ring) are
// downloaded, one at a time, over Wi-Fi; SD access stays on the main loop.
struct MapTilesState {bool sd=false,online=false;int queued=0;uint32_t downloaded=0,failed=0,stored=0;};
void mapTilesBegin(fs::FS &fs,bool sdReady);
// Main loop only. Returns the tile (RAM, else SD) or nullptr; when `download`
// is set and Wi-Fi is up, a missing tile is queued.
const uint8_t *mapTile(int z,uint32_t x,uint32_t y,bool download);
void mapTilePrefetch(int z,uint32_t x,uint32_t y); // off-screen ring: queue the download if not on SD, no SD read
void mapTilesForget(); // drop queued (not started) downloads after a pan/zoom
void mapTilesRescan(); // forget "not on SD" / "failed" memos (app opened: the card may have new tiles)
void mapTilesLoop();
uint32_t mapTilesRevision();
MapTilesState mapTilesState();
}
