// Linked with -Wl,--wrap=free (emu/Makefile): every free() in the emulator passes here first, so freeing a
// heap_caps_malloc block with plain free() (valid on the ESP32, used by the firmware) returns its bytes to the
// emulated RAM / PSRAM budget before the memory goes back to the host allocator.
#include <cstdlib>
namespace emu {void heapForget(void *p);}
extern "C" void __real_free(void *p);
extern "C" void __wrap_free(void *p){if(p)emu::heapForget(p);__real_free(p);}
