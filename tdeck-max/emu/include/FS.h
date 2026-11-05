#pragma once
#include "Arduino.h"
#include <memory>
#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"
namespace fs {
enum SeekMode { SeekSet = 0, SeekCur = 1, SeekEnd = 2 };
struct FileImpl;
class File : public Stream {
  std::shared_ptr<FileImpl> impl_;

public:
  File() { _timeout = 0; }
  explicit File(std::shared_ptr<FileImpl> p) : impl_(std::move(p)) {
    _timeout = 0;
  }
  using Print::write;
  using Stream::readBytes;
  size_t write(uint8_t) override;
  size_t write(const uint8_t *, size_t) override;
  int read() override;
  size_t read(uint8_t *, size_t);
  size_t readBytes(char *p, size_t n) override {
    return read(reinterpret_cast<uint8_t *>(p), n);
  }
  int available() override;
  int peek() override;
  void flush() override;
  bool seek(uint32_t, SeekMode = SeekSet);
  size_t position() const;
  size_t size() const;
  bool setBufferSize(size_t) { return true; }
  void close() { impl_.reset(); }
  operator bool() const;
  const char *name() const;
  const char *path() const;
  bool isDirectory();
  File openNextFile(const char *mode = FILE_READ);
  void rewindDirectory();
  time_t getLastWrite();
};
class FS {
protected:
  virtual bool ready() const { return true; }

public:
  virtual ~FS() = default;
  File open(const char *, const char * = FILE_READ, bool = false);
  File open(const String &p, const char *m = FILE_READ, bool create = false) {
    return open(p.c_str(), m, create);
  }
  bool exists(const char *);
  bool remove(const char *);
  bool rename(const char *, const char *);
  bool mkdir(const char *);
  bool rmdir(const char *);
  bool exists(const String &p) { return exists(p.c_str()); }
  bool remove(const String &p) { return remove(p.c_str()); }
  bool rename(const String &a, const String &b) {
    return rename(a.c_str(), b.c_str());
  }
  bool mkdir(const String &p) { return mkdir(p.c_str()); }
  bool rmdir(const String &p) { return rmdir(p.c_str()); }
};
} // namespace fs
using fs::File;
using fs::FS;
using fs::SeekCur;
using fs::SeekEnd;
using fs::SeekMode;
using fs::SeekSet;
