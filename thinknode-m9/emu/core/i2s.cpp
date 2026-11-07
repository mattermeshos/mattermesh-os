#include "driver/i2s.h"
#include <cstring>
esp_err_t i2s_driver_install(i2s_port_t, const i2s_config_t *, int,
                             QueueHandle_t *q) {
  if (q)
    *q = nullptr;
  return ESP_OK;
}
esp_err_t i2s_driver_uninstall(i2s_port_t) { return ESP_OK; }
esp_err_t i2s_set_pin(i2s_port_t, const i2s_pin_config_t *) { return ESP_OK; }
esp_err_t i2s_zero_dma_buffer(i2s_port_t) { return ESP_OK; }
esp_err_t i2s_write(i2s_port_t, const void *, size_t n, size_t *done,
                    TickType_t) {
  if (done)
    *done = n;
  return ESP_OK;
}
esp_err_t i2s_read(i2s_port_t, void *p, size_t n, size_t *done, TickType_t) {
  if (!p && n)
    return ESP_ERR_INVALID_ARG;
  if (n)
    std::memset(p, 0, n);
  if (done)
    *done = n;
  return ESP_OK;
}
