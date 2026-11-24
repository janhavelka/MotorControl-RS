// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.
// Reads retain raw target bits; typed writes use the existing conservative range.
bool profileTargetInRange(const uint16_t* pair, ESS::WordOrder order) {
    int32_t target = 0;
    return ESS::decodeInt32(pair, 2, order, target) && target >= -0x0FFFFFFF && target <= 0x0FFFFFFF;
}
// Production movement admission consumes the same cached observations and typed
// profile snapshot used by the public operations. Diagnostics grant no access.

// The facts one move admission reads for one target. The selected axis and each
// drive-group member build this view over their own storage; the rules below
// are shared, never copied per caller.
struct MotionFacts {
    const MotorControlRS::AxisConfig& axis;
    const ESS::ConfigObservation& configuration;
    const App::MotionProfileState& profile;
    const Probe::StateCache& state;
    bool startSpeedRelationRequired;
    const PositionStartSpeedRelation& startSpeedRelation;
    uint32_t bindingGeneration, serialGeneration;
    uint64_t maximumAgeUs;
    const MotorControlRS::AxisReference* reference; ///< Absolute targets only; null refuses them.
    const IoQualification* inputQualifications;
    /** Wired bench 11: the selected axis's soft-limit state is BOUND
     * (SoftLimitApp.h). Then 0x0018 = 1 is admitted and the host axis soft
     * limits bound endpoints. Group members and arms keep false: they need
     * 0x0018 known 0. */
    bool softLimitBound = false;
    MotionFacts(const MotorControlRS::AxisConfig& axisConfig, const ESS::ConfigObservation& config,
                const App::MotionProfileState& motionProfile, const Probe::StateCache& cache,
                bool relationRequired, const PositionStartSpeedRelation& relation,
                uint32_t binding, uint32_t serial, uint64_t maximumAge,
                const MotorControlRS::AxisReference* axisReference = nullptr, const IoQualification* io = nullptr)
        : axis(axisConfig), configuration(config), profile(motionProfile), state(cache),
          startSpeedRelationRequired(relationRequired), startSpeedRelation(relation),
          bindingGeneration(binding), serialGeneration(serial), maximumAgeUs(maximumAge), reference(axisReference), inputQualifications(io) {}
    const Probe::StateCache::Block& block(ESS::StateBlock kind) const { return state.blocks[static_cast<uint8_t>(kind)]; }
};
MotionFacts selectedFacts(const App& a, const MotorControlRS::AxisReference* reference = nullptr) {
    MotionFacts facts(a.axis, a.configuration, a.motionProfile, a.stateCache, a.positionStartSpeedRelationRequired,
        a.positionStartSpeedRelation, a.bindingGeneration, a.serial.generation, a.observationAgeUs(), reference, a.ioQualifications);
    facts.softLimitBound = softLimitState(a, nowUs()) == SoftLimitState::BOUND;
    return facts;
}
// Wired bench 07 decision: every motion admission (finite, staged, group,
// velocity, home) refuses before TX when a fresh input status, read through the
// qualified mapping, shows an assigned release (4), stop (5) or emergency stop (6)
// input active. Stops keep no input check. An input asserted after the last read
// cannot be excluded by the host; the motion observers report it (STOP_INPUT).
enum class StopInputs : uint8_t { CLEAR, STALE, MAPPING_UNKNOWN, STOP_ACTIVE, RELEASE_ACTIVE };
struct StopInputCheck {
    StopInputs result = StopInputs::STALE;
    uint8_t terminal = 0;
    uint8_t mask = 0;      ///< Terminals assigned 4-6, bit i for Xi.
    uint8_t activeLow = 0; ///< Of those, terminals active when their status bit reads 0.
};
// The terminal's move-purpose I/O record states the mapping; without one only
// default polarity has a known level (as moveInputPolicy).
const IoQualification* stopInputRecord(const MotionFacts& f, uint8_t i, uint64_t now) {
    const IoQualification* q = f.inputQualifications ? f.inputQualifications + i : nullptr;
    const uint16_t function = f.configuration.raw.inputFunctions[i], polarity = (f.configuration.raw.inputPolarity >> i) & 1;
    return q && q->purpose == IoPurpose::FINITE_MOVE && q->terminal == i && Probe::sameTarget(q->target, f.axis.target) &&
        q->configurationGeneration == f.axis.generation && q->function == function && q->polarity == polarity &&
        ioFactsValid(*q, now) ? q : nullptr;
}
StopInputCheck stopInputs(const MotionFacts& f, uint64_t now) {
    StopInputCheck check;
    const auto& raw = f.configuration.raw;
    for (uint8_t i = 0; i < 4; ++i)
        if (raw.inputFunctions[i] >= 4 && raw.inputFunctions[i] <= 6) check.mask |= static_cast<uint8_t>(1u << i);
    if (!check.mask) { check.result = StopInputs::CLEAR; return check; }
    const auto& io = f.block(ESS::StateBlock::IO);
    if (!Probe::fresh(io, f.axis.target, now, f.maximumAgeUs) || io.value.unknownInputBits) return check;
    for (uint8_t i = 0; i < 4; ++i) {
        if (!(check.mask & (1u << i))) continue;
        const uint16_t polarity = (raw.inputPolarity >> i) & 1;
        const auto* q = stopInputRecord(f, i, now);
        const auto mapping = q ? q->input.statusMapping : ESS::IoStatusMapping::UNKNOWN;
        bool active = false;
        check.terminal = i;
        if (!effectiveInput(io.value.rawInputs, i, polarity, polarity, mapping, active)) {
            check.result = StopInputs::MAPPING_UNKNOWN; return check;
        }
        if (active) {
            check.result = raw.inputFunctions[i] == 4 ? StopInputs::RELEASE_ACTIVE : StopInputs::STOP_ACTIVE;
            return check;
        }
        if (mapping == ESS::IoStatusMapping::BEFORE_INVERSION && polarity) check.activeLow |= static_cast<uint8_t>(1u << i);
    }
    check.terminal = 0;
    check.result = StopInputs::CLEAR;
    return check;
}
// Record the named cause of a motion refusal the stop/release rule decided,
// through the input-decided refusal record the console and API report.
void noteStopInputRefusal(App& a, const StopInputCheck& check) {
    a.settingsRefusal = Probe::SettingsRefusal();
    if (check.result == StopInputs::CLEAR) return;
    a.settingsRefusal.reason = check.result == StopInputs::STOP_ACTIVE ? "stop_input_active" :
        check.result == StopInputs::RELEASE_ACTIVE ? "release_input_active" :
        check.result == StopInputs::MAPPING_UNKNOWN ? "input_mapping_unknown" : "input_state_stale";
    a.settingsRefusal.readFirst = check.result == StopInputs::MAPPING_UNKNOWN ? "io qualification install" : "read state";
    a.settingsRefusal.purpose = "motion";
    a.settingsRefusal.terminal = check.result == StopInputs::STALE ? -1 : static_cast<int8_t>(check.terminal);
}
// Wired bench 11 decision: limit inputs (functions 2 and 3). Measured on drive 4
// (0x4EEA/0x0029, 0x0010 = 0): the drive stops motion toward an asserted limit
// (0x0017 = 0 with the configured deceleration, 1 directly), lets a move away
// from it run to its endpoint, and sets no flag and no alarm. A start toward an
// active limit refuses before TX. A start away refuses too, until a drive-decided
// start with the limit held (prompt 11 L5b) shows the drive accepts it. The
// observers record an assigned limit input reported active (LIMIT_INPUT) on
// terminals declared connected; an unconnected or undeclared terminal adds no
// 0x0008 read, and its stop is reported STOPPED_SHORT/STOPPED_EARLY, never success.
enum class LimitInputs : uint8_t { CLEAR, STALE, MAPPING_UNKNOWN, ACTIVE };
struct LimitInputCheck {
    LimitInputs result = LimitInputs::STALE;
    uint8_t terminal = 0;
    uint8_t mask = 0;      ///< Terminals assigned 2 or 3, bit i for Xi (admission).
    uint8_t witness = 0;   ///< Of those, terminals declared connected (observation mask).
    uint8_t activeLow = 0; ///< Of the witnesses, terminals active when their status bit reads 0.
};
LimitInputCheck limitInputs(const MotionFacts& f, uint64_t now) {
    LimitInputCheck check;
    const auto& raw = f.configuration.raw;
    for (uint8_t i = 0; i < 4; ++i)
        if (raw.inputFunctions[i] == 2 || raw.inputFunctions[i] == 3) {
            check.mask |= static_cast<uint8_t>(1u << i);
            if (f.configuration.wiring[i] == MotorControlRS::InputWiring::CONNECTED) check.witness |= static_cast<uint8_t>(1u << i);
        }
    if (!check.mask) { check.result = LimitInputs::CLEAR; return check; }
    const auto& io = f.block(ESS::StateBlock::IO);
    if (!Probe::fresh(io, f.axis.target, now, f.maximumAgeUs) || io.value.unknownInputBits) return check;
    for (uint8_t i = 0; i < 4; ++i) {
        if (!(check.mask & (1u << i))) continue;
        const uint16_t polarity = (raw.inputPolarity >> i) & 1;
        const auto* q = stopInputRecord(f, i, now);
        const auto mapping = q ? q->input.statusMapping : ESS::IoStatusMapping::UNKNOWN;
        bool active = false;
        check.terminal = i;
        if (!effectiveInput(io.value.rawInputs, i, polarity, polarity, mapping, active)) {
            check.result = LimitInputs::MAPPING_UNKNOWN; return check;
        }
        if (active) { check.result = LimitInputs::ACTIVE; return check; }
        if (mapping == ESS::IoStatusMapping::BEFORE_INVERSION && polarity && (check.witness & (1u << i)))
            check.activeLow |= static_cast<uint8_t>(1u << i);
    }
    check.terminal = 0;
    check.result = LimitInputs::CLEAR;
    return check;
}
void noteLimitInputRefusal(App& a, const LimitInputCheck& check) {
    a.settingsRefusal = Probe::SettingsRefusal();
    if (check.result == LimitInputs::CLEAR) return;
    a.settingsRefusal.reason = check.result == LimitInputs::ACTIVE ? "limit_input_active" :
        check.result == LimitInputs::MAPPING_UNKNOWN ? "input_mapping_unknown" : "input_state_stale";
    a.settingsRefusal.readFirst = check.result == LimitInputs::MAPPING_UNKNOWN ? "io qualification install" : "read state";
    a.settingsRefusal.purpose = "motion";
    a.settingsRefusal.terminal = check.result == LimitInputs::STALE ? -1 : static_cast<int8_t>(check.terminal);
}
bool moveInputPolicy(const MotionFacts& f, uint64_t now) {
    const auto& raw = f.configuration.raw;
    const auto& io = f.block(ESS::StateBlock::IO);
    for (uint8_t i = 0; i < 4; ++i) {
        const uint16_t function = raw.inputFunctions[i], polarity = (raw.inputPolarity >> i) & 1;
        if (function > 17 || (function >= 8 && function <= 10) || function == 13) return false;
        const IoQualification* q = f.inputQualifications ? f.inputQualifications + i : nullptr;
        const bool qualified = q && q->purpose == IoPurpose::FINITE_MOVE && q->terminal == i &&
            Probe::sameTarget(q->target, f.axis.target) && q->configurationGeneration == f.axis.generation &&
            q->function == function && q->polarity == polarity && ioFactsValid(*q, now);
        bool active = false;
        if (function && !effectiveInput(io.value.rawInputs, i, polarity, polarity,
            qualified ? q->input.statusMapping : ESS::IoStatusMapping::UNKNOWN, active)) return false;
        if (function >= 2 && function <= 12 && active) return false;
        if ((function == 7 || function == 11 || function == 12 || function >= 14) &&
            (!qualified || !q->input.externalTriggersInhibited)) return false;
    }
    return true;
}
// Bound later queued writes by the original supplied fact age as well as the
// state sample. Fresh reads cannot renew a commissioning statement.
void capMoveIoAge(const MotionFacts& f, ESS::MovePrerequisites& p) {
    p.inputEvidenceDeadlineUs = MotorControlRS::evidenceAgeDeadline(
        f.block(ESS::StateBlock::IO).observedEarliestUs, f.maximumAgeUs);
    if (!f.inputQualifications) return;
    for (uint8_t i = 0; i < 4; ++i) {
        const auto& q = f.inputQualifications[i];
        if (!q.target.id || q.purpose != IoPurpose::FINITE_MOVE || !q.maximumAgeUs) continue;
        p.inputEvidenceDeadlineUs = std::min(p.inputEvidenceDeadlineUs,
            MotorControlRS::evidenceAgeDeadline(q.qualifiedUs, q.maximumAgeUs));
    }
}
bool motionProfileEndpoint(const MotionFacts& f, const App::MotionProfileState& profile) {
    const auto& view = profile.view;
    return view.saved && view.address == f.axis.target.address &&
        view.serialGeneration == f.serialGeneration && profile.bindingGeneration == f.bindingGeneration &&
        f.configuration.operationId && Probe::sameTarget(f.configuration.target, f.axis.target) &&
        std::memcmp(&profile.configuration, &f.configuration.raw, sizeof(profile.configuration)) == 0;
}
bool motionProfileEndpoint(const App& a, const App::MotionProfileState& profile) {
    return motionProfileEndpoint(selectedFacts(a), profile);
}
bool motionProfileBound(const MotionFacts& f) {
    return motionProfileEndpoint(f, f.profile) && f.profile.view.generation == f.axis.generation;
}
bool motionProfileBound(const App& a) { return motionProfileBound(selectedFacts(a)); }
void revokePositionStartSpeedRelation(App& a, uint8_t address) {
    a.startSpeedRelationRevoked[address / 8] |= static_cast<uint8_t>(1U << (address % 8));
    if (auto* member = groupMember(a, address)) member->startSpeedRelationRequired = true;
    if (address != a.axis.target.address) return;
    a.positionStartSpeedRelationRequired = true;
    a.positionStartSpeedRelation = PositionStartSpeedRelation();
    a.movePrerequisites.startSpeedKnown = a.movePrerequisites.readinessQualified = false;
}
bool positionStartingSpeed(const MotionFacts& f, uint64_t now, uint16_t& minimumRpm) {
    const auto& profile = f.profile.view;
    if (!f.startSpeedRelationRequired) {
        minimumRpm = profile.current[0]; return true; // Existing startup native-word policy.
    }
    const auto& relation = f.startSpeedRelation;
    if (!Probe::sameTarget(relation.target, f.axis.target) ||
        relation.configurationGeneration != f.axis.generation || !f.axis.generation ||
        !motionProfileBound(f) || !profile.ok || profile.pending || !profile.closureQualified ||
        !MotorControlRS::evidenceAgeValid(profile.closureEarliestUs, now, f.maximumAgeUs) ||
        relation.rawStartSpeed != profile.current[0] || relation.qualifiedUs > now ||
        !MotorControlRS::evidenceAgeValid(relation.qualifiedUs, now, f.maximumAgeUs)) return false;
    minimumRpm = relation.minimumPositionSpeedRpm; return true;
}
bool positionStartingSpeed(const App& a, uint64_t now, uint16_t& minimumRpm) {
    return positionStartingSpeed(selectedFacts(a), now, minimumRpm);
}
bool motionProfileStationary(const MotionFacts& f, uint64_t now) {
    const auto& motion = f.block(ESS::StateBlock::MOTION);
    return Probe::fresh(motion, f.axis.target, now, f.maximumAgeUs) && !motion.value.rawAlarm &&
        !motion.value.alarmFlag && !motion.value.running &&
        !motion.value.positiveSoftLimit && !motion.value.negativeSoftLimit;
}
bool motionProfileStationary(const App& a, uint64_t now) { return motionProfileStationary(selectedFacts(a), now); }
// Native absolute targets need no manufactured current-position reference.
// Core preparation still enforces caller limits, units, origin and wire range;
// the application requires fresh stopped feedback before starting the move.
bool absoluteMoveReady(const MotionFacts& f, const MotorControlRS::MoveRequest& request, uint64_t now) {
    if (!f.reference) return false;
    MotorControlRS::PreparedTarget target;
    if (!MotorControlRS::preparePosition(request.position, f.axis,
            f.reference->nativeKnown ? f.reference : nullptr, target)) return false;
    const auto& feedback = f.block(ESS::StateBlock::FEEDBACK);
    return !request.position.relative && motionProfileBound(f) && motionProfileStationary(f, now) &&
        Probe::fresh(feedback, f.axis.target, now, f.maximumAgeUs) && feedback.value.pairKnown &&
        !feedback.value.rawSpeed;
}
// Selected-axis form kept for its direct fixtures; inline needs no production caller.
inline bool absoluteMoveReady(const App& a, const MotorControlRS::MoveRequest& request, uint64_t now) {
    const auto reference = axisReference(a);
    return absoluteMoveReady(selectedFacts(a, &reference), request, now);
}
/** Name of the first moveRequirements check that refuses, or nullptr when all
 * pass (wired bench 19: group refusals named no cause). Same order and terms. */
const char* moveReadinessReason(const MotionFacts& f, const MotorControlRS::MoveRequest& request, uint64_t now) {
    using namespace MotorControlRS;
    const auto& profile = f.profile.view;
    if (!motionProfileBound(f)) return "profile_unbound";
    if (!profile.ok || profile.pending || !profile.closureQualified ||
        !MotorControlRS::evidenceAgeValid(profile.closureEarliestUs, now, f.maximumAgeUs)) return "profile_stale";
    if (request.position.wrapped || request.position.basis != RelativeBasis::ACTUAL) return "request_basis";
    if (!motionProfileStationary(f, now)) return "motion_not_stationary";
    const auto& io = f.block(ESS::StateBlock::IO);
    if (!Probe::fresh(io, f.axis.target, now, f.maximumAgeUs) || io.value.unknownInputBits) return "io_stale";
    if (f.configuration.raw.softLimitEnable != 0 && !f.softLimitBound) return "soft_limit_unbound";
    if (!f.configuration.wordOrderKnown) return "word_order_unknown";
    if (f.block(ESS::StateBlock::MOTION).value.released) return "released";
    uint16_t minimumRpm = 0;
    if (!positionStartingSpeed(f, now, minimumRpm)) return "start_speed_relation";
    if (minimumRpm > request.speedRpm) return "start_speed_above_request";
    if (profile.current[1] > 2000 || profile.current[2] > 2000) return "ramp_out_of_range";
    if (stopInputs(f, now).result != StopInputs::CLEAR) return "stop_input";
    if (limitInputs(f, now).result != LimitInputs::CLEAR) return "limit_input";
    if (!moveInputPolicy(f, now)) return "input_policy";
    if (!request.position.relative && !absoluteMoveReady(f, request, now)) return "absolute_not_ready";
    return nullptr;
}
bool moveRequirements(const MotionFacts& f, const MotorControlRS::MoveRequest& request,
                      uint64_t now, ESS::MovePrerequisites& out) {
    using namespace MotorControlRS;
    const auto& profile = f.profile.view;
    if (!motionProfileBound(f) || !profile.ok || profile.pending || !profile.closureQualified ||
        !MotorControlRS::evidenceAgeValid(profile.closureEarliestUs, now, f.maximumAgeUs) ||
        request.position.wrapped ||
        request.position.basis != RelativeBasis::ACTUAL || !motionProfileStationary(f, now)) return false;
    const auto& raw = f.configuration.raw;
    const auto& io = f.block(ESS::StateBlock::IO);
    const auto& motion = f.block(ESS::StateBlock::MOTION);
    uint16_t minimumRpm = 0;
    if (!Probe::fresh(io, f.axis.target, now, f.maximumAgeUs) || io.value.unknownInputBits ||
        (raw.softLimitEnable != 0 && !f.softLimitBound) || !f.configuration.wordOrderKnown || motion.value.released ||
        !positionStartingSpeed(f, now, minimumRpm) || minimumRpm > request.speedRpm ||
        profile.current[1] > 2000 || profile.current[2] > 2000)
        return false;
    // Unconnected wiring does not establish inactive assignments. Preserve them
    // and require the checked logical input report for each assigned input.
    // Origin level alone does not trigger a serial move or act as a limit.
    const StopInputCheck stops = stopInputs(f, now);
    const LimitInputCheck limits = limitInputs(f, now);
    if (stops.result != StopInputs::CLEAR || limits.result != LimitInputs::CLEAR || !moveInputPolicy(f, now)) return false;
    if (!request.position.relative && !absoluteMoveReady(f, request, now)) return false;

    // Publish only after every guard passes; rejected preparation preserves out.
    // prepareMove performs the shared unit conversion and signed/range checks.
    // These facts qualify native command words, not the caller's physical scales.
    out = ESS::MovePrerequisites();
    out.target = f.axis.target;
    out.configurationGeneration = f.axis.generation;
    out.commandUnitsVerified = out.relativeBasisVerified = true;
    // Bench, 3 November 2026, firmware 0x0029: two's-complement targets moved back
    // by the commanded amount, and feedback below zero read back in the same encoding.
    out.negativeTwosComplementVerified = true;
    out.configuredRampVerified = out.serialInputsPermit = out.readinessQualified = true;
    out.accelerationTime = profile.current[1];
    out.decelerationTime = profile.current[2];
    out.startSpeedKnown = true;
    out.startSpeed = minimumRpm;
    out.maximumAgeUs = f.maximumAgeUs;
    out.stopInputMask = stops.mask; out.stopInputActiveLow = stops.activeLow;
    out.limitInputMask = limits.witness; out.limitInputActiveLow = limits.activeLow;
    return true;
}
bool moveRequirements(const App& a, const MotorControlRS::MoveRequest& request,
                      uint64_t now, ESS::MovePrerequisites& out) {
    // The selected axis supplies its coordinate reference for absolute targets.
    MotorControlRS::AxisReference reference;
    if (!request.position.relative) reference = axisReference(a);
    return moveRequirements(selectedFacts(a, request.position.relative ? nullptr : &reference), request, now, out);
}
