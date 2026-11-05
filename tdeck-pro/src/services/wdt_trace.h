#pragma once
namespace device {
// WDT_LAST: the tasks running on each core when the task watchdog last reset the board (RTC memory, read at boot).
void wdtTraceBegin();// at boot, after Serial.begin: takes the trace and prints it
void wdtTraceReport();// "wdt status"
}
