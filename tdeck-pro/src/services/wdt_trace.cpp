// Task-watchdog trace: ESP-IDF calls esp_task_wdt_isr_user_handler() (weak in libesp_system) when the watchdog fires,
// just before the panic reset. Its report goes to UART0, which nobody reads on a T-Deck, so the tasks then running on
// each core and the uptime are kept in RTC memory (survives the reset) and printed over USB at the next boot.
#include <Arduino.h>
#include "wdt_trace.h"
#ifdef MM_EMULATOR
namespace device {void wdtTraceBegin(){Serial.println("WDT_LAST none");}void wdtTraceReport(){Serial.println("WDT_LAST none");}}// no task watchdog on the PC
#else
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <esp_timer.h>
namespace {
constexpr uint32_t Magic=0x57445431;// "WDT1"
struct Trace {uint32_t magic;uint32_t uptimeMs;char core[2][16];};
RTC_NOINIT_ATTR Trace trace;
}
extern "C" void esp_task_wdt_isr_user_handler(void){
 for(int c=0;c<2;c++){TaskHandle_t h=xTaskGetCurrentTaskHandleForCPU(c);const char *name=h?pcTaskGetName(h):"?";
  size_t i=0;for(;name&&name[i]&&i<sizeof(trace.core[c])-1;i++)trace.core[c][i]=name[i];trace.core[c][i]=0;}
 trace.uptimeMs=uint32_t(esp_timer_get_time()/1000);trace.magic=Magic;
}
namespace device {
static char last[64]="WDT_LAST none";
void wdtTraceBegin(){
 if(trace.magic==Magic){trace.core[0][15]=trace.core[1][15]=0;
  snprintf(last,sizeof(last),"WDT_LAST core0=%s core1=%s uptime_ms=%lu",trace.core[0],trace.core[1],(unsigned long)trace.uptimeMs);}
 trace.magic=0;Serial.println(last);
}
void wdtTraceReport(){Serial.println(last);}
}
#endif
