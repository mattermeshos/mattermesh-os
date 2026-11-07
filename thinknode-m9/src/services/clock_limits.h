#pragma once
// A wall clock is trusted from 2024-01-01 to 2100-01-01 (UTC); local time is Central European.
#define MM_EPOCH_MIN 1704067200
#define MM_EPOCH_END 4102444800LL
// The local zone chosen by the user (services/time_zone.h); it was fixed to CET/CEST before.
namespace device {const char *timeZonePosix();}
#define MM_TZ device::timeZonePosix()
