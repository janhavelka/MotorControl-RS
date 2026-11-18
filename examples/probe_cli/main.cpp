// SPDX-License-Identifier: MIT
#include "ProbeApp.h"
#include "../common/BoardPins.h"
#include <Arduino.h>
#if MOTORCONTROLRS_BENCH_IO
#include "../common/Esp32BenchIo.h"
#include <esp_attr.h>
#endif
using namespace MotorControlRSExample;

// Bench, 16 November 2026: the group move path left 900 of the core's default
// 8192 loop-task bytes free. `memory` reports the remaining margin.
SET_LOOP_TASK_STACK_SIZE(12 * 1024);

#if MOTORCONTROLRS_BENCH_IO
namespace {
DRAM_ATTR Esp32BenchIo benchIo; // Timer and interrupt state stay in internal RAM.
}
#endif

void setup() {
    ApplicationOptions options;
#if MOTORCONTROLRS_BENCH_IO
    // Drive the bench outputs to their inactive levels before anything else.
    BenchIo::Pins pins;
    pins.outputs[0].gpio = Board::kRelay1Pin;
    pins.outputs[1].gpio = Board::kRelay2Pin;
    pins.outputs[2].gpio = Board::kMosfet1Pin;
    pins.outputs[3].gpio = Board::kMosfet2Pin;
    pins.input.gpio = Board::kAux1InputPin;
    pins.input.activeLow = Board::kAux1InputActiveLow;
    const int reserved[] = {Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin};
    // A failed fixture stays visible as `bench` ready=false; motor control still starts.
    benchIo.begin(pins, reserved, sizeof(reserved) / sizeof(reserved[0]));
    options.benchIo = &benchIo;
#endif
#if MOTORCONTROLRS_FIXTURE_WIRING
    // Board-declared terminal wiring (BoardPins.h). An invalid table stays
    // visible as `wiring` fixture invalid; motor control still starts.
    static_assert(kFixtureWiringCount <= FIXTURE_WIRING_CAPACITY, "board fixture table exceeds the application capacity");
    options.fixtureWiringSupplied = true;
    options.fixtureWiringCount = static_cast<uint8_t>(kFixtureWiringCount);
    for (std::size_t i = 0; i < kFixtureWiringCount; ++i) options.fixtureWiring[i] = kFixtureWiring[i];
#endif
    beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
                      Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit, options);
}
void loop() { serviceApplication(); }
