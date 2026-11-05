#pragma once
#include "Printable.h"
#include "WString.h"
#include <ctime>
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2
class Print {
  int error_ = 0;
  size_t printInteger(unsigned long long, int, bool = false);

protected:
  void setWriteError(int e = 1) { error_ = e; }

public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *, size_t);
  size_t write(const char *p) {
    return p ? write(reinterpret_cast<const uint8_t *>(p), strlen(p)) : 0;
  }
  size_t write(const char *p, size_t n) {
    return write(reinterpret_cast<const uint8_t *>(p), n);
  }
  virtual void flush() {}
  virtual int availableForWrite() { return 0; }
  int getWriteError() { return error_; }
  void clearWriteError() { error_ = 0; }
  size_t printf(const char *, ...) __attribute__((format(printf, 2, 3)));
  size_t print(const String &s) {
    return write(reinterpret_cast<const uint8_t *>(s.c_str()), s.length());
  }
  size_t print(const char *p) { return write(p); }
  size_t print(const __FlashStringHelper *p) {
    return print(reinterpret_cast<const char *>(p));
  }
  size_t print(char c) { return write(uint8_t(c)); }
  size_t print(const Printable &v) { return v.printTo(*this); }
  size_t print(struct tm *t, const char *format = nullptr);
  size_t print(unsigned char v, int base = DEC) {
    return printInteger(v, base);
  }
  size_t print(int v, int base = DEC) {
    if (base != 10)
      return printInteger(static_cast<unsigned int>(v), base == 0 ? 10 : base);
    return print(static_cast<long>(v), base);
  }
  size_t print(unsigned int v, int base = DEC) { return printInteger(v, base); }
  size_t print(long v, int base = DEC) {
    return printInteger(base == 10 ? static_cast<unsigned long long>(v)
                                   : static_cast<unsigned long>(v),
                        base == 0 ? 10 : base, base == 10 && v < 0);
  }
  size_t print(unsigned long v, int base = DEC) {
    return printInteger(v, base);
  }
  size_t print(long long v, int base = DEC) {
    return printInteger(static_cast<unsigned long long>(v), base,
                        base == 10 && v < 0);
  }
  size_t print(unsigned long long v, int base = DEC) {
    return printInteger(v, base);
  }

  size_t print(double v, int digits = 2);
  size_t println() { return write("\r\n"); }
  template <class T> size_t println(const T &v) {
    size_t n = print(v);
    return n + println();
  }
  template <class T> size_t println(T v, int format) {
    size_t n = print(v, format);
    return n + println();
  }
  size_t println(struct tm *v, const char *fmt = nullptr) {
    size_t n = print(v, fmt);
    return n + println();
  }
};
