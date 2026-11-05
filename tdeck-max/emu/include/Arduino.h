#pragma once
#ifndef ARDUINO
#define ARDUINO 10819
#endif
inline bool isDigit(int c){return c>=48&&c<=57;}
#include "HardwareSerial.h"
#include "WString.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/time.h>
using std::abs;
using std::max;
using std::min;
using byte = uint8_t;
using boolean = bool;
using word = uint16_t;
enum BitOrder { LSBFIRST = 0, MSBFIRST = 1 };
#define IRAM_ATTR
#define DRAM_ATTR
#define INPUT 0x01
#define OUTPUT 0x03
#define INPUT_PULLUP 0x05
#define INPUT_PULLDOWN 0x09
#define OUTPUT_OPEN_DRAIN 0x13
#define PULLUP 0x04
#define PULLDOWN 0x08
#define HIGH 1
#define LOW 0
#define RISING 0x01
#define FALLING 0x02
#define CHANGE 0x03
#define ONLOW 0x04
#define ONHIGH 0x05
#define PI 3.1415926535897932384626433832795
#define TWO_PI (2 * PI)
#define DEG_TO_RAD (PI / 180.0)
#define RAD_TO_DEG (180.0 / PI)
#define radians(deg) ((deg) * DEG_TO_RAD)
#define degrees(rad) ((rad) * RAD_TO_DEG)
#define sq(x) ((x) * (x))
#define PGM_P const char *
// ESP32-S3 DevKit default I2C pins (pins_arduino.h); the firmware passes its own pins.
static const uint8_t SDA = 8;
static const uint8_t SCL = 9;
// Arduino-ESP32 loop task stack size: the emulator runs loop() on the main thread.
#define SET_LOOP_TASK_STACK_SIZE(sz) size_t getArduinoLoopTaskStackSize(void) { return sz; }
#define bitRead(v, b) (((v) >> (b)) & 1U)
#define bitSet(v, b) ((v) |= (1UL << (b)))
#define bitClear(v, b) ((v) &= ~(1UL << (b)))
#define bitWrite(v, b, x) ((x) ? bitSet(v, b) : bitClear(v, b))
#define pgm_read_byte(p) (*reinterpret_cast<const uint8_t *>(p))
#define pgm_read_word(p) (*reinterpret_cast<const uint16_t *>(p))
#define pgm_read_dword(p) (*reinterpret_cast<const uint32_t *>(p))
#define pgm_read_float(p) (*reinterpret_cast<const float *>(p))
#define pgm_read_ptr(p) (*reinterpret_cast<const void *const *>(p))
#define memcpy_P memcpy
#define strlen_P strlen
#define strcpy_P strcpy
#define snprintf_P snprintf
#define sprintf_P sprintf
inline long map(long x, long a, long b, long c, long d) {
  return (x - a) * (d - c) / (b - a) + c;
}
template <class A, class B, class C> constexpr auto constrain(A v, B lo, C hi) {
  using T = std::common_type_t<A, B, C>;
  return T(v) < T(lo) ? T(lo) : T(v) > T(hi) ? T(hi) : T(v);
}
unsigned long millis();
unsigned long micros();
void delay(unsigned long);
void delayMicroseconds(unsigned int);
inline void disableCore0WDT(){}
inline void enableCore0WDT(){}// no task watchdog on the PC
void yield();
void pinMode(uint8_t, uint8_t);
void digitalWrite(uint8_t, uint8_t);
int digitalRead(uint8_t);
void attachInterrupt(uint8_t, void (*)(), int);
void detachInterrupt(uint8_t);
void attachInterruptArg(uint8_t, void (*)(void *), void *, int);
inline int digitalPinToInterrupt(int p) { return p; }
inline void noInterrupts() {}
inline void interrupts() {}
uint32_t analogReadMilliVolts(uint8_t);
uint16_t analogRead(uint8_t);
void analogWrite(uint8_t, int);
long random(long);
long random(long, long);
void randomSeed(unsigned long);
bool setCpuFrequencyMhz(uint32_t);
uint32_t getCpuFrequencyMhz();
void configTime(long, int, const char *, const char * = nullptr,
                const char * = nullptr);
bool getLocalTime(struct tm *, uint32_t = 5000);
// configTzTime: the POSIX TZ string becomes the process time zone (as on the ESP32); SNTP is not started.
void configTzTime(const char *tz, const char *server1, const char *server2 = nullptr, const char *server3 = nullptr);
inline void tone(uint8_t, unsigned int, unsigned long = 0) {}
inline void noTone(uint8_t) {}
inline unsigned long pulseIn(uint8_t, uint8_t, unsigned long = 1000000UL) { return 0; }
char *ltoa(long value, char *out, int base);
char *ultoa(unsigned long value, char *out, int base);
char *itoa(int value, char *out, int base);
char *utoa(unsigned value, char *out, int base);
char *dtostrf(double value, signed char width, unsigned char precision, char *out);
void emuRunLoop(void (*)(), void (*)());
class EspClass {
public:
  uint32_t getFreeHeap();
  uint32_t getFreePsram();
  uint32_t getPsramSize();
  uint32_t getFlashChipSize() { return 16 * 1024 * 1024; }
  uint64_t getEfuseMac() { return 0x102030405060ULL; }
  void restart();
};
extern EspClass ESP;

#include "esp_heap_caps.h"
inline void *ps_malloc(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); }
#include "esp_log.h"
#include "esp_idf_version.h"
#define log_e(fmt, ...) ESP_LOGE("Arduino", fmt, ##__VA_ARGS__)
#define log_w(fmt, ...) ESP_LOGW("Arduino", fmt, ##__VA_ARGS__)
#define log_i(fmt, ...) ESP_LOGI("Arduino", fmt, ##__VA_ARGS__)
#define log_d(fmt, ...) ESP_LOGD("Arduino", fmt, ##__VA_ARGS__)
