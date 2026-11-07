#pragma once
#include <cstddef>
#include <cstdint>
#define MALLOC_CAP_DMA (1U << 3)
#define MALLOC_CAP_SPIRAM (1U << 10)
#define MALLOC_CAP_INTERNAL (1U << 11)
#define MALLOC_CAP_8BIT (1U << 2)
#define MALLOC_CAP_DEFAULT (1U << 12)
void *heap_caps_malloc(size_t, uint32_t);
void *heap_caps_calloc(size_t, size_t, uint32_t);
void *heap_caps_realloc(void *, size_t, uint32_t);
void heap_caps_free(void *);
size_t heap_caps_get_free_size(uint32_t);
size_t heap_caps_get_largest_free_block(uint32_t);
inline size_t heap_caps_get_minimum_free_size(uint32_t caps){return heap_caps_get_free_size(caps);}// no low-water mark on the host
size_t heap_caps_get_total_size(uint32_t);
