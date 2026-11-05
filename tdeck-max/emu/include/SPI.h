#pragma once
#include "Arduino.h"
#include <cstddef>
#include <cstdint>
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3
#define SPI_MSBFIRST MSBFIRST
#define SPI_LSBFIRST LSBFIRST
#define FSPI 0
#define HSPI 1
#define VSPI 2
class SPISettings {
public:
  uint32_t clock;
  uint8_t bitOrder, dataMode;
  SPISettings(uint32_t hz = 1000000, uint8_t order = MSBFIRST,
              uint8_t mode = SPI_MODE0)
      : clock(hz), bitOrder(order), dataMode(mode) {}
};
class SPIClass {
  SPISettings settings_;

public:
  explicit SPIClass(uint8_t = HSPI) {}
  void begin(int8_t = -1, int8_t = -1, int8_t = -1, int8_t = -1) {}
  void end() {}
  void beginTransaction(SPISettings settings) { settings_ = settings; }
  void endTransaction() {}
  uint8_t transfer(uint8_t);
  uint16_t transfer16(uint16_t);
  void transfer(void *p, uint32_t n) {
    transferBytes(static_cast<uint8_t *>(p), static_cast<uint8_t *>(p), n);
  }
  void transferBytes(const uint8_t *, uint8_t *, uint32_t);
  void write(uint8_t b) { transfer(b); }
  void write16(uint16_t b) { transfer16(b); }
  void writeBytes(const uint8_t *, uint32_t);
  void setFrequency(uint32_t) {}
  void setDataMode(uint8_t) {}
  void setBitOrder(uint8_t order) { settings_.bitOrder = order; }
};
extern SPIClass SPI;
