// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/MoveOperation.h>
#include <MotorControlRS/profiles/ess_rs/InputStart.h>
#include <cstdint>

namespace MotorControlRSExample { namespace Probe {

/** Input-start route (wired bench 08): arm a host observation session, pulse one
 * owned bench output once, observe and attribute, disarm. Console `arm`/`disarm`,
 * the programmatic API (ProbeApp.h) and the Python harness call one route.
 * Function 7 (position start), 12 (PT record, wired bench 13), 8-10 (speed input
 * and JOG) and 13 (PV record, wired bench 14) are delivered; home (11) staging
 * refuses before any traffic. PT, velocity and PV arms write nothing: the record
 * or the JOG words are stored beforehand through their setters and selectors are
 * held by their own bench outputs. */
constexpr uint8_t INPUT_START_MEMBERS = 4; ///< Drives on one fixture line (FIXTURE_WIRING_CAPACITY).
/// Shortest trigger pulse. Wired bench 09 (A3, drive 4, input filter 2): 10 ms started
/// every trial (10..200 ms, 3/3 each), 5 ms none; 20 ms keeps a 10 ms margin.
constexpr uint32_t INPUT_START_MIN_WIDTH_MS = 20;
constexpr uint32_t INPUT_START_MAX_WIDTH_MS = 5000; ///< Fixed maximum, enough for 09's held level.
constexpr uint32_t INPUT_START_MIN_POLL_MS = 20, INPUT_START_MAX_POLL_MS = 500, INPUT_START_DEFAULT_POLL_MS = 50;
constexpr uint32_t INPUT_START_MIN_WINDOW_MS = 100, INPUT_START_MAX_WINDOW_MS = 30000, INPUT_START_DEFAULT_WINDOW_MS = 10000;
/// D14 backstop: every interlock read is at most this old at the ON.
constexpr uint32_t INPUT_START_MAX_READ_AGE_MS = 1000;
/** PT records a four-input drive can select: one terminal is the trigger, three
 * carry PIN0-PIN2, so record = PIN2*4 + PIN1*2 + PIN0 + 1 reaches 1..8 (function
 * manual p22-23 table and notice). Records 9-16 need PIN3 and refuse. */
constexpr uint8_t INPUT_START_PT_SELECTABLE = 8;
constexpr uint8_t INPUT_START_PT_SELECTORS = 3; ///< PIN0..PIN2, functions 14..16.
/** A PT trigger needs every PIN output the armed record holds on to stay on for
 * the trigger pulse plus this margin: the p23 rule (PIN levels valid 5 ms after
 * the trigger) plus the input delay up to 32.4 ms at filter 2 (wired bench 05)
 * and relay bounce, rounded up as the harness settle (prompt 13 Implement 5). */
constexpr uint32_t INPUT_START_PT_PIN_HOLD_MARGIN_MS = 50;
/** Stored PT target claim promoted by wired bench 13's W0 witness: word order 0
 * (0x0019 = 0, high word at the lower address) and non-negative targets. A
 * negative target (W0b sign witness not run) refuses the arm. */
constexpr bool INPUT_START_PT_SIGNED_TARGETS = false;

/** VELOCITY: functions 8, 9, 10 on the trigger terminal (stored 0x001D/3 words);
 * PV: function 13 with a named selector layout (stored speed record). */
enum class InputStartKind : uint8_t { POSITION, HOME, SEGMENT, VELOCITY, PV };
/** Selector layouts of a PV arm (wired bench 14); each refuses any other
 * assignment it reads on the drive.
 * PIN3: the other three terminals hold PIN0..PIN2 (14-16) once each, records 1-8
 * (prompt 13's layout). PV2_DEADMAN: X0 = 13, X1 = 14, X2 = 15, X3 = 5 with input
 * polarity 1 (a dead-man stop held inactive by its bench output), records 1-4.
 * PV_PT_MIX: X0 = 13, X1 = 14, X2 = 15, X3 = 12 (one PT trigger to switch the
 * run to fixed length, p29 method 2), records 1-4. */
enum class InputStartLayout : uint8_t { NONE, PIN3, PV2_DEADMAN, PV_PT_MIX };
/// Records each PV layout reaches with its selectors.
constexpr uint8_t INPUT_START_PV2_SELECTABLE = 4;
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
    MotorControlRS::MoveRequest move; ///< POSITION: native steps; relative, or absolute target. Unused for SEGMENT.
};
/** Programmatic arm request. The selected axis: group false, one member with
 * the selected address. A group: every declared member, in any order. */
struct InputStartRequest {
    InputStartKind kind = InputStartKind::POSITION;
    uint8_t terminal = 0; ///< X0..X3.
    InputStartMode mode = InputStartMode::RELATIVE;
    bool group = false;
    uint8_t record = 0; ///< SEGMENT: the PT record 1..16 every member selects (one shared selector state). PV: the speed record.
    InputStartLayout layout = InputStartLayout::NONE; ///< PV only.
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
    /** SEGMENT: the armed record, its checked read (speed, acceleration,
     * deceleration, pulse words in register order), the read's operation, the
     * decoded target and the terminal of PIN0..PIN2. expectedEndpoint is the
     * target (absolute) or the baseline plus the pulses (relative). */
    uint8_t record = 0;
    uint16_t recordWords[5] = {};
    uint32_t recordOperationId = 0;
    int32_t recordTarget = 0;
    uint8_t selectorTerminals[INPUT_START_PT_SELECTORS] = {};
    /** VELOCITY and PV (wired bench 14): the selector count (PV), the layout, the
     * stored speed the run uses (0x001D for 8-10, the record's speed word for PV)
     * and the direction the function gives it: +1, -1 (9 forces +, 10 forces -,
     * 8 and 13 follow the sign). stagedWords hold the checked 0x001D/3 read for
     * VELOCITY; recordWords the record's speed, acceleration, deceleration for PV. */
    uint8_t selectorCount = 0;
    InputStartLayout layout = InputStartLayout::NONE;
    int16_t storedRpm = 0;
    int8_t expectedDirection = 0;
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
    // Capacity witnesses while sampling: debug-copy records overwritten before
    // the reader copied them, and aux1 edges recorded since arming that the
    // never-drained 32-edge ring no longer retains (not its wrap-around count).
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
    uint8_t record = 0; ///< SEGMENT arm: PT record 1..16; PV arm: speed record 1..8.
    InputStartLayout layout = InputStartLayout::NONE; ///< PV arm.
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
    case InputStartKind::PV: return "pv";
    }
    return "unknown";
}
inline const char* inputStartLayoutName(InputStartLayout layout) {
    switch (layout) {
    case InputStartLayout::NONE: return "none";
    case InputStartLayout::PIN3: return "pin3";
    case InputStartLayout::PV2_DEADMAN: return "pv2-deadman";
    case InputStartLayout::PV_PT_MIX: return "pv-pt-mix";
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
