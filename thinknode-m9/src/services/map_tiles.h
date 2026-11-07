#pragma once
#include <stdint.h>
#include <FS.h>
namespace device {
// Map tiles for the Maps app: OpenStreetMap PNG on SD (/maps/osm/...), decoded on core 0 to RGB565, 12 kept in PSRAM. Only tiles the
// user looks at are downloaded, one at a time, over Wi-Fi; SD access stays on the main loop.
struct MapTilesState {bool sd=false,online=false,worker=false,decoder=false;int queued=0;uint32_t downloaded=0,failed=0,stored=0;uint32_t sdLoaded=0,purged=0,lastReadMs=0,lastDecodeMs=0,lastBytes=0;};
// Plus: tiles are kept on SD as the downloaded PNG (/maps/osm, ~20-50 KB); the main loop only reads the file and a
// core-0 task decodes it to RGB565.
void mapTilesBegin(fs::FS &fs,bool sdReady);
// Main loop only. Returns the tile (RAM, else SD) or nullptr; when `download`
// (1 bpp on the Max; RGB565 in panel byte order on the Plus: cast to uint16_t*)
// is set and Wi-Fi is up, a missing tile is queued.
const uint8_t *mapTile(int z,uint32_t x,uint32_t y,bool download);
// A compose pass loads at most one tile from SD; the others come on later passes (mapTilesRevision changes).
void mapTilesBeginPass();
void mapTilesForget(); // drop queued (not started) downloads after a pan/zoom
void mapTilesRescan(); // forget "not on SD" / "failed" memos (app opened: the card may have new tiles)
void mapTilesLoop();
// Plus, main loop while locked: deletes a few files of the old OpenTopoMap caches (/maps/otmp, /maps/otmc).
void mapTilesCleanup();extern bool mapCleanupTrace;// Plus: "map cleanup step" prints each delete
uint32_t mapTilesRevision();
MapTilesState mapTilesState();
}
