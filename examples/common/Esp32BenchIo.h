// SPDX-License-Identifier: MIT
#pragma once
#include "BenchIo.h"
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

namespace MotorControlRSExample {

/** ESP32-S3 bench outputs and input for the standalone example.
 *
 * begin() drives every present output to its inactive level before it becomes
 * an output. Each ON starts a one-shot esp_timer that turns it OFF after the
 * hold; the application service loop is a backstop. The input interrupt records
 * edge times. Keep this object in internal RAM; use one instance.
 */
class Esp32BenchIo final : public BenchIo::Port {
public:
    Esp32BenchIo() = default;
    Esp32BenchIo(const Esp32BenchIo&) = delete;
    Esp32BenchIo& operator=(const Esp32BenchIo&) = delete;
    /** Configure pins once. reserved lists pins owned elsewhere (RS485).
     * Failure leaves configured outputs at their inactive level. */
    bool begin(const BenchIo::Pins&, const int* reserved, std::size_t reservedCount);
    bool ready() const { return ready_; }
    BenchIo::Result apply(const BenchIo::Request&, BenchIo::View&) override;
    void quiesce(BenchIo::OffReason) override;
    void service(uint64_t nowUs) override;
private:
    struct Output {
        Esp32BenchIo* owner = nullptr;
        uint8_t index = 0;
        esp_timer_handle_t timer = nullptr;
        BenchIo::OutputView view;
    };
    static void expire(void*);
    static void edge(void*);
    void write(uint8_t index, bool active);
    void off(uint8_t index, BenchIo::OffReason);
    void snapshot(BenchIo::View&);
    bool present(uint8_t index) const { return index < BenchIo::OUTPUTS && pins_.outputs[index].gpio >= 0; }
    BenchIo::Pins pins_;
    Output outputs_[BenchIo::OUTPUTS];
    BenchIo::EdgeRing edges_;
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    bool ready_ = false;
};

} // namespace MotorControlRSExample
