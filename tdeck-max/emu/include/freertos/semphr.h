#pragma once
#include "FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#define EMU_DEFAULT_NULL = nullptr
#else
#define EMU_DEFAULT_NULL
#endif
SemaphoreHandle_t xSemaphoreCreateMutex();
SemaphoreHandle_t xSemaphoreCreateBinary();
BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t);
BaseType_t xSemaphoreGive(SemaphoreHandle_t);
void vSemaphoreDelete(SemaphoreHandle_t);

#ifdef __cplusplus
}
#endif
