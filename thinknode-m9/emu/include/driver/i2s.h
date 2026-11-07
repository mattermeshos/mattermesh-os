#pragma once
#include "esp_system.h"
#include "freertos/queue.h"
#define ESP_INTR_FLAG_LEVEL1 1
#define I2S_PIN_NO_CHANGE (-1)
enum i2s_port_t { I2S_NUM_0, I2S_NUM_1, I2S_NUM_MAX };
enum i2s_mode_t {
  I2S_MODE_MASTER = 1,
  I2S_MODE_SLAVE = 2,
  I2S_MODE_TX = 4,
  I2S_MODE_RX = 8
};
enum i2s_bits_per_sample_t {
  I2S_BITS_PER_SAMPLE_8BIT = 8,
  I2S_BITS_PER_SAMPLE_16BIT = 16,
  I2S_BITS_PER_SAMPLE_24BIT = 24,
  I2S_BITS_PER_SAMPLE_32BIT = 32
};
enum i2s_channel_fmt_t {
  I2S_CHANNEL_FMT_RIGHT_LEFT,
  I2S_CHANNEL_FMT_ALL_RIGHT,
  I2S_CHANNEL_FMT_ALL_LEFT,
  I2S_CHANNEL_FMT_ONLY_RIGHT,
  I2S_CHANNEL_FMT_ONLY_LEFT
};
enum i2s_comm_format_t {
  I2S_COMM_FORMAT_STAND_I2S = 1,
  I2S_COMM_FORMAT_I2S = 1,
  I2S_COMM_FORMAT_STAND_MSB = 2
};
enum i2s_mclk_multiple_t {
  I2S_MCLK_MULTIPLE_DEFAULT = 0,
  I2S_MCLK_MULTIPLE_256 = 256
};
enum i2s_bits_per_chan_t {
  I2S_BITS_PER_CHAN_DEFAULT = 0,
  I2S_BITS_PER_CHAN_16BIT = 16
};
enum i2s_channel_t { I2S_TDM_ACTIVE_CH0 = 1, I2S_TDM_ACTIVE_CH1 = 2 };
struct i2s_config_t {
  i2s_mode_t mode{};
  uint32_t sample_rate{};
  i2s_bits_per_sample_t bits_per_sample{};
  i2s_channel_fmt_t channel_format{};
  i2s_comm_format_t communication_format{};
  int intr_alloc_flags{}, dma_buf_count{}, dma_buf_len{};
  bool use_apll{}, tx_desc_auto_clear{};
  int fixed_mclk{};
  i2s_mclk_multiple_t mclk_multiple{};
  i2s_bits_per_chan_t bits_per_chan{};
  i2s_channel_t chan_mask{};
};
struct i2s_pin_config_t {
  int mck_io_num, bck_io_num, ws_io_num, data_out_num, data_in_num;
};
esp_err_t i2s_driver_install(i2s_port_t, const i2s_config_t *, int,
                             QueueHandle_t *);
esp_err_t i2s_driver_uninstall(i2s_port_t);
esp_err_t i2s_set_pin(i2s_port_t, const i2s_pin_config_t *);
esp_err_t i2s_write(i2s_port_t, const void *, size_t, size_t *, TickType_t);
esp_err_t i2s_read(i2s_port_t, void *, size_t, size_t *, TickType_t);
esp_err_t i2s_zero_dma_buffer(i2s_port_t);
