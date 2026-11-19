// SPDX-License-Identifier: MIT
#include "MotorControlRS/profiles/ess_rs/Homing.h"
#include "MotorControlRS/profiles/ess_rs/Registers.h"
#include "ActionEvidence.h"

namespace MotorControlRS { namespace ESS_RS {
namespace {
constexpr uint16_t RUNNING = static_cast<uint16_t>(MotionStatusBit::RUNNING);
constexpr uint16_t HOMED = static_cast<uint16_t>(MotionStatusBit::HOMING_COMPLETE);
constexpr uint16_t UNKNOWN_MOTION = 0xFF80; // Only bits 0..6 are documented; unknown is not a drive fault.
static_assert(ACTION_MAX_POLLS + 2 < 256, "home setup/trigger/poll/zero step must fit uint8_t");
static_assert(ACTION_MAX_REPLY_BYTES >= 11, "combined home alarm/motion/input reply must fit evidence");
// Bits 5/6 are soft-limit overtravel flags (wired bench 11), not alarms. A home
// stays faulted on them: the application admits a home only with soft limits
// known disabled, so a flag during a home contradicts that admission.
constexpr uint16_t FAULTS = static_cast<uint16_t>(MotionStatusBit::ALARM) |
    static_cast<uint16_t>(MotionStatusBit::RELEASED) |
    static_cast<uint16_t>(MotionStatusBit::POSITIVE_SOFT_LIMIT) |
    static_cast<uint16_t>(MotionStatusBit::NEGATIVE_SOFT_LIMIT);
constexpr uint8_t LIMIT_ROLES = static_cast<uint8_t>(HomeInput::POSITIVE_LIMIT) |
    static_cast<uint8_t>(HomeInput::NEGATIVE_LIMIT);
constexpr const char* COLLISION_UNRESOLVED = "negative method encoding, collision trajectory and conflicting parameter mappings are unresolved";
// Explicit descriptors; numeric membership never enables an operation.
const HomeMethodDescriptor METHODS[HOME_METHOD_COUNT] = {
    {HomingMethod::COLLISION_MINUS_4, HomeSupport::UNRESOLVED, 0, false, true, COLLISION_UNRESOLVED},
    {HomingMethod::COLLISION_MINUS_3, HomeSupport::UNRESOLVED, 0, false, true, COLLISION_UNRESOLVED},
    {HomingMethod::COLLISION_MINUS_2, HomeSupport::UNRESOLVED, 0, false, true, COLLISION_UNRESOLVED},
    {HomingMethod::COLLISION_MINUS_1, HomeSupport::UNRESOLVED, 0, false, true, COLLISION_UNRESOLVED},
    {HomingMethod::METHOD_1, HomeSupport::IMPLEMENTED, 4, true, true, "negative-limit release then first internal Z in positive low-speed direction; qualified limit and index required"},
    {HomingMethod::METHOD_2, HomeSupport::IMPLEMENTED, 2, true, true, "positive-limit release then first internal Z in negative low-speed direction; qualified limit and index required"},
    {HomingMethod::METHOD_3, HomeSupport::IMPLEMENTED, 1, true, true, "origin falling then first internal Z in negative low-speed direction; qualified origin and index required"},
    {HomingMethod::METHOD_4, HomeSupport::IMPLEMENTED, 1, true, true, "origin rising then first internal Z in positive low-speed direction; qualified origin and index required"},
    {HomingMethod::METHOD_5, HomeSupport::IMPLEMENTED, 1, true, true, "origin falling then first internal Z in positive low-speed direction; qualified origin and index required"},
    {HomingMethod::METHOD_6, HomeSupport::IMPLEMENTED, 1, true, true, "origin rising then first internal Z in negative low-speed direction; qualified origin and index required"},
    {HomingMethod::METHOD_7, HomeSupport::IMPLEMENTED, 3, true, true, "origin falling then first internal Z in negative low-speed direction; qualified origin and positive limit and index required"},
    {HomingMethod::METHOD_8, HomeSupport::IMPLEMENTED, 3, true, true, "origin rising then first internal Z in positive low-speed direction; qualified origin and positive limit and index required"},
    {HomingMethod::METHOD_9, HomeSupport::IMPLEMENTED, 3, true, true, "origin rising then first internal Z in negative low-speed direction; qualified origin and positive limit and index required"},
    {HomingMethod::METHOD_10, HomeSupport::IMPLEMENTED, 3, true, true, "origin falling then first internal Z in positive low-speed direction; qualified origin and positive limit and index required"},
    {HomingMethod::METHOD_11, HomeSupport::IMPLEMENTED, 5, true, true, "origin falling then first internal Z in positive low-speed direction; qualified origin and negative limit and index required"},
    {HomingMethod::METHOD_12, HomeSupport::IMPLEMENTED, 5, true, true, "origin rising then first internal Z in negative low-speed direction; qualified origin and negative limit and index required"},
    {HomingMethod::METHOD_13, HomeSupport::IMPLEMENTED, 5, true, true, "origin rising then first internal Z in positive low-speed direction; qualified origin and negative limit and index required"},
    {HomingMethod::METHOD_14, HomeSupport::IMPLEMENTED, 5, true, true, "origin falling then first internal Z in negative low-speed direction; qualified origin and negative limit and index required"},
    {HomingMethod::METHOD_17, HomeSupport::IMPLEMENTED, 4, false, true, "negative-limit release in positive low-speed direction; qualified limit required, no index"},
    {HomingMethod::METHOD_18, HomeSupport::UNRESOLVED, 2, false, true, "heading, prose and trajectory say positive limit; negative-limit figure captions look like a caption error; sampled firmware input needs a switch fixture"},
    {HomingMethod::METHOD_19, HomeSupport::IMPLEMENTED, 1, false, true, "origin falling in negative low-speed direction; qualified origin required, no index"},
    {HomingMethod::METHOD_20, HomeSupport::IMPLEMENTED, 1, false, true, "origin rising in positive low-speed direction; qualified origin required, no index"},
    {HomingMethod::METHOD_21, HomeSupport::IMPLEMENTED, 1, false, true, "origin falling in positive low-speed direction; qualified origin required, no index"},
    {HomingMethod::METHOD_22, HomeSupport::IMPLEMENTED, 1, false, true, "origin rising in negative low-speed direction; qualified origin required, no index"},
    {HomingMethod::METHOD_23, HomeSupport::IMPLEMENTED, 3, false, true, "origin falling in negative low-speed direction; qualified origin and positive limit required, no index"},
    {HomingMethod::METHOD_24, HomeSupport::IMPLEMENTED, 3, false, true, "origin rising in positive low-speed direction; qualified origin and positive limit required, no index"},
    {HomingMethod::METHOD_25, HomeSupport::IMPLEMENTED, 3, false, true, "origin rising in negative low-speed direction; qualified origin and positive limit required, no index"},
    {HomingMethod::METHOD_26, HomeSupport::IMPLEMENTED, 3, false, true, "origin falling in positive low-speed direction; qualified origin and positive limit required, no index"},
    {HomingMethod::METHOD_27, HomeSupport::IMPLEMENTED, 5, false, true, "origin falling in positive low-speed direction; qualified origin and negative limit required, no index"},
    {HomingMethod::METHOD_28, HomeSupport::IMPLEMENTED, 5, false, true, "origin rising in negative low-speed direction; qualified origin and negative limit required, no index"},
    {HomingMethod::METHOD_29, HomeSupport::IMPLEMENTED, 5, false, true, "origin rising in positive low-speed direction; qualified origin and negative limit required, no index"},
    {HomingMethod::METHOD_30, HomeSupport::IMPLEMENTED, 5, false, true, "origin falling in negative low-speed direction; qualified origin and negative limit required, no index"},
    {HomingMethod::METHOD_33, HomeSupport::IMPLEMENTED, 0, true, true, "negative low-speed internal Z search; qualified mechanics and index required"},
    {HomingMethod::METHOD_34, HomeSupport::IMPLEMENTED, 0, true, true, "positive low-speed internal Z search; qualified mechanics and index required"},
    {HomingMethod::METHOD_35, HomeSupport::IMPLEMENTED, 0, false, false, "current mechanical position as origin; no search trajectory"}
};
Status invalid(HomeError reason, const char* message) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(reason), message);
}
Status failed(HomeError reason, const char* message) {
    return Status(Err::ILLEGAL_VALUE, static_cast<int32_t>(reason), message);
}
bool sameTarget(const ReadTarget& a, const ReadTarget& b) {
    return a.id == b.id && a.address == b.address && a.generation == b.generation;
}
constexpr int64_t OFFSET_LIMIT = 0x0FFFFFFF; // p73 range -0xFFFFFFF..0xFFFFFFF.
bool validOrder(WordOrder order) { return order == WordOrder::HIGH_WORD_FIRST || order == WordOrder::LOW_WORD_FIRST; }
// Auxiliary/offset admission. Method 35 keeps wired bench 10: 7 with a zero
// offset, 6 with the measured offset qualification (E1). 3 runs the offset
// (E2), which a nonmoving home cannot observe, so 35 refuses 0-5 and 8-11.
// Moving methods admit every documented choice 0-11 (wired bench 12); the
// expected end coordinate follows homeEndRule. Choices other than 7 decode the
// position pair, so they need a valid active word order.
Status offsetPolicy(const HomePrerequisites& p) {
    if (!p.auxiliaryQualified)
        return invalid(HomeError::AUXILIARY_REQUIRED, "existing active home auxiliary must be qualified");
    const uint16_t choice = static_cast<uint16_t>(p.auxiliary);
    if (choice > 11) return invalid(HomeError::AUXILIARY_REQUIRED, "home auxiliary outside the documented choices 0-11");
    const auto* method = homeMethod(p.qualifiedMethod);
    if (method && !method->moves) {
        if (p.auxiliary == HomingAuxiliary::KEEP_POSITION_SET_ZERO)
            return p.zeroOffsetQualified ? Ok() : invalid(HomeError::UNRESOLVED_OFFSET, "zero offset must be qualified");
        if (p.auxiliary != HomingAuxiliary::KEEP_POSITION_SET_OFFSET)
            return invalid(HomeError::AUXILIARY_REQUIRED, "method 35 admits auxiliary 7 or 6 only");
        if (!p.offsetQualified || !validOrder(p.offsetOrder))
            return invalid(HomeError::UNRESOLVED_OFFSET, "offset encoding and end coordinate must be qualified");
        return Ok();
    }
    if (!p.zeroOffsetQualified && !p.offsetQualified)
        return invalid(HomeError::UNRESOLVED_OFFSET, "zero offset or the offset encoding must be qualified");
    if (p.auxiliary != HomingAuxiliary::KEEP_POSITION_SET_ZERO && !validOrder(p.offsetOrder))
        return invalid(HomeError::UNRESOLVED_OFFSET, "auxiliary end coordinate needs a valid word order");
    return Ok();
}
Status inputReadiness(const HomePrerequisites& p, uint16_t inputs) {
    if (p.inputPolarity & ~uint16_t(15) || inputs & ~uint16_t(15))
        return invalid(HomeError::INPUT_REQUIRED, "unknown input polarity or logical level bits");
    const auto* method = homeMethod(p.qualifiedMethod);
    // Host policy rejects active limits outside the method's required inputs.
    // This does not prove the drive's direction-specific response. An unrelated
    // origin is allowed; method 35 has no moving search and permits sensor levels.
    const uint8_t forbidden = p.qualifiedMethod == HomingMethod::METHOD_35 ? 0 :
        uint8_t(LIMIT_ROLES & ~method->requiredInputs);
    uint8_t available = 0;
    for (uint8_t i = 0; i < 4; ++i) {
        const uint16_t function = p.inputFunctions[i];
        if (function > 17 || p.inputWiring[i] > InputWiring::CONNECTED)
            return invalid(HomeError::INPUT_REQUIRED, "unqualified input assignment or wiring");
        // INPUT_STATUS already represents logical validity. Physical limit
        // activation required by a trajectory is not a soft-limit fault.
        const bool active = inputs & (1u << i);
        if (function && function <= 3) {
            const uint8_t role = uint8_t(1u << (function - 1));
            if (p.inputWiring[i] == InputWiring::CONNECTED) available |= role;
            if (active && (forbidden & role))
                return invalid(HomeError::INPUT_REQUIRED, "active nonrequired limit rejected by home policy");
        } else if (function >= 4 && function <= 6) {
            // Reject active release/stop inputs without assuming their trajectory effect.
            if (active) return invalid(HomeError::INPUT_REQUIRED, "active release or stop input rejected by home policy");
        } else if (function >= 7 && function <= 13) {
            // Position, speed, jog, home and segment triggers start motion the host cannot
            // attribute, and an edge can fall between polls, so nothing may drive them.
            if (active || p.inputWiring[i] != InputWiring::UNCONNECTED)
                return invalid(HomeError::INPUT_REQUIRED, "motion trigger input is active or may be driven");
        } // 0 has no function; segment selectors 14..17 act only through a trigger.
    }
    if (available != p.availableInputs || (available & method->requiredInputs) != method->requiredInputs)
        return invalid(HomeError::INPUT_REQUIRED, "required input function and declared wiring do not match qualification");
    return Ok();
}
uint64_t stepDeadline(const HomeContext& c) {
    const auto& p = c.prerequisites;
    uint64_t end = c.deadlineUs;
    if (c.step < 2) {
        const uint64_t ends[] = {evidenceAgeDeadline(p.observedUs, p.maximumAgeUs),
            evidenceAgeDeadline(p.qualifiedUs, p.qualificationMaximumAgeUs),
            p.observeInputs ? evidenceAgeDeadline(p.ioObservedUs, p.maximumAgeUs) : UINT64_MAX,
            p.observeInputs ? evidenceAgeDeadline(p.readbackObservedUs, p.maximumAgeUs) : UINT64_MAX};
        for (auto ageEnd : ends) if (ageEnd < end) end = ageEnd;
    }
    return end;
}
Status expired(const HomeContext& c) {
    return stepDeadline(c) < c.deadlineUs ? failed(HomeError::READINESS, "home readiness expired before write completion") :
        failed(HomeError::DEADLINE_EXPIRED, "home deadline expired");
}
void finish(HomeContext& c, ActionOutcome outcome, Status status) {
    c.outcome = outcome; c.status = status;
    c.state = outcome == ActionOutcome::OBSERVED ? ActionState::SUCCEEDED : ActionState::FAILED;
    c.uncertain = c.state == ActionState::FAILED &&
        (c.stagingEvidence.txAccepted || c.stagingEvidence.executionUnknown ||
         c.triggerEvidence.txAccepted || c.triggerEvidence.executionUnknown);
}
void wait(HomeContext& c, uint64_t nowUs, bool runningEarlier = false) {
    ++c.step;
    const uint32_t interval = runningEarlier && c.options.runningPollIntervalUs ?
        c.options.runningPollIntervalUs : c.options.pollIntervalUs;
    c.eligibleUs = interval >= c.deadlineUs - nowUs ? c.deadlineUs : nowUs + interval;
}
} // namespace
HomeEndRule homeEndRule(HomingAuxiliary auxiliary) noexcept {
    const uint16_t choice = static_cast<uint16_t>(auxiliary);
    if (choice > 11) return HomeEndRule::OBSERVED;
    switch (choice % 6) {
    case 0: return HomeEndRule::OFFSET;
    case 1: return HomeEndRule::ZERO;
    case 2: return HomeEndRule::NEGATIVE_OFFSET;
    default: return HomeEndRule::OBSERVED;
    }
}
const HomeMethodDescriptor* homeMethodAt(uint8_t index) noexcept {
    return index < HOME_METHOD_COUNT ? METHODS + index : nullptr;
}
const HomeMethodDescriptor* homeMethod(HomingMethod method) noexcept {
    for (const auto& descriptor : METHODS) if (descriptor.method == method) return &descriptor;
    return nullptr;
}

Status checkHomeQualification(const HomePrerequisites& p, uint64_t nowUs) noexcept {
    if (!p.target.id || !p.target.generation || !isValidAddress(p.target.address) || !p.configurationGeneration)
        return invalid(HomeError::INVALID_TARGET, "invalid qualification binding");
    const auto* method = homeMethod(p.qualifiedMethod);
    if (!method) return invalid(HomeError::INVALID_METHOD, "undocumented home method");
    if (method->support == HomeSupport::UNRESOLVED)
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(HomeError::METHOD_UNRESOLVED), method->reason);
    const Status offset = offsetPolicy(p);
    if (!offset) return offset;
    if (p.qualifiedSearchSpeed < 5 || p.qualifiedSearchSpeed > 3000 || p.qualifiedReturnSpeed < 5 ||
        p.qualifiedReturnSpeed > 300 || p.qualifiedRampTime < 30 || p.qualifiedRampTime > 2000)
        return invalid(HomeError::INVALID_PARAMETERS, "qualified native words exceed admitted bounds");
    if (!p.nativeRatesQualified) return invalid(HomeError::UNRESOLVED_RATES, "native rates are unqualified");
    if (!p.nativeRampQualified) return invalid(HomeError::UNRESOLVED_RAMP, "native ramp is unqualified");
    if (!p.inputsQualified || (p.availableInputs & method->requiredInputs) != method->requiredInputs)
        return invalid(HomeError::INPUT_REQUIRED, "required input qualification is absent");
    if (method->requiredInputs && !p.observeInputs)
        return invalid(HomeError::INPUT_REQUIRED, "switch homing requires checked input mapping and logical levels");
    if (method->requiresIndex && !p.indexQualified) return invalid(HomeError::INDEX_REQUIRED, "index qualification is absent");
    if (!p.methodQualified || !p.readinessQualified || !evidenceAgeValid(p.qualifiedUs, nowUs, p.qualificationMaximumAgeUs))
        return invalid(HomeError::READINESS, "fixture qualification is absent or expired");
    return p.observeInputs ? inputReadiness(p, p.rawInputs) : Ok();
}

Status revokeHome(HomeContext& c) noexcept {
    if (c.state == ActionState::EMPTY) return invalid(HomeError::INVALID_STATE, "home is empty");
    if (c.state == ActionState::ACTIVE) c.qualificationRevoked = true;
    return Ok();
}

Status prepareHome(HomeContext& output, const AxisConfig& axis, uint32_t id,
        const HomeRequest& request, const HomePrerequisites& p, uint64_t nowUs,
        uint64_t deadlineUs, const ActionOptions& options) noexcept {
    if (!axis.target.id || !axis.target.generation || !isValidAddress(axis.target.address))
        return invalid(HomeError::INVALID_TARGET, "invalid home target");
    if (!id) return invalid(HomeError::INVALID_OPERATION, "zero home id");
    if (deadlineUs <= nowUs) return invalid(HomeError::INVALID_DEADLINE, "expired home deadline");
    if (!options.pollIntervalUs || !options.maxPolls || options.maxPolls > ACTION_MAX_POLLS ||
        (options.runningPollIntervalUs && options.runningPollIntervalUs < options.pollIntervalUs))
        return invalid(HomeError::INVALID_OPTIONS, "invalid bounded home observation policy");
    const auto* method = homeMethod(request.method);
    if (!method) return invalid(HomeError::INVALID_METHOD, "undocumented home method");
    if (method->support != HomeSupport::IMPLEMENTED)
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(method->support == HomeSupport::UNRESOLVED ?
            HomeError::METHOD_UNRESOLVED : HomeError::METHOD_UNIMPLEMENTED), method->reason);
    if (!axis.generation || !sameTarget(axis.target, p.target) || axis.generation != p.configurationGeneration ||
        request.configurationGeneration != axis.generation)
        return invalid(HomeError::STALE_CONFIGURATION, "home target/configuration binding mismatch");
    // A zero offset is order independent; a nonzero one needs the measured
    // encoding. Method 35 keeps wired bench 10's subset (7 zero, 6 offset).
    const bool zeroOffset = request.offset == 0;
    const bool still = !method->moves;
    if (request.offset < -OFFSET_LIMIT || request.offset > OFFSET_LIMIT ||
        (zeroOffset ? !(p.zeroOffsetQualified || p.offsetQualified) : !p.offsetQualified) ||
        (still && (p.auxiliary == HomingAuxiliary::KEEP_POSITION_SET_ZERO ? !zeroOffset || !p.zeroOffsetQualified :
                   !p.offsetQualified)))
        return Status(Err::UNSUPPORTED, static_cast<int32_t>(HomeError::UNRESOLVED_OFFSET),
            "nonzero offset needs the measured offset encoding; method 35 admits it with auxiliary 6 only");
    const Status offset = offsetPolicy(p);
    if (!offset) return offset;
    if (request.searchSpeed < 5 || request.searchSpeed > 3000 || request.returnSpeed < 5 || request.returnSpeed > 300 ||
        request.rampTime < 30 || request.rampTime > 2000)
        return invalid(HomeError::INVALID_PARAMETERS, "home parameters exceed the conservative admitted raw subset");
    if (request.method != p.qualifiedMethod || request.searchSpeed != p.qualifiedSearchSpeed ||
        request.returnSpeed != p.qualifiedReturnSpeed || request.rampTime != p.qualifiedRampTime)
        return invalid(HomeError::QUALIFICATION_MISMATCH, "home method and native words differ from the qualified request");
    const Status qualification = checkHomeQualification(p, nowUs);
    if (!qualification) return qualification;
    if (p.observeInputs && (!evidenceAgeValid(p.ioObservedUs, nowUs, p.maximumAgeUs) ||
        !evidenceAgeValid(p.readbackObservedUs, nowUs, p.maximumAgeUs)))
        return invalid(HomeError::READINESS, "input or configuration readback is stale");
    if (!evidenceAgeValid(p.observedUs, nowUs, p.maximumAgeUs) ||
        p.rawAlarm || (p.rawMotion & (FAULTS | RUNNING | UNKNOWN_MOTION)))
        return invalid(HomeError::READINESS, "fresh enabled stationary alarm-free qualified home readiness is required");
    HomeContext c;
    c.target = axis.target; c.operationId = id; c.request = request; c.prerequisites = p;
    c.options = options; c.state = ActionState::ACTIVE;
    c.startedUs = c.servicedUs = c.eligibleUs = nowUs; c.deadlineUs = deadlineUs;
    c.words[0] = static_cast<uint16_t>(request.method); c.words[1] = request.searchSpeed;
    c.words[2] = request.returnSpeed; c.words[3] = request.rampTime;
    // Zero in both words is order independent; a nonzero offset uses the
    // qualified active order (wired bench 10: high word first at order 0).
    if (zeroOffset) c.words[4] = c.words[5] = 0;
    else if (!encodeInt32(static_cast<int32_t>(request.offset), p.offsetOrder, c.words + 4, 2))
        return invalid(HomeError::UNRESOLVED_OFFSET, "invalid offset word order");
    // Function manual p72, 0x0030: offset, zero or -offset; 3-5 and 9-11 keep
    // the drive's coordinate, which is published as observed (wired bench 12).
    const HomeEndRule rule = homeEndRule(p.auxiliary);
    c.expectedKnown = rule != HomeEndRule::OBSERVED;
    c.expectedPosition = rule == HomeEndRule::OFFSET ? static_cast<int32_t>(request.offset) :
        rule == HomeEndRule::NEGATIVE_OFFSET ? static_cast<int32_t>(-request.offset) : 0;
    c.homedLowObserved = !method->moves && !(p.rawMotion & HOMED);
    const Status access = validateWriteMultipleRegistersRequest(c.target.address, Registers::HOMING_METHOD, c.words, 6);
    if (!access) return access;
    output = c; return Ok();
}

Status nextHome(const HomeContext& c, uint64_t nowUs, PreparedHome& output) noexcept {
    if (c.state == ActionState::EMPTY) return invalid(HomeError::INVALID_STATE, "home is empty");
    if (nowUs < c.servicedUs) return invalid(HomeError::CLOCK_ERROR, "home clock moved backwards");
    PreparedHome next;
    next.target = c.target; next.operationId = c.operationId; next.step = c.step;
    next.deadlineUs = stepDeadline(c); next.eligibleUs = c.eligibleUs;
    if (c.state != ActionState::ACTIVE) { output = next; return Ok(); }
    if (c.qualificationRevoked) return failed(HomeError::QUALIFICATION_REVOKED, "home admission revoked");
    if (nowUs >= next.deadlineUs) return expired(c);
    if (nowUs < c.eligibleUs) { next.kind = ActionWork::WAIT; output = next; return Ok(); }
    next.kind = ActionWork::TRANSACTION; next.write = c.step < 2;
    if (c.step == 0) {
        next.function = 16; next.reg = Registers::HOMING_METHOD; next.count = 6;
        next.length = buildWriteMultipleRegisters(c.target.address, next.reg, c.words, 6, next.bytes, sizeof(next.bytes));
    } else if (c.step == 1) {
        next.function = 6; next.reg = Registers::MOTION_COMMAND; next.value = 16; next.count = 1;
        next.length = buildWriteSingleRegister(c.target.address, next.reg, next.value, next.bytes, sizeof(next.bytes));
    } else {
        next.function = 3; next.reg = c.phase == HomePhase::ZERO_CHECK ? Registers::CURRENT_POSITION : Registers::ERROR_CODE;
        next.count = c.phase != HomePhase::ZERO_CHECK && c.prerequisites.observeInputs ? 3 : 2;
        next.length = buildReadRegisters(c.target.address, next.reg, next.count, next.bytes, sizeof(next.bytes));
    }
    if (!next.length) return invalid(HomeError::INVALID_STATE, "invalid home frame");
    output = next; return Ok();
}

Status advanceHome(HomeContext& c, const ActionEvent& supplied, uint64_t nowUs) noexcept {
    const auto& event = supplied.transport;
    if (c.state != ActionState::ACTIVE) return invalid(HomeError::INVALID_STATE, "home is not active");
    if (!sameTarget(c.target, event.target) || c.operationId != event.operationId || c.step != event.step)
        return invalid(HomeError::WRONG_CORRELATION, "home event correlation mismatch");
    if (nowUs < c.servicedUs) return invalid(HomeError::CLOCK_ERROR, "home clock moved backwards");
    const std::size_t expectedTx = c.step == 0 ? HOME_REQUEST_BYTES : READ_REQUEST_LEN;
    if (event.kind > ReadEventKind::DEADLINE || event.txAccepted > expectedTx ||
        (supplied.txComplete && event.txAccepted != expectedTx))
        return invalid(HomeError::INVALID_EVENT, "invalid home transport envelope");
    if (event.kind == ReadEventKind::FRAME) {
        if (!event.frame || event.txAccepted != expectedTx || !supplied.txComplete ||
            (event.qualified && (event.earliestUs < c.eligibleUs || event.earliestUs > event.latestUs || event.latestUs > nowUs)) ||
            (!event.qualified && (event.earliestUs || event.latestUs)))
            return invalid(HomeError::INVALID_EVENT, "invalid home frame envelope");
    } else if ((!event.frame && event.length) || event.qualified || event.earliestUs || event.latestUs ||
        supplied.responseConfirmed || (event.kind == ReadEventKind::DEADLINE && nowUs < stepDeadline(c)))
        return invalid(HomeError::INVALID_EVENT, "invalid local home event envelope");
    ActionEvidence evidence;
    Detail::copyActionEvidence(evidence, supplied, c.step, nowUs);
    uint16_t words[3] = {}; std::size_t count = 0;
    const uint16_t expectedWords = c.phase != HomePhase::ZERO_CHECK && c.prerequisites.observeInputs ? 3 : 2;
    if (event.kind == ReadEventKind::FRAME) {
        if (c.step == 0) evidence.status = parseWriteMultipleRegisters(event.frame, event.length, c.target.address,
            Registers::HOMING_METHOD, 6, &evidence.frameError);
        else if (c.step == 1) evidence.status = parseWriteSingleRegister(event.frame, event.length, c.target.address,
            Registers::MOTION_COMMAND, 16, &evidence.frameError);
        else evidence.status = parseRegisters(event.frame, event.length, c.target.address, expectedWords, words, 3, count, &evidence.frameError);
    } else if (event.kind == ReadEventKind::CANCEL) evidence.status = failed(HomeError::CANCELLED, "home locally cancelled");
    else if (event.kind == ReadEventKind::DEADLINE) evidence.status = expired(c);
    else evidence.status = failed(HomeError::TRANSPORT_FAILURE, "home transport failed");
    if (c.step == 0) {
        c.stagingEvidence = evidence; c.setupExecution = Detail::classifyWrite(evidence, event.latestUs <= stepDeadline(c));
        c.stagingApplied = c.setupExecution == ActionExecution::ACKNOWLEDGED;
    } else if (c.step == 1) { c.triggerEvidence = evidence; c.execution = Detail::classifyWrite(evidence, event.latestUs <= stepDeadline(c)); }
    else if (c.phase == HomePhase::ZERO_CHECK) {
        c.zeroEvidence = evidence;
        // Publish only what the zero decision below may use; zeroEvidence keeps every frame.
        if (event.kind == ReadEventKind::FRAME && evidence.status && event.qualified && supplied.responseConfirmed &&
            event.latestUs <= stepDeadline(c)) {
            c.rawPositionWords[0] = words[0]; c.rawPositionWords[1] = words[1];
            const WordOrder order = homeEndRule(c.prerequisites.auxiliary) == HomeEndRule::ZERO &&
                !validOrder(c.prerequisites.offsetOrder) ? WordOrder::HIGH_WORD_FIRST : c.prerequisites.offsetOrder;
            c.endKnown = decodeInt32(words, 2, order, c.endPosition).isOk();
        }
    } else if (c.qualificationRevoked) {
        c.lastObservation = evidence;
        // Retain the settled observation, without promoting it to a completion.
        if (event.kind == ReadEventKind::FRAME && evidence.status && event.qualified && supplied.responseConfirmed) {
            c.observationKnown = true; c.rawAlarm = words[0]; c.rawMotion = words[1];
            if (c.prerequisites.observeInputs) { c.inputsObserved = true; c.rawInputs = words[2]; }
        }
    }
    c.servicedUs = nowUs;
    if (c.qualificationRevoked) finish(c, ActionOutcome::CANCELLED,
        failed(HomeError::QUALIFICATION_REVOKED, "home admission revoked; settled effects retained"));
    else if (event.kind == ReadEventKind::CANCEL) finish(c, ActionOutcome::CANCELLED, evidence.status);
    else if (event.kind == ReadEventKind::DEADLINE) finish(c, ActionOutcome::DEADLINE, evidence.status);
    else if (event.kind == ReadEventKind::TRANSPORT_FAILURE) finish(c, ActionOutcome::TRANSPORT_ERROR, evidence.status);
    else if (!event.qualified) finish(c, ActionOutcome::TIMING_UNQUALIFIED,
        failed(HomeError::TIMING_UNQUALIFIED, "home closure timing is unqualified"));
    else if (event.latestUs > stepDeadline(c)) finish(c, ActionOutcome::DEADLINE, expired(c));
    else if (!evidence.status) finish(c, ActionOutcome::REPLY_ERROR, evidence.status);
    else if ((!supplied.responseConfirmed || (c.step < 2 && event.executionUnknown)) && c.step != 1) finish(c, ActionOutcome::UNCONFIRMED_RESPONSE,
        failed(HomeError::UNCONFIRMED_RESPONSE, "home response source or write execution is unconfirmed"));
    else if (c.step < 2) {
        if (nowUs >= c.deadlineUs) finish(c, ActionOutcome::DEADLINE, failed(HomeError::DEADLINE_EXPIRED, "no budget for next home step"));
        else if (c.step == 0 && nowUs >= stepDeadline(c)) finish(c, ActionOutcome::DEADLINE, expired(c));
        else if (c.step == 0) { ++c.step; c.phase = HomePhase::TRIGGER; c.eligibleUs = nowUs; }
        else {
            // A valid on-time echo may be local. Observe without confirming or
            // replaying the trigger; transport failures have already ended above.
            c.phase = HomePhase::OBSERVING; wait(c, nowUs);
        }
    } else if (c.phase == HomePhase::ZERO_CHECK) {
        const HomeEndRule rule = homeEndRule(c.prerequisites.auxiliary);
        const bool expected = rule == HomeEndRule::ZERO ? !words[0] && !words[1] :
            rule == HomeEndRule::OBSERVED ? c.endKnown : c.endKnown && c.endPosition == c.expectedPosition;
        if (!expected) finish(c, ActionOutcome::REPLY_ERROR,
            failed(HomeError::ZERO_NOT_OBSERVED, "fresh position pair does not report the expected end coordinate"));
        else {
            c.completion = ActionCompletion::OBSERVED;
            if (c.execution == ActionExecution::ACKNOWLEDGED) finish(c, ActionOutcome::OBSERVED, Ok());
            else finish(c, ActionOutcome::UNCONFIRMED_RESPONSE,
                failed(HomeError::UNCONFIRMED_RESPONSE, "home state observed; trigger execution remains unknown"));
        }
    } else {
        const bool runningEarlier = c.runningObserved;
        ++c.polls; c.lastObservation = evidence; c.observationKnown = true;
        c.rawAlarm = words[0]; c.rawMotion = words[1];
        if (c.prerequisites.observeInputs) { c.inputsObserved = true; c.rawInputs = words[2]; }
        if (words[0] || (words[1] & FAULTS)) finish(c, ActionOutcome::REPLY_ERROR,
            failed(HomeError::DRIVE_FAULT, "drive alarm, release or limit interrupted home"));
        else if (words[1] & UNKNOWN_MOTION) finish(c, ActionOutcome::REPLY_ERROR,
            failed(HomeError::READINESS, "home motion status contains unqualified bits"));
        else if (c.prerequisites.observeInputs && !inputReadiness(c.prerequisites, words[2]))
            finish(c, ActionOutcome::REPLY_ERROR, failed(HomeError::INPUT_REQUIRED, "logical inputs contradict qualified mapping"));
        else if (c.request.method == HomingMethod::METHOD_35 && (words[1] & RUNNING))
            finish(c, ActionOutcome::REPLY_ERROR,
                failed(HomeError::UNEXPECTED_ACTIVITY, "running report contradicts nonmoving home method35"));
        else {
            if (!(words[1] & HOMED)) {
                if (!c.lowEvidence.length) { c.lowEvidence = evidence; c.lowObservedUs = c.eligibleUs; }
                c.homedLowObserved = true;
            }
            if (words[1] & RUNNING) {
                if (!c.runningObserved) c.activityEvidence = evidence;
                c.runningObserved = true;
            }
            const bool needsActivity = c.request.method != HomingMethod::METHOD_35;
            if (c.homedLowObserved && (!needsActivity || c.runningObserved) &&
                !(words[1] & RUNNING) && (words[1] & HOMED)) {
                // HOMED is the documented completion (function manual p68). ARRIVED is not
                // required: on firmware 0x0029 a home begun with it clear ends with it clear.
                c.completionEvidence = evidence; c.completionObservedUs = c.eligibleUs;
                c.phase = HomePhase::ZERO_CHECK;
            }
        }
        if (c.state == ActionState::ACTIVE) {
            if (c.polls >= c.options.maxPolls && c.phase != HomePhase::ZERO_CHECK)
                finish(c, ActionOutcome::OBSERVATION_LIMIT, failed(HomeError::OBSERVATION_LIMIT, "fresh homing transition was not observed"));
            else if (nowUs >= c.deadlineUs) finish(c, ActionOutcome::DEADLINE,
                failed(HomeError::DEADLINE_EXPIRED, "no budget for next home observation"));
            else wait(c, nowUs, runningEarlier);
        }
    }
    if (c.state == ActionState::FAILED)
        c.failureEvidence = c.completion == ActionCompletion::OBSERVED ? c.triggerEvidence : evidence;
    return Ok();
}

Status getHomeReference(const HomeContext& c, uint64_t nowUs, uint64_t maximumAgeUs, AxisReference& output) noexcept {
    if (c.qualificationRevoked) return invalid(HomeError::QUALIFICATION_REVOKED, "home admission revoked");
    if (c.completion == ActionCompletion::OBSERVED && c.execution != ActionExecution::ACKNOWLEDGED)
        return invalid(HomeError::REFERENCE_UNQUALIFIED, "observed home state does not confirm the trigger");
    if (c.state != ActionState::SUCCEEDED || c.completion != ActionCompletion::OBSERVED)
        return invalid(HomeError::NOT_COMPLETE, "home completion is not established");
    if (!c.prerequisites.referenceSemanticsQualified)
        return invalid(HomeError::REFERENCE_UNQUALIFIED, "homing zero to command-coordinate relation is unresolved");
    // Request eligibility bounds when the device may have sampled its values;
    // response closure/delivery must not renew the older stationary witness.
    const uint64_t observedUs = c.completionObservedUs;
    if (nowUs < c.servicedUs || !evidenceAgeValid(observedUs, nowUs, maximumAgeUs))
        return invalid(HomeError::STALE_REFERENCE, "homing reference is stale");
    AxisReference reference;
    reference.target = c.target; reference.configurationGeneration = c.request.configurationGeneration;
    reference.idle = reference.stationary = reference.nativeKnown = true;
    reference.nativePosition = homeEndRule(c.prerequisites.auxiliary) == HomeEndRule::ZERO ? 0 : c.endPosition;
    reference.basis = RelativeBasis::ACTUAL; reference.source = ScaleSource::QUALIFIED;
    reference.observedUs = observedUs; reference.nowUs = nowUs; reference.maximumAgeUs = maximumAgeUs;
    output = reference; return Ok();
}
}} // namespace MotorControlRS::ESS_RS
