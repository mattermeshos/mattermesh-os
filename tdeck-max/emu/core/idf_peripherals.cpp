// ESP-IDF bus/GPIO entry points used by the unmodified HynTouch and codec libraries.
#include <Arduino.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp32-hal-i2c.h>
#include <emu_hal.h>
#include <array>
namespace {
std::array<void (*)(void *),64> handlers{};
std::array<void *,64> arguments{};
std::array<int,64> modes{};
}
int gpio_get_level(gpio_num_t p){return digitalRead(p);}
esp_err_t gpio_config(const gpio_config_t *c){
 if(!c)return ESP_ERR_INVALID_ARG;
 for(int p=0;p<64;p++)if(c->pin_bit_mask&(1ULL<<p)){
  pinMode(p,c->mode==GPIO_MODE_OUTPUT?OUTPUT:c->pull_up_en?INPUT_PULLUP:INPUT);
  modes[p]=c->intr_type;
 }
 return ESP_OK;
}
esp_err_t gpio_install_isr_service(int){return ESP_OK;}
esp_err_t gpio_isr_handler_add(gpio_num_t p,void (*fn)(void *),void *arg){
 if(p<0||p>=64)return ESP_ERR_INVALID_ARG;
 handlers[p]=fn;arguments[p]=arg;
 attachInterruptArg(p,fn,arg,modes[p]==GPIO_INTR_NEGEDGE?FALLING:CHANGE);
 return ESP_OK;
}
esp_err_t gpio_isr_handler_remove(gpio_num_t p){if(p<0||p>=64)return ESP_ERR_INVALID_ARG;detachInterrupt(p);handlers[p]=nullptr;return ESP_OK;}
bool i2cIsInit(int){return true;}
esp_err_t i2cInit(int,int sda,int scl,uint32_t hz){Wire.begin(sda,scl,hz);return ESP_OK;}
esp_err_t i2c_master_write_to_device(int,uint8_t address,const uint8_t *d,size_t n,TickType_t){Wire.beginTransmission(address);Wire.write(d,n);return Wire.endTransmission()==0?ESP_OK:ESP_FAIL;}
esp_err_t i2c_master_read_from_device(int,uint8_t address,uint8_t *d,size_t n,TickType_t){size_t got=Wire.requestFrom(address,n);for(size_t i=0;i<got;i++)d[i]=Wire.read();return got==n?ESP_OK:ESP_FAIL;}
