#pragma once
#include "Print.h"
class IPAddress : public Printable {
  uint8_t bytes_[4]{};

public:
  IPAddress() = default;
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes_{a, b, c, d} {}
  IPAddress(uint32_t v) { *this = v; }
  IPAddress(const uint8_t *p) {
    if (p)
      memcpy(bytes_, p, 4);
  }
  IPAddress &operator=(uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
      bytes_[i] = v >> (8 * i);
    return *this;
  }
  operator uint32_t() const {
    return uint32_t(bytes_[0]) | (uint32_t(bytes_[1]) << 8) |
           (uint32_t(bytes_[2]) << 16) | (uint32_t(bytes_[3]) << 24);
  }
  uint8_t operator[](int i) const { return bytes_[i]; }
  uint8_t &operator[](int i) { return bytes_[i]; }
  bool operator==(const IPAddress &v) const {
    return uint32_t(*this) == uint32_t(v);
  }
  bool operator!=(const IPAddress &v) const { return !(*this == v); }
  bool fromString(const char *);
  bool fromString(const String &v) { return fromString(v.c_str()); }
  String toString() const {
    return String(bytes_[0]) + "." + bytes_[1] + "." + bytes_[2] + "." +
           bytes_[3];
  }
  size_t printTo(Print &out) const override { return out.print(toString()); }
};
