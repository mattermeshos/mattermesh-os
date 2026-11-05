#pragma once
// Only what the firmware uses to tell a clean flash (empty NVS) from an installed device.
#include <cstddef>
bool emuNvsHasEntries();
typedef void *nvs_iterator_t;
typedef enum { NVS_TYPE_ANY = 0xff } nvs_type_t;
#define NVS_DEFAULT_PART_NAME "nvs"
inline nvs_iterator_t nvs_entry_find(const char *, const char *, nvs_type_t) {
  static int token;
  return emuNvsHasEntries() ? &token : nullptr;
}
inline void nvs_release_iterator(nvs_iterator_t) {}
