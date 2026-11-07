#pragma once
// MeshCore picks the file system type by platform macro (ESP32 / RP2040 -> fs::FS). The emulator does not define
// ESP32 (that would select ESP-IDF code paths elsewhere), so the type is set here before the real header.
#include <FS.h>
#define FILESYSTEM fs::FS
#include "../../../meshcore-base/src/helpers/IdentityStore.h"
