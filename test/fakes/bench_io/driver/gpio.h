// SPDX-License-Identifier: MIT
#pragma once
#include "../esp_err.h"
#include <cstdint>

enum gpio_num_t { GPIO_NUM_NC = -1, GPIO_NUM_0 = 0, GPIO_NUM_MAX = 49 };
enum gpio_mode_t { GPIO_MODE_DISABLE = 0, GPIO_MODE_INPUT = 1, GPIO_MODE_OUTPUT = 2 };
enum gpio_pullup_t { GPIO_PULLUP_DISABLE = 0, GPIO_PULLUP_ENABLE = 1 };
enum gpio_pulldown_t { GPIO_PULLDOWN_DISABLE = 0, GPIO_PULLDOWN_ENABLE = 1 };
enum gpio_int_type_t { GPIO_INTR_DISABLE = 0, GPIO_INTR_ANYEDGE = 3 };
struct gpio_config_t {
    uint64_t pin_bit_mask;
    gpio_mode_t mode;
    gpio_pullup_t pull_up_en;
    gpio_pulldown_t pull_down_en;
    gpio_int_type_t intr_type;
};
using gpio_isr_t = void (*)(void*);
esp_err_t gpio_config(const gpio_config_t*);
esp_err_t gpio_set_level(gpio_num_t, uint32_t level);
int gpio_get_level(gpio_num_t);
esp_err_t gpio_install_isr_service(int flags);
esp_err_t gpio_isr_handler_add(gpio_num_t, gpio_isr_t, void*);
