// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/profiles/ess_rs/DriverSettings.h>
#include <cstddef>
#include <cstdint>

namespace MotorControlRSExample {
/** Measured input status mapping (0x0008 against the 0x0040 inversion) for one
 * exact model and firmware. An application record, not a family default: it
 * applies only to a target with a fresh checked identity read of that exact
 * target and binding (`copyRecordedInputMapping`). sourceId names the
 * measurement (date), whose report holds the raw status words. */
struct RecordedInputMapping {
    uint16_t model, firmware;
    MotorControlRS::ESS_RS::IoStatusMapping inputs[4];
    uint32_t sourceId;
};
/** Wired bench 05, 19 November 2026 (docs/reports/2026-11-19_wired_bench_05_status_mapping_input_timing.md):
 * drives 1, 2 and 4, model 0x4EEA, firmware 0x0029, X0-X3, function 0 and PIN
 * function 14+t, polarity 1 with the source off read 1 and on read 0 on all 24
 * terminal/drive/function cases: the status bit is the level after inversion. */
constexpr RecordedInputMapping RECORDED_INPUT_MAPPINGS[] = {
    {0x4EEA, 0x0029, {MotorControlRS::ESS_RS::IoStatusMapping::AFTER_INVERSION,
                      MotorControlRS::ESS_RS::IoStatusMapping::AFTER_INVERSION,
                      MotorControlRS::ESS_RS::IoStatusMapping::AFTER_INVERSION,
                      MotorControlRS::ESS_RS::IoStatusMapping::AFTER_INVERSION}, 20261119},
};
/** Input-filter coefficients (0x0108) the standalone tuning experiment admits,
 * from and to any value in the set: the prompt 05 candidates with complete
 * delay and width results on drive 1 (wired bench 05, 19 November 2026). 100
 * left the set: it suppressed every 200 ms pulse, so its delay step has no
 * interval. Stored readback only; the delay (about 2.5-2.6 ms per unit,
 * inferred from 10-100) is characterized in the 05 report, not promised here. */
constexpr uint16_t MEASURED_INPUT_FILTERS[] = {0, 1, 2, 3, 5, 10, 20, 50};
inline bool measuredInputFilter(uint16_t value) noexcept {
    for (const uint16_t admitted : MEASURED_INPUT_FILTERS)
        if (admitted == value) return true;
    return false;
}
} // namespace MotorControlRSExample
