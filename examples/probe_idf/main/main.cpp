// SPDX-License-Identifier: MIT
#include "ProbeApp.h"
#include "ProbePlatform.h"
#include <esp_attr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>
#if CONFIG_MOTORCONTROLRS_BENCH_IO
#include "../../common/Esp32BenchIo.h"
#endif

#if !configSUPPORT_STATIC_ALLOCATION || configNUMBER_OF_CORES != 2
#error The standalone owner requires static FreeRTOS allocation and both ESP32-S3 cores
#endif

#if CONFIG_MOTORCONTROLRS_RS485_TX_PIN == 19 || CONFIG_MOTORCONTROLRS_RS485_TX_PIN == 20 || \
    CONFIG_MOTORCONTROLRS_RS485_RX_PIN == 19 || CONFIG_MOTORCONTROLRS_RS485_RX_PIN == 20 || \
    CONFIG_MOTORCONTROLRS_RS485_DE_PIN == 19 || CONFIG_MOTORCONTROLRS_RS485_DE_PIN == 20
#error GPIO19 and GPIO20 are reserved for the standalone USB Serial/JTAG console
#endif

namespace {
using namespace MotorControlRSExample;
// These driver-facing task objects stay internal even with external BSS enabled.
// Bench, 16 November 2026: the Arduino group move path left 900 of 8192 bytes
// free; both builds give the shared application 12 KiB. `memory` reports it.
DRAM_ATTR StackType_t ownerStack[12288 / sizeof(StackType_t)];
DRAM_ATTR StaticTask_t ownerState;
#if CONFIG_MOTORCONTROLRS_BENCH_IO
DRAM_ATTR Esp32BenchIo benchIo; // Timer and interrupt state stay in internal RAM.
#endif

void ownerTask(void*) {
    const Esp32S3Uart::Pins pins = {CONFIG_MOTORCONTROLRS_RS485_TX_PIN,
        CONFIG_MOTORCONTROLRS_RS485_RX_PIN, CONFIG_MOTORCONTROLRS_RS485_DE_PIN,
#if CONFIG_MOTORCONTROLRS_DE_ACTIVE_HIGH
        true
#else
        false
#endif
    };
#if CONFIG_MOTORCONTROLRS_RECEIVER_DISABLED_DURING_TX
    constexpr bool receiverDisabledDuringTransmit = true;
#else
    constexpr bool receiverDisabledDuringTransmit = false;
#endif
    ApplicationOptions options;
#if CONFIG_MOTORCONTROLRS_BENCH_IO
    // Drive the bench outputs to their inactive levels before the application starts.
    BenchIo::Pins bench;
    bench.outputs[0].gpio = CONFIG_MOTORCONTROLRS_BENCH_RELAY1_PIN;
    bench.outputs[1].gpio = CONFIG_MOTORCONTROLRS_BENCH_RELAY2_PIN;
    bench.outputs[2].gpio = CONFIG_MOTORCONTROLRS_BENCH_MOSFET1_PIN;
    bench.outputs[3].gpio = CONFIG_MOTORCONTROLRS_BENCH_MOSFET2_PIN;
    bench.input.gpio = CONFIG_MOTORCONTROLRS_BENCH_AUX1_PIN;
    const int reserved[] = {pins.tx, pins.rx, pins.de};
    // A failed fixture stays visible as `bench` ready=false; motor control still starts.
    benchIo.begin(bench, reserved, sizeof(reserved) / sizeof(reserved[0]));
    options.benchIo = &benchIo;
#endif
#if CONFIG_MOTORCONTROLRS_FIXTURE_WIRING
    // Board-declared terminal wiring from menuconfig; address 0 leaves a slot
    // unused. An invalid table stays visible as `wiring` fixture invalid.
    const FixtureWiringEntry slots[FIXTURE_WIRING_CAPACITY] = {
        {CONFIG_MOTORCONTROLRS_FIXTURE_SLOT1_ADDRESS, CONFIG_MOTORCONTROLRS_FIXTURE_SLOT1_INPUTS, CONFIG_MOTORCONTROLRS_FIXTURE_SLOT1_OUTPUTS},
        {CONFIG_MOTORCONTROLRS_FIXTURE_SLOT2_ADDRESS, CONFIG_MOTORCONTROLRS_FIXTURE_SLOT2_INPUTS, CONFIG_MOTORCONTROLRS_FIXTURE_SLOT2_OUTPUTS},
        {CONFIG_MOTORCONTROLRS_FIXTURE_SLOT3_ADDRESS, CONFIG_MOTORCONTROLRS_FIXTURE_SLOT3_INPUTS, CONFIG_MOTORCONTROLRS_FIXTURE_SLOT3_OUTPUTS},
        {CONFIG_MOTORCONTROLRS_FIXTURE_SLOT4_ADDRESS, CONFIG_MOTORCONTROLRS_FIXTURE_SLOT4_INPUTS, CONFIG_MOTORCONTROLRS_FIXTURE_SLOT4_OUTPUTS}};
    options.fixtureWiringSupplied = true;
    for (const auto& slot : slots)
        if (slot.address) options.fixtureWiring[options.fixtureWiringCount++] = slot;
#endif
    beginApplication(pins, receiverDisabledDuringTransmit, options);
    // Initialization failure is reported by the shared application. Its cached
    // console remains available when storage exists; a missing App only idles.
    for (;;) serviceApplication();
}
}

extern "C" void app_main() {
    // ESP-IDF expresses this stack bound in bytes, unlike vanilla FreeRTOS.
    if (!xTaskCreateStaticPinnedToCore(ownerTask, "motor_owner", sizeof(ownerStack),
                                     nullptr, 1, ownerStack, &ownerState, 1)) {
        MotorControlRSExample::Platform::beginConsole();
        MotorControlRSExample::Platform::bootFailure("owner_task_allocation");
    }
}
