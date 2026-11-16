// SPDX-License-Identifier: MIT
#pragma once
// Minimal SDK surface used by examples/common/Esp32BenchIo.cpp. The fake
// implementations live in test/bench_io_test.cpp.
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_INVALID_STATE = 0x103;
