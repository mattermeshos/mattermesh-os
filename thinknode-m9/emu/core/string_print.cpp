#include "Arduino.h"
#include <climits>
#include <cstdarg>
#include <vector>
const String emptyString;
std::string String::number(unsigned long long v, unsigned base) {
  const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
  char b[65];
  char *p = b + sizeof b;
  do {
    *--p = digits[v % base];
    v /= base;
  } while (v);
  return std::string(p, b + sizeof b);
}
namespace {
// Arduino-ESP32 dtostrf rounding and width, with bounded precision for host
// safety.
std::string fixedNumber(double value, unsigned precision, bool pad) {
  if (std::isnan(value))
    return "nan";
  if (std::isinf(value))
    return "inf";
  precision = std::min(precision, 254U);
  bool negative = value < 0;
  double number = negative ? -value : value;
  double rounding = 0.5;
  for (unsigned i = 0; i < precision; ++i)
    rounding /= 10;
  number += rounding;
  double ten = 1;
  unsigned digits = 1;
  while (number / ten >= 10 && ten <= 1e307) {
    ten *= 10;
    ++digits;
  }
  number /= ten;
  std::string out;
  if (pad && precision == 0 && digits + (negative ? 1 : 0) < 2)
    out += ' ';
  if (negative)
    out += '-';
  for (unsigned left = digits + precision; left > 0; --left) {
    unsigned digit = std::min(9U, static_cast<unsigned>(number));
    out += char('0' + digit);
    if (left - 1 == precision && precision)
      out += '.';
    number = (number - digit) * 10;
  }
  return out;
}
} // namespace
String::String(double v, unsigned decimals)
    : s_(fixedNumber(v, decimals, true)) {}
bool String::equalsIgnoreCase(const String &v) const {
  return length() == v.length() &&
         std::equal(begin(), end(), v.begin(),
                    [](unsigned char a, unsigned char b) {
                      return std::tolower(a) == std::tolower(b);
                    });
}
void String::getBytes(unsigned char *p, unsigned n, unsigned from) const {
  if (!p || !n)
    return;
  unsigned count = from < length() ? std::min(n - 1, length() - from) : 0;
  if (count)
    std::memcpy(p, s_.data() + from, count);
  p[count] = 0;
}
int String::indexOf(char c, unsigned from) const {
  if (from >= length())
    return -1;
  auto p = s_.find(c, from);
  return p == std::string::npos ? -1 : int(p);
}
int String::indexOf(const String &s, unsigned from) const {
  if (from >= length())
    return -1;
  auto p = s_.find(s.s_, from);
  return p == std::string::npos ? -1 : int(p);
}
int String::lastIndexOf(char c, unsigned from) const {
  if (from >= length())
    return -1;
  auto p = s_.rfind(c, from);
  return p == std::string::npos ? -1 : int(p);
}
int String::lastIndexOf(const String &s, unsigned from) const {
  if (s.isEmpty() || isEmpty() || s.length() > length())
    return -1;
  auto p = s_.rfind(s.s_, std::min(from, length() - s.length()));
  return p == std::string::npos ? -1 : int(p);
}
String String::substring(unsigned from, unsigned to) const {
  if (from > to)
    std::swap(from, to);
  from = std::min(from, length());
  to = std::min(to, length());
  return String(s_.data() + from, to - from);
}
void String::trim() {
  auto a = s_.find_first_not_of(" \t\n\r\f\v");
  if (a == std::string::npos) {
    s_.clear();
    return;
  }
  s_ = s_.substr(a, s_.find_last_not_of(" \t\n\r\f\v") - a + 1);
}
void String::toLowerCase() {
  for (char &c : s_)
    c = std::tolower(static_cast<unsigned char>(c));
}
void String::toUpperCase() {
  for (char &c : s_)
    c = std::toupper(static_cast<unsigned char>(c));
}
void String::replace(const String &a, const String &b) {
  // Snapshot arguments so replace(s, ...) and replace(..., s) are safe.
  auto needle = a.s_, replacement = b.s_;
  if (needle.empty())
    return;
  size_t p = 0;
  while ((p = s_.find(needle, p)) != std::string::npos) {
    s_.replace(p, needle.size(), replacement);
    p += replacement.size();
  }
}
size_t Print::write(const uint8_t *p, size_t n) {
  if (!p)
    return 0;
  size_t count = 0;
  while (count < n) {
    if (!write(p[count]))
      break;
    ++count;
  }
  return count;
}
size_t Print::printf(const char *fmt, ...) {
  if (!fmt)
    return 0;
  va_list a, b;
  va_start(a, fmt);
  va_copy(b, a);
  int n = vsnprintf(nullptr, 0, fmt, a);
  va_end(a);
  if (n < 0) {
    va_end(b);
    return 0;
  }
  std::vector<char> buf(size_t(n) + 1);
  vsnprintf(buf.data(), buf.size(), fmt, b);
  va_end(b);
  return write(buf.data(), n);
}
size_t Print::printInteger(unsigned long long value, int base, bool negative) {
  if (base == 0)
    return write(uint8_t(value));
  if (base < 2 || base > 36)
    base = 10;
  if (negative)
    value = 0 - value;
  char buf[65];
  char *p = buf + sizeof buf;
  do {
    *--p = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[value % base];
    value /= base;
  } while (value);
  size_t prefix = negative ? write('-') : 0;
  return prefix + write(p, size_t(buf + sizeof buf - p));
}
size_t Print::print(double v, int digits) {
  if (std::isnan(v))
    return write("nan");
  if (std::isinf(v))
    return write("inf");
  if (v > 4294967040.0 || v < -4294967040.0)
    return write("ovf");
  auto text = fixedNumber(v, static_cast<uint8_t>(digits), false);
  return write(text.c_str());
}
size_t Print::print(struct tm *t, const char *fmt) {
  char buf[128];
  return t && std::strftime(buf, sizeof buf, fmt ? fmt : "%c", t) ? write(buf)
                                                                  : 0;
}
int Stream::timedRead() {
  auto start = millis();
  int c;
  do {
    if ((c = read()) >= 0)
      return c;
    if (!_timeout)
      break;
    delay(1);
  } while (millis() - start < _timeout);
  return -1;
}
int Stream::timedPeek() {
  auto start = millis();
  int c;
  do {
    if ((c = peek()) >= 0)
      return c;
    if (!_timeout)
      break;
    delay(1);
  } while (millis() - start < _timeout);
  return -1;
}
size_t Stream::readBytes(char *p, size_t n) {
  if (!p)
    return 0;
  size_t i = 0;
  for (; i < n; ++i) {
    int c = timedRead();
    if (c < 0)
      break;
    p[i] = char(c);
  }
  return i;
}
size_t Stream::readBytesUntil(char stop, char *p, size_t n) {
  if (!p)
    return 0;
  size_t i = 0;
  for (; i < n; ++i) {
    int c = timedRead();
    if (c < 0 || c == stop)
      break;
    p[i] = char(c);
  }
  return i;
}
String Stream::readString() {
  String s;
  int c;
  while ((c = timedRead()) >= 0)
    s += char(c);
  return s;
}
String Stream::readStringUntil(char stop) {
  String s;
  int c;
  while ((c = timedRead()) >= 0 && c != stop)
    s += char(c);
  return s;
}

// ---- Arduino-ESP32 extras added at integration (Stream parsing, number formatting, configTzTime) ----
#include <Arduino.h>
#include <cctype>
#include <cstdlib>
#include <ctime>
long Stream::parseInt(){
 int c;do{c=timedPeek();if(c<0)return 0;if(c=='-'||isdigit(c))break;read();}while(true);
 bool negative=false;long value=0;if(c=='-'){negative=true;read();}
 while((c=timedPeek())>=0&&isdigit(c)){value=value*10+(c-'0');read();}
 return negative?-value:value;
}
float Stream::parseFloat(){
 int c;do{c=timedPeek();if(c<0)return 0;if(c=='-'||c=='.'||isdigit(c))break;read();}while(true);
 String text;while((c=timedPeek())>=0&&(isdigit(c)||c=='.'||(c=='-'&&!text.length()))){text+=char(c);read();}
 return text.toFloat();
}
bool Stream::find(const char *target){
 size_t n=strlen(target),matched=0;if(!n)return true;int c;
 while((c=timedRead())>=0){if(c==target[matched]){if(++matched==n)return true;}else matched=c==target[0]?1:0;}
 return false;
}
static char *formatInteger(unsigned long long value,bool negative,char *out,int base){
 if(base<2||base>36){out[0]=0;return out;}char tmp[72];int n=0;do{int d=int(value%base);tmp[n++]=char(d<10?'0'+d:'a'+d-10);value/=base;}while(value);
 int k=0;if(negative)out[k++]='-';while(n)out[k++]=tmp[--n];out[k]=0;return out;
}
char *ltoa(long v,char *out,int base){return base==10&&v<0?formatInteger(0ULL-(unsigned long long)v,true,out,10):formatInteger((unsigned long)v,false,out,base);}
char *ultoa(unsigned long v,char *out,int base){return formatInteger(v,false,out,base);}
char *itoa(int v,char *out,int base){return ltoa(v,out,base);}
char *utoa(unsigned v,char *out,int base){return formatInteger(v,false,out,base);}
char *dtostrf(double v,signed char width,unsigned char precision,char *out){sprintf(out,"%*.*f",width,precision,v);return out;}
void configTzTime(const char *tz,const char*,const char*,const char*){if(tz){setenv("TZ",tz,1);tzset();}}
