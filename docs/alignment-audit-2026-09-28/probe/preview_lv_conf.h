#pragma once
// Rendering/fonts/widgets come from the firmware configuration. The desktop
// renderer registers four extra 64 KiB pools, matching runtime.cpp's budget.
#include "../../../include/lv_conf.h"
#define LV_MEM_SIZE (64 * 1024U)
