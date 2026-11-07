#pragma once
// Host UI builds (tests, previews): heap_caps_* map to the C heap; every capability is satisfied.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#define MALLOC_CAP_8BIT (1U << 2)
#define MALLOC_CAP_DMA (1U << 3)
#define MALLOC_CAP_SPIRAM (1U << 10)
#define MALLOC_CAP_INTERNAL (1U << 11)
#define MALLOC_CAP_DEFAULT (1U << 12)
inline void *heap_caps_malloc(size_t n,uint32_t){return std::malloc(n);}
inline void *heap_caps_calloc(size_t n,size_t size,uint32_t){return std::calloc(n,size);}
inline void *heap_caps_realloc(void *p,size_t n,uint32_t){return std::realloc(p,n);}
inline void heap_caps_free(void *p){std::free(p);}
