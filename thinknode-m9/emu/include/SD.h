#pragma once
#include "FS.h"
#include "SPI.h"
#include "emu_hal.h"
enum sdcard_type_t { CARD_NONE, CARD_MMC, CARD_SD, CARD_SDHC, CARD_UNKNOWN };
namespace fs {
class SDFS : public FS {
  bool mounted_ = false;
  uint64_t used_ = 0;
  uint32_t cachedAt_ = 0;
  bool cached_ = false;
  std::string cachedRoot_;

protected:
  bool ready() const override { return mounted_ && emu::sdPresent(); }

public:
  bool begin(uint8_t = 5, SPIClass & = SPI, uint32_t = 4000000,
             const char * = "/sd", uint8_t = 5, bool = false) {
    mounted_ = emu::sdPresent();
    if (mounted_)
      emu::sdRoot();
    cached_ = false;
    return mounted_;
  }
  void end() {
    mounted_ = false;
    cached_ = false;
  }
  sdcard_type_t cardType() { return ready() ? CARD_SDHC : CARD_NONE; }
  uint64_t cardSize() { return ready() ? 32ULL * 1024 * 1024 * 1024 : 0; }
  uint64_t totalBytes() { return cardSize(); }
  uint64_t usedBytes();
};
} // namespace fs
extern fs::SDFS SD;
