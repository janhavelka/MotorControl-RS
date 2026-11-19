// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.
// One explicit write of exact words, then one FC03 readback of the same window,
// through the ordinary owner, on the selected axis (wired bench 06):
// - home restore: 0x0031/6 from an earlier checked home read (method, rates,
//   ramp, raw offset pair);
// - velocity restore: 0x001D/3 from an earlier checked velocity read.
// No start word, no replay and no rollback. A transmitted write whose execution
// is unknown refuses the next restore until a checked read of the same window.
// The wired bench 06 D9 custom-output experiment that shared this session ran
// on image A and was removed with the ledger record (Implement 3).
// The wired bench 10 PT pair experiment (PAIR_EXPERIMENT) shares the session;
// its admission exists only in an image built with MOTORCONTROLRS_WB10_EXPERIMENT
// (PairExperimentApp.h) and every other image refuses it UNSUPPORTED before TX.

constexpr uint16_t HOME_RESTORE_FIRST = 0x0031, VELOCITY_RESTORE_FIRST = 0x001D;
constexpr uint8_t HOME_RESTORE_WORDS = 6, VELOCITY_RESTORE_WORDS = 3;

// Slot 0 keeps the first checked read of the current binding and host tuple:
// the state before this session's first write of the window, which a
// restoration must still find after its own reads (wired bench 06, O7: six
// later reads evicted it from a plain ring). Later reads rotate through the
// other slots. A new binding or tuple replaces slot 0 with its first read.
void rememberParameterSnapshot(App& a, bool home, uint32_t operationId, const MotorControlRS::ReadTarget& target,
                               uint32_t serialGeneration, const uint16_t* words, uint8_t count) {
    auto* ring = home ? a.homeSnapshots : a.velocitySnapshots;
    uint8_t& next = home ? a.homeSnapshotNext : a.velocitySnapshotNext;
    const bool first = !ring[0].operationId || !Probe::sameTarget(ring[0].target, target) ||
        ring[0].serialGeneration != serialGeneration;
    if (next < 1 || next >= PARAMETER_SNAPSHOTS) next = 1;
    auto& s = first ? ring[0] : ring[next];
    if (!first) next = static_cast<uint8_t>(next + 1 < PARAMETER_SNAPSHOTS ? next + 1 : 1);
    s = App::ParameterSnapshot();
    s.operationId = operationId; s.target = target; s.serialGeneration = serialGeneration;
    std::memcpy(s.words, words, count * sizeof(uint16_t));
}
const App::ParameterSnapshot* findParameterSnapshot(const App& a, bool home, uint32_t operationId) {
    const auto* ring = home ? a.homeSnapshots : a.velocitySnapshots;
    for (uint8_t i = 0; i < PARAMETER_SNAPSHOTS; ++i)
        if (operationId && ring[i].operationId == operationId) return &ring[i];
    return nullptr;
}
// A checked read of the session's window that started after the unknown write
// was delivered settles it: the stored words are then known.
void settleParameterWrite(App& a, Probe::ParameterWriteKind kind, uint8_t address, uint64_t attemptedUs) {
    auto& v = a.parameterWrite.view;
    if (v.unsettled && v.kind == kind && v.target.address == address && attemptedUs > v.writeEvidence.deliveredUs)
        v.unsettled = false;
}
// A restore needs no input that would act on the words: function 11 (home
// enable) for the homing words, 8-10 (speed movement, JOG) for the velocity
// words, unless a current CONFIGURE record states triggers inhibited.
Probe::Action restoreInputs(App& a, bool home, uint64_t now) {
    const auto& c = a.configuration;
    if (!c.operationId || !Probe::sameTarget(c.target, a.axis.target)) return Probe::Action::UNAVAILABLE;
    for (const auto& p : c.provenance)
        if (!p.qualified || !p.status || p.latestUs > now ||
            !MotorControlRS::evidenceAgeValid(p.attemptedUs, now, a.observationAgeUs())) return Probe::Action::UNAVAILABLE;
    for (uint8_t i = 0; i < 4; ++i) {
        if (!c.inputFunctionKnown[i]) return Probe::Action::UNAVAILABLE;
        const uint16_t f = c.raw.inputFunctions[i];
        if (!(home ? f == 11 : f >= 8 && f <= 10)) continue;
        const auto& q = a.ioQualifications[i];
        if (q.purpose != IoPurpose::CONFIGURE || !q.input.externalTriggersInhibited || !ioQualificationCurrent(a, q, now))
            return Probe::Action::QUALIFICATION_REQUIRED;
    }
    return Probe::Action::OK;
}
bool pairExperiment(const Probe::ParameterWriteView& v) { return v.kind == Probe::ParameterWriteKind::PAIR_EXPERIMENT; }
Probe::Action submitParameterFrame(App& a, bool write, uint64_t now) {
    auto& s = a.parameterWrite; const auto& v = s.view;
    const bool experiment = pairExperiment(v);
    Rtu::BusRequest request;
    request.wire.length = !write ? ESS::buildReadRegisters(v.target.address, v.first, v.count, s.tx, sizeof(s.tx)) :
        experiment ? ESS::buildExperimentWrite(v.target.address, v.first, v.words, v.count, s.tx, sizeof(s.tx)) :
        ESS::buildWriteMultipleRegisters(v.target.address, v.first, v.words, v.count, s.tx, sizeof(s.tx));
    if (!request.wire.length) return Probe::Action::INVALID;
    request.wire.bytes = s.tx;
    request.wire.replyLength = write ? ESS::WRITE_RESPONSE_LEN : ESS::expectedReadRegistersLen(v.count);
    request.wire.responseTimeoutUs = a.serial.timing.responseTimeoutUs; request.wire.replyGapUs = a.serial.timing.replyGapUs;
    request.wire.deadlineUs = v.deadlineUs;
    request.expected.address = v.target.address; request.expected.target = v.target.id;
    request.expected.targetGeneration = v.target.generation;
    request.expected.function = write ? 16 : 3;
    request.expected.first = v.first; request.expected.count = v.count;
    request.validator = experiment ? Rtu::essExperimentValidator() : Rtu::essValidator();
    return admissionResult(a.owner.admit(request, now, s.request));
}
Probe::Action startParameterWrite(App& a, const Probe::ParameterWriteRequest& r, uint32_t& operationId) {
    using Kind = Probe::ParameterWriteKind;
    auto& s = a.parameterWrite;
    const bool experiment = r.kind == Kind::PAIR_EXPERIMENT;
    if (r.kind != Kind::HOME_RESTORE && r.kind != Kind::VELOCITY_RESTORE && !experiment) return Probe::Action::INVALID;
#if !MOTORCONTROLRS_WB10_EXPERIMENT
    if (experiment) return Probe::Action::UNSUPPORTED; // Wired bench 10 experimental image only.
#endif
    if (s.view.pending || s.view.unsettled) return Probe::Action::BUSY;
    if (!platformReady) return Probe::Action::UNAVAILABLE;
    if (a.serial.blocked || a.serial.configuring || a.owner.configurationOwned() ||
        a.owner.needsRecovery() || uart.needsRecovery()) return Probe::Action::RECOVERY_REQUIRED;
    if (a.owner.active() || a.owner.pending() || a.owner.commissioningOwned() || a.discovery.owned ||
        reading(a) || acting(a) || groupSequencing(a) || a.motionProfile.view.pending ||
        a.monitorState.settings.enabled) return Probe::Action::BUSY;
    const uint8_t address = a.axis.target.address;
    if (!(a.knownTargets[address / 8] & (1U << (address % 8)))) return Probe::Action::UNAVAILABLE;
    if (axisReserved(a, address)) return Probe::Action::AXIS_CONFLICT;
    if (!a.nextOperationId) return Probe::Action::IDS_EXHAUSTED;
    const uint64_t now = uart.sample();
    if (enableSettling(a, address, now)) return Probe::Action::BUSY;
    const auto& feedback = a.stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
    if (!motionProfileStationary(a, now) || !Probe::fresh(feedback, a.axis.target, now, a.observationAgeUs()) ||
        feedback.value.rawSpeed) return Probe::Action::UNAVAILABLE;
    Probe::ParameterWriteView v;
    v.kind = r.kind; v.target = a.axis.target; v.configurationGeneration = a.axis.generation;
    v.serialGeneration = a.serial.generation;
    const bool home = r.kind == Kind::HOME_RESTORE;
    if (experiment) {
#if MOTORCONTROLRS_WB10_EXPERIMENT
        const auto admitted = pairExperimentAdmission(a, r, v, now);
        if (admitted != Probe::Action::OK) return admitted;
        v.snapshotId = r.snapshotId;
#endif
    } else {
        const auto* snapshot = findParameterSnapshot(a, home, r.snapshotId);
        // Same drive and binding, same host tuple; any other source is stale.
        if (!snapshot || !Probe::sameTarget(snapshot->target, a.axis.target) ||
            snapshot->serialGeneration != a.serial.generation) return Probe::Action::UNAVAILABLE;
        const auto inputs = restoreInputs(a, home, now);
        if (inputs != Probe::Action::OK) return inputs;
        v.snapshotId = r.snapshotId;
        v.first = home ? HOME_RESTORE_FIRST : VELOCITY_RESTORE_FIRST;
        v.count = home ? HOME_RESTORE_WORDS : VELOCITY_RESTORE_WORDS;
        std::memcpy(v.words, snapshot->words, v.count * sizeof(uint16_t));
    }
    if (now > UINT64_MAX - 2 * REQUEST_US) return Probe::Action::INVALID;
    v.operationId = a.nextOperationId; v.startedUs = now; v.deadlineUs = now + 2 * REQUEST_US;
    s.~ParameterWriteSession(); new (&s) App::ParameterWriteSession();
    s.view = v; s.phase = 1;
    const auto admitted = submitParameterFrame(a, true, now);
    if (admitted != Probe::Action::OK) { s.~ParameterWriteSession(); new (&s) App::ParameterWriteSession(); return admitted; }
    s.view.pending = true;
    operationId = a.nextOperationId++; a.latestOperationId = operationId;
    return Probe::Action::OK;
}
// From the first accepted byte the words may have changed.
void parameterWriteEffects(App& a, const Probe::ParameterWriteView& v, uint64_t now) {
    const uint8_t address = v.target.address;
    // As a stored homing policy: expire prepared work, qualifications and the
    // generation; the position counter and the host scale are not changed.
    if (pairExperiment(v)) { // A PT pulse pair changes its stored record.
        invalidateDriverAssumptions(a, address, 0, now, nullptr, ESS::DriverGroup::POSITION_SEGMENT);
        a.segmentSettings.operationId = 0;
        return;
    }
    invalidateDriverAssumptions(a, address, 0, now, nullptr, ESS::DriverGroup::HOMING_AUXILIARY);
    if (v.kind == Probe::ParameterWriteKind::HOME_RESTORE) a.homingParameters.operationId = 0;
    else a.velocityParameters.operationId = 0;
}
void finishParameterWrite(App::ParameterWriteSession& s, const char* error, uint64_t now) {
    auto& v = s.view;
    v.pending = false; v.finishedUs = now; v.error = error; s.phase = 0;
    v.ok = !std::strcmp(error, "none");
    // Unknown execution without a checked readback: the stored words are unknown.
    v.unsettled = v.execution == MotorControlRS::ActionExecution::UNKNOWN && !v.readbackKnown;
}
void cancelInvalidParameterWrite(App& a, uint64_t now) {
    auto& s = a.parameterWrite;
    if (!s.view.pending) return;
    if (!Probe::sameTarget(s.view.target, a.axis.target) || s.view.serialGeneration != a.serial.generation)
        s.view.cancelled = true;
    if (s.view.cancelled && s.request.owner && !a.owner.txAccepted(s.request)) a.owner.cancelUnsent(s.request, now);
}
// A stop on this address ends unsent work; a frame on the wire settles first.
void cancelParameterWrite(App& a, uint8_t address, uint64_t now) {
    auto& s = a.parameterWrite;
    if (!s.view.pending || s.view.target.address != address) return;
    s.view.cancelled = true;
    if (s.request.owner && !a.owner.txAccepted(s.request)) a.owner.cancelUnsent(s.request, now);
}
void serviceParameterWrite(App& a, uint64_t now) {
    using namespace MotorControlRS;
    auto& s = a.parameterWrite; auto& v = s.view;
    if (!v.pending) return;
    const bool stale = !Probe::sameTarget(v.target, a.axis.target) || v.serialGeneration != a.serial.generation;
    if (s.request.owner) {
        const std::size_t accepted = a.owner.txAccepted(s.request);
        if (s.phase == 1 && accepted && v.execution == ActionExecution::NOT_TRANSMITTED) {
            v.execution = ActionExecution::UNKNOWN;
            parameterWriteEffects(a, v, now);
        }
        if ((v.cancelled || stale) && !accepted) a.owner.cancelUnsent(s.request, now);
        const auto* result = a.owner.result(s.request);
        if (!result) return;
        const auto& t = result->transport;
        auto& evidence = s.phase == 1 ? v.writeEvidence : v.readEvidence;
        copyStageEvidence(evidence, *result, s.phase == 1 ? 0 : 1, now);
        const bool timely = t.closureQualified && t.closureEarliestUs <= t.closureLatestUs && t.closureLatestUs <= v.deadlineUs;
        const bool checked = result->outcome == Rtu::Outcome::SUCCESS && t.reason == Rtu::Reason::FRAME &&
            timely && t.txComplete && !result->executionUnknown;
        if (s.phase == 1) {
            if (t.reason == Rtu::Reason::FRAME)
                evidence.status = pairExperiment(v) ?
                    ESS::parseExperimentWrite(result->raw, t.rxLength, v.target.address, v.first, v.count, &evidence.frameError) :
                    ESS::parseWriteMultipleRegisters(result->raw, t.rxLength, v.target.address,
                                                     v.first, v.count, &evidence.frameError);
            const bool rejected = timely && evidence.responseConfirmed && !evidence.executionUnknown &&
                evidence.status.code == Err::EXCEPTION && evidence.status.detail >= 1 && evidence.status.detail <= 7 &&
                evidence.status.detail != 5;
            const bool wasSent = v.execution != ActionExecution::NOT_TRANSMITTED;
            v.execution = !t.txAccepted && !result->executionUnknown ? ActionExecution::NOT_TRANSMITTED :
                checked && evidence.status ? ActionExecution::ACKNOWLEDGED : rejected ? ActionExecution::REJECTED : ActionExecution::UNKNOWN;
            if (!wasSent && v.execution != ActionExecution::NOT_TRANSMITTED) parameterWriteEffects(a, v, now);
        } else {
            std::size_t count = 0; uint16_t words[6] = {};
            if (t.reason == Rtu::Reason::FRAME)
                evidence.status = ESS::parseRegisters(result->raw, t.rxLength, v.target.address, v.count,
                    words, 6, count, &evidence.frameError);
            v.readbackKnown = checked && evidence.status && count == v.count;
            if (v.readbackKnown) std::memcpy(v.readback, words, sizeof(v.readback));
        }
        a.owner.release(s.request); s.request = Rtu::RequestId();
        if (v.cancelled || stale) { v.cancelled = true; finishParameterWrite(s, "cancelled", now); return; }
        if (s.phase == 2) {
            const bool match = v.readbackKnown && std::memcmp(v.readback, v.words, v.count * sizeof(uint16_t)) == 0;
            finishParameterWrite(s, match ? "none" : v.readbackKnown ? "readback_mismatch" : "readback_failed", now);
            return;
        }
        if (v.execution != ActionExecution::ACKNOWLEDGED) {
            finishParameterWrite(s, v.execution == ActionExecution::REJECTED ? "write_rejected" :
                v.execution == ActionExecution::NOT_TRANSMITTED ? "write_not_transmitted" : "write_unknown", now);
            return;
        }
        s.phase = 2;
    }
    if (v.cancelled || stale) { v.cancelled = true; finishParameterWrite(s, "cancelled", now); return; }
    if (now >= v.deadlineUs) { finishParameterWrite(s, "deadline", now); return; }
    if (s.phase != 2 || s.request.owner || a.owner.needsRecovery() || uart.needsRecovery()) return;
    const auto admitted = submitParameterFrame(a, false, now);
    if (admitted != Probe::Action::OK && admitted != Probe::Action::QUEUE_FULL && admitted != Probe::Action::RESULTS_FULL)
        finishParameterWrite(s, "readback_admission", now);
}
Probe::Action parameterWriteCommand(void* context, const Probe::ParameterWriteRequest* request,
                                    Probe::ParameterWriteView& out) {
    App& a = *static_cast<App*>(context);
    Probe::Action result = Probe::Action::OK;
    if (request) { uint32_t id = 0; result = startParameterWrite(a, *request, id); }
    out = a.parameterWrite.view;
    return result;
}
