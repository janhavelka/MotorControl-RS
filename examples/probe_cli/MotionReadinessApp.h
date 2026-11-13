// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.
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
    MotionFacts(const MotorControlRS::AxisConfig& axisConfig, const ESS::ConfigObservation& config,
                const App::MotionProfileState& motionProfile, const Probe::StateCache& cache,
                bool relationRequired, const PositionStartSpeedRelation& relation,
                uint32_t binding, uint32_t serial, uint64_t maximumAge,
                const MotorControlRS::AxisReference* axisReference = nullptr)
        : axis(axisConfig), configuration(config), profile(motionProfile), state(cache),
          startSpeedRelationRequired(relationRequired), startSpeedRelation(relation),
          bindingGeneration(binding), serialGeneration(serial), maximumAgeUs(maximumAge), reference(axisReference) {}
    const Probe::StateCache::Block& block(ESS::StateBlock kind) const { return state.blocks[static_cast<uint8_t>(kind)]; }
};
MotionFacts selectedFacts(const App& a, const MotorControlRS::AxisReference* reference = nullptr) {
    return MotionFacts(a.axis, a.configuration, a.motionProfile, a.stateCache, a.positionStartSpeedRelationRequired,
        a.positionStartSpeedRelation, a.bindingGeneration, a.serial.generation, a.observationAgeUs(), reference);
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
void revokePositionStartSpeedRelation(App& a) {
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
        raw.softLimitEnable != 0 || !f.configuration.wordOrderKnown || motion.value.released ||
        !positionStartingSpeed(f, now, minimumRpm) || minimumRpm > request.speedRpm ||
        profile.current[1] > 2000 || profile.current[2] > 2000)
        return false;
    // Unconnected wiring does not establish inactive assignments. Preserve them
    // and require the checked logical input report for each assigned input.
    for (uint8_t i = 0; i < 4; ++i)
        if (raw.inputFunctions[i] > 3 || (raw.inputFunctions[i] && io.value.inputs[i])) return false;
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
    return true;
}
bool moveRequirements(const App& a, const MotorControlRS::MoveRequest& request,
                      uint64_t now, ESS::MovePrerequisites& out) {
    // The selected axis supplies its coordinate reference for absolute targets.
    MotorControlRS::AxisReference reference;
    if (!request.position.relative) reference = axisReference(a);
    return moveRequirements(selectedFacts(a, request.position.relative ? nullptr : &reference), request, now, out);
}
