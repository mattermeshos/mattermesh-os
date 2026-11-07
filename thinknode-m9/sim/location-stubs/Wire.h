#pragma once
// Host tests: the Plus I/O expander is emulated and never touches the bus. Like the Arduino core, Wire.h brings Arduino.h (HIGH/LOW).
#include <Arduino.h>
struct TwoWire {void beginTransmission(int){}size_t write(uint8_t){return 1;}int endTransmission(){return 0;}};
inline TwoWire Wire;
