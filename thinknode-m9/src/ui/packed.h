#pragma once
// Raw-deflate assets: the colour emoji sprites (ui/emoji_color.h) and the 320x240 photos (lock themes, splash). The
// device inflates them with the ESP32-S3 ROM decoder (tinfl, no code in the image); the PC builds (previews, UI
// tests) with zlib (-lz), the emulator with the zlib copy inside PNGdec (emu/core/inflate.cpp).
#include <stddef.h>
#include <stdint.h>
#include <esp_heap_caps.h>
#if defined(ESP_PLATFORM)&&!defined(MM_EMULATOR)
#include "esp32s3/rom/miniz.h"
#define MM_ROM_INFLATE 1
#elif !defined(MM_EMULATOR)
#include <zlib.h>
#endif
namespace mm {
#ifdef MM_EMULATOR
bool emulatorInflateRaw(const uint8_t *src,size_t size,uint8_t *dst,size_t capacity);// emu/core/inflate.cpp
#endif
// True when `src` inflates to exactly `capacity` bytes. UI task only (one shared decoder state).
inline bool inflateRaw(const uint8_t *src,size_t size,uint8_t *dst,size_t capacity){
#ifdef MM_ROM_INFLATE
 static tinfl_decompressor *state=nullptr;// ~11 KB: PSRAM, allocated once
 if(!state)state=static_cast<tinfl_decompressor*>(heap_caps_malloc(sizeof(tinfl_decompressor),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!state)return false;
 tinfl_init(state);size_t in=size,out=capacity;
 const tinfl_status status=tinfl_decompress(state,src,&in,dst,dst,&out,TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
 return status==TINFL_STATUS_DONE&&out==capacity;
#elif defined(MM_EMULATOR)
 return emulatorInflateRaw(src,size,dst,capacity);
#else
 z_stream z{};if(inflateInit2(&z,-15)!=Z_OK)return false;
 z.next_in=const_cast<Bytef*>(src);z.avail_in=uInt(size);z.next_out=dst;z.avail_out=uInt(capacity);
 const int status=inflate(&z,Z_FINISH);const bool full=z.avail_out==0;inflateEnd(&z);return status==Z_STREAM_END&&full;
#endif
}
// The RGB565 pixels of a packed photo. The photos share one PSRAM frame (150 KB): asking for another one inflates it
// over the previous one (~10 ms). nullptr when out of memory or the data is damaged.
inline const uint16_t *unpackPhoto(const uint8_t *packed,size_t size,int width,int height){
 static uint8_t *frame=nullptr;static size_t capacity=0;static const uint8_t *current=nullptr;
 const size_t bytes=size_t(width)*size_t(height)*2;
 if(current==packed&&capacity>=bytes)return reinterpret_cast<const uint16_t*>(frame);
 current=nullptr;
 if(capacity<bytes){heap_caps_free(frame);frame=static_cast<uint8_t*>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));capacity=frame?bytes:0;}
 if(!frame||!inflateRaw(packed,size,frame,bytes))return nullptr;
 current=packed;return reinterpret_cast<const uint16_t*>(frame);
}
}
