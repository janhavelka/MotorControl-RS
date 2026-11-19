// SPDX-License-Identifier: MIT
// Private cooperative application adapter. One existing commissioning lease,
// one retained persistence invocation; typed reads provide explicit verification.
// Wired bench 17: a begin needs the operator's stated facts for the snapshot's
// endpoint; factory restore is refused at the begin boundary; the custom node
// record and restore live here too.
constexpr uint8_t PERSISTENCE_MAX_INVOCATIONS = 2;
Probe::QualificationReply factsRefusal(Probe::Action action, const char* reason) {
    Probe::QualificationReply reply(action); reply.reason = reason; return reply;
}
bool persistenceSessionOpen(const App& a) { return a.persistence.operationId && !a.persistenceFinished; }
// The target a persistence snapshot and save use now: the confirmed responding
// endpoint inside an open communication session, else the selected axis.
MotorControlRS::ReadTarget persistenceEndpoint(const App& a) {
    if (a.owner.commissioningOwned() && a.persistenceParentSession && a.commissioningLease &&
        a.commissioning.observedActiveKnown && a.commissioningConfirmedGeneration == a.serial.generation)
        return a.commissioning.observedActiveTarget;
    return a.axis.target;
}
void persistenceView(const App& a, Probe::PersistenceView& out) {
    out.context = a.persistence.operationId ? &a.persistence : nullptr;
    out.baseline = a.persistenceBaselineKnown ? &a.persistenceBaseline : nullptr;
    out.snapshotKnown = a.persistenceBaselineKnown;
    out.pending = a.persistenceRequest.owner || a.persistenceCapture || a.node.view.pending;
    out.owned = a.owner.commissioningOwned(); out.host = a.serial;
    // The open invocation's route, or a valid stated route for the next begin.
    out.routeReady = (persistenceSessionOpen(a) && a.persistencePrerequisites.routeBackQualified) ||
        (factsValidity(a, a.persistenceFacts, uart.sample(), persistenceEndpoint(a)) == Probe::FactsValidity::VALID &&
         a.persistenceFacts.routeBack);
    out.parentSession = a.persistenceParentSession; out.finished = a.persistenceFinished;
    out.invocations = a.persistenceInvocations;
    out.snapshotFailed = a.persistenceSnapshotFailed;
    out.verificationAttempts = a.persistenceVerificationAttempts;
}
MotorControlRS::ActionEvent persistenceEvent(const Rtu::Completion& result, uint32_t operation,
                                           uint8_t step, bool write) {
    MotorControlRS::ActionEvent event;
    event.transport.operationId = operation; event.transport.step = step;
    event.transport.kind = result.transport.reason == Rtu::Reason::FRAME ? ReadEventKind::FRAME :
        result.outcome == Rtu::Outcome::CANCELLED ? ReadEventKind::CANCEL : ReadEventKind::TRANSPORT_FAILURE;
    copyCompletion(event.transport, result);
    event.txComplete = result.transport.txComplete;
    event.responseConfirmed = event.transport.kind == ReadEventKind::FRAME && (!write || writeResponseConfirmed);
    return event;
}
Probe::Action persistenceReadStep(App& a, uint64_t now) {
    auto& read = a.persistenceReads[a.persistenceReadIndex];
    ESS::PreparedRead work;
    if (!ESS::nextRead(read, now, work)) return Probe::Action::FAILED;
    Rtu::BusRequest request;
    request.wire.bytes = work.bytes; request.wire.length = work.length;
    request.wire.replyLength = ESS::expectedReadRegistersLen(work.count);
    request.wire.deadlineUs = work.deadlineUs;
    request.wire.responseTimeoutUs = a.serial.timing.responseTimeoutUs;
    request.wire.replyGapUs = a.serial.timing.replyGapUs;
    request.expected.address = work.target.address; request.expected.target = work.target.id;
    request.expected.targetGeneration = work.target.generation; request.expected.function = 3;
    request.expected.first = work.first; request.expected.count = work.count;
    request.validator = Rtu::essValidator();
    return admissionResult(a.owner.admitCommissioning(request, a.commissioningLease, now, a.persistenceReadRequest));
}
bool persistencePrepareRead(App& a, uint64_t now) {
    auto& read = a.persistenceReads[a.persistenceReadIndex];
    const auto serial = communicationTuple(a.serial.active);
    if (a.persistenceReadIndex == 0)
        return ESS::prepareIdentity(read, a.persistenceReadTarget, a.persistenceReadOperation, now, a.persistenceReadDeadline, serial).isOk();
    if (a.persistenceReadIndex == 1)
        return ESS::prepareConfig(read, a.persistenceReadTarget, a.persistenceReadOperation, now, a.persistenceReadDeadline, serial, a.inputWiring).isOk();
    if (!ESS::getConfig(a.persistenceReads[1], a.persistenceVerification.config)) return false;
    return ESS::prepareState(read, a.persistenceReadTarget, a.persistenceReadOperation, now, a.persistenceReadDeadline,
                            serial, &a.persistenceVerification.config).isOk();
}
void persistenceCaptureFailed(App& a) {
    a.persistenceCapture = false; a.persistenceSnapshotFailed = true;
    a.persistenceConfirmedGeneration = 0;
    // Failed partial reads remain in their caller-owned contexts. No new attempt
    // or write is scheduled, and the commissioning lease remains inspectable.
}
Probe::Action persistenceCaptureStart(App& a, uint64_t now, bool after) {
    if (!a.nextOperationId) return Probe::Action::IDS_EXHAUSTED;
    a.persistenceAfter = after; a.persistenceReadIndex = 0;
    a.persistenceCapture = true; a.persistenceSnapshotFailed = false;
    a.persistenceReadDeadline = now + 3000000;
    a.persistenceReadOperation = a.nextOperationId++;
    a.persistenceSnapshotSerialGeneration = a.serial.generation;
    a.persistenceSnapshotBinding = a.bindingGeneration;
    a.persistenceConfirmedGeneration = 0;
    if (!after) a.persistenceBaselineKnown = false;
    if (!persistencePrepareRead(a, now)) { persistenceCaptureFailed(a); return Probe::Action::INVALID; }
    const auto admitted = persistenceReadStep(a, now);
    if (admitted != Probe::Action::OK) persistenceCaptureFailed(a);
    return admitted;
}
void serviceNode(App& a, uint64_t now);
void servicePersistence(App& a, uint64_t now) {
    serviceNode(a, now);
    if (a.persistenceRequest.owner) {
        if (!a.persistenceInvalidated && a.owner.txAccepted(a.persistenceRequest)) {
            a.persistenceInvalidated = true;
            a.commissioningConfirmedGeneration = a.persistenceConfirmedGeneration = 0;
            // Save-all can commit pending assumptions; restore can replace all
            // settings. Invalidate interpretation without editing old results.
            // A confirmed new address is still the same physical axis. Its
            // host binding is deliberately not changed until commissioning ends.
            invalidateDriverAssumptions(a, a.persistenceParentSession ?
                a.axis.target.address : a.persistence.target.address, UINT32_MAX, now);
            invalidateSerialConfidence(a, now);
        }
        const auto* result = a.owner.result(a.persistenceRequest);
        if (result) {
            const auto event = persistenceEvent(*result, a.persistence.operationId, 0, true);
            if (ESS::advancePersistence(a.persistence, event, now)) {
                a.owner.release(a.persistenceRequest); a.persistenceRequest = Rtu::RequestId();
            }
        }
    }
    if (!a.persistenceCapture) return;
    auto& read = a.persistenceReads[a.persistenceReadIndex];
    if (a.persistenceReadRequest.owner) {
        const auto* result = a.owner.result(a.persistenceReadRequest);
        if (!result) return;
        const auto event = persistenceEvent(*result, read.operationId, read.step, false);
        if (!ESS::advanceRead(read, event.transport, now)) return;
        a.owner.release(a.persistenceReadRequest); a.persistenceReadRequest = Rtu::RequestId();
    }
    if (a.persistenceSnapshotSerialGeneration != a.serial.generation ||
        a.persistenceSnapshotBinding != a.bindingGeneration || read.state == ReadState::FAILED) {
        persistenceCaptureFailed(a); return;
    }
    if (read.state == ReadState::ACTIVE) {
        if (persistenceReadStep(a, now) != Probe::Action::OK) persistenceCaptureFailed(a);
        return;
    }
    if (++a.persistenceReadIndex < 3) {
        if (!persistencePrepareRead(a, now) || persistenceReadStep(a, now) != Probe::Action::OK)
            persistenceCaptureFailed(a);
        return;
    }
    auto& verification = a.persistenceVerification;
    if (!ESS::getIdentity(a.persistenceReads[0], verification.identity) ||
        !ESS::getConfig(a.persistenceReads[1], verification.config) ||
        !ESS::getStateBlock(a.persistenceReads[2], 0, verification.stationary)) {
        persistenceCaptureFailed(a); return;
    }
    verification.serial = communicationTuple(a.serial.active);
    verification.configurationGeneration = a.axis.generation;
    verification.maxAgeUs = a.persistencePrerequisites.maxAgeUs;
    // No motor restart evidence in this application: the restart marker needs
    // prompt 14's JOG setter inside the lease (wired bench 17 audit). MCU restart
    // and host restoration never mark a field as proven durable.
    verification.restartObserved = false; verification.restartUs = 0; verification.restartSourceId = 0;
    a.persistenceCapture = false;
    if (a.persistenceAfter) {
        if (!ESS::verifyPersistence(a.persistence, verification, now)) { persistenceCaptureFailed(a); return; }
        a.persistenceConfirmedGeneration = a.serial.generation;
    } else {
        a.persistenceBaseline = a.persistencePrerequisites;
        auto& baseline = a.persistenceBaseline;
        baseline.beforeIdentity = verification.identity; baseline.beforeConfig = verification.config;
        baseline.stationary = verification.stationary; baseline.beforeSerial = verification.serial;
        baseline.configurationGeneration = verification.configurationGeneration;
        a.persistenceBaselineKnown = true;
    }
}
Probe::Action persistence(void* context, const Probe::PersistenceCommand* command, Probe::PersistenceView& out) {
    App& a = *static_cast<App*>(context); persistenceView(a, out);
    if (!command) return Probe::Action::OK;
    using Kind = Probe::PersistenceCommandKind;
    // A parsed begin takes the stated facts before any refusal (one-shot).
    const Probe::PersistenceFacts facts = command->kind == Kind::BEGIN ? a.persistenceFacts : Probe::PersistenceFacts();
    if (command->kind == Kind::BEGIN) a.persistenceFacts = Probe::PersistenceFacts();
    if (a.discovery.owned) return Probe::Action::BUSY;
    const uint64_t now = uart.sample();
    if (!platformReady) return Probe::Action::UNAVAILABLE;
    if (command->kind == Kind::PREVIEW) return Probe::Action::OK;
    if (a.persistenceRequest.owner || a.persistenceCapture || a.commissioningRequest.owner || a.node.view.pending)
        return Probe::Action::BUSY;
    if (command->kind == Kind::SELECT_BEFORE) {
        if (!a.owner.commissioningOwned() || !a.commissioningLease) return Probe::Action::INVALID;
        Probe::HostRequest request;
        const auto& serial = a.persistence.operationId && !a.persistenceFinished ?
            a.persistence.before.beforeSerial : a.persistenceBaseline.beforeSerial;
        if (!serial.known) return Probe::Action::INVALID;
        request.tuple = communicationHost(serial);
        const auto selected = hostSerialImpl(&a, &request, out.host, true);
        persistenceView(a, out); return selected;
    }
    if (a.serial.blocked || !a.serial.activeKnown || a.owner.needsRecovery() || uart.needsRecovery())
        return Probe::Action::RECOVERY_REQUIRED;
    if (command->kind == Kind::SNAPSHOT) {
        if (a.persistence.operationId && !a.persistenceFinished) return Probe::Action::BUSY;
        if (a.owner.commissioningOwned()) {
            if (a.persistenceParentSession && a.commissioningLease &&
                a.commissioning.observedActiveKnown && a.commissioningConfirmedGeneration == a.serial.generation) {
                a.persistenceReadTarget = a.commissioning.observedActiveTarget;
                a.persistenceParentSession = true;
            } else if (!a.persistenceParentSession && a.commissioningLease) {
                a.persistenceReadTarget = a.axis.target;
            } else return Probe::Action::RECOVERY_REQUIRED;
        } else {
            if (reading(a) || acting(a) || axisReserved(a) || a.monitorState.settings.enabled || (a.recovery.operationId && !a.recovery.automatic) ||
                groupSequencing(a))
                return Probe::Action::BUSY;
            if (!a.owner.beginCommissioning(now, a.commissioningLease)) return Probe::Action::BUSY;
            a.persistenceParentSession = false; a.persistenceReadTarget = a.axis.target;
            a.persistenceBaseline.beforeSerial = communicationTuple(a.serial.active);
        }
        const auto result = persistenceCaptureStart(a, now, false); persistenceView(a, out); return result;
    }
    if (command->kind == Kind::BEGIN) {
        if (command->request > ESS::PersistenceKind::FACTORY_RESTORE) return Probe::Action::INVALID;
        // The restricted recommissioning sequence (prompt 17 item 6) does not exist.
        if (command->request == ESS::PersistenceKind::FACTORY_RESTORE) {
            out.reason = "recommissioning_unavailable"; return Probe::Action::UNAVAILABLE;
        }
        if (a.persistence.operationId && !a.persistenceFinished) return Probe::Action::BUSY;
        if (a.persistenceInvocations == PERSISTENCE_MAX_INVOCATIONS) return Probe::Action::RESULTS_FULL;
        if (!a.nextOperationId || a.bindingGeneration == UINT32_MAX) return Probe::Action::IDS_EXHAUSTED;
        if (!a.owner.commissioningOwned() && (reading(a) || acting(a) || axisReserved(a) || a.monitorState.settings.enabled ||
            (a.recovery.operationId && !a.recovery.automatic) || groupSequencing(a)))
            return Probe::Action::BUSY;
        // Physical invocation needs the operator's stated facts (wired bench 17)
        // for the current endpoint; nothing is inferred from a register.
        const bool stated = factsValidity(a, facts, now, persistenceEndpoint(a)) == Probe::FactsValidity::VALID &&
            facts.kind == command->request;
        auto& p = a.persistencePrerequisites;
        p.stationaryQualified = stated && facts.stationary; p.effectsQualified = stated && facts.effects;
        p.qualifiedKind = facts.kind; p.routeBackQualified = stated && facts.routeBack;
        p.completeBackupQualified = false; p.backupSourceId = stated ? facts.backupToken : 0;
        if (!stated) return Probe::Action::UNAVAILABLE;
        if (a.persistenceBaselineKnown && a.owner.commissioningOwned()) {
            if (a.persistenceSnapshotBinding != a.bindingGeneration ||
                a.persistenceSnapshotSerialGeneration != a.serial.generation) return Probe::Action::INVALID;
            p.beforeConfig = a.persistenceBaseline.beforeConfig; p.beforeIdentity = a.persistenceBaseline.beforeIdentity;
            p.stationary = a.persistenceBaseline.stationary; p.beforeSerial = a.persistenceBaseline.beforeSerial;
            p.configurationGeneration = a.persistenceBaseline.configurationGeneration;
        } else {
            if (a.owner.commissioningOwned()) return Probe::Action::INVALID;
            p.beforeConfig = a.configuration; p.beforeIdentity = a.identity;
            const auto& motion = a.stateCache.blocks[0];
            if (!Probe::fresh(motion, a.axis.target, now, p.maxAgeUs)) return Probe::Action::INVALID;
            p.stationary = motion.value; p.beforeSerial = communicationTuple(a.serial.active);
            p.configurationGeneration = a.axis.generation;
        }
        // The facts, the snapshot and the prepared write name one endpoint.
        if (!Probe::sameTarget(facts.target, p.beforeConfig.target)) return Probe::Action::UNAVAILABLE;
        if (!ESS::prepareSave(a.persistencePrepared, p.beforeConfig.target, a.nextOperationId, p, now, now + REQUEST_US))
            return Probe::Action::INVALID;
        if (!a.owner.commissioningOwned()) {
            if (!a.owner.beginCommissioning(now, a.commissioningLease)) return Probe::Action::BUSY;
            a.persistenceParentSession = false;
        }
        a.persistence = a.persistencePrepared; ++a.nextOperationId; ++a.persistenceInvocations;
        a.persistenceFinished = false; a.persistenceInvalidated = false; a.persistenceConfirmedGeneration = 0;
        a.persistenceVerificationAttempts = 0;
        ESS::PreparedPersistence work;
        if (!ESS::nextPersistence(a.persistence, now, work)) return Probe::Action::INVALID;
        Rtu::BusRequest request;
        request.wire.bytes = work.bytes; request.wire.length = work.length;
        request.wire.replyLength = ESS::WRITE_RESPONSE_LEN; request.wire.deadlineUs = work.deadlineUs;
        request.wire.responseTimeoutUs = a.serial.timing.responseTimeoutUs; request.wire.replyGapUs = a.serial.timing.replyGapUs;
        request.expected.address = work.target.address; request.expected.target = work.target.id;
        request.expected.targetGeneration = work.target.generation; request.expected.function = 6;
        request.expected.first = work.reg; request.expected.count = 1; request.expected.value = work.value;
        request.validator = Rtu::essValidator();
        const auto admitted = a.owner.admitCommissioning(request, a.commissioningLease, now, a.persistenceRequest);
        if (admitted != Rtu::BusAdmission::ACCEPTED) {
            MotorControlRS::ActionEvent failed;
            failed.transport.target = work.target; failed.transport.operationId = work.operationId;
            failed.transport.kind = ReadEventKind::TRANSPORT_FAILURE;
            failed.transport.transportDetail = static_cast<int32_t>(admitted);
            ESS::advancePersistence(a.persistence, failed, now);
        }
        persistenceView(a, out); return admissionResult(admitted);
    }
    if (!a.owner.commissioningOwned() || !a.commissioningLease) return Probe::Action::INVALID;
    if (command->kind == Kind::VERIFY) {
        if (!a.persistence.operationId || a.persistenceFinished || a.persistenceVerificationAttempts >= 2) return Probe::Action::INVALID;
        if (!sameTuple(a.serial.active, communicationHost(a.persistence.before.beforeSerial))) return Probe::Action::INVALID;
        a.persistenceReadTarget = a.persistence.target;
        ++a.persistenceVerificationAttempts;
        const auto result = persistenceCaptureStart(a, now, true); persistenceView(a, out); return result;
    }
    if (command->kind != Kind::FINISH) return Probe::Action::INVALID;
    const bool noWrite = !a.persistence.operationId || a.persistenceFinished || !a.persistence.effects;
    if (!noWrite && !a.persistenceParentSession && a.persistence.communicationChanged)
        return Probe::Action::RECOVERY_REQUIRED;
    if (!noWrite && (a.persistenceConfirmedGeneration != a.serial.generation ||
        a.persistenceSnapshotBinding != a.bindingGeneration || !a.persistence.verificationKnown))
        return Probe::Action::RECOVERY_REQUIRED;
    if (noWrite) {
        const auto& before = a.persistence.operationId && !a.persistenceFinished ?
            a.persistence.before.beforeSerial : a.persistenceBaseline.beforeSerial;
        if (!before.known || !sameTuple(a.serial.active, communicationHost(before))) return Probe::Action::RECOVERY_REQUIRED;
    }
    auto& p = a.persistencePrerequisites;
    if (!a.persistenceParentSession) {
        if (!a.owner.endCommissioning(a.commissioningLease, now)) return Probe::Action::BUSY;
        a.commissioningLease = 0;
        a.persistenceBaselineKnown = false;
        if (!noWrite) { ++a.bindingGeneration; a.axis.target.generation = a.bindingGeneration; invalidateSerialConfidence(a, now); }
    }
    a.persistenceFinished = true;
    p.stationaryQualified = p.effectsQualified = p.routeBackQualified = p.completeBackupQualified = false;
    p.backupSourceId = 0;
    persistenceView(a, out); return Probe::Action::OK;
}

// `persistence qualification install|inspect|revoke` (wired bench 17 item 3),
// behind installPersistenceFacts and its pair. Factory restore stays refused
// before TX until the restricted recommissioning sequence (item 6) exists.
Probe::QualificationReply persistenceFactsWork(App& a, const Probe::PersistenceFactsCommand& c,
                                               Probe::PersistenceFactsView& out) {
    const uint64_t now = uart.sample();
    Probe::QualificationReply reply(Probe::Action::OK);
    if (c.action == Probe::FactsAction::REVOKE) a.persistenceFacts = Probe::PersistenceFacts();
    else if (c.action == Probe::FactsAction::INSTALL) {
        if (c.kind > ESS::PersistenceKind::FACTORY_RESTORE || !c.stationary || !c.effects || !c.routeBack ||
            !c.backupToken || (c.completeBackup && c.kind != ESS::PersistenceKind::FACTORY_RESTORE))
            reply = Probe::QualificationReply(Probe::Action::INVALID);
        else if (c.kind == ESS::PersistenceKind::FACTORY_RESTORE)
            reply = factsRefusal(Probe::Action::UNAVAILABLE, "recommissioning_unavailable");
        else if (a.persistenceRequest.owner || a.persistenceCapture || a.node.view.pending || a.discovery.owned)
            reply = Probe::QualificationReply(Probe::Action::BUSY);
        else if (!a.axis.generation) reply = factsRefusal(Probe::Action::UNAVAILABLE, "axis_configuration_unknown");
        else {
            Probe::PersistenceFacts f;
            f.kind = c.kind; f.stationary = c.stationary; f.effects = c.effects; f.routeBack = c.routeBack;
            f.completeBackup = c.completeBackup; f.backupToken = c.backupToken;
            bindFacts(a, f, now, persistenceEndpoint(a)); a.persistenceFacts = f;
        }
    }
    out.facts = a.persistenceFacts; out.validity = factsValidity(a, a.persistenceFacts, now, persistenceEndpoint(a));
    out.nowUs = now;
    out.reason = reply.result == Probe::Action::OK ? nullptr : reply.reason;
    return reply;
}
Probe::QualificationReply persistenceFactsCommand(void*, const Probe::PersistenceFactsCommand& c,
                                                  Probe::PersistenceFactsView& out) {
    if (c.action == Probe::FactsAction::INSTALL) {
        const auto status = installPersistenceFacts(c, out); // Sets out.reason before it is read.
        return factsReply(status, out.reason);
    }
    if (c.action == Probe::FactsAction::REVOKE) revokePersistenceFacts();
    inspectPersistenceFacts(out);
    return Probe::QualificationReply(Probe::Action::OK);
}
// Custom node route back (wired bench 17 item 8, D7): RECORD retains the word
// of a checked session snapshot (S1); RESTORE writes exactly that word back
// inside a later snapshot lease of the same drive, then reads it back. One
// attempt; an unknown execution is settled only by a later checked read.
bool snapshotCurrent(const App& a) {
    return a.persistenceBaselineKnown && !persistenceSessionOpen(a) && a.owner.commissioningOwned() &&
        a.commissioningLease && a.persistenceSnapshotBinding == a.bindingGeneration &&
        a.persistenceSnapshotSerialGeneration == a.serial.generation;
}
Probe::Action submitNodeFrame(App& a, bool write, uint64_t now) {
    auto& s = a.node; const auto& r = s.view.restore;
    Rtu::BusRequest request;
    request.wire.length = write ? ESS::buildWriteSingleRegister(r.target.address, ESS::Registers::CUSTOM_NODE, r.word, s.tx, sizeof(s.tx)) :
        ESS::buildReadRegisters(r.target.address, ESS::Registers::CUSTOM_NODE, 1, s.tx, sizeof(s.tx));
    if (!request.wire.length) return Probe::Action::INVALID;
    request.wire.bytes = s.tx;
    request.wire.replyLength = write ? ESS::WRITE_RESPONSE_LEN : ESS::expectedReadRegistersLen(1);
    request.wire.responseTimeoutUs = a.serial.timing.responseTimeoutUs; request.wire.replyGapUs = a.serial.timing.replyGapUs;
    request.wire.deadlineUs = s.deadlineUs;
    request.expected.address = r.target.address; request.expected.target = r.target.id;
    request.expected.targetGeneration = r.target.generation;
    request.expected.function = write ? 6 : 3;
    request.expected.first = ESS::Registers::CUSTOM_NODE; request.expected.count = 1;
    if (write) request.expected.value = r.word;
    request.validator = Rtu::essValidator();
    return admissionResult(a.owner.admitCommissioning(request, a.commissioningLease, now, s.request));
}
// From the first accepted byte the stored word may have changed.
void nodeEffects(App& a, uint64_t now) {
    invalidateDriverAssumptions(a, a.node.view.restore.target.address, UINT32_MAX, now);
    a.persistenceBaselineKnown = false; // The save needs a fresh snapshot of the restored word.
}
void finishNode(App::NodeSession& s, const char* error) { s.view.pending = false; s.view.error = error; s.phase = 0; }
void serviceNode(App& a, uint64_t now) {
    using namespace MotorControlRS;
    auto& s = a.node; auto& v = s.view; auto& r = v.restore;
    if (!v.pending) return;
    if (s.request.owner) {
        if (s.phase == 1 && a.owner.txAccepted(s.request) && r.execution == ActionExecution::NOT_TRANSMITTED) {
            r.execution = ActionExecution::UNKNOWN; nodeEffects(a, now);
        }
        const auto* result = a.owner.result(s.request);
        if (!result) return;
        const auto& t = result->transport;
        auto& evidence = s.phase == 1 ? v.writeEvidence : v.readEvidence;
        copyStageEvidence(evidence, *result, s.phase == 1 ? 0 : 1, now);
        const bool timely = t.closureQualified && t.closureEarliestUs <= t.closureLatestUs && t.closureLatestUs <= s.deadlineUs;
        const bool checked = result->outcome == Rtu::Outcome::SUCCESS && t.reason == Rtu::Reason::FRAME &&
            timely && t.txComplete && !result->executionUnknown;
        bool readChecked = false;
        if (s.phase == 1) {
            if (t.reason == Rtu::Reason::FRAME)
                evidence.status = ESS::parseWriteSingleRegister(result->raw, t.rxLength, r.target.address,
                    ESS::Registers::CUSTOM_NODE, r.word, &evidence.frameError);
            const bool rejected = timely && evidence.responseConfirmed && !evidence.executionUnknown &&
                evidence.status.code == Err::EXCEPTION && evidence.status.detail >= 1 && evidence.status.detail <= 7 &&
                evidence.status.detail != 5;
            const bool wasSent = r.execution != ActionExecution::NOT_TRANSMITTED;
            r.execution = !t.txAccepted && !result->executionUnknown ? ActionExecution::NOT_TRANSMITTED :
                checked && evidence.status ? ActionExecution::ACKNOWLEDGED : rejected ? ActionExecution::REJECTED : ActionExecution::UNKNOWN;
            if (!wasSent && r.execution != ActionExecution::NOT_TRANSMITTED) nodeEffects(a, now);
        } else {
            std::size_t read = 0; uint16_t word = 0;
            if (t.reason == Rtu::Reason::FRAME)
                evidence.status = ESS::parseRegisters(result->raw, t.rxLength, r.target.address, 1, &word, 1, read, &evidence.frameError);
            readChecked = checked && evidence.status && read == 1;
            if (readChecked) { r.readbackKnown = true; r.readback = word; }
        }
        a.owner.release(s.request); s.request = Rtu::RequestId();
        if (s.phase == 2) {
            r.ok = readChecked && r.execution == ActionExecution::ACKNOWLEDGED && r.readback == r.word;
            finishNode(s, r.ok ? "none" : readChecked ? "readback_mismatch" : "readback_failed");
            return;
        }
        if (r.execution != ActionExecution::ACKNOWLEDGED) {
            finishNode(s, r.execution == ActionExecution::REJECTED ? "write_rejected" :
                r.execution == ActionExecution::NOT_TRANSMITTED ? "write_not_transmitted" : "write_unknown");
            return;
        }
        s.phase = 2;
    }
    if (now >= s.deadlineUs) { finishNode(s, "deadline"); return; }
    if (s.phase != 2 || s.request.owner || a.owner.needsRecovery() || uart.needsRecovery()) return;
    const auto admitted = submitNodeFrame(a, false, now);
    if (admitted != Probe::Action::OK && admitted != Probe::Action::QUEUE_FULL && admitted != Probe::Action::RESULTS_FULL)
        finishNode(s, "readback_admission");
}
Probe::QualificationReply persistenceNodeWork(App& a, const Probe::NodeCommand& c, uint64_t now) {
    using K = Probe::NodeCommandKind;
    auto& s = a.node; auto& v = s.view;
    if (c.kind == K::INSPECT) return Probe::QualificationReply(Probe::Action::OK);
    if (c.kind > K::RESTORE) return Probe::QualificationReply(Probe::Action::INVALID);
    if (!platformReady) return Probe::QualificationReply(Probe::Action::UNAVAILABLE);
    if (v.pending || a.persistenceRequest.owner || a.persistenceCapture || a.commissioningRequest.owner || a.discovery.owned)
        return Probe::QualificationReply(Probe::Action::BUSY);
    if (!a.owner.commissioningOwned() || !a.commissioningLease) return factsRefusal(Probe::Action::INVALID, "lease_required");
    if (!snapshotCurrent(a)) return factsRefusal(Probe::Action::INVALID, "snapshot_required");
    const auto& b = a.persistenceBaseline;
    const auto& target = b.beforeConfig.target;
    if (c.kind == K::RECORD) {
        Probe::NodeRecord record;
        record.present = true; record.address = target.address; record.targetId = target.id;
        record.word = b.beforeConfig.raw.customNode; record.source = b.beforeConfig.operationId; record.recordedUs = now;
        v.record = record;
        return Probe::QualificationReply(Probe::Action::OK);
    }
    // RESTORE: only the recorded word of the same drive, the stated DIP
    // observation and a fresh identity read showing the DIP address.
    if (a.serial.blocked || !a.serial.activeKnown || a.owner.needsRecovery() || uart.needsRecovery())
        return Probe::QualificationReply(Probe::Action::RECOVERY_REQUIRED);
    if (!a.nextOperationId || now > UINT64_MAX - 2 * REQUEST_US) return Probe::QualificationReply(Probe::Action::IDS_EXHAUSTED);
    const auto& record = v.record;
    if (!record.present) return factsRefusal(Probe::Action::UNAVAILABLE, "node_record_required");
    if (target.address != record.address || target.id != record.targetId) return factsRefusal(Probe::Action::INVALID, "wrong_target");
    if (c.word != record.word) return factsRefusal(Probe::Action::INVALID, "record_mismatch");
    if (c.source != record.source) return factsRefusal(Probe::Action::INVALID, "source_mismatch");
    if (!c.dipObserved) return factsRefusal(Probe::Action::UNAVAILABLE, "dip_fact_required");
    const auto& identity = b.beforeIdentity.provenance;
    if (now < identity.attemptedUs || now - identity.attemptedUs > Probe::COMMISSIONING_FACTS_MAX_AGE_US)
        return factsRefusal(Probe::Action::UNAVAILABLE, "dip_evidence_stale");
    if (b.beforeIdentity.rawActiveNode != record.address || b.beforeConfig.raw.customNode == record.address)
        return factsRefusal(Probe::Action::UNAVAILABLE, "dip_not_governing");
    if (b.beforeConfig.raw.customNode == record.word) return factsRefusal(Probe::Action::INVALID, "word_unchanged");
    if (v.restore.attempted && v.restore.snapshotOperationId == b.beforeConfig.operationId)
        return factsRefusal(Probe::Action::INVALID, "already_attempted");
    Probe::NodeRestore restore;
    restore.target = target; restore.word = record.word; restore.previous = b.beforeConfig.raw.customNode;
    restore.source = record.source; restore.snapshotOperationId = b.beforeConfig.operationId; restore.attempted = true;
    v.restore = restore; v.error = "none"; v.operationId = a.nextOperationId;
    s.deadlineUs = now + 2 * REQUEST_US; s.phase = 1;
    const auto admitted = submitNodeFrame(a, true, now);
    if (admitted != Probe::Action::OK) { v.restore.attempted = false; s.phase = 0; return Probe::QualificationReply(admitted); }
    v.pending = true; ++a.nextOperationId;
    return Probe::QualificationReply(Probe::Action::OK);
}
Probe::QualificationReply persistenceNodeCommand(void*, const Probe::NodeCommand& c, Probe::NodeView& out) {
    const char* reason = nullptr;
    const auto status = persistenceNode(c, out, &reason);
    return factsReply(status, reason);
}
