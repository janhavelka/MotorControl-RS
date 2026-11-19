// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/MoveOperation.h>
#include <MotorControlRS/profiles/ess_rs/InputStart.h>
#include <cstdint>

namespace MotorControlRSExample { namespace Probe {

/** Input-start route (wired bench 08): arm a host observation session, pulse one
 * owned bench output once, observe and attribute, disarm. Console `arm`/`disarm`,
 * the programmatic API (ProbeApp.h) and the Python harness call one route.
 * Function 7 (position start) is delivered; home (11), PT (12) and PV (13)
 * staging refuse before any traffic until prompts 12, 13 and 14. */
constexpr uint8_t INPUT_START_MEMBERS = 4; ///< Drives on one fixture line (FIXTURE_WIRING_CAPACITY).
constexpr uint32_t INPUT_START_MIN_WIDTH_MS = 20;   ///< Named minimum; prompt 09 raises it from its sweep.
constexpr uint32_t INPUT_START_MAX_WIDTH_MS = 5000; ///< Fixed maximum, enough for 09's held level.
constexpr uint32_t INPUT_START_MIN_POLL_MS = 20, INPUT_START_MAX_POLL_MS = 500, INPUT_START_DEFAULT_POLL_MS = 50;
constexpr uint32_t INPUT_START_MIN_WINDOW_MS = 100, INPUT_START_MAX_WINDOW_MS = 30000, INPUT_START_DEFAULT_WINDOW_MS = 10000;
/// D14 backstop: every interlock read is at most this old at the ON.
constexpr uint32_t INPUT_START_MAX_READ_AGE_MS = 1000;

enum class InputStartKind : uint8_t { POSITION, HOME, SEGMENT, VELOCITY };
/// 0x0050 the request needs: RELATIVE 0, ABSOLUTE 1 (absolute and absolute-by).
enum class InputStartMode : uint8_t { RELATIVE, ABSOLUTE };
enum class InputStartPhase : uint8_t { NONE, STAGING, ARMED, TRIGGERING, OBSERVING, DONE };
/** Owned trigger output, as commanded and as the port reported it. */
enum class InputSourceState : uint8_t {
    NOT_COMMANDED, ACTIVE, HOLD_ELAPSED, QUIESCED_BY_STOP, QUIESCED_BY_DISARM, QUIESCED_BY_CANCEL,
    OFF_REQUESTED, PORT_FAILED
};
struct InputStartMember {
    uint8_t address = 0;
    MotorControlRS::MoveRequest move; ///< Native steps; relative, or absolute target.
};
/** Programmatic arm request. The selected axis: group false, one member with
 * the selected address. A group: every declared member, in any order. */
struct InputStartRequest {
    InputStartKind kind = InputStartKind::POSITION;
    uint8_t terminal = 0; ///< X0..X3.
    InputStartMode mode = InputStartMode::RELATIVE;
    bool group = false;
    uint8_t count = 0;
    InputStartMember members[INPUT_START_MEMBERS];
    uint32_t pollMs = INPUT_START_DEFAULT_POLL_MS, windowMs = INPUT_START_DEFAULT_WINDOW_MS;
};
/** One participant of the line at the trigger: its reads and its truth-table row.
 * row "L1".."L7" for non-members; "M" for an admitted armed member, L1/L2/L4 when
 * a member is unreadable, alarmed or changed since arming. */
struct InputStartRow {
    uint8_t address = 0;
    bool member = false, admit = false, ioKnown = false, stateKnown = false;
    char row[3] = {};
    const char* reason = "none";
    uint16_t io[5] = {};    ///< 0x0040/5: input polarity, X0..X3 functions.
    uint16_t state[7] = {}; ///< 0x0006/7.
    uint32_t ioOperationId = 0, stateOperationId = 0; ///< Sequence of the reads within the trigger step.
    uint64_t ioStartedUs = 0, stateStartedUs = 0, stateReplyUs = 0;
    uint32_t ioAgeUs = 0, stateAgeUs = 0; ///< To the ON (or to the decision when refused).
};
struct InputStartMemberView {
    uint8_t address = 0;
    uint32_t configurationGeneration = 0;
    uint16_t function = 0, polarity = 0, positionMode = 0;
    uint16_t stagedWords[5] = {};
    uint32_t stageOperationId = 0;
    bool staged = false;
    MotorControlRS::ActionExecution stageExecution = MotorControlRS::ActionExecution::NOT_TRANSMITTED;
    int64_t expectedDisplacement = 0;
    int32_t expectedEndpoint = 0, baselinePosition = 0;
    uint32_t stopId = 0;
    const char* stopResult = "none"; ///< none, pending, confirmed, failed, unknown.
    uint64_t stopServiceDelayUs = 0; ///< Stop admission to its first accepted TX byte, while sampling.
    bool assignmentKnown = false;
    uint16_t assignment = 0; ///< Function on the terminal at the latest check; disarm never changes it.
    bool resultKnown = false;
    MotorControlRS::ESS_RS::InputStartResult result;
};
struct InputStartView {
    uint32_t operationId = 0;
    InputStartKind kind = InputStartKind::POSITION;
    uint8_t terminal = 0;
    InputStartMode mode = InputStartMode::RELATIVE;
    bool group = false;
    InputStartPhase phase = InputStartPhase::NONE;
    const char* error = "none";
    uint32_t pollMs = 0, windowMs = 0;
    uint64_t startedUs = 0, armedUs = 0, finishedUs = 0;
    // Trigger source: controller GPIO-write times, not an electrical measurement.
    bool triggered = false, outputKnown = false;
    uint8_t output = 0;
    uint32_t widthMs = 0;
    InputSourceState source = InputSourceState::NOT_COMMANDED;
    uint64_t onUs = 0, offUs = 0;
    // Line isolation at the latest trigger attempt.
    bool decided = false, admitted = false;
    char decisionRow[3] = {};
    uint8_t decisionAddress = 0;
    const char* decisionReason = "none";
    uint8_t rowCount = 0;
    InputStartRow rows[INPUT_START_MEMBERS];
    // Capacity witnesses while sampling.
    uint32_t debugOverwrites = 0, aux1Overwrites = 0;
    uint64_t stopServiceDelayMaxUs = 0;
    uint8_t count = 0;
    InputStartMemberView members[INPUT_START_MEMBERS];
};
/** Console form of an arm: `absolute-by` sets each target to the fresh position
 * plus value; the application builds the per-member requests. */
enum class InputStartArmForm : uint8_t { RELATIVE, ABSOLUTE, ABSOLUTE_BY };
enum class InputStartCommandKind : uint8_t { INSPECT, ARM, TRIGGER, DISARM };
struct InputStartCommand {
    InputStartCommandKind kind = InputStartCommandKind::INSPECT;
    uint32_t operationId = 0; ///< TRIGGER, DISARM, INSPECT.
    InputStartKind armKind = InputStartKind::POSITION;
    InputStartArmForm form = InputStartArmForm::RELATIVE;
    uint8_t terminal = 0;
    int64_t value = 0;
    uint16_t speedRpm = 0;
    bool group = false;
    uint32_t pollMs = INPUT_START_DEFAULT_POLL_MS, windowMs = INPUT_START_DEFAULT_WINDOW_MS;
    uint8_t output = 0;
    uint32_t widthMs = 0;
};
/** Result of one call, separate from the session view (which a refusal leaves
 * unchanged). reason names the refusal; address the deciding drive, or 0. */
struct InputStartReply {
    uint32_t operationId = 0;
    const char* reason = "none";
    uint8_t address = 0;
};
inline const char* inputStartPhaseName(InputStartPhase phase) {
    switch (phase) {
    case InputStartPhase::NONE: return "none";
    case InputStartPhase::STAGING: return "staging";
    case InputStartPhase::ARMED: return "armed";
    case InputStartPhase::TRIGGERING: return "triggering";
    case InputStartPhase::OBSERVING: return "observing";
    case InputStartPhase::DONE: return "done";
    }
    return "unknown";
}
inline const char* inputSourceName(InputSourceState state) {
    switch (state) {
    case InputSourceState::NOT_COMMANDED: return "not_commanded";
    case InputSourceState::ACTIVE: return "active";
    case InputSourceState::HOLD_ELAPSED: return "hold_elapsed";
    case InputSourceState::QUIESCED_BY_STOP: return "quiesced_by_stop";
    case InputSourceState::QUIESCED_BY_DISARM: return "quiesced_by_disarm";
    case InputSourceState::QUIESCED_BY_CANCEL: return "quiesced_by_cancel";
    case InputSourceState::OFF_REQUESTED: return "off_requested";
    case InputSourceState::PORT_FAILED: return "port_failed";
    }
    return "unknown";
}
inline const char* inputStartKindName(InputStartKind kind) {
    switch (kind) {
    case InputStartKind::POSITION: return "position";
    case InputStartKind::HOME: return "home";
    case InputStartKind::SEGMENT: return "segment";
    case InputStartKind::VELOCITY: return "velocity";
    }
    return "unknown";
}
inline const char* inputStartOutcomeName(MotorControlRS::ESS_RS::InputStartOutcome outcome) {
    using O = MotorControlRS::ESS_RS::InputStartOutcome;
    switch (outcome) {
    case O::NONE: return "none";
    case O::ATTRIBUTED: return "attributed";
    case O::INDETERMINATE: return "indeterminate";
    case O::NO_START: return "no_start";
    case O::UNATTRIBUTED_MOTION: return "unattributed_motion";
    case O::SECOND_START: return "second_start";
    case O::STOPPED: return "stopped";
    case O::DRIVE_FAULT: return "drive_fault";
    case O::TRANSPORT_ERROR: return "transport_error";
    case O::DEADLINE: return "deadline";
    case O::CANCELLED: return "cancelled";
    }
    return "unknown";
}
inline const char* inputStartEffectName(MotorControlRS::ESS_RS::InputStartEffect effect) {
    using E = MotorControlRS::ESS_RS::InputStartEffect;
    switch (effect) {
    case E::DISPLACEMENT: return "displacement";
    case E::ENDPOINT: return "endpoint";
    case E::HOME_COMPLETION: return "home_completion";
    case E::VELOCITY_RUN: return "velocity_run";
    }
    return "unknown";
}
inline const char* inputStartCompletionName(MotorControlRS::ESS_RS::InputStartCompletion completion) {
    using C = MotorControlRS::ESS_RS::InputStartCompletion;
    switch (completion) {
    case C::NOT_OBSERVED: return "not_observed";
    case C::FINITE_STOPPED: return "finite_stopped";
    case C::HOMED: return "homed";
    case C::VELOCITY_STOPPED: return "velocity_stopped";
    }
    return "unknown";
}
inline const char* inputStartFurtherName(MotorControlRS::ESS_RS::InputStartFurther further) {
    using F = MotorControlRS::ESS_RS::InputStartFurther;
    switch (further) {
    case F::NOT_EVALUATED: return "not_evaluated";
    case F::NONE_SEEN: return "none_seen";
    case F::SEEN: return "seen";
    case F::NOT_EXCLUDED: return "not_excluded";
    }
    return "unknown";
}
/// Names of the missing-witness bits, in bit order.
constexpr const char* INPUT_START_MISSING_NAMES[] = {
    "pre_edge_sample", "sample_lost", "spacing", "overwrite", "serial_start", "isolation", "generation",
    "source_edge", "change_after_edge", "input_before_edge", "running_not_sampled"};

}} // namespace MotorControlRSExample::Probe
