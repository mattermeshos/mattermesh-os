#pragma once
#ifdef MM_BOARD_MAX
#include "max_devices.h"
namespace boarddev = maxdev;
constexpr const char *emuBoardName = "max";
#else
#include "plus_devices.h"
namespace boarddev = plusdev;
constexpr const char *emuBoardName = "plus";
#endif
