#pragma once
#include "Stream.h"
#include <vector>
class TwoWire : public Stream {
  uint16_t address_ = 0;
  std::vector<uint8_t> tx_, rx_;
  size_t pos_ = 0;
  uint32_t clock_ = 100000;
  uint16_t timeout_ = 50;

public:
  explicit TwoWire(uint8_t) {}
  bool begin(int = -1, int = -1, uint32_t freq = 0) {
    if (freq)
      clock_ = freq;
    return true;
  }
  bool end() {
    tx_.clear();
    rx_.clear();
    return true;
  }
  bool setClock(uint32_t n) {
    clock_ = n;
    return true;
  }
  uint32_t getClock() { return clock_; }
  bool setPins(int, int) { return true; }
  size_t setBufferSize(size_t n) {
    tx_.reserve(n);
    rx_.reserve(n);
    return n;
  }
  void setTimeOut(uint16_t n) { timeout_ = n; }
  uint16_t getTimeOut() { return timeout_; }
  void beginTransmission(uint16_t a) {
    address_ = a;
    tx_.clear();
  }
  uint8_t endTransmission(bool = true);
  size_t requestFrom(uint16_t, size_t, bool = true);
  using Print::write;
  size_t write(int n) { return write(static_cast<uint8_t>(n)); }
  size_t write(unsigned n) { return write(static_cast<uint8_t>(n)); }
  size_t write(long n) { return write(static_cast<uint8_t>(n)); }
  size_t write(unsigned long n) { return write(static_cast<uint8_t>(n)); }
  size_t write(uint8_t b) override {
    tx_.push_back(b);
    return 1;
  }
  size_t write(const uint8_t *p, size_t n) override {
    if (!p)
      return 0;
    tx_.insert(tx_.end(), p, p + n);
    return n;
  }
  int available() override { return rx_.size() - pos_; }
  int read() override { return pos_ < rx_.size() ? rx_[pos_++] : -1; }
  int peek() override { return pos_ < rx_.size() ? rx_[pos_] : -1; }
  void flush() override {}
};
extern TwoWire Wire, Wire1;
