// SPDX-License-Identifier: MIT
// Actual cooperative application with real owner/runner/UART fakes: every stop
// claim below is asserted against the bytes on the fake wire.
#include "../examples/probe_cli/ProbeApp.cpp"
#include "fakes/esp32_uart/UsbConsoleFixture.h"
#if MOTORCONTROLRS_TEST_IDF
#include "fakes/esp32_uart/FakeUsb.h"
#include "../examples/probe_idf/main/IdfPlatform.cpp"
#include "../examples/common/BoardPins.h"
#else
#include "../examples/probe_cli/ArduinoPlatform.cpp"
#include "../examples/probe_cli/main.cpp"
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
namespace {
using namespace MotorControlRS;
std::vector<std::vector<uint8_t>> wire;
unsigned seenWrites = 0;
void step(uint32_t us = 10) {
    advanceHardware(hardware.time + us); serviceApplication();
    if (hardware.writes != seenWrites) { seenWrites = hardware.writes; wire.push_back(hardware.tx); }
}
void pump(unsigned n = 1000) { while (n--) step(); }
void fresh(const ApplicationOptions& options = ApplicationOptions()) {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = writeResponseConfirmed = false;
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit, options));
    hardware.txCharacterUs = 87; assert(uart.startCapture(20, timing().holdUs));
    wire.clear(); seenWrites = 0; assert(app && !hardware.writes);
}
Probe::ResultView view(uint32_t id) { Probe::ResultView v; assert(host(app).result(app, id, v)); return v; }
std::vector<uint8_t> crc(std::vector<uint8_t> v) {
    const uint16_t c = ESS::calcCrc16(v.data(), v.size());
    v.push_back(static_cast<uint8_t>(c)); v.push_back(static_cast<uint8_t>(c >> 8)); return v;
}
std::vector<uint8_t> words(std::initializer_list<uint16_t> values, uint8_t address = 1) {
    std::vector<uint8_t> result = {address, 3, static_cast<uint8_t>(values.size() * 2)};
    for (const auto value : values) { result.push_back(static_cast<uint8_t>(value >> 8)); result.push_back(static_cast<uint8_t>(value)); }
    return crc(result);
}
const std::vector<uint8_t> FAST_STOP = crc({1, 6, 0, 0x27, 2, 0});
const std::vector<uint8_t> NORMAL_STOP = crc({1, 6, 0, 0x27, 1, 0});
unsigned frames(const std::vector<uint8_t>& expected) {
    unsigned count = 0;
    for (const auto& frame : wire) if (frame == expected) ++count;
    return count;
}
void command(const std::string& text) {
    Serial.input = text; Serial.output.clear();
    for (unsigned i = 0; i < 500 && !Serial.input.empty(); ++i) step();
    assert(Serial.input.empty()); pump(64);
}
bool said(const std::string& text) { return Serial.output.find(text) != std::string::npos; }
std::string stopRecoveryJson() {
    const auto first = Serial.output.find("\"stop_recovery\":");
    const auto last = Serial.output.find(",\"host_serial\":", first);
    assert(first != std::string::npos && last != std::string::npos);
    return Serial.output.substr(first, last - first);
}
void waitTx(uint32_t id) {
    for (unsigned i = 0; i < 300000 && !app->owner.txAccepted(findRecord(*app, id)->requestId); ++i) step();
    assert(app->owner.txAccepted(findRecord(*app, id)->requestId));
}
void deliver(const std::vector<uint8_t>& bytes) {
    scheduleReply(std::max(hardware.writeStarted + hardware.tx.size() * 87 + 1000, hardware.time + 1000), bytes);
}
// Answer the current action/home frame and wait until its sequence token advances.
uint8_t token(uint32_t id) { const auto v = view(id); return v.homeContext ? v.homeContext->step : v.actionContext->step; }
void reply(uint32_t id, const std::vector<uint8_t>& given = {}) {
    const auto before = token(id);
    waitTx(id);
    deliver(given.empty() ? crc(std::vector<uint8_t>(hardware.tx.begin(), hardware.tx.begin() + 6)) : given);
    for (unsigned i = 0; i < 25000 && view(id).pending && token(id) == before; ++i) step();
    assert(!view(id).pending || token(id) != before);
}
void readReply(uint32_t id, const std::vector<uint8_t>& bytes) {
    const auto before = view(id).typedRead->step;
    waitTx(id); deliver(bytes);
    for (unsigned i = 0; i < 25000 && view(id).pending && view(id).typedRead->step == before; ++i) step();
    assert(!view(id).pending || view(id).typedRead->step != before);
}
void until(uint32_t id, uint64_t limitUs = 6000000) {
    const auto start = hardware.time;
    while (view(id).pending && hardware.time - start < limitUs) step(100);
    assert(!view(id).pending);
}
void untilWire(const std::vector<uint8_t>& frame, uint64_t limitUs = 4000000) {
    const auto start = hardware.time;
    while (!frames(frame) && hardware.time - start < limitUs) step(100);
    assert(frames(frame));
}
uint32_t latest() { return app->latestOperationId; }
uint32_t accepted(const std::string& line) {
    command(line + "\n");
    if (!said("\"result\":\"accepted\"")) std::fprintf(stderr, "Not accepted [%s]: %s\n", line.c_str(), Serial.output.c_str());
    assert(said("\"result\":\"accepted\"")); return latest();
}
void noMoreFrames(unsigned loops = 40000) { const auto before = wire.size(); for (unsigned i = 0; i < loops; ++i) step(100); assert(wire.size() == before); }
void known() {
    writeResponseConfirmed = true;
    app->knownTargets[0] |= 2; app->communicationKnown = true;
    app->communicationTarget.id = app->communicationTarget.address = 1;
    app->communicationTarget.generation = app->bindingGeneration;
}
// The home fixture from the audit reproducer: method 3 with an origin input.
void homeQualified() {
    command("@89 wiring x0 connected\n");
    writeResponseConfirmed = true;
    command("@90 read config\n"); const uint32_t config = latest();
    readReply(config, words({0, 1000})); readReply(config, words({0, 0, 0}));
    readReply(config, words({0, 0, 0})); readReply(config, words({0, 1, 0, 0, 0}));
    readReply(config, words({3, 4000})); pump(64);
    assert(app->configuration.operationId == config && releaseOperation(config));
    command("@91 read state\n"); const uint32_t state = latest();
    readReply(state, words({0, 1})); readReply(state, words({0, 0})); readReply(state, words({0, 0, 0})); pump(64);
    assert(releaseOperation(state));
    uint32_t parameters = 0; assert(submitHomingParameterRead(parameters));
    readReply(parameters, words({7, 3, 60, 30, 100, 0, 0})); pump(64);
    assert(app->homingParameters.operationId == parameters && releaseOperation(parameters));
    ESS::HomePrerequisites p; p.target = app->axis.target; p.configurationGeneration = app->axis.generation;
    p.qualifiedMethod = static_cast<ESS::HomingMethod>(3);
    p.qualifiedSearchSpeed = 60; p.qualifiedReturnSpeed = 30; p.qualifiedRampTime = 100;
    p.methodQualified = p.nativeRatesQualified = p.nativeRampQualified = p.zeroOffsetQualified = p.auxiliaryQualified = true;
    p.inputsQualified = p.indexQualified = p.readinessQualified = p.referenceSemanticsQualified = true;
    p.qualifiedUs = hardware.time; p.qualificationMaximumAgeUs = 1000000; p.maximumAgeUs = 1000000;
    p.inputPolarity = app->configuration.raw.inputPolarity;
    for (unsigned i = 0; i < 4; ++i) { p.inputFunctions[i] = app->configuration.raw.inputFunctions[i]; p.inputWiring[i] = app->inputWiring[i]; }
    p.availableInputs = 1;
    assert(installHomeQualification(p));
}
uint32_t runningHome() {
    fresh(); homeQualified();
    ESS::HomeRequest r; r.method = static_cast<ESS::HomingMethod>(3); r.configurationGeneration = app->axis.generation;
    uint32_t id = 0; assert(submitHome(r, id));
    reply(id); reply(id); reply(id, words({0, 4, 0}));
    assert(view(id).pending);
    wire.clear();
    return id;
}
void assertRecovered(const char* context) {
    (void)context;
    assert(!app->owner.needsRecovery() && !uart.needsRecovery());
}
ESS::ActionContext actionOf(uint32_t id) { return *view(id).actionContext; }
uint32_t apiStop(uint8_t address = 1, StopBehavior behavior = StopBehavior::DIRECT) {
    StopPolicy policy; policy.behavior = behavior;
    uint32_t id = 0; assert(submitStop(address, policy, id) && id);
    return id;
}
ActionResult copiedAction(uint32_t id) {
    ActionResult result; assert(copyActionResult(id, result)); return result;
}

// T2: stop fast escalates a stop normal that is still observing.
void escalatesRunningNormalStop() {
    const auto home = runningHome();
    const auto normal = accepted("@70 stop normal");
    reply(normal); reply(normal, words({0, 4}));
    const auto observing = wire.size();
    const auto fast = accepted("@71 stop fast");
    assert(fast != normal && findRecord(*app, normal) != findRecord(*app, fast));
    reply(fast); reply(fast, words({0, 0}));
    pump(2000);
    assert(frames(NORMAL_STOP) == 1 && frames(FAST_STOP) == 1 && wire.size() == observing + 2);
    const auto oldStop = actionOf(normal), newStop = actionOf(fast);
    assert(oldStop.execution == ActionExecution::ACKNOWLEDGED && oldStop.outcome == ActionOutcome::CANCELLED);
    assert(view(normal).interruptedByStop && !view(normal).pending);
    assert(newStop.execution == ActionExecution::ACKNOWLEDGED && newStop.completion == ActionCompletion::OBSERVED);
    assert(view(home).interruptedByStop && !view(home).pending && !axisReserved(*app, 1));
    noMoreFrames(2000);
}
// A-03 under USB pressure: an undelivered old stop report does not block escalation.
void escalatesPastUndeliveredStopReport() {
    fresh(); known();
    const auto normal = accepted("@1 stop normal");
    Serial.writeCapacity = 0;
    while (app->outputCount < OUTPUT_LINES) assert(emit(app, "blocked", 7));
    for (const char c : std::string("@2 status\n")) app->console.feed(c);
    assert(app->console.outputPending());
    reply(normal); reply(normal, words({0, 0}));
    assert(!view(normal).pending && !findRecord(*app, normal)->delivered);
    ActionRequest stop; stop.kind = ActionKind::STOP; stop.stop.behavior = StopBehavior::DIRECT;
    for (const char c : std::string("@3 stop fast\n")) app->console.feed(c);
    const auto fast = latest(); assert(fast != normal);
    reply(fast); reply(fast, words({0, 0}));
    assert(frames(FAST_STOP) == 1 && actionOf(normal).completion == ActionCompletion::OBSERVED);
    // A retained normal stop leaves a separate fast-escalation reserve.
    assert(!findRecord(*app, fast)->delivered);
    for (const char c : std::string("@4 stop fast\n")) app->console.feed(c);
    const auto escalation = latest(); assert(escalation != fast);
    assert(findRecord(*app, escalation)->stopEscalation);
    reply(escalation); reply(escalation, words({0, 0}));
    uint32_t refused = 777; assert(startAction(app, 5, 1, stop, refused) == Probe::Action::RESULTS_FULL && refused == 777);
    Serial.writeCapacity = 64; pump(8000);
    assert(findRecord(*app, normal)->delivered && findRecord(*app, fast)->delivered && findRecord(*app, escalation)->delivered);
}
// T3 / A-01(b): the stop is queued behind a poll whose reply is lost.
void queuedStopSurvivesLostPoll() {
    const auto home = runningHome();
    waitTx(home); // The poll is on the wire and no reply will ever be delivered.
    const auto polls = wire.size();
    const auto start = hardware.time;
    const auto stopping = accepted("@73 stop fast");
    assert(said("\"operation_id\":" + std::to_string(stopping)));
    const auto deadline = actionOf(stopping).deadlineUs;
    assert(deadline > start && deadline <= start + REQUEST_US + STOP_RECOVERY_US + 1000000);
    untilWire(FAST_STOP);
    assert(hardware.time < deadline && wire.size() == polls + 1 && wire.back() == FAST_STOP);
    assert(app->recovery.automatic && view(app->recovery.operationId).recoveryResult.outcome == Rtu::RecoveryOutcome::RECOVERED);
    assert(actionOf(stopping).deadlineUs == deadline); // Never renewed by re-preparation.
    assert(actionOf(stopping).target.generation == app->bindingGeneration);
    reply(stopping); reply(stopping, words({0, 0}));
    const auto finished = actionOf(stopping);
    assert(finished.execution == ActionExecution::ACKNOWLEDGED && finished.completion == ActionCompletion::OBSERVED);
    assert(frames(FAST_STOP) == 1);
    // The interrupted home keeps its own outcome and uncertainty; nothing resumes.
    assert(!view(home).pending && view(home).interruptedByStop);
    const auto interrupted = *view(home).homeContext;
    assert(interrupted.uncertain && interrupted.outcome != ActionOutcome::OBSERVED);
    pump(3000);
    assert(view(home).homeContext->outcome == interrupted.outcome);
    noMoreFrames(3000);
    assertRecovered("after queued stop");
    command("@75 recover\n"); assert(said("\"result\":\"accepted\"")); // A-07: automatic recovery blocks nothing.
}
// A-01(a): the stop is submitted while the owner already needs recovery.
void stopSubmittedAfterFault() {
    fresh(); known();
    command("@1 read state\n"); const auto read = latest(); waitTx(read); until(read);
    assert(app->owner.needsRecovery() && wire.size() == 1);
    const auto generation = app->bindingGeneration;
    const auto stopping = accepted("@2 stop fast");
    assert(view(stopping).pending && actionOf(stopping).execution == ActionExecution::NOT_TRANSMITTED);
    untilWire(FAST_STOP);
    assert(app->bindingGeneration == generation + 1 && wire.size() == 2);
    reply(stopping); reply(stopping, words({0, 0}));
    assert(actionOf(stopping).completion == ActionCompletion::OBSERVED && frames(FAST_STOP) == 1);
    assert(view(read).typedRead->state == ReadState::FAILED); // Independent retained failure.
    noMoreFrames(3000);
}
// A-01(a) with a capture/UART fault rather than a runner fault.
void stopSubmittedAfterUartFault() {
    fresh(); known(); hardware.rx.push_back(1); hardware.rx.push_back(3);
    uint32_t unchanged = 77; assert(probe(app, 1, 1, unchanged) == Probe::Action::RECOVERY_REQUIRED && unchanged == 77);
    const auto stopping = accepted("@2 stop fast");
    untilWire(FAST_STOP); assert(wire.size() == 1);
    reply(stopping); reply(stopping, words({0, 0}));
    assert(actionOf(stopping).completion == ActionCompletion::OBSERVED);
}
// R-02: harvest the owner's terminal zero-byte failure, not just a queued token.
// Service the real owner up to its result boundary before the app consumes it.
uint32_t stopWithSettledZeroTxFault() {
    fresh(); known();
    const auto stopping = apiStop();
    auto* record = findRecord(*app, stopping);
    const auto request = record->requestId;
    hardware.rx.push_back(1); hardware.rx.push_back(3);
    for (unsigned i = 0; i < 10000 && !app->owner.result(request); ++i) {
        advanceHardware(hardware.time + 10);
        app->owner.service(uart.sample());
    }
    const auto* settled = app->owner.result(request);
    assert(settled && !settled->transport.txAccepted && !settled->transport.txComplete);
    assert(!settled->executionUnknown && hardware.writes == 0);
    const auto outcome = settled->outcome;
    const auto reason = settled->transport.reason;
    step();
    assert(record->stopHeld && record->stopBeforeRecoveryKnown && !record->requestId.owner);
    assert(record->stopBeforeRecovery.id.slot == request.slot &&
        record->stopBeforeRecovery.id.generation == request.generation);
    assert(record->stopBeforeRecovery.outcome == outcome && record->stopBeforeRecovery.transport.reason == reason);
    assert(!record->stopBeforeRecovery.transport.txAccepted && !record->stopBeforeRecovery.executionUnknown);
    assert(record->stopRecoveryOperationId == app->recovery.operationId && record->stopRecoveryOperationId);
    assert(!app->owner.result(request)); // The copy survives release of owner storage.
    const auto exposed = copiedAction(stopping);
    assert(exposed.pending && exposed.beforeRecoveryKnown && exposed.recoveryOperationId == record->stopRecoveryOperationId);
    assert(exposed.beforeRecovery.id.generation == request.generation && exposed.beforeRecovery.id.slot == request.slot &&
        exposed.beforeRecovery.outcome == outcome && exposed.beforeRecovery.transport.reason == reason);
    assert(!exposed.beforeRecovery.transport.txAccepted && !exposed.beforeRecovery.executionUnknown && !exposed.recovery.id);
    return stopping;
}
void settledZeroTxStopRecoversOnce() {
    const auto stopping = stopWithSettledZeroTxFault();
    auto* record = findRecord(*app, stopping);
    const auto deadline = record->deadlineUs;
    const auto recovery = record->stopRecoveryOperationId;
    const auto failedBinding = record->stopBeforeRecovery.expected.targetGeneration;
    untilWire(FAST_STOP);
    assert(actionOf(stopping).deadlineUs == deadline && frames(FAST_STOP) == 1);
    assert(actionOf(stopping).target.generation != failedBinding);
    assert(record->stopBeforeRecovery.expected.targetGeneration == failedBinding);
    assert(view(recovery).recoveryResult.outcome == Rtu::RecoveryOutcome::RECOVERED);
    reply(stopping); reply(stopping, words({0, 0}));
    assert(actionOf(stopping).execution == ActionExecution::ACKNOWLEDGED &&
        actionOf(stopping).completion == ActionCompletion::OBSERVED);
    assert(record->stopBeforeRecoveryKnown && record->stopRecoveryOperationId == recovery);
    noMoreFrames(3000);
    assert(frames(FAST_STOP) == 1 && app->recovery.operationId == recovery);
    const auto recovered = record->stopRecoveryResult;
    assert(recovered.id == record->stopRecoveryId && recovered.outcome == Rtu::RecoveryOutcome::RECOVERED);
    command("@200 result " + std::to_string(stopping) + "\n");
    const auto originalJson = stopRecoveryJson();
    assert(originalJson.find("\"operation_id\":" + std::to_string(recovery) + ",\"pending\":false,\"result\":{") != std::string::npos);
    assert(originalJson.find("\"before_attempt\":{") != std::string::npos);
    assert(originalJson.find("\"generation\":" + std::to_string(failedBinding) + ",\"address\":1") != std::string::npos);
    assert(originalJson.find("\"tx_accepted\":0,\"tx_complete\":false") != std::string::npos);
    uint32_t laterRecovery = 0;
    assert(startRecovery(*app, 2, false, laterRecovery) == Probe::Action::OK && laterRecovery != recovery);
    until(laterRecovery, 3000000);
    assert(record->stopRecoveryOperationId == recovery && record->stopRecoveryResult.id == recovered.id &&
        record->stopRecoveryResult.outcome == recovered.outcome &&
        record->stopRecoveryResult.requestedUs == recovered.requestedUs &&
        record->stopRecoveryResult.finishedUs == recovered.finishedUs);
    assert(actionOf(stopping).completion == ActionCompletion::OBSERVED && frames(FAST_STOP) == 1);
    const auto exposed = copiedAction(stopping);
    assert(!exposed.pending && exposed.recoveryOperationId == recovery && exposed.recovery.id == recovered.id &&
        exposed.recovery.outcome == recovered.outcome && exposed.recovery.finishedUs == recovered.finishedUs &&
        exposed.beforeRecovery.expected.targetGeneration == failedBinding);
    command("@201 result " + std::to_string(stopping) + "\n");
    assert(stopRecoveryJson() == originalJson);
    assert(said("\"execution\":\"acknowledged\",\"completion\":\"observed\""));
}
void settledZeroTxStopRecoveryFailure() {
    const auto stopping = stopWithSettledZeroTxFault();
    auto* record = findRecord(*app, stopping);
    const auto recovery = record->stopRecoveryOperationId;
    fakeUart.status.rxd = 0; // No quiet receive interval; recovery cannot clear it.
    until(stopping);
    assert(actionOf(stopping).execution == ActionExecution::NOT_TRANSMITTED &&
        actionOf(stopping).completion == ActionCompletion::NOT_OBSERVED &&
        actionOf(stopping).outcome == ActionOutcome::TRANSPORT_ERROR);
    assert(view(recovery).recoveryResult.outcome == Rtu::RecoveryOutcome::EXPIRED);
    assert(record->stopRecoveryResult.outcome == Rtu::RecoveryOutcome::EXPIRED);
    assert(record->stopBeforeRecoveryKnown && wire.empty());
    // Wired bench 16: a held stop whose recovery fails ends not_transmitted and
    // is never eligible for the release after a failed stop.
    ReleaseAfterStopPrerequisites facts; facts.target.id = 999;
    assert(!inspectReleaseAfterStop(stopping, facts) && facts.target.id == 999);
    facts.mechanicsPermitRelease = facts.externalTriggersInhibited = true;
    uint32_t unchanged = 777;
    assert(!submitReleaseAfterStop(stopping, facts, unchanged) && unchanged == 777);
    noMoreFrames(3000);
    assert(app->recovery.operationId == recovery);
}
void settledZeroTxStopDeadlineAndCancellation() {
    for (const bool cancelStop : {false, true}) {
        const auto stopping = stopWithSettledZeroTxFault();
        auto* record = findRecord(*app, stopping);
        const auto deadline = record->deadlineUs;
        const auto recovery = record->stopRecoveryOperationId;
        if (cancelStop) {
            assert(cancel(app, stopping) == Probe::Action::OK);
            step();
        } else {
            advanceHardware(deadline);
            serviceApplication();
        }
        assert(view(stopping).pending == cancelStop && actionOf(stopping).state == ActionState::FAILED &&
            actionOf(stopping).execution == ActionExecution::NOT_TRANSMITTED);
        assert(actionOf(stopping).outcome == (cancelStop ? ActionOutcome::CANCELLED : ActionOutcome::DEADLINE));
        assert(actionOf(stopping).deadlineUs == deadline && record->stopBeforeRecoveryKnown);
        until(recovery, 3000000);
        const auto exposed = copiedAction(stopping);
        assert(!exposed.pending && exposed.beforeRecoveryKnown && exposed.recoveryOperationId == recovery &&
            exposed.recovery.id == record->stopRecoveryId &&
            exposed.recovery.outcome == view(recovery).recoveryResult.outcome);
        noMoreFrames(3000);
        assert(wire.empty() && app->recovery.operationId == recovery);
    }
}
void incompleteStopTxIsNeverRecovered() {
    fresh(); known(); hardware.txCharacterUs = 0;
    const auto stopping = accepted("@1 stop fast");
    waitTx(stopping); until(stopping);
    const auto& stopped = actionOf(stopping);
    assert(stopped.execution == ActionExecution::UNKNOWN && stopped.writeEvidence.txAccepted == 8 &&
        !stopped.writeEvidence.txComplete && stopped.completion == ActionCompletion::NOT_OBSERVED);
    assert(!findRecord(*app, stopping)->stopBeforeRecoveryKnown && !app->recovery.operationId);
    assert(wire.size() == 1 && frames(FAST_STOP) == 1 && app->owner.needsRecovery());
    noMoreFrames(3000);
    assert(frames(FAST_STOP) == 1 && !app->recovery.operationId);
}
// A-01(c): the stop is submitted while an explicit recovery runs.
void stopDuringExplicitRecovery() {
    fresh(); known();
    command("@1 recover\n"); assert(said("\"result\":\"accepted\"")); const auto recovery = latest();
    assert(app->owner.recovering());
    const auto stopping = accepted("@2 stop fast");
    assert(!wire.size() && view(recovery).pending && app->recovery.operationId == recovery);
    untilWire(FAST_STOP);
    assert(!view(recovery).pending && view(recovery).recoveryResult.outcome == Rtu::RecoveryOutcome::RECOVERED);
    assert(app->recovery.operationId == recovery && !app->recovery.automatic && wire.size() == 1);
    reply(stopping); reply(stopping, words({0, 0}));
    assert(actionOf(stopping).completion == ActionCompletion::OBSERVED && frames(FAST_STOP) == 1);
}
// Wired bench 18 item 7 (audit V2-F10): a reply delayed past its request's
// closure can match a later request of the same context byte for byte. The old
// read keeps its failure; the owner's interlock refuses the same request before
// any byte until an explicit recovery boundary. After that boundary a delayed
// matching reply is accepted as the new request's own: the recovery drained an
// idle link, which proves nothing about the age of a reply that arrives later.
void sameContextDelayedReply() {
    fresh(); known();
    command("@1 read state\n"); const auto first = latest(); waitTx(first);
    const auto frame = hardware.tx;
    until(first); // No reply: the read closes at its response timeout.
    const auto failed = *view(first).typedRead;
    assert(failed.state == ReadState::FAILED && app->owner.needsRecovery());
    const auto writes = hardware.writes;
    command("@2 read state\n");
    assert(said("\"result\":\"recovery_required\"") && hardware.writes == writes);
    command("@3 recover\n"); assert(said("\"result\":\"accepted\"")); const auto recovery = latest();
    until(recovery);
    assert(view(recovery).recoveryResult.outcome == Rtu::RecoveryOutcome::RECOVERED && !app->owner.needsRecovery());
    command("@4 read state\n"); const auto second = latest(); waitTx(second);
    assert(hardware.tx == frame && hardware.writes == writes + 1);
    // The first request's late reply arrives now; nothing separates it from the second's.
    readReply(second, words({0, 1}));
    assert(view(second).typedRead->step == 1 && view(second).pending);
    assert(view(first).typedRead->state == ReadState::FAILED && view(first).typedRead->step == failed.step);
}
// An explicit recover arriving after a stop was queued holds that stop too.
void explicitRecoverHoldsQueuedStop() {
    fresh(); known();
    command("@1 read state\n"); const auto read = latest(); waitTx(read);
    const auto stopping = accepted("@2 stop fast");
    assert(findRecord(*app, stopping)->requestId.owner && app->owner.pending() == 1);
    command("@3 recover\n"); assert(said("\"result\":\"accepted\""));
    assert(findRecord(*app, stopping)->stopHeld && !findRecord(*app, stopping)->requestId.owner);
    untilWire(FAST_STOP); assert(frames(FAST_STOP) == 1);
    reply(stopping); reply(stopping, words({0, 0}));
    assert(actionOf(stopping).completion == ActionCompletion::OBSERVED);
}
// Lost stop reply: UNKNOWN, one FC06, and only a new explicit stop sends again.
void transmittedStopStaysUnknown() {
    fresh(); known();
    const auto stopping = accepted("@1 stop fast");
    waitTx(stopping); assert(frames(FAST_STOP) == 1);
    until(stopping);
    const auto lost = actionOf(stopping);
    assert(lost.execution == ActionExecution::UNKNOWN && lost.outcome == ActionOutcome::TRANSPORT_ERROR);
    assert(app->owner.needsRecovery() && axisReserved(*app, 1));
    noMoreFrames();
    assert(frames(FAST_STOP) == 1 && app->owner.needsRecovery() && !app->recovery.operationId); // No automatic anything.
    const auto again = accepted("@2 stop fast");
    untilWire(FAST_STOP); // Already satisfied by the old frame; wait for the second below.
    for (unsigned i = 0; i < 40000 && frames(FAST_STOP) < 2; ++i) step(100);
    assert(frames(FAST_STOP) == 2 && wire.size() == 2);
    reply(again); reply(again, words({0, 0}));
    assert(actionOf(again).completion == ActionCompletion::OBSERVED && !axisReserved(*app, 1));
    assert(actionOf(stopping).execution == ActionExecution::UNKNOWN); // Old uncertainty unchanged.
}
// Two faults in a row: the poll is lost, then the stop's own reply is lost.
void twoFaultsInARow() {
    const auto home = runningHome(); waitTx(home);
    const auto first = accepted("@1 stop fast");
    untilWire(FAST_STOP);
    until(first);
    assert(actionOf(first).execution == ActionExecution::UNKNOWN && frames(FAST_STOP) == 1);
    noMoreFrames();
    const auto second = accepted("@2 stop normal");
    untilWire(NORMAL_STOP);
    reply(second); reply(second, words({0, 0}));
    assert(frames(FAST_STOP) == 1 && frames(NORMAL_STOP) == 1);
    assert(actionOf(second).completion == ActionCompletion::OBSERVED && actionOf(first).execution == ActionExecution::UNKNOWN);
}
// Recovery expiry ends the stop honestly, without a retry; a later stop can start again.
void recoveryExpiryEndsStop() {
    fresh(); known();
    hardware.txCharacterUs = 0; // Physical TX never completes, so recovery cannot settle.
    command("@1 read state\n"); const auto read = latest();
    for (unsigned i = 0; i < 25000 && !wire.size(); ++i) step();
    assert(wire.size() == 1);
    const auto stopping = accepted("@2 stop fast");
    const auto deadline = actionOf(stopping).deadlineUs;
    const auto start = hardware.time;
    while (view(stopping).pending && hardware.time - start < 8000000) step(1000);
    const auto ended = actionOf(stopping);
    assert(!view(stopping).pending && hardware.time <= deadline + 2000);
    assert(ended.execution == ActionExecution::NOT_TRANSMITTED);
    assert(ended.outcome == ActionOutcome::TRANSPORT_ERROR && !frames(FAST_STOP));
    assert(view(app->recovery.operationId).recoveryResult.outcome == Rtu::RecoveryOutcome::EXPIRED);
    assert(!view(read).pending && view(read).typedRead->state == ReadState::FAILED);
    const auto recoveryId = app->recovery.operationId;
    for (unsigned i = 0; i < 5000; ++i) step(1000);
    assert(wire.size() == 1 && app->recovery.operationId == recoveryId); // No retry loop.
    // After the physical cause clears, one new explicit stop starts over.
    fakeUart.status.txfifo_cnt = 0; fakeUart.fsm_status.st_utx_out = 0;
    hardware.txCharacterUs = 87;
    for (unsigned i = 0; i < 20 && app->runner.transmitEnabled(); ++i) step(100);
    assert(!app->runner.transmitEnabled());
    const auto again = accepted("@3 stop fast");
    untilWire(FAST_STOP);
    assert(app->recovery.operationId != recoveryId && frames(FAST_STOP) == 1);
    reply(again); reply(again, words({0, 0}));
    assert(actionOf(again).completion == ActionCompletion::OBSERVED);
    assert(actionOf(stopping).execution == ActionExecution::NOT_TRANSMITTED);
}
// A held stop escalated while recovery runs: only the newest stop reaches the wire.
void heldStopEscalation() {
    fresh(); known();
    command("@1 recover\n");
    const auto normal = accepted("@2 stop normal");
    const auto fast = accepted("@3 stop fast");
    pump(100);
    assert(view(normal).pending && actionOf(normal).execution == ActionExecution::NOT_TRANSMITTED);
    assert(actionOf(normal).state == ActionState::FAILED && actionOf(normal).outcome == ActionOutcome::CANCELLED && view(normal).interruptedByStop);
    untilWire(FAST_STOP);
    reply(fast); reply(fast, words({0, 0}));
    assert(!view(normal).pending && !frames(NORMAL_STOP) && frames(FAST_STOP) == 1);
    // Local cancellation of a held stop is honest and sends nothing afterwards.
    fresh(); known();
    command("@4 recover\n"); const auto recovery = latest();
    const auto cancelled = accepted("@5 stop fast");
    command("@6 cancel " + std::to_string(cancelled) + "\n");
    assert(view(cancelled).pending && actionOf(cancelled).execution == ActionExecution::NOT_TRANSMITTED);
    assert(actionOf(cancelled).state == ActionState::FAILED && actionOf(cancelled).outcome == ActionOutcome::CANCELLED);
    until(recovery, 3000000); noMoreFrames(3000);
    assert(!view(cancelled).pending && wire.empty());
}
// T4 / A-02: cancelling in-flight work leaves the frame to settle; no interlock.
void cancelAndMonitorOffDoNotInterlock() {
    fresh(); known();
    command("@60 read state\n"); const auto read = latest(); waitTx(read);
    command("@61 cancel " + std::to_string(read) + "\n");
    assert(said("\"result\":\"done\"") || said("\"result\":\"accepted\""));
    assert(app->owner.active());
    deliver(words({0, 1}));
    for (unsigned i = 0; i < 25000 && view(read).pending; ++i) step();
    assert(!view(read).pending && view(read).typedRead->outcome == ReadOutcome::CANCELLED);
    assert(!app->owner.needsRecovery());
    const auto stopping = accepted("@62 stop fast");
    assert(!findRecord(*app, stopping)->stopHeld);
    reply(stopping); reply(stopping, words({0, 0}));
    assert(frames(FAST_STOP) == 1 && !app->recovery.operationId);

    fresh(); known();
    command("@63 monitor 100 5\n");
    App::Record& monitored = app->records[REQUEST_CAPACITY];
    for (unsigned i = 0; i < 25000 && !monitored.operationId; ++i) step();
    const auto request = monitored.requestId;
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(request); ++i) step();
    assert(app->owner.txAccepted(request));
    command("@64 monitor off\n"); assert(said("\"enabled\":false"));
    assert(app->owner.active() && !app->owner.needsRecovery());
    deliver(words({0, 1}));
    pump(3000);
    assert(!app->owner.needsRecovery() && !monitored.operationId);
    const auto afterMonitor = accepted("@65 stop fast");
    reply(afterMonitor); reply(afterMonitor, words({0, 0}));
    assert(!app->recovery.operationId && frames(FAST_STOP) == 1);

    // A sequenced action follows the same rule, and a lost reply still reaches stop.
    fresh(); known();
    command("@66 enable\n"); const auto enabling = latest(); waitTx(enabling);
    command("@67 cancel " + std::to_string(enabling) + "\n");
    assert(app->owner.active() && !app->owner.needsRecovery());
    until(enabling);
    assert(actionOf(enabling).execution == ActionExecution::UNKNOWN && app->owner.needsRecovery());
    const auto afterLoss = accepted("@68 stop fast");
    untilWire(FAST_STOP);
    reply(afterLoss); reply(afterLoss, words({0, 0}));
    assert(actionOf(afterLoss).completion == ActionCompletion::OBSERVED);
}
// T5 / A-06: first stop after a reset, with an empty known-target bitmap.
void firstStopAfterBoot() {
    fresh();
    const auto stopping = accepted("@1 stop fast");
    waitTx(stopping);
    assert(wire.size() == 1 && wire[0] == FAST_STOP);
    fresh();
    command("@2 stop fast 9\n"); assert(said("\"result\":\"unavailable\"") && !wire.size());
    command("@3 enable\n"); assert(said("\"result\":\"unavailable\"") && !wire.size());
}
// Bench fixture outputs may hold or retrigger a wired drive input. Every stop
// request turns them OFF before admission, including a refused stop; the
// bench command itself is local and never reaches the bus.
struct RecordingBench final : BenchIo::Port {
    unsigned applies = 0, quiesces = 0, services = 0;
    BenchIo::Request request;
    BenchIo::OffReason reason = BenchIo::OffReason::NONE;
    BenchIo::Result apply(const BenchIo::Request& r, BenchIo::View& view) override {
        ++applies; request = r; view = BenchIo::View(); view.ready = true; return BenchIo::Result::OK;
    }
    void quiesce(BenchIo::OffReason why) override { ++quiesces; reason = why; }
    void service(uint64_t) override { ++services; }
};
void stopTurnsBenchOutputsOff() {
    RecordingBench bench;
    ApplicationOptions options; options.benchIo = &bench;
    fresh(options);
    pump(4); assert(bench.services >= 4);
    command("@1 bench on relay2 1500\n");
    assert(said("\"command\":\"bench\",\"ok\":true") && bench.applies == 1 && !bench.quiesces);
    assert(bench.request.op == BenchIo::Op::ON && bench.request.output == 1 && bench.request.holdMs == 1500);
    assert(!wire.size());
    const auto stopping = accepted("@2 stop fast");
    assert(bench.quiesces == 1 && bench.reason == BenchIo::OffReason::STOP);
    waitTx(stopping);
    assert(wire.size() == 1 && wire[0] == FAST_STOP);
    command("@3 stop normal 9\n"); // Refused (unknown target), still releases the outputs.
    assert(said("\"result\":\"unavailable\"") && bench.quiesces == 2);
    command("@4 enable\n"); assert(bench.quiesces == 2); // Only stops quiesce.
    fresh();
    command("@5 bench\n"); assert(said("\"result\":\"unavailable\"") && !wire.size());
}
void failStop(uint32_t id) {
    reply(id, crc({1, 0x86, 2}));
    until(id); pump(200);
    assert(!view(id).pending && findRecord(*app, id)->delivered);
    assert(actionOf(id).execution == ActionExecution::REJECTED);
}
// T6 / A-05: stop admission never depends on ordinary result capacity.
void failedStopsDoNotDependOnOrdinaryCapacity() {
    fresh(); known();
    uint32_t retained[REQUEST_CAPACITY];
    for (unsigned i = 0; i < REQUEST_CAPACITY; ++i) {
        retained[i] = accepted("@" + std::to_string(100 + i) + " probe 1");
        waitTx(retained[i]); deliver(crc({1, 3, 2, 0, 60}));
        for (unsigned k = 0; k < 25000 && view(retained[i]).pending; ++k) step();
    }
    command("@200 probe 1\n"); assert(said("\"result\":\"results_full\""));
    const auto first = accepted("@201 stop fast"); failStop(first);
    const auto second = accepted("@202 stop fast"); failStop(second);
    // Both evidence slots stay inspectable while a free dedicated slot exists.
    assert(actionOf(first).execution == ActionExecution::REJECTED && actionOf(second).execution == ActionExecution::REJECTED);
    assert(!app->stopEvidenceOverwritten);
    const auto third = accepted("@203 stop fast");
    assert(app->stopEvidenceOverwritten == 1);
    Probe::ResultView gone; assert(!host(app).result(app, first, gone)); // Oldest, counted.
    assert(actionOf(second).execution == ActionExecution::REJECTED);
    reply(third); reply(third, words({0, 0}));
    assert(actionOf(third).completion == ActionCompletion::OBSERVED && frames(FAST_STOP) == 3);
    for (const auto id : retained) assert(view(id).probe.outcome == Rtu::Outcome::SUCCESS);
    command("@204 drv\n"); assert(said("\"stop_evidence_overwritten\":1"));
}
void runRecovery(uint32_t id) { until(id, 3000000); pump(200); }
// A-07: a delivered recovery result is replaced; an undelivered one still refuses.
void recoverReplacesDeliveredResult() {
    fresh(); known();
    command("@1 recover\n"); const auto first = latest();
    command("@2 recover\n"); assert(said("\"result\":\"results_full\"")); // Still running.
    runRecovery(first); assert(app->recovery.delivered);
    command("@3 recover\n"); assert(said("\"result\":\"accepted\""));
    const auto second = latest(); assert(second != first && !wire.size());
    Probe::ResultView gone; assert(!host(app).result(app, first, gone));
    runRecovery(second);
    assert(view(second).recoveryResult.outcome == Rtu::RecoveryOutcome::RECOVERED);
}
// A held stop does not wait for output: an undelivered recovery report moves aside.
void automaticRecoveryDisplacesUndeliveredResult() {
    fresh(); known();
    command("@1 recover\n"); const auto explicitRecovery = latest();
    Serial.writeCapacity = 0;
    while (app->outputCount < OUTPUT_LINES) assert(emit(app, "blocked", 7));
    for (const char c : std::string("@2 status\n")) app->console.feed(c);
    assert(app->console.outputPending());
    until(explicitRecovery, 3000000); pump(200);
    assert(!app->recovery.delivered);
    // Force the next transport fault, then stop through the API.
    hardware.rx.push_back(1); hardware.rx.push_back(3); pump(5);
    assert(uart.needsRecovery());
    ActionRequest stop; stop.kind = ActionKind::STOP; stop.stop.behavior = StopBehavior::DIRECT;
    uint32_t stopping = 0; assert(startAction(app, 3, 1, stop, stopping) == Probe::Action::OK);
    untilWire(FAST_STOP);
    assert(app->recovery.automatic && app->recoveryDisplaced.operationId == explicitRecovery);
    assert(view(explicitRecovery).recoveryResult.outcome == Rtu::RecoveryOutcome::RECOVERED);
    Serial.writeCapacity = 64; pump(8000);
    assert(app->recoveryDisplaced.delivered);
    assert(said("\"operation_id\":" + std::to_string(explicitRecovery)));
}
// ProbeConsole:1800: a stop arriving while a stop reply is pending gets its own reply.
void everyStopGetsAnExplicitReply() {
    fresh(); known();
    Serial.writeCapacity = 0;
    while (app->outputCount < OUTPUT_LINES) assert(emit(app, "blocked", 7));
    for (const char c : std::string("@2 status\n")) app->console.feed(c);
    assert(app->console.outputPending());
    const auto dropped = app->console.inputDropped();
    for (const char c : std::string("@3 stop normal\n@4 stop fast\n@5 stop fast\n@6 stop fast\n")) app->console.feed(c);
    assert(app->console.inputDropped() == dropped);
    pump(100);
    Serial.writeCapacity = 64; Serial.output.clear(); pump(8000);
    assert(said("\"id\":3,\"command\":\"stop\",\"ok\":true,\"result\":\"accepted\""));
    assert(said("\"id\":4,\"command\":\"stop\",\"ok\":true,\"result\":\"accepted\""));
    assert(said("\"id\":5,\"command\":\"stop\",\"ok\":true,\"result\":\"accepted\""));
    assert(said("\"id\":6,\"command\":\"stop\",\"ok\":false,\"result\":\"results_full\""));
    assert(app->console.inputDropped() == dropped);
}
// Non-transport host ownership keeps today's refusal.
void hostBlocksStillRefuse() {
    fresh(); known(); app->serial.blocked = true;
    command("@1 stop fast\n"); assert(said("\"result\":\"recovery_required\"") && !wire.size());
    app->serial.blocked = false;
    assert(app->owner.beginConfiguration(uart.sample()));
    ActionRequest stop; stop.kind = ActionKind::STOP; stop.stop.behavior = StopBehavior::DIRECT;
    uint32_t unchanged = 7; assert(startAction(app, 2, 1, stop, unchanged) == Probe::Action::RECOVERY_REQUIRED && unchanged == 7);
}
// Public callers retain their result until they copy it; console output is irrelevant.
void headlessStopResults() {
    fresh(); known(); Serial.output.clear();
    uint32_t previous = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const auto id = apiStop();
        auto pending = copiedAction(id);
        assert(pending.pending && pending.action.operationId == id && !pending.stopEscalation);
        assert(pending.replacedOperationId == previous && !releaseOperation(id));
        assert(!findRecord(*app, id)->delivered);
        reply(id); reply(id, words({0, 0})); pump(200);
        assert(!findRecord(*app, id)->delivered && !releaseOperation(id));
        assert(Serial.output.find("\"operation_id\":" + std::to_string(id)) == std::string::npos);
        const auto done = copiedAction(id);
        assert(!done.pending && done.action.execution == ActionExecution::ACKNOWLEDGED &&
            done.action.completion == ActionCompletion::OBSERVED && findRecord(*app, id)->delivered);
        if (previous) { ActionResult absent; assert(!copyActionResult(previous, absent)); }
        previous = id;
    }
    assert(releaseOperation(previous) && !releaseOperation(previous));
    assert(frames(FAST_STOP) == 3 && !app->stopEvidenceOverwritten);
    noMoreFrames(3000);
}
void cancelledStopWaitsForRecoveryEvidence() {
    fresh(); known();
    hardware.rx.push_back(1); hardware.rx.push_back(3); uart.sample();
    assert(uart.needsRecovery());
    const auto id = apiStop(); step();
    assert(app->owner.recovering());
    const auto recovery = app->recovery.operationId;
    const auto ownerRecovery = app->recovery.id;
    const auto deadline = actionOf(id).deadlineUs;
    assert(cancel(app, id) == Probe::Action::OK);
    const auto pending = copiedAction(id);
    assert(pending.action.state == ActionState::FAILED && pending.action.outcome == ActionOutcome::CANCELLED);
    assert(pending.pending && pending.recoveryOperationId == recovery && !pending.recovery.id);
    assert(!findRecord(*app, id)->delivered && !releaseOperation(id) && wire.empty());
    command("@200 result " + std::to_string(id) + "\n");
    assert(said("\"result\":\"pending\"") && said("\"stop_recovery\":{\"operation_id\":" +
        std::to_string(recovery) + ",\"pending\":true,\"result\":null,\"before_attempt\":null}"));
    assert(app->owner.recovering() && !findRecord(*app, id)->delivered && !releaseOperation(id));
    runRecovery(recovery);
    assert(!findRecord(*app, id)->delivered && !releaseOperation(id));
    const auto complete = copiedAction(id);
    assert(!complete.pending && complete.action.outcome == ActionOutcome::CANCELLED);
    assert(complete.action.execution == ActionExecution::NOT_TRANSMITTED && complete.action.deadlineUs == deadline);
    assert(complete.recoveryOperationId == recovery && complete.recovery.id == ownerRecovery &&
        complete.recovery.outcome == Rtu::RecoveryOutcome::RECOVERED);
    assert(releaseOperation(id));
    noMoreFrames(3000); assert(wire.empty());
}
void heldEscalationPreservesPendingRecoveryResults() {
    fresh(); known();
    hardware.rx.push_back(1); hardware.rx.push_back(3); uart.sample();
    assert(uart.needsRecovery());
    const auto normal = apiStop(1, StopBehavior::CONFIGURED_DECELERATION); step();
    assert(app->owner.recovering());
    const auto recovery = app->recovery.operationId;
    const auto ownerRecovery = app->recovery.id;
    const auto fast = apiStop(); step();
    const auto cancelled = copiedAction(normal);
    assert(cancelled.action.outcome == ActionOutcome::CANCELLED && cancelled.pending);
    assert(!findRecord(*app, normal)->delivered && !releaseOperation(normal));
    // Copying the cancelled action must not make its primary slot reusable.
    const auto escalation = apiStop(); step();
    const auto reserved = copiedAction(escalation);
    assert(reserved.pending && reserved.stopEscalation && !reserved.replacedOperationId);
    assert(findRecord(*app, normal) && findRecord(*app, fast));
    const auto cancelledFast = copiedAction(fast);
    assert(cancelledFast.pending && cancelledFast.action.outcome == ActionOutcome::CANCELLED);
    assert(!findRecord(*app, fast)->delivered && !releaseOperation(fast));
    StopPolicy direct; direct.behavior = StopBehavior::DIRECT;
    uint32_t unchanged = 777;
    const auto refused = submitStop(1, direct, unchanged);
    assert(!refused && refused.detail == static_cast<int32_t>(Probe::Action::RESULTS_FULL) && unchanged == 777);
    assert(wire.empty() && !app->stopEvidenceOverwritten);
    runRecovery(recovery);
    for (const auto id : {normal, fast}) {
        assert(!findRecord(*app, id)->delivered && !releaseOperation(id));
        const auto result = copiedAction(id);
        assert(!result.pending && result.action.outcome == ActionOutcome::CANCELLED);
        assert(result.action.execution == ActionExecution::NOT_TRANSMITTED);
        assert(result.recoveryOperationId == recovery && result.recovery.id == ownerRecovery &&
            result.recovery.outcome == Rtu::RecoveryOutcome::RECOVERED);
        assert(releaseOperation(id));
    }
    reply(escalation); reply(escalation, words({0, 0}));
    assert(copiedAction(escalation).action.completion == ActionCompletion::OBSERVED);
    assert(releaseOperation(escalation));
    assert(!frames(NORMAL_STOP) && frames(FAST_STOP) == 1 && !app->stopEvidenceOverwritten);
    noMoreFrames(3000);
}
void wrongActionCopyPreservesOutput() {
    fresh(); known();
    const auto stop = apiStop();
    ActionResult output = copiedAction(stop);
    output.recoveryOperationId = 789;
    assert(!copyActionResult(0, output) && !copyActionResult(UINT32_MAX, output));
    assert(output.action.operationId == stop && output.recoveryOperationId == 789 && output.pending);
    uint32_t read = 0; assert(submitHomingParameterRead(read));
    assert(!copyActionResult(read, output));
    assert(output.action.operationId == stop && output.recoveryOperationId == 789 && output.pending);
    assert(!findRecord(*app, stop)->delivered && !findRecord(*app, read)->delivered);
    fresh(); known();
    const auto consoleStop = accepted("@1 stop fast");
    assert(!copyActionResult(consoleStop, output));
    assert(output.action.operationId == stop && output.recoveryOperationId == 789);
}
// Two distinct normal stops must each retain a fast escalation under pressure.
void escalationCapacityAcrossAddresses() {
    fresh(); known(); app->knownTargets[0] |= 4;
    const auto normal1 = apiStop(1, StopBehavior::CONFIGURED_DECELERATION);
    reply(normal1); reply(normal1, words({0, 4}));
    const auto normal2 = apiStop(2, StopBehavior::CONFIGURED_DECELERATION);
    const auto firstDeadline = actionOf(normal1).deadlineUs;
    const auto secondDeadline = actionOf(normal2).deadlineUs;
    const auto fast1 = apiStop(1);
    const auto first = copiedAction(fast1);
    assert(first.pending && first.stopEscalation && !first.replacedOperationId);
    StopPolicy direct; direct.behavior = StopBehavior::DIRECT;
    uint32_t unchanged = 777;
    const auto duplicate = submitStop(1, direct, unchanged);
    assert(!duplicate && duplicate.detail == static_cast<int32_t>(Probe::Action::RESULTS_FULL) && unchanged == 777);
    assert(copiedAction(fast1).pending && !findRecord(*app, fast1)->cancelContinuation);
    const auto fast2 = apiStop(2);
    assert(copiedAction(fast2).stopEscalation && !copiedAction(fast2).replacedOperationId);
    // Both normal results survive; only the first normal stop had reached TX.
    pump(100);
    const auto old1 = view(normal1), old2 = view(normal2);
    assert(!old1.pending && old1.interruptedByStop && old1.actionContext->execution == ActionExecution::ACKNOWLEDGED);
    assert(old1.actionContext->outcome == ActionOutcome::CANCELLED && old1.actionContext->deadlineUs == firstDeadline);
    assert(!old2.pending && old2.interruptedByStop && old2.actionContext->execution == ActionExecution::NOT_TRANSMITTED);
    assert(old2.actionContext->outcome == ActionOutcome::CANCELLED && old2.actionContext->deadlineUs == secondDeadline);
    reply(fast1); reply(fast2); // Urgent writes precede ordinary observation polls.
    reply(fast1, words({0, 0})); reply(fast2, words({0, 0}, 2));
    assert(copiedAction(fast1).action.completion == ActionCompletion::OBSERVED);
    assert(copiedAction(fast2).action.completion == ActionCompletion::OBSERVED);
    // Primary results are still undelivered. Reuse the oldest confirmed reserve.
    const auto again = apiStop(1);
    const auto replacement = copiedAction(again);
    assert(replacement.stopEscalation && replacement.replacedOperationId == fast1);
    assert(!findRecord(*app, fast1) && findRecord(*app, normal1) && findRecord(*app, normal2));
    reply(again); reply(again, words({0, 0}));
    assert(copiedAction(again).action.completion == ActionCompletion::OBSERVED);
    assert(frames(NORMAL_STOP) == 1 && frames(crc({2, 6, 0, 0x27, 1, 0})) == 0);
    assert(frames(FAST_STOP) == 2 && frames(crc({2, 6, 0, 0x27, 2, 0})) == 1);
    assert(!app->stopEvidenceOverwritten && !axisReserved(*app));
    for (const auto id : {normal1, normal2, again, fast2}) { copiedAction(id); assert(releaseOperation(id)); }
    noMoreFrames(3000);
}
// R-05: an interlocked observation cannot wait until the action deadline.
void stopObservationFaultKeepsAcknowledgement() {
    fresh(); known();
    const auto id = apiStop(); reply(id);
    const auto before = copiedAction(id);
    assert(before.pending && before.action.execution == ActionExecution::ACKNOWLEDGED);
    hardware.rx.push_back(1); hardware.rx.push_back(3);
    pump(500);
    const auto ended = copiedAction(id);
    assert(!ended.pending && ended.action.outcome == ActionOutcome::TRANSPORT_ERROR);
    assert(ended.action.execution == ActionExecution::ACKNOWLEDGED &&
        ended.action.completion == ActionCompletion::NOT_OBSERVED);
    assert(ended.action.writeEvidence.txAccepted == before.action.writeEvidence.txAccepted &&
        ended.action.writeEvidence.txComplete == before.action.writeEvidence.txComplete);
    assert(hardware.time + 500000 < ended.action.deadlineUs && axisReserved(*app, 1));
    assert(!ended.recoveryOperationId && !app->recovery.operationId && frames(FAST_STOP) == 1);
    noMoreFrames(3000);
}
// Settle the real recovery before the application consumes its completion.
void settleRecoveryWithoutActionService() {
    advanceHardware(std::max(hardware.time, app->recoveryGuardUntilUs));
    app->recovery.prepared = uart.clear(); assert(app->recovery.prepared);
    for (unsigned i = 0; i < 30000 && app->owner.recovering(); ++i) {
        advanceHardware(hardware.time + 100);
        app->owner.service(uart.sample(), app->recovery.prepared);
    }
    assert(!app->owner.recovering());
    assert(app->owner.recoveryResult(app->recovery.id)->outcome == Rtu::RecoveryOutcome::RECOVERED);
}
// Completing one recovery and starting another cannot silently relink a held stop.
void heldStopKeepsFirstRecoveryIdentity() {
    for (const bool delayedService : {false, true}) {
        const auto id = stopWithSettledZeroTxFault();
        auto* record = findRecord(*app, id);
        const auto recovery = record->stopRecoveryOperationId;
        const auto ownerRecovery = record->stopRecoveryId;
        const auto deadline = record->deadlineUs;
        settleRecoveryWithoutActionService();
        assert(record->stopHeld && wire.empty());
        deliver(*app); assert(app->recovery.delivered);
        uint32_t second = 0;
        assert(startRecovery(*app, 100, false, second) == Probe::Action::OK && second != recovery);
        // The owner may complete recovery 2 in this turn before action service.
        step(delayedService ? RECOVER_US + 1000 : 10);
        assert(!view(id).pending && actionOf(id).outcome == ActionOutcome::TRANSPORT_ERROR);
        assert(actionOf(id).execution == ActionExecution::NOT_TRANSMITTED && actionOf(id).deadlineUs == deadline);
        assert(record->stopRecoveryOperationId == recovery && record->stopRecoveryId == ownerRecovery);
        assert(record->stopRecoveryResult.id == ownerRecovery &&
            record->stopRecoveryResult.outcome == Rtu::RecoveryOutcome::RECOVERED);
        until(second); noMoreFrames(3000);
        assert(wire.empty() && record->stopRecoveryOperationId == recovery && record->stopRecoveryResult.id == ownerRecovery);
    }
}
void allHeldStopsBindRecoveryBeforeFirstService() {
    fresh(); known(); app->knownTargets[0] |= 4;
    hardware.rx.push_back(1); hardware.rx.push_back(3); uart.sample();
    assert(uart.needsRecovery());
    const uint32_t first = apiStop(1), second = apiStop(2);
    assert(findRecord(*app, first)->stopHeld && findRecord(*app, second)->stopHeld && wire.empty());
    uint32_t recovery = 0;
    assert(startRecovery(*app, 0, true, recovery) == Probe::Action::OK);
    const auto ownerRecovery = app->recovery.id;
    for (const auto id : {first, second}) {
        const auto* record = findRecord(*app, id);
        assert(record->stopRecoveryId == ownerRecovery && record->stopRecoveryOperationId == recovery);
    }
    settleRecoveryWithoutActionService();
    deliver(*app); assert(app->recovery.delivered);
    uint32_t nextRecovery = 0;
    assert(startRecovery(*app, 101, false, nextRecovery) == Probe::Action::OK && nextRecovery != recovery);
    step(RECOVER_US + 1000);
    for (const auto id : {first, second}) {
        const auto result = copiedAction(id);
        assert(!result.pending && result.action.outcome == ActionOutcome::TRANSPORT_ERROR);
        assert(result.action.execution == ActionExecution::NOT_TRANSMITTED && !result.action.writeEvidence.txAccepted);
        assert(result.recoveryOperationId == recovery && result.recovery.id == ownerRecovery &&
            result.recovery.outcome == Rtu::RecoveryOutcome::RECOVERED);
    }
    until(nextRecovery); noMoreFrames(3000); assert(wire.empty());
}
void stopAdmittedDuringRecoveryKeepsItsCompletion() {
    fresh(); known();
    uint32_t recovery = 0;
    assert(startRecovery(*app, 100, false, recovery) == Probe::Action::OK);
    const auto ownerRecovery = app->recovery.id;
    const auto id = apiStop();
    auto* record = findRecord(*app, id);
    assert(record->stopHeld && record->stopRecoveryId == ownerRecovery && record->stopRecoveryOperationId == recovery);
    const auto deadline = record->deadlineUs;
    settleRecoveryWithoutActionService();
    assert(record->stopHeld && !record->stopRecoveryResult.id && wire.empty());
    reply(id); reply(id, words({0, 0}));
    const auto result = copiedAction(id);
    assert(!result.pending && result.action.completion == ActionCompletion::OBSERVED);
    assert(result.action.deadlineUs == deadline && frames(FAST_STOP) == 1);
    assert(result.recoveryOperationId == recovery && result.recovery.id == ownerRecovery &&
        result.recovery.outcome == Rtu::RecoveryOutcome::RECOVERED);
    noMoreFrames(3000); assert(frames(FAST_STOP) == 1 && app->recovery.operationId == recovery);
}
uint32_t failedStopForRelease(uint32_t maxAgeMs = 0) {
    ApplicationOptions options; options.observationMaxAgeMs = maxAgeMs;
    fresh(options); known();
    const auto failed = apiStop(); waitTx(failed); until(failed);
    assert(copiedAction(failed).action.execution == ActionExecution::UNKNOWN && axisReserved(*app, 1));
    ReleaseAfterStopPrerequisites facts;
    assert(!inspectReleaseAfterStop(failed, facts)); // The transport must recover first.
    uint32_t recovery = 0; assert(startRecovery(*app, 100, false, recovery) == Probe::Action::OK);
    runRecovery(recovery);
    const auto probeId = accepted("@101 probe 1");
    waitTx(probeId); deliver(words({60})); until(probeId); pump(200);
    assert(view(probeId).probe.outcome == Rtu::Outcome::SUCCESS && releaseOperation(probeId));
    assert(inspectReleaseAfterStop(failed, facts));
    assert(!facts.mechanicsPermitRelease && !facts.externalTriggersInhibited);
    return failed;
}
ReleaseAfterStopPrerequisites releaseFacts(uint32_t failed) {
    ReleaseAfterStopPrerequisites facts;
    assert(inspectReleaseAfterStop(failed, facts));
    facts.mechanicsPermitRelease = facts.externalTriggersInhibited = true;
    return facts;
}
void qualifiedReleasePreservesMotionUncertainty() {
    const auto failed = failedStopForRelease(100);
    const auto original = copiedAction(failed);
    const auto facts = releaseFacts(failed);
    const auto before = wire.size();
    uint32_t unchanged = 987;
    for (unsigned which = 0; which < 9; ++which) {
        auto invalid = facts;
        switch (which) {
        case 0: invalid.target.id++; break;
        case 1: invalid.target.address++; break;
        case 2: invalid.target.generation++; break;
        case 3: invalid.configurationGeneration++; break;
        case 4: invalid.qualifiedUs = hardware.time + 1; break;
        case 5: invalid.qualifiedUs = hardware.time - 100001; break;
        case 6: invalid.mechanicsPermitRelease = false; break;
        case 7: invalid.externalTriggersInhibited = false; break;
        case 8: invalid.configurationGeneration = 0; break;
        }
        assert(!submitReleaseAfterStop(failed, invalid, unchanged) && unchanged == 987 && wire.size() == before);
    }
    assert(!submitReleaseAfterStop(failed + 1000, facts, unchanged) && unchanged == 987);
    for (const auto* operation : {"enable", "alarm-clear", "motor-release"}) {
        command(std::string("@102 ") + operation + "\n");
        assert(said("\"result\":\"axis_conflict\"") && wire.size() == before);
    }
    const auto current = releaseFacts(failed);
    uint32_t releaseId = 0; assert(submitReleaseAfterStop(failed, current, releaseId));
    assert(!submitReleaseAfterStop(failed, current, unchanged) && unchanged == 987); // Live work still owns the axis.
    reply(releaseId); reply(releaseId, words({0, 0x14})); // RELEASED and RUNNING coexist.
    const auto released = copiedAction(releaseId), retained = copiedAction(failed);
    assert(released.action.request.kind == ActionKind::RELEASE && released.action.completion == ActionCompletion::OBSERVED);
    assert(released.action.rawMotion == 0x14 && axisReserved(*app, 1));
    assert(retained.action.execution == original.action.execution && retained.action.outcome == original.action.outcome &&
        retained.action.writeEvidence.txAccepted == original.action.writeEvidence.txAccepted);
    assert(frames(crc({1, 6, 0, 0x2d, 0, 0x11})) == 1 && frames(FAST_STOP) == 1);
    assert(releaseOperation(releaseId));
    command("@103 enable\n"); assert(said("\"result\":\"axis_conflict\""));
    command("@104 alarm-clear\n"); assert(said("\"result\":\"axis_conflict\""));
    noMoreFrames(3000);
}
void qualifiedReleaseNeverReplays() {
    const auto failed = failedStopForRelease();
    uint32_t releaseId = 0; assert(submitReleaseAfterStop(failed, releaseFacts(failed), releaseId));
    waitTx(releaseId); until(releaseId);
    const auto result = copiedAction(releaseId);
    assert(result.action.execution == ActionExecution::UNKNOWN && result.action.outcome == ActionOutcome::TRANSPORT_ERROR);
    assert(copiedAction(failed).action.execution == ActionExecution::UNKNOWN && axisReserved(*app, 1));
    assert(frames(crc({1, 6, 0, 0x2d, 0, 0x11})) == 1 && frames(FAST_STOP) == 1);
    noMoreFrames(3000);
}
void releaseQualificationExpiresBeforeTx() {
    const auto failed = failedStopForRelease(1);
    uint32_t releaseId = 0; assert(submitReleaseAfterStop(failed, releaseFacts(failed), releaseId));
    step(2000); until(releaseId);
    const auto expired = copiedAction(releaseId);
    assert(expired.action.execution == ActionExecution::NOT_TRANSMITTED && expired.action.outcome == ActionOutcome::CANCELLED);
    assert(!frames(crc({1, 6, 0, 0x2d, 0, 0x11})) && frames(FAST_STOP) == 1 && axisReserved(*app, 1));
    noMoreFrames(3000);
}
void releaseQualificationExactExpiry() {
    for (const bool alreadyQueued : {false, true}) {
        const auto failed = failedStopForRelease(100);
        const auto facts = releaseFacts(failed);
        const auto before = wire.size();
        hardware.clockStep = 0;
        uint32_t releaseId = 777;
        if (alreadyQueued) assert(submitReleaseAfterStop(failed, facts, releaseId));
        advanceHardware(facts.qualifiedUs + 100000);
        assert(!evidenceAgeValid(facts.qualifiedUs, hardware.time, 100000));
        if (alreadyQueued) {
            serviceApplication(); until(releaseId);
            const auto expired = copiedAction(releaseId);
            assert(!expired.pending && expired.action.execution == ActionExecution::NOT_TRANSMITTED &&
                expired.action.outcome == ActionOutcome::CANCELLED);
        } else {
            const auto refused = submitReleaseAfterStop(failed, facts, releaseId);
            assert(!refused && refused.detail == static_cast<int32_t>(Probe::Action::QUALIFICATION_REQUIRED) && releaseId == 777);
        }
        assert(wire.size() == before && !frames(crc({1, 6, 0, 0x2d, 0, 0x11})) && axisReserved(*app, 1));
        noMoreFrames(3000);
    }
}
void releaseRequiresFailedTransmittedStop() {
    for (const bool cancelBeforeTx : {false, true}) {
        fresh(); known();
        const auto id = apiStop();
        if (cancelBeforeTx) { assert(cancel(app, id) == Probe::Action::OK); until(id); }
        else { reply(id); reply(id, words({0, 0})); }
        const auto result = copiedAction(id);
        assert(result.action.execution == (cancelBeforeTx ? ActionExecution::NOT_TRANSMITTED : ActionExecution::ACKNOWLEDGED));
        ReleaseAfterStopPrerequisites output; output.target.id = 999; output.qualifiedUs = 555;
        assert(!inspectReleaseAfterStop(id, output) && output.target.id == 999 && output.qualifiedUs == 555);
        uint32_t unchanged = 999;
        assert(!submitReleaseAfterStop(id, output, unchanged) && unchanged == 999);
        noMoreFrames(3000);
    }
}
void releaseQualificationClosesAfterObservedStop() {
    const auto failed = failedStopForRelease();
    const auto facts = releaseFacts(failed);
    const auto stopping = apiStop();
    reply(stopping); reply(stopping, words({0, 0}));
    assert(!axisReserved(*app, 1) && copiedAction(failed).action.execution == ActionExecution::UNKNOWN);
    ReleaseAfterStopPrerequisites output; output.target.id = 999;
    assert(!inspectReleaseAfterStop(failed, output) && output.target.id == 999);
    uint32_t unchanged = 777;
    assert(!submitReleaseAfterStop(failed, facts, unchanged) && unchanged == 777);
    assert(frames(FAST_STOP) == 2 && !frames(crc({1, 6, 0, 0x2d, 0, 0x11})));
    noMoreFrames(3000);
}
void qualifiedReleaseConsoleParity() {
    const auto failed = failedStopForRelease();
    const auto before = wire.size();
    const auto prefix = "@102 motor-release after-stop " + std::to_string(failed);
    command(prefix + " mechanics-permit-release\n"); assert(!said("\"result\":\"accepted\""));
    command(prefix + " mechanics-permit-release triggers-inhibited extra\n"); assert(!said("\"result\":\"accepted\""));
    assert(wire.size() == before);
    const auto releasing = accepted(prefix + " mechanics-permit-release triggers-inhibited");
    reply(releasing); reply(releasing, words({0, 0x14})); pump(200);
    const auto result = actionOf(releasing);
    assert(result.request.kind == ActionKind::RELEASE && result.completion == ActionCompletion::OBSERVED && result.rawMotion == 0x14);
    assert(axisReserved(*app, 1) && copiedAction(failed).action.execution == ActionExecution::UNKNOWN);
    assert(frames(crc({1, 6, 0, 0x2d, 0, 0x11})) == 1 && frames(FAST_STOP) == 1);
    assert(findRecord(*app, releasing)->delivered && !findRecord(*app, releasing)->programmaticAction);
    noMoreFrames(3000);
}
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--emit-recovery-result") {
        settledZeroTxStopRecoversOnce();
        std::fputs(Serial.output.c_str(), stdout);
        if (app) { app->~App(); std::free(app); app = nullptr; }
        return 0;
    }
    if (argc != 1) return 2;
    using Scenario = void (*)();
    const struct { const char* name; Scenario run; } scenarios[] = {
        {"escalatesRunningNormalStop", escalatesRunningNormalStop},
        {"escalatesPastUndeliveredStopReport", escalatesPastUndeliveredStopReport},
        {"queuedStopSurvivesLostPoll", queuedStopSurvivesLostPoll},
        {"stopSubmittedAfterFault", stopSubmittedAfterFault},
        {"stopSubmittedAfterUartFault", stopSubmittedAfterUartFault},
        {"settledZeroTxStopRecoversOnce", settledZeroTxStopRecoversOnce},
        {"settledZeroTxStopRecoveryFailure", settledZeroTxStopRecoveryFailure},
        {"settledZeroTxStopDeadlineAndCancellation", settledZeroTxStopDeadlineAndCancellation},
        {"incompleteStopTxIsNeverRecovered", incompleteStopTxIsNeverRecovered},
        {"stopDuringExplicitRecovery", stopDuringExplicitRecovery},
        {"explicitRecoverHoldsQueuedStop", explicitRecoverHoldsQueuedStop},
        {"sameContextDelayedReply", sameContextDelayedReply},
        {"transmittedStopStaysUnknown", transmittedStopStaysUnknown},
        {"twoFaultsInARow", twoFaultsInARow},
        {"recoveryExpiryEndsStop", recoveryExpiryEndsStop},
        {"heldStopEscalation", heldStopEscalation},
        {"cancelAndMonitorOffDoNotInterlock", cancelAndMonitorOffDoNotInterlock},
        {"firstStopAfterBoot", firstStopAfterBoot},
        {"stopTurnsBenchOutputsOff", stopTurnsBenchOutputsOff},
        {"failedStopsDoNotDependOnOrdinaryCapacity", failedStopsDoNotDependOnOrdinaryCapacity},
        {"recoverReplacesDeliveredResult", recoverReplacesDeliveredResult},
        {"automaticRecoveryDisplacesUndeliveredResult", automaticRecoveryDisplacesUndeliveredResult},
        {"everyStopGetsAnExplicitReply", everyStopGetsAnExplicitReply},
        {"hostBlocksStillRefuse", hostBlocksStillRefuse},
        {"headlessStopResults", headlessStopResults},
        {"cancelledStopWaitsForRecoveryEvidence", cancelledStopWaitsForRecoveryEvidence},
        {"heldEscalationPreservesPendingRecoveryResults", heldEscalationPreservesPendingRecoveryResults},
        {"wrongActionCopyPreservesOutput", wrongActionCopyPreservesOutput},
        {"escalationCapacityAcrossAddresses", escalationCapacityAcrossAddresses},
        {"stopObservationFaultKeepsAcknowledgement", stopObservationFaultKeepsAcknowledgement},
        {"heldStopKeepsFirstRecoveryIdentity", heldStopKeepsFirstRecoveryIdentity},
        {"allHeldStopsBindRecoveryBeforeFirstService", allHeldStopsBindRecoveryBeforeFirstService},
        {"stopAdmittedDuringRecoveryKeepsItsCompletion", stopAdmittedDuringRecoveryKeepsItsCompletion},
        {"qualifiedReleasePreservesMotionUncertainty", qualifiedReleasePreservesMotionUncertainty},
        {"qualifiedReleaseNeverReplays", qualifiedReleaseNeverReplays},
        {"releaseQualificationExpiresBeforeTx", releaseQualificationExpiresBeforeTx},
        {"releaseQualificationExactExpiry", releaseQualificationExactExpiry},
        {"releaseRequiresFailedTransmittedStop", releaseRequiresFailedTransmittedStop},
        {"releaseQualificationClosesAfterObservedStop", releaseQualificationClosesAfterObservedStop},
        {"qualifiedReleaseConsoleParity", qualifiedReleaseConsoleParity},
    };
    for (const auto& scenario : scenarios) {
        std::puts(scenario.name); std::fflush(stdout);
        scenario.run();
    }
    if (app) { app->~App(); std::free(app); app = nullptr; }
    return 0;
}
