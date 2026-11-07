#pragma once
#include "Print.h"
class Stream : public Print {
protected:
  unsigned long _timeout = 1000;
  int timedRead();
  int timedPeek();

public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  void setTimeout(unsigned long n) { _timeout = n; }
  unsigned long getTimeout() { return _timeout; }
  virtual size_t readBytes(char *, size_t);
  size_t readBytes(uint8_t *p, size_t n) {
    return readBytes(reinterpret_cast<char *>(p), n);
  }
  size_t readBytesUntil(char, char *, size_t);
  String readString();
  String readStringUntil(char);
  long parseInt();
  float parseFloat();
  bool find(const char *target);
};
