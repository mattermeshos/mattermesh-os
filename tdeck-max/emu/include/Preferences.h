#pragma once
#include "Arduino.h"
enum PreferenceType {
  PT_I8,
  PT_U8,
  PT_I16,
  PT_U16,
  PT_I32,
  PT_U32,
  PT_I64,
  PT_U64,
  PT_STR,
  PT_BLOB,
  PT_INVALID
};
class Preferences {
  std::string ns_;
  bool started_ = false, readOnly_ = false;
  bool put(const char *, PreferenceType, const void *, size_t);
  bool get(const char *, PreferenceType, void *, size_t);

public:
  bool begin(const char *, bool = false, const char * = nullptr);
  void end() {
    started_ = false;
    ns_.clear();
  }
  bool clear();
  bool remove(const char *);
  bool isKey(const char *);
  PreferenceType getType(const char *);
  size_t putChar(const char *, int8_t);
  int8_t getChar(const char *, int8_t = 0);
  size_t putUChar(const char *, uint8_t);
  uint8_t getUChar(const char *, uint8_t = 0);
  size_t putShort(const char *, int16_t);
  int16_t getShort(const char *, int16_t = 0);
  size_t putUShort(const char *, uint16_t);
  uint16_t getUShort(const char *, uint16_t = 0);
  size_t putInt(const char *, int32_t);
  int32_t getInt(const char *, int32_t = 0);
  size_t putUInt(const char *, uint32_t);
  uint32_t getUInt(const char *, uint32_t = 0);
  size_t putLong(const char *, int32_t);
  int32_t getLong(const char *, int32_t = 0);
  size_t putULong(const char *, uint32_t);
  uint32_t getULong(const char *, uint32_t = 0);
  size_t putLong64(const char *, int64_t);
  int64_t getLong64(const char *, int64_t = 0);
  size_t putULong64(const char *, uint64_t);
  uint64_t getULong64(const char *, uint64_t = 0);
  size_t putFloat(const char *, float);
  float getFloat(const char *, float = NAN);
  size_t putDouble(const char *, double);
  double getDouble(const char *, double = NAN);
  size_t putBool(const char *, bool);
  bool getBool(const char *, bool = false);

  size_t putString(const char *, const char *);
  size_t putString(const char *, String);
  String getString(const char *, String = String());
  size_t getString(const char *, char *, size_t);
  size_t putBytes(const char *, const void *, size_t);
  size_t getBytes(const char *, void *, size_t);
  size_t getBytesLength(const char *);
  size_t freeEntries();
};
