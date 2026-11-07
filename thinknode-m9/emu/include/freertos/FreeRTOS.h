#pragma once
#include <cstddef>
#include <cstdint>
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
using StackType_t = uint32_t;
using TaskHandle_t = void *;
using QueueHandle_t = void *;
using SemaphoreHandle_t = void *;
using TaskFunction_t = void (*)(void *);
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY UINT32_MAX
#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(ms) (static_cast<TickType_t>(ms))
#define tskNO_AFFINITY (-1)
#define configTICK_RATE_HZ 1000
#define portTICK_RATE_MS portTICK_PERIOD_MS
