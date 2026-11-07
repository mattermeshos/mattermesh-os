#pragma once
#include <cstdint>
typedef struct {int type;int subtype;uint32_t address;uint32_t size;char label[17];bool encrypted;} esp_partition_t;
// bmorcelli's Launcher keeps itself in an app partition of subtype "test": MM_EMU_LAUNCHER=1 makes the emulator run as
// a firmware started by it (emu/tests/ota_launcher_plus.sh); otherwise only the running image exists.
#include <cstdlib>
#include <cstring>
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_SUBTYPE_APP_TEST 0x20
inline const esp_partition_t *esp_partition_find_first(int type,int subtype,const char*){
 static const esp_partition_t launcher={0,0x20,0x10000,0x180000,"app0",false};const char *on=std::getenv("MM_EMU_LAUNCHER");
 return type==ESP_PARTITION_TYPE_APP&&subtype==ESP_PARTITION_SUBTYPE_APP_TEST&&on&&!std::strcmp(on,"1")?&launcher:nullptr;}
