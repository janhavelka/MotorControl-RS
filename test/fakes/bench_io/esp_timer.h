// SPDX-License-Identifier: MIT
#pragma once
#include "esp_err.h"
#include <cstdint>

struct esp_timer;
using esp_timer_handle_t = esp_timer*;
using esp_timer_cb_t = void (*)(void*);
enum esp_timer_dispatch_t { ESP_TIMER_TASK = 0, ESP_TIMER_ISR = 1 };
struct esp_timer_create_args_t {
    esp_timer_cb_t callback;
    void* arg;
    esp_timer_dispatch_t dispatch_method;
    const char* name;
    bool skip_unhandled_events;
};
int64_t esp_timer_get_time();
esp_err_t esp_timer_create(const esp_timer_create_args_t*, esp_timer_handle_t*);
esp_err_t esp_timer_start_once(esp_timer_handle_t, uint64_t timeoutUs);
esp_err_t esp_timer_stop(esp_timer_handle_t);
