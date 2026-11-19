/** @file Homing.h
 * @brief Bounded ESS limit, origin, index and current-position homing. No I/O or retry.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "MotorControlRS/Axis.h"
#include "MotorControlRS/profiles/ess_rs/Actions.h"

namespace MotorControlRS { namespace ESS_RS {
constexpr uint8_t HOME_METHOD_COUNT = 35;
constexpr std::size_t HOME_REQUEST_BYTES = 21;
enum class HomeSupport : uint8_t { IMPLEMENTED, UNIMPLEMENTED, UNRESOLVED };
enum class HomeInput : uint8_t { ORIGIN = 1, POSITIVE_LIMIT = 2, NEGATIVE_LIMIT = 4 };
/** Static descriptors distinguish a documented number from implemented work.
 * Inputs describe reviewed trajectory dependencies, not assignment setters.
 * Origin methods 3..6 and 19..22 require function 1; only 3..6 require index.
 * Methods 7..10 and 23..26 require both origin/function 1 and positive-limit/
 * function 2 for every starting location; only 7..10 require index.
 * Methods 11..14 and 27..30 require both origin/function 1 and negative-limit/
 * function 3 for every starting location; only 11..14 require index.
 * Index is internal motor Z; methods 33/34 need no external input terminal.
 * With checked inputs, every method except 35 fails on an active assigned limit
 * outside requiredInputs; an unrequired origin is never a fault. On every method
 * release/stop/emergency-stop functions 4..6 fail while active; motion triggers
 * 7..13 must be declared unconnected and inactive; selectors 14..17 are ignored. */
struct HomeMethodDescriptor {
    HomingMethod method;
    HomeSupport support;
    uint8_t requiredInputs;
    bool requiresIndex, moves;
    const char* reason;
};
const HomeMethodDescriptor* homeMethod(HomingMethod) noexcept;
const HomeMethodDescriptor* homeMethodAt(uint8_t index) noexcept;
/** End coordinate that auxiliary choice 0x0030 sets (function manual p72):
 * the offset (0, 6), zero (1, 7), the negated offset (2, 8) or the drive's own
 * coordinate (3-5, 9-11: actual, actual plus or minus offset), which the home
 * publishes as observed. Choices 0-5 also run the offset motion. */
enum class HomeEndRule : uint8_t { OFFSET, ZERO, NEGATIVE_OFFSET, OBSERVED };
HomeEndRule homeEndRule(HomingAuxiliary) noexcept;
enum class HomeError : int32_t {
    INVALID_TARGET, INVALID_OPERATION, INVALID_DEADLINE, INVALID_OPTIONS,
    INVALID_METHOD, METHOD_UNIMPLEMENTED, METHOD_UNRESOLVED, STALE_CONFIGURATION,
    INVALID_PARAMETERS, UNRESOLVED_RATES, UNRESOLVED_RAMP, UNRESOLVED_OFFSET,
    AUXILIARY_REQUIRED, INPUT_REQUIRED, INDEX_REQUIRED, READINESS,
    INVALID_STATE, WRONG_CORRELATION, INVALID_EVENT, CLOCK_ERROR,
    DEADLINE_EXPIRED, TRANSPORT_FAILURE, CANCELLED, TIMING_UNQUALIFIED,
    UNCONFIRMED_RESPONSE, DRIVE_FAULT, OBSERVATION_LIMIT, ZERO_NOT_OBSERVED,
    NOT_COMPLETE, REFERENCE_UNQUALIFIED, STALE_REFERENCE, UNEXPECTED_ACTIVITY,
    QUALIFICATION_MISMATCH, QUALIFICATION_REVOKED
};
/** Native qualified parameter words; no physical-rate/ramp formula is inferred.
 * Method 35 establishes the current mechanical position as origin and performs
 * no sensor-search trajectory. offset is a native signed value within
 * +-0x0FFFFFFF. A nonzero offset needs the prerequisites' offset qualification
 * and is staged in the 0x0031/6 window with encodeInt32 in the qualified active
 * word order (wired bench 10 measured order 0 on 0x4EEA/0x0029). Method 35
 * admits it with auxiliary 6 only and keeps auxiliary 7 at zero; moving methods
 * admit it with every auxiliary choice (wired bench 12). */
struct HomeRequest {
    HomingMethod method = HomingMethod::METHOD_35;
    uint32_t configurationGeneration = 0;
    uint16_t searchSpeed = 60, returnSpeed = 30, rampTime = 100;
    int64_t offset = 0;
};
/** Exact immutable target/configuration qualification. methodQualified covers
 * direction, trajectory, mechanics and expected completion transitions for
 * qualifiedMethod. Rate/ramp qualification applies only to the retained exact
 * native words; another request cannot reuse it by changing its parameters.
 * Inputs
 * must already have qualified assignments/levels; unwired is not disabled.
 * auxiliaryQualified means the existing active 0x0030 choice in auxiliary was
 * qualified, never merely a matching stored read. Method 35: 7 (zero offset
 * only) or, with offsetQualified, 6 (end coordinate = offset, no offset motion);
 * wired bench 10 E2 measured that 3 runs the offset (end = start + offset), so
 * 35 refuses every other choice. Moving methods admit 0-11 with zeroOffsetQualified
 * (zero offset) or offsetQualified; homeEndRule gives the end coordinate, and
 * choices other than 7 decode it in offsetOrder (the active 0x0019 of the same
 * read generation). offsetQualified states that the offset pair encoding at
 * offsetOrder and the chosen auxiliary's end coordinate are qualified for this
 * target; wired bench 10 measured the encoding on 0x4EEA/0x0029. The operation
 * never writes auxiliary or I/O settings.
 * referenceSemanticsQualified establishes the homing-zero/current-position
 * relation to actual command coordinates; raw zero alone cannot establish it. */
struct HomePrerequisites {
    ReadTarget target;
    uint32_t configurationGeneration = 0;
    bool methodQualified = false, nativeRatesQualified = false, nativeRampQualified = false;
    HomingMethod qualifiedMethod = HomingMethod::METHOD_35;
    uint16_t qualifiedSearchSpeed = 0, qualifiedReturnSpeed = 0, qualifiedRampTime = 0;
    bool zeroOffsetQualified = false, auxiliaryQualified = false;
    HomingAuxiliary auxiliary = HomingAuxiliary::KEEP_POSITION_SET_ZERO;
    bool offsetQualified = false;
    WordOrder offsetOrder = WordOrder::HIGH_WORD_FIRST;
    bool inputsQualified = false, indexQualified = false, readinessQualified = false;
    bool referenceSemanticsQualified = false;
    uint8_t availableInputs = 0;
    uint16_t rawAlarm = 0, rawMotion = 0;
    uint64_t observedUs = 0, maximumAgeUs = 0; ///< Zero disables age expiry, not evidence or operation deadlines.
    /** Supplied fixture facts have their own age; refreshing register reads does
     * not renew them. Zero maximum age disables expiry, not future-time checks. */
    uint64_t qualifiedUs = 0, qualificationMaximumAgeUs = 0;
    /** Checked logical-input observation, required for switch methods; optional
     * for 33/34/35. Standalone application homes always enable it and bind these
     * assignments to real configuration readback. INPUT_STATUS is already
     * logical; polarity must not be applied again. */
    bool observeInputs = false;
    uint16_t inputFunctions[4] = {}, inputPolarity = 0;
    InputWiring inputWiring[4] = {};
    uint16_t rawInputs = 0;
    uint64_t ioObservedUs = 0, readbackObservedUs = 0;
};
enum class HomePhase : uint8_t { STAGING, TRIGGER, OBSERVING, ZERO_CHECK };
/** Caller-owned state, read-only between calls. One axis reservation spans
 * staging/trigger/waits/zero check. Parameter application is not atomic. CANCEL
 * only ends local sequencing; application stop preemption settles in-flight
 * work and uses prepareStop. No timeout or cancelled search proves standstill. */
struct HomeContext {
    ReadTarget target;
    uint32_t operationId = 0;
    HomeRequest request;
    HomePrerequisites prerequisites;
    ActionOptions options;
    ActionState state = ActionState::EMPTY;
    ActionOutcome outcome = ActionOutcome::NONE;
    ActionExecution setupExecution = ActionExecution::NOT_TRANSMITTED;
    ActionExecution execution = ActionExecution::NOT_TRANSMITTED;
    ActionCompletion completion = ActionCompletion::NOT_OBSERVED;
    HomePhase phase = HomePhase::STAGING;
    bool stagingApplied = false, uncertain = false, runningObserved = false;
    bool homedLowObserved = false, observationKnown = false;
    bool qualificationRevoked = false, inputsObserved = false;
    uint16_t rawInputs = 0;
    uint16_t words[6] = {}, rawAlarm = 0, rawMotion = 0;
    uint16_t rawPositionWords[2] = {}; ///< Qualified confirmed on-time zero-check frame only; zeroEvidence retains any other.
    int32_t expectedPosition = 0; ///< End coordinate homeEndRule requires when expectedKnown (offset, 0 or -offset).
    int32_t endPosition = 0; ///< rawPositionWords decoded in offsetOrder when endKnown.
    bool endKnown = false;
    bool expectedKnown = true; ///< False for auxiliary 3-5 and 9-11: the observed end coordinate is published.
    uint64_t startedUs = 0, deadlineUs = 0, servicedUs = 0, eligibleUs = 0;
    uint64_t completionObservedUs = 0; ///< Eligibility of the completing status read, not reply closure/delivery.
    uint64_t lowObservedUs = 0; ///< First post-trigger low status read eligibility; baseline-low method35 uses prerequisites instead.
    uint8_t step = 0, polls = 0;
    ActionEvidence stagingEvidence, triggerEvidence, activityEvidence, lowEvidence;
    ActionEvidence lastObservation, completionEvidence, zeroEvidence, failureEvidence;
    Status status;
};
struct PreparedHome {
    ActionWork kind = ActionWork::DONE;
    ReadTarget target;
    uint32_t operationId = 0;
    uint8_t step = 0, function = 0;
    uint8_t bytes[HOME_REQUEST_BYTES] = {};
    std::size_t length = 0;
    uint16_t reg = 0, count = 0, value = 0;
    bool write = false;
    uint64_t deadlineUs = 0, eligibleUs = 0;
};
/** Check exact supplied qualification and, when enabled, checked logical input
 * mapping/levels. Documented future methods may be qualified without enabling
 * execution. This never supplies missing mechanical or index facts. */
Status checkHomeQualification(const HomePrerequisites&, uint64_t nowUs) noexcept;
/** Revoke active admission without discarding in-flight evidence. nextHome
 * yields no further work; advanceHome settles the current event as CANCELLED.
 * Terminal historical results are unchanged. This does not stop the drive. */
Status revokeHome(HomeContext&) noexcept;
/** Rejection leaves output unchanged and yields no traffic. Writes are capped
 * by the admitted readiness age. Reviewed staging is exactly 0x0031/6 with the
 * request's offset encoded in offsetOrder (zero with auxiliary 7); no subset,
 * split pair, negative mode or collision alias is used.
 * Unknown MOTION bits outside 0..6 cannot qualify admission or observation. */
Status prepareHome(HomeContext&, const AxisConfig&, uint32_t operationId,
                   const HomeRequest&, const HomePrerequisites&, uint64_t nowUs,
                   uint64_t deadlineUs, const ActionOptions& = ActionOptions()) noexcept;
Status nextHome(const HomeContext&, uint64_t nowUs, PreparedHome&) noexcept;
/** Checked start echo only acknowledges. Moving homes need new RUNNING and
 * post-trigger HOMED low then high with RUNNING clear. IN_POSITION is not required.
 * Method35 needs
 * HOMED low-to-high; its fresh admission baseline may supply low. A RUNNING
 * report during method35 fails with uncertain execution. Missed status
 * transitions remain unobserved (from a homed state the drive clears HOMED for
 * about 13 ms only, wired bench 06/10). Completion additionally needs a new
 * position pair equal to the expected end coordinate of homeEndRule (zero, the
 * offset or its negation), decoded in offsetOrder; for auxiliary 3-5 and 9-11 a
 * decodable pair, which is published. Required limit activation is expected during the drive's trajectory;
 * sampled inputs cannot prove each edge, reversal or internal Z passage. Missing
 * sampled input edges alone do not defeat qualified status/zero completion.
 * Unknown MOTION bits fail READINESS with raw evidence retained; known alarm/
 * fault reports take precedence. Unknown bits are not classified as drive faults.
 * A valid, on-time trigger echo with unconfirmed source or supplied execution
 * uncertainty permits bounded observation only. Satisfied transitions and zero
 * set completion OBSERVED, but execution stays UNKNOWN and the operation fails
 * UNCONFIRMED_RESPONSE with retained trigger evidence and no reference. Lost
 * trigger replies and other transport/reply failures end without further work.
 * Wrong envelopes leave state unchanged; evidence is copied, never retained
 * by pointer. Qualified final evidence may be delivered after its deadline. */
Status advanceHome(HomeContext&, const ActionEvent&, uint64_t nowUs) noexcept;
/** Acknowledged trigger, successful completion under the optional age policy and
 * qualified coordinate semantics publish the reported end coordinate (zero for
 * the ZERO end rule) as the native reference. Does not change host origin or generation;
 * the application reconciles coordinate effects before retaining this witness.
 * maximumAgeUs=0 disables expiry, not the required post-trigger observations. */
Status getHomeReference(const HomeContext&, uint64_t nowUs, uint64_t maximumAgeUs,
                        AxisReference&) noexcept;
}} // namespace MotorControlRS::ESS_RS

namespace MotorControlRS {
inline Status prepareHome(ESS_RS::HomeContext& c, const AxisConfig& axis, uint32_t id,
        const ESS_RS::HomeRequest& request, const ESS_RS::HomePrerequisites& prerequisites,
        uint64_t now, uint64_t deadline, const ActionOptions& options = ActionOptions()) noexcept {
    return ESS_RS::prepareHome(c, axis, id, request, prerequisites, now, deadline, options);
}
}
