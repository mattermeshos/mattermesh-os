#include "Arduino.h"
#include "driver/gpio.h"
#include "emu_hal.h"
#include "soc/gpio_reg.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <unistd.h>
#include <vector>
namespace {
using Clock = std::chrono::steady_clock;
const auto started = Clock::now();
const auto mainThread = std::this_thread::get_id();
std::mutex halMutex;
struct Pin {
  int mode = INPUT, level = 0, mv = 0, isrMode = CHANGE;
  bool driven = false;
  void (*isr)() = nullptr;
  std::vector<std::function<void(int)>> writes, analog;
};
std::map<int, Pin> pins;
std::map<uint8_t, emu::I2cDevice *> i2c;
std::map<int, emu::SpiDevice *> spi;
std::vector<std::function<void()>> pumps;
std::atomic<bool> quit{false}, sd{true};
std::string data = std::getenv("MM_EMU_DATA_DIR")
                       ? std::getenv("MM_EMU_DATA_DIR")
                       : "emu/data-plus";
std::atomic<uint32_t> cpu{240};
std::mutex randomMutex;
std::mt19937 rng{std::random_device{}()};
int level(const Pin &p) {
  return p.driven ? p.level : p.mode == INPUT_PULLUP ? HIGH : LOW;
}
} // namespace
namespace emu {
void stopTasks();
uint64_t micros64() {
  return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() -
                                                               started)
      .count();
}
uint32_t millis32() { return uint32_t(micros64() / 1000); }
bool isMainThread() { return std::this_thread::get_id() == mainThread; }
int gpioLevel(int pin) {
  std::lock_guard<std::mutex> g(halMutex);
  return level(pins[pin]);
}
void gpioDrive(int pin, int value) {
  void (*isr)() = nullptr;
  {
    std::lock_guard<std::mutex> g(halMutex);
    auto &p = pins[pin];
    int before = level(p);
    p.level = !!value;
    p.driven = true;
    bool edge = before != p.level;
    if (p.isr &&
        ((p.isrMode == CHANGE && edge) ||
         (p.isrMode == RISING && edge && p.level) ||
         (p.isrMode == FALLING && edge && !p.level) ||
         (p.isrMode == ONLOW && !p.level) || (p.isrMode == ONHIGH && p.level)))
      isr = p.isr;
  }
  if (isr)
    isr();
}
void onGpioWrite(int pin, std::function<void(int)> f) {
  std::lock_guard<std::mutex> g(halMutex);
  pins[pin].writes.push_back(std::move(f));
}
void setAnalogMilliVolts(int pin, int mv) {
  std::lock_guard<std::mutex> g(halMutex);
  pins[pin].mv = std::max(0, mv);
}
void onAnalogWrite(int pin, std::function<void(int)> f) {
  std::lock_guard<std::mutex> g(halMutex);
  pins[pin].analog.push_back(std::move(f));
}
void i2cAttach(uint8_t a, I2cDevice *d) {
  std::lock_guard<std::mutex> g(halMutex);
  if (d)
    i2c[a] = d;
  else
    i2c.erase(a);
}
I2cDevice *i2cLookup(uint8_t a) {
  std::lock_guard<std::mutex> g(halMutex);
  auto p = i2c.find(a);
  return p == i2c.end() ? nullptr : p->second;
}
void spiAttach(int pin, SpiDevice *d) {
  bool selected = false;
  {
    std::lock_guard<std::mutex> g(halMutex);
    if (d)
      spi[pin] = d;
    else
      spi.erase(pin);
    selected = level(pins[pin]) == LOW;
  }
  if (d)
    d->select(selected);
}
std::vector<SpiDevice *> spiSelected() {
  std::lock_guard<std::mutex> g(halMutex);
  std::vector<SpiDevice *> result;
  for (auto &p : spi)
    if (level(pins[p.first]) == LOW)
      result.push_back(p.second);
  return result;
}
void addPumpHook(std::function<void()> f) {
  std::lock_guard<std::mutex> g(halMutex);
  pumps.push_back(std::move(f));
}
void pump() {
  if (!isMainThread())
    return;
  static thread_local bool active = false;
  if (active)
    return;
  std::vector<std::function<void()>> list;
  {
    std::lock_guard<std::mutex> g(halMutex);
    list = pumps;
  }
  active = true;
  try {
    for (auto &f : list)
      f();
  } catch (...) {
    active = false;
    throw;
  }
  active = false;
}
bool quitRequested() { return quit; }
void requestQuit() { quit = true; }
const std::string &dataDir() { return data; }
void setDataDir(const std::string &path) {
  data = path;
  std::filesystem::create_directories(sdRoot());
}
std::string sdRoot() {
  auto p = data + "/sd";
  std::filesystem::create_directories(p);
  return p;
}
std::string nvsPath() {
  std::filesystem::create_directories(data);
  return data + "/nvs.json";
}
void setSdPresent(bool present) { sd = present; }
bool sdPresent() { return sd; }
} // namespace emu
unsigned long millis() { return emu::millis32(); }
unsigned long micros() { return uint32_t(emu::micros64()); }
void delay(unsigned long ms) {
  if (!emu::isMainThread()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return;
  }
  auto end = Clock::now() + std::chrono::milliseconds(ms);
  do {
    emu::pump();
    auto left = end - Clock::now();
    if (left <= Clock::duration::zero())
      break;
    std::this_thread::sleep_for(
        std::min(left, std::chrono::duration_cast<Clock::duration>(
                           std::chrono::milliseconds(5))));
  } while (true);
}
void delayMicroseconds(unsigned int us) {
  std::this_thread::sleep_for(std::chrono::microseconds(us));
}
void yield() {
  emu::pump();
  std::this_thread::yield();
}
void pinMode(uint8_t pin, uint8_t mode) {
  std::lock_guard<std::mutex> g(halMutex);
  pins[pin].mode = mode;
}
void digitalWrite(uint8_t pin, uint8_t value) {
  std::vector<std::function<void(int)>> hooks;
  emu::SpiDevice *device = nullptr;
  {
    std::lock_guard<std::mutex> g(halMutex);
    auto &p = pins[pin];
    p.level = !!value;
    p.driven = true;
    hooks = p.writes;
    auto it = spi.find(pin);
    if (it != spi.end())
      device = it->second;
  }
  if (device)
    device->select(!value);
  for (auto &f : hooks)
    f(!!value);
}
int digitalRead(uint8_t pin) { return emu::gpioLevel(pin); }
void attachInterrupt(uint8_t pin, void (*f)(), int mode) {
  std::lock_guard<std::mutex> g(halMutex);
  pins[pin].isr = f;
  pins[pin].isrMode = mode;
}
void detachInterrupt(uint8_t pin) {
  std::lock_guard<std::mutex> g(halMutex);
  pins[pin].isr = nullptr;
}
uint32_t analogReadMilliVolts(uint8_t pin) {
  std::lock_guard<std::mutex> g(halMutex);
  return pins[pin].mv;
}
uint16_t analogRead(uint8_t pin) {
  return std::min(4095U, analogReadMilliVolts(pin) * 4095U / 3300U);
}
void analogWrite(uint8_t pin, int value) {
  std::vector<std::function<void(int)>> hooks;
  {
    std::lock_guard<std::mutex> g(halMutex);
    hooks = pins[pin].analog;
  }
  for (auto &f : hooks)
    f(value);
}
long random(long hi) { return random(0, hi); }
long random(long lo, long hi) {
  if (lo >= hi)
    return lo;
  std::lock_guard<std::mutex> g(randomMutex);
  return std::uniform_int_distribution<long>(lo, hi - 1)(rng);
}
void randomSeed(unsigned long seed) {
  if (seed) {
    std::lock_guard<std::mutex> g(randomMutex);
    rng.seed(seed);
  }
}
uint32_t esp_random() {
  std::lock_guard<std::mutex> g(randomMutex);
  return rng();
}
void esp_fill_random(void *p, size_t n) {
  auto b = static_cast<uint8_t *>(p);
  while (n) {
    uint32_t v = esp_random();
    size_t take = std::min(n, sizeof v);
    std::memcpy(b, &v, take);
    b += take;
    n -= take;
  }
}
esp_reset_reason_t esp_reset_reason() { return ESP_RST_POWERON; }
int64_t esp_timer_get_time() { return emu::micros64(); }
uint32_t esp_crc32_le(uint32_t crc, const uint8_t *p, uint32_t n) {
  crc = ~crc;
  while (n--) {
    crc ^= *p++;
    for (int i = 0; i < 8; ++i)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
  }
  return ~crc;
}
bool setCpuFrequencyMhz(uint32_t v) {
  cpu = v;
  return true;
}
uint32_t getCpuFrequencyMhz() { return cpu; }
void configTime(
    long offset, int daylight, const char *, const char *,
    const char *) { // Host timezone is used; never alter the host clock.
  (void)offset;
  (void)daylight;
}
bool getLocalTime(struct tm *out, uint32_t ms) {
  if (!out)
    return false;
  auto start = millis();
  do {
    time_t t = std::time(nullptr);
    localtime_r(&t, out);
    if (out->tm_year > 120)
      return true;
    if (millis() - start >= ms)
      break;
    delay(10);
  } while (true);
  return false;
}
void emuRunLoop(void (*setup)(), void (*loop)()) {
  try {
    setup();
    while (!emu::quitRequested()) {
      loop();
      emu::pump();
    }
  } catch (const emu::TaskExit &) {
  }
  emu::stopTasks();
}
EspClass ESP;
uint32_t EspClass::getFreeHeap() {
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
}
uint32_t EspClass::getFreePsram() {
  return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}
uint32_t EspClass::getPsramSize() {
  return heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
}
void EspClass::restart() {
  // Linux exposes the original NUL-separated argv, including integration flags.
  std::ifstream in("/proc/self/cmdline", std::ios::binary);
  std::vector<std::string> args;
  std::string arg;
  while (std::getline(in, arg, '\0'))
    args.push_back(arg);
  setenv("MM_EMU_DATA_DIR", emu::dataDir().c_str(), 1);
  std::vector<char *> argv;
  for (auto &s : args)
    argv.push_back(s.data());
  argv.push_back(nullptr);
  if (args.empty())
    std::abort();
  execv("/proc/self/exe", argv.data());
  std::abort();
}
uint32_t emuGpioRouting[64]{};
void esp_rom_gpio_connect_out_signal(uint32_t pin, uint32_t signal, bool,
                                     bool) {
  if (pin < 64)
    emuGpioRouting[pin] = signal;
}
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) {
  if (pin < 0 || pin >= 64)
    return ESP_ERR_INVALID_ARG;
  digitalWrite(pin, level);
  return ESP_OK;
}

// Permit the firmware to override this no-op, as with ESP-IDF SNTP.
extern "C" __attribute__((weak)) void sntp_sync_time(struct timeval *) {}
