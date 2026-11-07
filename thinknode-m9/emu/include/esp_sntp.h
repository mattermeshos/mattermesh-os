#pragma once
#include <cstdint>
#include <sys/time.h>
using sntp_sync_time_cb_t = void (*)(struct timeval *);
enum sntp_sync_status_t {
  SNTP_SYNC_STATUS_RESET,
  SNTP_SYNC_STATUS_COMPLETED,
  SNTP_SYNC_STATUS_IN_PROGRESS
};
#define SNTP_OPMODE_POLL 0
inline void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t) {}
inline bool sntp_restart() { return false; }
// The firmware may supply its own SNTP time validation callback.
extern "C" void sntp_sync_time(struct timeval *);
inline void sntp_set_sync_status(sntp_sync_status_t) {}
inline void sntp_setoperatingmode(uint8_t) {}
inline void sntp_setservername(uint8_t, const char *) {}
inline void sntp_init() {}
inline bool sntp_enabled() { return false; }
