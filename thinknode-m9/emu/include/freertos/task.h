#pragma once
#include "FreeRTOS.h"
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t, const char *, uint32_t,
                                   void *, UBaseType_t, TaskHandle_t *,
                                   BaseType_t);
BaseType_t xTaskCreate(TaskFunction_t, const char *, uint32_t, void *,
                       UBaseType_t, TaskHandle_t *);
void vTaskDelete(TaskHandle_t = nullptr);
void vTaskDelay(TickType_t);
UBaseType_t xTaskGetStackHighWaterMark(TaskHandle_t = nullptr);
inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t t = nullptr) { return xTaskGetStackHighWaterMark(t); }
#define tskIDLE_PRIORITY 0
TickType_t xTaskGetTickCount();
TaskHandle_t xTaskGetCurrentTaskHandle();

namespace emu {
struct TaskExit {}; /* private cooperative task unwind */
}                   // namespace emu
