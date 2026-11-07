#include "HardwareSerial.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <mutex>
#include <poll.h>
#include <thread>
#include <unistd.h>
struct HardwareSerial::State {
  std::mutex mutex;
  std::deque<uint8_t> input;
  std::atomic<bool> stop{false};
  std::thread reader;
  std::atomic<size_t> written{0};
  ~State() {
    stop = true;
    if (reader.joinable())
      reader.join();
  }
};
namespace {
std::mutex stdoutMutex;
}
HardwareSerial Serial(0), Serial2(2);
HardwareSerial::HardwareSerial(int port)
    : state_(std::make_shared<State>()), port_(port) {}
HardwareSerial::~HardwareSerial() = default;
void HardwareSerial::begin(unsigned long baud, uint32_t, int8_t, int8_t, bool,
                           unsigned long, uint8_t) {
  emuUart().open = true;
  emuUart().baud = baud;
  if (port_ != 0 || state_->reader.joinable())
    return;
  {
    std::lock_guard<std::mutex> g(stdoutMutex);
    setvbuf(stdout, nullptr, _IOLBF, 0);
  }
  auto s = state_.get();
  s->reader = std::thread([s] {
    while (!s->stop) {
      pollfd fd{STDIN_FILENO, POLLIN, 0};
      int ready = poll(&fd, 1, 20);
      if (ready < 0)
        break;
      if (!ready)
        continue;
      if (fd.revents & (POLLIN | POLLHUP)) {
        uint8_t buf[256];
        ssize_t n = ::read(STDIN_FILENO, buf, sizeof buf);
        if (n <= 0)
          break;
        std::lock_guard<std::mutex> g(s->mutex);
        s->input.insert(s->input.end(), buf, buf + n);
      } else if (fd.revents & (POLLERR | POLLNVAL))
        break;
    }
  });
}
size_t HardwareSerial::write(uint8_t b) { return write(&b, 1); }
// Per-port UART extras for device models (kept outside State so the original layout is untouched).
HardwareSerial::Uart &HardwareSerial::emuUart() const {
  static Uart ports[3];
  return ports[port_ < 0 || port_ > 2 ? 2 : port_];
}
void HardwareSerial::end() {
  emuUart().open = false;
  if (port_ == 0)
    return;
  std::lock_guard<std::mutex> g(state_->mutex);
  state_->input.clear();
}
bool HardwareSerial::emuOpen() const { return emuUart().open; }
unsigned long HardwareSerial::emuBaud() const { return emuUart().baud; }
void HardwareSerial::emuOnWrite(std::function<void(const uint8_t *, size_t)> hook) {
  emuUart().onWrite = std::move(hook);
}
size_t HardwareSerial::write(const uint8_t *p, size_t n) {
  if (!p)
    return 0;
  if (port_ != 0) {
    state_->written += n;
    if (emuUart().onWrite)
      emuUart().onWrite(p, n);
    return n;
  }
  std::lock_guard<std::mutex> g(stdoutMutex);
  size_t count = fwrite(p, 1, n, stdout);
  state_->written += count;
  return count;
}
int HardwareSerial::available() {
  std::lock_guard<std::mutex> g(state_->mutex);
  return state_->input.size();
}
int HardwareSerial::read() {
  std::lock_guard<std::mutex> g(state_->mutex);
  if (state_->input.empty())
    return -1;
  int c = state_->input.front();
  state_->input.pop_front();
  return c;
}
int HardwareSerial::peek() {
  std::lock_guard<std::mutex> g(state_->mutex);
  return state_->input.empty() ? -1 : state_->input.front();
}
void HardwareSerial::flush() {
  if (port_ == 0) {
    std::lock_guard<std::mutex> g(stdoutMutex);
    fflush(stdout);
  }
}
void HardwareSerial::feed(const uint8_t *p, size_t n) {
  if (!p)
    return;
  std::lock_guard<std::mutex> g(state_->mutex);
  state_->input.insert(state_->input.end(), p, p + n);
}
size_t HardwareSerial::emuBytesWritten() const { return state_->written; }
void emuSerial2Feed(const uint8_t *p, size_t n) { Serial2.feed(p, n); }
