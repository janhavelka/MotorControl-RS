// SPDX-License-Identifier: MIT
#pragma once
#include "PositionStage.h"
#include <MotorControlRS/profiles/ess_rs/Actions.h>

namespace MotorControlRSExample { namespace Probe {

/** Untrusted archive values, without imported generations or readiness.
 * words are ACC, DEC, RPM and the two target words in the supplied order.
 * Start speed at 0x0020 is never part of this write. */
struct ProfileCandidate {
    uint16_t words[5] = {};
    MotorControlRS::ESS_RS::WordOrder wordOrder = MotorControlRS::ESS_RS::WordOrder::HIGH_WORD_FIRST;
    uint8_t address = 0;
    bool identitySpecified = false;
    uint16_t model = 0, firmware = 0;
};

/** Copied result from the one retained candidate slot. Explicit release is
 * required, also after failure. Original is the drive-read saved snapshot;
 * before and priorEvidence come from this operation's fresh six-word read.
 * unchanged means no write was needed; staged then has NOT_TRANSMITTED execution.
 * Candidate success never publishes a prepared-move or interrupt witness. */
struct ProfileCandidateResult {
    ProfileCandidate candidate;
    PositionStageView stage;
    uint16_t original[6] = {}, before[6] = {};
    MotorControlRS::ESS_RS::ActionEvidence priorEvidence;
    bool unchanged = false;
    const char* error = "none";
};

}} // namespace MotorControlRSExample::Probe
