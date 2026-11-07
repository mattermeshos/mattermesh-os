#pragma once
// Elecrow ThinkNode M9 (ESP32-S3-R8, 16 MB flash, ST7789 320x240 LCD without touch, LR1110, keyboard MCU on I2C,
// GPS, PCF8563 RTC on GPIO6/7, buzzer, no microphone and no speaker). Pins from the MeshCore thinknode_m9 variant, the
// Meshtastic ELECROW-ThinkNode-M9 variant and the Elecrow wiki. Emulator prototype: nothing here has run on a real M9.
// Names follow m9_board.h so the shared code compiles unchanged; parts the M9 lacks use unused addresses or -1.
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#define BOARD_NAME "ThinkNode-M9"
#define SerialMon Serial
#define SerialGPS Serial2
#define BOARD_POWERON 18 // VDD_PERIPH_EN: LOW turns the peripherals on
#define BOARD_POWERON_LEVEL LOW

#define BOARD_I2C_SDA 20 // bus to the keyboard MCU
#define BOARD_I2C_SCL 21
#define BOARD_I2C_ADDR_KEYBOARD 0x6C // 0x6D on the V1.1/V2 board (m9_keyboard.h probes both)
#define BOARD_KEYBOARD_INT 12 // rests low, rises on a key press
#define BOARD_BAT_ADC 13      // through a 1:2 divider; ADC2: unreadable while Wi-Fi is on

#define BOARD_SPI_SCK  40
#define BOARD_SPI_MOSI 47
#define BOARD_SPI_MISO 38
#define BOARD_TFT_CS 16
#define BOARD_TFT_DC 15
#define BOARD_TFT_RST 14
#define BOARD_TFT_BL 17
#define BOARD_EPD_CS   BOARD_TFT_CS
#define BOARD_EPD_DC   BOARD_TFT_DC
#define BOARD_EPD_RST  (-1)
#define BOARD_EPD_BUSY (-1)
#define BOARD_EPD_BL   BOARD_TFT_BL
#define BOARD_SD_CS 48
#define BOARD_LORA_CS   39
#define BOARD_LORA_BUSY 41
#define BOARD_LORA_RST  45
#define BOARD_LORA_INT  42
#define BOARD_GPS_RXD 2
#define BOARD_GPS_TXD 3
#define BOARD_GPS_EN 11        // the "on" level differs between board revisions: location.cpp finds it (NVS mmlocation/gps_en_high)
#define BOARD_GPS_RESET 5      // HIGH resets
#define BOARD_GPS_WAKE 10      // the module's idle pin: rests HIGH
#define BOARD_GPS_BAUD 115200
#define BOARD_USB_POWER 1      // LOW while USB power is present
#define BOARD_BUZZER 9

namespace hw {
// PWM, active low (TFT_BACKLIGHT_ON LOW in the Meshtastic variant): to confirm on a real M9.
inline void backlight(uint8_t value){pinMode(BOARD_TFT_BL,OUTPUT);analogWrite(BOARD_TFT_BL,255-value);}
// Host GPIO, on or off: no brightness levels (Meshtastic STC8HKeyboard::set_keyboard_blight).
inline uint8_t &keyboardAddress(){static uint8_t a=0;return a;}// set once the keyboard controller answered
// Keyboard light: a PWM duty in the keyboard controller, register 0x02 (GPIO46 is not wired on the M9). Written on change
// and again every 2 s: the controller switches the light off on its own about 10 s after the last key (real M9, 2026-11-06),
// so a "keyboard light off after 30 s / 1 min" was cut to 10 s.
inline void keyboardLight(uint8_t duty){static int last=-1;static uint32_t at=0;if(!keyboardAddress()||(duty==last&&uint32_t(millis()-at)<2000))return;at=millis();Wire.beginTransmission(keyboardAddress());Wire.write(0x02);Wire.write(duty);if(Wire.endTransmission()==0)last=duty;}
// Board revision, told by the keyboard address (m9_keyboard.h): V2 inverted the GPS enable line (Meshtastic variant).
inline bool &boardV2(){static bool v2=false;return v2;}
inline void gpsPower(bool on,bool activeHigh=false){pinMode(BOARD_GPS_EN,OUTPUT);digitalWrite(BOARD_GPS_EN,on==activeHigh?HIGH:LOW);}// EN active low on the boards seen so far; some revisions want it high
inline void gpsPinsBegin(){pinMode(BOARD_GPS_RESET,OUTPUT);digitalWrite(BOARD_GPS_RESET,LOW);pinMode(BOARD_GPS_WAKE,OUTPUT);digitalWrite(BOARD_GPS_WAKE,HIGH);gpsPower(false);}
inline bool usbPowered(){pinMode(BOARD_USB_POWER,INPUT_PULLUP);return digitalRead(BOARD_USB_POWER)==LOW;}
// GPIO13 is on ADC2, which belongs to the Wi-Fi driver while Wi-Fi is on.
inline bool batteryReadable(){return WiFi.getMode()==WIFI_OFF;}
inline int batteryMillivolts(){uint32_t sum=0;for(int i=0;i<16;i++){sum+=analogReadMilliVolts(BOARD_BAT_ADC);delayMicroseconds(500);}return int((sum*2+8)/16);}
}
