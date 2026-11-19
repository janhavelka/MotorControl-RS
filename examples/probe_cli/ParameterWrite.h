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
 * offset included). VELOCITY_SET (the JOG setter, wired bench 14) writes the
 * supplied JOG speed and both ramp words to 0x001D/3: it needs a checked
 * `velocity read` of the same binding first, a nonzero signed speed inside the
 * input velocity limits (inputs 8-10 run at the stored speed) and ramp words
 * 0..2000. */
enum class ParameterWriteKind : uint8_t { NONE, HOME_RESTORE, VELOCITY_RESTORE, VELOCITY_SET };

/** Request for one session: the kind and the operation ID of the checked source
 * read (restores), or the three words to write (VELOCITY_SET: speed as a signed
 * native rpm in two's complement, acceleration and deceleration words). */
struct ParameterWriteRequest {
    ParameterWriteKind kind = ParameterWriteKind::NONE;
    uint32_t snapshotId = 0;
    uint16_t words[3] = {};
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
