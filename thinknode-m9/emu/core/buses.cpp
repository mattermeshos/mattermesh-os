#include "IPAddress.h"
#include "SPI.h"
#include "Wire.h"
#include "emu_hal.h"
#include <vector>
namespace emu {
I2cDevice *i2cLookup(uint8_t);
std::vector<SpiDevice *> spiSelected();
} // namespace emu
TwoWire Wire(0), Wire1(1);
SPIClass SPI;
uint8_t TwoWire::endTransmission(bool) {
  auto d = address_ < 128 ? emu::i2cLookup(address_) : nullptr;
  return d && d->write(tx_.data(), tx_.size()) ? 0 : 2;
}
size_t TwoWire::requestFrom(uint16_t address, size_t n, bool) {
  rx_.assign(n, 0);
  pos_ = 0;
  auto d = address < 128 ? emu::i2cLookup(address) : nullptr;
  size_t got = d ? std::min(n, d->read(rx_.data(), n)) : 0;
  rx_.resize(got);
  return got;
}
uint8_t SPIClass::transfer(uint8_t b) {
  auto devices = emu::spiSelected();
  uint8_t in = 0xff;
  for (auto d : devices)
    in &= d->transfer(b);
  return in;
}
uint16_t SPIClass::transfer16(uint16_t b) {
  if (settings_.bitOrder == LSBFIRST) {
    uint16_t lo = transfer(b & 0xff);
    return lo | (uint16_t(transfer(b >> 8)) << 8);
  }
  uint16_t hi = transfer(b >> 8);
  return (hi << 8) | transfer(b & 0xff);
}
void SPIClass::transferBytes(const uint8_t *out, uint8_t *in, uint32_t n) {
  for (uint32_t i = 0; i < n; ++i) {
    uint8_t b = transfer(out ? out[i] : 0xff);
    if (in)
      in[i] = b;
  }
}
void SPIClass::writeBytes(const uint8_t *p, uint32_t n) {
  if (!p)
    return;
  for (auto d : emu::spiSelected())
    d->write(p, n);
}
bool IPAddress::fromString(const char *p) {
  if (!p)
    return false;
  uint8_t bytes[4];
  for (int i = 0; i < 4; ++i) {
    if (*p < '0' || *p > '9')
      return false;
    unsigned n = 0;
    do {
      n = n * 10 + unsigned(*p++ - '0');
      if (n > 255)
        return false;
    } while (*p >= '0' && *p <= '9');
    bytes[i] = n;
    if (i != 3) {
      if (*p++ != '.')
        return false;
    } else if (*p)
      return false;
  }
  memcpy(bytes_, bytes, 4);
  return true;
}
