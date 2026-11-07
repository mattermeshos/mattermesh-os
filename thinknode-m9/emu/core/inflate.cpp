// mm::inflateRaw for the emulator (src/ui/packed.h). The emulator links PNGdec's modified zlib, which allocates
// nothing itself: the caller provides the state and the 32 KB window (as PNGdec's png.inl does), inflate() takes a
// CRC-check flag and inflateEnd() must not run. Kept out of packed.h: zlib's internal headers define macros
// (local, Assert...) the UI sources must not see.
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <zutil.h>
#include <inftrees.h>
#include <inflate.h>
namespace mm {
bool emulatorInflateRaw(const uint8_t *src,size_t size,uint8_t *dst,size_t capacity){
 static uint8_t *work=nullptr;if(!work)work=static_cast<uint8_t*>(malloc(sizeof(inflate_state)+32768));
 if(!work)return false;
 z_stream z{};auto *state=reinterpret_cast<inflate_state*>(work);z.state=reinterpret_cast<internal_state*>(state);
 state->window=work+sizeof(inflate_state);
 if(inflateInit2(&z,-15)!=Z_OK)return false;
 z.next_in=const_cast<Bytef*>(src);z.avail_in=uInt(size);z.next_out=dst;z.avail_out=uInt(capacity);
 return inflate(&z,Z_FINISH,0)==Z_STREAM_END&&z.avail_out==0;
}
}
