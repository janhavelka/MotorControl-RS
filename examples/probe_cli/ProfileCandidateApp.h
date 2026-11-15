// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.

void copyCandidate(const App::CandidateSession& s, Probe::ProfileCandidateResult& out) {
    out.candidate = s.candidate; out.stage = s.view;
    std::memcpy(out.original, s.original, sizeof(out.original));
    std::memcpy(out.before, s.before, sizeof(out.before));
    out.priorEvidence = s.priorEvidence; out.unchanged = s.unchanged;
    out.error = s.view.cancelled && !std::strcmp(s.error, "none") ? "cancelled" : s.error;
}
void endCandidate(App::CandidateSession& s, const char* error, uint64_t now) {
    s.error = error;
    finishStage(s, false, MotorControlRS::Status(MotorControlRS::Err::INVALID_CONFIG, 0, error), now);
}
void invalidateCandidateWords(App& a, uint8_t address) {
    storedWordsChanged(a, address);
    for (auto& record : a.records)
        if (record.address == address) record.storedPosition.qualified = false;
    a.motionProfile.view.ok = a.motionProfile.view.restored = false; // Keep raw bytes and wire qualification.
    if (auto* member = groupMember(a, address))
        member->profile.view.ok = member->profile.view.restored = false;
}
void candidateEffects(App& a, App::CandidateSession& s, uint64_t now) {
    if (s.effectsInvalidated || s.phase != 1 || !s.request.owner || !a.owner.txAccepted(s.request)) return;
    s.effectsInvalidated = true;
    invalidateCandidateWords(a, s.view.target.address);
    revokePositionStartSpeedRelation(a, s.view.target.address);
    a.configuration.operationId = 0;
    a.movePrerequisites = ESS::MovePrerequisites(); a.velocityPrerequisites = ESS::VelocityPrerequisites();
    a.homePrerequisites = ESS::HomePrerequisites(); a.positionClearQualification = PositionClearQualification();
    invalidateObservations(a, s.view.target.address, now);
    invalidateMemberConfiguration(a, s.view.target.address, now);
}
Probe::Action startProfileCandidate(void* context, uint32_t commandId, const Probe::ProfileCandidate& candidate,
                                    uint32_t& operationId) {
    App& a = *static_cast<App*>(context);
    if (!ESS::isValidAddress(candidate.address) || candidate.words[0] > 2000 || candidate.words[1] > 2000 ||
        candidate.words[2] > 3000 || static_cast<uint16_t>(candidate.wordOrder) > 1) return Probe::Action::INVALID;
    if (!profileTargetInRange(candidate.words + 3, candidate.wordOrder)) return Probe::Action::TARGET_RANGE;
    auto& s = a.profileCandidate;
    if (s.view.operationId) return s.view.pending ? Probe::Action::BUSY : Probe::Action::RESULTS_FULL;
    if (!platformReady || candidate.address != a.axis.target.address || !a.configuration.wordOrderKnown ||
        candidate.wordOrder != a.configuration.wordOrder)
        return Probe::Action::UNAVAILABLE;
    if (a.serial.blocked || a.serial.configuring || a.owner.configurationOwned() ||
        a.owner.needsRecovery() || uart.needsRecovery()) return Probe::Action::RECOVERY_REQUIRED;
    if (a.owner.active() || a.owner.pending() || a.owner.commissioningOwned() || a.discovery.owned ||
        reading(a) || acting(a) || groupSequencing(a) || a.motionProfile.view.pending ||
        a.motionProfile.view.restoreUnsettled || a.monitorState.settings.enabled) return Probe::Action::BUSY;
    if (axisReserved(a, candidate.address)) return Probe::Action::AXIS_CONFLICT;
    if (!a.nextOperationId) return Probe::Action::IDS_EXHAUSTED;
    const uint64_t now = uart.sample();
    if (enableSettling(a, candidate.address, now)) return Probe::Action::BUSY;
    if (!singleSettingPolicy(a, now) || !motionProfileBound(a) || !a.motionProfile.view.ok ||
        !a.motionProfile.view.closureQualified ||
        !MotorControlRS::evidenceAgeValid(a.motionProfile.view.closureEarliestUs, now, a.observationAgeUs()))
        return Probe::Action::UNAVAILABLE;
    if (candidate.identitySpecified && (!a.identity.operationId || !Probe::sameTarget(a.identity.target, a.axis.target) ||
        !a.identity.provenance.qualified || !a.identity.provenance.status ||
        !MotorControlRS::evidenceAgeValid(a.identity.provenance.earliestUs, now, a.observationAgeUs()) ||
        candidate.model != a.identity.rawModel || candidate.firmware != a.identity.rawVersion)) return Probe::Action::UNAVAILABLE;
    if (now > UINT64_MAX - 3000000) return Probe::Action::INVALID;
    s.~CandidateSession(); new (&s) App::CandidateSession();
    s.candidate = candidate; s.wordOrder = candidate.wordOrder; s.commandId = commandId;
    s.serialTuple = a.serial.active;
    auto& v = s.view;
    v.target = a.axis.target; v.operationId = a.nextOperationId;
    v.configurationGeneration = a.axis.generation; v.serialGeneration = a.serial.generation;
    v.startedUs = now; v.deadlineUs = now + 3000000;
    std::memcpy(v.words, candidate.words, sizeof(v.words));
    std::memcpy(s.original, a.motionProfile.view.original, sizeof(s.original));
    Rtu::BusRequest request;
    request.wire.length = ESS::buildReadPositionProfile(candidate.address, s.tx, sizeof(s.tx));
    request.wire.bytes = s.tx; request.wire.replyLength = ESS::expectedReadRegistersLen(6);
    request.wire.responseTimeoutUs = a.serial.timing.responseTimeoutUs; request.wire.replyGapUs = a.serial.timing.replyGapUs;
    request.wire.deadlineUs = v.deadlineUs;
    request.expected.address = candidate.address; request.expected.target = v.target.id;
    request.expected.targetGeneration = v.target.generation;
    request.expected.function = 3; request.expected.first = 0x0020; request.expected.count = 6;
    request.validator = Rtu::essValidator();
    const auto admitted = admissionResult(a.owner.admit(request, now, s.request));
    if (admitted != Probe::Action::OK) { s.~CandidateSession(); new (&s) App::CandidateSession(); return admitted; }
    s.phase = 3; v.pending = true;
    operationId = a.nextOperationId++; a.latestOperationId = operationId;
    return Probe::Action::OK;
}
// Called before owner service, so an invalid queued write cannot get one TX turn.
void cancelInvalidProfileCandidate(App& a, uint64_t now) {
    auto& s = a.profileCandidate;
    if (!s.view.pending) return;
    candidateEffects(a, s, now);
    const bool expiredUnsent = now >= s.view.deadlineUs && (!s.request.owner || !a.owner.txAccepted(s.request));
    if (expiredUnsent || (!s.effectsInvalidated && (!a.configuration.wordOrderKnown ||
        s.wordOrder != a.configuration.wordOrder || !singleSettingPolicy(a, now)))) {
        s.view.cancelled = true;
        s.error = expiredUnsent ? "deadline" : "cancelled";
    }
    if (s.view.cancelled && s.request.owner && !a.owner.txAccepted(s.request)) a.owner.cancelUnsent(s.request, now);
}
void serviceProfileCandidate(App& a, uint64_t now) {
    using namespace MotorControlRS;
    auto& s = a.profileCandidate; auto& v = s.view;
    if (!v.pending) return;
    candidateEffects(a, s, now);
    if (s.phase == 3) {
        const auto* completion = a.owner.result(s.request);
        if (!completion) return;
        // This read is a transaction of this operation, never imported evidence.
        auto& e = s.priorEvidence;
        copyStageEvidence(e, *completion, 0, now);
        std::size_t count = 0;
        if (e.event == ReadEventKind::FRAME)
            e.status = ESS::parseRegisters(e.raw, e.length, v.target.address, 6, s.before, 6, count, &e.frameError);
        const bool checked = completion->outcome == Rtu::Outcome::SUCCESS && e.qualified && e.txComplete &&
            e.latestUs <= v.deadlineUs && e.status && count == 6;
        a.owner.release(s.request); s.request = Rtu::RequestId();
        if (v.cancelled) { endCandidate(s, !std::strcmp(s.error, "none") ? "cancelled" : s.error, now); return; }
        if (!checked) { endCandidate(s, "prior_read", now); return; }
        if (s.before[0] != a.motionProfile.view.current[0]) revokePositionStartSpeedRelation(a, v.target.address);
        if (std::memcmp(s.before, a.motionProfile.view.current, sizeof(s.before))) invalidateCandidateWords(a, v.target.address);
        if (!std::memcmp(s.before + 1, v.words, sizeof(v.words))) {
            s.unchanged = true; std::memcpy(v.readback, s.before + 1, sizeof(v.readback)); v.readbackKnown = true;
            finishStage(s, true, Ok(), now); return;
        }
        if (now >= v.deadlineUs) { endCandidate(s, "deadline", now); return; }
        if (!singleSettingPolicy(a, now)) { v.cancelled = true; endCandidate(s, "cancelled", now); return; }
        // Keep the admitted deadline and metadata; the shared path owns only its base session.
        const uint64_t started = v.startedUs, deadline = v.deadlineUs;
        const auto target = v.target; const uint32_t generation = v.configurationGeneration, id = v.operationId;
        v.pending = false;
        const auto admitted = beginPositionStage(a, s, target, generation, s.candidate.words, id, now, s.wordOrder, deadline);
        v.startedUs = started;
        if (admitted != Probe::Action::OK) {
            v.target = target; v.operationId = id; v.configurationGeneration = generation; v.serialGeneration = a.serial.generation;
            v.deadlineUs = deadline; std::memcpy(v.words, s.candidate.words, sizeof(v.words));
            endCandidate(s, "admission", now);
        }
        return;
    }
    servicePositionStage(a, s, now);
    if (!v.pending && !v.staged) {
        if (v.cancelled) { if (!std::strcmp(s.error, "none")) s.error = "cancelled"; }
        else if (now >= v.deadlineUs) s.error = "deadline";
        else if (v.execution != ActionExecution::ACKNOWLEDGED) s.error = "write";
        else s.error = v.readbackKnown ? "readback_mismatch" : "readback_failed";
    }
}
