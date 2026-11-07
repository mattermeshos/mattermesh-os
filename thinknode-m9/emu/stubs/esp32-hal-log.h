#pragma once
// Arduino-ESP32 log macros: errors and warnings go to stderr (as the core's default level would show them),
// info/debug/verbose are compiled out like a release build.
#include <cstdio>
#define log_e(fmt,...) fprintf(stderr,"[E][%s:%d] " fmt "\n",__FILE__,__LINE__,##__VA_ARGS__)
#define log_w(fmt,...) fprintf(stderr,"[W][%s:%d] " fmt "\n",__FILE__,__LINE__,##__VA_ARGS__)
#define log_i(fmt,...) do{}while(0)
#define log_d(fmt,...) do{}while(0)
#define log_v(fmt,...) do{}while(0)
