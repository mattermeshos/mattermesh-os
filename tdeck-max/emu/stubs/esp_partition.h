#pragma once
#include <cstdint>
typedef struct {int type;int subtype;uint32_t address;uint32_t size;char label[17];bool encrypted;} esp_partition_t;
