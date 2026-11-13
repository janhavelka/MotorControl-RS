// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.
// Drive group (multi-drive 02): a host selection of up to Probe::GROUP_CAPACITY
// unicast drives. Every member uses the ordinary typed reads, move preparation,
// stop path and bus owner. Its target, generation, facts, reservations and
// results stay its own; the group only orders the work and copies evidence.
// A STAGED group move first writes and reads back every member's five 0x0021
// words with no start (PositionStageApp.h), then sends only the start words, back
// to back, so the measured spread is the start frames. A SEQUENTIAL group move
// runs each member's ordinary move (write or verify, then start) in turn.

const PositionStartSpeedRelation NO_START_SPEED_RELATION = PositionStartSpeedRelation();
MotionFacts memberFacts(const App& a, const App::GroupMember& m) {
    // Members keep no start-speed relation. After a possible start-speed write
    // the shared rule refuses the member (startSpeedRelationRequired).
    return MotionFacts(m.axis, m.configuration, m.profile, m.state, m.startSpeedRelationRequired,
        NO_START_SPEED_RELATION, a.bindingGeneration, a.serial.generation, a.observationAgeUs());
}
bool knownTarget(const App& a, uint8_t address) {
    return (a.knownTargets[address / 8] & (1U << (address % 8))) != 0;
}
// Owner-confirmed ESS23-RS20 bench drives: the reply names this exact node and
// reports model 0x4EEA and firmware 0x0029. Anything else is an unexpected responder.
bool expectedIdentity(const ESS::IdentityObservation& identity, uint8_t address) {
    return identity.rawActiveNode == address && identity.rawModel == 0x4EEA && identity.rawVersion == 0x0029;
}
bool memberCurrent(const App& a, const App::GroupMember& m) {
    return a.group.declared && m.axis.generation && m.axis.target.generation == a.bindingGeneration &&
        knownTarget(a, m.address) && m.identity.operationId && Probe::sameTarget(m.identity.target, m.axis.target) &&
        expectedIdentity(m.identity, m.address) &&
        m.configuration.operationId && Probe::sameTarget(m.configuration.target, m.axis.target) &&
        motionProfileBound(memberFacts(a, m)) && m.profile.view.ok && !m.profile.view.pending;
}
App::GroupOperation* groupOperation(App& a, uint32_t operationId) {
    if (!operationId) return nullptr;
    auto& g = a.group;
    if (g.set.result.operationId == operationId) return &g.set;
    if (g.read.result.operationId == operationId) return &g.read;
    if (g.move.result.operationId == operationId) return &g.move;
    for (auto& stop : g.stops) if (stop.result.operationId == operationId) return &stop;
    return nullptr;
}
void clearGroupOperation(App::GroupOperation& op) {
    op.~GroupOperation();
    new (&op) App::GroupOperation();
}
bool groupPending(const App& a) {
    const auto& g = a.group;
    bool pending = g.set.result.pending || g.read.result.pending || g.move.result.pending;
    for (const auto& stop : g.stops) pending = pending || stop.result.pending;
    return pending;
}
// The same idle host as useaddr and wiring, plus no group operation in progress.
bool groupBusy(const App& a) {
    return groupPending(a) || a.serial.configuring || a.owner.configurationOwned() || a.owner.commissioningOwned() ||
        a.discovery.owned || a.owner.active() || a.owner.pending() || a.owner.recovering() ||
        a.runner.busy() || a.runner.transmitEnabled() || reading(a) || acting(a) ||
        a.monitorState.settings.enabled || a.persistenceCapture || a.persistenceRequest.owner ||
        a.persistenceReadRequest.owner || a.commissioningRequest.owner || a.motionProfile.view.pending ||
        a.motionProfile.request.owner || a.simple.view.pending;
}
App::Record* freeRecord(App& a) {
    for (std::size_t i = 0; i < REQUEST_CAPACITY; ++i) if (!a.records[i].operationId) return &a.records[i];
    return nullptr;
}
Probe::Action readAdmission(Rtu::BusAdmission admitted) {
    return admitted == Rtu::BusAdmission::QUEUE_FULL ? Probe::Action::QUEUE_FULL :
        admitted == Rtu::BusAdmission::RESULTS_FULL ? Probe::Action::RESULTS_FULL : Probe::Action::FAILED;
}
// One typed read of one member through the ordinary owner. The member's cache
// receives its state; the selected axis caches are not touched.
Probe::Action startMemberRead(App& a, App::GroupMember& m, ESS::ReadKind kind, uint32_t& operationId) {
    if (!platformReady) return Probe::Action::UNAVAILABLE;
    if (a.serial.blocked || a.serial.configuring || a.owner.configurationOwned() ||
        a.owner.needsRecovery() || uart.needsRecovery()) return Probe::Action::RECOVERY_REQUIRED;
    if (!a.nextOperationId) return Probe::Action::IDS_EXHAUSTED;
    if (m.axis.target.generation != a.bindingGeneration) return Probe::Action::UNAVAILABLE;
    App::Record* record = freeRecord(a);
    if (!record) return Probe::Action::RESULTS_FULL;
    const uint64_t sampled = uart.sample();
    const auto serial = communicationTuple(a.serial.active);
    const auto* configuration = m.configuration.operationId && Probe::sameTarget(m.configuration.target, m.axis.target) ?
        &m.configuration : nullptr;
    MotorControlRS::Status prepared;
    switch (kind) {
    case ESS::ReadKind::IDENTITY:
        prepared = ESS::prepareIdentity(record->read, m.axis.target, a.nextOperationId, sampled, sampled + REQUEST_US, serial); break;
    case ESS::ReadKind::CONFIG:
        prepared = ESS::prepareConfig(record->read, m.axis.target, a.nextOperationId, sampled, sampled + REQUEST_US, serial, m.inputWiring); break;
    case ESS::ReadKind::STATE:
        prepared = ESS::prepareState(record->read, m.axis.target, a.nextOperationId, sampled, sampled + REQUEST_US, serial, configuration); break;
    default:
        prepared = MotorControlRS::Status(MotorControlRS::Err::INVALID_CONFIG, 0, "not a group read");
    }
    if (!prepared) { clearRecord(*record); return Probe::Action::INVALID; }
    record->groupChild = true;
    const auto admitted = admitStep(a, *record, sampled);
    if (admitted != Rtu::BusAdmission::ACCEPTED) { clearRecord(*record); return readAdmission(admitted); }
    record->serialTuple = a.serial.active; record->serialGeneration = a.serial.generation;
    record->operationId = a.nextOperationId++; record->address = m.address;
    record->deadlineUs = record->read.deadlineUs; record->kind = App::OperationKind::TYPED_READ;
    record->configurationGeneration = m.axis.generation;
    operationId = record->operationId;
    return Probe::Action::OK;
}
// Set and read children are released once their facts are copied; the group
// line keeps their evidence. Nothing is resent.
void releaseChild(App& a, uint32_t& operationId) {
    auto* record = findRecord(a, operationId);
    if (record && (!record->requestId.owner || a.owner.release(record->requestId))) clearRecord(*record);
    operationId = 0;
}
bool replied(const ESS::ReadContext& read) {
    if (read.status.code == MotorControlRS::Err::EXCEPTION) return true;
    for (const auto& step : read.observations) if (step.receivedLength) return true;
    return false;
}
void finishGroupOperation(App::GroupOperation& op, bool ok, uint64_t now) {
    op.result.ok = ok; op.result.pending = false; op.result.finishedUs = now;
}

// group set: identity, configuration and position profile of each member in
// order. The first absent or unexpected responder rejects the declaration.
void serviceGroupSet(App& a, uint64_t now) {
    auto& op = a.group.set;
    auto& r = op.result;
    if (!r.pending) return;
    while (!r.aborted && op.next < r.count) {
        auto& m = a.group.members[op.next];
        auto& member = r.members[op.next];
        if (op.phase < 2) {
            if (!op.childId) {
                const auto admitted = startMemberRead(a, m, op.phase ? ESS::ReadKind::CONFIG : ESS::ReadKind::IDENTITY, op.childId);
                if (admitted == Probe::Action::QUEUE_FULL || admitted == Probe::Action::RESULTS_FULL) return;
                member.admission = admitted;
                if (admitted != Probe::Action::OK) { member.join = Probe::GroupJoin::ABSENT; r.aborted = true; break; }
                member.operationId = op.childId;
                return;
            }
            const auto* child = findRecord(a, op.childId);
            if (child && child->read.state == ReadState::ACTIVE) return;
            bool ok = child && child->read.state == ReadState::SUCCEEDED;
            if (child) { member.readState = child->read.state; member.status = child->read.status; }
            if (ok && op.phase == 0) {
                ok = ESS::getIdentity(child->read, m.identity).isOk();
                member.rawModel = m.identity.rawModel; member.rawVersion = m.identity.rawVersion;
                member.rawActiveNode = m.identity.rawActiveNode;
                if (ok && !expectedIdentity(m.identity, m.address)) {
                    ok = false; member.join = Probe::GroupJoin::UNEXPECTED;
                    member.status = MotorControlRS::Status(MotorControlRS::Err::INVALID_CONFIG, 0, "unexpected responder identity");
                }
            } else if (ok) member.configKnown = ok = ESS::getConfig(child->read, m.configuration).isOk();
            if (!ok && member.join == Probe::GroupJoin::NOT_READ)
                member.join = child && replied(child->read) ? Probe::GroupJoin::UNEXPECTED : Probe::GroupJoin::ABSENT;
            releaseChild(a, op.childId);
            if (!ok) { r.aborted = true; break; }
            ++op.phase;
            continue;
        }
        auto& view = m.profile.view;
        if (op.phase == 2) {
            if (a.owner.needsRecovery() || uart.needsRecovery() || m.axis.target.generation != a.bindingGeneration) {
                member.join = Probe::GroupJoin::ABSENT; r.aborted = true; break;
            }
            m.profile.~MotionProfileState(); new (&m.profile) App::MotionProfileState();
            m.profile.configuration = m.configuration.raw;
            m.profile.bindingGeneration = a.bindingGeneration;
            view.address = m.address; view.generation = m.axis.generation;
            view.serialGeneration = a.serial.generation; view.deadlineUs = now + REQUEST_US;
            view.phase = Probe::MotionProfilePhase::READ; view.pending = true; view.error = "none";
            if (!submitMotionProfile(a, m.profile, false, now)) {
                view.pending = false;
                if (a.owner.pending() || a.owner.active()) return; // Queue pressure: nothing was sent.
                member.admission = Probe::Action::FAILED; member.join = Probe::GroupJoin::ABSENT; r.aborted = true; break;
            }
            op.phase = 3;
            return;
        }
        serviceMotionProfile(a, m.profile, now, m.configuration);
        if (view.pending) return;
        member.profileKnown = view.ok;
        if (!view.ok) {
            member.join = view.rxLength ? Probe::GroupJoin::UNEXPECTED : Probe::GroupJoin::ABSENT;
            member.status = MotorControlRS::Status(MotorControlRS::Err::FRAME_ERROR, 0, "position profile read failed");
            r.aborted = true; break;
        }
        member.join = Probe::GroupJoin::JOINED;
        ++op.next; op.phase = 0;
    }
    if (r.aborted) {
        r.failedAddress = r.members[op.next < r.count ? op.next : 0].address;
        a.group.declared = false; a.group.count = 0; // Rejected before any write; no group remains.
        finishGroupOperation(op, false, now);
        return;
    }
    a.group.declared = true;
    finishGroupOperation(op, true, now);
}

// group read state: one checked state read per member, in order. A failed member
// read does not stop the others; each keeps its own evidence.
void serviceGroupRead(App& a, uint64_t now) {
    auto& op = a.group.read;
    auto& r = op.result;
    if (!r.pending) return;
    while (op.next < r.count) {
        auto& member = r.members[op.next];
        auto* m = groupMember(a, member.address);
        if (!m) { member.status = MotorControlRS::Status(MotorControlRS::Err::INVALID_CONFIG, 0, "member removed"); ++op.next; continue; }
        if (!op.childId && !member.operationId) {
            const auto admitted = startMemberRead(a, *m, ESS::ReadKind::STATE, op.childId);
            if (admitted == Probe::Action::QUEUE_FULL || admitted == Probe::Action::RESULTS_FULL) return;
            member.admission = admitted;
            if (admitted != Probe::Action::OK) { ++op.next; continue; }
            member.operationId = op.childId;
            return;
        }
        const auto* child = findRecord(a, op.childId);
        if (child && child->read.state == ReadState::ACTIVE) return;
        if (child) { member.readState = child->read.state; member.status = child->read.status; }
        const auto& motion = m->state.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
        const auto& io = m->state.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
        const auto& feedback = m->state.blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
        member.stateKnown = member.readState == ReadState::SUCCEEDED && Probe::current(motion, m->axis.target) &&
            Probe::current(io, m->axis.target) && Probe::current(feedback, m->axis.target);
        if (member.stateKnown) {
            member.rawAlarm = motion.value.rawAlarm; member.rawMotion = motion.value.rawMotion;
            member.rawInputs = io.value.rawInputs;
            member.positionKnown = feedback.value.pairKnown; member.rawPosition = feedback.value.rawPosition;
        }
        releaseChild(a, op.childId);
        ++op.next;
    }
    bool ok = true;
    for (uint8_t i = 0; i < r.count; ++i) ok = ok && r.members[i].stateKnown;
    finishGroupOperation(op, ok, now);
}

Probe::GroupStart classifyStart(const ESS::MoveContext& c) {
    using MotorControlRS::ActionExecution;
    if (!c.triggerEvidence.txAccepted && !c.triggerEvidence.executionUnknown) return Probe::GroupStart::NOT_SENT;
    return c.execution == ActionExecution::ACKNOWLEDGED ? Probe::GroupStart::STARTED :
        c.execution == ActionExecution::REJECTED ? Probe::GroupStart::REJECTED : Probe::GroupStart::UNKNOWN;
}
// First frame of a prepared member move, at its turn. The member's own record
// already reserves its address; the remaining write checks are checkAxisWrite's.
Probe::Action admitMemberMove(App& a, App::Record& record, uint64_t now) {
    ESS::PreparedMove work;
    if (!ESS::nextMove(record.move, now, work)) return Probe::Action::FAILED;
    if (work.kind != ESS::ActionWork::TRANSACTION) return Probe::Action::QUEUE_FULL; // Not yet eligible.
    if (!platformReady) return Probe::Action::UNAVAILABLE;
    if (a.serial.blocked || a.serial.configuring || a.owner.configurationOwned() ||
        a.owner.needsRecovery() || uart.needsRecovery()) return Probe::Action::RECOVERY_REQUIRED;
    if (!knownTarget(a, work.target.address) || work.target.generation != a.bindingGeneration ||
        record.serialGeneration != a.serial.generation) return Probe::Action::UNAVAILABLE;
    return admitMoveStep(a, record, work, now);
}
void endWaitingMember(App& a, App::Record& record, uint64_t now) {
    record.groupWaiting = false;
    if (terminal(a, record)) return;
    record.cancelContinuation = true;
    advanceOperation(record, actionEvent(record, ReadEventKind::CANCEL), now); // No frame was sent.
    updateActionReservation(a, record);
}
// After a failure no later start is sent: waiting members end locally, and a
// member still staging, or with an unsent start, is cancelled before its start.
// A start frame already on the wire settles with its own evidence.
void endGroupStarts(App& a, App::GroupOperation& op, uint64_t now) {
    auto& r = op.result;
    op.startsEnded = true;
    for (uint8_t i = 0; i < r.count; ++i) {
        if (auto* m = groupMember(a, r.members[i].address)) if (m->stage.view.pending) {
            m->stage.view.cancelled = true; // An unsent stage write never goes out; one on the wire settles.
            if (m->stage.request.owner && !a.owner.txAccepted(m->stage.request)) a.owner.cancelUnsent(m->stage.request, now);
        }
        auto* record = findRecord(a, r.members[i].operationId);
        if (!record) continue;
        if (record->groupWaiting) { endWaitingMember(a, *record, now); continue; }
        if (terminal(a, *record) || record->move.step > record->move.triggerStep) continue;
        const bool startLeft = record->move.step == record->move.triggerStep && record->requestId.owner &&
            a.owner.txAccepted(record->requestId);
        if (startLeft) continue;
        record->cancelContinuation = true;
        if (record->requestId.owner) a.owner.cancelUnsent(record->requestId, now);
    }
}
bool groupStartsResolved(const App& a, const App::GroupOperation& op) {
    for (uint8_t i = 0; i < op.result.count; ++i) {
        const auto* record = findRecord(a, op.result.members[i].operationId);
        if (record && !terminal(a, *record) && record->move.step <= record->move.triggerStep) return false;
    }
    return true;
}
// Once every start resolved: with a stop policy, each member whose start left
// the host gets its own urgent stop, unless a newer explicit stop already owns
// it. Nothing is resent or replayed.
void applyGroupFailurePolicy(App& a, App::GroupOperation& op) {
    using namespace MotorControlRS;
    auto& r = op.result;
    op.policyApplied = true;
    if (r.move.onFailure == Probe::GroupFailurePolicy::LEAVE) return;
    ActionRequest stop; stop.kind = ActionKind::STOP;
    stop.stop.behavior = r.move.onFailure == Probe::GroupFailurePolicy::STOP_FAST ?
        StopBehavior::DIRECT : StopBehavior::CONFIGURED_DECELERATION;
    for (uint8_t i = 0; i < r.count; ++i) {
        auto& member = r.members[i];
        const auto* record = findRecord(a, member.operationId);
        if (!record || record->interruptedByStop) continue;
        const auto start = classifyStart(record->move);
        if (start != Probe::GroupStart::STARTED && start != Probe::GroupStart::UNKNOWN) continue;
        uint32_t id = 0;
        member.stopAdmission = startActionIn(a, 0, member.address, stop, id, true);
        if (member.stopAdmission == Probe::Action::OK) member.stopOperationId = id;
    }
}
bool memberWitnessValid(const App& a, const App::GroupMember& m, const uint16_t* words) {
    return m.storedKnown && m.storedGeneration == m.axis.generation && m.storedBinding == a.bindingGeneration &&
        m.axis.target.generation == a.bindingGeneration && m.storedSerial == a.serial.generation &&
        std::memcmp(m.storedWords, words, sizeof(m.storedWords)) == 0;
}
// Members' observation polls, oldest eligible first, so a shallow shared queue only
// delays a poll by a few transactions. Queue pressure ends this service turn.
void admitMemberPolls(App& a, uint64_t now) {
    for (std::size_t round = 0; round < GROUP_MOVE_RECORDS; ++round) {
        App::Record* oldest = nullptr;
        for (std::size_t i = GROUP_MOVE_FIRST; i < RECORD_COUNT; ++i) {
            auto& r = a.records[i];
            if (!r.operationId || r.kind != App::OperationKind::MOVE || !r.groupChild || r.requestId.owner ||
                r.cancelContinuation || terminal(a, r) || r.move.step <= r.move.triggerStep || r.move.eligibleUs > now) continue;
            if (!oldest || r.move.eligibleUs < oldest->move.eligibleUs) oldest = &r;
        }
        if (!oldest || a.owner.needsRecovery() || uart.needsRecovery()) return;
        ESS::PreparedMove work;
        if (!ESS::nextMove(oldest->move, now, work) || work.kind != ESS::ActionWork::TRANSACTION ||
            admitMoveStep(a, *oldest, work, now) != Probe::Action::OK) return;
    }
}
// Staged phase 1: one member's stage at a time. Any failure ends the group with
// no start sent to any member. Each success becomes that member's witness.
void stageGroupMembers(App& a, App::GroupOperation& op, uint64_t now) {
    auto& r = op.result;
    while (op.next < r.count) {
        auto& member = r.members[op.next];
        const auto* record = findRecord(a, member.operationId);
        auto* m = groupMember(a, member.address);
        if (!record || !m) { r.aborted = true; r.failedAddress = member.address; return; }
        if (member.stageState == MotorControlRS::ActionState::EMPTY) {
            const auto admitted = beginPositionStage(a, m->stage, m->axis.target, m->axis.generation,
                record->move.words, record->operationId, now);
            if (admitted == Probe::Action::QUEUE_FULL || admitted == Probe::Action::RESULTS_FULL) return;
            member.stageState = admitted == Probe::Action::OK ? MotorControlRS::ActionState::ACTIVE : MotorControlRS::ActionState::FAILED;
            if (admitted != Probe::Action::OK) {
                member.stageStatus = MotorControlRS::Status(MotorControlRS::Err::INVALID_CONFIG, static_cast<int32_t>(admitted), "stage not admitted");
                r.aborted = true; r.failedAddress = member.address;
            }
            return;
        }
        const auto& stage = m->stage.view;
        if (stage.pending) return;
        if (!stage.staged) { r.aborted = true; r.failedAddress = member.address; return; }
        m->storedKnown = true;
        std::memcpy(m->storedWords, stage.words, sizeof(m->storedWords));
        m->storedGeneration = stage.configurationGeneration;
        m->storedBinding = stage.target.generation; m->storedSerial = stage.serialGeneration;
        ++op.next;
    }
    // All staged. The starts rely on every witness, so check them all first.
    for (uint8_t i = 0; i < r.count; ++i) {
        const auto* record = findRecord(a, r.members[i].operationId);
        const auto* m = groupMember(a, r.members[i].address);
        if (!record || !m || !memberWitnessValid(a, *m, record->move.words)) {
            r.aborted = true; r.failedAddress = r.members[i].address; return;
        }
    }
    op.phase = 1; op.next = 0;
}
// Starts in member order, each after the previous member's start reply was
// checked. A staged start also needs that member's witness at its turn.
void startGroupMembers(App& a, App::GroupOperation& op, uint64_t now) {
    auto& r = op.result;
    const bool staged = r.move.start == Probe::GroupStartMode::STAGED;
    while (!r.aborted && op.next < r.count) {
        auto& member = r.members[op.next];
        auto* record = findRecord(a, member.operationId);
        if (!record || record->kind != App::OperationKind::MOVE) { r.aborted = true; r.failedAddress = member.address; return; }
        if (record->groupWaiting) {
            if (record->cancelContinuation || terminal(a, *record)) {
                endWaitingMember(a, *record, now);
                r.aborted = true; r.failedAddress = member.address; return;
            }
            // Begin each member on a quiet bus; staged starts hold every poll.
            if (a.owner.active() || a.owner.pending()) return;
            const auto* m = groupMember(a, member.address);
            if (staged && (!m || !memberWitnessValid(a, *m, record->move.words))) {
                member.admission = Probe::Action::UNAVAILABLE;
                endWaitingMember(a, *record, now);
                r.aborted = true; r.failedAddress = member.address; return;
            }
            const auto admitted = admitMemberMove(a, *record, now);
            if (admitted == Probe::Action::OK) record->groupWaiting = false;
            else if (admitted != Probe::Action::QUEUE_FULL && admitted != Probe::Action::RESULTS_FULL) {
                member.admission = admitted;
                endWaitingMember(a, *record, now);
                r.aborted = true; r.failedAddress = member.address;
            }
            return;
        }
        if (!terminal(a, *record)) {
            // The start reply was checked once the move observes its motion.
            if (record->move.step > record->move.triggerStep) { member.start = Probe::GroupStart::STARTED; ++op.next; continue; }
            return;
        }
        member.start = classifyStart(record->move);
        if (member.start == Probe::GroupStart::STARTED && record->move.state == MotorControlRS::ActionState::SUCCEEDED) {
            ++op.next; continue;
        }
        r.aborted = true; r.failedAddress = member.address; return;
    }
    if (!r.aborted) op.startsDone = true;
}
void harvestGroupMove(App& a, App::GroupOperation& op, uint64_t now) {
    auto& r = op.result;
    bool ok = !r.aborted;
    for (uint8_t i = 0; i < r.count; ++i) {
        auto& member = r.members[i];
        if (const auto* record = findRecord(a, member.operationId)) {
            const auto& c = record->move;
            member.state = c.state; member.outcome = c.outcome; member.execution = c.execution;
            member.setupExecution = c.setupExecution; member.completion = c.completion; member.status = c.status;
            member.uncertain = c.uncertain || record->motionUnresolved;
            member.interruptedByStop = record->interruptedByStop;
            member.start = classifyStart(c); member.effectiveNative = c.prepared.effectiveNative;
            member.startTxComplete = record->triggerTxComplete; member.startTxEndUs = record->triggerTxEndUs;
            member.startTxUncertaintyUs = record->triggerTxUncertaintyUs;
            member.runningObserved = c.runningObserved;
            if (c.runningObserved) {
                member.runningEarliestUs = c.activityEvidence.earliestUs; member.runningLatestUs = c.activityEvidence.latestUs;
            }
        }
        const auto* m = groupMember(a, member.address);
        if (m && member.stageState != MotorControlRS::ActionState::EMPTY) {
            const auto& stage = m->stage.view;
            member.stageState = stage.staged ? MotorControlRS::ActionState::SUCCEEDED : MotorControlRS::ActionState::FAILED;
            member.stageExecution = stage.execution;
            member.stageReadback = !stage.readbackKnown ? Probe::GroupStageReadback::NOT_READ :
                std::memcmp(stage.readback, stage.words, sizeof(stage.words)) ? Probe::GroupStageReadback::MISMATCH :
                Probe::GroupStageReadback::MATCH;
            member.stageTxComplete = stage.txComplete; member.stageTxEndUs = stage.txEndUs;
            member.stageReadLatestUs = stage.readLatestUs;
            if (member.stageStatus.isOk()) member.stageStatus = stage.status;
            // An unacknowledged stage write may have changed the drive's words.
            member.uncertain = member.uncertain || stage.execution == MotorControlRS::ActionExecution::UNKNOWN;
        }
        if (const auto* stop = findRecord(a, member.stopOperationId)) {
            member.stopState = stop->action.state; member.stopExecution = stop->action.execution;
            member.stopCompletion = stop->action.completion;
        }
        ok = ok && member.state == MotorControlRS::ActionState::SUCCEEDED &&
            member.completion == MotorControlRS::ActionCompletion::OBSERVED && !member.uncertain;
    }
    finishGroupOperation(op, ok, now);
}
void serviceGroupMove(App& a, uint64_t now) {
    auto& op = a.group.move;
    auto& r = op.result;
    if (!r.pending) return;
    const bool staged = r.move.start == Probe::GroupStartMode::STAGED;
    for (uint8_t i = 0; i < r.count; ++i)
        if (auto* m = groupMember(a, r.members[i].address)) servicePositionStage(a, m->stage, now);
    // A stop, cancellation or recovery of any member before every start resolved
    // ends the sequence at once: no later stage or start is sent.
    for (uint8_t i = 0; !r.aborted && !op.startsDone && i < r.count; ++i) {
        const auto* record = findRecord(a, r.members[i].operationId);
        if (record && (record->interruptedByStop || record->cancelContinuation || (record->groupWaiting && terminal(a, *record)))) {
            r.aborted = true; r.failedAddress = r.members[i].address;
        }
    }
    if (!r.aborted && !op.startsDone) {
        if (staged && op.phase == 0) stageGroupMembers(a, op, now);
        if (!r.aborted && (!staged || op.phase == 1)) startGroupMembers(a, op, now);
    }
    if (r.aborted && !op.startsEnded) endGroupStarts(a, op, now);
    if (r.aborted && !op.policyApplied && groupStartsResolved(a, op)) applyGroupFailurePolicy(a, op);
    // Staged starts hold every member's poll until the last start resolved.
    if (!staged || op.startsDone || op.policyApplied) admitMemberPolls(a, now);
    for (uint8_t i = 0; i < r.count; ++i) {
        const auto* record = findRecord(a, r.members[i].operationId);
        const auto* stop = findRecord(a, r.members[i].stopOperationId);
        const auto* m = groupMember(a, r.members[i].address);
        if ((record && !terminal(a, *record)) || (stop && !terminal(a, *stop)) || (m && m->stage.view.pending)) return;
    }
    harvestGroupMove(a, op, now);
}
void serviceGroupStop(App& a, App::GroupOperation& op, uint64_t now) {
    using namespace MotorControlRS;
    auto& r = op.result;
    if (!r.pending) return;
    for (uint8_t i = 0; i < r.count; ++i) {
        const auto* record = findRecord(a, r.members[i].operationId);
        if (record && !terminal(a, *record)) return;
    }
    bool ok = true;
    for (uint8_t i = 0; i < r.count; ++i) {
        auto& member = r.members[i];
        const auto* record = findRecord(a, member.operationId);
        if (record) {
            const auto& c = record->action;
            member.state = c.state; member.outcome = c.outcome; member.execution = c.execution;
            member.completion = c.completion; member.status = c.status;
            member.uncertain = c.execution == ActionExecution::UNKNOWN ||
                (c.execution == ActionExecution::ACKNOWLEDGED && c.completion != ActionCompletion::OBSERVED);
            member.stateKnown = c.observationKnown; member.rawAlarm = c.rawAlarm; member.rawMotion = c.rawMotion;
        }
        ok = ok && record && member.state == ActionState::SUCCEEDED && member.completion == ActionCompletion::OBSERVED;
    }
    finishGroupOperation(op, ok, now);
}
void serviceGroup(App& a, uint64_t now) {
    serviceGroupSet(a, now);
    serviceGroupRead(a, now);
    serviceGroupMove(a, now);
    for (auto& stop : a.group.stops) serviceGroupStop(a, stop, now);
}

void beginGroupOperation(App& a, App::GroupOperation& op, Probe::GroupOperationKind kind, uint32_t commandId, uint64_t now) {
    clearGroupOperation(op);
    auto& r = op.result;
    r.kind = kind; r.operationId = a.nextOperationId++; r.commandId = commandId; r.pending = true;
    r.declaration = a.group.declaration; r.bindingGeneration = a.bindingGeneration; r.startedUs = now;
    op.serialTuple = a.serial.active; op.serialGeneration = a.serial.generation;
    a.latestOperationId = r.operationId;
}
Probe::Action groupAdmissionReady(const App& a) {
    if (a.owner.commissioningOwned() || a.discovery.owned) return Probe::Action::BUSY;
    if (!platformReady) return Probe::Action::UNAVAILABLE;
    if (a.serial.blocked || !a.serial.activeKnown || a.serial.configuring || a.owner.configurationOwned() ||
        a.owner.needsRecovery() || uart.needsRecovery()) return Probe::Action::RECOVERY_REQUIRED;
    if (!a.nextOperationId) return Probe::Action::IDS_EXHAUSTED;
    return Probe::Action::OK;
}
Probe::Action startGroupSet(App& a, uint32_t commandId, const Probe::GroupCommand& command, Probe::GroupReply& reply) {
    using MotorControlRS::InputWiring;
    if (!command.count || command.count > Probe::GROUP_CAPACITY) return Probe::Action::INVALID;
    for (std::size_t i = 0; i < command.count; ++i) {
        if (!ESS::isValidAddress(command.addresses[i])) { reply.address = command.addresses[i]; return Probe::Action::INVALID; }
        for (std::size_t j = 0; j < i; ++j)
            if (command.addresses[j] == command.addresses[i]) { reply.address = command.addresses[i]; return Probe::Action::INVALID; }
    }
    const auto ready = groupAdmissionReady(a);
    if (ready != Probe::Action::OK) return ready;
    if (groupBusy(a)) return Probe::Action::BUSY;
    auto& op = a.group.set;
    if (op.result.operationId && !op.delivered) return Probe::Action::RESULTS_FULL;
    if (!freeRecord(a)) return Probe::Action::RESULTS_FULL;
    // Wiring declarations of addresses that stay members are kept (App storage).
    auto& inputs = a.group.keptInputs;
    auto& outputs = a.group.keptOutputs;
    for (std::size_t i = 0; i < command.count; ++i) {
        const auto* old = groupMember(a, command.addresses[i]);
        for (uint8_t t = 0; t < 4; ++t) inputs[i][t] = old ? old->inputWiring[t] : InputWiring::UNKNOWN;
        for (uint8_t t = 0; t < 2; ++t) outputs[i][t] = old ? old->outputWiring[t] : InputWiring::UNKNOWN;
    }
    for (auto& member : a.group.members) { member.~GroupMember(); new (&member) App::GroupMember(); }
    a.group.count = static_cast<uint8_t>(command.count); a.group.declared = false; ++a.group.declaration;
    for (std::size_t i = 0; i < command.count; ++i) {
        auto& m = a.group.members[i];
        m.address = command.addresses[i];
        m.axis.target.id = m.axis.target.address = m.address; m.axis.target.generation = a.bindingGeneration;
        m.axis.supportedRelativeBases = 1; // Reviewed ESS actual-position relative basis.
        m.axis.units.settings = a.axis.units.settings; // No command scale is inferred for another drive.
        m.startSpeedRelationRequired = (a.startSpeedWritten[m.address / 8] & (1U << (m.address % 8))) != 0;
        std::memcpy(m.inputWiring, inputs[i], sizeof(m.inputWiring));
        std::memcpy(m.outputWiring, outputs[i], sizeof(m.outputWiring));
    }
    beginGroupOperation(a, op, Probe::GroupOperationKind::SET, commandId, uart.sample());
    op.result.count = static_cast<uint8_t>(command.count);
    for (std::size_t i = 0; i < command.count; ++i) op.result.members[i].address = command.addresses[i];
    reply.operationId = op.result.operationId;
    return Probe::Action::OK;
}
Probe::Action startGroupRead(App& a, uint32_t commandId, Probe::GroupReply& reply) {
    if (!a.group.declared || !a.group.count) return Probe::Action::UNAVAILABLE;
    const auto ready = groupAdmissionReady(a);
    if (ready != Probe::Action::OK) return ready;
    auto& op = a.group.read;
    if (groupSequencing(a) || a.group.set.result.pending) return Probe::Action::BUSY;
    if (op.result.operationId && !op.delivered) return Probe::Action::RESULTS_FULL;
    for (uint8_t i = 0; i < a.group.count; ++i)
        if (!memberCurrent(a, a.group.members[i])) { reply.address = a.group.members[i].address; return Probe::Action::UNAVAILABLE; }
    if (!freeRecord(a)) return Probe::Action::RESULTS_FULL;
    beginGroupOperation(a, op, Probe::GroupOperationKind::READ_STATE, commandId, uart.sample());
    op.result.count = a.group.count;
    for (uint8_t i = 0; i < a.group.count; ++i) op.result.members[i].address = a.group.members[i].address;
    reply.operationId = op.result.operationId;
    return Probe::Action::OK;
}
// Every member is checked and prepared with the shared move rules before any
// frame is sent. The first member's first frame follows on the next service.
Probe::Action startGroupMove(App& a, uint32_t commandId, const Probe::GroupMoveRequest& supplied, Probe::GroupReply& reply) {
    using namespace MotorControlRS;
    const auto& move = supplied.move;
    const bool staged = supplied.start == Probe::GroupStartMode::STAGED;
    if (supplied.onFailure == Probe::GroupFailurePolicy::NONE || supplied.onFailure > Probe::GroupFailurePolicy::LEAVE ||
        supplied.start > Probe::GroupStartMode::SEQUENTIAL ||
        !move.position.relative || move.position.wrapped || move.position.basis != RelativeBasis::ACTUAL ||
        !move.speedRpm || move.ramp != MoveRamp::VERIFIED_CONFIGURED || move.setup > MoveSetup::USE_STORED ||
        (staged && move.setup == MoveSetup::VERIFY_AND_UPDATE)) // Staging always writes all five words.
        return Probe::Action::INVALID;
    // A request cannot name stored words: a staged start relies only on its own fresh stage.
    if (move.setup == MoveSetup::USE_STORED) return Probe::Action::UNSUPPORTED;
    if (!a.group.declared || !a.group.count) return Probe::Action::UNAVAILABLE;
    const auto ready = groupAdmissionReady(a);
    if (ready != Probe::Action::OK) return ready;
    if (groupBusy(a)) return Probe::Action::BUSY;
    auto& op = a.group.move;
    if (op.result.operationId) return Probe::Action::RESULTS_FULL; // Release the earlier group move first.
    const uint8_t count = a.group.count;
    if (a.nextOperationId > UINT32_MAX - count) return Probe::Action::IDS_EXHAUSTED;
    const uint64_t now = uart.sample();
    for (uint8_t i = 0; i < count; ++i) {
        const auto& m = a.group.members[i];
        reply.address = m.address;
        if (!memberCurrent(a, m)) return Probe::Action::UNAVAILABLE;
        if (axisReserved(a, m.address)) return Probe::Action::AXIS_CONFLICT;
        if (enableSettling(a, m.address, now)) return Probe::Action::BUSY; // HW-15: the drive ignores an early start.
    }
    reply.address = 0;
    // Each member's move has its own record in the group pool.
    for (uint8_t i = 0; i < count; ++i) if (a.records[GROUP_MOVE_FIRST + i].operationId) return Probe::Action::RESULTS_FULL;
    const uint64_t deadline = now + static_cast<uint64_t>(a.options.moveTimeoutMs) * 1000;
    ActionOptions options; options.maxPolls = ESS::ACTION_MAX_POLLS; options.pollIntervalUs = 20000;
    options.runningPollIntervalUs = RUNNING_POLL_US;
    for (uint8_t i = 0; i < count; ++i) {
        const auto& m = a.group.members[i];
        auto& record = a.records[GROUP_MOVE_FIRST + i];
        MoveRequest request = move;
        request.position.configurationGeneration = m.axis.generation;
        if (staged) request.setup = MoveSetup::USE_STORED; // Its own stage writes the words; the move only starts.
        const MotionFacts facts = memberFacts(a, m);
        ESS::MovePrerequisites prerequisites;
        const bool readiness = moveRequirements(facts, request, now, prerequisites);
        Status checked(Err::INVALID_CONFIG, 0, "member move readiness unavailable");
        if (readiness) {
            const auto& motion = facts.block(ESS::StateBlock::MOTION);
            if (!Probe::fresh(motion, m.axis.target, now, prerequisites.maximumAgeUs)) prerequisites.readinessQualified = false;
            else {
                prerequisites.rawAlarm = motion.value.rawAlarm; prerequisites.rawMotion = motion.value.rawMotion;
                prerequisites.observedUs = motion.observedEarliestUs;
            }
            prerequisites.wordOrderKnown = m.configuration.wordOrderKnown;
            prerequisites.wordOrder = m.configuration.wordOrder;
            prerequisites.subdivision = m.configuration.raw.subdivision;
            checked = ESS::prepareMoveRelative(record.move, m.axis, nullptr, a.nextOperationId + 1 + i,
                request, prerequisites, now, deadline, options);
        }
        if (!checked) {
            for (uint8_t j = 0; j <= i; ++j) clearRecord(a.records[GROUP_MOVE_FIRST + j]);
            reply.address = m.address;
            return !readiness ? Probe::Action::UNAVAILABLE :
                checked.code == Err::UNSUPPORTED ? Probe::Action::UNSUPPORTED : Probe::Action::INVALID;
        }
    }
    beginGroupOperation(a, op, Probe::GroupOperationKind::MOVE, commandId, now);
    op.result.move = supplied; op.result.count = count;
    for (uint8_t i = 0; i < count; ++i) {
        auto& record = a.records[GROUP_MOVE_FIRST + i];
        const auto& m = a.group.members[i];
        record.serialTuple = a.serial.active; record.serialGeneration = a.serial.generation;
        record.operationId = a.nextOperationId++; record.address = m.address;
        record.deadlineUs = deadline; record.kind = App::OperationKind::MOVE; record.axisReserved = true;
        record.groupChild = record.groupWaiting = true; record.configurationGeneration = m.axis.generation;
        op.result.members[i].address = m.address; op.result.members[i].operationId = record.operationId;
    }
    reply.operationId = op.result.operationId;
    return Probe::Action::OK;
}
// A delivered terminal group stop may be replaced, as stop records are: first a
// confirmed one, otherwise the oldest. Pending or undelivered results stay.
App::GroupOperation* groupStopOperation(App& a) {
    App::GroupOperation* oldest = nullptr;
    for (auto& stop : a.group.stops) {
        if (!stop.result.operationId) return &stop;
        if (stop.result.pending || !stop.delivered) continue;
        if (stop.result.ok) return &stop;
        if (!oldest || stop.result.operationId < oldest->result.operationId) oldest = &stop;
    }
    return oldest;
}
Probe::Action startGroupStop(App& a, uint32_t commandId, MotorControlRS::StopBehavior behavior, Probe::GroupReply& reply) {
    using namespace MotorControlRS;
    if (behavior != StopBehavior::CONFIGURED_DECELERATION && behavior != StopBehavior::DIRECT) return Probe::Action::INVALID;
    if (!a.group.declared || !a.group.count) return Probe::Action::UNAVAILABLE;
    if (!a.nextOperationId) return Probe::Action::IDS_EXHAUSTED;
    auto* op = groupStopOperation(a);
    if (!op) return Probe::Action::RESULTS_FULL;
    // The group identifier comes first; every member stop has its own.
    const uint32_t groupId = a.nextOperationId++;
    ActionRequest stop; stop.kind = ActionKind::STOP; stop.stop.behavior = behavior;
    auto& ids = a.group.stopIds;
    auto& admissions = a.group.stopAdmissions;
    Probe::Action first = Probe::Action::OK;
    std::size_t admitted = 0;
    for (uint8_t i = 0; i < a.group.count; ++i) {
        ids[i] = 0;
        admissions[i] = startActionIn(a, 0, a.group.members[i].address, stop, ids[i], true);
        if (admissions[i] == Probe::Action::OK) ++admitted;
        else if (first == Probe::Action::OK) { first = admissions[i]; reply.address = a.group.members[i].address; }
    }
    if (!admitted) return first;
    if (op->result.operationId && !op->result.ok) ++a.stopEvidenceOverwritten;
    clearGroupOperation(*op);
    auto& r = op->result;
    r.kind = Probe::GroupOperationKind::STOP; r.operationId = groupId; r.commandId = commandId; r.pending = true;
    r.stop = behavior; r.declaration = a.group.declaration; r.bindingGeneration = a.bindingGeneration;
    r.startedUs = nowUs(); r.count = a.group.count;
    op->serialTuple = a.serial.active; op->serialGeneration = a.serial.generation;
    for (uint8_t i = 0; i < r.count; ++i) {
        r.members[i].address = a.group.members[i].address;
        r.members[i].admission = admissions[i]; r.members[i].operationId = admissions[i] == Probe::Action::OK ? ids[i] : 0;
    }
    a.latestOperationId = groupId;
    reply.operationId = groupId;
    return Probe::Action::OK;
}
Probe::Action releaseGroup(App& a, App::GroupOperation& op) {
    auto& r = op.result;
    if (r.pending || !op.delivered) return Probe::Action::BUSY;
    for (uint8_t i = 0; i < r.count; ++i)
        for (uint32_t id : {r.members[i].operationId, r.members[i].stopOperationId}) {
            auto* record = findRecord(a, id);
            if (record && record->groupChild && !terminal(a, *record)) return Probe::Action::BUSY;
        }
    for (uint8_t i = 0; i < r.count; ++i)
        for (uint32_t id : {r.members[i].operationId, r.members[i].stopOperationId}) {
            auto* record = findRecord(a, id);
            if (record && record->groupChild && (!record->requestId.owner || a.owner.release(record->requestId))) clearRecord(*record);
        }
    clearGroupOperation(op);
    return Probe::Action::OK;
}
void markGroupDelivered(App& a, App::GroupOperation& op) {
    op.delivered = true;
    // Children become reusable/releasable only after the group line left.
    for (uint8_t i = 0; i < op.result.count; ++i)
        for (uint32_t id : {op.result.members[i].operationId, op.result.members[i].stopOperationId})
            if (auto* record = findRecord(a, id)) if (record->groupChild) { record->delivered = true; record->deliveredUs = nowUs(); }
}
void deliverGroups(App& a) {
    auto& g = a.group;
    for (auto* op : {&g.set, &g.read, &g.move, &g.stops[0], &g.stops[1]}) {
        if (!op->result.operationId || op->result.pending || op->delivered) continue;
        if (!op->programmatic) {
            op->interactive = a.console.humanResult(op->result.operationId);
            if (!a.console.reportGroup(op->result.commandId, op->result.operationId, op->result, &op->serialTuple, op->serialGeneration))
                continue;
        }
        markGroupDelivered(a, *op);
    }
}
static_assert(Probe::GROUP_STOP_OPERATIONS == 2, "deliverGroups lists both group stop operations");

Probe::Action clearGroupMembers(App& a) {
    if (groupBusy(a)) return Probe::Action::BUSY;
    for (auto& member : a.group.members) { member.~GroupMember(); new (&member) App::GroupMember(); }
    a.group.count = 0; a.group.declared = false; ++a.group.declaration;
    return Probe::Action::OK;
}
// Same declaration as `wiring`, for one member. It changes no drive assignment.
// Move admission does not read wiring; the next group set records it in the
// member's configuration read. Cached state expires, as with `wiring`.
Probe::Action groupWiring(App& a, const Probe::GroupCommand& command, uint8_t& blocking) {
    using MotorControlRS::InputWiring;
    blocking = command.address;
    auto* member = groupMember(a, command.address);
    if (!member) return Probe::Action::UNAVAILABLE;
    if (command.terminal >= 6 || command.wiring > InputWiring::CONNECTED) return Probe::Action::INVALID;
    auto& declaration = command.terminal < 4 ? member->inputWiring[command.terminal] : member->outputWiring[command.terminal - 4];
    blocking = 0;
    if (declaration == command.wiring) return Probe::Action::OK;
    if (groupBusy(a)) return Probe::Action::BUSY;
    declaration = command.wiring;
    for (auto& block : member->state.blocks) block.invalidatedUs = nowUs();
    return Probe::Action::OK;
}
const Probe::GroupView& buildGroupView(App& a) {
    auto& v = a.group.view;
    v.~GroupView(); new (&v) Probe::GroupView();
    const uint64_t now = nowUs();
    v.count = a.group.count; v.declaration = a.group.declaration; v.bindingGeneration = a.bindingGeneration;
    v.nowUs = now; v.sequencing = groupSequencing(a);
    v.setOperationId = a.group.set.result.operationId; v.readOperationId = a.group.read.result.operationId;
    v.moveOperationId = a.group.move.result.operationId;
    for (std::size_t i = 0; i < Probe::GROUP_STOP_OPERATIONS; ++i) v.stopOperationIds[i] = a.group.stops[i].result.operationId;
    for (uint8_t i = 0; i < a.group.count; ++i) {
        const auto& m = a.group.members[i];
        auto& out = v.members[i];
        out.address = m.address; out.target = m.axis.target; out.configurationGeneration = m.axis.generation;
        out.current = memberCurrent(a, m);
        out.identityKnown = m.identity.operationId && Probe::sameTarget(m.identity.target, m.axis.target);
        out.rawModel = m.identity.rawModel; out.rawVersion = m.identity.rawVersion; out.rawActiveNode = m.identity.rawActiveNode;
        out.configKnown = m.configuration.operationId && Probe::sameTarget(m.configuration.target, m.axis.target);
        out.profileKnown = motionProfileBound(memberFacts(a, m)) && m.profile.view.ok;
        std::memcpy(out.profile, m.profile.view.current, sizeof(out.profile));
        std::memcpy(out.inputs, m.inputWiring, sizeof(out.inputs));
        std::memcpy(out.outputs, m.outputWiring, sizeof(out.outputs));
        const auto& motion = m.state.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
        const auto& io = m.state.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
        const auto& feedback = m.state.blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
        const uint64_t age = a.observationAgeUs();
        out.motionFresh = Probe::fresh(motion, m.axis.target, now, age);
        out.ioFresh = Probe::fresh(io, m.axis.target, now, age);
        out.feedbackFresh = Probe::fresh(feedback, m.axis.target, now, age);
        if (motion.valid) { out.rawAlarm = motion.value.rawAlarm; out.rawMotion = motion.value.rawMotion; out.motionObservedUs = motion.observedEarliestUs; }
        if (io.valid) out.rawInputs = io.value.rawInputs;
        if (feedback.valid) { out.positionKnown = feedback.value.pairKnown; out.rawPosition = feedback.value.rawPosition; }
        out.settleUntilUs = m.enableSettleUntilUs; out.settling = now < m.enableSettleUntilUs;
        out.reserved = axisReserved(a, m.address);
        out.startSpeedRelationRequired = m.startSpeedRelationRequired;
    }
    return v;
}
// The one application entry for group work; console routes and the public API
// both call it.
Probe::Action groupCommand(void* context, uint32_t commandId, const Probe::GroupCommand& command, Probe::GroupReply& reply) {
    App& a = *static_cast<App*>(context);
    reply = Probe::GroupReply();
    Probe::Action result = Probe::Action::INVALID;
    switch (command.kind) {
    case Probe::GroupCommandKind::SHOW: case Probe::GroupCommandKind::STATUS:
        reply.view = &buildGroupView(a); return Probe::Action::OK;
    case Probe::GroupCommandKind::CLEAR:
        result = clearGroupMembers(a); reply.view = &buildGroupView(a); return result;
    case Probe::GroupCommandKind::WIRING:
        result = groupWiring(a, command, reply.address); reply.view = &buildGroupView(a); return result;
    case Probe::GroupCommandKind::SET: return startGroupSet(a, commandId, command, reply);
    case Probe::GroupCommandKind::MOVE: return startGroupMove(a, commandId, command.move, reply);
    case Probe::GroupCommandKind::STOP: return startGroupStop(a, commandId, command.stop, reply);
    case Probe::GroupCommandKind::READ_STATE: return startGroupRead(a, commandId, reply);
    }
    return result;
}
