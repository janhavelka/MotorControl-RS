// SPDX-License-Identifier: MIT
// Included inside the standalone application's namespace. Console routes that turn
// stated facts into the install/revoke API calls. Admission stays in those calls and
// in the operations themselves; the checks here only name what the operator lacks.
bool qualificationIdle(const App& a) {
    return !(a.owner.active() || a.owner.pending() || a.owner.recovering() || a.owner.configurationOwned() ||
        a.owner.commissioningOwned() || a.discovery.owned || a.serial.configuring || a.serial.blocked ||
        axisReserved(a) || reading(a) || acting(a));
}
Probe::QualificationReply refusal(Probe::Action action, const char* reason = nullptr, const char* readFirst = nullptr) {
    Probe::QualificationReply reply(action);
    reply.reason = reason; reply.readFirst = readFirst;
    return reply;
}
Probe::QualificationReply needRead(const char* command) {
    return refusal(Probe::Action::UNAVAILABLE, "reads_required", command);
}
Probe::QualificationReply statusReply(const MotorControlRS::Status& status) {
    if (status) return Probe::QualificationReply(Probe::Action::OK);
    return refusal(status.detail == static_cast<int32_t>(Probe::Action::BUSY) ? Probe::Action::BUSY : Probe::Action::UNAVAILABLE,
        status.detail == static_cast<int32_t>(Probe::Action::BUSY) ? nullptr : "qualification_rejected");
}
// The first ordinary read homeReadiness still needs, in the order they are taken.
// A state read is bound to the configuration read before it.
const char* homeFirstRead(const App& a, uint64_t now) {
    using MotorControlRS::evidenceAgeValid;
    const auto& config = a.configuration;
    const uint64_t age = a.observationAgeUs();
    const auto checked = [&](const ESS::ReadStepObservation& e) {
        return e.qualified && e.status && e.latestUs <= now && evidenceAgeValid(e.attemptedUs, now, age);
    };
    bool configured = config.operationId && Probe::sameTarget(config.target, a.axis.target);
    for (uint8_t i = 0; configured && i < 4; ++i)
        configured = config.inputFunctionKnown[i] && config.wiring[i] == a.inputWiring[i];
    for (const auto& e : config.provenance) configured = configured && checked(e);
    if (!configured) return "read config";
    const auto& motion = a.stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    const auto& io = a.stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    if (!Probe::current(motion, a.axis.target) || !Probe::current(io, a.axis.target) ||
        motion.value.configOperationId != config.operationId || io.value.configOperationId != config.operationId ||
        !evidenceAgeValid(motion.observedEarliestUs, now, age) || !evidenceAgeValid(io.observedEarliestUs, now, age))
        return "read state";
    const auto& parameters = a.homingParameters;
    if (!parameters.operationId || parameters.configurationGeneration != a.axis.generation ||
        !Probe::sameTarget(parameters.target, a.axis.target) || !checked(parameters.provenance))
        return "home read";
    return nullptr;
}
Probe::QualificationReply installHomeFacts(const App& a, const Probe::QualificationCommand& c) {
    using namespace MotorControlRS;
    if (!a.axis.generation) return refusal(Probe::Action::UNAVAILABLE, "axis_configuration_unknown");
    const uint64_t now = uart.sample();
    if (const char* read = homeFirstRead(a, now)) return needRead(read);
    const auto& parameters = a.homingParameters;
    // homeReadiness refuses these; name them, because no read will change them.
    if (parameters.rawAuxiliary != static_cast<uint16_t>(ESS::HomingAuxiliary::KEEP_POSITION_SET_ZERO))
        return refusal(Probe::Action::UNAVAILABLE, "stored_auxiliary_differs");
    if (parameters.rawOffsetWords[0] || parameters.rawOffsetWords[1])
        return refusal(Probe::Action::UNAVAILABLE, "stored_offset_nonzero");
    const auto& motion = a.stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    if (motion.value.rawAlarm || (motion.value.rawMotion & 0x7C))
        return refusal(Probe::Action::UNAVAILABLE, "drive_not_ready", "read state");
    ESS::HomePrerequisites p;
    p.target = a.axis.target; p.configurationGeneration = a.axis.generation;
    p.qualifiedMethod = c.home.method; p.qualifiedSearchSpeed = c.home.searchSpeed;
    p.qualifiedReturnSpeed = c.home.returnSpeed; p.qualifiedRampTime = c.home.rampTime;
    p.methodQualified = c.mechanics; p.nativeRatesQualified = c.rates; p.nativeRampQualified = c.ramp;
    p.zeroOffsetQualified = c.zeroOffset; p.auxiliaryQualified = c.auxiliary; p.inputsQualified = c.inputs;
    p.indexQualified = c.index; p.referenceSemanticsQualified = c.reference;
    p.readinessQualified = true; // Stands until invalidated; live reads decide each admission.
    p.qualifiedUs = now; p.qualificationMaximumAgeUs = 0; // Stated facts have no age; changes revoke them.
    p.maximumAgeUs = a.observationAgeUs();
    p.inputPolarity = a.configuration.raw.inputPolarity;
    for (uint8_t i = 0; i < 4; ++i) {
        const uint16_t function = a.configuration.raw.inputFunctions[i];
        p.inputFunctions[i] = function; p.inputWiring[i] = a.inputWiring[i];
        if (function && function <= 3 && a.inputWiring[i] == InputWiring::CONNECTED)
            p.availableInputs |= static_cast<uint8_t>(1U << (function - 1));
    }
    return statusReply(installHomeQualification(p));
}
Probe::QualificationReply installVelocityFacts(const App& a, const Probe::QualificationCommand& c) {
    if (!a.axis.generation) return refusal(Probe::Action::UNAVAILABLE, "axis_configuration_unknown");
    if (!a.configuration.operationId || !Probe::sameTarget(a.configuration.target, a.axis.target)) return needRead("read config");
    const auto& observed = a.velocityParameters;
    if (!observed.operationId || observed.configurationGeneration != a.axis.generation ||
        !Probe::sameTarget(observed.target, a.axis.target)) return needRead("velocity read");
    if (observed.rawAccelerationTime > 2000 || observed.rawDecelerationTime > 2000)
        return refusal(Probe::Action::UNAVAILABLE, "stored_ramp_out_of_range");
    ESS::VelocityPrerequisites p;
    p.target = a.axis.target; p.configurationGeneration = a.axis.generation;
    p.nativeRpmVerified = c.rpm; p.negativeTwosComplementVerified = c.negative;
    p.configuredRampVerified = c.ramp; p.serialInputsPermit = c.inputs;
    p.readinessQualified = true; // Stands until invalidated; each admission needs fresh state.
    p.accelerationTime = observed.rawAccelerationTime; p.decelerationTime = observed.rawDecelerationTime;
    p.maximumAgeUs = a.observationAgeUs();
    return statusReply(installVelocityQualification(p));
}
Probe::QualificationReply installStartSpeedRelation(const App& a, const Probe::QualificationCommand& c) {
    if (!a.axis.generation) return refusal(Probe::Action::UNAVAILABLE, "axis_configuration_unknown");
    if (!a.configuration.operationId || !Probe::sameTarget(a.configuration.target, a.axis.target)) return needRead("read config");
    const uint64_t now = uart.sample();
    const auto& profile = a.motionProfile.view;
    if (!motionProfileBound(a) || !profile.ok || profile.pending || !profile.closureQualified ||
        !MotorControlRS::evidenceAgeValid(profile.closureEarliestUs, now, a.observationAgeUs()))
        return needRead("motion-profile read");
    if (profile.current[0] != c.rawStartSpeed) return refusal(Probe::Action::UNAVAILABLE, "stored_word_differs");
    PositionStartSpeedRelation relation;
    relation.target = a.axis.target; relation.configurationGeneration = a.axis.generation;
    relation.rawStartSpeed = c.rawStartSpeed; relation.minimumPositionSpeedRpm = c.minimumRpm;
    relation.qualifiedUs = now;
    return statusReply(installPositionStartSpeedRelation(relation));
}
Probe::QualificationReply qualificationCommand(void* context, const Probe::QualificationCommand& c) {
    const App& a = *static_cast<App*>(context);
    if (c.revoke) {
        if (c.route == Probe::QualificationRoute::HOME) return statusReply(revokeHomeQualification());
        if (c.route == Probe::QualificationRoute::VELOCITY) return statusReply(revokeVelocityQualification());
        return statusReply(revokePositionClearQualification());
    }
    if (axisReserved(a)) return refusal(Probe::Action::AXIS_CONFLICT);
    if (!qualificationIdle(a)) return refusal(Probe::Action::BUSY);
    switch (c.route) {
    case Probe::QualificationRoute::HOME: return installHomeFacts(a, c);
    case Probe::QualificationRoute::VELOCITY: return installVelocityFacts(a, c);
    case Probe::QualificationRoute::POSITION_CLEAR: return statusReply(installPositionClearQualification());
    case Probe::QualificationRoute::START_SPEED_RELATION: return installStartSpeedRelation(a, c);
    }
    return refusal(Probe::Action::FAILED);
}
