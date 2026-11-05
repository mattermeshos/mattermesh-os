#include "Arduino.h"
#include "IPAddress.h"
#include "Preferences.h"
#include "SD.h"
#include "SPI.h"
#include "Wire.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "emu_hal.h"
#include "esp_crc.h"
#include "esp_rom_gpio.h"
#include "esp_sntp.h"
#include "esp_spi_flash.h"
#include "esp_timer.h"
#include "soc/gpio_reg.h"
#include "soc/soc_memory_layout.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #x "\n";     \
      std::exit(1);                                                            \
    }                                                                          \
  } while (false)
namespace {
std::string testDir;
void pass(const char *s) { std::cout << "PASS " << s << '\n'; }
void child(const char *mode) {
  pid_t pid = fork();
  CHECK(pid >= 0);
  if (!pid) {
    execl("/proc/self/exe", "core-tests", mode, testDir.c_str(), nullptr);
    _exit(127);
  }
  int status = 0;
  CHECK(waitpid(pid, &status, 0) == pid);
  CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
void stringTests() {
  CHECK(String().isEmpty());
  String invalid(static_cast<const char *>(nullptr));
  CHECK(!invalid);
  CHECK(invalid.reserve(0));
  CHECK(bool(invalid));
  CHECK(String('x') == "x");
  CHECK(String(uint8_t(65)) == "65");
  CHECK(String(-42) == "-42");
  CHECK(String(-31, 16) == "-1f");
  CHECK(String(255, 16) == "ff");
  CHECK(String(10, 2) == "1010");
  CHECK(String(8, 8) == "10");
  CHECK(String(1, 1).isEmpty());
  CHECK(String(std::numeric_limits<long long>::min()) ==
        "-9223372036854775808");
  CHECK(String(std::numeric_limits<unsigned long long>::max()) ==
        "18446744073709551615");
  CHECK(String(1.25, 1U) == "1.3");
  CHECK(String(1.0, 0U) == " 1");
  CHECK(String(-1.0, 0U) == "-1");
  CHECK(String(INFINITY) == "inf");
  CHECK(String(NAN) == "nan");
  String s = "abcabc";
  CHECK(s.length() == 6);
  CHECK(s.indexOf('b') == 1);
  CHECK(s.indexOf('b', 2) == 4);
  CHECK(s.indexOf('z') == -1);
  CHECK(s.indexOf("bc", 2) == 4);
  CHECK(s.indexOf("", 6) == -1);
  CHECK(String().indexOf("") == -1);
  CHECK(s.lastIndexOf('a') == 3);
  CHECK(s.lastIndexOf('a', 2) == 0);
  CHECK(s.lastIndexOf('a', 99) == -1);
  CHECK(s.lastIndexOf("bc") == 4);
  CHECK(s.lastIndexOf("bc", 3) == 1);
  CHECK(s.lastIndexOf("") == -1);
  CHECK(s.substring(3) == "abc");
  CHECK(s.substring(99).isEmpty());
  CHECK(s.substring(5, 1) == "bcab");
  CHECK(s.substring(0, 99) == s);
  CHECK(s.startsWith("abc"));
  CHECK(s.startsWith("abc", 3));
  CHECK(!s.startsWith("abc", 4));
  CHECK(s.endsWith("abc"));
  CHECK(s.equals("abcabc"));
  CHECK(String("aBc").equalsIgnoreCase("AbC"));
  CHECK(String("a") < String("b"));
  CHECK(s.charAt(99) == 0);
  s[99] = 'x';
  CHECK(s == "abcabc");
  s.setCharAt(0, 'A');
  CHECK(s[0] == 'A');
  s.setCharAt(99, 'Q');
  char b[4];
  s.toCharArray(b, sizeof b, 1);
  CHECK(std::string(b) == "bca");
  s.toCharArray(b, sizeof b, 99);
  CHECK(b[0] == 0);
  s = " \tAbC\r\n";
  s.trim();
  CHECK(s == "AbC");
  s.toLowerCase();
  CHECK(s == "abc");
  s.toUpperCase();
  CHECK(s == "ABC");
  s.replace('A', 'a');
  s.replace("BC", "xyz");
  CHECK(s == "axyz");
  s.replace("", "oops");
  CHECK(s == "axyz");
  s.remove(1, 2);
  CHECK(s == "az");
  s.remove(1);
  CHECK(s == "a");
  s.remove(99);
  CHECK(s == "a");
  CHECK(String(" -123tail").toInt() == -123);
  CHECK(String("x").toInt() == 0);
  CHECK(String("1.25rest").toFloat() == 1.25f);
  CHECK(String("-2.5").toDouble() == -2.5);
  s = "prefix";
  s += ':';
  s += 12;
  s += String("!");
  CHECK(s == "prefix:12!");
  CHECK("x" + String(3) + ',' + 4 == "x3,4");
  CHECK(12 + String("x") == "12x");
  CHECK('a' + String("bc") == "abc");
  CHECK(String("f=") + 1.25 == "f=1.25");
  CHECK(String(F("flash")) == "flash");
  CHECK(!s.concat(static_cast<const char *>(nullptr)));
  CHECK(s.concat(s));
  CHECK(s == "prefix:12!prefix:12!");
  uint8_t bytes[3] = {'a', 0, 'b'};
  String binary(bytes, 3);
  CHECK(binary.length() == 3 && binary[2] == 'b');
  pass("String constructors, search, bounds, conversion, mutation and "
       "concatenation");
}
struct Capture : Print {
  std::string text;
  using Print::write;
  size_t write(uint8_t b) override {
    text += char(b);
    return 1;
  }
};
void serialTests() {
  Capture out;
  out.print(255, HEX);
  out.print(' ');
  out.println(-42);
  out.print(1.25, 1);
  out.print(' ');
  out.print(NAN);
  CHECK(out.text == "FF -42\r\n1.3 nan");
  Serial.begin(115200);
  CHECK(Serial.printf("Serial printf check: %s %d\n", "ok", 42) == 27);
  Serial.flush();
  CHECK(Serial.available() >= 0);
  Serial.setTimeout(2);
  CHECK(Serial.getTimeout() == 2);
  CHECK(Serial.setTxBufferSize(80) == 80);
  CHECK(Serial.setRxBufferSize(90) == 90);
  Serial.setTxTimeoutMs(4);
  CHECK(Serial.availableForWrite() > 0);
  Serial2.begin(9600, SERIAL_8N1, 44, 43);
  CHECK(Serial2.read() == -1);
  auto before = Serial2.emuBytesWritten();
  CHECK(Serial2.print("discard") == 7);
  CHECK(Serial2.emuBytesWritten() == before + 7);
  const uint8_t nmea[] = "$GPGGA\n";
  emuSerial2Feed(nmea, sizeof(nmea) - 1);
  CHECK(Serial2.available() == 7);
  CHECK(Serial2.peek() == '$');
  char b[8]{};
  CHECK(Serial2.readBytes(b, 7) == 7);
  CHECK(std::string(b) == "$GPGGA\n");
  CHECK(Serial2.read() == -1);
  IPAddress ip;
  CHECK(ip.fromString("192.168.1.2"));
  CHECK(ip.toString() == "192.168.1.2");
  CHECK(!ip.fromString("256.1.2.3"));
  CHECK(ip.toString() == "192.168.1.2");
  CHECK(IPAddress(uint32_t(ip)) == ip);
  pass("Print formatting, Serial printf, Serial2 injection and IPAddress");
}
std::atomic<unsigned> pumps{0};
std::thread::id mainId;
void timeTests() {
  mainId = std::this_thread::get_id();
  emu::addPumpHook([] {
    CHECK(std::this_thread::get_id() == mainId);
    ++pumps;
  });
  auto before = pumps.load();
  uint64_t start = emu::micros64();
  delay(21);
  CHECK(emu::micros64() - start >= 20000);
  CHECK(pumps >= before + 4);
  CHECK(millis() == emu::millis32() || millis() == emu::millis32() + 1);
  before = pumps;
  vTaskDelay(12);
  CHECK(pumps >= before + 3);
  before = pumps;
  yield();
  CHECK(pumps == before + 1);
  std::thread worker([] {
    delay(8);
    vTaskDelay(8);
    yield();
  });
  before = pumps;
  worker.join();
  CHECK(pumps == before);
  CHECK(esp_timer_get_time() > 0);
  CHECK(getCpuFrequencyMhz() == 240);
  CHECK(setCpuFrequencyMhz(160));
  CHECK(getCpuFrequencyMhz() == 160);
  CHECK(setCpuFrequencyMhz(240));
  struct tm now{};
  CHECK(getLocalTime(&now, 0));
  CHECK(now.tm_year > 120);
  CHECK(esp_reset_reason() == ESP_RST_POWERON);
  randomSeed(123);
  auto r = random(1000);
  randomSeed(123);
  CHECK(random(1000) == r);
  CHECK(random(5, 5) == 5);
  uint8_t noise[16];
  esp_fill_random(noise, sizeof noise);
  const uint8_t text[] = "123456789";
  CHECK(esp_crc32_le(0, text, 9) == 0xcbf43926);
  CHECK(esp_crc32_le(esp_crc32_le(0, text, 4), text + 4, 5) == 0xcbf43926);
  size_t flash = 0;
  CHECK(spi_flash_get_chip_size(&flash) == ESP_OK && flash == 16 * 1024 * 1024);
  pass("steady time, main-thread pumping, CPU settings, CRC and host time");
}
int edges = 0;
void edge() {
  ++edges;
  CHECK(digitalRead(5) == emu::gpioLevel(5));
}
void gpioTests() {
  pinMode(5, INPUT_PULLUP);
  CHECK(digitalRead(5) == HIGH);
  attachInterrupt(5, edge, FALLING);
  emu::gpioDrive(5, LOW);
  CHECK(edges == 1);
  emu::gpioDrive(5, LOW);
  CHECK(edges == 1);
  emu::gpioDrive(5, HIGH);
  CHECK(edges == 1);
  attachInterrupt(5, edge, RISING);
  emu::gpioDrive(5, LOW);
  emu::gpioDrive(5, HIGH);
  CHECK(edges == 2);
  attachInterrupt(5, edge, CHANGE);
  emu::gpioDrive(5, LOW);
  emu::gpioDrive(5, HIGH);
  CHECK(edges == 4);
  attachInterrupt(5, edge, ONHIGH);
  emu::gpioDrive(5, HIGH);
  CHECK(edges == 5);
  attachInterrupt(5, edge, ONLOW);
  emu::gpioDrive(5, LOW);
  emu::gpioDrive(5, LOW);
  CHECK(edges == 7);
  detachInterrupt(5);
  emu::gpioDrive(5, HIGH);
  CHECK(edges == 7);
  static int wrote = -1, analog = -1;
  emu::onGpioWrite(7, [](int n) {
    wrote = n;
    CHECK(digitalRead(7) == n);
  });
  digitalWrite(7, HIGH);
  CHECK(wrote == 1);
  CHECK(gpio_set_level(7, 0) == ESP_OK);
  CHECK(wrote == 0);
  emu::setAnalogMilliVolts(4, 1650);
  CHECK(analogReadMilliVolts(4) == 1650);
  CHECK(analogRead(4) == 2047);
  emu::onAnalogWrite(4, [](int n) { analog = n; });
  analogWrite(4, 123);
  CHECK(analog == 123);
  esp_rom_gpio_connect_out_signal(7, 256, false, false);
  CHECK(emuGpioRouting[7] == 256);
  pass(
      "GPIO levels, synchronous ISR modes, analog hooks and routing registers");
}
struct I2c : emu::I2cDevice {
  bool ack = true;
  std::vector<uint8_t> got;
  bool write(const uint8_t *p, size_t n) override {
    got.assign(p, p + n);
    return ack;
  }
  size_t read(uint8_t *p, size_t n) override {
    size_t count = std::min(n, size_t(2));
    if (count)
      p[0] = 11;
    if (count > 1)
      p[1] = 22;
    return count;
  }
};
struct Spi : emu::SpiDevice {
  bool selected = false;
  uint8_t response;
  std::vector<uint8_t> got;
  explicit Spi(uint8_t v) : response(v) {}
  void select(bool b) override { selected = b; }
  uint8_t transfer(uint8_t b) override {
    CHECK(selected);
    got.push_back(b);
    return response;
  }
};
void busTests() {
  I2c d;
  emu::i2cAttach(0x55, &d);
  Wire.begin(18, 8, 400000);
  CHECK(Wire.getClock() == 400000);
  Wire.beginTransmission(0x55);
  Wire.write(0);
  Wire.write(1);
  const uint8_t packet[] = {2, 3};
  Wire.write(packet, 2);
  CHECK(Wire.endTransmission(false) == 0);
  CHECK(d.got == std::vector<uint8_t>({0, 1, 2, 3}));
  d.ack = false;
  CHECK(Wire.endTransmission() == 2);
  Wire.beginTransmission(0x54);
  CHECK(Wire.endTransmission() == 2);
  CHECK(Wire.requestFrom(0x55, 5) == 2);
  CHECK(Wire.available() == 2);
  CHECK(Wire.peek() == 11);
  CHECK(Wire.read() == 11 && Wire.read() == 22 && Wire.read() == -1);
  CHECK(Wire.requestFrom(0x54, 2) == 0);
  CHECK(Wire1.begin());
  CHECK(Wire1.requestFrom(0x55, 1, true) == 1);
  CHECK(Wire1.read() == 11);
  emu::i2cAttach(0x55, nullptr);
  Spi a(0xa5), b(0x5a);
  digitalWrite(12, HIGH);
  digitalWrite(9, HIGH);
  emu::spiAttach(12, &a);
  emu::spiAttach(9, &b);
  CHECK(!a.selected && !b.selected);
  CHECK(SPI.transfer(1) == 0xff);
  SPI.begin(40, 38, 41, 12);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  digitalWrite(12, LOW);
  CHECK(a.selected);
  CHECK(SPI.transfer(0x42) == 0xa5);
  CHECK(SPI.transfer16(0x1234) == 0xa5a5);
  SPI.writeBytes(packet, 2);
  CHECK(a.got == std::vector<uint8_t>({0x42, 0x12, 0x34, 2, 3}));
  digitalWrite(12, HIGH);
  digitalWrite(9, LOW);
  uint8_t output[2];
  SPI.transferBytes(packet, output, 2);
  CHECK(output[0] == 0x5a && output[1] == 0x5a);
  CHECK(b.got == std::vector<uint8_t>({2, 3}));
  digitalWrite(9, HIGH);
  CHECK(SPI.transfer(0) == 0xff);
  SPI.beginTransaction(SPISettings(1000000, LSBFIRST, SPI_MODE0));
  digitalWrite(9, LOW);
  b.got.clear();
  CHECK(SPI.transfer16(0x1234) == 0x5a5a);
  CHECK(b.got == std::vector<uint8_t>({0x34, 0x12}));
  digitalWrite(9, HIGH);
  SPI.endTransaction();
  emu::spiAttach(12, nullptr);
  emu::spiAttach(9, nullptr);
  pass("Wire ACK/NACK and reads; SPI active-low CS routing and transfers");
}
void heapTests() {
  CHECK(ESP.getFreeHeap() == 170 * 1024);
  CHECK(ESP.getPsramSize() == 8 * 1024 * 1024);
  emu::setHeapBudgets(64, 128);
  CHECK(ESP.getFreePsram() == 128);
  auto p = static_cast<uint8_t *>(
      heap_caps_malloc(128, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  CHECK(p);
  CHECK(!esp_ptr_internal(p));
  CHECK(!heap_caps_malloc(1, MALLOC_CAP_SPIRAM));
  CHECK(ESP.getFreePsram() == 0);
  CHECK(ESP.getFreeHeap() == 64);
  heap_caps_free(p);
  CHECK(ESP.getFreePsram() == 128);
  p = static_cast<uint8_t *>(
      heap_caps_calloc(16, 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
  CHECK(p && esp_ptr_internal(p) && esp_ptr_internal(p + 31));
  CHECK(!esp_ptr_internal(p + 32));
  for (int i = 0; i < 32; ++i)
    CHECK(p[i] == 0);
  p[0] = 91;
  CHECK(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) == 32);
  CHECK(!heap_caps_realloc(p, 65, MALLOC_CAP_INTERNAL));
  CHECK(p[0] == 91);
  p = static_cast<uint8_t *>(heap_caps_realloc(p, 64, MALLOC_CAP_INTERNAL));
  CHECK(p && p[0] == 91);
  CHECK(ESP.getFreeHeap() == 0);
  p = static_cast<uint8_t *>(heap_caps_realloc(p, 80, MALLOC_CAP_SPIRAM));
  CHECK(p && p[0] == 91 && !esp_ptr_internal(p));
  CHECK(ESP.getFreeHeap() == 64 && ESP.getFreePsram() == 48);
  CHECK(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) == 48);
  CHECK(!heap_caps_realloc(p, 129, MALLOC_CAP_SPIRAM));
  CHECK(heap_caps_realloc(p, 0, MALLOC_CAP_SPIRAM) == nullptr);
  CHECK(ESP.getFreePsram() == 128);
  CHECK(!heap_caps_calloc(SIZE_MAX, 2, MALLOC_CAP_INTERNAL));
  CHECK(!heap_caps_malloc(SIZE_MAX, MALLOC_CAP_INTERNAL));
  CHECK(!heap_caps_malloc(1, MALLOC_CAP_SPIRAM | MALLOC_CAP_INTERNAL));
  CHECK(!esp_ptr_internal(&p));
  emu::setHeapBudgets(170 * 1024, 8 * 1024 * 1024);
  pass("heap pool exhaustion, accounting, realloc, zeroing and pointer "
       "classification");
}
struct Job {
  QueueHandle_t q;
  SemaphoreHandle_t mutex, done;
  std::atomic<bool> unwound{false}, afterDelete{false};
  int value = 0;
};
void task(void *arg) {
  auto &job = *static_cast<Job *>(arg);
  struct Guard {
    Job &j;
    ~Guard() { j.unwound = true; }
  } guard{job};
  int n = 0;
  CHECK(xQueueReceive(job.q, &n, portMAX_DELAY) == pdTRUE);
  CHECK(xSemaphoreTake(job.mutex, portMAX_DELAY));
  job.value = n + 1;
  CHECK(xSemaphoreGive(job.mutex));
  CHECK(xSemaphoreGive(job.done));
  vTaskDelete(nullptr);
  job.afterDelete = true;
}
void rtosTests() {
  auto q = xQueueCreate(1, sizeof(int));
  CHECK(q);
  int n = 7, out = 0;
  CHECK(xQueueSend(q, &n, 0));
  n = 9;
  CHECK(!xQueueSendToBack(q, &n, 3));
  CHECK(uxQueueMessagesWaiting(q) == 1);
  CHECK(xQueueReceive(q, &out, 0) && out == 7);
  CHECK(!xQueueReceive(q, &out, 3));
  auto mutex = xSemaphoreCreateMutex();
  auto done = xSemaphoreCreateBinary();
  CHECK(!xSemaphoreTake(done, 0));
  CHECK(xSemaphoreTake(mutex, 0));
  CHECK(!xSemaphoreTake(mutex, 2));
  CHECK(xSemaphoreGive(mutex));
  CHECK(!xSemaphoreGive(mutex));
  Job job{q, mutex, done};
  TaskHandle_t handle = nullptr;
  CHECK(xTaskCreatePinnedToCore(task, "core-test", 4096, &job, 1, &handle, 0) ==
        pdPASS);
  CHECK(handle);
  n = 41;
  CHECK(xQueueSendToBack(q, &n, 100));
  CHECK(xSemaphoreTake(done, 1000));
  vTaskDelete(handle);
  CHECK(job.value == 42 && job.unwound && !job.afterDelete);
  CHECK(xTaskGetStackHighWaterMark(handle) >= 1024);
  std::atomic<bool> ran{false};
  auto cancel = [](void *p) {
    *static_cast<std::atomic<bool> *>(p) = true;
    for (;;)
      vTaskDelay(100);
  };
  CHECK(xTaskCreate(cancel, "cancel", 4096, &ran, 1, &handle));
  while (!ran)
    delay(1);
  vTaskDelete(handle);
  vSemaphoreDelete(done);
  vSemaphoreDelete(mutex);
  vQueueDelete(q);
  CHECK(!xQueueCreate(0, 4));
  pass("FreeRTOS byte-copy queues, timeouts, nonrecursive mutex, tasks and "
       "self-delete");
}
void storageTests() {
  CHECK(std::filesystem::is_directory(emu::sdRoot()));
  emu::setSdPresent(false);
  CHECK(!SD.begin());
  CHECK(!SD.open("/x", FILE_WRITE));
  emu::setSdPresent(true);
  CHECK(SD.begin(39, SPI, 4000000));
  CHECK(SD.cardType() == CARD_SDHC);
  CHECK(SD.cardSize() == 32ULL * 1024 * 1024 * 1024);
  CHECK(!SD.open("/missing/file", FILE_WRITE));
  CHECK(SD.mkdir("/dir"));
  File f = SD.open("/dir/log", FILE_WRITE);
  CHECK(f);
  CHECK(f.print("first\n") == 6);
  f.flush();
  CHECK(f.size() == 6);
  File copy = f;
  f.close();
  CHECK(!f && copy);
  CHECK(copy.print("second") == 6);
  copy.close();
  f = SD.open("/dir/log", FILE_APPEND);
  CHECK(f.write(uint8_t('!')) == 1);
  f.close();
  CHECK(SD.usedBytes() == 13);
  f = SD.open("/dir/log");
  CHECK(f);
  CHECK(std::string(f.name()) == "log");
  CHECK(std::string(f.path()) == "/dir/log");
  CHECK(f.available() == 13);
  CHECK(f.peek() == 'f');
  CHECK(f.position() == 0);
  CHECK(f.readStringUntil('\n') == "first");
  CHECK(f.position() == 6);
  CHECK(f.read() == 's');
  CHECK(f.seek(0));
  char text[14]{};
  CHECK(f.readBytes(text, 13) == 13);
  CHECK(std::string(text) == "first\nsecond!");
  CHECK(f.read() == -1);
  CHECK(f.seek(1));
  CHECK(f.seek(2, SeekCur));
  CHECK(f.position() == 3);
  CHECK(f.seek(0, SeekEnd));
  CHECK(f.position() == 13);
  CHECK(f.getLastWrite() > 0);
  f.close();
  CHECK(SD.rename("/dir/log", "/dir/renamed"));
  CHECK(!SD.exists("/dir/log") && SD.exists("/dir/renamed"));
  CHECK(SD.mkdir("/dir/sub"));
  auto dir = SD.open("/dir");
  CHECK(dir && dir.isDirectory());
  std::set<std::string> names;
  while (auto entry = dir.openNextFile()) {
    names.insert(entry.name());
    CHECK(std::string(entry.name()).find('/') == std::string::npos);
  }
  CHECK(names == std::set<std::string>({"renamed", "sub"}));
  dir.rewindDirectory();
  CHECK(dir.openNextFile());
  CHECK(!SD.rmdir("/dir"));
  CHECK(!SD.remove("/dir/sub"));
  CHECK(SD.remove("/dir/renamed"));
  CHECK(SD.rmdir("/dir/sub"));
  dir.close();
  CHECK(SD.rmdir("/dir"));
  CHECK(!SD.open("/../../escape", FILE_WRITE));
  CHECK(!SD.rmdir("/"));
  SD.end();
  CHECK(!SD.exists("/"));
  CHECK(SD.cardSize() == 0);
  CHECK(SD.begin());
  pass("SD presence, file copies, append/read/seek, basename iteration and "
       "directory operations");
}
void preferencesTests() {
  Preferences p;
  CHECK(!p.begin("namespace-too-long"));
  CHECK(p.begin("core"));
  CHECK(!p.begin("again"));
  CHECK(p.putInt("count", 42) == 4);
  CHECK(p.getInt("count") == 42);
  CHECK(p.getUInt("count", 9) == 9);
  CHECK(p.putUInt("u", UINT32_MAX) == 4);
  CHECK(p.getUInt("u") == UINT32_MAX);
  CHECK(p.putLong("long", -7) == 4 && p.getLong("long") == -7);
  CHECK(p.putULong("ulong", 123) == 4 && p.getULong("ulong") == 123);
  CHECK(p.putUChar("uc", 250) == 1 && p.getUChar("uc") == 250);
  CHECK(p.putChar("c", -120) == 1 && p.getChar("c") == -120);
  CHECK(p.putShort("s", -1234) == 2 && p.getShort("s") == -1234);
  CHECK(p.putUShort("us", 65530) == 2 && p.getUShort("us") == 65530);
  CHECK(p.putBool("b", true) == 1 && p.getBool("b"));
  CHECK(p.putFloat("f", 1.25f) == 4 && p.getFloat("f") == 1.25f);
  CHECK(p.putDouble("d", -2.5) == 8 && p.getDouble("d") == -2.5);
  CHECK(p.putLong64("ll", INT64_MIN) == 8 && p.getLong64("ll") == INT64_MIN);
  CHECK(p.putULong64("ull", UINT64_MAX) == 8 &&
        p.getULong64("ull") == UINT64_MAX);
  CHECK(p.putString("string", String("hello")) == 5);
  CHECK(p.getString("string") == "hello");
  char b[6];
  CHECK(p.getString("string", b, 5) == 0);
  CHECK(p.getString("string", b, 6) == 6 && std::string(b) == "hello");
  CHECK(p.putString("empty", "") == 0 && p.isKey("empty"));
  uint8_t data[] = {0, 1, 255}, got[3]{};
  CHECK(p.putBytes("blob", data, 3) == 3);
  CHECK(p.getBytesLength("blob") == 3);
  CHECK(p.getBytes("blob", got, 2) == 0);
  CHECK(p.getBytes("blob", got, 3) == 3 && memcmp(data, got, 3) == 0);
  CHECK(p.getBytes("blob", nullptr, 0) == 3);
  CHECK(p.freeEntries() > 0);
  CHECK(!p.putInt("key-is-too-long!x", 7));
  CHECK(!p.isKey("key-is-too-long!x"));
  CHECK(!p.putInt("", 7));
  CHECK(p.putInt("123456789012345", 3) == 4);
  CHECK(p.isKey("123456789012345"));
  Preferences second;
  CHECK(second.begin("core", true));
  CHECK(second.getInt("count") == 42);
  CHECK(!second.putInt("count", 2));
  CHECK(!second.clear() && !second.remove("count"));
  CHECK(p.putInt("count", 43) == 4);
  CHECK(second.getInt("count") == 43);
  second.end();
  p.end();
  // Each read is reloaded from disk; test a new process as well as new objects.
  child("--reload");
  CHECK(p.begin("core"));
  CHECK(p.getInt("count") == 44);
  CHECK(p.remove("count"));
  CHECK(!p.isKey("count"));
  CHECK(!p.remove("count"));
  p.end();
  child("--restart");
  Preferences other;
  CHECK(other.begin("other"));
  CHECK(!other.isKey("string"));
  CHECK(other.putInt("x", 1) == 4);
  CHECK(other.clear());
  CHECK(!other.isKey("x"));
  for (auto &entry : std::filesystem::directory_iterator(emu::dataDir()))
    CHECK(entry.path().filename().string().find(".tmp.") == std::string::npos);
  pass("Preferences typed values, atomic persistence, disk reload, restart and "
       "key limits");
}
void i2sTests() {
  i2s_config_t cfg{};
  CHECK(i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) == ESP_OK);
  uint8_t b[16];
  memset(b, 0x7f, sizeof b);
  size_t n = 0;
  CHECK(i2s_read(I2S_NUM_0, b, sizeof b, &n, 0) == ESP_OK && n == sizeof b);
  for (auto v : b)
    CHECK(!v);
  CHECK(i2s_write(I2S_NUM_0, b, sizeof b, &n, 0) == ESP_OK && n == sizeof b);
  CHECK(i2s_zero_dma_buffer(I2S_NUM_0) == ESP_OK);
  CHECK(i2s_driver_uninstall(I2S_NUM_0) == ESP_OK);
  pass("I2S discard writes and silence reads");
}
int loops = 0;
bool setupRan = false;
void setup() { setupRan = true; }
void loop() {
  CHECK(setupRan);
  if (++loops == 3)
    emu::requestQuit();
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 3 && std::string(argv[1]) == "--reload") {
    emu::setDataDir(argv[2]);
    Preferences p;
    CHECK(p.begin("core"));
    CHECK(p.getInt("count") == 43);
    CHECK(p.getString("string") == "hello");
    CHECK(p.putInt("count", 44) == 4);
    return 0;
  }
  if (argc == 3 && std::string(argv[1]) == "--restart") {
    if (!getenv("CORE_RESTARTED")) {
      emu::setDataDir(std::string(argv[2]) + "/restart-dir");
      Preferences p;
      CHECK(p.begin("restart"));
      CHECK(p.putInt("value", 77) == 4);
      setenv("CORE_RESTARTED", "1", 1);
      ESP.restart();
      CHECK(false);
    }
    CHECK(emu::dataDir() == std::string(argv[2]) + "/restart-dir");
    Preferences p;
    CHECK(p.begin("restart", true));
    CHECK(p.getInt("value") == 77);
    return 0;
  }
  char pattern[] = "/tmp/mattermesh-core-XXXXXX";
  auto path = mkdtemp(pattern);
  CHECK(path);
  testDir = path;
  emu::setDataDir(testDir);
  stringTests();
  serialTests();
  timeTests();
  gpioTests();
  busTests();
  heapTests();
  rtosTests();
  storageTests();
  preferencesTests();
  i2sTests();
  auto before = pumps.load();
  std::atomic<bool> backgroundStarted{false}, backgroundStopped{false};
  struct Background {
    std::atomic<bool> &started, &stopped;
  } background{backgroundStarted, backgroundStopped};
  auto backgroundJob = [](void *arg) {
    auto &job = *static_cast<Background *>(arg);
    struct Cleanup {
      Background &job;
      ~Cleanup() { job.stopped = true; }
    } cleanup{job};
    job.started = true;
    for (;;)
      vTaskDelay(100);
  };
  CHECK(xTaskCreate(backgroundJob, "shutdown", 2048, &background, 1, nullptr));
  while (!backgroundStarted)
    std::this_thread::yield();
  emuRunLoop(setup, loop);
  CHECK(backgroundStopped);
  CHECK(loops == 3 && pumps == before + 3);
  pass("setup/loop driver, quit handling and worker shutdown");
  std::filesystem::remove_all(testDir);
  pass("all emulator core tests (ASan + UBSan)");
  return 0;
}
