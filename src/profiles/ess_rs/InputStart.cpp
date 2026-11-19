// SPDX-License-Identifier: MIT
#include "MotorControlRS/profiles/ess_rs/InputStart.h"
#include "MotorControlRS/profiles/ess_rs/Registers.h"
#include "MotorControlRS/profiles/ess_rs/Types.h"
#include <new>

namespace MotorControlRS { namespace ESS_RS {
namespace {
constexpr uint16_t RUNNING = static_cast<uint16_t>(MotionStatusBit::RUNNING);
constexpr uint16_t HOMED = static_cast<uint16_t>(MotionStatusBit::HOMING_COMPLETE);
constexpr uint16_t FAULTS = static_cast<uint16_t>(MotionStatusBit::ALARM) |
    static_cast<uint16_t>(MotionStatusBit::RELEASED) |
    static_cast<uint16_t>(MotionStatusBit::POSITIVE_SOFT_LIMIT) |
    static_cast<uint16_t>(MotionStatusBit::NEGATIVE_SOFT_LIMIT);
constexpr uint16_t APPLICATION_MISSING = INPUT_START_MISSING_OVERWRITE | INPUT_START_MISSING_SERIAL_START |
    INPUT_START_MISSING_GENERATION;

Status invalid(InputStartError reason, const char* message) {
    return Status(Err::INVALID_CONFIG, static_cast<int32_t>(reason), message);
}
bool sameTarget(const ReadTarget& a, const ReadTarget& b) {
    return a.id == b.id && a.address == b.address && a.generation == b.generation;
}
void finish(InputStartContext& c, InputStartOutcome outcome, uint64_t nowUs, const char* message) {
    c.outcome = outcome; c.finishedUs = nowUs;
    const bool positive = outcome == InputStartOutcome::ATTRIBUTED || outcome == InputStartOutcome::NO_START ||
        outcome == InputStartOutcome::STOPPED;
    c.state = positive ? ActionState::SUCCEEDED : ActionState::FAILED;
    c.status = positive ? Ok() : Status(Err::ILLEGAL_VALUE, static_cast<int32_t>(outcome), message);
}
// Time from which "no further start" is observed: the falling edge, or a later stop.
uint64_t releaseReference(const InputStartContext& c) {
    uint64_t reference = c.sourceOns && !c.sourceActive ? c.offUs : 0;
    if (c.stopRequested && c.stopUs > reference) reference = c.stopUs;
    return reference;
}
bool released(const InputStartContext& c) {
    return (c.sourceOns && !c.sourceActive) || c.stopRequested;
}
// Standstill as the drive reports it: RUNNING clear and speed word 0. After a
// serial, group or input stop the drive clears RUNNING while the speed word still
// decays and the position still settles (wired bench 09, A6d); a sample without
// RUNNING alone is neither a completion nor a stopped state.
bool moving(const uint16_t* words) { return (words[1] & RUNNING) != 0 || words[6] != 0; }
bool concludable(const InputStartContext& c, uint64_t nowUs) {
    if (!c.lastKnown || moving(c.last.words) || !released(c)) return false;
    if (c.stopRequested && !c.stoppedAfterStop) return false;
    const uint64_t reference = releaseReference(c);
    return nowUs >= reference + c.options.postReleaseUs && c.samplesAfterRelease >= c.options.postReleaseSamples;
}
uint16_t attributionMissing(const InputStartContext& c) {
    uint16_t missing = c.missing;
    if (!c.preEdgeKnown || !c.preEdgeMatched) missing |= INPUT_START_MISSING_PRE_EDGE;
    if (!c.isolationKnown || !c.isolationHeld) missing |= INPUT_START_MISSING_ISOLATION;
    if (c.sourceOns != 1 || c.sourceOutsideWindow) missing |= INPUT_START_MISSING_SOURCE_EDGE;
    if (c.changeObserved && !c.changeAfterEdge) missing |= INPUT_START_MISSING_CHANGE_AFTER_EDGE;
    if (!c.changeObserved && !c.stopRequested && c.effect == InputStartEffect::ENDPOINT &&
        c.baselinePosition == c.prerequisites.expectedEndpoint) missing |= INPUT_START_MISSING_RUNNING;
    return missing;
}
void conclude(InputStartContext& c, uint64_t nowUs) {
    c.missing = attributionMissing(c);
    if (c.stopRequested) { finish(c, InputStartOutcome::STOPPED, nowUs, "stopped"); return; }
    if (c.missing) { finish(c, InputStartOutcome::INDETERMINATE, nowUs, "a witness is missing"); return; }
    finish(c, c.changeObserved ? InputStartOutcome::ATTRIBUTED : InputStartOutcome::NO_START, nowUs, "concluded");
}
void recordSpacing(InputStartContext& c, uint64_t requestUs) {
    if (c.spacingFromUs && requestUs >= c.spacingFromUs) {
        const uint64_t gap = requestUs - c.spacingFromUs;
        const uint32_t spacing = gap > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(gap);
        c.spacingUs[c.spacingCount % INPUT_START_SPACING_RING] = spacing;
        if (!c.spacingCount || spacing < c.spacingMinUs) c.spacingMinUs = spacing;
        if (spacing > c.spacingMaxUs) c.spacingMaxUs = spacing;
        if (c.spacingCount != UINT32_MAX) ++c.spacingCount;
        if (gap > 2ull * c.options.pollIntervalUs) { ++c.lateSamples; c.missing |= INPUT_START_MISSING_SPACING; }
    }
    c.spacingFromUs = requestUs;
}
bool tracksSpacing(const InputStartContext& c, bool preEdge) { return preEdge || c.preEdgeKnown || c.sourceOns; }
void consume(InputStartContext& c, const InputStartSample& s, int32_t position, uint64_t nowUs) {
    const auto& p = c.prerequisites;
    const uint16_t alarm = s.words[0], motion = s.words[1], speed = s.words[6];
    if (tracksSpacing(c, s.preEdge)) recordSpacing(c, s.requestUs);
    if (c.samples != UINT32_MAX) ++c.samples;
    c.last = s; c.lastKnown = true; c.lastPosition = position;
    if (!s.preEdge) ++c.nextSequence;
    c.nextSampleUs = s.requestUs + c.options.pollIntervalUs;
    if (alarm || (motion & FAULTS)) { finish(c, InputStartOutcome::DRIVE_FAULT, nowUs, "drive reported alarm, release or soft limit"); return; }
    const bool running = (motion & RUNNING) != 0, inMotion = moving(s.words);
    const int32_t reference = c.completed ? c.completedPosition : c.baselinePosition;
    const bool changed = inMotion || position != reference;
    bool inputActive = false;
    inputStartLevel(s.words[2], p.terminal, p.polarity, p.statusMapping, inputActive);
    if (s.preEdge) {
        c.preEdgeKnown = true; c.preEdge = s;
        c.preEdgeMatched = !changed && !inputActive && !speed;
        if (changed) { finish(c, InputStartOutcome::UNATTRIBUTED_MOTION, nowUs, "change before the source edge"); return; }
        if (inputActive) c.missing |= INPUT_START_MISSING_INPUT_BEFORE_EDGE;
        else c.unchangedRequestUs = s.requestUs;
        return;
    }
    if (!c.sourceOns) {
        if (changed) { finish(c, InputStartOutcome::UNATTRIBUTED_MOTION, nowUs, "change before the source edge"); return; }
        if (inputActive) c.missing |= INPUT_START_MISSING_INPUT_BEFORE_EDGE;
        else c.unchangedRequestUs = s.requestUs;
        return;
    }
    if (c.stopRequested && s.requestUs >= c.stopUs && !inMotion) c.stoppedAfterStop = true;
    if (released(c) && s.requestUs >= releaseReference(c) && c.samplesAfterRelease != UINT32_MAX) ++c.samplesAfterRelease;
    if (inputActive && !c.inputObserved) { c.inputObserved = true; c.inputObservedUs = s.requestUs; }
    if (changed && c.sourceOutsideWindow) {
        finish(c, InputStartOutcome::UNATTRIBUTED_MOTION, nowUs, "change with the source edge outside the armed window"); return;
    }
    if (!c.changeObserved) {
        if (!changed) { c.unchangedRequestUs = s.requestUs; return; }
        c.changeObserved = true;
        c.changeStartUs = c.unchangedRequestUs; c.changeEndUs = s.replyEndUs;
        c.changeAfterEdge = s.requestUs >= c.onUs;
    } else if (c.completed) {
        // A further start is a drive-reported RUNNING sample after the completion.
        // Position or speed changing without RUNNING is the drive settling after a
        // stop (wired bench 09: further changes after the reported standstill
        // with no second move) or a start that completed between samples: not
        // excluded, never reported as a start.
        if (running) {
            c.furtherStart = true; c.furtherStartUs = s.requestUs;
            finish(c, InputStartOutcome::SECOND_START, nowUs, "a further start after the completion");
        } else if (changed && !c.settledAfterCompletion) {
            c.settledAfterCompletion = true; c.settledUs = s.requestUs;
        }
        return;
    }
    if (inMotion) { if (running) c.runningObserved = true; return; }
    // At standstill after a change: a finite move may complete between samples (onset unknown).
    c.completed = true; c.completedUs = s.replyEndUs; c.completedPosition = position;
    c.homedObserved = (motion & HOMED) != 0;
}
} // namespace

bool inputStartLevel(uint16_t rawInputs, uint8_t terminal, uint16_t polarity, IoStatusMapping mapping, bool& active) noexcept {
    if (terminal > 3 || polarity > 1 || mapping > IoStatusMapping::AFTER_INVERSION) return false;
    const bool bit = (rawInputs & (1u << terminal)) != 0;
    if (mapping == IoStatusMapping::UNKNOWN) {
        if (polarity) return false;
        active = bit;
    } else active = mapping == IoStatusMapping::BEFORE_INVERSION ? bit != bool(polarity) : bit;
    return true;
}

Status prepareInputStart(InputStartContext& output, const InputStartPrerequisites& p, uint32_t operationId,
                         const InputStartOptions& o, uint64_t nowUs) noexcept {
    using E = InputStartError;
    if (!p.target.id || !p.target.generation || !isValidAddress(p.target.address) || !p.configurationGeneration)
        return invalid(E::INVALID_TARGET, "invalid input-start target");
    if (!operationId) return invalid(E::INVALID_OPERATION, "zero operation id");
    if (p.terminal > 3) return invalid(E::INVALID_TERMINAL, "terminal outside X0..X3");
    if (p.function != 7 && p.function != 11 && p.function != 12 && p.function != 13)
        return invalid(E::INVALID_FUNCTION, "not a start function (7, 11, 12, 13)");
    if (o.pollIntervalUs < INPUT_START_MIN_POLL_US || o.pollIntervalUs > INPUT_START_MAX_POLL_US ||
        o.windowUs < INPUT_START_MIN_WINDOW_US || o.windowUs > INPUT_START_MAX_WINDOW_US || !o.postReleaseUs ||
        o.postReleaseSamples < INPUT_START_POST_RELEASE_SAMPLES || p.polarity > 1 ||
        p.statusMapping > IoStatusMapping::AFTER_INVERSION) return invalid(E::INVALID_OPTIONS, "invalid input-start options");
    InputStartEffect effect = InputStartEffect::HOME_COMPLETION;
    if (p.function == 7 || p.function == 12) {
        if (!p.positionModeKnown || p.positionMode > 1) return invalid(E::POSITION_MODE_UNKNOWN, "0x0050 unknown");
        effect = p.positionMode ? InputStartEffect::ENDPOINT : InputStartEffect::DISPLACEMENT;
    } else if (p.function == 13) {
        if (!p.pvTriggerModeKnown || p.pvTriggerMode > 1) return invalid(E::PV_TRIGGER_MODE_UNKNOWN, "0x0051 unknown");
        effect = InputStartEffect::VELOCITY_RUN;
    }
    if (p.statusMapping == IoStatusMapping::UNKNOWN && p.polarity)
        return invalid(E::MAPPING_UNKNOWN, "inverted polarity needs a stated status mapping");
    if (!p.sourceInhibitedUntilArmed) return invalid(E::SOURCE_NOT_INHIBITED, "trigger source not inhibited until armed");
    if (!p.baselineKnown || !p.wordOrderKnown) return invalid(E::BASELINE_REQUIRED, "baseline sample required");
    if (p.baselineUs > nowUs) return invalid(E::CLOCK_ERROR, "baseline after now");
    if (!evidenceAgeValid(p.baselineUs, nowUs, p.maximumAgeUs)) return invalid(E::STALE_FACTS, "baseline too old");
    const uint16_t* w = p.baselineWords;
    if (w[0] || (w[1] & static_cast<uint16_t>(MotionStatusBit::ALARM))) return invalid(E::ALARM, "baseline alarm");
    if (w[1] & static_cast<uint16_t>(MotionStatusBit::RELEASED)) return invalid(E::RELEASED, "baseline released");
    if ((w[1] & (RUNNING | FAULTS)) || w[6]) return invalid(E::NOT_STATIONARY, "baseline not stationary");
    bool active = false;
    if (!inputStartLevel(w[2], p.terminal, p.polarity, p.statusMapping, active))
        return invalid(E::MAPPING_UNKNOWN, "input level unknown");
    if (active) return invalid(E::INPUT_ACTIVE, "input active before arming");
    if ((effect == InputStartEffect::DISPLACEMENT || effect == InputStartEffect::ENDPOINT) && !p.expectedKnown)
        return invalid(E::EXPECTED_REQUIRED, "expected displacement or endpoint required");
    int32_t position = 0;
    if (!decodeInt32(w + 4, 2, p.wordOrder, position)) return invalid(E::BASELINE_REQUIRED, "baseline position undecodable");
    if (nowUs > UINT64_MAX - o.windowUs) return invalid(E::CLOCK_ERROR, "window overflows the clock");
    // Every check passed: build in place, so no context copy sits on the caller's stack.
    const InputStartPrerequisites facts = p; // p may alias output.prerequisites.
    output.~InputStartContext();
    InputStartContext& c = *new (&output) InputStartContext();
    c.target = facts.target; c.operationId = operationId; c.configurationGeneration = facts.configurationGeneration;
    c.prerequisites = facts; c.options = o; c.effect = effect; c.state = ActionState::ACTIVE;
    c.armedUs = c.nextSampleUs = nowUs; c.windowEndUs = nowUs + o.windowUs;
    c.unchangedRequestUs = facts.baselineUs;
    c.baselinePosition = c.lastPosition = c.completedPosition = position;
    return Ok();
}

Status noteInputStartSource(InputStartContext& c, uint64_t atUs, bool active) noexcept {
    if (c.state != ActionState::ACTIVE) return invalid(InputStartError::INVALID_STATE, "input start is not active");
    if (active) {
        if (c.sourceOns) return invalid(InputStartError::SECOND_SOURCE_ON, "one source ON per arm");
        c.sourceOns = 1; c.sourceActive = true; c.onUs = atUs;
        c.sourceOutsideWindow = atUs < c.armedUs || atUs > c.windowEndUs;
        return Ok();
    }
    if (!c.sourceOns || !c.sourceActive) return invalid(InputStartError::INVALID_STATE, "no active source edge");
    if (atUs < c.onUs) return invalid(InputStartError::CLOCK_ERROR, "falling edge before the rising edge");
    c.sourceActive = false; c.offUs = atUs;
    if (!c.stopRequested || c.stopUs <= atUs) c.samplesAfterRelease = 0;
    return Ok();
}

Status nextInputStart(const InputStartContext& c, uint64_t nowUs, PreparedInputStart& output) noexcept {
    PreparedInputStart w;
    w.target = c.target; w.operationId = c.operationId;
    if (c.state != ActionState::ACTIVE) { output = w; return Ok(); }
    if (nowUs >= c.windowEndUs || concludable(c, nowUs)) {
        w.kind = InputStartWork::CONCLUDE; w.eligibleUs = nowUs; output = w; return Ok();
    }
    if (nowUs < c.nextSampleUs) {
        w.kind = InputStartWork::WAIT; w.eligibleUs = c.nextSampleUs < c.windowEndUs ? c.nextSampleUs : c.windowEndUs;
        output = w; return Ok();
    }
    w.length = buildReadRegisters(c.target.address, Registers::ERROR_CODE, INPUT_START_SAMPLE_WORDS, w.bytes, sizeof(w.bytes));
    if (!w.length) return invalid(InputStartError::INVALID_STATE, "sample request not buildable");
    w.kind = InputStartWork::SAMPLE; w.sequence = c.nextSequence;
    w.eligibleUs = c.nextSampleUs; w.deadlineUs = c.windowEndUs;
    output = w;
    return Ok();
}

Status advanceInputStart(InputStartContext& c, const InputStartEvent& e, uint64_t nowUs) noexcept {
    using E = InputStartError;
    if (c.state != ActionState::ACTIVE) return invalid(E::INVALID_STATE, "input start is not active");
    if (e.operationId != c.operationId) return invalid(E::WRONG_CORRELATION, "wrong operation");
    switch (e.kind) {
    case InputStartEventKind::SAMPLE: case InputStartEventKind::LOST: {
        if (e.preEdge ? e.sequence != 0 || c.sourceOns || c.preEdgeKnown : e.sequence != c.nextSequence)
            return invalid(E::WRONG_CORRELATION, "wrong sample sequence");
        if (e.requestUs > nowUs) return invalid(E::CLOCK_ERROR, "sample request after now");
        uint16_t words[INPUT_START_SAMPLE_WORDS] = {};
        std::size_t count = 0;
        bool checked = false;
        if (e.kind == InputStartEventKind::SAMPLE) {
            const auto& t = e.transport;
            if (t.kind != ReadEventKind::FRAME || !t.frame || !sameTarget(t.target, c.target))
                return invalid(E::INVALID_EVENT, "sample needs the target's reply frame");
            if (t.qualified && (t.latestUs > nowUs || t.earliestUs > t.latestUs || t.earliestUs < e.requestUs))
                return invalid(E::CLOCK_ERROR, "reply closure outside its request bracket");
            checked = t.qualified && parseRegisters(t.frame, t.length, c.target.address, INPUT_START_SAMPLE_WORDS,
                words, INPUT_START_SAMPLE_WORDS, count).isOk() && count == INPUT_START_SAMPLE_WORDS;
        }
        int32_t position = 0;
        if (checked && !decodeInt32(words + 4, 2, c.prerequisites.wordOrder, position)) checked = false;
        if (!checked) {
            // A lost sample is a missing witness after the pre-edge sample; nothing is resent.
            if (c.lostSamples != UINT32_MAX) ++c.lostSamples;
            if (e.preEdge || c.preEdgeKnown || c.sourceOns) c.missing |= INPUT_START_MISSING_SAMPLE;
            if (!e.preEdge) ++c.nextSequence;
            c.nextSampleUs = e.requestUs + c.options.pollIntervalUs;
            return Ok();
        }
        InputStartSample s;
        s.sequence = e.sequence; s.requestUs = e.requestUs; s.replyEndUs = e.transport.latestUs; s.preEdge = e.preEdge;
        for (std::size_t i = 0; i < INPUT_START_SAMPLE_WORDS; ++i) s.words[i] = words[i];
        consume(c, s, position, nowUs);
        return Ok();
    }
    case InputStartEventKind::ISOLATION:
        if (c.sourceOns) return invalid(E::INVALID_STATE, "isolation is decided before the ON");
        c.isolationKnown = true; c.isolationHeld = e.held;
        return Ok();
    case InputStartEventKind::STOP:
        c.stopRequested = true; c.stopId = e.stopId; c.stopUs = nowUs; c.stoppedAfterStop = false;
        c.samplesAfterRelease = 0;
        return Ok();
    case InputStartEventKind::DEGRADED:
        if (!e.missing || (e.missing & ~APPLICATION_MISSING)) return invalid(E::INVALID_EVENT, "unknown degradation");
        c.missing |= e.missing;
        return Ok();
    case InputStartEventKind::FAULT:
        finish(c, InputStartOutcome::TRANSPORT_ERROR, nowUs, "sampling ended by a transport fault");
        return Ok();
    case InputStartEventKind::CANCEL:
        finish(c, InputStartOutcome::CANCELLED, nowUs, "observation cancelled");
        return Ok();
    case InputStartEventKind::TIME:
        if (concludable(c, nowUs)) conclude(c, nowUs);
        else if (nowUs >= c.windowEndUs) {
            c.missing = attributionMissing(c);
            finish(c, InputStartOutcome::DEADLINE, nowUs, "armed window closed");
        }
        return Ok();
    }
    return invalid(E::INVALID_EVENT, "unknown event");
}

Status getInputStart(const InputStartContext& c, InputStartResult& out) noexcept {
    if (c.state == ActionState::EMPTY) return invalid(InputStartError::INVALID_STATE, "input start is empty");
    InputStartResult r;
    r.effect = c.effect; r.state = c.state; r.outcome = c.outcome; r.status = c.status;
    r.missing = c.state == ActionState::ACTIVE ? attributionMissing(c) : c.missing;
    r.execution = c.changeObserved ? InputStartExecution::OBSERVED : InputStartExecution::NOT_OBSERVED;
    if (c.completed)
        r.completion = c.effect == InputStartEffect::VELOCITY_RUN ? InputStartCompletion::VELOCITY_STOPPED :
            c.effect == InputStartEffect::HOME_COMPLETION && c.homedObserved ? InputStartCompletion::HOMED :
            InputStartCompletion::FINITE_STOPPED;
    if (c.furtherStart) r.further = InputStartFurther::SEEN;
    else if (c.completed && c.state != ActionState::ACTIVE)
        r.further = c.effect == InputStartEffect::ENDPOINT || c.settledAfterCompletion ? InputStartFurther::NOT_EXCLUDED :
            InputStartFurther::NONE_SEEN;
    r.sourceCommanded = c.sourceOns != 0; r.sourceActive = c.sourceActive; r.inputObserved = c.inputObserved;
    r.onUs = c.onUs; r.offUs = c.offUs; r.changeObserved = c.changeObserved; r.runningObserved = c.runningObserved;
    r.changeStartUs = c.changeStartUs; r.changeEndUs = c.changeEndUs;
    r.stopRequested = c.stopRequested; r.stoppedAfterStop = c.stoppedAfterStop; r.stopId = c.stopId;
    r.positionKnown = c.lastKnown;
    r.positionChange = static_cast<int64_t>(c.lastPosition) - c.baselinePosition;
    if (c.lastKnown) { r.lastRequestUs = c.last.requestUs; r.lastReplyEndUs = c.last.replyEndUs; }
    r.expectedKnown = c.prerequisites.expectedKnown &&
        (c.effect == InputStartEffect::DISPLACEMENT || c.effect == InputStartEffect::ENDPOINT);
    r.expectedChange = c.effect == InputStartEffect::DISPLACEMENT ? c.prerequisites.expectedDisplacement :
        static_cast<int64_t>(c.prerequisites.expectedEndpoint) - c.baselinePosition;
    r.positionMatches = c.completed && r.expectedKnown &&
        static_cast<int64_t>(c.completedPosition) - c.baselinePosition == r.expectedChange;
    r.samples = c.samples; r.lostSamples = c.lostSamples; r.lateSamples = c.lateSamples;
    r.spacingMinUs = c.spacingMinUs; r.spacingMaxUs = c.spacingMaxUs; r.spacingCount = c.spacingCount;
    const std::size_t kept = c.spacingCount < INPUT_START_SPACING_RING ? c.spacingCount : INPUT_START_SPACING_RING;
    if (kept) {
        uint32_t sorted[INPUT_START_SPACING_RING];
        for (std::size_t i = 0; i < kept; ++i) {
            const uint32_t value = c.spacingUs[i];
            std::size_t j = i;
            for (; j && sorted[j - 1] > value; --j) sorted[j] = sorted[j - 1];
            sorted[j] = value;
        }
        r.spacingMedianUs = sorted[kept / 2];
    }
    out = r;
    return Ok();
}

}} // namespace MotorControlRS::ESS_RS
