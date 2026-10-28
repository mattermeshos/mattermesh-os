#pragma once
#include <esp_heap_caps.h>
// Buffers that prefer PSRAM and fall back to internal RAM when PSRAM is short; free with heap_caps_free() or free().
// Large buffers that must never land in internal RAM call heap_caps_malloc(..., MALLOC_CAP_SPIRAM) directly.
namespace device {
inline void *psramMalloc(size_t bytes){if(void *p=heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))return p;return heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);}
inline void *psramCalloc(size_t n,size_t size){if(void *p=heap_caps_calloc(n,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT))return p;return heap_caps_calloc(n,size,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);}
}
