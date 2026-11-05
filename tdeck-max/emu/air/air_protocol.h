#pragma once
// Version 1: bounded newline-delimited ASCII, integer RF settings, hexadecimal payload.
// All transport deadlines are relative durations, never another process's clock epoch.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct AirFrame {
  uint64_t id = 0, durationUs = 0, headerUs = 0;
  uint32_t frequency = 869618000, bandwidth = 62500;
  unsigned sf = 8, cr = 8, syncWord = 0x1424, preamble = 16;
  bool implicit = false, crc = true, ldro = false, invertIQ = false;
  std::vector<uint8_t> payload;
};
namespace air {
inline uint64_t nowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline bool channel(const AirFrame& a, const AirFrame& b) {
  return std::abs(int64_t(a.frequency) - b.frequency) <= 1000 &&
         a.bandwidth == b.bandwidth && a.sf == b.sf;
}
inline std::string hex(const std::vector<uint8_t>& bytes) {
  if (bytes.empty()) return "-";
  std::ostringstream o;
  for (auto b : bytes) o << std::hex << std::setfill('0') << std::setw(2) << unsigned(b);
  return o.str();
}
inline std::string encode(const AirFrame& f) {
  std::ostringstream o;
  o << f.durationUs << ' ' << f.headerUs << ' ' << f.frequency << ' ' << f.bandwidth
    << ' ' << f.sf << ' ' << f.cr << ' ' << f.syncWord << ' ' << f.preamble
    << ' ' << f.implicit << ' ' << f.crc << ' ' << f.ldro << ' ' << f.invertIQ
    << ' ' << hex(f.payload);
  return o.str();
}
inline bool decode(std::istream& in, AirFrame& f) {
  std::string h, extra;
  unsigned ih, crc, ldro, iq;
  if (!(in >> f.durationUs >> f.headerUs >> f.frequency >> f.bandwidth >> f.sf >> f.cr
        >> f.syncWord >> f.preamble >> ih >> crc >> ldro >> iq >> h)) return false;
  if (in >> extra) return false;
  if (!f.durationUs || f.durationUs > 120000000 || f.headerUs > f.durationUs ||
      f.sf < 5 || f.sf > 12 || f.cr < 4 || f.cr > 8 || !f.bandwidth ||
      f.bandwidth > 500000 || f.syncWord > 65535 || f.preamble > 65535 ||
      ih > 1 || crc > 1 || ldro > 1 || iq > 1 || h.size() > 510) return false;
  f.implicit = ih; f.crc = crc; f.ldro = ldro; f.invertIQ = iq;
  f.payload.clear();
  if (h == "-") return true;
  if (h.size() % 2) return false;
  auto digit = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < h.size(); i += 2) {
    int a = digit(h[i]), b = digit(h[i+1]);
    if (a < 0 || b < 0) return false;
    f.payload.push_back(uint8_t(a * 16 + b));
  }
  return true;
}
inline bool validName(const std::string& s) {
  return !s.empty() && s.size() <= 64 && s.find_first_not_of(
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") == std::string::npos;
}
struct Link { double rssi = -70, snr = 10, loss = 0; bool enabled = true; };
struct Topology {
  Link fallback;
  std::map<std::pair<std::string,std::string>,Link> links;
  Link get(const std::string& a, const std::string& b) const {
    auto i = links.find({a,b}); return i == links.end() ? fallback : i->second;
  }
  void read(std::istream& input) {
    std::string line; unsigned number = 0;
    while (std::getline(input, line)) {
      ++number; line = line.substr(0, line.find('#'));
      std::istringstream s(line); std::string a, b, value, extra; Link l;
      if (!(s >> a)) continue;
      bool directed = false;
      if (a == "default") {
        if (!(s >> value)) throw std::runtime_error("missing default value");
        if (value == "none") { l.enabled = false; }
        else {
          std::istringstream n(value);
          if (!(n >> l.rssi) || (n >> extra) || !(s >> l.snr >> l.loss))
            throw std::runtime_error("invalid default at line " + std::to_string(number));
        }
      } else {
        if (!(s >> b)) throw std::runtime_error("missing link destination");
        if (b == ">") { directed = true; s >> b; }
        if (!validName(a) || !validName(b) || !(s >> l.rssi >> l.snr))
          throw std::runtime_error("invalid link at line " + std::to_string(number));
        s >> std::ws;
        if (!s.eof() && !(s >> l.loss)) throw std::runtime_error("invalid loss");
      }
      if ((s >> extra) || !std::isfinite(l.rssi) || !std::isfinite(l.snr) ||
          !std::isfinite(l.loss) || l.rssi < -127.5 || l.rssi > 0 ||
          l.snr < -32 || l.snr > 31.75 || l.loss < 0 || l.loss > 100)
        throw std::runtime_error("invalid topology at line " + std::to_string(number));
      if (a == "default") fallback = l;
      else { links[{a,b}] = l; if (!directed) links[{b,a}] = l; }
    }
  }
};
// Shared deterministic rules for LocalAir and the broker. The caller owns the clock/poll loop.
class Medium {
public:
  using Start = std::function<void(const AirFrame&,double,double)>;
  using End = std::function<void(const AirFrame&,bool)>;
  struct Node { Start start; End end; uint64_t generation; };
  struct Reception { std::string name; Link link; uint64_t generation; bool ok = true, heard = true; };
  struct Flight { AirFrame frame; std::string sender; uint64_t end; std::vector<Reception> receivers; };
  Topology topology;
  std::function<void(const Flight&)> log;
  void add(const std::string& name, Start start, End end) {
    if (nodes.count(name)) throw std::invalid_argument("duplicate air node: " + name);
    nodes[name] = {std::move(start),std::move(end),++generation};
  }
  void remove(const std::string& name) { nodes.erase(name); }
  void transmit(const std::string& sender, AirFrame frame, uint64_t now) {
    pump(now);
    if (flights.size() >= 1024) return;
    frame.id = ++sequence;
    Flight f{frame,sender,now + frame.durationUs,{}};
    for (auto& n : nodes) {
      if (n.first == sender) continue;
      auto l = topology.get(sender,n.first);
      Reception r{n.first,l,n.second.generation,true,
        l.enabled && std::uniform_real_distribution<double>(0,100)(rng) >= l.loss};
      if (!r.heard) { r.ok = false; f.receivers.push_back(r); continue; }
      for (auto& old : flights) if (channel(frame,old.frame)) {
        for (auto& q : old.receivers) if (q.heard && q.name == r.name && q.generation == r.generation) {
          if (r.link.rssi - q.link.rssi < 6) r.ok = false;
          if (q.link.rssi - r.link.rssi < 6) q.ok = false;
        }
      }
      f.receivers.push_back(r);
      n.second.start(frame,l.rssi,l.snr);
    }
    flights.push_back(std::move(f));
  }
  void pump(uint64_t now) {
    for (size_t i = 0; i < flights.size();) {
      if (flights[i].end > now) { ++i; continue; }
      auto f = std::move(flights[i]); flights.erase(flights.begin() + i);
      for (auto& r : f.receivers) {
        auto n = nodes.find(r.name);
        if (r.heard && n != nodes.end() && n->second.generation == r.generation)
          n->second.end(f.frame,r.ok);
      }
      if (log) log(f);
    }
  }
private:
  std::map<std::string,Node> nodes;
  std::vector<Flight> flights;
  uint64_t sequence = 0, generation = 0;
  std::mt19937 rng{0x1262};
};
} // namespace air
