// SPDX-License-Identifier: MIT
// Actual cooperative application with real owner/runner/UART fakes: every stop
// claim below is asserted against the bytes on the fake wire.
#include "../examples/probe_cli/ProbeApp.cpp"
#include "fakes/esp32_uart/UsbConsoleFixture.h"
#include "../examples/probe_cli/ArduinoPlatform.cpp"
#include "../examples/probe_cli/main.cpp"
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
void fresh() {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = writeResponseConfirmed = false;
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit));
    hardware.txCharacterUs = 87; assert(uart.startCapture(20, timing().holdUs));
    wire.clear(); seenWrites = 0; assert(app && !hardware.writes);
}
Probe::ResultView view(uint32_t id) { Probe::ResultView v; assert(host(app).result(app, id, v)); return v; }
std::vector<uint8_t> crc(std::vector<uint8_t> v) {
    const uint16_t c = ESS::calcCrc16(v.data(), v.size());
    v.push_back(static_cast<uint8_t>(c)); v.push_back(static_cast<uint8_t>(c >> 8)); return v;
}
std::vector<uint8_t> words(std::initializer_list<uint16_t> values) {
    std::vector<uint8_t> result = {1, 3, static_cast<uint8_t>(values.size() * 2)};
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
    // Two reports waiting for output is the explicit limit, never a silent loss.
    assert(!findRecord(*app, fast)->delivered);
    uint32_t refused = 777; assert(startAction(app, 4, 1, stop, refused) == Probe::Action::RESULTS_FULL && refused == 777);
    Serial.writeCapacity = 64; pump(8000);
    assert(findRecord(*app, normal)->delivered && findRecord(*app, fast)->delivered);
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
    assert(!view(normal).pending && actionOf(normal).execution == ActionExecution::NOT_TRANSMITTED);
    assert(actionOf(normal).outcome == ActionOutcome::CANCELLED && view(normal).interruptedByStop);
    untilWire(FAST_STOP);
    reply(fast); reply(fast, words({0, 0}));
    assert(!frames(NORMAL_STOP) && frames(FAST_STOP) == 1);
    // Local cancellation of a held stop is honest and sends nothing afterwards.
    fresh(); known();
    command("@4 recover\n"); const auto recovery = latest();
    const auto cancelled = accepted("@5 stop fast");
    command("@6 cancel " + std::to_string(cancelled) + "\n");
    assert(!view(cancelled).pending && actionOf(cancelled).execution == ActionExecution::NOT_TRANSMITTED);
    assert(actionOf(cancelled).outcome == ActionOutcome::CANCELLED);
    until(recovery, 3000000); noMoreFrames(3000);
    assert(wire.empty());
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
    for (const char c : std::string("@3 stop normal\n@4 stop fast\n@5 stop fast\n")) app->console.feed(c);
    assert(app->console.inputDropped() == dropped);
    pump(100);
    Serial.writeCapacity = 64; Serial.output.clear(); pump(8000);
    assert(said("\"id\":3,\"command\":\"stop\",\"ok\":true,\"result\":\"accepted\""));
    assert(said("\"id\":4,\"command\":\"stop\",\"ok\":true,\"result\":\"accepted\""));
    assert(said("\"id\":5,\"command\":\"stop\",\"ok\":false,\"result\":\"busy\""));
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
}
int main() {
    using Scenario = void (*)();
    const struct { const char* name; Scenario run; } scenarios[] = {
        {"escalatesRunningNormalStop", escalatesRunningNormalStop},
        {"escalatesPastUndeliveredStopReport", escalatesPastUndeliveredStopReport},
        {"queuedStopSurvivesLostPoll", queuedStopSurvivesLostPoll},
        {"stopSubmittedAfterFault", stopSubmittedAfterFault},
        {"stopSubmittedAfterUartFault", stopSubmittedAfterUartFault},
        {"stopDuringExplicitRecovery", stopDuringExplicitRecovery},
        {"explicitRecoverHoldsQueuedStop", explicitRecoverHoldsQueuedStop},
        {"transmittedStopStaysUnknown", transmittedStopStaysUnknown},
        {"twoFaultsInARow", twoFaultsInARow},
        {"recoveryExpiryEndsStop", recoveryExpiryEndsStop},
        {"heldStopEscalation", heldStopEscalation},
        {"cancelAndMonitorOffDoNotInterlock", cancelAndMonitorOffDoNotInterlock},
        {"firstStopAfterBoot", firstStopAfterBoot},
        {"failedStopsDoNotDependOnOrdinaryCapacity", failedStopsDoNotDependOnOrdinaryCapacity},
        {"recoverReplacesDeliveredResult", recoverReplacesDeliveredResult},
        {"automaticRecoveryDisplacesUndeliveredResult", automaticRecoveryDisplacesUndeliveredResult},
        {"everyStopGetsAnExplicitReply", everyStopGetsAnExplicitReply},
        {"hostBlocksStillRefuse", hostBlocksStillRefuse},
    };
    for (const auto& scenario : scenarios) {
        std::puts(scenario.name); std::fflush(stdout);
        scenario.run();
    }
    if (app) { app->~App(); std::free(app); app = nullptr; }
    return 0;
}
