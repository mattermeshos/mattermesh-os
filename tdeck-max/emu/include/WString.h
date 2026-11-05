#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>
class __FlashStringHelper;
#define PROGMEM
#define PSTR(s) (s)
#define FPSTR(s) reinterpret_cast<const __FlashStringHelper *>(s)
#define F(s) FPSTR(s)
class String {
  std::string s_;
  bool valid_ = true;
  static std::string number(unsigned long long v, unsigned base);
  template <class T> void integer(T v, unsigned base) {
    if (base < 2 || base > 16) {
      s_.clear();
      return;
    }
    if constexpr (std::is_signed_v<T>) {
      if (v < 0) {
        s_ = "-" + number(0 - static_cast<unsigned long long>(v), base);
        return;
      }
    }
    using U = std::make_unsigned_t<T>;
    s_ = number(static_cast<U>(v), base);
  }

public:
  String(const char *p = "") : s_(p ? p : ""), valid_(p != nullptr) {}
  String(const char *p, unsigned n)
      : s_(p ? std::string(p, n) : ""), valid_(p != nullptr) {}
  String(const uint8_t *p, unsigned n)
      : String(reinterpret_cast<const char *>(p), n) {}
  String(const __FlashStringHelper *p)
      : String(reinterpret_cast<const char *>(p)) {}
  explicit String(char c) : s_(1, c) {}
  explicit String(unsigned char v, unsigned char base = 10) {
    integer(v, base);
  }
  explicit String(int v, unsigned char base = 10) { integer(v, base); }
  explicit String(unsigned int v, unsigned char base = 10) { integer(v, base); }
  explicit String(long v, unsigned char base = 10) { integer(v, base); }
  explicit String(unsigned long v, unsigned char base = 10) {
    integer(v, base);
  }
  explicit String(long long v, unsigned char base = 10) { integer(v, base); }
  explicit String(unsigned long long v, unsigned char base = 10) {
    integer(v, base);
  }

  explicit String(double v, unsigned decimals = 2);
  explicit String(float v, unsigned decimals = 2)
      : String(double(v), decimals) {}
  const char *c_str() const { return s_.c_str(); }
  unsigned length() const { return s_.size(); }
  // size_t-typed size(): lets ArduinoJson recognise String on a 64-bit host (length() is unsigned int as on the ESP32).
  size_t size() const { return s_.size(); }
  bool isEmpty() const { return s_.empty(); }
  explicit operator bool() const { return valid_; }
  bool reserve(unsigned n) {
    try {
      s_.reserve(n);
      valid_ = true;
      return true;
    } catch (...) {
      return false;
    }
  }
  void clear() { s_.clear(); }
  bool concat(const char *p, unsigned n) {
    if (!p)
      return false;
    try {
      s_.append(p, n);
      valid_ = true;
      return true;
    } catch (...) {
      return false;
    }
  }
  bool concat(const char *p) { return p && concat(p, std::strlen(p)); }
  bool concat(const uint8_t *p, unsigned n) {
    return concat(reinterpret_cast<const char *>(p), n);
  }
  bool concat(const String &v) {
    return v.valid_ && concat(v.c_str(), v.length());
  }
  bool concat(const __FlashStringHelper *p) {
    return concat(reinterpret_cast<const char *>(p));
  }
  template <class T> bool concat(T v) { return concat(String(v)); }
  template <class T> String &operator+=(const T &v) {
    concat(v);
    return *this;
  }
  friend String operator+(String a, const String &b) {
    a += b;
    return a;
  }
  template <class T> friend String operator+(String a, T b) {
    a.concat(b);
    return a;
  }
  friend String operator+(const char *a, const String &b) {
    return String(a) + b;
  }
  friend String operator+(const __FlashStringHelper *a, const String &b) {
    return String(a) + b;
  }
  template <class T, std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
  friend String operator+(T a, const String &b) {
    return String(a) + b;
  }
  int compareTo(const String &v) const { return s_.compare(v.s_); }
  bool equals(const String &v) const { return s_ == v.s_; }
  bool equals(const char *p) const { return s_ == (p ? p : ""); }
  bool equalsIgnoreCase(const String &v) const;
  bool operator==(const String &v) const { return equals(v); }
  bool operator!=(const String &v) const { return !equals(v); }
  bool operator<(const String &v) const { return s_ < v.s_; }
  bool operator>(const String &v) const { return s_ > v.s_; }
  bool operator<=(const String &v) const { return s_ <= v.s_; }
  bool operator>=(const String &v) const { return s_ >= v.s_; }
  friend bool operator==(const char *a, const String &b) { return b.equals(a); }
  friend bool operator!=(const char *a, const String &b) {
    return !b.equals(a);
  }
  char charAt(unsigned i) const { return i < s_.size() ? s_[i] : 0; }
  char operator[](unsigned i) const { return charAt(i); }
  char &operator[](unsigned i) {
    static thread_local char dummy;
    dummy = 0;
    return i < s_.size() ? s_[i] : dummy;
  }
  void setCharAt(unsigned i, char c) {
    if (i < s_.size())
      s_[i] = c;
  }
  void getBytes(unsigned char *p, unsigned n, unsigned from = 0) const;
  void toCharArray(char *p, unsigned n, unsigned from = 0) const {
    getBytes(reinterpret_cast<unsigned char *>(p), n, from);
  }
  int indexOf(char c, unsigned from = 0) const;
  int indexOf(const String &s, unsigned from = 0) const;
  int lastIndexOf(char c) const { return lastIndexOf(c, length() - 1); }
  int lastIndexOf(char c, unsigned from) const;
  int lastIndexOf(const String &s) const { return lastIndexOf(s, length()); }
  int lastIndexOf(const String &s, unsigned from) const;
  String substring(unsigned from) const { return substring(from, length()); }
  String substring(unsigned from, unsigned to) const;
  bool startsWith(const String &v, unsigned offset = 0) const {
    return offset <= length() && s_.compare(offset, v.length(), v.s_) == 0;
  }
  bool endsWith(const String &v) const {
    return length() >= v.length() &&
           s_.compare(length() - v.length(), v.length(), v.s_) == 0;
  }
  long toInt() const { return std::strtol(c_str(), nullptr, 10); }
  float toFloat() const { return std::strtof(c_str(), nullptr); }
  double toDouble() const { return std::strtod(c_str(), nullptr); }
  void trim();
  void toLowerCase();
  void toUpperCase();
  void replace(char a, char b) { std::replace(s_.begin(), s_.end(), a, b); }
  void replace(const String &a, const String &b);
  void remove(unsigned i) {
    if (i < length())
      s_.erase(i);
  }
  void remove(unsigned i, unsigned n) {
    if (i < length())
      s_.erase(i, n);
  }
  char *begin() { return s_.data(); }
  char *end() { return s_.data() + s_.size(); }
  const char *begin() const { return s_.data(); }
  const char *end() const { return s_.data() + s_.size(); }
};
extern const String emptyString;
