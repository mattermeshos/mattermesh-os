#pragma once
// A wall clock is trusted from 2024-01-01 to 2100-01-01 (UTC); local time follows the chosen zone (services/time_zone.h).
#define MM_EPOCH_MIN 1704067200
#define MM_EPOCH_END 4102444800LL
namespace device {const char *timeZonePosix();}
#define MM_TZ device::timeZonePosix()
