#pragma once
// LilyGO T-Deck Pro (ESP32-S3FN16R8, GDEQ031T10/UC8253 240x320 e-paper, CST328 touch, TCA8418 keyboard matrix,
// SX1262 LoRa, MIA-M10Q GPS, BQ25896 charger, BQ27220 fuel gauge). Pins from LilyGO's T-Deck-Pro
// examples/factory/utilities.h (hardware v1.0, firmware v1.6-260401).
//
// Unlike the T-Deck Max (which gates most of these lines through an XL9555 IO expander), the Pro drives its
// peripheral enables, the touch reset and the vibration motor as plain ESP32 GPIOs. The "EN" pins below are
// assumed active-HIGH by their datasheet-style naming; this has not been verified against real hardware -
// if LoRa or GPS never come up, try inverting the polarity first.
#include <Arduino.h>
#include <Wire.h>
#define BOARD_NAME "T-Deck-Pro"
#define SerialMon Serial
#define SerialGPS Serial2

// I2C addresses
#define BOARD_I2C_ADDR_TOUCH       0x1A // CST328 touch controller
#define BOARD_I2C_ADDR_LTR_553ALS  0x23 // light sensor (not used by this firmware)
#define BOARD_I2C_ADDR_GYROSCOPE   0x28 // BHI260AP gyroscope (not used by this firmware)
#define BOARD_I2C_ADDR_KEYBOARD    0x34 // TCA8418 keyboard matrix controller
#define BOARD_I2C_ADDR_BQ27220     0x55 // BQ27220 fuel gauge (same chip/registers as T-Deck Max)
#define BOARD_I2C_ADDR_BQ25896     0x6B // BQ25896 charger

#define BOARD_I2C_SDA 13
#define BOARD_I2C_SCL 14

// Keyboard (TCA8418, 4x10 matrix - same chip and matrix size as T-Deck Max; no reset pin on this board)
#define BOARD_KEYBOARD_SCL BOARD_I2C_SCL
#define BOARD_KEYBOARD_SDA BOARD_I2C_SDA
#define BOARD_KEYBOARD_INT 15
#define BOARD_KEYBOARD_LED 42

// Touch (CST328, direct reset GPIO instead of Max's expander-gated reset)
#define BOARD_TOUCH_SCL BOARD_I2C_SCL
#define BOARD_TOUCH_SDA BOARD_I2C_SDA
#define BOARD_TOUCH_INT 12
#define BOARD_TOUCH_RST 45

// Shared SPI bus
#define BOARD_SPI_SCK  36
#define BOARD_SPI_MOSI 33
#define BOARD_SPI_MISO 47

// Display (GDEQ031T10 / UC8253, same panel class as T-Deck Max: GxEPD2_310_GDEQ031T10 / TypingPanel)
#define LCD_HOR_SIZE 240
#define LCD_VER_SIZE 320
#define BOARD_EPD_SCK  BOARD_SPI_SCK
#define BOARD_EPD_MOSI BOARD_SPI_MOSI
#define BOARD_EPD_DC   35
#define BOARD_EPD_CS   34
#define BOARD_EPD_BUSY 37
#define BOARD_EPD_RST  (-1)
#define BOARD_EPD_BL   (-1) // this panel has no front light; power.cpp skips the PWM write when the pin is negative

// SD card
#define BOARD_SD_CS   48
#define BOARD_SD_SCK  BOARD_SPI_SCK
#define BOARD_SD_MOSI BOARD_SPI_MOSI
#define BOARD_SD_MISO BOARD_SPI_MISO

// LoRa (SX1262, same radio as T-Deck Max; single fixed antenna, no antenna switch pin here)
#define BOARD_LORA_SCK  BOARD_SPI_SCK
#define BOARD_LORA_MOSI BOARD_SPI_MOSI
#define BOARD_LORA_MISO BOARD_SPI_MISO
#define BOARD_LORA_CS   3
#define BOARD_LORA_BUSY 6
#define BOARD_LORA_RST  4
#define BOARD_LORA_INT  5
#define BOARD_LORA_EN   46 // direct enable GPIO; Max gates the same net through its IO expander instead

// GPS (MIA-M10Q, same receiver as T-Deck Max; different UART pins and its own enable line)
#define BOARD_GPS_RXD 44
#define BOARD_GPS_TXD 43
#define BOARD_GPS_PPS 1
#define BOARD_GPS_EN  39 // direct enable GPIO; Max gates the same net through its IO expander instead

// Audio (PCM5102A I2S DAC - the audio-SKU variant of this board; the alternate 4G SKU reuses these same three
// GPIOs for its A7682E modem instead, so this only applies if the audio DAC is actually fitted). Simple I2S DAC,
// no I2C control needed (unlike the Max's ES7210/ES8311 codec) - always listening once the I2S bus is clocked.
#define BOARD_I2S_BCLK 7
#define BOARD_I2S_DOUT 8
#define BOARD_I2S_LRC  9

// Misc
#define BOARD_BOOT_PIN  0
#define BOARD_MOTOR_PIN 2 // plain PWM-driven vibration motor (Max drives a DRV2605 haptic IC over I2C instead)

// Drop-in replacement for Adafruit_DRV2605 (T-Deck Max's haptic driver): same method names the shared UI code
// calls (begin/selectLibrary/setMode/setWaveform/stop/setRealtimeValue/readRegister8), driving a plain PWM
// pin instead of an I2C haptic IC. The mode/waveform calls are accepted and ignored.
#define DRV2605_MODE_INTTRIG  0
#define DRV2605_MODE_REALTIME 1
// Referenced only by CLI diagnostic printouts (readRegister8 always returns 0); kept so those lines need no edits.
#define DRV2605_REG_RTPIN     0
#define DRV2605_REG_WAVESEQ1  1
#define DRV2605_REG_GO        2
namespace pro {
class SimpleHaptic {
public:
 bool begin(TwoWire* =nullptr){pinMode(BOARD_MOTOR_PIN,OUTPUT);analogWrite(BOARD_MOTOR_PIN,0);return true;}
 void selectLibrary(uint8_t){}
 void setMode(uint8_t){}
 void setWaveform(uint8_t,uint8_t){}
 void stop(){analogWrite(BOARD_MOTOR_PIN,0);}
 // DRV2605 real-time-playback values are 0..127; scale onto the plain motor PWM duty (0..255), with a floor so
 // weak values still turn the motor (most ERM motors need a minimum duty to overcome static friction).
 void setRealtimeValue(uint8_t v){analogWrite(BOARD_MOTOR_PIN,v?uint8_t(40+v*215/127):0);}
 uint8_t readRegister8(uint8_t){return 0;}
};
}
