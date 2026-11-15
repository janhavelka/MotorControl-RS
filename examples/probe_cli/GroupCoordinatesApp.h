// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.
// Host coordinates belong to one member. Checked feedback names an observation;
// only an explicit caller qualification establishes its command-coordinate value.

bool memberCoordinateObservations(const App& a, const App::GroupMember& m, uint64_t now) {
    if (!memberCurrent(a, m)) return false;
    const auto& motion = m.state.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    const auto& feedback = m.state.blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
    const uint64_t age = a.observationAgeUs();
    return Probe::fresh(motion, m.axis.target, now, age) && Probe::fresh(feedback, m.axis.target, now, age) &&
        motion.value.operationId && motion.value.operationId == feedback.value.operationId &&
        motion.value.configOperationId == m.configuration.operationId &&
        feedback.value.configOperationId == m.configuration.operationId &&
        !motion.value.running && !motion.value.released && !motion.value.alarmFlag && !motion.value.rawAlarm &&
        !motion.value.unknownMotionBits && feedback.value.pairKnown && !feedback.value.rawSpeed;
}

MotorControlRS::AxisReference memberReference(const App& a, const App::GroupMember& m, uint64_t now) {
    using namespace MotorControlRS;
    const auto& coordinate = m.coordinate;
    AxisReference reference = coordinate.reference;
    reference.nowUs = now;
    reference.idle = !groupBusy(a) && !axisReserved(a, m.address) &&
        !a.serial.blocked && !a.owner.needsRecovery() && !uart.needsRecovery();
    const auto& motion = m.state.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    const auto& feedback = m.state.blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
    reference.stationary = memberCoordinateObservations(a, m, now);
    const uint64_t observed = std::min(motion.observedEarliestUs, feedback.observedEarliestUs);
    if (!reference.stationary || !reference.nativeKnown || reference.source != ScaleSource::QUALIFIED ||
        reference.basis != RelativeBasis::ACTUAL || !Probe::sameTarget(reference.target, m.axis.target) ||
        reference.configurationGeneration != m.axis.generation || coordinate.declaration != a.group.declaration ||
        coordinate.configurationOperationId != m.configuration.operationId ||
        coordinate.motionOperationId != motion.value.operationId ||
        coordinate.feedbackOperationId != feedback.value.operationId || reference.observedUs > observed ||
        !evidenceAgeValid(reference.observedUs, now, reference.maximumAgeUs))
        reference.nativeKnown = false;
    return reference;
}

Probe::Action installMemberReference(App& a, const Probe::GroupReference& supplied) {
    using namespace MotorControlRS;
    auto* m = groupMember(a, supplied.reference.target.address);
    if (!m || !memberCurrent(a, *m)) return Probe::Action::UNAVAILABLE;
    if (axisReserved(a, m->address)) return Probe::Action::AXIS_CONFLICT;
    if (groupBusy(a)) return Probe::Action::BUSY;
    if (a.serial.blocked || a.owner.needsRecovery() || uart.needsRecovery()) return Probe::Action::RECOVERY_REQUIRED;
    const uint64_t now = uart.sample();
    if (!memberCoordinateObservations(a, *m, now)) return Probe::Action::REFERENCE_NOT_READY;
    const auto& motion = m->state.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    const auto& feedback = m->state.blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
    const auto& reference = supplied.reference;
    if (!reference.nativeKnown || !reference.idle || !reference.stationary || reference.source != ScaleSource::QUALIFIED ||
        reference.basis != RelativeBasis::ACTUAL || !Probe::sameTarget(reference.target, m->axis.target) ||
        reference.configurationGeneration != m->axis.generation || supplied.declaration != a.group.declaration ||
        supplied.configurationOperationId != m->configuration.operationId ||
        supplied.motionOperationId != motion.value.operationId || supplied.feedbackOperationId != feedback.value.operationId ||
        !validateNativePosition(reference.nativePosition, m->axis.nativeMinimum, m->axis.nativeMaximum))
        return Probe::Action::INVALID;
    Probe::GroupReference installed = supplied;
    const uint64_t observed = std::min(motion.observedEarliestUs, feedback.observedEarliestUs);
    if (!installed.reference.observedUs) installed.reference.observedUs = observed;
    if (installed.reference.observedUs > observed ||
        !evidenceAgeValid(installed.reference.observedUs, now, installed.reference.maximumAgeUs))
        return Probe::Action::REFERENCE_NOT_READY;
    installed.reference.nowUs = now;
    m->coordinate = installed;
    return Probe::Action::OK;
}

Probe::Action revokeMemberReference(App& a, uint8_t address) {
    auto* m = groupMember(a, address);
    if (!m) return Probe::Action::UNAVAILABLE;
    if (axisReserved(a, address)) return Probe::Action::AXIS_CONFLICT;
    if (groupBusy(a)) return Probe::Action::BUSY;
    const auto status = MotorControlRS::invalidateAxisReference(m->axis, m->coordinate.reference);
    if (!status) m->axis.generation = 0;
    m->coordinate = Probe::GroupReference();
    m->storedKnown = false;
    m->profile.view.generation = m->axis.generation;
    return status ? Probe::Action::OK : Probe::Action::IDS_EXHAUSTED;
}

MotorControlRS::Status groupAxisCommand(App& a, uint8_t address, const Probe::AxisCommand& command,
                                      Probe::AxisView& view) {
    using namespace MotorControlRS;
    auto* m = groupMember(a, address);
    if (!m) return Status(Err::INVALID_CONFIG, static_cast<int32_t>(Probe::Action::UNAVAILABLE), "group member is absent");
    if (command.kind == Probe::AxisCommandKind::QUERY) {
        view = Probe::AxisView(); view.configuration = m->axis;
        return Ok();
    }
    if (command.kind > Probe::AxisCommandKind::PREPARE)
        return Status(Err::INVALID_CONFIG, 0, "invalid member axis command");
    if (groupBusy(a) || axisReserved(a, address))
        return Status(Err::INVALID_CONFIG, static_cast<int32_t>(Probe::Action::BUSY), "member axis requires idle owner");
    if (a.serial.blocked || a.owner.needsRecovery() || uart.needsRecovery())
        return Status(Err::INVALID_CONFIG, static_cast<int32_t>(Probe::Action::RECOVERY_REQUIRED), "member axis requires settled transport");
    const uint64_t now = uart.sample();
    if (!memberCoordinateObservations(a, *m, now))
        return Status(Err::INVALID_CONFIG, static_cast<int32_t>(Probe::Action::REFERENCE_NOT_READY), "member needs current stationary observations");
    AxisReference reference = memberReference(a, *m, now);
    if (command.kind == Probe::AxisCommandKind::PREPARE) {
        PositionRequest request = command.position;
        request.configurationGeneration = m->axis.generation;
        PreparedTarget prepared;
        const auto status = preparePosition(request, m->axis, reference.nativeKnown ? &reference : nullptr, prepared);
        if (status) {
            view = Probe::AxisView(); view.configuration = m->axis; view.prepared = prepared;
        }
        return status;
    }
    if (command.kind == Probe::AxisCommandKind::ORIGIN) {
        if (command.value.denominator != 1)
            return Status(Err::INVALID_CONFIG, 0, "member origin must be a native integer");
        const auto status = setAxisOrigin(m->axis, command.value.numerator, reference);
        if (!status) return status;
        m->coordinate.reference = reference;
        m->coordinate.reference.configurationGeneration = m->axis.generation;
    } else {
        AxisConfig candidate = m->axis;
        const auto applied = applyAxisConfiguration(candidate, command);
        if (!applied) return applied;
        if (!reference.nativeKnown) {
            // Stationary readback permits host configuration, never an origin.
            const auto& motion = m->state.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
            const auto& feedback = m->state.blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
            reference = AxisReference();
            reference.target = m->axis.target; reference.configurationGeneration = m->axis.generation;
            reference.idle = reference.stationary = true; reference.source = ScaleSource::READBACK;
            reference.observedUs = std::min(motion.observedEarliestUs, feedback.observedEarliestUs);
            reference.nowUs = now; reference.maximumAgeUs = a.observationAgeUs();
        }
        AxisReference retained;
        const auto status = configureAxis(m->axis, candidate, reference, &retained);
        if (!status) return status;
        if (retained.nativeKnown) m->coordinate.reference = retained;
        else m->coordinate = Probe::GroupReference();
    }
    // Host interpretation changed. The drive profile bytes are still the same;
    // prepared intents and stored-word witnesses keep their earlier generations.
    m->profile.view.generation = m->axis.generation;
    m->storedKnown = false;
    view = Probe::AxisView(); view.configuration = m->axis;
    return Ok();
}
