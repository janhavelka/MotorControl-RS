// SPDX-License-Identifier: MIT
#pragma once
#include "../FakeEsp.h"
using TickType_t = uint32_t;
using StackType_t = uint32_t;
#define pdMS_TO_TICKS(ms) (ms)
#define configSUPPORT_STATIC_ALLOCATION 1
#define configNUMBER_OF_CORES 2
