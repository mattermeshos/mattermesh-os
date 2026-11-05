#pragma once
#include "FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#define EMU_DEFAULT_NULL = nullptr
#else
#define EMU_DEFAULT_NULL
#endif
QueueHandle_t xQueueCreate(UBaseType_t, UBaseType_t);
BaseType_t xQueueSend(QueueHandle_t, const void *, TickType_t);
BaseType_t xQueueSendToBack(QueueHandle_t, const void *, TickType_t);
BaseType_t xQueueReceive(QueueHandle_t, void *, TickType_t);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t);
void vQueueDelete(QueueHandle_t);

#ifdef __cplusplus
}
#endif
