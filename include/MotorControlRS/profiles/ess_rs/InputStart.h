/** @file InputStart.h
 * @brief Bounded observation of motion that a drive input starts. No I/O, clock or heap.
 * SPDX-License-Identifier: MIT
 *
 * A host arms one context per drive after it has staged the motion the input
 * will start. The application owns the trigger source (for example a bench
 * output), the state reads and every write; this context only judges whether
 * the samples it is given attribute a change to one noted source edge. It never
 * yields a write, a retrigger or a retry. Wired bench 08.
 *
 * Evidence levels stay separate: a noted source edge is a controller time of the
 * source command, not proof that the drive input switched; a sample is a drive
 * report bracketed by its request start and reply end. A change is attributed
 * only when every witness holds; otherwise the outcome names what is missing.
 */
#pragma once
#include "MotorControlRS/ActionOperation.h"
#include "MotorControlRS/ReadOperation.h"
#include "MotorControlRS/profiles/ess_rs/Codec.h"
#include "MotorControlRS/profiles/ess_rs/DriverSettings.h"

namespace MotorControlRS { namespace ESS_RS {

/// One FC03 0x0006/7 sample: alarm, motion, inputs, outputs, position pair, speed.
constexpr uint16_t INPUT_START_SAMPLE_WORDS = 7;
constexpr uint32_t INPUT_START_MIN_POLL_US = 20000, INPUT_START_MAX_POLL_US = 500000;
constexpr uint32_t INPUT_START_MIN_WINDOW_US = 100000, INPUT_START_MAX_WINDOW_US = 30000000;
/// Default observation after the source falling edge before "no further start".
constexpr uint32_t INPUT_START_POST_RELEASE_US = 2000000;
constexpr uint8_t INPUT_START_POST_RELEASE_SAMPLES = 4; ///< Minimum samples after the falling edge.
constexpr std::size_t INPUT_START_SPACING_RING = 64;    ///< Retained spacings for the median.

/** What the staged input function does when it starts (function manual p27, p75).
 * DISPLACEMENT: function 7 or 12 with 0x0050 = 0 (relative); ENDPOINT: 0x0050 = 1
 * (absolute); HOME_COMPLETION: function 11; VELOCITY_RUN: function 13 (0x0051
 * selects level or rising edge). A velocity run is never reported finite. */
enum class InputStartEffect : uint8_t { DISPLACEMENT, ENDPOINT, HOME_COMPLETION, VELOCITY_RUN };
enum class InputStartOutcome : uint8_t {
    NONE,
    ATTRIBUTED,          ///< A change after the noted edge with every witness held.
    INDETERMINATE,       ///< A witness is missing (see missing); never counts as passed.
    NO_START,            ///< No change through the hold, the release and the post-release window.
    UNATTRIBUTED_MOTION, ///< Change before the edge, or with the edge outside the armed window.
    SECOND_START,        ///< RUNNING reported again after the first completion.
    STOPPED,             ///< A stop was recorded; a stopped state and the post-release window followed.
    DRIVE_FAULT,         ///< A sample reported alarm, release or a soft limit.
    TRANSPORT_ERROR,     ///< The application could no longer sample the drive.
    DEADLINE,            ///< The armed window closed before the context could conclude.
    CANCELLED            ///< Invalidated by the application (settings write, rebinding, recovery).
};
/// Start of motion seen in a sample after the source edge (RUNNING or a position change).
enum class InputStartExecution : uint8_t { NOT_OBSERVED, OBSERVED };
/** Completion evidence, separate from execution. "Stopped" is the drive's own
 * standstill report: RUNNING clear and speed word 0 (a sample without RUNNING but
 * with speed is still moving; wired bench 09). FINITE_STOPPED: stopped with a
 * changed position (or RUNNING seen, then stopped). HOMED: stopped with the homed
 * bit. VELOCITY_STOPPED: a velocity run reported stopped; never a finite completion. */
enum class InputStartCompletion : uint8_t { NOT_OBSERVED, FINITE_STOPPED, HOMED, VELOCITY_STOPPED };
/** Whether a further start after the first completion is excluded. SEEN: RUNNING
 * was reported again. NOT_EXCLUDED: an endpoint already reached cannot show a
 * repeated start by position, or the position (or speed word) changed after the
 * completion without RUNNING; that is either the drive settling (measured after
 * stops, wired bench 09) or a start that completed between samples. */
enum class InputStartFurther : uint8_t { NOT_EVALUATED, NONE_SEEN, SEEN, NOT_EXCLUDED };
/// Witnesses a result lacks (bit mask). Any bit makes an attribution INDETERMINATE.
enum InputStartMissing : uint16_t {
    INPUT_START_MISSING_PRE_EDGE = 1u << 0,      ///< No pre-edge sample, or it did not match the baseline.
    INPUT_START_MISSING_SAMPLE = 1u << 1,        ///< A sample was lost after the pre-edge sample.
    INPUT_START_MISSING_SPACING = 1u << 2,       ///< Sample spacing above twice the poll interval.
    INPUT_START_MISSING_OVERWRITE = 1u << 3,     ///< Application ring overwrite (debug copy, input edges).
    INPUT_START_MISSING_SERIAL_START = 1u << 4,  ///< A serial start may have reached the drive.
    INPUT_START_MISSING_ISOLATION = 1u << 5,     ///< Line isolation was not shown at the ON.
    INPUT_START_MISSING_GENERATION = 1u << 6,    ///< Configuration generation changed.
    INPUT_START_MISSING_SOURCE_EDGE = 1u << 7,   ///< No single source ON inside the armed window.
    INPUT_START_MISSING_CHANGE_AFTER_EDGE = 1u << 8, ///< First changed sample was requested before the edge.
    INPUT_START_MISSING_INPUT_BEFORE_EDGE = 1u << 9, ///< The input read active before the source ON.
    INPUT_START_MISSING_RUNNING = 1u << 10       ///< Endpoint already reached and RUNNING never sampled.
};
enum class InputStartError : int32_t {
    NONE, INVALID_TARGET, INVALID_OPERATION, INVALID_TERMINAL, INVALID_FUNCTION,
    POSITION_MODE_UNKNOWN, PV_TRIGGER_MODE_UNKNOWN, MAPPING_UNKNOWN, SOURCE_NOT_INHIBITED,
    BASELINE_REQUIRED, NOT_STATIONARY, ALARM, RELEASED, INPUT_ACTIVE, STALE_FACTS,
    EXPECTED_REQUIRED, INVALID_OPTIONS, INVALID_STATE, WRONG_CORRELATION, INVALID_EVENT,
    CLOCK_ERROR, SECOND_SOURCE_ON
};

/** Facts the application established before arming, for one drive. The caller
 * states sourceInhibitedUntilArmed: its trigger source stayed inactive for the
 * whole configuration and staging sequence (a host flag does not inhibit an
 * input). baselineWords are one checked 0x0006/7 report (or the same words from
 * the three state blocks) whose request started at baselineUs. */
struct InputStartPrerequisites {
    ReadTarget target;
    uint32_t configurationGeneration = 0;
    uint8_t terminal = 0;   ///< X0..X3.
    uint16_t function = 0;  ///< 0x0041 + terminal: 7, 11, 12 or 13.
    uint16_t polarity = 0;  ///< That terminal's bit of 0x0040.
    IoStatusMapping statusMapping = IoStatusMapping::UNKNOWN; ///< Of 0x0008 against the inversion.
    bool positionModeKnown = false;
    uint16_t positionMode = 0;  ///< 0x0050: 0 relative, 1 absolute (functions 7 and 12).
    bool pvTriggerModeKnown = false;
    uint16_t pvTriggerMode = 0; ///< 0x0051: 0 level, 1 rising edge (function 13).
    bool sourceInhibitedUntilArmed = false;
    bool wordOrderKnown = false;
    WordOrder wordOrder = WordOrder::HIGH_WORD_FIRST;
    bool baselineKnown = false;
    uint16_t baselineWords[INPUT_START_SAMPLE_WORDS] = {};
    uint64_t baselineUs = 0;
    /** DISPLACEMENT: expectedDisplacement in native increments; ENDPOINT:
     * expectedEndpoint. Reported against the position change, never judged. */
    bool expectedKnown = false;
    int64_t expectedDisplacement = 0;
    int32_t expectedEndpoint = 0;
    bool stagedKnown = false;
    /** Checked words the start relies on (evidence only). Function 7: the
     * 0x0021/5 profile readback. Function 12: the armed PT record read in
     * register order of its first five words, pulse words first (0x0060 +
     * 6(n-1): pulse high, pulse low, speed, acceleration, deceleration), so
     * index 0..1 is the target at word order 0. */
    uint16_t stagedWords[5] = {};
    uint64_t maximumAgeUs = 0;    ///< Zero disables age expiry of the baseline.
};
struct InputStartOptions {
    uint32_t pollIntervalUs = 50000; ///< 20..500 ms.
    uint32_t windowUs = 10000000;    ///< Armed window from preparation, 100 ms..30 s.
    uint32_t postReleaseUs = INPUT_START_POST_RELEASE_US;
    uint8_t postReleaseSamples = INPUT_START_POST_RELEASE_SAMPLES; ///< At least 4.
};
/** One consumed sample. requestUs is the request start (or step eligibility);
 * replyEndUs the latest reply closure. Their order with a source edge is a
 * bracket, not an exact drive time. */
struct InputStartSample {
    uint32_t sequence = 0;
    uint64_t requestUs = 0, replyEndUs = 0;
    uint16_t words[INPUT_START_SAMPLE_WORDS] = {};
    bool preEdge = false;
};
enum class InputStartEventKind : uint8_t {
    SAMPLE,    ///< Checked 0x0006/7 reply (transport FRAME); preEdge marks the trigger step's read.
    LOST,      ///< A requested sample failed (timeout, transport error, cancelled).
    ISOLATION, ///< The trigger step's line-isolation decision, before the ON.
    STOP,      ///< A stop request was recorded (stopId).
    DEGRADED,  ///< Application witnesses lost (missing bits): overwrite, serial start, generation.
    FAULT,     ///< The application can no longer sample (transport recovery required).
    CANCEL,    ///< Invalidation; ends CANCELLED.
    TIME       ///< Evaluate time-based conclusions (post-release window, armed window).
};
struct InputStartEvent {
    InputStartEventKind kind = InputStartEventKind::TIME;
    uint32_t operationId = 0, sequence = 0; ///< SAMPLE/LOST: the yielded sequence; 0 for a pre-edge sample.
    ReadEvent transport;
    uint64_t requestUs = 0;
    bool preEdge = false, held = false;
    uint32_t stopId = 0;
    uint16_t missing = 0;
};
enum class InputStartWork : uint8_t { DONE, WAIT, SAMPLE, CONCLUDE };
/** SAMPLE: one FC03 0x0006/7 request; the same sequence repeats until consumed.
 * CONCLUDE: deliver a TIME event now. WAIT: nothing is due before eligibleUs. */
struct PreparedInputStart {
    InputStartWork kind = InputStartWork::DONE;
    ReadTarget target;
    uint32_t operationId = 0, sequence = 0;
    uint8_t bytes[READ_REQUEST_LEN] = {};
    std::size_t length = 0;
    uint64_t eligibleUs = 0, deadlineUs = 0;
};
/** Caller-owned bounded storage; treat fields as read-only between calls. */
struct InputStartContext {
    ReadTarget target;
    uint32_t operationId = 0, configurationGeneration = 0;
    InputStartPrerequisites prerequisites;
    InputStartOptions options;
    InputStartEffect effect = InputStartEffect::DISPLACEMENT;
    ActionState state = ActionState::EMPTY;
    InputStartOutcome outcome = InputStartOutcome::NONE;
    Status status;
    uint64_t armedUs = 0, windowEndUs = 0, nextSampleUs = 0, finishedUs = 0;
    uint32_t nextSequence = 1;
    // Source edges as the application noted them (controller-timestamped).
    uint8_t sourceOns = 0;
    bool sourceActive = false, sourceOutsideWindow = false, isolationKnown = false, isolationHeld = false;
    uint64_t onUs = 0, offUs = 0;
    // Samples.
    uint32_t samples = 0, lostSamples = 0, lateSamples = 0, samplesAfterRelease = 0;
    bool lastKnown = false, preEdgeKnown = false, preEdgeMatched = false;
    InputStartSample last, preEdge;
    uint64_t unchangedRequestUs = 0; ///< Request start of the latest sample still showing the baseline.
    int32_t baselinePosition = 0, lastPosition = 0, completedPosition = 0;
    bool inputObserved = false;      ///< The input read active in a sample after the ON.
    uint64_t inputObservedUs = 0;
    bool changeObserved = false, changeAfterEdge = false, runningObserved = false;
    uint64_t changeStartUs = 0, changeEndUs = 0; ///< First-change bracket.
    bool completed = false, homedObserved = false;
    uint64_t completedUs = 0;
    bool furtherStart = false;
    uint64_t furtherStartUs = 0;
    bool settledAfterCompletion = false; ///< Position or speed changed after the completion without RUNNING.
    uint64_t settledUs = 0;
    bool stopRequested = false, stoppedAfterStop = false;
    uint32_t stopId = 0;
    uint64_t stopUs = 0;
    uint16_t missing = 0;
    // Achieved spacing between consumed samples from the pre-edge sample on.
    uint64_t spacingFromUs = 0;
    uint32_t spacingMinUs = 0, spacingMaxUs = 0, spacingCount = 0;
    uint32_t spacingUs[INPUT_START_SPACING_RING] = {};
};
/** Summary for reports. Execution and completion are separate fields. */
struct InputStartResult {
    InputStartEffect effect = InputStartEffect::DISPLACEMENT;
    ActionState state = ActionState::EMPTY;
    InputStartOutcome outcome = InputStartOutcome::NONE;
    InputStartExecution execution = InputStartExecution::NOT_OBSERVED;
    InputStartCompletion completion = InputStartCompletion::NOT_OBSERVED;
    InputStartFurther further = InputStartFurther::NOT_EVALUATED;
    uint16_t missing = 0;
    bool sourceCommanded = false, sourceActive = false, inputObserved = false;
    uint64_t onUs = 0, offUs = 0, changeStartUs = 0, changeEndUs = 0;
    bool changeObserved = false, runningObserved = false, stopRequested = false, stoppedAfterStop = false;
    uint32_t stopId = 0;
    bool positionKnown = false, expectedKnown = false, positionMatches = false;
    int64_t positionChange = 0, expectedChange = 0;
    /** Bracket of the latest consumed sample (positionChange is from it): its
     * request start and reply end. A host path guard compares the position
     * with an expected path at these controller times (wired bench 13). */
    uint64_t lastRequestUs = 0, lastReplyEndUs = 0;
    uint32_t samples = 0, lostSamples = 0, lateSamples = 0;
    uint32_t spacingMinUs = 0, spacingMedianUs = 0, spacingMaxUs = 0, spacingCount = 0;
    Status status;
};

/** Validate the facts and arm. Output is unchanged on any error. The effect
 * follows the function and the stated mode: 7/12 need a known 0x0050 (0 or 1),
 * 13 a known 0x0051. An inverted polarity with UNKNOWN mapping, an active input,
 * a running, alarmed or released baseline or speed, stale facts, or a missing
 * source inhibition statement refuse. The first sample is due at nowUs. */
Status prepareInputStart(InputStartContext& output, const InputStartPrerequisites&, uint32_t operationId,
                         const InputStartOptions&, uint64_t nowUs) noexcept;
/** Note one application-commanded source edge (ON active=true, OFF false) at its
 * controller time. A second ON is refused with the context unchanged; an ON
 * outside the armed window is kept and makes any later change unattributed. */
Status noteInputStartSource(InputStartContext&, uint64_t atUs, bool active) noexcept;
/** Next work; repeated calls yield the same sample until it is consumed. Never a write. */
Status nextInputStart(const InputStartContext&, uint64_t nowUs, PreparedInputStart& output) noexcept;
/** Consume one correlated event. Malformed or wrong-sequence events leave the
 * context unchanged. A lost sample, a timeout or a transport error never yields
 * another trigger or write. */
Status advanceInputStart(InputStartContext&, const InputStartEvent&, uint64_t nowUs) noexcept;
/** Copy the summary (with the spacing median). EMPTY context leaves output unchanged. */
Status getInputStart(const InputStartContext&, InputStartResult& output) noexcept;
/** Effective level of one terminal in a 0x0008 report under the stated mapping;
 * false when it cannot be known (UNKNOWN mapping with inverted polarity). */
bool inputStartLevel(uint16_t rawInputs, uint8_t terminal, uint16_t polarity, IoStatusMapping, bool& active) noexcept;

}} // namespace MotorControlRS::ESS_RS
