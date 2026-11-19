// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.
// Input-start route (wired bench 08, docs/ess_input_start.md): arm a host
// observation session, pulse one owned bench output once, observe and attribute
// with the core InputStart contexts, disarm. Function 7 is delivered; home (11),
// PT (12) and PV (13) staging refuse before any traffic (prompts 12, 13, 14).
//
// - Arm: per drive, cached fresh facts are checked before any frame; then each
//   drive's five 0x0021 words are staged and read back with no start (the
//   selected axis through startSelectedStage, members as the group STAGED phase
//   without its start). Any staging failure ends the arm; nothing is resent.
// - Trigger: the line-isolation reads (0x0040/5, 0x0006/7) of every drive the
//   board table declares connected on the terminal are the last bus traffic
//   before the ON (D14); non-members follow the shared truth table rows L1-L7
//   (test/line_isolation_cases.json), members their armed facts. One ON.
// - Observe: one 0x0006/7 sample per member at poll-ms, round-robin, through the
//   ordinary owner. The core contexts judge attribution; this file only feeds
//   them. No retrigger, no write, no replay.
// - Disarm closes observation and turns the owned output off; it neither stops
//   a drive nor changes an assignment.

using InputSession = App::InputStartSession;
constexpr uint16_t INPUT_IO_FIRST = 0x0040, INPUT_IO_WORDS = 5; // Input polarity and X0..X3 functions.

bool inputStartActive(const App& a) {
    const auto phase = a.inputStart.view.phase;
    return phase != Probe::InputStartPhase::NONE && phase != Probe::InputStartPhase::DONE;
}
uint8_t outputsForTerminal(const App& a, uint8_t terminal) {
    uint8_t mask = 0;
    if (a.benchOutputMap != FixtureState::APPLIED) return 0;
    for (uint8_t i = 0; i < BenchIo::OUTPUTS && i < FIXTURE_BENCH_OUTPUTS; ++i)
        if (a.benchOutputTerminals[i] == terminal) mask |= static_cast<uint8_t>(1u << i);
    return mask;
}
bool inputStartOwnsOutput(const App& a, uint8_t output) {
    return inputStartActive(a) && output < BenchIo::OUTPUTS && (a.inputStart.ownedMask >> output) & 1u;
}
bool mosfetOutput(uint8_t output) { return output == 2 || output == 3; }
Probe::Action refuseInputStart(App& a, Probe::Action result, const char* reason, uint8_t address = 0) {
    a.inputStartRefusal = Probe::InputStartReply();
    a.inputStartRefusal.reason = reason; a.inputStartRefusal.address = address;
    return result;
}
// Operations still pending that are not this session's own samples or reads
// (D14, line-level row L4). Returns the first one's address through address.
bool foreignPending(const App& a, uint8_t& address) {
    for (const auto& record : a.records)
        if (record.operationId && resultPending(a, record)) { address = record.address; return true; }
    address = 0;
    return groupPending(a) || a.stage.view.pending || a.profileCandidate.view.pending || a.parameterWrite.view.pending ||
        a.motionProfile.view.pending || a.simple.view.pending || a.persistenceCapture || a.persistenceRequest.owner ||
        a.persistenceReadRequest.owner || a.commissioningRequest.owner || a.owner.commissioningOwned() ||
        a.discovery.owned || a.monitorState.settings.enabled;
}
uint32_t debugOverwrites(const App& a) {
    return a.debug.state.mode == Probe::DebugMode::OFF ? 0 : a.debug.state.overwritten + a.debug.state.captureDropped;
}
const BenchIo::View& benchView(App& a) {
    const BenchIo::Request show;
    a.benchScratch = BenchIo::View();
    if (a.options.benchIo) a.options.benchIo->apply(show, a.benchScratch);
    return a.benchScratch;
}
bool anyOutputActive(const BenchIo::View& view) {
    for (const auto& output : view.outputs) if (output.active) return true;
    return false;
}

// ---------------------------------------------------------------- arm checks
struct ArmDrive {
    const MotorControlRS::AxisConfig* axis = nullptr;
    const ESS::ConfigObservation* configuration = nullptr;
    const Probe::StateCache* state = nullptr;
    const IoQualification* io = nullptr;
    const MotorControlRS::InputWiring* wiring = nullptr;
    const App::GroupMember* member = nullptr;
};
bool armDrive(const App& a, bool group, uint8_t address, ArmDrive& d) {
    if (group) {
        d.member = groupMember(a, address);
        if (!d.member) return false;
        d.axis = &d.member->axis; d.configuration = &d.member->configuration; d.state = &d.member->state;
        d.io = d.member->ioQualifications; d.wiring = d.member->inputWiring;
        return true;
    }
    if (address != a.axis.target.address) return false;
    d.axis = &a.axis; d.configuration = &a.configuration; d.state = &a.stateCache;
    d.io = a.ioQualifications; d.wiring = a.inputWiring;
    return true;
}
bool positionMode(const App& a, const ArmDrive& d, uint16_t& mode, uint16_t& pv) {
    if (d.member) {
        if (!d.member->modeKnown || d.member->modeGeneration != d.member->axis.generation || !d.member->axis.generation) return false;
        mode = d.member->positionMode; pv = d.member->pvTriggerMode; return true;
    }
    const auto& s = a.driverSettings;
    if (!s.operationId || s.group != ESS::DriverGroup::DRIVE || !Probe::sameTarget(s.target, a.axis.target) ||
        s.configurationGeneration != a.axis.generation || !a.axis.generation) return false;
    mode = s.raw[6]; pv = s.raw[5]; return true;
}
// Every fact the arm needs for one drive, from cached reads only: no traffic.
Probe::Action checkArmDrive(App& a, const Probe::InputStartRequest& r, const Probe::InputStartMember& want,
                            uint64_t now, ArmDrive& d, ESS::MovePrerequisites& prerequisites, const char*& reason) {
    using namespace MotorControlRS;
    const uint8_t t = r.terminal, address = want.address;
    if (!armDrive(a, r.group, address, d)) { reason = r.group ? "not_a_member" : "not_selected"; return Probe::Action::INVALID; }
    if (!knownTarget(a, address)) { reason = "target_unknown"; return Probe::Action::UNAVAILABLE; }
    if (d.member && !memberCurrent(a, *d.member)) { reason = "member_not_current"; return Probe::Action::UNAVAILABLE; }
    const auto& c = *d.configuration;
    if (!c.operationId || !Probe::sameTarget(c.target, d.axis->target) || !c.wordOrderKnown) {
        reason = "configuration_stale"; return Probe::Action::UNAVAILABLE;
    }
    if (!c.inputFunctionKnown[t] || c.raw.inputFunctions[t] != 7) { reason = "function_not_7"; return Probe::Action::UNAVAILABLE; }
    if (d.wiring[t] != InputWiring::CONNECTED) { reason = "terminal_not_connected"; return Probe::Action::UNAVAILABLE; }
    uint16_t mode = 0, pv = 0;
    if (!positionMode(a, d, mode, pv) || mode > 1) { reason = "position_mode_unknown"; return Probe::Action::UNAVAILABLE; }
    if (mode != (r.mode == Probe::InputStartMode::ABSOLUTE ? 1u : 0u)) { reason = "position_mode_mismatch"; return Probe::Action::UNAVAILABLE; }
    const auto age = a.observationAgeUs();
    const auto& motion = d.state->blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    const auto& io = d.state->blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    const auto& feedback = d.state->blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
    if (!Probe::fresh(motion, d.axis->target, now, age) || !Probe::fresh(io, d.axis->target, now, age) ||
        !Probe::fresh(feedback, d.axis->target, now, age) || io.value.unknownInputBits || !feedback.value.pairKnown) {
        reason = "state_stale"; return Probe::Action::UNAVAILABLE;
    }
    if (motion.value.rawAlarm || motion.value.alarmFlag) { reason = "alarm"; return Probe::Action::UNAVAILABLE; }
    if (motion.value.released) { reason = "released"; return Probe::Action::UNAVAILABLE; }
    if (motion.value.running || feedback.value.rawSpeed) { reason = "moving"; return Probe::Action::UNAVAILABLE; }
    const uint16_t polarity = (c.raw.inputPolarity >> t) & 1u;
    const auto& q = d.io[t];
    const bool qualified = q.target.id && q.purpose == IoPurpose::FINITE_MOVE && q.terminal == t &&
        Probe::sameTarget(q.target, d.axis->target) && q.configurationGeneration == d.axis->generation &&
        q.function == 7 && q.polarity == polarity && ioFactsValid(q, now);
    if (!qualified || !q.input.externalTriggersInhibited) { reason = "triggers_inhibited_required"; return Probe::Action::QUALIFICATION_REQUIRED; }
    bool active = false;
    if ((polarity && q.input.statusMapping == ESS::IoStatusMapping::UNKNOWN) ||
        !effectiveInput(io.value.rawInputs, t, polarity, polarity, q.input.statusMapping, active)) {
        reason = "mapping_unknown"; return Probe::Action::QUALIFICATION_REQUIRED;
    }
    if (active) { reason = "input_active"; return Probe::Action::UNAVAILABLE; }
    if (axisReserved(a, address)) { reason = "reserved"; return Probe::Action::AXIS_CONFLICT; }
    if (enableSettling(a, address, now)) { reason = "enable_settling"; return Probe::Action::BUSY; }
    if (want.move.position.configurationGeneration != d.axis->generation) { reason = "generation_mismatch"; return Probe::Action::INVALID; }
    AxisReference reference;
    if (!want.move.position.relative) reference = d.member ? memberReference(a, *d.member, now) : axisReference(a);
    if (d.member && d.member->coordinate.reference.nativeKnown && !memberReference(a, *d.member, now).nativeKnown) {
        reason = "member_reference_stale"; return Probe::Action::UNAVAILABLE;
    }
    const MotionFacts facts = d.member ? memberFacts(a, *d.member, want.move.position.relative ? nullptr : &reference) :
        selectedFacts(a, want.move.position.relative ? nullptr : &reference);
    const auto stops = stopInputs(facts, now);
    if (stops.result != StopInputs::CLEAR) {
        noteStopInputRefusal(a, stops); reason = a.settingsRefusal.reason ? a.settingsRefusal.reason : "stop_input";
        return Probe::Action::UNAVAILABLE;
    }
    if (!moveRequirements(facts, want.move, now, prerequisites)) { reason = "move_readiness"; return Probe::Action::UNAVAILABLE; }
    capMoveIoAge(facts, prerequisites);
    return Probe::Action::OK;
}
// Baseline words in 0x0006/7 order from the three cached state blocks.
void baselineFromCache(const Probe::StateCache& state, ESS::InputStartPrerequisites& p) {
    const auto& motion = state.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    const auto& io = state.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    const auto& feedback = state.blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
    const uint16_t words[7] = {motion.value.rawAlarm, motion.value.rawMotion, io.value.rawInputs, io.value.rawOutputs,
        feedback.value.rawPositionWords[0], feedback.value.rawPositionWords[1], feedback.value.rawSpeed};
    std::memcpy(p.baselineWords, words, sizeof(words));
    p.baselineKnown = true;
    p.baselineUs = std::min(motion.observedEarliestUs, std::min(io.observedEarliestUs, feedback.observedEarliestUs));
}
Probe::Action armInputStartIn(App& a, const Probe::InputStartRequest& r, uint32_t& operationId) {
    using namespace MotorControlRS;
    a.inputStartRefusal = Probe::InputStartReply();
    if (r.kind == Probe::InputStartKind::HOME) return refuseInputStart(a, Probe::Action::UNSUPPORTED, "home_staging_prompt_12");
    if (r.kind == Probe::InputStartKind::SEGMENT) return refuseInputStart(a, Probe::Action::UNSUPPORTED, "segment_staging_prompt_13");
    if (r.kind == Probe::InputStartKind::VELOCITY) return refuseInputStart(a, Probe::Action::UNSUPPORTED, "pv_staging_prompt_14");
    if (r.kind != Probe::InputStartKind::POSITION || r.terminal > 3 || !r.count || r.count > Probe::INPUT_START_MEMBERS ||
        r.mode > Probe::InputStartMode::ABSOLUTE || r.pollMs < Probe::INPUT_START_MIN_POLL_MS ||
        r.pollMs > Probe::INPUT_START_MAX_POLL_MS || r.windowMs < Probe::INPUT_START_MIN_WINDOW_MS ||
        r.windowMs > Probe::INPUT_START_MAX_WINDOW_MS) return refuseInputStart(a, Probe::Action::INVALID, "invalid_request");
    for (uint8_t i = 0; i < r.count; ++i) {
        const auto& m = r.members[i];
        if (!ESS::isValidAddress(m.address) || m.move.position.relative != (r.mode == Probe::InputStartMode::RELATIVE) ||
            m.move.position.wrapped || m.move.position.frame != CoordinateFrame::NATIVE || !m.move.speedRpm ||
            m.move.ramp != MoveRamp::VERIFIED_CONFIGURED || m.move.setup != MoveSetup::WRITE_ALL ||
            m.move.position.basis != RelativeBasis::ACTUAL)
            return refuseInputStart(a, Probe::Action::INVALID, "invalid_member_request", m.address);
        for (uint8_t j = 0; j < i; ++j)
            if (r.members[j].address == m.address) return refuseInputStart(a, Probe::Action::INVALID, "duplicate_member", m.address);
    }
    if (r.group) {
        if (!a.group.declared || r.count != a.group.count) return refuseInputStart(a, Probe::Action::UNAVAILABLE, "group_not_declared");
    } else if (r.count != 1 || r.members[0].address != a.axis.target.address)
        return refuseInputStart(a, Probe::Action::INVALID, "not_selected", r.members[0].address);
    if (inputStartActive(a) || a.inputStart.members[0].sample.owner || a.inputStart.triggerRequest.owner)
        return refuseInputStart(a, Probe::Action::BUSY, "session_active");
    if (!platformReady) return refuseInputStart(a, Probe::Action::UNAVAILABLE, "platform");
    if (a.serial.blocked || a.serial.configuring || a.owner.configurationOwned() || a.owner.needsRecovery() || uart.needsRecovery())
        return refuseInputStart(a, Probe::Action::RECOVERY_REQUIRED, "recovery_required");
    if (a.owner.commissioningOwned() || a.discovery.owned || groupSequencing(a) || a.owner.active() || a.owner.pending())
        return refuseInputStart(a, Probe::Action::BUSY, "bus_busy");
    if (!a.nextOperationId || a.nextOperationId > UINT32_MAX - 2) return refuseInputStart(a, Probe::Action::IDS_EXHAUSTED, "ids_exhausted");
    if (!a.options.benchIo) return refuseInputStart(a, Probe::Action::UNAVAILABLE, "bench_unavailable");
    if (a.fixtureState != FixtureState::APPLIED) return refuseInputStart(a, Probe::Action::UNAVAILABLE, "fixture_table_unavailable");
    const uint8_t owned = outputsForTerminal(a, r.terminal);
    if (!owned) return refuseInputStart(a, Probe::Action::UNAVAILABLE, "no_output_for_terminal");
    const auto& view = benchView(a);
    if (!view.ready) return refuseInputStart(a, Probe::Action::UNAVAILABLE, "bench_unavailable");
    for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i)
        if (((owned >> i) & 1u) && view.outputs[i].active) return refuseInputStart(a, Probe::Action::BUSY, "output_active");
    const uint64_t now = uart.sample();
    auto& prerequisites = a.inputStart.prerequisites;
    ArmDrive drives[Probe::INPUT_START_MEMBERS];
    for (uint8_t i = 0; i < r.count; ++i) {
        const char* reason = "none";
        const auto checked = checkArmDrive(a, r, r.members[i], now, drives[i], prerequisites[i], reason);
        if (checked != Probe::Action::OK) return refuseInputStart(a, checked, reason, r.members[i].address);
    }
    auto& s = a.inputStart;
    const uint64_t deadline = now + static_cast<uint64_t>(a.options.moveTimeoutMs) * 1000;
    ActionOptions options; options.maxPolls = ESS::ACTION_MAX_POLLS; options.pollIntervalUs = 20000;
    options.runningPollIntervalUs = RUNNING_POLL_US;
    // Members are prepared with the shared move rules into session scratch first;
    // a refusal here sends nothing. The selected axis prepares inside its stage.
    if (r.group) for (uint8_t i = 0; i < r.count; ++i) {
        auto& p = prerequisites[i];
        const auto& m = *drives[i].member;
        const auto& motion = m.state.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
        p.rawAlarm = motion.value.rawAlarm; p.rawMotion = motion.value.rawMotion; p.observedUs = motion.observedEarliestUs;
        p.wordOrderKnown = m.configuration.wordOrderKnown; p.wordOrder = m.configuration.wordOrder;
        p.subdivision = m.configuration.raw.subdivision;
        const auto reference = memberReference(a, m, now);
        const auto prepare = r.members[i].move.position.relative ? ESS::prepareMoveRelative : ESS::prepareMoveAbsolute;
        const Status prepared = prepare(s.members[i].prepared, m.axis, reference.nativeKnown ? &reference : nullptr,
            a.nextOperationId, r.members[i].move, p, now, deadline, options);
        if (!prepared) return refuseInputStart(a, prepared.code == Err::UNSUPPORTED ? Probe::Action::UNSUPPORTED :
            Probe::Action::INVALID, "move_preparation", m.address);
    }
    uint32_t stageId = 0;
    if (!r.group) {
        const auto staged = startSelectedStage(a, r.members[0].move, stageId);
        if (staged != Probe::Action::OK)
            return refuseInputStart(a, staged, a.settingsRefusal.reason ? a.settingsRefusal.reason : "stage_refused", r.members[0].address);
    }
    // Commit: nothing above changed the session view.
    const uint32_t first = stageId ? stageId : a.nextOperationId;
    s.view.~InputStartView();
    auto& v = *new (&s.view) Probe::InputStartView();
    v.operationId = a.nextOperationId++; v.kind = r.kind; v.terminal = r.terminal; v.mode = r.mode; v.group = r.group;
    v.phase = Probe::InputStartPhase::STAGING; v.pollMs = r.pollMs; v.windowMs = r.windowMs; v.startedUs = now;
    v.count = r.count;
    if (&s.request != &r) s.request = r;
    s.next = 0; s.stageBegun = false; s.firstOperationId = first;
    s.participantCount = s.step = 0; s.triggerRequest = Rtu::RequestId(); s.disarming = false;
    s.ownedMask = owned;
    for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i) s.ownedOns[i] = view.outputs[i].ons;
    s.debugBase = debugOverwrites(a); s.aux1Base = view.input.overwritten;
    for (uint8_t i = 0; i < r.count; ++i) {
        auto& member = s.members[i];
        const auto& d = drives[i];
        member.address = r.members[i].address;
        member.context = ESS::InputStartContext(); member.sample = Rtu::RequestId();
        member.sampleSequence = 0; member.seenStopId = 0; member.stopTxSeen = false;
        auto& p = member.prerequisites;
        p = ESS::InputStartPrerequisites();
        p.target = d.axis->target; p.configurationGeneration = d.axis->generation; p.terminal = r.terminal; p.function = 7;
        p.polarity = (d.configuration->raw.inputPolarity >> r.terminal) & 1u;
        p.statusMapping = d.io[r.terminal].input.statusMapping;
        uint16_t mode = 0, pv = 0; positionMode(a, d, mode, pv);
        p.positionModeKnown = true; p.positionMode = mode; p.pvTriggerModeKnown = pv <= 1; p.pvTriggerMode = pv;
        // The session owned the trigger output from before staging and checked it off.
        p.sourceInhibitedUntilArmed = true;
        p.wordOrderKnown = d.configuration->wordOrderKnown; p.wordOrder = d.configuration->wordOrder;
        baselineFromCache(*d.state, p);
        p.maximumAgeUs = a.observationAgeUs();
        auto& mv = s.view.members[i];
        mv = Probe::InputStartMemberView();
        mv.address = member.address; mv.configurationGeneration = d.axis->generation; mv.function = 7;
        mv.polarity = p.polarity; mv.positionMode = mode; mv.assignmentKnown = true; mv.assignment = 7;
        mv.stageOperationId = r.group ? v.operationId : stageId;
        int32_t baseline = 0; ESS::decodeInt32(p.baselineWords + 4, 2, p.wordOrder, baseline);
        mv.baselinePosition = baseline;
    }
    s.serialGeneration = a.serial.generation; s.bindingGeneration = a.bindingGeneration; s.declaration = a.group.declaration;
    operationId = a.latestOperationId = v.operationId;
    return Probe::Action::OK;
}

// ---------------------------------------------------------- session service
bool inputStartCurrent(const App& a) {
    const auto& s = a.inputStart;
    if (s.serialGeneration != a.serial.generation || s.bindingGeneration != a.bindingGeneration) return false;
    for (uint8_t i = 0; i < s.view.count; ++i) {
        const auto& mv = s.view.members[i];
        if (s.view.group) {
            const auto* m = groupMember(a, mv.address);
            if (!m || !a.group.declared || a.group.declaration != s.declaration || m->axis.generation != mv.configurationGeneration ||
                m->axis.target.generation != a.bindingGeneration) return false;
        } else if (a.axis.target.address != mv.address || a.axis.generation != mv.configurationGeneration ||
                   a.axis.target.generation != a.bindingGeneration) return false;
    }
    return true;
}
void cancelSessionRequests(App& a, uint64_t now) {
    auto& s = a.inputStart;
    for (uint8_t i = 0; i < Probe::INPUT_START_MEMBERS; ++i)
        if (s.members[i].sample.owner && !a.owner.txAccepted(s.members[i].sample)) a.owner.cancelUnsent(s.members[i].sample, now);
    if (s.triggerRequest.owner && !a.owner.txAccepted(s.triggerRequest)) a.owner.cancelUnsent(s.triggerRequest, now);
}
// Call before owner.service so stale queued reads cannot reach TX first.
void cancelInvalidInputStart(App& a, uint64_t now) {
    if (inputStartActive(a) && !inputStartCurrent(a)) cancelSessionRequests(a, now);
}
void sendContextEvent(App& a, uint8_t i, const ESS::InputStartEvent& event, uint64_t now) {
    auto& c = a.inputStart.members[i].context;
    if (c.state == MotorControlRS::ActionState::ACTIVE) ESS::advanceInputStart(c, event, now);
}
void eventAll(App& a, ESS::InputStartEventKind kind, uint64_t now, uint16_t missing = 0) {
    for (uint8_t i = 0; i < a.inputStart.view.count; ++i) {
        ESS::InputStartEvent e; e.kind = kind; e.operationId = a.inputStart.view.operationId; e.missing = missing;
        sendContextEvent(a, i, e, now);
    }
}
// Turn every owned output off that is still on; the hold timer stays the backstop.
void quiesceOwnedOutput(App& a, Probe::InputSourceState reason) {
    auto& s = a.inputStart;
    auto& v = s.view;
    if (!a.options.benchIo) return;
    for (uint8_t o = 0; o < BenchIo::OUTPUTS; ++o) {
        if (!((s.ownedMask >> o) & 1u) || !benchView(a).outputs[o].active) continue;
        BenchIo::Request off; off.op = BenchIo::Op::OFF; off.output = o;
        a.benchScratch = BenchIo::View();
        a.options.benchIo->apply(off, a.benchScratch);
        if (!v.triggered || o != v.output || a.benchScratch.outputs[o].active) continue;
        v.source = reason; v.offUs = a.benchScratch.outputs[o].offUs;
        for (uint8_t i = 0; i < v.count; ++i)
            if (s.members[i].context.state == MotorControlRS::ActionState::ACTIVE)
                ESS::noteInputStartSource(s.members[i].context, v.offUs, false);
    }
}
void copyResults(App& a) {
    auto& s = a.inputStart;
    for (uint8_t i = 0; i < s.view.count; ++i) {
        auto& mv = s.view.members[i];
        mv.resultKnown = ESS::getInputStart(s.members[i].context, mv.result).isOk();
    }
}
void finishInputStart(App& a, const char* error, uint64_t now) {
    auto& v = a.inputStart.view;
    eventAll(a, ESS::InputStartEventKind::CANCEL, now);
    quiesceOwnedOutput(a, a.inputStart.disarming ? Probe::InputSourceState::QUIESCED_BY_DISARM :
        Probe::InputSourceState::QUIESCED_BY_CANCEL);
    cancelSessionRequests(a, now);
    copyResults(a);
    if (!std::strcmp(v.error, "none")) v.error = error;
    v.phase = Probe::InputStartPhase::DONE; v.finishedUs = now;
}
// Staged phase: one drive at a time; any failure ends the arm with no start.
void serviceInputStaging(App& a, uint64_t now) {
    auto& s = a.inputStart;
    auto& v = s.view;
    const auto& bench = benchView(a);
    for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i)
        if (((s.ownedMask >> i) & 1u) && (bench.outputs[i].active || bench.outputs[i].ons != s.ownedOns[i])) {
            // A source edge before ARMED: the input may have started the staged move.
            cancelPositionStages(a, s.members[v.group ? (s.next < v.count ? s.next : 0) : 0].address, now);
            finishInputStart(a, "trigger_during_staging", now);
            return;
        }
    if (!v.group) {
        const auto& stage = a.stage.view;
        if (stage.pending) return;
        auto& mv = v.members[0];
        mv.staged = stage.staged && stage.operationId == mv.stageOperationId; mv.stageExecution = stage.execution;
        std::memcpy(mv.stagedWords, stage.words, sizeof(mv.stagedWords));
        if (!mv.staged) { finishInputStart(a, "stage_failed", now); return; }
    } else while (s.next < v.count) {
        auto& member = s.members[s.next];
        auto& mv = v.members[s.next];
        auto* m = groupMember(a, member.address);
        if (!m) { finishInputStart(a, "member_removed", now); return; }
        if (!s.stageBegun) {
            ESS::PreparedMove work;
            const auto checked = ESS::nextMove(member.prepared, now, work);
            if (!checked || work.kind != ESS::ActionWork::TRANSACTION || work.deadlineUs <= now) {
                finishInputStart(a, "stage_readiness_expired", now); return;
            }
            const auto admitted = beginPositionStage(a, m->stage, m->axis.target, m->axis.generation, member.prepared.words,
                v.operationId, now, member.prepared.prerequisites.wordOrder,
                std::min(work.deadlineUs, now > UINT64_MAX - 2 * REQUEST_US ? UINT64_MAX : now + 2 * REQUEST_US));
            if (admitted == Probe::Action::QUEUE_FULL || admitted == Probe::Action::RESULTS_FULL) return;
            if (admitted != Probe::Action::OK) { finishInputStart(a, "stage_not_admitted", now); return; }
            s.stageBegun = true;
            return;
        }
        servicePositionStage(a, m->stage, now);
        if (m->stage.view.pending) return;
        mv.staged = m->stage.view.staged; mv.stageExecution = m->stage.view.execution;
        std::memcpy(mv.stagedWords, m->stage.view.words, sizeof(mv.stagedWords));
        if (!mv.staged) { finishInputStart(a, "stage_failed", now); return; }
        // The member's stored-word witness, as the group STAGED phase records it.
        m->storedKnown = true;
        std::memcpy(m->storedWords, m->stage.view.words, sizeof(m->storedWords));
        m->storedGeneration = m->stage.view.configurationGeneration;
        m->storedBinding = m->stage.view.target.generation; m->storedSerial = m->stage.view.serialGeneration;
        ++s.next; s.stageBegun = false;
    }
    // Every drive staged: arm the contexts from the arm-time baseline.
    ESS::InputStartOptions options;
    options.pollIntervalUs = v.pollMs * 1000u; options.windowUs = v.windowMs * 1000u;
    for (uint8_t i = 0; i < v.count; ++i) {
        auto& member = s.members[i];
        auto& p = member.prerequisites;
        auto& mv = v.members[i];
        std::memcpy(p.stagedWords, mv.stagedWords, sizeof(p.stagedWords)); p.stagedKnown = true;
        int32_t target = 0;
        p.expectedKnown = ESS::decodeInt32(mv.stagedWords + 3, 2, p.wordOrder, target).isOk();
        p.expectedDisplacement = target; p.expectedEndpoint = target;
        mv.expectedDisplacement = v.mode == Probe::InputStartMode::RELATIVE ? target : 0;
        mv.expectedEndpoint = v.mode == Probe::InputStartMode::ABSOLUTE ? target : 0;
        const auto prepared = ESS::prepareInputStart(member.context, p, v.operationId, options, now);
        if (!prepared) {
            v.error = prepared.detail == static_cast<int32_t>(ESS::InputStartError::STALE_FACTS) ? "baseline_stale" : "context_refused";
            finishInputStart(a, v.error, now); return;
        }
    }
    v.phase = Probe::InputStartPhase::ARMED; v.armedUs = now;
}
// Stops of armed drives: the context ends STOPPED only after a stopped sample
// and the post-release window, never at the stop acknowledgement.
void serviceInputStops(App& a, uint64_t now) {
    using namespace MotorControlRS;
    auto& s = a.inputStart;
    for (uint8_t i = 0; i < s.view.count; ++i) {
        auto& member = s.members[i];
        auto& mv = s.view.members[i];
        for (const auto& record : a.records) {
            if (record.kind != App::OperationKind::ACTION || record.action.request.kind != ActionKind::STOP ||
                record.address != member.address || record.operationId < s.firstOperationId) continue;
            if (record.operationId > member.seenStopId) {
                member.seenStopId = record.operationId; member.stopTxSeen = false;
                ESS::InputStartEvent e; e.kind = ESS::InputStartEventKind::STOP; e.operationId = s.view.operationId;
                e.stopId = record.operationId;
                sendContextEvent(a, i, e, now);
                mv.stopId = record.operationId;
            }
            if (record.operationId != mv.stopId) continue;
            const auto& c = record.action;
            mv.stopResult = c.state == ActionState::ACTIVE ? "pending" :
                c.state == ActionState::SUCCEEDED && c.completion == ActionCompletion::OBSERVED ? "confirmed" :
                c.execution == ActionExecution::UNKNOWN ? "unknown" : "failed";
            if (!member.stopTxSeen && record.requestId.owner && a.owner.txAccepted(record.requestId)) {
                member.stopTxSeen = true;
                mv.stopServiceDelayUs = now > c.startedUs ? now - c.startedUs : 0;
                if (mv.stopServiceDelayUs > s.view.stopServiceDelayMaxUs) s.view.stopServiceDelayMaxUs = mv.stopServiceDelayUs;
            }
        }
        // A serial start to an armed drive would break attribution; the reservation should refuse it.
        for (const auto& record : a.records)
            if (record.operationId >= s.firstOperationId && record.address == member.address &&
                ((record.kind == App::OperationKind::MOVE && record.move.triggerEvidence.txAccepted) ||
                 (record.kind == App::OperationKind::VELOCITY && record.velocity.triggerEvidence.txAccepted) ||
                 (record.kind == App::OperationKind::HOME && record.home.triggerEvidence.txAccepted))) {
                ESS::InputStartEvent e; e.kind = ESS::InputStartEventKind::DEGRADED; e.operationId = s.view.operationId;
                e.missing = ESS::INPUT_START_MISSING_SERIAL_START;
                sendContextEvent(a, i, e, now);
            }
    }
}
// Source edges as the port reports them, and the ring-overwrite witnesses.
void serviceInputSource(App& a, uint64_t now) {
    auto& s = a.inputStart;
    auto& v = s.view;
    const auto& bench = benchView(a);
    const uint32_t debug = debugOverwrites(a) > s.debugBase ? debugOverwrites(a) - s.debugBase : 0;
    const uint32_t aux1 = bench.input.overwritten > s.aux1Base ? bench.input.overwritten - s.aux1Base : 0;
    if (debug > v.debugOverwrites || aux1 > v.aux1Overwrites) {
        v.debugOverwrites = std::max(debug, v.debugOverwrites); v.aux1Overwrites = std::max(aux1, v.aux1Overwrites);
        if (v.phase == Probe::InputStartPhase::OBSERVING || v.phase == Probe::InputStartPhase::TRIGGERING)
            eventAll(a, ESS::InputStartEventKind::DEGRADED, now, ESS::INPUT_START_MISSING_OVERWRITE);
    }
    if (!v.triggered || v.source != Probe::InputSourceState::ACTIVE) return;
    const auto& output = bench.outputs[v.output];
    if (output.active) return;
    v.offUs = output.offUs;
    v.source = output.offReason == BenchIo::OffReason::STOP ? Probe::InputSourceState::QUIESCED_BY_STOP :
        output.offReason == BenchIo::OffReason::LEASE || output.offReason == BenchIo::OffReason::LATE ?
        Probe::InputSourceState::HOLD_ELAPSED : output.offReason == BenchIo::OffReason::REQUEST ?
        (s.disarming ? Probe::InputSourceState::QUIESCED_BY_DISARM : Probe::InputSourceState::OFF_REQUESTED) :
        Probe::InputSourceState::PORT_FAILED;
    for (uint8_t i = 0; i < v.count; ++i)
        if (s.members[i].context.state == MotorControlRS::ActionState::ACTIVE)
            ESS::noteInputStartSource(s.members[i].context, v.offUs, false);
    (void)now;
}
Probe::Action admitInputRead(App& a, Rtu::RequestId& id, uint8_t* tx, uint8_t address, uint16_t first, uint16_t count, uint64_t now) {
    Rtu::BusRequest request;
    request.wire.length = ESS::buildReadRegisters(address, first, count, tx, 8);
    if (!request.wire.length) return Probe::Action::INVALID;
    request.wire.bytes = tx;
    request.wire.replyLength = ESS::expectedReadRegistersLen(count);
    request.wire.responseTimeoutUs = a.serial.timing.responseTimeoutUs; request.wire.replyGapUs = a.serial.timing.replyGapUs;
    request.wire.deadlineUs = now + REQUEST_US;
    request.expected.address = address; request.expected.target = address; request.expected.targetGeneration = a.bindingGeneration;
    request.expected.function = 3; request.expected.first = first; request.expected.count = count;
    request.validator = Rtu::essValidator();
    return admissionResult(a.owner.admit(request, now, id));
}
// Settle one member sample whose owner result is ready.
void settleSample(App& a, uint8_t i, uint64_t now) {
    auto& s = a.inputStart;
    auto& member = s.members[i];
    const auto* result = a.owner.result(member.sample);
    if (!result) return;
    ESS::InputStartEvent e;
    e.operationId = s.view.operationId; e.sequence = member.sampleSequence; e.requestUs = member.sampleRequestUs;
    const auto& t = result->transport;
    const bool frame = t.reason == Rtu::Reason::FRAME && result->outcome == Rtu::Outcome::SUCCESS;
    e.kind = frame ? ESS::InputStartEventKind::SAMPLE : ESS::InputStartEventKind::LOST;
    if (frame) {
        e.transport.kind = MotorControlRS::ReadEventKind::FRAME;
        copyCompletion(e.transport, *result);
        e.transport.target = member.prerequisites.target;
        if (e.transport.earliestUs < e.requestUs) e.requestUs = e.transport.earliestUs;
    }
    if (member.context.state == MotorControlRS::ActionState::ACTIVE && !ESS::advanceInputStart(member.context, e, now)) {
        e.kind = ESS::InputStartEventKind::LOST; ESS::advanceInputStart(member.context, e, now);
    }
    a.owner.release(member.sample); member.sample = Rtu::RequestId();
}
// Round-robin sampling through the ordinary owner: the most overdue sample first.
void serviceInputSamples(App& a, uint64_t now, bool admit) {
    auto& s = a.inputStart;
    for (uint8_t i = 0; i < s.view.count; ++i) if (s.members[i].sample.owner) settleSample(a, i, now);
    if (a.owner.needsRecovery() || uart.needsRecovery()) {
        // Sampling cannot continue; the observation ends with a transport error.
        if (s.view.phase == Probe::InputStartPhase::ARMED || s.view.phase == Probe::InputStartPhase::OBSERVING)
            eventAll(a, ESS::InputStartEventKind::FAULT, now);
        return;
    }
    int8_t due = -1; uint64_t dueUs = 0;
    for (uint8_t i = 0; i < s.view.count; ++i) {
        auto& member = s.members[i];
        if (member.sample.owner || member.context.state != MotorControlRS::ActionState::ACTIVE) continue;
        ESS::PreparedInputStart work;
        if (!ESS::nextInputStart(member.context, now, work)) continue;
        if (work.kind == ESS::InputStartWork::CONCLUDE) {
            ESS::InputStartEvent e; e.kind = ESS::InputStartEventKind::TIME; e.operationId = s.view.operationId;
            ESS::advanceInputStart(member.context, e, now);
        } else if (work.kind == ESS::InputStartWork::SAMPLE && admit && (due < 0 || work.eligibleUs < dueUs)) {
            due = static_cast<int8_t>(i); dueUs = work.eligibleUs;
        }
    }
    if (due < 0) return;
    auto& member = s.members[due];
    ESS::PreparedInputStart work;
    if (!ESS::nextInputStart(member.context, now, work) || work.kind != ESS::InputStartWork::SAMPLE) return;
    std::memcpy(member.tx, work.bytes, work.length);
    if (admitInputRead(a, member.sample, member.tx, member.address, ESS::Registers::ERROR_CODE,
                       ESS::INPUT_START_SAMPLE_WORDS, now) == Probe::Action::OK) {
        member.sampleSequence = work.sequence; member.sampleRequestUs = now;
    }
}

// -------------------------------------------------------------- the trigger
// Shared line-isolation truth table (prompt 04, test/line_isolation_cases.json)
// for one non-member; armed members keep their armed facts instead.
void decideRow(const App& a, Probe::InputStartRow& row, uint8_t terminal, uint64_t now, uint8_t memberIndex) {
    const auto set = [&](const char* name, bool admit, const char* reason) {
        row.row[0] = name[0]; row.row[1] = name[1]; row.row[2] = 0; row.admit = admit; row.reason = reason;
    };
    const uint64_t limit = static_cast<uint64_t>(Probe::INPUT_START_MAX_READ_AGE_MS) * 1000;
    if (!row.ioKnown || !row.stateKnown) { set("L1", false, std::strcmp(row.reason, "silent") ? "unreadable" : "silent"); return; }
    row.ioAgeUs = static_cast<uint32_t>(std::min<uint64_t>(now - std::min(now, row.ioStartedUs), UINT32_MAX));
    row.stateAgeUs = static_cast<uint32_t>(std::min<uint64_t>(now - std::min(now, row.stateStartedUs), UINT32_MAX));
    if (row.ioAgeUs > limit || row.stateAgeUs > limit) { set("L1", false, "read older than 1000 ms"); return; }
    for (uint8_t i = 1; i < 5; ++i) if (row.io[i] > 17) { set("L1", false, "input function outside 0..17"); return; }
    if (row.io[0] > 0xF) { set("L1", false, "input polarity bits unknown"); return; }
    if (row.state[0] || (row.state[1] & static_cast<uint16_t>(ESS::MotionStatusBit::ALARM))) { set("L2", false, "alarm"); return; }
    const bool running = (row.state[1] & static_cast<uint16_t>(ESS::MotionStatusBit::RUNNING)) != 0;
    const uint16_t function = row.io[1 + terminal];
    if (row.member) {
        const auto& s = a.inputStart;
        const auto& mv = s.view.members[memberIndex];
        const uint16_t polarity = (row.io[0] >> terminal) & 1u;
        bool witness = false;
        if (s.view.group) {
            const auto* m = groupMember(a, mv.address);
            witness = m && memberWitnessValid(a, *m, mv.stagedWords);
        } else witness = a.rememberedMoveGeneration == a.axis.generation && a.rememberedMoveBinding == a.bindingGeneration &&
            a.rememberedMoveSerial == a.serial.generation && !std::memcmp(a.rememberedMoveWords, mv.stagedWords, sizeof(mv.stagedWords));
        if (running) { set("L4", false, "armed member running"); return; }
        if (function != 7 || polarity != mv.polarity) { set("L4", false, "member assignment changed since arming"); return; }
        if (row.state[1] & static_cast<uint16_t>(ESS::MotionStatusBit::RELEASED)) { set("L4", false, "armed member released"); return; }
        if (!witness) { set("L4", false, "staged words witness ended"); return; }
        set("M", true, "armed member, function 7, stopped, staged words current");
        row.row[1] = 0;
        return;
    }
    if (running) { set("L4", false, "running and not a member"); return; }
    if (function >= 7 && function <= 13) { set("L5", false, "terminal holds a start function 7-13"); return; }
    if (function >= 14 && function <= 17)
        for (uint8_t i = 1; i < 5; ++i)
            if (row.io[i] == 12 || row.io[i] == 13) { set("L6", false, "selector with a 12/13 trigger on the drive"); return; }
    set("L7", true, "no start function on the terminal");
}
int8_t memberIndex(const App& a, uint8_t address) {
    for (uint8_t i = 0; i < a.inputStart.view.count; ++i) if (a.inputStart.members[i].address == address) return static_cast<int8_t>(i);
    return -1;
}
Probe::Action triggerInputStartIn(App& a, uint32_t id, uint8_t output, uint32_t widthMs) {
    auto& s = a.inputStart;
    auto& v = s.view;
    a.inputStartRefusal = Probe::InputStartReply();
    if (!id || id != v.operationId) return refuseInputStart(a, Probe::Action::INVALID, "unknown_session");
    if (v.triggered) return refuseInputStart(a, Probe::Action::UNAVAILABLE, "already_triggered");
    if (v.phase != Probe::InputStartPhase::ARMED) return refuseInputStart(a, Probe::Action::UNAVAILABLE, "not_armed");
    if (output >= BenchIo::OUTPUTS) return refuseInputStart(a, Probe::Action::INVALID, "invalid_output");
    if (!mosfetOutput(output)) return refuseInputStart(a, Probe::Action::INVALID, "relay_output");
    if (!((s.ownedMask >> output) & 1u)) return refuseInputStart(a, Probe::Action::INVALID, "output_not_on_terminal");
    if (widthMs < Probe::INPUT_START_MIN_WIDTH_MS || widthMs > Probe::INPUT_START_MAX_WIDTH_MS)
        return refuseInputStart(a, Probe::Action::INVALID, "width_out_of_range");
    if (!a.options.benchIo || !benchView(a).ready) return refuseInputStart(a, Probe::Action::UNAVAILABLE, "bench_unavailable");
    if (anyOutputActive(a.benchScratch)) return refuseInputStart(a, Probe::Action::BUSY, "output_already_on");
    if (a.owner.needsRecovery() || uart.needsRecovery()) return refuseInputStart(a, Probe::Action::RECOVERY_REQUIRED, "recovery_required");
    uint8_t pendingAddress = 0;
    if (foreignPending(a, pendingAddress)) {
        v.decided = true; v.admitted = false; v.rowCount = 0; v.decisionAddress = pendingAddress;
        std::memcpy(v.decisionRow, "L4", 3); v.decisionReason = "an accepted operation is still pending";
        return refuseInputStart(a, Probe::Action::BUSY, "operation_pending", pendingAddress);
    }
    // Participants: every drive the board declares connected on the terminal;
    // non-members first, members last, so the pre-edge samples are the newest reads.
    uint8_t list[Probe::INPUT_START_MEMBERS] = {}, count = 0;
    for (uint8_t pass = 0; pass < 2; ++pass)
        for (uint8_t i = 0; i < a.boardWiringCount; ++i) {
            const auto& entry = a.boardWiring[i];
            if (!((entry.connectedInputs >> v.terminal) & 1u)) continue;
            if ((memberIndex(a, entry.address) >= 0) != (pass == 1)) continue;
            if (count == Probe::INPUT_START_MEMBERS) return refuseInputStart(a, Probe::Action::UNAVAILABLE, "too_many_participants");
            list[count++] = entry.address;
        }
    for (uint8_t i = 0; i < v.count; ++i) {
        bool listed = false;
        for (uint8_t j = 0; j < count; ++j) listed = listed || list[j] == s.members[i].address;
        if (!listed) return refuseInputStart(a, Probe::Action::UNAVAILABLE, "member_not_on_line", s.members[i].address);
    }
    // Sort each part by address, as the harness truth table reports them.
    for (uint8_t i = 1; i < count; ++i)
        for (uint8_t j = i; j && (memberIndex(a, list[j - 1]) >= 0) == (memberIndex(a, list[j]) >= 0) && list[j - 1] > list[j]; --j)
            std::swap(list[j - 1], list[j]);
    std::memcpy(s.participants, list, sizeof(list)); s.participantCount = count; s.step = 0;
    s.output = output; s.widthMs = widthMs;
    v.outputKnown = true; v.output = output; v.widthMs = widthMs;
    v.decided = v.admitted = false; v.decisionRow[0] = 0; v.decisionAddress = 0; v.decisionReason = "none";
    v.rowCount = count;
    for (uint8_t i = 0; i < count; ++i) {
        v.rows[i] = Probe::InputStartRow();
        v.rows[i].address = list[i]; v.rows[i].member = memberIndex(a, list[i]) >= 0;
    }
    s.triggerOperationMark = a.nextOperationId;
    v.phase = Probe::InputStartPhase::TRIGGERING;
    return Probe::Action::OK;
}
void refuseTrigger(App& a, const char* row, uint8_t address, const char* reason) {
    auto& v = a.inputStart.view;
    v.decided = true; v.admitted = false; v.decisionAddress = address; v.decisionReason = reason;
    v.decisionRow[0] = row[0]; v.decisionRow[1] = row[1]; v.decisionRow[2] = 0;
    v.phase = Probe::InputStartPhase::ARMED; // Nothing was switched; another trigger may follow.
}
void decideTrigger(App& a, uint64_t now) {
    auto& s = a.inputStart;
    auto& v = s.view;
    // D14 ordering: the reads were the last bus traffic and nothing else is pending.
    uint8_t pendingAddress = 0;
    if (a.nextOperationId != s.triggerOperationMark || a.owner.active() || a.owner.pending()) {
        refuseTrigger(a, "L1", 0, "interlock reads were not the last bus traffic"); return;
    }
    if (foreignPending(a, pendingAddress)) { refuseTrigger(a, "L4", pendingAddress, "an accepted operation is still pending"); return; }
    const auto& bench = benchView(a);
    if (!bench.ready) { refuseTrigger(a, "L1", 0, "bench view unavailable"); return; }
    if (anyOutputActive(bench)) { refuseTrigger(a, "L4", 0, "an output is already on"); return; }
    const uint64_t decision = uart.sample();
    Probe::InputStartRow* refused = nullptr;
    for (uint8_t i = 0; i < v.rowCount; ++i) {
        auto& row = v.rows[i];
        const int8_t index = memberIndex(a, row.address);
        decideRow(a, row, v.terminal, decision, index < 0 ? 0 : static_cast<uint8_t>(index));
        if (index >= 0 && row.ioKnown) { v.members[index].assignmentKnown = true; v.members[index].assignment = row.io[1 + v.terminal]; }
        if (!row.admit && !refused) refused = &row;
    }
    if (refused) { refuseTrigger(a, refused->row, refused->address, refused->reason); return; }
    v.decided = v.admitted = true; v.decisionAddress = v.rows[0].address; v.decisionReason = "every participant admitted";
    std::memcpy(v.decisionRow, v.rows[0].row, 3);
    // Before the ON: the isolation witness and the pre-edge sample of each member.
    for (uint8_t i = 0; i < v.rowCount; ++i) {
        const auto& row = v.rows[i];
        const int8_t index = memberIndex(a, row.address);
        if (index < 0) continue;
        ESS::InputStartEvent isolation; isolation.kind = ESS::InputStartEventKind::ISOLATION;
        isolation.operationId = v.operationId; isolation.held = true;
        sendContextEvent(a, static_cast<uint8_t>(index), isolation, now);
        uint8_t frame[19] = {static_cast<uint8_t>(row.address), 3, 14};
        for (uint8_t w = 0; w < 7; ++w) { frame[3 + 2 * w] = static_cast<uint8_t>(row.state[w] >> 8); frame[4 + 2 * w] = static_cast<uint8_t>(row.state[w]); }
        const uint16_t crc = ESS::calcCrc16(frame, 17);
        frame[17] = static_cast<uint8_t>(crc); frame[18] = static_cast<uint8_t>(crc >> 8);
        ESS::InputStartEvent sample; sample.kind = ESS::InputStartEventKind::SAMPLE; sample.operationId = v.operationId;
        sample.preEdge = true; sample.requestUs = row.stateStartedUs;
        sample.transport.kind = MotorControlRS::ReadEventKind::FRAME; sample.transport.target = s.members[index].prerequisites.target;
        sample.transport.frame = frame; sample.transport.length = sizeof(frame); sample.transport.qualified = true;
        sample.transport.earliestUs = sample.transport.latestUs = std::max(row.stateStartedUs, row.stateReplyUs);
        sendContextEvent(a, static_cast<uint8_t>(index), sample, std::max(std::max(now, decision), sample.transport.latestUs));
    }
    // One ON with the hold; its GPIO-write times become the source edges.
    BenchIo::Request on; on.op = BenchIo::Op::ON; on.output = s.output; on.holdMs = s.widthMs;
    a.benchScratch = BenchIo::View();
    const auto result = a.options.benchIo->apply(on, a.benchScratch);
    const auto& out = a.benchScratch.outputs[s.output];
    v.triggered = true;
    if (!out.active || result != BenchIo::Result::OK) {
        v.source = out.active ? Probe::InputSourceState::ACTIVE : Probe::InputSourceState::PORT_FAILED;
        if (!out.active) { finishInputStart(a, "trigger_port_failed", uart.sample()); return; }
    }
    v.source = Probe::InputSourceState::ACTIVE; v.onUs = out.onUs;
    for (uint8_t i = 0; i < v.count; ++i) {
        if (s.members[i].context.state == MotorControlRS::ActionState::ACTIVE)
            ESS::noteInputStartSource(s.members[i].context, v.onUs, true);
        // The input may move the drive outside any host record: its cached state
        // and position reference expire, so the next admission reads fresh state.
        invalidateMotionObservations(a, s.members[i].address);
    }
    for (uint8_t i = 0; i < v.rowCount; ++i) {
        v.rows[i].ioAgeUs = static_cast<uint32_t>(std::min<uint64_t>(v.onUs - std::min(v.onUs, v.rows[i].ioStartedUs), UINT32_MAX));
        v.rows[i].stateAgeUs = static_cast<uint32_t>(std::min<uint64_t>(v.onUs - std::min(v.onUs, v.rows[i].stateStartedUs), UINT32_MAX));
    }
    v.phase = Probe::InputStartPhase::OBSERVING;
}
void serviceInputTrigger(App& a, uint64_t now) {
    auto& s = a.inputStart;
    auto& v = s.view;
    for (uint8_t i = 0; i < v.count; ++i) if (s.members[i].sample.owner) return; // Own samples settle first.
    if (s.triggerRequest.owner) {
        const auto* result = a.owner.result(s.triggerRequest);
        if (!result) return;
        auto& row = v.rows[s.step / 2];
        const bool io = s.step % 2 == 0;
        uint16_t words[7] = {};
        std::size_t count = 0;
        const auto& t = result->transport;
        const bool ok = result->outcome == Rtu::Outcome::SUCCESS && t.reason == Rtu::Reason::FRAME &&
            ESS::parseRegisters(result->raw, t.rxLength, row.address, io ? INPUT_IO_WORDS : ESS::INPUT_START_SAMPLE_WORDS,
                words, 7, count).isOk();
        if (ok && io) { std::memcpy(row.io, words, sizeof(row.io)); row.ioKnown = true; }
        if (ok && !io) {
            std::memcpy(row.state, words, sizeof(row.state)); row.stateKnown = true;
            row.stateReplyUs = t.closureQualified ? t.closureLatestUs : now;
        }
        const bool silent = !ok && !t.rxLength;
        a.owner.release(s.triggerRequest); s.triggerRequest = Rtu::RequestId();
        if (!ok) {
            row.reason = silent ? "silent" : "unreadable";
            decideRow(a, row, v.terminal, now, 0);
            refuseTrigger(a, row.row, row.address, row.reason);
            return;
        }
        ++s.step;
    }
    if (s.step >= 2 * s.participantCount) { decideTrigger(a, now); return; }
    if (a.owner.needsRecovery() || uart.needsRecovery()) { refuseTrigger(a, "L1", v.rows[s.step / 2].address, "unreadable"); return; }
    auto& row = v.rows[s.step / 2];
    const bool io = s.step % 2 == 0;
    const auto admitted = admitInputRead(a, s.triggerRequest, s.triggerTx, row.address, io ? INPUT_IO_FIRST : ESS::Registers::ERROR_CODE,
        io ? INPUT_IO_WORDS : ESS::INPUT_START_SAMPLE_WORDS, now);
    if (admitted == Probe::Action::QUEUE_FULL || admitted == Probe::Action::RESULTS_FULL) return;
    if (admitted != Probe::Action::OK) { refuseTrigger(a, "L1", row.address, "interlock read not admitted"); return; }
    if (io) { row.ioStartedUs = now; row.ioOperationId = s.step + 1u; }
    else { row.stateStartedUs = now; row.stateOperationId = s.step + 1u; }
}
void serviceInputStart(App& a, uint64_t now) {
    auto& s = a.inputStart;
    auto& v = s.view;
    if (v.phase == Probe::InputStartPhase::NONE) return;
    if (v.phase == Probe::InputStartPhase::DONE) {
        // Drain results of reads still in flight when the session ended.
        for (uint8_t i = 0; i < Probe::INPUT_START_MEMBERS; ++i)
            if (s.members[i].sample.owner && a.owner.result(s.members[i].sample)) {
                a.owner.release(s.members[i].sample); s.members[i].sample = Rtu::RequestId();
            }
        if (s.triggerRequest.owner && a.owner.result(s.triggerRequest)) { a.owner.release(s.triggerRequest); s.triggerRequest = Rtu::RequestId(); }
        return;
    }
    if (!inputStartCurrent(a)) { finishInputStart(a, "invalidated", now); return; }
    if (v.phase == Probe::InputStartPhase::STAGING) { serviceInputStaging(a, now); return; }
    serviceInputSource(a, now);
    serviceInputStops(a, now);
    if (v.phase == Probe::InputStartPhase::TRIGGERING) {
        serviceInputSamples(a, now, false);
        if (v.phase == Probe::InputStartPhase::TRIGGERING) serviceInputTrigger(a, now);
    } else serviceInputSamples(a, now, true);
    copyResults(a);
    bool active = false;
    for (uint8_t i = 0; i < v.count; ++i) active = active || s.members[i].context.state == MotorControlRS::ActionState::ACTIVE;
    if (!active && v.phase != Probe::InputStartPhase::DONE) {
        bool samples = false;
        for (uint8_t i = 0; i < v.count; ++i) samples = samples || s.members[i].sample.owner;
        if (!samples && !s.triggerRequest.owner) finishInputStart(a, "none", now);
    }
}
Probe::Action disarmInputStartIn(App& a, uint32_t id) {
    auto& s = a.inputStart;
    a.inputStartRefusal = Probe::InputStartReply();
    if (!id || id != s.view.operationId) return refuseInputStart(a, Probe::Action::INVALID, "unknown_session");
    if (!inputStartActive(a)) return refuseInputStart(a, Probe::Action::ALREADY_TERMINAL, "session_done");
    const uint64_t now = uart.sample();
    if (s.view.phase == Probe::InputStartPhase::STAGING) {
        // Unsent staging never goes out; a frame on the wire settles with its evidence.
        if (!s.view.group) cancelPositionStages(a, s.members[0].address, now);
        else if (auto* m = groupMember(a, s.members[s.next < s.view.count ? s.next : 0].address)) if (m->stage.view.pending) {
            m->stage.view.cancelled = true;
            if (m->stage.request.owner && !a.owner.txAccepted(m->stage.request)) a.owner.cancelUnsent(m->stage.request, now);
        }
    }
    s.disarming = true;
    finishInputStart(a, "disarmed", now);
    return Probe::Action::OK;
}
bool releaseInputStart(App& a) {
    auto& s = a.inputStart;
    if (!s.view.operationId || inputStartActive(a) || s.triggerRequest.owner) return false;
    for (const auto& member : s.members) if (member.sample.owner) return false;
    s.~InputStartSession(); new (&s) App::InputStartSession();
    return true;
}
// Console form: builds the per-member requests (absolute-by from fresh feedback).
Probe::Action inputStartCommand(void* context, const Probe::InputStartCommand& c, const Probe::InputStartView*& out,
                                Probe::InputStartReply& reply) {
    using namespace MotorControlRS;
    App& a = *static_cast<App*>(context);
    Probe::Action result = Probe::Action::OK;
    uint32_t id = 0;
    a.inputStartRefusal = Probe::InputStartReply();
    switch (c.kind) {
    case Probe::InputStartCommandKind::INSPECT:
        if (!c.operationId || c.operationId != a.inputStart.view.operationId)
            result = refuseInputStart(a, Probe::Action::INVALID, "unknown_session");
        break;
    case Probe::InputStartCommandKind::TRIGGER: result = triggerInputStartIn(a, c.operationId, c.output, c.widthMs); id = c.operationId; break;
    case Probe::InputStartCommandKind::DISARM: result = disarmInputStartIn(a, c.operationId); id = c.operationId; break;
    case Probe::InputStartCommandKind::ARM: {
        auto& built = a.inputStart.scratch; // Built in App storage, never on the loop stack.
        built = Probe::InputStartRequest();
        built.kind = c.armKind; built.terminal = c.terminal; built.group = c.group;
        built.mode = c.form == Probe::InputStartArmForm::RELATIVE ? Probe::InputStartMode::RELATIVE : Probe::InputStartMode::ABSOLUTE;
        built.pollMs = c.pollMs; built.windowMs = c.windowMs;
        if (c.armKind != Probe::InputStartKind::POSITION) { result = armInputStartIn(a, built, id); break; }
        if (c.form == Probe::InputStartArmForm::ABSOLUTE && c.group) { result = refuseInputStart(a, Probe::Action::INVALID, "absolute_needs_selected"); break; }
        if (c.group && (!a.group.declared || !a.group.count)) { result = refuseInputStart(a, Probe::Action::UNAVAILABLE, "group_not_declared"); break; }
        built.count = c.group ? static_cast<uint8_t>(std::min<uint8_t>(a.group.count, Probe::INPUT_START_MEMBERS)) : 1;
        if (c.group && a.group.count > Probe::INPUT_START_MEMBERS) { result = refuseInputStart(a, Probe::Action::INVALID, "too_many_members"); break; }
        const uint64_t now = uart.sample();
        for (uint8_t i = 0; result == Probe::Action::OK && i < built.count; ++i) {
            const auto* m = c.group ? &a.group.members[i] : nullptr;
            auto& member = built.members[i];
            member.address = m ? m->address : a.axis.target.address;
            auto& move = member.move;
            move.position.unit = PositionUnit::STEPS; move.position.frame = CoordinateFrame::NATIVE;
            move.position.relative = c.form == Probe::InputStartArmForm::RELATIVE;
            move.position.configurationGeneration = m ? m->axis.generation : a.axis.generation;
            move.speedRpm = c.speedRpm; move.ramp = MoveRamp::VERIFIED_CONFIGURED; move.setup = MoveSetup::WRITE_ALL;
            int64_t target = c.value;
            if (c.form == Probe::InputStartArmForm::ABSOLUTE_BY) {
                const auto& feedback = (m ? m->state : a.stateCache).blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
                int64_t position = 0;
                if (!Probe::fresh(feedback, m ? m->axis.target : a.axis.target, now, a.observationAgeUs()) ||
                    !feedback.value.pairKnown || !signedFeedback(feedback.value.rawPosition, position)) {
                    result = refuseInputStart(a, Probe::Action::UNAVAILABLE, "position_unknown", member.address); break;
                }
                target = position + c.value;
            }
            move.position.value = Rational(target);
        }
        if (result == Probe::Action::OK) result = armInputStartIn(a, built, id);
        break;
    }
    }
    out = &a.inputStart.view;
    reply = result == Probe::Action::OK ? Probe::InputStartReply() : a.inputStartRefusal;
    if (result == Probe::Action::OK) reply.operationId = id ? id : out->operationId;
    return result;
}
