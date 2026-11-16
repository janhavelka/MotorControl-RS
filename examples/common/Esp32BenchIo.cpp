// SPDX-License-Identifier: MIT
#include "Esp32BenchIo.h"
#include <driver/gpio.h>
#include <esp_err.h>

namespace MotorControlRSExample {
namespace {
uint64_t now() { return static_cast<uint64_t>(esp_timer_get_time()); }
gpio_num_t pin(int gpio) { return static_cast<gpio_num_t>(gpio); }
// One lock for outputs and edges, shared by the application task, the
// esp_timer task and the input interrupt. Sections hold only GPIO register
// writes and copies; timer start/stop run outside them.
struct Guard {
    explicit Guard(portMUX_TYPE& mux) : mux_(mux) { portENTER_CRITICAL_SAFE(&mux_); }
    ~Guard() { portEXIT_CRITICAL_SAFE(&mux_); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    portMUX_TYPE& mux_;
};
}

bool Esp32BenchIo::begin(const BenchIo::Pins& pins, const int* reserved, std::size_t reservedCount) {
    if (ready_ || !BenchIo::validPins(pins, reserved, reservedCount)) return false;
    pins_ = pins;
    const uint64_t started = now();
    bool ok = true;
    for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i) {
        Output& output = outputs_[i];
        output.owner = this; output.index = i;
        output.view = BenchIo::OutputView();
        output.view.gpio = pins_.outputs[i].gpio;
        output.view.offReason = BenchIo::OffReason::BOOT;
        output.view.offUs = started;
        if (!present(i)) continue;
        // Set the inactive level before the pad becomes an output, and again
        // after configuration, so the pin never drives the active level.
        write(i, false);
        gpio_config_t config = {};
        config.pin_bit_mask = 1ULL << pins_.outputs[i].gpio;
        config.mode = GPIO_MODE_OUTPUT;
        config.pull_up_en = GPIO_PULLUP_DISABLE;
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.intr_type = GPIO_INTR_DISABLE;
        if (gpio_config(&config) != ESP_OK) ok = false;
        write(i, false);
        esp_timer_create_args_t timer = {};
        timer.callback = expire; timer.arg = &output;
        timer.dispatch_method = ESP_TIMER_TASK; timer.name = "bench_off";
        if (!output.timer && esp_timer_create(&timer, &output.timer) != ESP_OK) ok = false;
    }
    if (ok && pins_.input.gpio >= 0) {
        // The board's input stage has its own pull-up; no internal pull here.
        gpio_config_t config = {};
        config.pin_bit_mask = 1ULL << pins_.input.gpio;
        config.mode = GPIO_MODE_INPUT;
        config.pull_up_en = GPIO_PULLUP_DISABLE;
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.intr_type = GPIO_INTR_ANYEDGE;
        const esp_err_t service = gpio_install_isr_service(0);
        ok = gpio_config(&config) == ESP_OK &&
            (service == ESP_OK || service == ESP_ERR_INVALID_STATE) &&
            gpio_isr_handler_add(pin(pins_.input.gpio), edge, this) == ESP_OK;
    }
    ready_ = ok;
    return ok;
}

void Esp32BenchIo::write(uint8_t index, bool active) {
    const BenchIo::OutputPin& output = pins_.outputs[index];
    gpio_set_level(pin(output.gpio), active == output.activeHigh ? 1 : 0);
}

void Esp32BenchIo::off(uint8_t index, BenchIo::OffReason reason) {
    Output& output = outputs_[index];
    const uint64_t at = now();
    {
        Guard guard(mux_);
        if (output.view.active) {
            write(index, false);
            output.view.active = false;
            output.view.offUs = at; output.view.dueUs = 0;
            output.view.offReason = reason;
        }
    }
    // Already OFF: a late callback finds it inactive and does nothing.
    if (output.timer) esp_timer_stop(output.timer);
}

void Esp32BenchIo::expire(void* argument) {
    Output& output = *static_cast<Output*>(argument);
    Esp32BenchIo& self = *output.owner;
    const uint64_t at = now();
    Guard guard(self.mux_);
    if (!BenchIo::timerExpires(output.view.active, output.view.dueUs, at)) return;
    self.write(output.index, false);
    output.view.active = false;
    output.view.offUs = at; output.view.dueUs = 0;
    output.view.offReason = BenchIo::OffReason::LEASE;
}

// The GPIO ISR service is installed without ESP_INTR_FLAG_IRAM, so this
// handler is deferred while the flash cache is disabled and may run from flash.
void Esp32BenchIo::edge(void* argument) {
    Esp32BenchIo& self = *static_cast<Esp32BenchIo*>(argument);
    const int level = gpio_get_level(pin(self.pins_.input.gpio));
    const bool active = self.pins_.input.activeLow ? level == 0 : level != 0;
    const uint64_t at = now();
    Guard guard(self.mux_);
    self.edges_.push(at, active);
}

void Esp32BenchIo::service(uint64_t nowUs) {
    if (!ready_) return;
    for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i) {
        if (!present(i)) continue;
        Output& output = outputs_[i];
        Guard guard(mux_);
        if (!BenchIo::lateExpiry(output.view.active, output.view.dueUs, nowUs)) continue;
        write(i, false);
        output.view.active = false;
        output.view.offUs = nowUs; output.view.dueUs = 0;
        output.view.offReason = BenchIo::OffReason::LATE;
    }
}

void Esp32BenchIo::quiesce(BenchIo::OffReason reason) {
    if (!ready_) return;
    for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i) if (present(i)) off(i, reason);
}

void Esp32BenchIo::snapshot(BenchIo::View& view) {
    view = BenchIo::View();
    view.ready = ready_;
    view.input.gpio = pins_.input.gpio;
    {
        Guard guard(mux_);
        for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i) view.outputs[i] = outputs_[i].view;
        edges_.copy(view.input);
    }
    if (ready_ && pins_.input.gpio >= 0) {
        const int level = gpio_get_level(pin(pins_.input.gpio));
        view.input.known = true;
        view.input.active = pins_.input.activeLow ? level == 0 : level != 0;
    }
    view.nowUs = now();
}

BenchIo::Result Esp32BenchIo::apply(const BenchIo::Request& request, BenchIo::View& view) {
    BenchIo::Result result = BenchIo::Result::OK;
    if (!ready_) result = BenchIo::Result::UNAVAILABLE;
    else switch (request.op) {
    case BenchIo::Op::SHOW: break;
    case BenchIo::Op::ON: {
        if (!present(request.output) || !BenchIo::validHold(request.holdMs)) { result = BenchIo::Result::INVALID; break; }
        Output& output = outputs_[request.output];
        // A callback already dispatched for an earlier ON sees the new due time
        // and does nothing; see BenchIo::timerExpires.
        esp_timer_stop(output.timer);
        const uint64_t holdUs = static_cast<uint64_t>(request.holdMs) * 1000;
        const uint64_t at = now();
        {
            Guard guard(mux_);
            if (!output.view.active) {
                output.view.onUs = at;
                if (output.view.ons != UINT32_MAX) ++output.view.ons;
            }
            output.view.active = true;
            output.view.holdMs = request.holdMs;
            output.view.dueUs = at + holdUs;
            output.view.offReason = BenchIo::OffReason::NONE;
            write(request.output, true);
        }
        if (esp_timer_start_once(output.timer, holdUs) != ESP_OK) {
            off(request.output, BenchIo::OffReason::FAULT);
            result = BenchIo::Result::FAILED;
        }
        break;
    }
    case BenchIo::Op::OFF:
        if (!present(request.output)) { result = BenchIo::Result::INVALID; break; }
        off(request.output, BenchIo::OffReason::REQUEST);
        break;
    case BenchIo::Op::OFF_ALL:
        quiesce(BenchIo::OffReason::REQUEST);
        break;
    case BenchIo::Op::CLEAR_EDGES: {
        Guard guard(mux_);
        edges_.clear();
        break;
    }
    }
    snapshot(view);
    return result;
}

} // namespace MotorControlRSExample
