#include "emu_hal.h"
#include "esp_heap_caps.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
namespace {
// Blocks are plain malloc() memory (no header in front), so a firmware free() on a heap_caps block is valid
// as on the ESP32; size and pool live in this registry. The emulator links with --wrap=free
// (emu/devices/free_wrap.cpp) so such a free() also returns the bytes to the budget. Recursive mutex: the
// wrapped free() of our own releases re-enters emu::heapForget().
struct Header {
  size_t bytes;
  bool external;
};
std::recursive_mutex mutex;
size_t totals[2] = {170 * 1024, 8 * 1024 * 1024}, used[2] = {};
std::map<uintptr_t, Header> blocks;
int pool(uint32_t caps) { return (caps & MALLOC_CAP_SPIRAM) ? 1 : 0; }
bool impossible(uint32_t caps) {
  return (caps & MALLOC_CAP_SPIRAM) &&
         (caps & (MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
}
size_t freeBytes(int p) {
  return used[p] < totals[p] ? totals[p] - used[p] : 0;
}
void *allocate(size_t n, uint32_t caps) {
  int p = pool(caps);
  if (!n || impossible(caps) || n > freeBytes(p))
    return nullptr;
  void *result = std::malloc(n);
  if (!result)
    return nullptr;
  try {
    blocks[reinterpret_cast<uintptr_t>(result)] = Header{n, p != 0};
  } catch (...) {
    std::free(result);
    return nullptr;
  }
  used[p] += n;
  return result;
}
void release(void *p) {
  auto it = blocks.find(reinterpret_cast<uintptr_t>(p));
  if (it == blocks.end())
    return;
  used[it->second.external] -= it->second.bytes;
  blocks.erase(it);
  std::free(p);
}
} // namespace
namespace emu {
// A firmware free() on a heap_caps block (called by the --wrap=free hook before the real free).
void heapForget(void *p) {
  std::lock_guard<std::recursive_mutex> g(mutex);
  auto it = blocks.find(reinterpret_cast<uintptr_t>(p));
  if (it == blocks.end())
    return;
  used[it->second.external] -= it->second.bytes;
  blocks.erase(it);
}
void setHeapBudgets(size_t a, size_t b) {
  std::lock_guard<std::recursive_mutex> g(mutex);
  totals[0] = a;
  totals[1] = b;
}
} // namespace emu
void *heap_caps_malloc(size_t n, uint32_t caps) {
  std::lock_guard<std::recursive_mutex> g(mutex);
  return allocate(n, caps);
}
void heap_caps_free(void *p) {
  std::lock_guard<std::recursive_mutex> g(mutex);
  release(p);
}
void *heap_caps_calloc(size_t n, size_t size, uint32_t caps) {
  if (size && n > std::numeric_limits<size_t>::max() / size)
    return nullptr;
  auto p = heap_caps_malloc(n * size, caps);
  if (p)
    std::memset(p, 0, n * size);
  return p;
}
void *heap_caps_realloc(void *p, size_t n, uint32_t caps) {
  if (!p)
    return heap_caps_malloc(n, caps);
  if (!n) {
    heap_caps_free(p);
    return nullptr;
  }
  std::lock_guard<std::recursive_mutex> g(mutex);
  auto it = blocks.find(reinterpret_cast<uintptr_t>(p));
  if (it == blocks.end() || impossible(caps))
    return nullptr;
  Header old = it->second;
  int target = pool(caps);
  size_t credit = old.external == target ? old.bytes : 0;
  if (n > freeBytes(target) + credit)
    return nullptr;
  // Temporarily credit the old block so a grow can use the entire pool budget.
  used[old.external] -= old.bytes;
  void *result = allocate(n, caps);
  used[old.external] += old.bytes;
  if (!result)
    return nullptr;
  std::memcpy(result, p, std::min(n, old.bytes));
  release(p);
  return result;
}
size_t heap_caps_get_free_size(uint32_t caps) {
  std::lock_guard<std::recursive_mutex> g(mutex);
  return impossible(caps) ? 0 : freeBytes(pool(caps));
}
size_t heap_caps_get_largest_free_block(uint32_t caps) {
  return heap_caps_get_free_size(caps);
}
size_t heap_caps_get_total_size(uint32_t caps) {
  std::lock_guard<std::recursive_mutex> g(mutex);
  return impossible(caps) ? 0 : totals[pool(caps)];
}
bool esp_ptr_internal(const void *p) {
  if (!p)
    return false;
  std::lock_guard<std::recursive_mutex> g(mutex);
  uintptr_t addr = reinterpret_cast<uintptr_t>(p);
  auto it = blocks.upper_bound(addr);
  if (it == blocks.begin())
    return false;
  --it;
  return !it->second.external && addr - it->first < it->second.bytes;
}
