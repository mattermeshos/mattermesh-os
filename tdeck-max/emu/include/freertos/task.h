#pragma once
#include "FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#define EMU_DEFAULT_NULL = nullptr
#else
#define EMU_DEFAULT_NULL
#endif
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t, const char *, uint32_t,
                                   void *, UBaseType_t, TaskHandle_t *,
                                   BaseType_t);
BaseType_t xTaskCreate(TaskFunction_t, const char *, uint32_t, void *,
                       UBaseType_t, TaskHandle_t *);
void vTaskDelete(TaskHandle_t EMU_DEFAULT_NULL);
void vTaskDelay(TickType_t);
UBaseType_t xTaskGetStackHighWaterMark(TaskHandle_t EMU_DEFAULT_NULL);
#define tskIDLE_PRIORITY 0
TickType_t xTaskGetTickCount();
TaskHandle_t xTaskGetCurrentTaskHandle();

#ifdef __cplusplus
}
inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t t = nullptr) { return xTaskGetStackHighWaterMark(t); }
namespace emu {
struct TaskExit {}; /* private cooperative task unwind */
}                   // namespace emu

#endif
