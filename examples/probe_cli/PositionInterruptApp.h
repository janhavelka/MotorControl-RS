// SPDX-License-Identifier: MIT
// Private application handoff. The owner settles every existing transaction;
// this reserved frontend result holds the replacement without queuing its TX.
void captureStoredPosition(App& a, App::Record& record, uint64_t now) {
    using namespace MotorControlRS;
    const auto& move = record.move;
    const auto& request = move.request.position;
    ESS::MovePrerequisites checked;
    if (a.simple.admitting || request.unit != PositionUnit::STEPS || request.frame != CoordinateFrame::NATIVE ||
        request.wrapped || request.basis != RelativeBasis::ACTUAL || move.reference.nativeKnown || a.axis.softLimitsKnown ||
        !moveRequirements(a, move.request, now, checked)) return;
    auto& stored = record.storedPosition;
    stored.target = move.target; stored.configurationGeneration = move.prepared.configurationGeneration;
    stored.originalOperationId = record.operationId; stored.subdivision = move.prerequisites.subdivision;
    stored.wordOrder = move.prerequisites.wordOrder; stored.relative = request.relative;
    stored.profile.startSpeed = a.motionProfile.view.current[0];
    stored.profile.accelerationTime = move.words[0]; stored.profile.decelerationTime = move.words[1];
    stored.profile.speed = move.words[2];
    ESS::decodeUint32(move.words + 3, 2, stored.wordOrder, stored.profile.targetBits);
    const auto& io = a.stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    stored.qualifiedUs = std::min(move.prerequisites.observedUs,
        std::min(a.motionProfile.view.closureEarliestUs, io.observedEarliestUs));
    stored.maximumAgeUs = move.prerequisites.maximumAgeUs;
    const uint64_t appAge = a.observationAgeUs();
    if (appAge && (!stored.maximumAgeUs || appAge < stored.maximumAgeUs)) stored.maximumAgeUs = appAge;
    // Five original words become qualified only after setup/read verification
    // and the original checked trigger. No new candidate supplies this evidence.
}
bool storedPositionCurrent(const App& a, const ESS::StoredPositionQualification& stored, uint64_t now) {
    if (!stored.qualified || !a.axis.generation || stored.configurationGeneration != a.axis.generation ||
        !Probe::sameTarget(stored.target, a.axis.target) || stored.target.generation != a.bindingGeneration ||
        !MotorControlRS::evidenceAgeValid(stored.qualifiedUs, now, stored.maximumAgeUs) ||
        !motionProfileBound(a) || !a.motionProfile.view.ok || a.motionProfile.view.pending ||
        a.motionProfile.view.current[0] != stored.profile.startSpeed || a.axis.softLimitsKnown ||
        !a.configuration.wordOrderKnown || a.configuration.wordOrder != stored.wordOrder ||
        a.configuration.raw.subdivision != stored.subdivision || a.configuration.raw.softLimitEnable)
        return false;
    // Own trigger invalidates cached stationary evidence, but does not erase
    // later contradictory checked facts. Moving is allowed; faults are not.
    const auto& motion = a.stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    if (motion.valid && Probe::sameTarget(motion.value.target, stored.target) &&
        motion.observedLatestUs >= stored.qualifiedUs &&
        (motion.value.rawAlarm || motion.value.alarmFlag || motion.value.released ||
         motion.value.positiveSoftLimit || motion.value.negativeSoftLimit)) return false;
    const auto& io = a.stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    if (io.valid && Probe::sameTarget(io.value.target, stored.target) && io.observedLatestUs >= stored.qualifiedUs) {
        if (io.value.unknownInputBits) return false;
        for (uint8_t i = 0; i < 4; ++i)
            if (a.configuration.raw.inputFunctions[i] > 3 ||
                (a.configuration.raw.inputFunctions[i] && io.value.inputs[i])) return false;
    }
    // A later full profile read can contradict the admitted setup. The earlier
    // pre-staging snapshot is retained history and need not match the new words.
    const auto* original = findRecord(a, stored.originalOperationId);
    if (!original) return false;
    if (a.motionProfile.view.closureEarliestUs > original->move.triggerEvidence.latestUs) {
        uint32_t bits = 0;
        if (!ESS::decodeUint32(a.motionProfile.view.current + 4, 2, stored.wordOrder, bits) ||
            bits != stored.profile.targetBits || a.motionProfile.view.current[1] != stored.profile.accelerationTime ||
            a.motionProfile.view.current[2] != stored.profile.decelerationTime ||
            a.motionProfile.view.current[3] != stored.profile.speed) return false;
    }
    return true;
}
bool pendingStop(const App& a, uint8_t address) {
    for (const auto& record : a.records)
        if (record.kind == App::OperationKind::ACTION && record.address == address &&
            record.action.request.kind == MotorControlRS::ActionKind::STOP && !terminal(a, record)) return true;
    return false;
}
Probe::Action admitPositionInterrupt(App& a, uint32_t commandId, uint32_t originalId,
        const ESS::PositionCommand& supplied, uint32_t bits, bool relative, uint32_t& operationId) {
    using namespace MotorControlRS;
    if (!platformReady) return Probe::Action::UNAVAILABLE;
    if (a.owner.commissioningOwned() || a.discovery.owned) return Probe::Action::BUSY;
    if (!a.serial.activeKnown || a.serial.blocked || a.serial.configuring || a.owner.configurationOwned() ||
        a.owner.needsRecovery() || uart.needsRecovery()) return Probe::Action::RECOVERY_REQUIRED;
    if (!a.nextOperationId) return Probe::Action::IDS_EXHAUSTED;
    auto* original = findRecord(a, originalId);
    if (!original || original->kind != App::OperationKind::MOVE || original->simpleChild) return Probe::Action::INVALID;
    if (terminal(a, *original)) return Probe::Action::ALREADY_TERMINAL;
    if (original->cancelContinuation || original->moveRelation.originalOperationId ||
        pendingStop(a, original->address)) return Probe::Action::BUSY;
    const auto& old = original->move;
    if (old.admission != ESS::MoveAdmission::OBSERVED_STATE || old.execution != ActionExecution::ACKNOWLEDGED ||
        old.step <= old.triggerStep || old.uncertain || !original->axisReserved ||
        original->serialGeneration != a.serial.generation || !writeResponseConfirmed) return Probe::Action::UNAVAILABLE;
    const uint64_t now = uart.sample();
    if (!storedPositionCurrent(a, original->storedPosition, now)) return Probe::Action::UNAVAILABLE;
    if (supplied.interruption != ESS::PositionInterruption::INTERRUPT || supplied.setup != MoveSetup::USE_STORED ||
        supplied.limits.maximumRpm != old.prerequisites.limits.maximumRpm ||
        supplied.limits.maximumCommandRate != old.prerequisites.limits.maximumCommandRate ||
        supplied.limits.minimumRampTime != old.prerequisites.limits.minimumRampTime) return Probe::Action::INVALID;
    App::Record* record = nullptr;
    for (std::size_t i = 0; i < REQUEST_CAPACITY; ++i)
        if (!a.records[i].operationId) { record = &a.records[i]; break; }
    if (!record) return Probe::Action::RESULTS_FULL;
    ESS::PositionCommand command = supplied;
    command.stored = original->storedPosition;
    const uint64_t deadline = now + REQUEST_US;
    const auto prepared = relative ? command.prepareRelative(record->move, a.nextOperationId, bits, now, deadline) :
        command.prepareAbsolute(record->move, a.nextOperationId, bits, now, deadline);
    if (!prepared) { clearRecord(*record); return Probe::Action::INVALID; }
    record->kind = App::OperationKind::MOVE; record->operationId = a.nextOperationId++;
    record->commandId = commandId; record->address = original->address;
    record->serialTuple = original->serialTuple; record->serialGeneration = original->serialGeneration;
    record->deadlineUs = deadline; record->axisReserved = record->interruptWaiting = true;
    record->moveRelation.originalOperationId = originalId;
    record->moveRelation.replacementOperationId = record->operationId;
    original->moveRelation = record->moveRelation;
    // This frontend record reserves the entire terminal outcome across the
    // handoff. The owner must not dispatch new TX before we inspect old results.
    original->cancelContinuation = true;
    if (original->requestId.owner) a.owner.cancelUnsent(original->requestId, now);
    operationId = a.latestOperationId = record->operationId;
    return Probe::Action::OK;
}
Probe::Action startPositionInterrupt(void* context, uint32_t commandId, uint32_t originalId, uint32_t& operationId) {
    App& a = *static_cast<App*>(context);
    const auto* original = findRecord(a, originalId);
    if (!original || original->kind != App::OperationKind::MOVE) return Probe::Action::INVALID;
    const auto& stored = original->storedPosition;
    ESS::PositionCommand command;
    command.target = stored.target; command.configurationGeneration = stored.configurationGeneration;
    command.subdivision = stored.subdivision; command.wordOrder = stored.wordOrder;
    command.limits = original->move.prerequisites.limits;
    command.accelerationTime = stored.profile.accelerationTime;
    command.decelerationTime = stored.profile.decelerationTime; command.speedRpm = stored.profile.speed;
    command.interruption = ESS::PositionInterruption::INTERRUPT; command.setup = MotorControlRS::MoveSetup::USE_STORED;
    return admitPositionInterrupt(a, commandId, originalId, command, stored.profile.targetBits, stored.relative, operationId);
}
bool settlePositionInterrupt(App& a, App::Record& record, uint64_t now) {
    using namespace MotorControlRS;
    auto* original = findRecord(a, record.moveRelation.originalOperationId);
    if (original && terminal(a, *original) && original->move.outcome == ActionOutcome::CANCELLED &&
        !original->interruptedByStop) {
        original->moveRelation.observationSuperseded = record.moveRelation.observationSuperseded = true;
    }
    ESS::PreparedMove work;
    const auto next = ESS::nextMove(record.move, now, work);
    const bool invalid = record.cancelContinuation || !original ||
        record.serialGeneration != a.serial.generation || pendingStop(a, record.address) ||
        !storedPositionCurrent(a, record.move.stored, now) || a.owner.needsRecovery() || uart.needsRecovery();
    if (!next || invalid || (terminal(a, *original) &&
        (original->move.outcome != ActionOutcome::CANCELLED || original->move.execution != ActionExecution::ACKNOWLEDGED))) {
        ESS::advanceMove(record.move, actionEvent(record, !next ? ReadEventKind::DEADLINE : ReadEventKind::CANCEL), now);
        record.interruptWaiting = false;
        return false;
    }
    if (!terminal(a, *original) || original->requestId.owner || a.owner.active() || a.owner.pending()) return false;
    return true;
}
void cancelInvalidPositionInterrupts(App& a, uint64_t now) {
    for (auto& record : a.records) {
        if (record.kind != App::OperationKind::MOVE || terminal(a, record) ||
            record.move.interruption != ESS::PositionInterruption::INTERRUPT || !record.requestId.owner ||
            a.owner.txAccepted(record.requestId)) continue;
        if (record.cancelContinuation || record.serialGeneration != a.serial.generation ||
            !storedPositionCurrent(a, record.move.stored, now) || pendingStop(a, record.address)) {
            record.cancelContinuation = true;
            a.owner.cancelUnsent(record.requestId, now);
        }
    }
}
