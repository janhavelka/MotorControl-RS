// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/ActionOperation.h>
#include <MotorControlRS/ReadOperation.h>
#include <MotorControlRS/Status.h>
#include <MotorControlRS/profiles/ess_rs/Actions.h>
#include <cstdint>

namespace MotorControlRSExample { namespace Probe {

/** Evidence for five 0x0021 words: acceleration, deceleration, speed and target
 * pair in the drive's word order. Used by prepared-move staging and raw candidates.
 * No start word is sent and 0x0020 start speed is not written.
 * Successful stagePositionMove publishes the selected-axis witness for a
 * matching USE_STORED move. Raw candidates never publish that witness. */
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
