// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.
// Stage one finite move without a start: one FC10 write of the five 0x0021 words
// (acceleration, deceleration, speed, target pair) and one FC03 readback, through
// the ordinary owner. No start word and no 0x0020 write. A matching readback is
// the stored-word witness a USE_STORED start relies on; anything that may write
// those words ends the witness (storedWordsChanged). Nothing is resent.

constexpr uint16_t STAGE_FIRST = ESS::Registers::POSITION_ACCELERATION_TIME;
constexpr uint16_t STAGE_WORDS = 5;

Probe::Action submitStageFrame(App& a, App::StageSession& s, bool write, uint64_t now) {
    const auto& v = s.view;
    Rtu::BusRequest request;
    request.wire.length = write ?
        ESS::buildWriteMultipleRegisters(v.target.address, STAGE_FIRST, v.words, STAGE_WORDS, s.tx, sizeof(s.tx)) :
        ESS::buildReadRegisters(v.target.address, STAGE_FIRST, STAGE_WORDS, s.tx, sizeof(s.tx));
    if (!request.wire.length) return Probe::Action::INVALID;
    request.wire.bytes = s.tx;
    request.wire.replyLength = write ? ESS::WRITE_RESPONSE_LEN : ESS::expectedReadRegistersLen(STAGE_WORDS);
    request.wire.responseTimeoutUs = a.serial.timing.responseTimeoutUs; request.wire.replyGapUs = a.serial.timing.replyGapUs;
    request.wire.deadlineUs = v.deadlineUs;
    request.expected.address = v.target.address; request.expected.target = v.target.id;
    request.expected.targetGeneration = v.target.generation;
    request.expected.function = write ? 16 : 3; request.expected.first = STAGE_FIRST; request.expected.count = STAGE_WORDS;
    request.validator = Rtu::essValidator();
    return admissionResult(a.owner.admit(request, now, s.request));
}
void finishStage(App::StageSession& s, bool staged, const MotorControlRS::Status& status, uint64_t now) {
    s.view.pending = false; s.view.staged = staged; s.view.status = status;
    s.view.finishedUs = now; s.phase = 0;
}
// Admit the write. Callers have checked readiness and prepared the words with the
// shared move rules; this checks only the wire, binding and owner.
Probe::Action beginPositionStage(App& a, App::StageSession& s, const MotorControlRS::ReadTarget& target,
                                 uint32_t configurationGeneration, const uint16_t* words, uint32_t operationId, uint64_t now) {
    if (s.view.pending) return Probe::Action::BUSY;
    if (!platformReady) return Probe::Action::UNAVAILABLE;
    if (a.serial.blocked || a.serial.configuring || a.owner.configurationOwned() ||
        a.owner.needsRecovery() || uart.needsRecovery()) return Probe::Action::RECOVERY_REQUIRED;
    if (!(a.knownTargets[target.address / 8] & (1U << (target.address % 8))) ||
        target.generation != a.bindingGeneration) return Probe::Action::UNAVAILABLE;
    if (!ESS::validateWriteMultipleRegistersRequest(target.address, STAGE_FIRST, words, STAGE_WORDS)) return Probe::Action::INVALID;
    s.~StageSession(); new (&s) App::StageSession();
    auto& v = s.view;
    v.target = target; v.operationId = operationId; v.configurationGeneration = configurationGeneration;
    v.serialGeneration = a.serial.generation;
    std::memcpy(v.words, words, sizeof(v.words));
    v.startedUs = now; v.deadlineUs = now + 2 * REQUEST_US;
    s.phase = 1;
    const auto admitted = submitStageFrame(a, s, true, now);
    if (admitted != Probe::Action::OK) { s.~StageSession(); new (&s) App::StageSession(); return admitted; }
    v.pending = true;
    return Probe::Action::OK;
}
void servicePositionStage(App& a, App::StageSession& s, uint64_t now) {
    using namespace MotorControlRS;
    auto& v = s.view;
    if (!v.pending) return;
    const bool stale = v.target.generation != a.bindingGeneration || v.serialGeneration != a.serial.generation;
    if (s.request.owner) {
        const std::size_t accepted = a.owner.txAccepted(s.request);
        // From the first accepted byte the write may apply: no older witness survives it.
        if (s.phase == 1 && accepted && v.execution == ActionExecution::NOT_TRANSMITTED) {
            v.execution = ActionExecution::UNKNOWN;
            storedWordsChanged(a, v.target.address);
        }
        if ((v.cancelled || stale) && !accepted) a.owner.cancelUnsent(s.request, now);
        const auto* result = a.owner.result(s.request);
        if (!result) return;
        const auto& t = result->transport;
        const bool checked = result->outcome == Rtu::Outcome::SUCCESS && t.reason == Rtu::Reason::FRAME &&
            t.closureQualified && t.txComplete;
        if (s.phase == 1) {
            v.txComplete = t.txComplete; v.txEndUs = t.txEndUs; v.txUncertaintyUs = t.txUncertaintyUs;
            v.writeQualified = t.closureQualified; v.writeLatestUs = t.closureLatestUs;
            v.execution = checked ? ActionExecution::ACKNOWLEDGED : result->outcome == Rtu::Outcome::DEVICE_REJECTED ?
                ActionExecution::REJECTED : t.txAccepted ? ActionExecution::UNKNOWN : ActionExecution::NOT_TRANSMITTED;
            if (t.txAccepted) storedWordsChanged(a, v.target.address);
        } else {
            v.readQualified = t.closureQualified; v.readEarliestUs = t.closureEarliestUs; v.readLatestUs = t.closureLatestUs;
            std::size_t count = 0;
            v.readbackKnown = checked && ESS::parseRegisters(result->raw, t.rxLength, v.target.address, STAGE_WORDS,
                v.readback, STAGE_WORDS, count).isOk() && count == STAGE_WORDS;
        }
        a.owner.release(s.request); s.request = Rtu::RequestId();
        if (s.phase == 2) {
            const bool match = v.readbackKnown && std::memcmp(v.readback, v.words, sizeof(v.words)) == 0;
            finishStage(s, match, match ? Ok() : Status(v.readbackKnown ? Err::ILLEGAL_VALUE : Err::FRAME_ERROR, 0,
                v.readbackKnown ? "staged words differ on readback" : "stage readback failed"), now);
            return;
        }
        if (v.execution != ActionExecution::ACKNOWLEDGED) {
            finishStage(s, false, Status(result->outcome == Rtu::Outcome::DEVICE_REJECTED ? Err::EXCEPTION : Err::FRAME_ERROR,
                static_cast<int32_t>(result->outcome), "stage write not acknowledged"), now);
            return;
        }
        s.phase = 2;
    }
    if (v.cancelled || stale) {
        v.cancelled = true;
        finishStage(s, false, Status(Err::INVALID_CONFIG, 0, "stage cancelled before its readback"), now);
        return;
    }
    if (now >= v.deadlineUs) { finishStage(s, false, Status(Err::INVALID_CONFIG, 0, "stage deadline expired"), now); return; }
    if (s.phase != 2 || s.request.owner || a.owner.needsRecovery() || uart.needsRecovery()) return;
    const auto admitted = submitStageFrame(a, s, false, now);
    if (admitted != Probe::Action::OK && admitted != Probe::Action::QUEUE_FULL && admitted != Probe::Action::RESULTS_FULL)
        finishStage(s, false, Status(Err::INVALID_CONFIG, static_cast<int32_t>(admitted), "stage readback not admitted"), now);
}
// A stop on this address ends unsent staging; a frame on the wire settles first.
void cancelPositionStages(App& a, uint8_t address, uint64_t now) {
    const auto cancel = [&](App::StageSession& s) {
        if (!s.view.pending || s.view.target.address != address) return;
        s.view.cancelled = true;
        if (s.request.owner && !a.owner.txAccepted(s.request)) a.owner.cancelUnsent(s.request, now);
    };
    cancel(a.stage);
    for (uint8_t i = 0; i < a.group.count; ++i) cancel(a.group.members[i].stage);
}
// Selected axis: the same readiness and preparation as an ordinary move, then the
// stage. The prepared context stays in App scratch storage, never on the stack.
Probe::Action startSelectedStage(App& a, const MotorControlRS::MoveRequest& supplied, uint32_t& operationId) {
    using namespace MotorControlRS;
    const uint8_t address = a.axis.target.address;
    if (a.owner.commissioningOwned() || a.discovery.owned || groupSequencing(a)) return Probe::Action::BUSY;
    if (!platformReady) return Probe::Action::UNAVAILABLE;
    if (a.serial.blocked || a.serial.configuring || a.owner.configurationOwned() ||
        a.owner.needsRecovery() || uart.needsRecovery()) return Probe::Action::RECOVERY_REQUIRED;
    if (!supplied.speedRpm || supplied.ramp != MoveRamp::VERIFIED_CONFIGURED || supplied.setup != MoveSetup::WRITE_ALL)
        return Probe::Action::INVALID;
    if (!(a.knownTargets[address / 8] & (1U << (address % 8)))) return Probe::Action::UNAVAILABLE;
    if (!a.commandPolarityKnown && supplied.position.frame != CoordinateFrame::NATIVE) return Probe::Action::UNAVAILABLE;
    if (!a.nextOperationId) return Probe::Action::IDS_EXHAUSTED;
    if (axisReserved(a, address)) return Probe::Action::AXIS_CONFLICT;
    if (a.owner.active() || a.owner.pending()) return Probe::Action::BUSY;
    if (!a.configuration.operationId || !Probe::sameTarget(a.configuration.target, a.axis.target))
        return Probe::Action::UNAVAILABLE;
    const uint64_t now = uart.sample();
    const uint64_t deadline = now + static_cast<uint64_t>(a.options.moveTimeoutMs) * 1000;
    const auto prepared = prepareSelectedMove(a, supplied, now, deadline, a.stagePrepared);
    if (prepared != Probe::Action::OK) return prepared;
    const auto admitted = beginPositionStage(a, a.stage, a.axis.target, a.axis.generation, a.stagePrepared.words,
        a.nextOperationId, now);
    if (admitted != Probe::Action::OK) return admitted;
    operationId = a.nextOperationId++;
    return Probe::Action::OK;
}
// Publish a checked stage of the still-bound selected axis as its stored-word witness.
void serviceSelectedStage(App& a, uint64_t now) {
    auto& s = a.stage;
    servicePositionStage(a, s, now);
    const auto& v = s.view;
    if (v.pending || !v.staged || s.applied) return;
    s.applied = true;
    if (!Probe::sameTarget(v.target, a.axis.target) || v.configurationGeneration != a.axis.generation ||
        v.serialGeneration != a.serial.generation) return;
    std::memcpy(a.rememberedMoveWords, v.words, sizeof(a.rememberedMoveWords));
    a.rememberedMoveGeneration = v.configurationGeneration;
    a.rememberedMoveBinding = v.target.generation; a.rememberedMoveSerial = v.serialGeneration;
}
