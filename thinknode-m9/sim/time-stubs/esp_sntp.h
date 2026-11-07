#pragma once
#include <sys/time.h>
inline void (*fakeNTPCallback)(timeval*)=nullptr;
inline void sntp_set_time_sync_notification_cb(void (*callback)(timeval*)){fakeNTPCallback=callback;}

constexpr int SNTP_SYNC_STATUS_COMPLETED=1;inline int fakeSNTPStatus=0;inline void sntp_set_sync_status(int value){fakeSNTPStatus=value;}inline bool sntp_restart(){return true;}
extern "C" void sntp_sync_time(struct timeval *tv);
