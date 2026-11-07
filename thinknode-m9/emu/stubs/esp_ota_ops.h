#pragma once
// Emulator: the running image is "app0", already marked valid; there is no second slot.
#include "esp_partition.h"
#include <esp_system.h>
typedef enum {ESP_OTA_IMG_NEW=0,ESP_OTA_IMG_PENDING_VERIFY=1,ESP_OTA_IMG_VALID=2,ESP_OTA_IMG_INVALID=3,ESP_OTA_IMG_ABORTED=4,ESP_OTA_IMG_UNDEFINED=-1} esp_ota_img_states_t;
inline const esp_partition_t *esp_ota_get_running_partition(){static const esp_partition_t app0={0,0x10,0x10000,0x640000,"app0",false};return &app0;}
inline const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t*){return nullptr;}
inline int esp_ota_get_state_partition(const esp_partition_t*,esp_ota_img_states_t *state){if(state)*state=ESP_OTA_IMG_VALID;return 0;}
inline int esp_ota_mark_app_valid_cancel_rollback(){return 0;}
