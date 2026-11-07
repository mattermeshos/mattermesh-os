#pragma once
// MatterMesh OS PC emulator: the contract between the emulated Arduino / ESP-IDF core (emu/core, Codex) and the
// emulated board chips + window (emu/devices, Claude). The firmware sources in src/ are compiled unchanged; the
// headers in emu/include shadow Arduino, ESP-IDF and the hardware libraries.
//
// Threading: setup() and loop() run on the main thread. Device models and the window also live on the main thread
// and are driven by emu::pump(), which the core calls from delay(), yield(), vTaskDelay() on the main thread and
// once per loop() pass. Interrupt handlers attached by the firmware run synchronously inside gpioDrive() (so on
// the main thread too); noInterrupts()/interrupts() may be no-ops. FreeRTOS tasks are std::threads.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
namespace emu {
// ---- time (steady clock since start) ----
uint32_t millis32();
uint64_t micros64();

// ---- GPIO ----
// Level the firmware wrote with digitalWrite (default: what the device last drove, else HIGH for INPUT_PULLUP, LOW).
int gpioLevel(int pin);
// A device drives an input pin. On an edge, interrupt handlers attached with attachInterrupt fire according to
// their mode (RISING, FALLING, CHANGE, ONLOW, ONHIGH as level-at-call), synchronously.
void gpioDrive(int pin,int level);
// Called after each firmware digitalWrite(pin,level) (e.g. SPI chip select / data-command lines).
void onGpioWrite(int pin,std::function<void(int level)> hook);
// analogReadMilliVolts / analogRead source (mV) for a pin; analogWrite(pin,value) is reported to onAnalogWrite.
void setAnalogMilliVolts(int pin,int millivolts);
void onAnalogWrite(int pin,std::function<void(int value)> hook);

// ---- I2C (Wire): one device model per 7-bit address ----
struct I2cDevice {
 virtual ~I2cDevice(){}
 // One complete write transaction (beginTransmission .. endTransmission). Return false to NACK (address absent).
 virtual bool write(const uint8_t *data,size_t length)=0;
 // requestFrom(address,length): fill up to length bytes, return how many were produced.
 virtual size_t read(uint8_t *data,size_t length)=0;
};
void i2cAttach(uint8_t address,I2cDevice *device);

// ---- SPI: devices selected by their chip-select pin (active LOW, via digitalWrite) ----
struct SpiDevice {
 virtual ~SpiDevice(){}
 virtual void select(bool selected){(void)selected;}
 virtual uint8_t transfer(uint8_t out)=0;                                  // full duplex byte
 virtual void write(const uint8_t *data,size_t length){for(size_t i=0;i<length;i++)transfer(data[i]);}
};
void spiAttach(int csPin,SpiDevice *device);

// ---- main-thread pump ----
// Registered hooks run, in order, from pump(). pump() is cheap and rate-limits nothing itself.
void addPumpHook(std::function<void()> hook);
void pump();
// True once the user closed the window (or a test asked to stop): the core's loop driver exits cleanly.
bool quitRequested();
void requestQuit();

// ---- storage locations (all under the emulator data directory, default emu/data-plus) ----
const std::string &dataDir();     // set by setDataDir before setup()
void setDataDir(const std::string &path);
std::string sdRoot();             // dataDir()+"/sd": the SD card contents (fs::FS SD)
std::string nvsPath();            // dataDir()+"/nvs.json": Preferences namespaces

// ---- SD card presence (the firmware's SD.begin fails when false) ----
void setSdPresent(bool present);
bool sdPresent();

// ---- memory budgets for heap_caps_* / ESP.getFreeHeap / getFreePsram (defaults: the Plus) ----
void setHeapBudgets(size_t internalBytes,size_t psramBytes);
}
