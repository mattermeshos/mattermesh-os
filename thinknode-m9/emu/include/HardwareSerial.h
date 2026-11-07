#pragma once
#include "Stream.h"
#include <memory>
#include <functional>
#define SERIAL_8N1 0x800001c
class HardwareSerial : public Stream {
  struct State;
  struct Uart {bool open = false; unsigned long baud = 0; std::function<void(const uint8_t *, size_t)> onWrite;};
  Uart &emuUart() const;
  std::shared_ptr<State> state_;
  int port_;

public:
  explicit HardwareSerial(int port);
  ~HardwareSerial();
  void begin(unsigned long, uint32_t = SERIAL_8N1, int8_t = -1, int8_t = -1,
             bool = false, unsigned long = 20000UL, uint8_t = 112);
  void end();
  operator bool() const { return true; }
  using Print::write;
  size_t write(uint8_t) override;
  size_t write(const uint8_t *, size_t) override;
  int available() override;
  int read() override;
  // Non-blocking block read, as HWCDC::read(buf,len) on the device: whatever is queued, up to len bytes.
  size_t read(uint8_t *buf, size_t len) { size_t n = 0; while (n < len && available() > 0) buf[n++] = uint8_t(read()); return n; }
  int peek() override;
  void flush() override;
  int availableForWrite() override { return 4096; }
  size_t setTxBufferSize(size_t n) { return n; }
  size_t setRxBufferSize(size_t n) { return n; }
  void setTxTimeoutMs(uint32_t) {}
  void feed(const uint8_t *, size_t);
  size_t emuBytesWritten() const;
  // Emulator hooks for UART devices (the GPS on Serial2): open state and baud set by begin()/end(), and every
  // byte block the firmware writes (called on the writing thread).
  bool emuOpen() const;
  unsigned long emuBaud() const;
  void emuOnWrite(std::function<void(const uint8_t *, size_t)> hook);
};
extern HardwareSerial Serial, Serial2;
void emuSerial2Feed(const uint8_t *, size_t);
