// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/ActionOperation.h>
#include <MotorControlRS/ReadOperation.h>
#include <MotorControlRS/Status.h>
#include <MotorControlRS/profiles/ess_rs/Actions.h>
#include <cstdint>

namespace MotorControlRSExample { namespace Probe {

/** One explicit write of exact words followed by one FC03 readback of the same
 * window, through the ordinary owner, on the selected axis. No start word.
 * HOME_RESTORE writes 0x0031/6 and VELOCITY_RESTORE 0x001D/3 (reviewed FC10
 * windows) with the words of an earlier checked `home read` / `velocity read`
 * of the same drive and binding (snapshotId; D7 exact raw words, a nonzero
 * offset included). */
enum class ParameterWriteKind : uint8_t { NONE, HOME_RESTORE, VELOCITY_RESTORE, PAIR_EXPERIMENT };
/** PAIR_EXPERIMENT (wired bench 10, decision D4) is admitted only by an image
 * built with MOTORCONTROLRS_WB10_EXPERIMENT; every other image refuses it
 * UNSUPPORTED before TX. It writes one ledger experiment window (a PT target
 * pair: first, two words) through the separate experiment builder: candidate
 * words from the image's fixed list, or (snapshotId) the exact words of an
 * earlier checked read of that window on the same drive and binding (D7
 * restoration), then reads the window back once; nothing is resent. */

/** Request for one session: the kind and the operation ID of the checked source read. */
struct ParameterWriteRequest {
    ParameterWriteKind kind = ParameterWriteKind::NONE;
    uint32_t snapshotId = 0;
    uint16_t first = 0;      ///< PAIR_EXPERIMENT window start.
    uint16_t words[2] = {};  ///< PAIR_EXPERIMENT candidate words (snapshotId 0).
};

/** Copied session state. pending covers the write and its readback. ok means
 * an acknowledged write and a readback equal to the requested words.
 * unsettled means a transmitted write whose execution is unknown and has no
 * checked readback yet: the next restore is refused until a checked read of
 * the same window (home read or velocity read) completes after it. Nothing is
 * ever resent. */
struct ParameterWriteView {
    ParameterWriteKind kind = ParameterWriteKind::NONE;
    MotorControlRS::ReadTarget target;
    uint32_t operationId = 0, snapshotId = 0, configurationGeneration = 0, serialGeneration = 0;
    uint16_t first = 0;
    uint8_t count = 0;
    uint16_t words[6] = {}, readback[6] = {};
    bool pending = false, ok = false, readbackKnown = false, cancelled = false, unsettled = false;
    MotorControlRS::ActionExecution execution = MotorControlRS::ActionExecution::NOT_TRANSMITTED;
    uint64_t startedUs = 0, deadlineUs = 0, finishedUs = 0;
    MotorControlRS::ESS_RS::ActionEvidence writeEvidence, readEvidence;
    const char* error = "none";
};

}} // namespace MotorControlRSExample::Probe
