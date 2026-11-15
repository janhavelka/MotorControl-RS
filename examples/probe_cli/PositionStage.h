// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/ActionOperation.h>
#include <MotorControlRS/ReadOperation.h>
#include <MotorControlRS/Status.h>
#include <MotorControlRS/profiles/ess_rs/Actions.h>
#include <cstdint>

namespace MotorControlRSExample { namespace Probe {

/** One staged finite move: its five 0x0021 words (acceleration, deceleration,
 * speed, target pair in the drive's word order) written with one FC10 and read
 * back with one FC03. No start word is ever sent; 0x0020 start speed is not
 * written. A following USE_STORED move sends only the start. Plain data. */
struct PositionStageView {
    MotorControlRS::ReadTarget target;
    uint32_t operationId = 0, configurationGeneration = 0, serialGeneration = 0;
    bool pending = false;
    bool staged = false; ///< Checked words match after an ACK, or an unchanged candidate needed no write.
    bool cancelled = false; ///< A stop, recovery or rebinding ended unsent work.
    uint16_t words[5] = {};
    bool readbackKnown = false;
    uint16_t readback[5] = {};
    MotorControlRS::ActionExecution execution = MotorControlRS::ActionExecution::NOT_TRANSMITTED; ///< FC10 write.
    bool txComplete = false; ///< FC10 physical TX completion.
    uint64_t txEndUs = 0;
    uint32_t txUncertaintyUs = 0;
    bool writeQualified = false, readQualified = false;
    uint64_t writeLatestUs = 0, readEarliestUs = 0, readLatestUs = 0;
    uint64_t startedUs = 0, deadlineUs = 0, finishedUs = 0;
    MotorControlRS::ESS_RS::ActionEvidence writeEvidence, readEvidence;
    MotorControlRS::Status status;
};

}} // namespace MotorControlRSExample::Probe
