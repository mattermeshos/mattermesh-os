#include "Preferences.h"
#include "emu_hal.h"
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <unistd.h>
#include <vector>
namespace {
struct Value {
  PreferenceType type;
  std::vector<uint8_t> bytes;
};
using Store = std::map<std::string, std::map<std::string, Value>>;
std::mutex nvsMutex;
bool valid(const char *s) { return s && *s && strlen(s) <= 15; }
std::string hex(const uint8_t *p, size_t n) {
  const char *d = "0123456789abcdef";
  std::string s;
  for (size_t i = 0; i < n; ++i) {
    s += d[p[i] >> 4];
    s += d[p[i] & 15];
  }
  return s;
}
bool unhex(const std::string &s, std::vector<uint8_t> &out) {
  if (s.size() % 2)
    return false;
  out.clear();
  auto digit = [](char c) {
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                  : -1;
  };
  for (size_t i = 0; i < s.size(); i += 2) {
    int a = digit(s[i]), b = digit(s[i + 1]);
    if (a < 0 || b < 0)
      return false;
    out.push_back((a << 4) | b);
  }
  return true;
}
Store load() {
  Store result;
  std::ifstream in(emu::nvsPath());
  std::string line;
  if (!std::getline(in, line) || line != "MMNVS1")
    return result;
  while (std::getline(in, line)) {
    std::istringstream row(line);
    char kind;
    std::string ns, key, encoded;
    int type;
    if (!(row >> kind >> std::quoted(ns)) || !valid(ns.c_str()))
      continue;
    if (kind == 'N') {
      result[ns];
      continue;
    }
    if (kind != 'V' || !(row >> std::quoted(key) >> type >> encoded) ||
        !valid(key.c_str()) || type < 0 || type >= PT_INVALID)
      continue;
    Value v{PreferenceType(type), {}};
    if (unhex(encoded, v.bytes))
      result[ns][key] = std::move(v);
  }
  return result;
}
bool save(const Store &store) {
  std::ostringstream out;
  out << "MMNVS1\n";
  for (auto &ns : store) {
    out << "N " << std::quoted(ns.first) << '\n';
    for (auto &v : ns.second)
      out << "V " << std::quoted(ns.first) << ' ' << std::quoted(v.first) << ' '
          << int(v.second.type) << ' '
          << hex(v.second.bytes.data(), v.second.bytes.size()) << '\n';
  }
  auto path = emu::nvsPath(), pattern = path + ".tmp.XXXXXX";
  std::vector<char> name(pattern.begin(), pattern.end());
  name.push_back(0);
  int fd = mkstemp(name.data());
  if (fd < 0)
    return false;
  std::string text = out.str();
  size_t offset = 0;
  bool ok = true;
  while (offset < text.size()) {
    ssize_t count = ::write(fd, text.data() + offset, text.size() - offset);
    if (count <= 0) {
      ok = false;
      break;
    }
    offset += count;
  }
  if (ok && fsync(fd))
    ok = false;
  if (::close(fd))
    ok = false;
  if (ok && ::rename(name.data(), path.c_str()))
    ok = false;
  if (!ok)
    unlink(name.data());
  return ok;
}
const Value *lookup(const Store &s, const std::string &ns, const char *key) {
  if (!valid(key))
    return nullptr;
  auto n = s.find(ns);
  if (n == s.end())
    return nullptr;
  auto k = n->second.find(key);
  return k == n->second.end() ? nullptr : &k->second;
}
} // namespace
bool Preferences::begin(const char *name, bool readOnly, const char *) {
  if (started_ || !valid(name))
    return false;
  std::lock_guard<std::mutex> g(nvsMutex);
  auto store = load();
  auto it = store.find(name);
  if (it == store.end()) {
    if (readOnly)
      return false;
    store[name];
    if (!save(store))
      return false;
  }
  ns_ = name;
  started_ = true;
  readOnly_ = readOnly;
  return true;
}
bool Preferences::clear() {
  if (!started_ || readOnly_)
    return false;
  std::lock_guard<std::mutex> g(nvsMutex);
  auto s = load();
  s[ns_].clear();
  return save(s);
}
bool Preferences::remove(const char *key) {
  if (!started_ || readOnly_ || !valid(key))
    return false;
  std::lock_guard<std::mutex> g(nvsMutex);
  auto s = load();
  if (!s[ns_].erase(key))
    return false;
  return save(s);
}
bool Preferences::put(const char *key, PreferenceType type, const void *data,
                      size_t n) {
  if (!started_ || readOnly_ || !valid(key) || !data || !n)
    return false;
  // Tests: MM_EMU_FAIL_KEY=<namespace>/<key> makes every write of that key fail, like a full or broken NVS.
  if (const char *fail = getenv("MM_EMU_FAIL_KEY"))
    if (ns_ + "/" + key == fail)
      return false;
  std::lock_guard<std::mutex> g(nvsMutex);
  auto store = load();
  auto p = static_cast<const uint8_t *>(data);
  store[ns_][key] = {type, std::vector<uint8_t>(p, p + n)};
  return save(store);
}
bool Preferences::get(const char *key, PreferenceType type, void *data,
                      size_t n) {
  if (!started_ || !data)
    return false;
  std::lock_guard<std::mutex> g(nvsMutex);
  auto store = load();
  auto v = lookup(store, ns_, key);
  if (!v || v->type != type || v->bytes.size() != n)
    return false;
  std::memcpy(data, v->bytes.data(), n);
  return true;
}
PreferenceType Preferences::getType(const char *key) {
  if (!started_)
    return PT_INVALID;
  std::lock_guard<std::mutex> g(nvsMutex);
  auto s = load();
  auto v = lookup(s, ns_, key);
  return v ? v->type : PT_INVALID;
}
bool Preferences::isKey(const char *key) { return getType(key) != PT_INVALID; }
#define NUMBER(Name, Type, Tag)                                                \
  size_t Preferences::put##Name(const char *key, Type value) {                 \
    return put(key, Tag, &value, sizeof value) ? sizeof value : 0;             \
  }                                                                            \
  Type Preferences::get##Name(const char *key, Type def) {                     \
    Type v{};                                                                  \
    return get(key, Tag, &v, sizeof v) ? v : def;                              \
  }
NUMBER(Char, int8_t, PT_I8)
NUMBER(UChar, uint8_t, PT_U8)
NUMBER(Short, int16_t, PT_I16)
NUMBER(UShort, uint16_t, PT_U16)
NUMBER(Int, int32_t, PT_I32)
NUMBER(UInt, uint32_t, PT_U32)
NUMBER(Long, int32_t, PT_I32)
NUMBER(ULong, uint32_t, PT_U32)
NUMBER(Long64, int64_t, PT_I64)
NUMBER(ULong64, uint64_t, PT_U64)
NUMBER(Float, float, PT_BLOB)
NUMBER(Double, double, PT_BLOB)
#undef NUMBER
size_t Preferences::putBool(const char *key, bool v) {
  return putUChar(key, v ? 1 : 0);
}
bool Preferences::getBool(const char *key, bool v) {
  return getUChar(key, v ? 1 : 0) == 1;
}
size_t Preferences::putString(const char *key, const char *v) {
  if (!v)
    return 0;
  size_t n = strlen(v);
  return put(key, PT_STR, v, n + 1) ? n : 0;
}
size_t Preferences::putString(const char *key, String v) {
  return putString(key, v.c_str());
}
String Preferences::getString(const char *key, String def) {
  if (!started_)
    return def;
  std::lock_guard<std::mutex> g(nvsMutex);
  auto s = load();
  auto v = lookup(s, ns_, key);
  if (!v || v->type != PT_STR || v->bytes.empty() || v->bytes.back() != 0)
    return def;
  return String(reinterpret_cast<const char *>(v->bytes.data()));
}
size_t Preferences::getString(const char *key, char *out, size_t n) {
  if (!started_ || !out || !n)
    return 0;
  std::lock_guard<std::mutex> g(nvsMutex);
  auto s = load();
  auto v = lookup(s, ns_, key);
  if (!v || v->type != PT_STR || v->bytes.size() > n)
    return 0;
  std::memcpy(out, v->bytes.data(), v->bytes.size());
  return v->bytes.size();
}
size_t Preferences::putBytes(const char *key, const void *p, size_t n) {
  return put(key, PT_BLOB, p, n) ? n : 0;
}
size_t Preferences::getBytesLength(const char *key) {
  if (!started_)
    return 0;
  std::lock_guard<std::mutex> g(nvsMutex);
  auto s = load();
  auto v = lookup(s, ns_, key);
  return v && v->type == PT_BLOB ? v->bytes.size() : 0;
}
size_t Preferences::getBytes(const char *key, void *p, size_t n) {
  if (!started_)
    return 0;
  std::lock_guard<std::mutex> g(nvsMutex);
  auto s = load();
  auto v = lookup(s, ns_, key);
  if (!v || v->type != PT_BLOB)
    return 0;
  if (!p || !n)
    return v->bytes.size();
  if (n < v->bytes.size())
    return 0;
  std::memcpy(p, v->bytes.data(), v->bytes.size());
  return v->bytes.size();
}
size_t Preferences::freeEntries() {
  std::lock_guard<std::mutex> g(nvsMutex);
  auto s = load();
  size_t count = 0;
  for (auto &ns : s)
    count += ns.second.size();
  return count < 1024 ? 1024 - count : 0;
}
// nvs.h stub (emu/include/nvs.h): true when any namespace holds a value, as nvs_entry_find on the ESP32.
bool emuNvsHasEntries() {
  std::lock_guard<std::mutex> g(nvsMutex);
  for (auto &ns : load())
    if (!ns.second.empty())
      return true;
  return false;
}
