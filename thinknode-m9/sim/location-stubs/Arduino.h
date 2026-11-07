#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <algorithm>
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define SERIAL_8N1 0
using std::max;using std::min;
inline uint32_t fakeMillis=100;
inline uint32_t millis(){return fakeMillis;}
inline void delay(uint32_t ms){fakeMillis+=ms;}// time passes while the firmware waits (GPS sleep request at both bauds)
inline void delayMicroseconds(uint32_t){}
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
inline uint32_t analogReadMilliVolts(int){return 0;}
struct FakeSerial {template<class...A> int printf(const char*,A...){return 0;}};
inline FakeSerial Serial;
// The Plus GPS UART (plus_board.h: SerialGPS is Serial2).
struct FakeUART {
 std::string injected,written;size_t cursor=0;
 bool active=false;unsigned baud=0,reads=0,begins=0,ends=0,bytes=0;
 void setRxBufferSize(int){}
 void setTxBufferSize(int){}
 size_t write(const uint8_t *d,size_t n){written.append((const char*)d,n);return n;}
 void flush(){}
 void begin(unsigned rate,int,int,int){active=true;baud=rate;begins++;}
 void end(){active=false;baud=0;ends++;bytes=0;}
 int available(){return cursor<injected.size()?int(injected.size()-cursor):bytes;}
 int read(){reads++;if(cursor<injected.size())return uint8_t(injected[cursor++]);if(bytes)bytes--;return 0;}
 void inject(const std::string &s){injected=s;cursor=0;}
};
inline FakeUART Serial2;
// M9 board helpers (board/m9_board.h) used by the location power test
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 5
#endif
inline int digitalRead(int){return 1;}
inline void analogWrite(int,int){}
