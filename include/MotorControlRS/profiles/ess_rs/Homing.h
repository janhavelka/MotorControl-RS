/** @file Homing.h
 * @brief Bounded ESS index search and current-position homing. No I/O or retry.
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
 * Index is internal motor Z; methods 33/34 need no external input terminal. */
struct HomeMethodDescriptor {
    HomingMethod method;
    HomeSupport support;
    uint8_t requiredInputs;
    bool requiresIndex, moves;
    const char* reason;
};
const HomeMethodDescriptor* homeMethod(HomingMethod) noexcept;
const HomeMethodDescriptor* homeMethodAt(uint8_t index) noexcept;
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
 * Only offset zero is implemented. Method 35 establishes the current mechanical
 * position as origin and performs no sensor-search trajectory. */
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
 * auxiliaryQualified means existing active 0x0030=7 was qualified, never merely
 * a matching stored read. The operation never writes auxiliary or I/O settings.
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
    bool inputsQualified = false, indexQualified = false, readinessQualified = false;
    bool referenceSemanticsQualified = false;
    uint8_t availableInputs = 0;
    uint16_t rawAlarm = 0, rawMotion = 0;
    uint64_t observedUs = 0, maximumAgeUs = 0; ///< Zero disables age expiry, not evidence or operation deadlines.
    /** Supplied fixture facts have their own age; refreshing register reads does
     * not renew them. Zero maximum age disables expiry, not future-time checks. */
    uint64_t qualifiedUs = 0, qualificationMaximumAgeUs = 0;
    /** Optional checked logical-input observation. Standalone application homes
     * enable this and bind these assignments to real configuration readback. */
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
    uint16_t words[6] = {}, rawAlarm = 0, rawMotion = 0, rawPositionWords[2] = {};
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
 * by the admitted readiness age. Reviewed staging is exactly 0x0031/6 with
 * zero offset; no subset, split pair, negative mode or collision alias is used.
 * Unknown MOTION bits outside 0..6 cannot qualify admission or observation. */
Status prepareHome(HomeContext&, const AxisConfig&, uint32_t operationId,
                   const HomeRequest&, const HomePrerequisites&, uint64_t nowUs,
                   uint64_t deadlineUs, const ActionOptions& = ActionOptions()) noexcept;
Status nextHome(const HomeContext&, uint64_t nowUs, PreparedHome&) noexcept;
/** Checked start echo only acknowledges. Index searches need new RUNNING and
 * post-trigger HOMED low then high while stopped/in-position. Method35 needs
 * HOMED low-to-high; its fresh admission baseline may supply low. A RUNNING
 * report during nonmoving method35 fails with uncertain execution. Missed
 * transitions remain unobserved. Completion additionally needs new zero pair.
 * Unknown MOTION bits fail READINESS with raw evidence retained; known alarm/
 * fault reports take precedence. Unknown bits are not classified as drive faults.
 * Wrong envelopes leave state unchanged; evidence is copied, never retained
 * by pointer. Qualified final evidence may be delivered after its deadline. */
Status advanceHome(HomeContext&, const ActionEvent&, uint64_t nowUs) noexcept;
/** Successful completion under the optional age policy plus qualified coordinate semantics
 * publishes native-zero evidence. Does not change host origin or generation;
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
