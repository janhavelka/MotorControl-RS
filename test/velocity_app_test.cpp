// SPDX-License-Identifier: MIT
// Exercise the actual cooperative application with simulated SDK/wire evidence.
// Qualification below belongs solely to this fake fixture, never the real bench.
#include "../examples/probe_cli/ProbeApp.cpp"
#include "fakes/esp32_uart/UsbConsoleFixture.h"
#include "ActionEvidenceChecks.h"
#if MOTORCONTROLRS_TEST_IDF
#include "fakes/esp32_uart/FakeUsb.h"
#include "../examples/probe_idf/main/IdfPlatform.cpp"
#include "../examples/common/BoardPins.h"
#else
#include "../examples/probe_cli/ArduinoPlatform.cpp"
#include "../examples/probe_cli/main.cpp"
#endif
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

namespace {
using namespace MotorControlRS;
void fresh() {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = false; writeResponseConfirmed = false;
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit));
    assert(app && uart.ready() && app->owner.valid() && hardware.writes == 0);
    hardware.txCharacterUs = 87;
    assert(uart.startCapture(20, timing().holdUs));
}
void step(uint32_t us = 10) { advanceHardware(hardware.time + us); serviceApplication(); }
void pump(unsigned count = 64) { for (unsigned i = 0; i < count; ++i) step(); }
Probe::ResultView view(uint32_t operation) {
    Probe::ResultView result;
    assert(host(app).result(app, operation, result)); return result;
}
void command(const std::string& text) {
    Serial.input = text; Serial.output.clear();
    for (unsigned i = 0; i < 500 && !Serial.input.empty(); ++i) step();
    assert(Serial.input.empty()); pump();
}
std::vector<uint8_t> crc(std::vector<uint8_t> bytes) {
    const uint16_t value = ESS::calcCrc16(bytes.data(), bytes.size());
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8));
    return bytes;
}
std::vector<uint8_t> registers(uint8_t address, std::initializer_list<uint16_t> words) {
    std::vector<uint8_t> bytes = {address, 3, static_cast<uint8_t>(words.size() * 2)};
    for (const auto word : words) { bytes.push_back(static_cast<uint8_t>(word >> 8)); bytes.push_back(static_cast<uint8_t>(word)); }
    return crc(bytes);
}
void waitTx(uint32_t operation) {
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(findRecord(*app, operation)->requestId); ++i) step();
    if (!app->owner.txAccepted(findRecord(*app, operation)->requestId))
        std::fprintf(stderr,"No TX operation %u pending %d phase %u step %u outcome %u needsRecovery %d writes %u\n",
            operation,view(operation).pending,static_cast<unsigned>(findRecord(*app,operation)->velocity.phase),
            findRecord(*app,operation)->velocity.step,static_cast<unsigned>(findRecord(*app,operation)->velocity.outcome),
            app->owner.needsRecovery(),hardware.writes);
    assert(app->owner.txAccepted(findRecord(*app, operation)->requestId));
}
std::vector<uint8_t> acknowledgement() {
    assert(hardware.tx.size() >= 8);
    if (hardware.tx[1] == 6) return hardware.tx;
    assert(hardware.tx[1] == 16);
    return crc(std::vector<uint8_t>(hardware.tx.begin(), hardware.tx.begin() + 6));
}
void velocityStep(uint32_t operation, const std::vector<uint8_t>& supplied = {}) {
    const uint8_t token = view(operation).velocityContext->step;
    waitTx(operation);
    const auto response = supplied.empty() ? acknowledgement() : supplied;
    scheduleReply(std::max(hardware.writeStarted + hardware.tx.size() * 87 + 1000, hardware.time + 1000), response);
    for (unsigned i = 0; i < 25000 && view(operation).pending && view(operation).velocityContext->step == token; ++i) step();
    assert(!view(operation).pending || view(operation).velocityContext->step != token);
}
void actionStep(uint32_t operation, const std::vector<uint8_t>& supplied = {}) {
    const uint8_t token = view(operation).actionContext->step;
    waitTx(operation);
    const auto response = supplied.empty() ? acknowledgement() : supplied;
    scheduleReply(std::max(hardware.writeStarted + 8 * 87 + 1000, hardware.time + 1000), response);
    for (unsigned i = 0; i < 25000 && view(operation).pending && view(operation).actionContext->step == token; ++i) step();
    assert(!view(operation).pending || view(operation).actionContext->step != token);
}
VelocityRequest request() {
    VelocityRequest r; r.value = Rational(30); r.configurationGeneration = app->axis.generation;
    r.ramp = VelocityRamp::VERIFIED_CONFIGURED; r.durationUs = 200000;
    r.stop.behavior = StopBehavior::CONFIGURED_DECELERATION; return r;
}
void qualify() {
    // Deliberately simulated qualifications. Production starts with every flag
    // false until stated; qualification_routes_app_test covers the console route.
    writeResponseConfirmed = true;
    app->knownTargets[0] |= 2;
    app->axis.nativeMinimum = -1000; app->axis.nativeMaximum = 1000;
    app->axis.supportedRelativeBases = 1;
    auto& p = app->velocityPrerequisites;
    p.target = app->axis.target; p.configurationGeneration = app->axis.generation;
    p.nativeRpmVerified = p.negativeTwosComplementVerified = true;
    p.configuredRampVerified = p.serialInputsPermit = p.readinessQualified = true;
    p.accelerationTime = p.decelerationTime = 100;
    p.rawMotion = 1; p.observedUs = hardware.time; p.maximumAgeUs = 1000000;
    app->configuration.target = app->axis.target;
    app->configuration.operationId = 99;
    app->configuration.wordOrderKnown = true;
    app->configuration.raw.subdivision = 1000;
    app->configuration.raw.algorithm = 2; app->configuration.raw.encoderResolution = 4000;
    app->configuration.raw.inputFunctions[0] = 1;
    app->configuration.raw.inputFunctions[1] = 2; app->configuration.raw.inputFunctions[2] = 3;
    auto& motion = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    motion.valid = true; motion.value.target = app->axis.target;
    motion.value.rawMotion = 1; motion.value.inPosition = motion.value.enabled = true;
    motion.observedEarliestUs = motion.observedLatestUs = hardware.time;
    // Wired bench 07: velocity needs a fresh checked I/O read like a move.
    auto& io = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    io.valid = true; io.value.target = app->axis.target; io.value.rawInputs = 0;
    io.observedEarliestUs = io.observedLatestUs = hardware.time;
}
void inputs(uint16_t functionX3, uint16_t raw, uint16_t polarity = 0) {
    app->configuration.raw.inputFunctions[3] = functionX3; app->configuration.raw.inputPolarity = polarity;
    auto& io = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    io.value.rawInputs = raw; io.value.inputs[3] = (raw & 8) != 0;
    io.observedEarliestUs = io.observedLatestUs = hardware.time;
}
uint32_t admitVelocity(uint32_t commandId = 1) {
    uint32_t id = 0;
    assert(host(app).startVelocity(app, commandId, 1, request(), id) == Probe::Action::OK);
    assert(id && view(id).velocityContext && axisReserved(*app, 1)); return id;
}
uint32_t admitStop(uint32_t commandId = 50) {
    ActionRequest stop; stop.kind = ActionKind::STOP; stop.stop.behavior = StopBehavior::CONFIGURED_DECELERATION;
    uint32_t id = 0;
    assert(host(app).startAction(app, commandId, 1, stop, id) == Probe::Action::OK); return id;
}
void gatesAndParity() {
    fresh(); uint32_t untouched = 77;
    assert(host(app).startVelocity(app, 1, 1, request(), untouched) != Probe::Action::OK);
    assert(untouched == 77 && !hardware.writes);
    command("@1 velocity 30 rpm native 200 configured normal\n");
    assert(!hardware.writes);
    qualify(); auto invalid = request(); invalid.value = Rational(61);
    const auto next = app->nextOperationId;
    assert(startVelocity(app, 2, 1, invalid, untouched) == Probe::Action::INVALID);
    assert(untouched == 77 && app->nextOperationId == next && !app->records[0].operationId);
    assert(app->records[0].kind == App::OperationKind::EMPTY && !app->owner.pending() && !axisReserved(*app, 1));
    const auto reused = app->nextOperationId;
    for (const char value : std::string("@3 velocity 30 rpm native 200 configured normal\n")) app->console.feed(value);
    assert(app->latestOperationId == reused);
    assert(findRecord(*app, reused) == &app->records[0] && app->records[0].kind == App::OperationKind::VELOCITY);
    assert(cancel(app, reused) == Probe::Action::OK); pump(1000);
    assert(!view(reused).pending && !hardware.writes && !axisReserved(*app, 1));
    assert(release(app, reused) == Probe::Action::OK && app->records[0].kind == App::OperationKind::EMPTY);
    for (bool profile : {false, true}) {
        fresh(); qualify(); app->axis.units.commandStepsPerMotorTurn = UnitScale(1000,1,ScaleSource::ASSUMED);
        command(profile ? "@1 velocity 30 rpm native 200 configured normal\n" :
            "@1 velocity 500 steps/s motor 200 configured normal\n");
        if (Serial.output.find("\"result\":\"accepted\"") == std::string::npos)
            std::fprintf(stderr,"Velocity CLI: %s\n",Serial.output.c_str());
        assert(Serial.output.find("\"result\":\"accepted\"") != std::string::npos);
        const auto id = view(0).operationId;
        assert(view(id).velocityContext->prepared.nativeRpm == 30);
        velocityStep(id); assert(hardware.tx.size() == 15 && hardware.tx[3] == 0x1D);
        uint8_t bytes[8];
        Rtu::BusRequest write; write.expected.address = write.expected.target = 1;
        write.expected.targetGeneration = app->bindingGeneration;
        write.expected.function = 6; write.expected.first = 0x1D; write.expected.count = 1;
        write.wire.bytes = bytes; write.wire.length = ESS::buildWriteSingleRegister(1,0x1D,50,bytes,sizeof(bytes));
        write.wire.replyLength = 8; write.wire.responseTimeoutUs = app->serial.timing.responseTimeoutUs;
        write.wire.deadlineUs = nowUs() + REQUEST_US; write.validator = Rtu::essValidator();
        assert(checkAxisWrite(*app,write) == Probe::Action::AXIS_CONFLICT);
        velocityStep(id); assert(hardware.tx[1] == 6 && hardware.tx[5] == 2);
        velocityStep(id, registers(1,{0,4})); assert(view(id).velocityContext->runningObserved);
        const auto due = view(id).velocityContext->stopDueUs;
        // A settled wait yields stop on the immutable finite boundary.
        assert(!findRecord(*app,id)->requestId.owner);
        advanceHardware(due); serviceApplication();
        assert(nowUs() >= due && view(id).velocityContext->deadlineUs > due);
        velocityStep(id); assert(hardware.tx[5] == 0 && hardware.tx[4] == 1);
        velocityStep(id, registers(1,{0,1}));
        assert(!view(id).pending && !view(id).velocityContext->needsStop);
        assert(view(id).velocityContext->completion == ActionCompletion::OBSERVED && !axisReserved(*app,1));
        assert(host(app).cancel(app,id) == Probe::Action::ALREADY_TERMINAL);
    }
}
void externalStopAndLocalCancel() {
    fresh(); qualify(); const auto id = admitVelocity();
    velocityStep(id); velocityStep(id);
    velocityStep(id,registers(1,{0,4}));
    assert(view(id).velocityContext->runningObserved);
    const auto stop = admitStop();
    for (unsigned i=0;i<25000 && view(id).pending;++i) step();
    assert(!view(id).pending && view(id).interruptedByStop && view(id).velocityContext->needsStop);
    actionStep(stop); actionStep(stop,registers(1,{0,1}));
    assert(!axisReserved(*app,1) && view(id).velocityContext->needsStop);
    // Original uncertainty survives a later independent checked stop.
    assert(view(id).velocityContext->outcome == ActionOutcome::CANCELLED);
    fresh(); qualify(); const auto cancelled = admitVelocity();
    assert(host(app).cancel(app,cancelled) == Probe::Action::OK);
    pump(1000); assert(!view(cancelled).pending && !hardware.writes);
}
void pressureAndPhysicalCancellation() {
    fresh(); qualify(); uint32_t retained[7] = {};
    for (unsigned i=0;i<7;++i) {
        assert(probe(app,10+i,2,retained[i]) == Probe::Action::OK);
        waitTx(retained[i]); scheduleReply(hardware.time+2000,registers(2,{0x4EEA}));
        for (unsigned j=0;j<25000 && view(retained[i]).pending;++j) step();
        assert(!view(retained[i]).pending);
    }
    const auto id=admitVelocity(); waitTx(id);
    const auto stopping=admitStop(); velocityStep(id);
    assert(!view(id).pending && view(id).velocityContext->execution == ActionExecution::NOT_TRANSMITTED);
    assert(view(id).velocityContext->uncertain && view(id).interruptedByStop);
    actionStep(stopping); actionStep(stopping,registers(1,{0,1}));
    assert(!axisReserved(*app,1));
    for (auto kept:retained) assert(view(kept).probe.outcome == Rtu::Outcome::SUCCESS);
    // Local cancellation settles accepted physical staging; it cannot truncate TX.
    fresh(); qualify(); const auto cancelled=admitVelocity(); waitTx(cancelled);
    const uint64_t physicalEnd=hardware.writeStarted+hardware.tx.size()*87;
    assert(cancel(app,cancelled) == Probe::Action::OK);
    for (unsigned i=0;i<25000 && view(cancelled).pending;++i) step();
    assert(!view(cancelled).pending && hardware.deReleasedAt >= physicalEnd && hardware.writes == 1);
    assert(view(cancelled).velocityContext->uncertain && axisReserved(*app,1));
}
void delayedEvidenceAndRecovery() {
    fresh(); qualify(); const auto id=admitVelocity(); velocityStep(id); velocityStep(id); waitTx(id);
    const auto due=view(id).velocityContext->stopDueUs;
    scheduleReply(hardware.writeStarted+8*87+1000,registers(1,{0,4}));
    advanceHardware(due+100000); serviceApplication(); // Beyond the declared 50-ms normal-stop service interval.
    const auto& context=*view(id).velocityContext;
    assert(context.runningObserved && context.serviceMissed && context.needsStop);
    assert(context.phase == ESS::VelocityPhase::STOPPING && context.stopDueUs == due);
    velocityStep(id); velocityStep(id,registers(1,{0,1}));
    assert(!view(id).pending && !view(id).velocityContext->needsStop);
    assert(view(id).velocityContext->outcome == ActionOutcome::DEADLINE);
    // Explicit recovery invalidates a still-active old-generation continuation.
    fresh(); qualify(); const auto recovering=admitVelocity(); velocityStep(recovering); waitTx(recovering);
    const uint32_t oldGeneration=app->bindingGeneration;
    uint32_t recovery=0; assert(recover(app,99,recovery) == Probe::Action::OK);
    for (unsigned i=0;i<250000 && (view(recovering).pending || view(recovery).pending);++i) step();
    assert(!view(recovering).pending && !view(recovery).pending && app->bindingGeneration == oldGeneration+1);
    assert(view(recovering).velocityContext->needsStop && axisReserved(*app,1));
    assert(view(recovery).recovery && hardware.writes == 2);
}
void expiredPollRetainsTransportEvidence() {
    for (bool transmitted : {false, true}) {
        fresh(); qualify(); const auto id = admitVelocity();
        velocityStep(id); velocityStep(id); velocityStep(id, registers(1,{0,4}));
        const auto due = view(id).velocityContext->stopDueUs;
        const auto deadline = view(id).velocityContext->deadlineUs;
        // Stop before the next owner service dispatches the admitted poll.
        for (unsigned i = 0; i < 25000 && !findRecord(*app,id)->requestId.owner; ++i) step();
        const auto token = findRecord(*app,id)->requestId;
        assert(token.owner && !app->owner.txAccepted(token) && hardware.writes == 3);
        if (transmitted) {
            hardware.txCharacterUs = 0; // UART accepts the poll but cannot drain it.
            waitTx(id);
        }
        advanceHardware(transmitted ? deadline : due); // ISR capture survives this task gap.
        app->owner.service(uart.sample());
        const auto* settled = app->owner.result(token);
        assert(settled && settled->expected.function == 3 && settled->expected.first == 6);
        const auto reason = settled->transport.reason;
        if (transmitted) {
            assert(reason == Rtu::Reason::REQUEST_DEADLINE && settled->transport.txAccepted == 8 &&
                !settled->transport.txComplete);
        } else {
            assert(settled->outcome == Rtu::Outcome::DISPATCH_EXPIRED && !settled->transport.txAccepted &&
                !settled->transport.rxLength && !settled->executionUnknown);
        }
        serviceApplication();
        assert(!app->owner.result(token));
        if (transmitted) {
            const auto& c = *view(id).velocityContext;
            assert(!view(id).pending && c.state == ActionState::FAILED && c.outcome == ActionOutcome::DEADLINE);
            assert(c.status.detail == static_cast<int32_t>(VelocityError::DEADLINE_EXPIRED));
            assert(c.serviceMissed && c.needsStop && c.uncertain && c.runningObserved);
            assert(c.failureEvidence.step == 3 && c.failureEvidence.event == ReadEventKind::DEADLINE &&
                c.failureEvidence.txAccepted == 8 && !c.failureEvidence.txComplete &&
                c.failureEvidence.transportDetail == static_cast<int32_t>(reason) &&
                c.failureEvidence.deliveredUs == c.servicedUs);
            assert(c.execution == ActionExecution::ACKNOWLEDGED && c.stop.execution == ActionExecution::NOT_TRANSMITTED);
            assert(c.stopDueUs == due && c.deadlineUs == deadline && hardware.writes == 4 && axisReserved(*app,1));
            const auto failure = c.failureEvidence;
            pump(2000);
            assert(hardware.writes == 4 && !app->recovery.operationId);
            checkSameActionEvidence(failure, view(id).velocityContext->failureEvidence);
        } else {
            const auto& waiting = *view(id).velocityContext;
            assert(view(id).pending && waiting.phase == ESS::VelocityPhase::STOPPING &&
                waiting.outcome == ActionOutcome::NONE && !waiting.failureEvidence.deliveredUs);
            velocityStep(id); velocityStep(id, registers(1,{0,0}));
            const auto& c = *view(id).velocityContext;
            assert(!view(id).pending && c.outcome == ActionOutcome::OBSERVED && !c.needsStop && !c.serviceMissed);
            assert(c.stopDueUs == due && c.deadlineUs == deadline && hardware.writes == 5);
        }
    }
}
void urgentPressureDefersWithinOriginalDeadline() {
    for (bool releaseReserved : {false,true}) {
        fresh(); qualify();
        uint8_t bytes[8]; Rtu::BusRequest r; r.wire.bytes=bytes;
        r.wire.length=ESS::buildProbe(2,bytes,sizeof(bytes)); r.wire.replyLength=7;
        r.wire.responseTimeoutUs=app->serial.timing.responseTimeoutUs; r.wire.replyGapUs=app->serial.timing.replyGapUs;
        r.wire.deadlineUs=nowUs()+REQUEST_US;
        r.expected.address=r.expected.target=2; r.expected.targetGeneration=app->bindingGeneration;
        r.expected.function=3; r.expected.count=1; r.validator=Rtu::essValidator();
        // Every urgent result stays occupied: the stop and escalation records of
        // the selected address and of the drive-group pool.
        Rtu::RequestId reserved[URGENT_RECORDS];
        for (auto& slot : reserved) {
            r.wire.deadlineUs=nowUs()+REQUEST_US;
            assert(app->owner.admitUrgent(r,nowUs(),slot) == Rtu::BusAdmission::ACCEPTED);
            for (unsigned i=0;i<25000 && !app->owner.txAccepted(slot);++i) step();
            scheduleReply(hardware.time+2000,registers(2,{0x4EEA}));
            for (unsigned i=0;i<25000 && !app->owner.result(slot);++i) step();
            assert(app->owner.result(slot));
        }
        // Fill the quota before the run, so its own polls never compete with the fixture.
        const auto id=admitVelocity(); velocityStep(id); velocityStep(id);
        velocityStep(id,registers(1,{0,4}));
        const uint64_t deadline=view(id).velocityContext->deadlineUs;
        advanceHardware(view(id).velocityContext->stopDueUs); serviceApplication();
        assert(view(id).pending && view(id).velocityContext->phase == ESS::VelocityPhase::STOPPING);
        assert(!findRecord(*app,id)->requestId.owner && !app->owner.needsRecovery());
        if (releaseReserved) {
            for (const auto& slot : reserved) assert(app->owner.release(slot));
            serviceApplication();
            velocityStep(id); velocityStep(id,registers(1,{0,1}));
            assert(!view(id).pending && !view(id).velocityContext->needsStop);
        } else {
            advanceHardware(deadline); serviceApplication();
            assert(!view(id).pending && view(id).velocityContext->needsStop && hardware.writes == 3 + URGENT_RECORDS);
            assert(view(id).velocityContext->stop.outcome == ActionOutcome::DEADLINE);
        }
        assert(view(id).velocityContext->deadlineUs == deadline);
    }
}
void normalStopAllowsFullConfiguredRamp() {
    fresh(); qualify(); app->velocityPrerequisites.decelerationTime = 2000;
    const auto id = admitVelocity();
    velocityStep(id); velocityStep(id); velocityStep(id, registers(1,{0,4}));
    advanceHardware(view(id).velocityContext->stopDueUs); serviceApplication();
    velocityStep(id); // One stop write after the finite running interval.
    const auto stopAck = view(id).velocityContext->stop.writeEvidence.deliveredUs;
    const auto writes = hardware.writes;
    unsigned polls = 0;
    while (hardware.time - stopAck < 2000000) {
        velocityStep(id, registers(1,{0,4})); ++polls;
        assert(view(id).pending);
    }
    velocityStep(id, registers(1,{0,0}));
    assert(view(id).velocityContext->completion == ActionCompletion::OBSERVED);
    assert(!view(id).velocityContext->needsStop && !axisReserved(*app,1));
    assert(hardware.writes == writes + polls + 1);
}
void healthyDurationSweepSendsOneStop() {
    unsigned afterBoundary = 0;
    for (unsigned durationMs = 200; durationMs <= 300; ++durationMs) {
        fresh(); qualify(); auto r = request(); r.durationUs = durationMs * 1000;
        uint32_t id = 0; assert(startVelocity(app, 1, 1, r, id) == Probe::Action::OK);
        const auto due = view(id).velocityContext->stopDueUs;
        const auto deadline = view(id).velocityContext->deadlineUs;
        unsigned seen = 0, setups = 0, starts = 0, stops = 0;
        while (view(id).pending && hardware.time <= deadline) {
            step();
            if (hardware.writes == seen) continue;
            seen = hardware.writes;
            const auto& tx = hardware.tx;
            if (tx[1] == 16) ++setups;
            if (tx[1] == 6 && tx[3] == 0x27 && tx[4] == 0 && tx[5] == 2) ++starts;
            if (tx[1] == 6 && tx[3] == 0x27 && tx[4] == 1 && tx[5] == 0) {
                ++stops; assert(hardware.writeStarted >= due);
            }
            const auto response = tx[1] == 3 ? registers(1, {0, static_cast<uint16_t>(stops ? 0 : 4)}) : acknowledgement();
            scheduleReply(std::max(hardware.writeStarted + tx.size()*87 + 1000, hardware.time + 1000), response);
        }
        const auto& c = *view(id).velocityContext;
        assert(!view(id).pending && c.outcome == ActionOutcome::OBSERVED && !c.needsStop);
        assert(c.stopDueUs == due && c.deadlineUs == deadline && !c.serviceMissed);
        assert(!app->owner.needsRecovery() && !app->recovery.operationId);
        assert(setups == 1 && starts == 1 && stops == 1);
        if (c.lastObservation.latestUs >= due) ++afterBoundary;
    }
    assert(afterBoundary); // The sweep includes real owner replies closing after dispatch ended.
}
void ownedStopSurvivesLostStartOrPoll() {
    for (bool lostPoll : {false, true}) {
        fresh(); qualify(); const auto id = admitVelocity();
        velocityStep(id);
        if (lostPoll) velocityStep(id);
        waitTx(id); // Leave this start or observation unanswered.
        const auto due = view(id).velocityContext->stopDueUs;
        const auto deadline = view(id).velocityContext->deadlineUs;
        const auto generation = view(id).velocityContext->target.generation;
        const auto writes = hardware.writes;
        while (hardware.writes == writes && view(id).pending && hardware.time < deadline) step(100);
        assert(view(id).pending && hardware.writes == writes + 1);
        assert(hardware.tx == crc({1,6,0,0x27,1,0}));
        const auto beforeStop = *view(id).velocityContext;
        assert(beforeStop.phase == ESS::VelocityPhase::STOPPING);
        assert(beforeStop.outcome == (lostPoll ? ActionOutcome::TRANSPORT_ERROR : ActionOutcome::DEADLINE));
        assert(beforeStop.execution == (lostPoll ? ActionExecution::ACKNOWLEDGED : ActionExecution::UNKNOWN));
        assert(beforeStop.target.generation == generation && beforeStop.stop.target.generation == generation + 1);
        assert(beforeStop.stopDueUs == due && beforeStop.deadlineUs == deadline);
        const auto recovery = app->recovery.operationId;
        assert(recovery && app->recovery.automatic && !view(recovery).pending);
        assert(view(recovery).recoveryResult.outcome == Rtu::RecoveryOutcome::RECOVERED);
        velocityStep(id); velocityStep(id, registers(1,{0,0}));
        const auto& c = *view(id).velocityContext;
        assert(!view(id).pending && c.outcome == beforeStop.outcome && c.status.code == beforeStop.status.code);
        assert(c.execution == beforeStop.execution && c.completion == ActionCompletion::OBSERVED && !c.needsStop);
        assert(c.stop.execution == ActionExecution::ACKNOWLEDGED && !axisReserved(*app,1));
        assert(c.stopDueUs == due && c.deadlineUs == deadline && hardware.writes == writes + 2);
        assert(c.triggerEvidence.txAccepted == beforeStop.triggerEvidence.txAccepted &&
            c.triggerEvidence.executionUnknown == beforeStop.triggerEvidence.executionUnknown &&
            c.failureEvidence.transportDetail == beforeStop.failureEvidence.transportDetail);
        const auto settledWrites = hardware.writes; pump(2000);
        assert(hardware.writes == settledWrites && app->recovery.operationId == recovery);
    }
}
void lostOwnedStopNeverReplays() {
    fresh(); qualify(); const auto id = admitVelocity();
    velocityStep(id); velocityStep(id); velocityStep(id, registers(1,{0,4}));
    advanceHardware(view(id).velocityContext->stopDueUs); serviceApplication(); waitTx(id);
    assert(hardware.tx == crc({1,6,0,0x27,1,0}));
    const auto writes = hardware.writes;
    while (view(id).pending && hardware.time < view(id).velocityContext->deadlineUs) step(100);
    assert(!view(id).pending && view(id).velocityContext->stop.execution == ActionExecution::UNKNOWN);
    assert(view(id).velocityContext->needsStop && axisReserved(*app,1));
    assert(app->owner.needsRecovery() && !app->recovery.operationId && hardware.writes == writes);
    pump(2000); assert(hardware.writes == writes);
}
std::string recoveryJson(const std::string& output) {
    const auto begin = output.find("\"stop_recovery\":");
    const auto end = output.find(",\"host_serial\":", begin);
    assert(begin != std::string::npos && end != std::string::npos);
    return output.substr(begin, end - begin);
}
void ownedStopBeforeTxRetainsSettlement(bool emit = false) {
    for (bool loseStopReply : {false, true}) {
        fresh(); qualify(); command("@31 velocity 30 rpm native 200 configured normal\n");
        const auto id = app->latestOperationId;
        assert(view(id).pending && view(id).velocityContext);
        velocityStep(id); velocityStep(id); velocityStep(id, registers(1,{0,4}));
        const auto due = view(id).velocityContext->stopDueUs;
        const auto deadline = view(id).velocityContext->deadlineUs;
        advanceHardware(due); serviceApplication();
        const auto writes = hardware.writes;
        scheduleReply(hardware.time + 1000, std::vector<uint8_t>(120, 0x55), 87, 913);
        while (hardware.writes == writes && view(id).pending && hardware.time < deadline) step(100);
        const auto* record = findRecord(*app,id);
        assert(hardware.writes == writes + 1 && hardware.tx == crc({1,6,0,0x27,1,0}));
        assert(record->stopBeforeRecoveryKnown && record->stopBeforeRecovery.transport.reason == Rtu::Reason::BUS_TIMEOUT);
        assert(!record->stopBeforeRecovery.transport.txAccepted && !record->stopBeforeRecovery.executionUnknown);
        assert(record->stopBeforeRecovery.expected.value == 0x100 && record->stopBeforeRecovery.txLength == 8);
        assert(record->stopBeforeRecovery.deadlineUs == deadline && record->stopRecoveryOperationId == app->recovery.operationId);
        const auto recovery = app->recovery.operationId;
        assert(recovery && view(recovery).recoveryResult.outcome == Rtu::RecoveryOutcome::RECOVERED);
        if (!loseStopReply) { velocityStep(id); velocityStep(id, registers(1,{0,0})); }
        else while (view(id).pending && hardware.time < deadline) step(100);
        const auto& c = *view(id).velocityContext;
        assert(!view(id).pending && c.stopDueUs == due && c.deadlineUs == deadline);
        assert(c.stop.execution == (loseStopReply ? ActionExecution::UNKNOWN : ActionExecution::ACKNOWLEDGED));
        assert(c.needsStop == loseStopReply && c.runningObserved);
        assert(hardware.writes == writes + (loseStopReply ? 1 : 2));
        assert(findRecord(*app,id)->stopBeforeRecoveryKnown && !findRecord(*app,id)->stopBeforeRecovery.transport.txAccepted);
        const auto settledWrites = hardware.writes;
        for (unsigned i = 0; i < 40000; ++i) step(100);
        assert(hardware.writes == settledWrites && app->recovery.operationId == recovery);
        assert(Serial.output.find("\"type\":\"velocity\"") != std::string::npos);
        const auto retained = recoveryJson(Serial.output);
        const auto ownerRecovery = findRecord(*app,id)->stopRecoveryResult.id;
        assert(retained.find("\"operation_id\":" + std::to_string(recovery)) != std::string::npos &&
            retained.find("\"pending\":false,\"result\":{\"id\":" + std::to_string(ownerRecovery)) != std::string::npos &&
            retained.find("\"outcome\":\"recovered\"") != std::string::npos &&
            retained.find("\"before_attempt\":{") != std::string::npos &&
            retained.find("\"transport\":\"BUS_TIMEOUT\",\"tx_accepted\":0") != std::string::npos);
        command("@90 result " + std::to_string(id) + "\n"); pump(2000);
        assert(recoveryJson(Serial.output) == retained);
        command("@91 recover\n"); const auto later = app->recovery.operationId;
        assert(later && later != recovery);
        for (unsigned i = 0; i < 250000 && view(later).pending; ++i) step();
        assert(!view(later).pending && view(later).recoveryResult.outcome == Rtu::RecoveryOutcome::RECOVERED);
        pump(2000);
        command("@92 result " + std::to_string(id) + "\n"); pump(2000);
        assert(recoveryJson(Serial.output) == retained && hardware.writes == settledWrites);
        assert(view(id).velocityContext->stop.execution ==
            (loseStopReply ? ActionExecution::UNKNOWN : ActionExecution::ACKNOWLEDGED));
        if (emit) {
            std::fwrite(Serial.output.data(), 1, Serial.output.size(), stdout);
            return;
        }
    }
}
void ownedStopRecoveryFailureAndCancellation() {
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        fresh(); qualify(); const auto id = admitVelocity(); velocityStep(id);
        if (scenario == 0) hardware.txCharacterUs = 0; // Accepted trigger bytes never physically settle.
        waitTx(id);
        const auto deadline = view(id).velocityContext->deadlineUs;
        const auto due = view(id).velocityContext->stopDueUs;
        while (!findRecord(*app,id)->stopHeld && view(id).pending && hardware.time < deadline) step(100);
        assert(findRecord(*app,id)->stopHeld && app->recovery.operationId && hardware.writes == 2);
        const auto recovery = app->recovery.operationId;
        const auto originalOutcome = view(id).velocityContext->outcome;
        uint32_t external = 0;
        if (scenario == 1) assert(cancel(app, id) == Probe::Action::OK);
        else if (scenario == 2) { advanceHardware(deadline); serviceApplication(); }
        else if (scenario == 3) {
            ActionRequest stop; stop.kind = ActionKind::STOP; stop.stop.behavior = StopBehavior::DIRECT;
            assert(startAction(app, 50, 1, stop, external) == Probe::Action::OK);
        }
        while (view(id).pending && hardware.time <= deadline) step(100);
        const auto& c = *view(id).velocityContext;
        assert(!view(id).pending && c.needsStop && c.outcome == originalOutcome);
        assert(c.stop.execution == ActionExecution::NOT_TRANSMITTED && !c.stop.writeEvidence.txAccepted);
        assert(c.stopDueUs == due && c.deadlineUs == deadline && c.stop.deadlineUs == deadline);
        if (scenario == 0) assert(view(recovery).recoveryResult.outcome == Rtu::RecoveryOutcome::EXPIRED);
        if (scenario == 1 || scenario == 3) assert(c.stop.outcome == ActionOutcome::CANCELLED);
        if (scenario == 2) assert(c.stop.outcome == ActionOutcome::DEADLINE);
        if (scenario == 3) {
            while (hardware.writes == 2 && view(external).pending && hardware.time < deadline) step(100);
            assert(hardware.writes == 3 && hardware.tx == crc({1,6,0,0x27,2,0}));
            actionStep(external); actionStep(external, registers(1,{0,0}));
            assert(!view(external).pending && view(external).actionContext->completion == ActionCompletion::OBSERVED);
            assert(!axisReserved(*app,1) && view(id).velocityContext->needsStop);
        } else assert(hardware.writes == 2 && axisReserved(*app,1));
        const auto writes = hardware.writes;
        for (unsigned i = 0; i < 40000; ++i) step(100);
        assert(hardware.writes == writes && app->recovery.operationId == recovery);
        assert(view(id).velocityContext->outcome == originalOutcome);
    }
}
const std::vector<uint8_t> PARAMETER_REPLY = {1,3,6,0,0x3C,0,0x32,0,0x64,0xD1,0x54};
uint32_t parameterRead() {
    uint32_t id = 0; assert(submitVelocityParameterRead(id));
    assert(id && view(id).typedRead && view(id).pending); return id;
}
void parameterReply(uint32_t id, const std::vector<uint8_t>& bytes = PARAMETER_REPLY) {
    waitTx(id);
    const std::vector<uint8_t> expected = {1,3,0,0x1D,0,3,0x95,0xCD};
    assert(hardware.tx == expected);
    scheduleReply(std::max(hardware.writeStarted + 8*87 + 1000, hardware.time + 1000), bytes);
    for (unsigned i=0;i<25000 && view(id).pending;++i) step();
    assert(!view(id).pending); pump();
}
void parameterReadApplicationPath() {
    fresh(); uint32_t unchanged = 77;
    ESS::ReadContext copied; copied.operationId = 999;
    assert(!copyReadResult(0, copied) && copied.operationId == 999);
    app->discovery.owned = true;
    assert(!submitVelocityParameterRead(unchanged) && unchanged == 77 && !hardware.writes);
    app->discovery.owned = false;
    assert(typedRead(app,1,2,ESS::ReadKind::VELOCITY_PARAMETERS,unchanged) == Probe::Action::INVALID);
    assert(unchanged == 77 && !hardware.writes);
    const auto generation = app->axis.generation;
    const auto id = parameterRead();
    assert(copyReadResult(id,copied) && copied.state == ReadState::ACTIVE);
    assert(!releaseOperation(id));
    ESS::VelocityParametersObservation result; result.rawSpeed = 999;
    assert(!ESS::getVelocityParameters(copied,result) && result.rawSpeed == 999);
    parameterReply(id);
    assert(copyReadResult(id,copied) && ESS::getVelocityParameters(copied,result));
    assert(result.rawSpeed == 60 && result.rawAccelerationTime == 50 && result.rawDecelerationTime == 100);
    assert(result.configurationGeneration == generation && copied.completedSteps == 1 && hardware.writes == 1);
    assert(!app->velocityPrerequisites.nativeRpmVerified && !app->velocityPrerequisites.configuredRampVerified &&
        !app->velocityPrerequisites.readinessQualified);
    assert(app->velocityParameters.operationId == id);
    command("status\n"); pump(1000); // Human text must not recycle an API result.
    assert(copyReadResult(id,copied) && copied.state == ReadState::SUCCEEDED);
    assert(releaseOperation(id));
    copied.operationId = 999;
    assert(!copyReadResult(id,copied) && copied.operationId == 999 && !releaseOperation(id));
    const auto again = parameterRead(); parameterReply(again);
    assert(hardware.writes == 2 && again != id); // A cache cannot substitute for a new transfer.
    assert(releaseOperation(again));
    qualify(); command("@30 velocity 30 rpm native 200 configured normal\n");
    const auto motion = app->latestOperationId;
    assert(!copyReadResult(motion,copied) && copied.operationId == 999);
    assert(cancel(app,motion) == Probe::Action::OK); pump(1000); assert(releaseOperation(motion));
}
void parameterReadQualificationAndHistoricalResults() {
    fresh(); qualify();
    app->velocityPrerequisites.accelerationTime = 50;
    const auto desired = app->simple.desired;
    const auto id = parameterRead(); parameterReply(id);
    assert(app->velocityPrerequisites.configuredRampVerified && app->velocityPrerequisites.readinessQualified);
    const auto observedUs = app->velocityPrerequisites.observedUs;
    const auto changed = parameterRead();
    parameterReply(changed,{1,3,6,0xFF,0xFF,7,0xD0,7,0xD1,0xE3,0x8F});
    assert(!app->velocityPrerequisites.configuredRampVerified && !app->velocityPrerequisites.readinessQualified);
    assert(app->velocityPrerequisites.accelerationTime == 50 && app->velocityPrerequisites.decelerationTime == 100 &&
        app->velocityPrerequisites.observedUs == observedUs);
    assert(app->simple.desired.speedRpm == desired.speedRpm); // Observed JOG speed is not desired position speed.
    for (unsigned stale = 0; stale < 3; ++stale) {
        fresh(); const auto historical = parameterRead();
        const auto generation = app->axis.generation;
        if (stale == 0) ++app->axis.generation;
        else if (stale == 1) ++app->bindingGeneration;
        else { app->velocityParameters.operationId = historical + 1; app->velocityParameters.rawSpeed = 321; }
        parameterReply(historical);
        ESS::ReadContext copy; ESS::VelocityParametersObservation out;
        assert(copyReadResult(historical,copy) && ESS::getVelocityParameters(copy,out));
        assert(out.configurationGeneration == generation && out.rawSpeed == 60);
        assert(app->velocityParameters.operationId == (stale == 2 ? historical + 1 : 0));
        if (stale == 2) assert(app->velocityParameters.rawSpeed == 321);
    }
}
void parameterReadConsoleAndCapacity() {
    fresh();
    for (const char* invalid : {"@1 velocity read extra", "@2 velocity read 1 extra", "@3 velocity read 0",
        "@4 velocity read 248", "@5 velocity read -1", "@6 velocity read 2", "@7 velocity reads"}) {
        command(std::string(invalid)+"\n");
        assert(Serial.output.find("\"ok\":false") != std::string::npos && !hardware.writes);
    }
    command("@8 velocity read\n");
    assert(Serial.output.find("\"command\":\"velocity-read\"") != std::string::npos &&
        Serial.output.find("\"read_kind\":\"velocity_parameters\"") != std::string::npos);
    const auto id = app->latestOperationId;
    command("@9 result "+std::to_string(id)+"\n");
    assert(Serial.output.find("\"read_kind\":\"velocity_parameters\"") != std::string::npos);
    parameterReply(id);
    assert(Serial.output.find("\"raw_acceleration_time\":50") != std::string::npos);
    command("@10 result "+std::to_string(id)+"\n");
    assert(Serial.output.find("\"configuration_generation\":") != std::string::npos);
    assert(releaseOperation(id));
    command("@11 result "+std::to_string(id)+"\n");
    assert(Serial.output.find("\"ok\":false") != std::string::npos);
    fresh(); const auto cancelled = parameterRead();
    assert(cancel(app,cancelled) == Probe::Action::OK); pump(1000);
    ESS::ReadContext copy;
    assert(copyReadResult(cancelled,copy) && copy.outcome == ReadOutcome::CANCELLED && !hardware.writes);
    assert(releaseOperation(cancelled));
    fresh(); uint32_t retained[REQUEST_CAPACITY] = {};
    for (auto& kept : retained) { kept = parameterRead(); parameterReply(kept); }
    uint32_t untouched = 77;
    assert(!submitVelocityParameterRead(untouched) && untouched == 77 && hardware.writes == REQUEST_CAPACITY);
    for (auto kept : retained) assert(copyReadResult(kept,copy) && copy.state == ReadState::SUCCEEDED);
    for (auto kept : retained) assert(releaseOperation(kept));
}
void parameterReadHumanTerminal() {
    const std::vector<uint8_t> unknownReply = {1,3,6,0xFF,0xFF,7,0xD0,7,0xD1,0xE3,0x8F};
    for (const bool unknown : {false, true}) {
        fresh();
        const auto desired = app->simple.desired;
        const auto generation = app->axis.generation;
        command("velocity read\n");
        const auto id = app->latestOperationId;
        assert(id && view(id).pending && view(id).typedRead && !findRecord(*app,id)->programmaticRead);
        parameterReply(id, unknown ? unknownReply : PARAMETER_REPLY);
        pump(1000); // Drain the real asynchronous human terminal before another command can recycle it.
        assert(Serial.output.find("[OK] velocity-read") != std::string::npos);
        assert(Serial.output.find(unknown ? "Speed (raw): 65535" : "Speed (raw): 60") != std::string::npos);
        assert(Serial.output.find(unknown ? "Raw acceleration time: 2000" : "Raw acceleration time: 50") != std::string::npos);
        assert(Serial.output.find(unknown ? "Raw deceleration time: 2001" : "Raw deceleration time: 100") != std::string::npos);
        assert(Serial.output.find("Acceleration word within numeric policy: yes") != std::string::npos);
        assert(Serial.output.find(unknown ? "Deceleration word within numeric policy: no" :
            "Deceleration word within numeric policy: yes") != std::string::npos);
        for (const char* text : {"Speed signed resolution: unresolved signed encoding",
            "Speed scale resolution: unresolved scale", "Ramp scale resolution: unresolved scale",
            "Range flags use library numeric policy bounds; drive limits remain unqualified."})
            assert(Serial.output.find(text) != std::string::npos);
        ESS::ReadContext retained;
        assert(copyReadResult(id,retained) && retained.state == ReadState::SUCCEEDED);
        assert(hardware.writes == 1 && hardware.tx[1] == 3 && !app->owner.pending() && !axisReserved(*app,1));
        const auto& p = app->velocityPrerequisites;
        assert(!p.nativeRpmVerified && !p.negativeTwosComplementVerified && !p.configuredRampVerified &&
            !p.serialInputsPermit && !p.readinessQualified && p.observedUs == 0);
        assert(app->axis.generation == generation && !app->axis.originKnown && !app->coordinateReference.nativeKnown);
        const auto& current = app->simple.desired;
        assert(current.speedRpm == desired.speedRpm && current.acceleration == desired.acceleration &&
            current.deceleration == desired.deceleration && current.accelerationKnown == desired.accelerationKnown &&
            current.decelerationKnown == desired.decelerationKnown && current.setup == desired.setup &&
            current.scaleKnown == desired.scaleKnown && current.stepsPerTurn.numerator == desired.stepsPerTurn.numerator &&
            current.stepsPerTurn.denominator == desired.stepsPerTurn.denominator);
        assert(releaseOperation(id));
    }
}
// Wired bench 06 Implement 4: `velocity restore` writes the three JOG words
// 0x001D..0x001F of an earlier checked velocity read of the same drive and
// binding with no start, then reads them back.
void typedReply(uint32_t id, const std::vector<uint8_t>& bytes) {
    const auto token = view(id).typedRead->step;
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(findRecord(*app, id)->requestId); ++i) step();
    assert(app->owner.txAccepted(findRecord(*app, id)->requestId));
    scheduleReply(std::max(hardware.writeStarted + hardware.tx.size() * 87 + 1000, hardware.time + 1000), bytes);
    for (unsigned i = 0; i < 25000 && view(id).pending && view(id).typedRead->step == token; ++i) step();
}
void restoreReads(uint16_t firstFunction = 0, uint16_t motion = 1, uint16_t speed = 0) {
    writeResponseConfirmed = true;
    command("@90 read config\n"); const uint32_t config = app->latestOperationId;
    typedReply(config, registers(1, {0, 1000})); typedReply(config, registers(1, {0, 0, 0}));
    typedReply(config, registers(1, {0, 0, 0})); typedReply(config, registers(1, {0, firstFunction, 0, 0, 0}));
    typedReply(config, registers(1, {3, 4000})); pump();
    assert(app->configuration.operationId == config && releaseOperation(config));
    command("@91 read state\n"); const uint32_t state = app->latestOperationId;
    typedReply(state, registers(1, {0, motion})); typedReply(state, registers(1, {0, 0}));
    typedReply(state, registers(1, {0, 0, speed})); pump();
    assert(releaseOperation(state));
}
uint32_t velocitySnapshot(uint16_t speed, uint16_t acceleration, uint16_t deceleration) {
    const auto id = parameterRead(); parameterReply(id, registers(1, {speed, acceleration, deceleration}));
    assert(app->velocityParameters.operationId == id && releaseOperation(id)); return id;
}
Probe::ParameterWriteView restoreView(uint32_t id) { Probe::ParameterWriteView v; assert(copyParameterWrite(id, v)); return v; }
void serveRestore(uint32_t id, const std::vector<uint8_t>& response) {
    const uint8_t phase = app->parameterWrite.phase;
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(app->parameterWrite.request); ++i) step();
    assert(app->owner.txAccepted(app->parameterWrite.request));
    scheduleReply(std::max(hardware.writeStarted + hardware.tx.size() * 87 + 1000, hardware.time + 1000), response);
    for (unsigned i = 0; i < 25000 && restoreView(id).pending && app->parameterWrite.phase == phase; ++i) step();
}
const std::vector<uint8_t> VELOCITY_RESTORE_ACK = crc({1, 16, 0, 0x1D, 0, 3});
void velocityRestoreRoutes() {
    fresh(); restoreReads();
    const auto snapshot = velocitySnapshot(120, 100, 100);
    const auto later = velocitySnapshot(30, 200, 200); // As a velocity run leaves them.
    unsigned writes = hardware.writes; uint32_t id = 77;
    assert(!restoreVelocityParameters(later + 100, id) && id == 77);
    assert(!restoreHomingParameters(snapshot, id) && id == 77); // Wrong kind of source.
    assert(hardware.writes == writes);
    assert(restoreVelocityParameters(snapshot, id) && axisReserved(*app, 1));
    serveRestore(id, VELOCITY_RESTORE_ACK);
    assert(hardware.writes == writes + 1 && hardware.tx == crc({1, 16, 0, 0x1D, 0, 3, 6, 0, 120, 0, 100, 0, 100}));
    assert(!app->velocityParameters.operationId && !app->velocityPrerequisites.target.id);
    serveRestore(id, registers(1, {120, 100, 100}));
    assert(hardware.tx == crc({1, 3, 0, 0x1D, 0, 3}));
    auto v = restoreView(id);
    assert(!v.pending && v.ok && v.kind == Probe::ParameterWriteKind::VELOCITY_RESTORE && v.count == 3);
    assert(v.readback[0] == 120 && !axisReserved(*app, 1) && hardware.writes == writes + 2);
    // Readback mismatch.
    restoreReads(); const auto again = velocitySnapshot(120, 100, 100);
    assert(restoreVelocityParameters(again, id));
    serveRestore(id, VELOCITY_RESTORE_ACK); serveRestore(id, registers(1, {120, 100, 99}));
    v = restoreView(id); assert(!v.ok && !std::strcmp(v.error, "readback_mismatch") && !v.unsettled);
    // Lost acknowledgement: unknown, not resent, a velocity read settles it.
    restoreReads(); const auto lost = velocitySnapshot(120, 100, 100);
    writes = hardware.writes;
    assert(restoreVelocityParameters(lost, id));
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(app->parameterWrite.request); ++i) step();
    for (unsigned i = 0; i < 200000 && restoreView(id).pending; ++i) step();
    v = restoreView(id);
    assert(v.unsettled && v.execution == ActionExecution::UNKNOWN && hardware.writes == writes + 1 && !axisReserved(*app, 1));
    uint32_t refused = 0; assert(!restoreVelocityParameters(lost, refused) && !refused);
    pump(5000); assert(hardware.writes == writes + 1);
    uint32_t recovery = 0; assert(recover(app, 0, recovery) == Probe::Action::OK);
    for (unsigned i = 0; i < 25000 && view(recovery).pending; ++i) step();
    restoreReads(); const auto settled = velocitySnapshot(120, 100, 100);
    assert(!app->parameterWrite.view.unsettled && !axisReserved(*app, 1));
    assert(restoreVelocityParameters(settled, id));
    serveRestore(id, VELOCITY_RESTORE_ACK); serveRestore(id, registers(1, {120, 100, 100}));
    assert(restoreView(id).ok);
    // Moving, fast feedback, another drive: refused before TX.
    restoreReads(0, 1 | 4); const auto moving = velocitySnapshot(120, 100, 100);
    writes = hardware.writes; id = 77;
    assert(!restoreVelocityParameters(moving, id) && id == 77);
    restoreReads(0, 1, 7); assert(!restoreVelocityParameters(moving, id) && id == 77 && hardware.writes == writes + 8);
    command("@95 useaddr 2\n"); writes = hardware.writes;
    assert(!restoreVelocityParameters(moving, id) && id == 77 && hardware.writes == writes);
    // JOG (9) or speed (8) inputs refuse without a CONFIGURE record that inhibits them.
    for (const uint16_t function : {8, 9, 10}) {
        fresh(); restoreReads(function); const auto jog = velocitySnapshot(120, 100, 100);
        writes = hardware.writes; id = 77;
        const auto status = restoreVelocityParameters(jog, id);
        assert(!status && status.detail == static_cast<int32_t>(Probe::Action::QUALIFICATION_REQUIRED) && id == 77);
        assert(hardware.writes == writes);
    }
    fresh(); restoreReads(11); const auto homeInput = velocitySnapshot(120, 100, 100);
    assert(restoreVelocityParameters(homeInput, id)); // Function 11 does not use the JOG words.
}
// Actual replies from the restore route, for the host parser's parity check.
void emitRestoreReplies() {
    const auto emit = [](const std::string& line) {
        command(line + "\n"); pump(); std::fwrite(Serial.output.data(), 1, Serial.output.size(), stdout);
    };
    fresh(); restoreReads();
    emit("@870 velocity restore inspect");
    const auto snapshot = velocitySnapshot(120, 100, 100);
    emit("@871 velocity restore " + std::to_string(snapshot));
    const auto id = app->parameterWrite.view.operationId;
    serveRestore(id, VELOCITY_RESTORE_ACK); serveRestore(id, registers(1, {120, 100, 100}));
    emit("@872 velocity restore inspect");
    restoreReads(0, 1 | 4); const auto moving = velocitySnapshot(120, 100, 100);
    emit("@873 velocity restore " + std::to_string(moving));
}
} // namespace
// Wired bench 07 Implement 2: the shared stop/release rule in startVelocity.
void stopReleaseRule() {
    struct Case { uint16_t function, raw, polarity; const char* reason; int8_t terminal; };
    const Case refused[] = {{5, 8, 0, "stop_input_active", 3}, {6, 8, 0, "stop_input_active", 3},
                            {4, 8, 0, "release_input_active", 3}, {5, 0, 8, "input_mapping_unknown", 3}};
    for (const auto& c : refused) {
        fresh(); qualify(); inputs(c.function, c.raw, c.polarity);
        uint32_t untouched = 77; const auto next = app->nextOperationId;
        assert(startVelocity(app, 2, 1, request(), untouched) == Probe::Action::UNAVAILABLE);
        assert(untouched == 77 && app->nextOperationId == next && !hardware.writes && !axisReserved(*app, 1));
        Probe::SettingsRefusal refusal;
        assert(copySettingsRefusal(refusal) && !std::strcmp(refusal.reason, c.reason) &&
               !std::strcmp(refusal.purpose, "motion") && refusal.terminal == c.terminal);
        command("@5 velocity 30 rpm native 200 configured normal\n");
        assert(!hardware.writes);
        assert(Serial.output.find(std::string("\"reason\":\"") + c.reason + "\"") != std::string::npos);
        assert(Serial.output.find("\"purpose\":\"motion\"") != std::string::npos);
    }
    // Stale I/O: velocity needs a fresh checked read, like a move.
    fresh(); qualify();
    app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)].valid = false;
    uint32_t untouched = 77;
    assert(startVelocity(app, 2, 1, request(), untouched) == Probe::Action::UNAVAILABLE && !hardware.writes);
    Probe::SettingsRefusal stale;
    assert(copySettingsRefusal(stale) && !std::strcmp(stale.reason, "input_state_stale") && stale.terminal == -1);
    fresh(); qualify(); inputs(5, 0);
    app->options.observationMaxAgeMs = 1000; // The default 0 keeps observations without age expiry.
    advanceHardware(hardware.time + app->observationAgeUs() + 1000);
    auto& motion = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    motion.observedEarliestUs = motion.observedLatestUs = hardware.time;
    app->velocityPrerequisites.observedUs = hardware.time;
    assert(startVelocity(app, 2, 1, request(), untouched) == Probe::Action::UNAVAILABLE && !hardware.writes);
    // Inactive stop input: admitted; the observation reads 0x0006..0x0008.
    fresh(); qualify(); inputs(5, 0);
    const auto id = admitVelocity();
    assert(view(id).velocityContext->prerequisites.stopInputMask == 8);
    velocityStep(id); velocityStep(id);
    waitTx(id); assert(hardware.tx[1] == 3 && hardware.tx[3] == 6 && hardware.tx[5] == 3);
    // No refusal reason is left behind by an admitted run.
    Probe::SettingsRefusal none; assert(!copySettingsRefusal(none));
    assert(cancel(app, id) == Probe::Action::OK);
    // A stopping input never blocks a stop.
    fresh(); qualify(); inputs(5, 8);
    const auto stop = admitStop();
    assert(stop && hardware.writes == 0);
    pump(100); waitTx(stop); assert(hardware.tx[1] == 6 && hardware.tx[3] == 0x27);
}
// Implement 3 witnesses through the application: a run whose report shows the
// stop input and then a stopped drive before the timed stop fails STOP_INPUT;
// a stopped report without the input fails STOPPED_EARLY. The timed stop is
// still sent once, at its due time, and nothing is resent.
uint32_t admitLongVelocity() {
    auto r = request(); r.durationUs = 1000000;
    uint32_t id = 0;
    assert(host(app).startVelocity(app, 1, 1, r, id) == Probe::Action::OK);
    return id;
}
void timedStop(uint32_t id) {
    assert(!findRecord(*app, id)->requestId.owner);
    const auto due = view(id).velocityContext->stopDueUs;
    advanceHardware(due); serviceApplication();
    velocityStep(id); assert(hardware.tx[1] == 6 && hardware.tx[3] == 0x27 && hardware.writeStarted >= due);
    velocityStep(id, registers(1, {0, 1}));
}
void stopInputWitnesses() {
    for (bool inputSeen : {true, false}) {
        fresh(); qualify(); inputs(5, 0);
        const auto id = admitLongVelocity();
        velocityStep(id); velocityStep(id);
        velocityStep(id, registers(1, {0, 4, 0}));
        assert(view(id).velocityContext->runningObserved && !view(id).velocityContext->stopInputObserved);
        velocityStep(id, registers(1, {0, 4, static_cast<uint16_t>(inputSeen ? 8 : 0)}));
        velocityStep(id, registers(1, {0, 0, 0}));
        const auto& c = *view(id).velocityContext;
        assert(c.stopInputObserved == inputSeen && c.stoppedEarly && c.phase == ESS::VelocityPhase::OBSERVING);
        assert(c.state == ActionState::ACTIVE && c.needsStop);
        // The run keeps its schedule: the one timed stop at the due time.
        timedStop(id);
        const auto& done = *view(id).velocityContext;
        assert(!view(id).pending && done.state == ActionState::FAILED && !done.needsStop);
        assert(done.status.detail == static_cast<int32_t>(inputSeen ? VelocityError::STOP_INPUT : VelocityError::STOPPED_EARLY));
        assert(done.completion == ActionCompletion::OBSERVED); // The timed stop's own settlement, not a completed run.
        Serial.output.clear(); pump(200);
    }
    // Negative trace: the input is reported active, but the drive keeps running
    // until the route's own timed stop. The run fails STOP_INPUT and stoppedEarly
    // stays false, so no input stop is claimed (the harness classifies it indeterminate).
    fresh(); qualify(); inputs(5, 0);
    const auto id = admitLongVelocity();
    velocityStep(id); velocityStep(id);
    velocityStep(id, registers(1, {0, 4, 8}));
    velocityStep(id, registers(1, {0, 4, 0}));
    timedStop(id);
    const auto& done = *view(id).velocityContext;
    assert(done.state == ActionState::FAILED && done.stopInputObserved && !done.stoppedEarly);
    assert(done.status.detail == static_cast<int32_t>(VelocityError::STOP_INPUT));
    // Without a stop/release assignment the observation stays two words and a
    // healthy run still succeeds.
    fresh(); qualify();
    const auto healthy = admitLongVelocity();
    assert(!view(healthy).velocityContext->prerequisites.stopInputMask);
    velocityStep(healthy); velocityStep(healthy);
    waitTx(healthy); assert(hardware.tx[5] == 2);
    velocityStep(healthy, registers(1, {0, 4}));
    timedStop(healthy);
    assert(view(healthy).velocityContext->state == ActionState::SUCCEEDED);
}
// Console terminals of the wired bench 07 outcomes, for the Python validators.
void emitStopInputResults() {
    struct Trace { const char* name; uint16_t function; std::vector<std::vector<uint16_t>> polls; };
    const Trace traces[] = {
        {"stop_input", 5, {{0, 4, 0}, {0, 4, 8}, {0, 0, 0}}},
        {"stopped_early", 5, {{0, 4, 0}, {0, 0, 0}}},
        {"input_then_timed_stop", 6, {{0, 4, 8}, {0, 4, 0}}},
        {"success_with_stop_input", 4, {{0, 4, 0}}},
    };
    for (const auto& t : traces) {
        fresh(); qualify(); inputs(t.function, 0);
        command("@6 velocity 30 rpm native 1000 configured normal\n");
        const auto id = app->latestOperationId;
        velocityStep(id); velocityStep(id);
        for (const auto& words : t.polls) {
            std::vector<uint8_t> reply = {1, 3, static_cast<uint8_t>(words.size() * 2)};
            for (const auto w : words) { reply.push_back(static_cast<uint8_t>(w >> 8)); reply.push_back(static_cast<uint8_t>(w)); }
            velocityStep(id, crc(reply));
        }
        timedStop(id);
        pump(200);
        assert(!view(id).pending);
        const auto start = Serial.output.find("{\"type\":\"velocity\"");
        assert(start != std::string::npos);
        const auto end = Serial.output.find('\n', start);
        std::printf("{\"case\":\"%s\",\"record\":%s}\n", t.name, Serial.output.substr(start, end - start).c_str());
    }
}
// Wired bench 11: an active limit input refuses velocity in both directions;
// an unknown soft-limit state refuses; a bound state admits (the drive ends the
// run at its soft limit). X1 = 2 and X2 = 3 come from qualify().
void limitVelocityRule() {
    for (const uint16_t raw : {uint16_t(2), uint16_t(4)}) {
        fresh(); qualify(); inputs(0, raw);
        uint32_t untouched = 77; const auto next = app->nextOperationId;
        assert(startVelocity(app, 2, 1, request(), untouched) == Probe::Action::UNAVAILABLE);
        assert(untouched == 77 && app->nextOperationId == next && !hardware.writes);
        Probe::SettingsRefusal refusal;
        assert(copySettingsRefusal(refusal) && !std::strcmp(refusal.reason, "limit_input_active") &&
               refusal.terminal == (raw == 2 ? 1 : 2));
    }
    fresh(); qualify(); inputs(0, 0);
    app->configuration.raw.softLimitEnable = 1; app->configuration.softLimitEnableKnown = true;
    uint32_t untouched = 77;
    assert(startVelocity(app, 2, 1, request(), untouched) == Probe::Action::UNAVAILABLE && !hardware.writes);
    Probe::SettingsRefusal unknown;
    assert(copySettingsRefusal(unknown) && !std::strcmp(unknown.reason, "soft_limit_state_unknown"));
    command("@6 velocity 30 rpm native 200 configured normal\n");
    assert(!hardware.writes && Serial.output.find("\"reason\":\"soft_limit_state_unknown\"") != std::string::npos);
    // Bound: admitted; connected limit terminals add the 0x0008 witness.
    fresh(); qualify(); inputs(0, 0);
    app->configuration.wiring[1] = app->configuration.wiring[2] = InputWiring::CONNECTED;
    app->configuration.raw.softLimitEnable = 1; app->configuration.softLimitEnableKnown = true;
    auto& b = app->softLimitBinding; b = App::SoftLimitBinding();
    b.target = app->axis.target; b.operationId = 1; b.axisGeneration = app->axis.generation;
    b.bindingGeneration = app->bindingGeneration; b.minimum = -500; b.maximum = 500;
    app->axis.softLimitsKnown = true; app->axis.softMinimum = -500; app->axis.softMaximum = 500;
    if (app->configuration.operationId <= b.operationId) app->configuration.operationId = b.operationId + 1;
    auto& motion = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    motion.value.homingComplete = true; motion.observedEarliestUs = motion.observedLatestUs = hardware.time;
    assert(softLimitState(*app, nowUs()) == SoftLimitState::BOUND);
    const auto id = admitVelocity();
    assert(view(id).velocityContext->prerequisites.limitInputMask == 6);
    assert(cancel(app, id) == Probe::Action::OK);
}
// ---------------------------------------------------------------- wired bench 14
Probe::VelocityCell cellOf(int16_t rpm, uint16_t ramp = 100, uint16_t subdivision = 1000, uint32_t ms = 2000,
                           Probe::VelocityStop stop = Probe::VelocityStop::NORMAL, bool alone = false) {
    Probe::VelocityCell c; c.rpm = rpm; c.accelerationTime = c.decelerationTime = ramp; c.subdivision = subdivision;
    c.durationMs = ms; c.stop = stop; c.standalone = alone; return c;
}
void velocityLimitRules() {
    using namespace Probe;
    using A = VelocityAdmission;
    assert(velocityRate(60, 1000) == 1000 && velocityRate(-3000, 1000) == 50000 && velocityRate(600, 6000) == 60000);
    assert(velocityRate(-500, 6000) == 50000 && velocityRate(1, 400) == 7);
    // Polls: 64 cover the duration; the base interval stays for short runs (legacy values).
    assert(velocityPollMs(1000, VelocityStop::NORMAL) == 50 && velocityPollMs(1000, VelocityStop::FAST) == 20);
    assert(velocityPollMs(2000, VelocityStop::FAST) == 32 && velocityPollMs(10000, VelocityStop::NORMAL) == 157);
    assert(velocityPollMs(VELOCITY_MAX_DURATION_MS, VelocityStop::FAST) * VELOCITY_POLLS >= VELOCITY_MAX_DURATION_MS);
    assert(velocityStopBudgetMs(VelocityStop::NORMAL) == 3000 && velocityStopBudgetMs(VelocityStop::FAST) == 2000);
    const VelocityCell table[] = {cellOf(120), cellOf(300), cellOf(600), cellOf(1200), cellOf(-120),
        cellOf(500, 500, 6000, 2000, VelocityStop::NORMAL, true), cellOf(600, 500, 6000, 2000, VelocityStop::NORMAL, true)};
    const auto admit = [&](const VelocityCell& run) { return velocityPromotedAdmission(table, 7, run); };
    assert(admit(cellOf(300)) == A::PROMOTED && admit(cellOf(300, 100, 1000, 1500)) == A::PROMOTED);
    assert(admit(cellOf(400)) == A::INTERPOLATED && admit(cellOf(900)) == A::INTERPOLATED);
    assert(admit(cellOf(200)) == A::REFUSED);  // 120 and 300 are more than a factor of two apart.
    assert(admit(cellOf(1300)) == A::REFUSED && admit(cellOf(100)) == A::REFUSED); // Above the highest, below the lowest.
    assert(admit(cellOf(-120)) == A::PROMOTED && admit(cellOf(-300)) == A::REFUSED); // Per direction.
    assert(admit(cellOf(400, 500)) == A::REFUSED && admit(cellOf(400, 100, 2000)) == A::REFUSED); // Ramp, subdivision.
    assert(admit(cellOf(400, 100, 1000, 2001)) == A::REFUSED); // Longer than the tested runs.
    assert(admit(cellOf(400, 100, 1000, 2000, VelocityStop::FAST)) == A::REFUSED);
    assert(admit(cellOf(600, 500, 6000)) == A::PROMOTED && admit(cellOf(550, 500, 6000)) == A::REFUSED); // HW-14 cells stand alone.
    // Audit F4: a standalone entry admits only its exact duration (its stop point is part of the cell).
    assert(admit(cellOf(600, 500, 6000, 1500)) == A::REFUSED && admit(cellOf(600, 500, 6000, 2001)) == A::REFUSED);
    assert(velocityPromotedAdmission(nullptr, 0, cellOf(300)) == A::REFUSED);
    assert(velocityLegacyRun(cellOf(60, 100, 1000, 1000)) && velocityLegacyRun(cellOf(-60, 2000, 51200, 1)));
    assert(!velocityLegacyRun(cellOf(61, 100, 1000, 1000)) && !velocityLegacyRun(cellOf(60, 100, 1000, 1001)));
    assert(!velocityCellValid(cellOf(0)) && !velocityCellValid(cellOf(3001)) && !velocityCellValid(cellOf(120, 2001)));
    assert(!velocityCellValid(cellOf(120, 100, 399)) && !velocityCellValid(cellOf(120, 100, 1000, 12001)));
}
VelocityRequest runRequest(int32_t rpm, uint32_t ms, bool normal = true) {
    auto r = request(); r.value = Rational(rpm); r.durationUs = ms * 1000;
    r.stop.behavior = normal ? StopBehavior::CONFIGURED_DECELERATION : StopBehavior::DIRECT; return r;
}
Probe::Action tryRun(int32_t rpm, uint32_t ms, uint32_t& id, bool normal = true) {
    id = 77; const auto result = host(app).startVelocity(app, 9, 1, runRequest(rpm, ms, normal), id);
    if (result != Probe::Action::OK) assert(id == 77);
    return result;
}
Probe::VelocityLimitsView limitsCall(Probe::VelocityLimitsCommandKind kind, const Probe::VelocityCell* cell = nullptr,
                                     Probe::Action expected = Probe::Action::OK) {
    Probe::VelocityLimitsCommand c; c.kind = kind; if (cell) c.cell = *cell;
    Probe::VelocityLimitsView v; assert(velocityLimitsCommand(app, &c, v) == expected); return v;
}
void dropRun(uint32_t id) {
    assert(cancel(app, id) == Probe::Action::OK); pump(1000);
    assert(!view(id).pending && !axisReserved(*app, 1));
    clearRecord(*findRecord(*app, id)); // Never delivered to a console; free the slot.
    qualify(); // A possible run expires the readiness observations; the fixture states them again.
}
bool refusedFor(const char* reason) {
    Probe::SettingsRefusal r; return copySettingsRefusal(r) && !std::strcmp(r.reason, reason);
}
void velocityLimitsRoutes() {
    using K = Probe::VelocityLimitsCommandKind;
    using P = Probe::Action;
    fresh(); qualify();
    uint32_t id = 0; const unsigned writes = hardware.writes;
    // Legacy refusals without any call (D12): 61 rpm, 1001 ms; zero frames, named reason.
    assert(tryRun(61, 1000, id) == P::INVALID && refusedFor("outside_velocity_limits"));
    assert(tryRun(30, 1001, id) == P::INVALID && tryRun(-61, 200, id) == P::INVALID && hardware.writes == writes);
    auto v = limitsCall(K::SHOW);
    assert(v.source == Probe::VelocityLimitsSource::LEGACY && v.maximumRpm == 60 && v.maximumDurationMs == 1000);
    assert(v.subdivision == 1000 && v.maximumRate == 1000 && v.pollMs == 50 && v.stopBudgetMs == 3000 && !v.promotedCount);
    // A legacy run keeps its poll interval and stop budget.
    assert(tryRun(60, 1000, id) == P::OK);
    assert(view(id).velocityContext->options.pollIntervalUs == 50000);
    assert(view(id).velocityContext->deadlineUs - view(id).velocityContext->startedUs == 4000000);
    dropRun(id);
    assert(tryRun(-60, 1000, id, false) == P::OK && view(id).velocityContext->options.pollIntervalUs == 20000);
    assert(view(id).velocityContext->deadlineUs - view(id).velocityContext->startedUs == 3000000);
    dropRun(id);
    // Qualification cell: checked against the selected axis, one at a time.
    const auto cell = cellOf(120);
    auto bad = cell; bad.subdivision = 6000;
    v = limitsCall(K::QUALIFY, &bad, P::UNAVAILABLE); assert(!std::strcmp(v.reason, "subdivision_mismatch") && !v.cellArmed);
    bad = cellOf(30, 100, 1000, 500); v = limitsCall(K::QUALIFY, &bad, P::INVALID); assert(!std::strcmp(v.reason, "inside_legacy_limits"));
    bad = cellOf(0); v = limitsCall(K::QUALIFY, &bad, P::INVALID); assert(!std::strcmp(v.reason, "invalid_cell"));
    v = limitsCall(K::QUALIFY, &cell);
    assert(v.cellArmed && v.source == Probe::VelocityLimitsSource::CELL && v.maximumRpm == 120 && v.maximumDurationMs == 2000);
    assert(v.maximumRate == 2000 && v.cell.rpm == 120);
    assert(!std::strcmp(limitsCall(K::QUALIFY, &cell, P::BUSY).reason, "cell_armed"));
    // Every other combination refuses before any frame while the cell is armed.
    assert(tryRun(121, 2000, id) == P::INVALID && tryRun(120, 2001, id) == P::INVALID && tryRun(-120, 2000, id) == P::INVALID);
    assert(tryRun(120, 2000, id, false) == P::INVALID && tryRun(120, 1999, id) == P::INVALID && hardware.writes == writes);
    app->velocityPrerequisites.accelerationTime = 200;
    assert(tryRun(120, 2000, id) == P::INVALID); // Other ramp words.
    app->velocityPrerequisites.accelerationTime = 100;
    // The matching run is admitted once: 64 polls cover 2 s, the normal stop budget follows; it consumes the cell.
    assert(tryRun(120, 2000, id) == P::OK);
    assert(view(id).velocityContext->options.pollIntervalUs == 50000);
    assert(view(id).velocityContext->deadlineUs - view(id).velocityContext->startedUs == 5000000);
    v = limitsCall(K::SHOW); assert(!v.cellArmed && v.source == Probe::VelocityLimitsSource::LEGACY && v.consumedOperationId == id);
    dropRun(id);
    uint32_t again = 0;
    assert(tryRun(120, 2000, again) == P::INVALID && tryRun(121, 2000, again) == P::INVALID && hardware.writes == writes);
    // Reset paths: reset, useaddr, recover.
    limitsCall(K::QUALIFY, &cell); limitsCall(K::RESET); assert(!limitsCall(K::SHOW).cellArmed);
    assert(tryRun(120, 2000, again) == P::INVALID);
    limitsCall(K::QUALIFY, &cell); command("@80 useaddr 2\n"); command("@81 useaddr 1\n"); qualify();
    assert(!limitsCall(K::SHOW).cellArmed && tryRun(120, 2000, again) == P::INVALID && refusedFor("outside_velocity_limits"));
    limitsCall(K::QUALIFY, &cell);
    uint32_t recovery = 0; assert(recover(app, 0, recovery) == P::OK);
    for (unsigned i = 0; i < 250000 && view(recovery).pending; ++i) step();
    qualify(); assert(!limitsCall(K::SHOW).cellArmed && tryRun(120, 2000, again) == P::INVALID);
    // Raise: only with a promoted table; then exact and interpolated runs, nothing else.
    v = limitsCall(K::RAISE, nullptr, P::UNAVAILABLE); assert(!std::strcmp(v.reason, "no_promoted_cells") && !v.raised);
    static const Probe::VelocityCell promoted[] = {cellOf(120), cellOf(300), cellOf(600)};
    app->velocityPromoted = promoted; app->velocityPromotedCount = 3;
    // Audit F4: a cell and a raise are exclusive, also while the cell's run is active.
    limitsCall(K::QUALIFY, &cell);
    v = limitsCall(K::RAISE, nullptr, P::BUSY); assert(!std::strcmp(v.reason, "cell_armed") && !v.raised);
    assert(tryRun(120, 2000, id) == P::OK);
    v = limitsCall(K::RAISE, nullptr, P::BUSY); assert(!std::strcmp(v.reason, "cell_run_active") && !v.raised);
    dropRun(id);
    v = limitsCall(K::RAISE);
    assert(!std::strcmp(limitsCall(K::QUALIFY, &cell, P::BUSY).reason, "raise_active") && !limitsCall(K::SHOW).cellArmed);
    assert(v.raised && v.source == Probe::VelocityLimitsSource::RAISED && v.maximumRpm == 600 && v.promotedCount == 3);
    const unsigned raisedWrites = hardware.writes;
    assert(tryRun(300, 2000, id) == P::OK); dropRun(id);
    assert(tryRun(400, 1500, id) == P::OK); dropRun(id); // Interpolated between 300 and 600.
    assert(tryRun(200, 2000, id) == P::INVALID && tryRun(700, 2000, id) == P::INVALID && tryRun(-300, 2000, id) == P::INVALID);
    assert(tryRun(300, 2000, id) == P::OK); dropRun(id); // A raise is not consumed by a run.
    assert(hardware.writes == raisedWrites);
    limitsCall(K::RESET); assert(tryRun(300, 2000, id) == P::INVALID);
    // The console route: show, qualify, reset; malformed forms refuse before the application.
    command("@82 velocity limits\n");
    assert(Serial.output.find("\"command\":\"velocity-limits\"") != std::string::npos &&
           Serial.output.find("\"source\":\"legacy\"") != std::string::npos && Serial.output.find("\"bus_traffic\":false") != std::string::npos);
    command("@83 velocity limits qualify -120 100 100 1000 2000 fast\n");
    assert(Serial.output.find("\"cell_armed\":true") != std::string::npos && Serial.output.find("\"rpm\":-120") != std::string::npos);
    command("@84 velocity limits qualify 120 100 100 1000 2000\n");
    assert(Serial.output.find("invalid_arguments") != std::string::npos);
    command("@85 velocity limits reset\n"); assert(Serial.output.find("\"cell_armed\":false") != std::string::npos);
    command("@86 velocity 61 rpm native 1000 configured normal\n");
    assert(Serial.output.find("\"reason\":\"outside_velocity_limits\"") != std::string::npos && hardware.writes == raisedWrites);
    command("@87 caps\n");
    assert(Serial.output.find("\"velocity_limits\":{\"route\":true,\"source\":\"legacy\",\"max_rpm\":60") != std::string::npos);
    assert(Serial.output.find("\"external_jog\"") == std::string::npos);
}
// Wired bench 14: a velocity start with a speed, JOG or PV input on a connected terminal.
void velocityInputFunctions() {
    for (const uint16_t function : {8, 9, 10, 13}) {
        fresh(); qualify();
        app->configuration.raw.inputFunctions[0] = function;
        app->configuration.wiring[0] = InputWiring::CONNECTED;
        uint32_t id = 0;
        assert(tryRun(30, 200, id) == Probe::Action::QUALIFICATION_REQUIRED && refusedFor("triggers_inhibited_required"));
        auto& q = app->ioQualifications[0]; q = IoQualification();
        q.target = app->axis.target; q.configurationGeneration = app->axis.generation; q.terminal = 0;
        q.purpose = IoPurpose::FINITE_MOVE; q.wiring = InputWiring::CONNECTED; q.function = q.previousFunction = function;
        q.qualifiedUs = hardware.time; q.maximumAgeUs = 60000000; q.input.supplied = true; q.input.externalTriggersInhibited = true;
        assert(ioFactsValid(q, nowUs()));
        auto& io = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
        io.value.rawInputs = 1; io.observedEarliestUs = io.observedLatestUs = hardware.time;
        assert(tryRun(30, 200, id) == Probe::Action::UNAVAILABLE && refusedFor("input_active") && !hardware.writes);
        io.value.rawInputs = 0;
        assert(tryRun(30, 200, id) == Probe::Action::OK); dropRun(id);
        // An unconnected terminal has no source: admitted without the record.
        fresh(); qualify(); app->configuration.raw.inputFunctions[0] = function;
        app->configuration.wiring[0] = InputWiring::UNCONNECTED;
        assert(tryRun(30, 200, id) == Probe::Action::OK); dropRun(id);
    }
}
// Wired bench 14: the JOG setter writes 0x001D/3 with no start, inside the input limits.
void jogSetter() {
    fresh(); restoreReads();
    uint32_t id = 77; unsigned writes = hardware.writes;
    auto status = setJogParameters(60, 100, 100, id);
    assert(!status && status.detail == static_cast<int32_t>(Probe::Action::UNAVAILABLE) && id == 77 && hardware.writes == writes);
    Probe::SettingsRefusal r; assert(copySettingsRefusal(r) && !std::strcmp(r.reason, "velocity_read_required"));
    velocitySnapshot(120, 100, 100); writes = hardware.writes;
    status = setJogParameters(61, 100, 100, id);
    assert(!status && status.detail == static_cast<int32_t>(Probe::Action::INVALID) && refusedFor("outside_velocity_limits"));
    assert(!setJogParameters(-61, 100, 100, id) && !setJogParameters(0, 100, 100, id) && !setJogParameters(60, 2001, 100, id));
    assert(hardware.writes == writes && id == 77);
    assert(setJogParameters(-60, 300, 500, id) && axisReserved(*app, 1));
    serveRestore(id, VELOCITY_RESTORE_ACK);
    assert(hardware.writes == writes + 1 && hardware.tx == crc({1, 16, 0, 0x1D, 0, 3, 6, 0xFF, 0xC4, 1, 0x2C, 1, 0xF4}));
    serveRestore(id, registers(1, {0xFFC4, 300, 500}));
    auto v = restoreView(id);
    assert(!v.pending && v.ok && v.kind == Probe::ParameterWriteKind::VELOCITY_SET && v.readback[0] == 0xFFC4 && !axisReserved(*app, 1));
    assert(hardware.writes == writes + 2 && !app->velocityParameters.operationId); // The write expired the read.
    // Audit F1: raised limits are serial-only; the setter keeps the legacy speed while raised.
    restoreReads(); velocitySnapshot(120, 100, 100);
    static const Probe::VelocityCell promoted[] = {cellOf(120)};
    app->velocityPromoted = promoted; app->velocityPromotedCount = 1;
    assert(limitsCall(Probe::VelocityLimitsCommandKind::RAISE).raised);
    writes = hardware.writes; id = 77;
    status = setJogParameters(120, 100, 100, id);
    assert(!status && status.detail == static_cast<int32_t>(Probe::Action::INVALID) && refusedFor("outside_velocity_limits"));
    assert(id == 77 && hardware.writes == writes);
    // An active JOG input refuses; one without a CONFIGURE record that inhibits it too.
    fresh(); restoreReads(9); velocitySnapshot(120, 100, 100);
    status = setJogParameters(30, 100, 100, id);
    assert(!status && status.detail == static_cast<int32_t>(Probe::Action::QUALIFICATION_REQUIRED));
    // The console route and its reply.
    fresh(); restoreReads(); velocitySnapshot(120, 100, 100);
    command("@89 velocity jog set 120 100 100\n");
    assert(Serial.output.find("\"reason\":\"outside_velocity_limits\"") != std::string::npos);
    command("@90 velocity jog set 30 100\n"); assert(Serial.output.find("invalid_arguments") != std::string::npos);
    command("@88 velocity jog set 30 100 100\n");
    assert(Serial.output.find("\"command\":\"velocity-jog\"") != std::string::npos &&
           Serial.output.find("\"kind\":\"velocity_jog_set\"") != std::string::npos && Serial.output.find("\"words\":[30,100,100]") != std::string::npos);
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--emit-stop-input-results") { emitStopInputResults(); return 0; }
    if (argc == 2 && std::string(argv[1]) == "--emit-recovery-result") {
        ownedStopBeforeTxRetainsSettlement(true); return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--emit-restore-replies") { emitRestoreReplies(); return 0; }
    parameterReadApplicationPath();
    parameterReadQualificationAndHistoricalResults();
    parameterReadConsoleAndCapacity();
    parameterReadHumanTerminal();
    gatesAndParity();
    externalStopAndLocalCancel();
    pressureAndPhysicalCancellation();
    delayedEvidenceAndRecovery();
    expiredPollRetainsTransportEvidence();
    urgentPressureDefersWithinOriginalDeadline();
    normalStopAllowsFullConfiguredRamp();
    healthyDurationSweepSendsOneStop();
    ownedStopSurvivesLostStartOrPoll();
    lostOwnedStopNeverReplays();
    ownedStopBeforeTxRetainsSettlement();
    ownedStopRecoveryFailureAndCancellation();
    velocityRestoreRoutes();
    stopReleaseRule();
    limitVelocityRule();
    stopInputWitnesses();
    velocityLimitRules();
    velocityLimitsRoutes();
    velocityInputFunctions();
    jogSetter();
}
