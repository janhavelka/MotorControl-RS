// SPDX-License-Identifier: MIT
// Actual setup/loop, owner, profile parsers and UART adapter; SDK/wire are faked.
#include "../examples/probe_cli/ProbeApp.cpp"
#include "fakes/esp32_uart/UsbConsoleFixture.h"
#include "../examples/probe_cli/ArduinoPlatform.cpp"
#include "../examples/probe_cli/main.cpp"
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

namespace {
using ScanPhase = Probe::DiscoveryPhase;
using ScanOutcome = Probe::DiscoveryOutcome;
using ProbeOutcome = MotorControlRS::ProbeOutcome;
void fresh() {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = false; writeResponseConfirmed = false;
    setup(); assert(app && uart.ready() && app->owner.valid() && !hardware.writes);
    hardware.txCharacterUs = 87;
    assert(uart.startCapture(20, timing().holdUs));
}
void step(uint32_t us = 10) {
    hardware.txCharacterUs = (1000000U * bitsPerCharacter(app->serial.active.format) +
        app->serial.active.baud - 1) / app->serial.active.baud;
    advanceHardware(hardware.time + us); loop();
}
void pump(unsigned count = 200) { for (unsigned i = 0; i < count; ++i) step(); }
Probe::Action control(Probe::DiscoveryCommandKind kind, const Probe::DiscoverySettings* settings = nullptr) {
    Probe::DiscoveryCommand command; command.kind = kind;
    if (settings) command.settings = *settings;
    Probe::DiscoveryView view;
    assert(host(app).discovery);
    return host(app).discovery(app, &command, view);
}
const Probe::DiscoveryScan& scan() {
    Probe::DiscoveryView view;
    assert(host(app).discovery(app, nullptr, view) == Probe::Action::OK && view.scan);
    return *view.scan;
}
std::vector<uint8_t> crc(std::vector<uint8_t> bytes) {
    const uint16_t value = ESS::calcCrc16(bytes.data(), bytes.size());
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8));
    return bytes;
}
std::vector<uint8_t> words(uint8_t address, std::initializer_list<uint16_t> values) {
    std::vector<uint8_t> bytes = {address, 3, static_cast<uint8_t>(2 * values.size())};
    for (auto value : values) { bytes.push_back(static_cast<uint8_t>(value >> 8)); bytes.push_back(static_cast<uint8_t>(value)); }
    return crc(bytes);
}
void waitTx(unsigned before) {
    for (unsigned i = 0; i < 100000 && hardware.writes == before; ++i) step();
    assert(hardware.writes == before + 1 && hardware.tx.size() == 8);
}
void reviewedFrame(uint8_t address, uint16_t count) {
    assert(hardware.tx[0] == address && hardware.tx[1] == 3 && hardware.tx[2] == 0 && hardware.tx[3] == 0);
    assert(hardware.tx[4] == 0 && hardware.tx[5] == count);
    assert(ESS::calcCrc16(hardware.tx.data(), hardware.tx.size()) == 0);
}
void reply(unsigned before, const std::vector<uint8_t>& bytes) {
    waitTx(before);
    const uint32_t character = hardware.txCharacterUs;
    scheduleReply(std::max(hardware.writeStarted + 8 * character + app->serial.timing.replyGapUs + 500,
        hardware.time + app->serial.timing.replyGapUs + 500), bytes, character);
    const uint64_t finish = hardware.events.back().at + app->serial.timing.runner.gap35Us + 3000;
    while (hardware.time < finish) step();
}
void waitSettled() {
    for (unsigned i = 0; i < 100000 && scan().phase != ScanPhase::TERMINAL && scan().phase != ScanPhase::INTERLOCK; ++i) step();
    assert(scan().phase == ScanPhase::TERMINAL || scan().phase == ScanPhase::INTERLOCK);
}
Probe::DiscoverySettings range(uint8_t first, uint8_t last) {
    Probe::DiscoverySettings value; value.first = first; value.last = last; return value;
}
void minimalCurrentTupleAndRetainedEvidence() {
    fresh(); const auto original = app->serial.active;
    const auto target = app->axis.target; const auto generation = app->axis.generation;
    assert(control(Probe::DiscoveryCommandKind::BEGIN) == Probe::Action::OK);
    waitTx(0); reviewedFrame(1, 1);
    reply(0, words(1, {0x4EEA})); waitSettled();
    assert(scan().outcome == ScanOutcome::COMPLETE && scan().restored && scan().count == 1 && scan().requests == 1);
    const auto& finding = scan().findings[0];
    assert(finding.probe.outcome == ProbeOutcome::RESPONDER && finding.probe.rawModelKnown && finding.probe.rawModel == 0x4EEA);
    assert(finding.probe.confidence == MotorControlRS::ProbeConfidence::RESPONDER_MODEL_UNRESOLVED);
    assert(!finding.collisionExcluded && !finding.identityAttempted && !finding.identityKnown);
    assert(finding.request.target.address == 1 && finding.request.activeSerial.baud == original.baud);
    assert(finding.probe.provenance.latestUs && finding.probe.provenance.latestUs <= finding.request.deadlineUs);
    assert(sameTuple(app->serial.active, original) && app->axis.target.id == target.id && app->axis.target.address == target.address);
    assert(app->axis.generation == generation && hardware.writes == 1);
    const auto observed = finding.probe.provenance.latestUs;
    reset(app); pump();
    assert(scan().findings[0].probe.provenance.latestUs == observed && scan().count == 1 && hardware.writes == 1);
}
void rejectedCandidatesAndExclusiveAdmission() {
    fresh(); const unsigned configurations = hardware.configCalls;
    Probe::DiscoverySettings candidates;
    candidates.first = 2; candidates.last = 1;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::INVALID);
    candidates = range(0, 2);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::INVALID);
    candidates = range(1, 248);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::INVALID);
    candidates = range(1, 1); candidates.tupleCount = 2;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::INVALID);
    candidates = range(1, 1); candidates.tupleCount = 1; candidates.tuples[0].baud = 57600;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::UNSUPPORTED);
    candidates = range(1, 1); candidates.queryMs = 0;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::INVALID);
    candidates = range(1, 1); candidates.overallMs = 0;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::INVALID);
    candidates = range(1, 1); candidates.requestLimit = 0;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::INVALID);
    candidates = range(1, 1); candidates.resultLimit = 0;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::INVALID);
    candidates = range(1, 1); candidates.resultLimit = Probe::DISCOVERY_MAX_RESULTS + 1;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::INVALID);
    candidates = range(1, 1); candidates.profile = static_cast<MotorControlRS::DriveProfile>(99);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::UNSUPPORTED);
    assert(hardware.configCalls == configurations && !hardware.writes && !app->owner.pending());

    Probe::MonitorSettings monitoring; monitoring.enabled = true; monitoring.intervalMs = 100; monitoring.count = 1;
    Probe::MonitorSnapshot monitored;
    assert(host(app).monitor(app, &monitoring, monitored) == Probe::Action::OK);
    assert(control(Probe::DiscoveryCommandKind::BEGIN) == Probe::Action::BUSY);
    monitoring.enabled = false;
    assert(host(app).monitor(app, &monitoring, monitored) == Probe::Action::OK);
    assert(control(Probe::DiscoveryCommandKind::BEGIN) == Probe::Action::OK);
    uint32_t unchanged = 123;
    assert(host(app).startProbe(app, 4, 1, unchanged) == Probe::Action::BUSY);
    assert(host(app).startTypedRead(app, 4, 1, ESS::ReadKind::IDENTITY, unchanged) == Probe::Action::BUSY);
    ESS::DriverRequest driver; ESS::HomeRequest home;
    MotorControlRS::MoveRequest move; MotorControlRS::VelocityRequest velocity;
    MotorControlRS::ActionRequest enable; enable.kind = MotorControlRS::ActionKind::ENABLE;
    assert(host(app).startAction(app, 4, 1, enable, unchanged) == Probe::Action::BUSY);
    assert(host(app).startMove(app, 4, 1, move, unchanged) == Probe::Action::BUSY);
    assert(host(app).startVelocity(app, 4, 1, velocity, unchanged) == Probe::Action::BUSY);
    assert(host(app).startHome(app, 4, 1, home, unchanged) == Probe::Action::BUSY);
    assert(host(app).startDriver(app, 4, 1, ESS::DriverKind::READ, driver, unchanged) == Probe::Action::BUSY);
    monitoring.enabled = true;
    assert(host(app).monitor(app, &monitoring, monitored) == Probe::Action::BUSY);
    Probe::HostRequest serial; serial.tuple.baud = 9600; Probe::HostSnapshot state;
    assert(host(app).hostSerial(app, &serial, state) == Probe::Action::BUSY);
    assert(control(Probe::DiscoveryCommandKind::BEGIN) == Probe::Action::BUSY);
    assert(unchanged == 123 && !hardware.writes && hardware.configCalls == configurations);
}
void finiteBudgetsAndCheckedExceptions() {
    fresh(); auto candidates = range(1, 2); candidates.requestLimit = 1;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {60})); waitSettled();
    assert(scan().outcome == ScanOutcome::REQUEST_LIMIT && scan().count == 1 && scan().requests == 1);
    assert(scan().restored && hardware.writes == 1);

    fresh(); candidates = range(1, 2); candidates.resultLimit = 1;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {60})); waitSettled();
    assert(scan().outcome == ScanOutcome::RESULT_LIMIT && scan().count == 1 && scan().requests == 1);
    assert(scan().restored && hardware.writes == 1);

    fresh(); candidates = range(1, 2);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, crc({1, 0x83, 2}));
    assert(!app->owner.needsRecovery());
    reply(1, words(2, {0xABCD})); reviewedFrame(2, 1); waitSettled();
    assert(scan().outcome == ScanOutcome::COMPLETE && scan().count == 2 && scan().requests == 2);
    assert(scan().findings[0].probe.outcome == ProbeOutcome::EXCEPTION);
    assert(scan().findings[0].probe.status.code == MotorControlRS::Err::EXCEPTION && scan().findings[0].probe.status.detail == 2);
    assert(!scan().findings[0].probe.rawModelKnown && scan().findings[1].probe.rawModel == 0xABCD);
    assert(scan().findings[0].request.target.address == 1 && scan().findings[1].request.target.address == 2);
    assert(!app->modelKnown && !app->configuration.operationId && !app->identity.operationId);
}
void identityRefinementAndBudget() {
    fresh(); auto candidates = range(1, 1); candidates.identity = true;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA}));
    reply(1, words(1, {0x4EEA, 0x29, 1, 0})); reviewedFrame(1, 4); waitSettled();
    assert(scan().outcome == ScanOutcome::COMPLETE && scan().count == 1 && scan().requests == 2);
    const auto& finding = scan().findings[0];
    assert(finding.identityAttempted && finding.identityKnown && !finding.identityAmbiguous);
    assert(finding.identity.rawModel == 0x4EEA && !finding.collisionExcluded);
    assert(finding.identityOperationId == finding.identity.operationId && finding.identityOperationId != finding.request.operationId);
    assert(finding.identityDeadlineUs <= scan().deadlineUs);
    assert(!app->identity.operationId && !app->modelKnown); // Scan does not bind selected-axis caches.

    fresh(); candidates.requestLimit = 1;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA})); waitSettled();
    assert(scan().outcome == ScanOutcome::REQUEST_LIMIT && scan().count == 1 && scan().requests == 1);
    assert(!scan().findings[0].identityAttempted && !scan().findings[0].identityKnown && hardware.writes == 1);

    fresh(); candidates.requestLimit = 2;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA})); reply(1, words(1, {60, 0x29, 1, 0})); waitSettled();
    assert(scan().count == 1 && scan().findings[0].probe.rawModel == 0x4EEA);
    assert(scan().findings[0].identityAmbiguous && !scan().findings[0].collisionExcluded);
}
void partialFindingsStopOnTransportFault() {
    for (unsigned scenario = 0; scenario < 6; ++scenario) {
        fresh(); const auto candidates = range(1, 3);
        assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
        reply(0, words(1, {0x4EEA}));
        if (scenario == 0) {
            auto broken = words(2, {60}); broken.back() ^= 1; reply(1, broken);
        } else if (scenario == 1) {
            reply(1, words(3, {60})); // Checked frame belongs to the wrong endpoint.
        } else if (scenario == 2) {
            reply(1, words(2, {60, 60})); // Wrong byte count/length.
        } else if (scenario == 3) {
            waitTx(1); // A timeout may not be silently recovered to scan endpoint 3.
        } else if (scenario == 5) {
            auto duplicate = words(2, {60}); const auto second = duplicate;
            duplicate.insert(duplicate.end(), second.begin(), second.end());
            reply(1, duplicate); // Two contending replies are not unique identity evidence.
        } else {
            waitTx(1);
            const auto late = hardware.writeStarted + 8 * hardware.txCharacterUs +
                app->serial.timing.responseTimeoutUs + 1000;
            scheduleReply(late, words(2, {60}), hardware.txCharacterUs);
        }
        waitSettled();
        assert(scan().outcome == ScanOutcome::TRANSPORT_FAULT && scan().count == 2 && scan().requests == 2);
        assert(scan().findings[0].probe.outcome == ProbeOutcome::RESPONDER && scan().findings[0].probe.rawModel == 0x4EEA);
        assert(scan().findings[1].probe.outcome != ProbeOutcome::RESPONDER && app->owner.needsRecovery());
        if (scenario == 0 || scenario == 2 || scenario == 5) assert(scan().findings[1].probe.outcome == ProbeOutcome::MALFORMED);
        if (scenario == 1) assert(scan().findings[1].probe.outcome == ProbeOutcome::MISMATCH);
        if (scenario == 3 || scenario == 4) assert(scan().findings[1].probe.outcome == ProbeOutcome::NO_RESPONSE);
        const unsigned configurations = hardware.configCalls;
        pump(1000);
        assert(hardware.writes == 2 && hardware.configCalls == configurations && scan().count == 2);
        uint32_t unchanged = 123;
        assert(host(app).startProbe(app, 4, 1, unchanged) != Probe::Action::OK);
        assert(unchanged == 123 && hardware.writes == 2);
    }
}
void cancellationSettlesCurrentFrame() {
    fresh(); auto candidates = range(1, 3);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    assert(host(app).cancel(app, scan().operationId) == Probe::Action::OK);
    waitSettled(); assert(scan().outcome == ScanOutcome::CANCELLED && scan().restored && !hardware.writes);

    fresh(); assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    for (unsigned i = 0; i < 10000 && app->runner.phase() != Rtu::Phase::SETUP; ++i) step();
    assert(app->runner.phase() == Rtu::Phase::SETUP && !hardware.writes);
    assert(host(app).cancel(app, 0) == Probe::Action::OK);
    waitSettled(); assert(scan().outcome == ScanOutcome::CANCELLED && !hardware.writes);

    for (bool receive : {false, true}) {
        fresh(); assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
        waitTx(0); reviewedFrame(1, 1);
        if (receive) {
            for (unsigned i = 0; i < 10000 && app->runner.phase() != Rtu::Phase::RECEIVE; ++i) step();
            assert(app->runner.phase() == Rtu::Phase::RECEIVE);
        } else assert(app->runner.transmitEnabled());
        assert(control(Probe::DiscoveryCommandKind::CANCEL) == Probe::Action::OK);
        reply(0, words(1, {0x4EEA})); waitSettled();
        assert(scan().outcome == ScanOutcome::CANCELLED && scan().count == 1 && scan().restored);
        assert(scan().findings[0].probe.outcome == ProbeOutcome::RESPONDER && !app->owner.needsRecovery());
        assert(!app->runner.transmitEnabled() && hardware.writes == 1);
    }
}
void exactOriginalTupleRestoration() {
    fresh(); Probe::HostRequest initial; initial.tuple.baud = 38400; initial.tuple.format = HostFormat::E8_1;
    Probe::HostSnapshot state;
    assert(host(app).hostSerial(app, &initial, state) == Probe::Action::OK);
    app->axis.originKnown = true; app->axis.originNative = 99;
    const uint32_t generation = app->axis.generation, binding = app->bindingGeneration;
    auto candidates = range(1, 1); candidates.tupleCount = 2;
    candidates.tuples[0].baud = 9600; candidates.tuples[0].format = HostFormat::N8_2;
    candidates.tuples[1].baud = 115200;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {60})); reply(1, words(1, {0x4EEA})); waitSettled();
    assert(scan().restored && scan().outcome == ScanOutcome::COMPLETE && scan().count == 2);
    assert(sameTuple(scan().originalTuple, initial.tuple) && sameTuple(app->serial.active, initial.tuple));
    assert(app->serial.original.baud == 115200); // Restore scan entry, not startup defaults.
    assert(scan().findings[0].request.activeSerial.baud == 9600 && scan().findings[0].request.activeSerial.stopBits == 2);
    assert(scan().findings[1].request.activeSerial.baud == 115200);
    assert(app->axis.generation == generation && app->bindingGeneration == binding);
    assert(app->axis.originKnown && app->axis.originNative == 99 && hardware.writes == 2);
}
void restorationFailureAndExplicitRepair() {
    fresh(); auto candidates = range(1, 1); candidates.tupleCount = 1; candidates.tuples[0].baud = 9600;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    waitTx(0); assert(app->serial.active.baud == 9600);
    hardware.configFailCalls = {hardware.configCalls + 1};
    reply(0, words(1, {0x4EEA})); waitSettled();
    assert(scan().phase == ScanPhase::INTERLOCK && !scan().restored && scan().owned);
    assert(scan().outcome == ScanOutcome::RESTORE_FAILED && scan().count == 1);
    assert(app->serial.blocked && !app->serial.activeKnown && app->owner.configurationOwned());
    const auto evidence = scan().findings[0].probe.provenance.latestUs;
    const auto calls = hardware.configCalls;
    pump(1000); assert(hardware.configCalls == calls && hardware.writes == 1);
    assert(control(Probe::DiscoveryCommandKind::FINISH) == Probe::Action::BUSY);
    uint32_t unchanged = 123;
    assert(host(app).startProbe(app, 9, 1, unchanged) != Probe::Action::OK && unchanged == 123);
    hardware.configFailCalls.clear();
    assert(control(Probe::DiscoveryCommandKind::RESTORE) == Probe::Action::OK);
    waitSettled();
    assert(scan().restored && !app->serial.blocked && app->serial.activeKnown && app->serial.active.baud == 115200);
    assert(!app->owner.configurationOwned() && scan().findings[0].probe.provenance.latestUs == evidence && hardware.writes == 1);
}
void timeoutOnCandidateNeedsExplicitRecovery() {
    fresh(); auto candidates = range(1, 2); candidates.tupleCount = 1; candidates.tuples[0].baud = 9600;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    waitTx(0); waitSettled();
    assert(scan().phase == ScanPhase::INTERLOCK && !scan().restored && scan().owned && scan().count == 1);
    assert(scan().outcome == ScanOutcome::TRANSPORT_FAULT && scan().findings[0].probe.outcome == ProbeOutcome::NO_RESPONSE);
    assert(app->serial.active.baud == 9600 && app->owner.needsRecovery());
    assert(control(Probe::DiscoveryCommandKind::RESTORE) == Probe::Action::RECOVERY_REQUIRED);
    const unsigned configurations = hardware.configCalls;
    pump(1000); assert(hardware.configCalls == configurations && hardware.writes == 1);
    uint32_t id = 0; assert(host(app).recover(app, 99, id) == Probe::Action::OK);
    for (unsigned i = 0; i < 10000 && app->owner.recovering(); ++i) step(1000);
    assert(!app->owner.needsRecovery() && !uart.needsRecovery());
    assert(control(Probe::DiscoveryCommandKind::RESTORE) == Probe::Action::OK);
    waitSettled();
    assert(scan().restored && app->serial.active.baud == 115200 && scan().count == 1 && hardware.writes == 1);
    assert(scan().findings[0].request.activeSerial.baud == 9600);
}
void overallDeadlineIncludesAdapterSetup() {
    fresh(); auto candidates = range(1, 1); candidates.tupleCount = 1; candidates.tuples[0].baud = 9600;
    candidates.overallMs = 1; hardware.configDelayUs = 5000;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    waitSettled();
    assert(scan().outcome == ScanOutcome::DEADLINE && scan().restored && !hardware.writes && !scan().requests);
    assert(app->serial.active.baud == 115200);
}
void capturedClosureSurvivesLateCancellationDelivery() {
    fresh(); const auto candidates = range(1, 2);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    waitTx(0);
    const auto id = app->discoveryRequest;
    scheduleReply(std::max(hardware.writeStarted + 8 * 87 + 1000, hardware.time + 1000), words(1, {0x4EEA}));
    for (unsigned i = 0; i < 25000 && !app->owner.result(id); ++i) {
        advanceHardware(hardware.time + 10); app->owner.service(uart.sample());
    }
    assert(app->owner.result(id) && app->owner.result(id)->outcome == Rtu::Outcome::SUCCESS && !scan().count);
    advanceHardware(scan().deadlineUs + 1000);
    assert(control(Probe::DiscoveryCommandKind::CANCEL) == Probe::Action::OK);
    serviceDiscovery(*app, uart.sample()); waitSettled();
    assert(scan().outcome == ScanOutcome::CANCELLED && scan().restored && scan().count == 1);
    const auto& finding = scan().findings[0];
    assert(finding.probe.outcome == ProbeOutcome::RESPONDER && finding.probe.rawModel == 0x4EEA);
    assert(finding.probe.provenance.latestUs <= finding.request.deadlineUs);
    assert(finding.probe.provenance.deliveredUs > scan().deadlineUs && hardware.writes == 1);
}
void urgentStopUsesReservedCapacityAndDoesNotReplayScan() {
    fresh();
    for (unsigned i = 0; i < 6; ++i) {
        uint32_t operation = 0;
        assert(host(app).startProbe(app, i + 1, 1, operation) == Probe::Action::OK);
        reply(i, words(1, {0x4EEA}));
        Probe::ResultView result;
        assert(host(app).result(app, operation, result) && !result.pending && result.probe.outcome == Rtu::Outcome::SUCCESS);
    }
    assert(app->knownTargets[0] & 2);
    const auto candidates = range(1, 3);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    waitTx(6); reviewedFrame(1, 1);
    MotorControlRS::ActionRequest stop; stop.kind = MotorControlRS::ActionKind::STOP;
    uint32_t id = 123;
    assert(host(app).startAction(app, 77, 1, stop, id) == Probe::Action::INVALID);
    assert(id == 123 && !scan().cancelRequested); // Rejected stop cannot cancel the scan.
    stop.stop.behavior = MotorControlRS::StopBehavior::CONFIGURED_DECELERATION;
    assert(host(app).startAction(app, 78, 1, stop, id) == Probe::Action::OK && id != 123);
    assert(scan().cancelRequested && scan().outcome == ScanOutcome::PREEMPTED);
    reply(6, words(1, {0x4EEA}));
    waitTx(7);
    assert(hardware.tx[0] == 1 && hardware.tx[1] == 6);
    const auto echo = hardware.tx; writeResponseConfirmed = true; // Simulated response-source qualification.
    reply(7, echo);
    reply(8, words(1, {0, 0}));
    Probe::ResultView result;
    assert(host(app).result(app, id, result) && result.actionContext && !result.pending);
    assert(result.actionContext->execution == MotorControlRS::ActionExecution::ACKNOWLEDGED);
    assert(result.actionContext->completion == MotorControlRS::ActionCompletion::OBSERVED);
    assert(scan().outcome == ScanOutcome::PREEMPTED && scan().restored && scan().count == 1 && hardware.writes == 9);
    pump(1000); assert(hardware.writes == 9 && !app->owner.needsRecovery());
}
void consoleAndDirectProductionParity() {
    fresh();
    const auto command = [](const std::string& text) {
        Serial.output.clear(); Serial.input = text;
        for (unsigned i = 0; i < 1000 && !Serial.input.empty(); ++i) step();
        assert(Serial.input.empty()); pump(1000);
    };
    command("@1 profile list\n");
    assert(Serial.output.find("\"manufacturer\":\"stepperonline\"") != std::string::npos &&
        Serial.output.find("\"profile\":\"ess_rs\"") != std::string::npos);
    assert(!hardware.writes);
    command("@2 discover addresses 1 1 requests 1 results 1\n");
    assert(Serial.output.find("\"command\":\"discover\"") != std::string::npos);
    assert(scan().settings.first == 1 && scan().settings.last == 1 && scan().settings.requestLimit == 1 && scan().settings.resultLimit == 1);
    reply(0, words(1, {0x4EEA})); waitSettled();
    command("@3 discover inspect\n");
    assert(Serial.output.find("\"raw_model\":20202") != std::string::npos);
    assert(scan().findings[0].probe.rawModel == 0x4EEA && hardware.writes == 1);
    command("@4 discover finish\n"); assert(scan().released);
    command("@5 probe\n"); reply(1, words(1, {0x4EEA}));
    Probe::ResultView result;
    assert(host(app).result(app, app->latestOperationId, result) && !result.pending);
    assert(result.probe.rawModel == scan().findings[0].probe.rawModel && hardware.writes == 2);
}
void deferredStopFailsPromptlyWhenScanIsInterlocked() {
    fresh(); uint32_t probeId = 0;
    assert(host(app).startProbe(app, 1, 1, probeId) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA}));
    assert(app->knownTargets[0] & 2);
    const auto candidates = range(2, 3);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    waitTx(1); reviewedFrame(2, 1);
    MotorControlRS::ActionRequest stop; stop.kind = MotorControlRS::ActionKind::STOP;
    stop.stop.behavior = MotorControlRS::StopBehavior::CONFIGURED_DECELERATION;
    uint32_t stopId = 0;
    assert(host(app).startAction(app, 2, 1, stop, stopId) == Probe::Action::OK);
    const auto deadline = findRecord(*app, stopId)->deadlineUs;
    assert(axisReserved(*app, 1));
    waitSettled(); assert(scan().phase == ScanPhase::INTERLOCK && app->owner.needsRecovery());
    Probe::ResultView result;
    step(); // Deliver the scan's failed restoration to the deferred stop.
    assert(host(app).result(app, stopId, result));
    assert(!result.pending && result.actionContext);
    assert(result.actionContext->outcome == MotorControlRS::ActionOutcome::TRANSPORT_ERROR);
    assert(result.actionContext->execution == MotorControlRS::ActionExecution::NOT_TRANSMITTED);
    assert(result.actionContext->completion == MotorControlRS::ActionCompletion::NOT_OBSERVED);
    assert(result.actionContext->deadlineUs == deadline && hardware.time < deadline);
    assert(!result.actionContext->writeEvidence.txAccepted);
    assert(!axisReserved(*app, 1) && scan().phase == ScanPhase::INTERLOCK && scan().owned && hardware.writes == 2);
    assert(scan().outcome == ScanOutcome::PREEMPTED && !scan().restored && !app->recovery.id);
    pump(1000); assert(hardware.writes == 2); // No deferred FC06 write or scan replay.
}
void interlockedScanRejectsStopWithoutChangingItsResult() {
    fresh(); uint32_t probeId = 0;
    assert(host(app).startProbe(app, 1, 1, probeId) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA}));
    const auto candidates = range(2, 3);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    waitTx(1); waitSettled();
    assert(scan().phase == ScanPhase::INTERLOCK && scan().owned && app->owner.needsRecovery());
    const auto outcome = scan().outcome;
    const auto generation = app->bindingGeneration;
    const auto nextId = app->nextOperationId;
    const auto requests = scan().requests;
    MotorControlRS::ActionRequest stop; stop.kind = MotorControlRS::ActionKind::STOP;
    for (const auto behavior : {MotorControlRS::StopBehavior::CONFIGURED_DECELERATION,
                                MotorControlRS::StopBehavior::DIRECT}) {
        stop.stop.behavior = behavior;
        uint32_t stopId = 0xCAFE;
        assert(host(app).startAction(app, 2, 1, stop, stopId) == Probe::Action::RECOVERY_REQUIRED);
        assert(stopId == 0xCAFE && app->nextOperationId == nextId && !axisReserved(*app, 1));
        assert(scan().outcome == outcome && scan().requests == requests && !scan().cancelRequested);
        assert(app->bindingGeneration == generation && !app->recovery.id);
    }
    pump(1000); assert(hardware.writes == 2 && scan().phase == ScanPhase::INTERLOCK);
}
void failedScanRestorationEndsDeferredStopWithoutLaterTransmission() {
    fresh(); uint32_t probeId = 0;
    assert(host(app).startProbe(app, 1, 1, probeId) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA}));
    const auto original = app->serial.active;
    auto candidates = range(2, 3); candidates.tupleCount = 1; candidates.tuples[0].baud = 9600;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    waitTx(1); assert(app->serial.active.baud == 9600);
    MotorControlRS::ActionRequest stop; stop.kind = MotorControlRS::ActionKind::STOP;
    stop.stop.behavior = MotorControlRS::StopBehavior::DIRECT;
    uint32_t stopId = 0;
    assert(host(app).startAction(app, 1, 1, stop, stopId) == Probe::Action::OK);
    const auto deadline = findRecord(*app, stopId)->deadlineUs;
    hardware.configFailCalls = {hardware.configCalls + 1};
    reply(1, words(2, {0x4EEA})); waitSettled(); step();
    assert(scan().phase == ScanPhase::INTERLOCK && scan().outcome == ScanOutcome::RESTORE_FAILED);
    Probe::ResultView result;
    assert(host(app).result(app, stopId, result) && !result.pending && result.actionContext);
    assert(result.actionContext->outcome == MotorControlRS::ActionOutcome::TRANSPORT_ERROR);
    assert(result.actionContext->execution == MotorControlRS::ActionExecution::NOT_TRANSMITTED);
    assert(!result.actionContext->writeEvidence.txAccepted && !axisReserved(*app, 1));
    assert(result.actionContext->deadlineUs == deadline && hardware.time < deadline);
    assert(sameTuple(result.serialTuple, original) && !app->recovery.id && hardware.writes == 2);
    const auto observed = scan().findings[0].probe.provenance.latestUs;
    hardware.configFailCalls.clear();
    assert(control(Probe::DiscoveryCommandKind::RESTORE) == Probe::Action::OK);
    waitSettled(); pump(1000);
    assert(scan().restored && sameTuple(app->serial.active, original));
    assert(scan().findings[0].probe.provenance.latestUs == observed && hardware.writes == 2);
    assert(host(app).result(app, stopId, result) && !result.pending &&
        result.actionContext->execution == MotorControlRS::ActionExecution::NOT_TRANSMITTED);
}
void commissioningLeaseRejectsStopWithoutConsumingCapacity() {
    fresh(); uint64_t lease = 0;
    assert(app->owner.beginCommissioning(uart.sample(), lease));
    const auto nextId = app->nextOperationId;
    MotorControlRS::ActionRequest stop; stop.kind = MotorControlRS::ActionKind::STOP;
    for (const auto behavior : {MotorControlRS::StopBehavior::CONFIGURED_DECELERATION,
                                MotorControlRS::StopBehavior::DIRECT}) {
        stop.stop.behavior = behavior;
        uint32_t stopId = 0xCAFE;
        assert(host(app).startAction(app, 1, 1, stop, stopId) == Probe::Action::BUSY);
        assert(stopId == 0xCAFE && app->nextOperationId == nextId && !axisReserved(*app, 1));
        assert(app->owner.commissioningOwned() && !app->owner.pending() && !app->owner.active());
    }
    pump(1000); assert(!hardware.writes && !app->recovery.id);
    assert(app->owner.endCommissioning(lease, uart.sample()));
    uint32_t stopId = 0;
    assert(host(app).startAction(app, 2, 1, stop, stopId) == Probe::Action::OK);
    waitTx(0); assert(hardware.tx == crc({1, 6, 0, 0x27, 2, 0}));
    writeResponseConfirmed = true;
    reply(0, hardware.tx); reply(1, words(1, {0, 0}));
    Probe::ResultView result;
    assert(host(app).result(app, stopId, result) && !result.pending && result.actionContext);
    assert(result.actionContext->completion == MotorControlRS::ActionCompletion::OBSERVED);
    assert(hardware.writes == 2 && !app->owner.needsRecovery());
}
void ordinaryProbeCancellationSettlesBeforeNextReadAndStop(bool receiving) {
    fresh(); uint32_t probeId = 0;
    assert(host(app).startProbe(app, 1, 1, probeId) == Probe::Action::OK);
    waitTx(0); reviewedFrame(1, 1);
    if (receiving) {
        for (unsigned i = 0; i < 10000 && app->runner.phase() != Rtu::Phase::RECEIVE; ++i) step();
        assert(app->runner.phase() == Rtu::Phase::RECEIVE);
    } else assert(app->runner.transmitEnabled());
    const auto request = findRecord(*app, probeId)->requestId;
    const auto generation = app->bindingGeneration;
    assert(app->owner.txAccepted(request) == 8);
    assert(host(app).cancel(app, probeId) == Probe::Action::OK);
    assert(findRecord(*app, probeId)->cancelContinuation);
    assert(!app->owner.needsRecovery() && !uart.needsRecovery() && !app->recovery.id);
    Probe::ResultView result;
    assert(host(app).result(app, probeId, result) && result.pending);
    reply(0, words(1, {0x4EEA}));
    assert(host(app).result(app, probeId, result) && !result.pending);
    assert(result.probe.outcome == Rtu::Outcome::SUCCESS && result.probe.rawModel == 0x4EEA);
    assert(result.probe.transport.txAccepted == 8 && result.probe.transport.closureQualified);
    assert(result.probe.cancellation == Rtu::Cancellation::NONE && !result.probe.executionUnknown);
    const auto closure = result.probe.observedLatestUs;
    assert(!app->owner.needsRecovery() && !uart.needsRecovery() && !app->recovery.id);
    assert(app->bindingGeneration == generation);
    assert(host(app).cancel(app, probeId) == Probe::Action::ALREADY_TERMINAL);

    uint32_t readId = 0;
    assert(host(app).startTypedRead(app, 2, 1, ESS::ReadKind::VELOCITY_PARAMETERS, readId) == Probe::Action::OK);
    reply(1, words(1, {60, 100, 100}));
    assert(host(app).result(app, readId, result) && !result.pending && result.typedRead);
    assert(result.typedRead->outcome == MotorControlRS::ReadOutcome::SUCCESS);
    MotorControlRS::ActionRequest stop; stop.kind = MotorControlRS::ActionKind::STOP;
    stop.stop.behavior = MotorControlRS::StopBehavior::DIRECT;
    uint32_t stopId = 0;
    assert(host(app).startAction(app, 3, 1, stop, stopId) == Probe::Action::OK);
    waitTx(2); assert(hardware.tx == crc({1, 6, 0, 0x27, 2, 0}));
    writeResponseConfirmed = true;
    reply(2, hardware.tx); reply(3, words(1, {0, 0}));
    assert(host(app).result(app, stopId, result) && !result.pending && result.actionContext);
    assert(result.actionContext->execution == MotorControlRS::ActionExecution::ACKNOWLEDGED);
    assert(result.actionContext->completion == MotorControlRS::ActionCompletion::OBSERVED);
    assert(host(app).result(app, probeId, result) && result.probe.observedLatestUs == closure);
    assert(result.probe.rawModel == 0x4EEA && result.probe.outcome == Rtu::Outcome::SUCCESS);
    pump(1000);
    assert(hardware.writes == 4 && app->bindingGeneration == generation);
    assert(!app->owner.needsRecovery() && !uart.needsRecovery() && !app->recovery.id);
}
void fullOrdinaryResultsCannotConsumeScanOrStopStorage() {
    fresh();
    for (unsigned i = 0; i < REQUEST_CAPACITY; ++i) {
        uint32_t operation = 0;
        assert(host(app).startProbe(app, i + 1, 1, operation) == Probe::Action::OK);
        reply(i, words(1, {0x4EEA}));
    }
    // The drive group's move pool adds ordinary owner results. An independent
    // producer holds those, so every ordinary owner result is unread again.
    const unsigned filled = RESULT_SLOTS - URGENT_RECORDS;
    std::vector<Rtu::RequestId> held(filled - REQUEST_CAPACITY);
    uint8_t probeBytes[8]; const auto probeLength = ESS::buildProbe(1, probeBytes, sizeof(probeBytes));
    for (std::size_t i = 0; i < held.size(); ++i) {
        Rtu::BusRequest request;
        request.wire.bytes = probeBytes; request.wire.length = probeLength; request.wire.replyLength = 7;
        request.wire.responseTimeoutUs = app->serial.timing.responseTimeoutUs; request.wire.replyGapUs = app->serial.timing.replyGapUs;
        request.wire.deadlineUs = nowUs() + REQUEST_US;
        request.expected.address = request.expected.target = 1; request.expected.targetGeneration = app->bindingGeneration;
        request.expected.function = 3; request.expected.count = 1; request.validator = Rtu::essValidator();
        assert(app->owner.admit(request, nowUs(), held[i]) == Rtu::BusAdmission::ACCEPTED);
        reply(REQUEST_CAPACITY + static_cast<unsigned>(i), words(1, {0x4EEA}));
        assert(app->owner.result(held[i]));
    }
    assert(control(Probe::DiscoveryCommandKind::BEGIN) == Probe::Action::OK);
    pump(); assert(scan().owned && !scan().count && !scan().requests && hardware.writes == filled);
    MotorControlRS::ActionRequest stop; stop.kind = MotorControlRS::ActionKind::STOP;
    stop.stop.behavior = MotorControlRS::StopBehavior::DIRECT;
    uint32_t id = 0;
    assert(host(app).startAction(app, 99, 1, stop, id) == Probe::Action::OK);
    waitTx(filled); assert(hardware.tx[1] == 6);
    const auto echo = hardware.tx; writeResponseConfirmed = true;
    reply(filled, echo); reply(filled + 1, words(1, {0, 0}));
    Probe::ResultView result;
    assert(host(app).result(app, id, result) && !result.pending && result.actionContext);
    assert(result.actionContext->completion == MotorControlRS::ActionCompletion::OBSERVED);
    assert(scan().outcome == ScanOutcome::PREEMPTED && scan().restored && scan().count == 0);
    assert(hardware.writes == filled + 2);
    for (unsigned i = 1; i <= REQUEST_CAPACITY; ++i) {
        assert(host(app).result(app, i, result) && result.probe.outcome == Rtu::Outcome::SUCCESS);
    }
}
void identityRefinementFailuresRetainProbeAndStopFaultedScans() {
    // A checked exception is a settled responder result and permits the next
    // reviewed query, without publishing a successful identity.
    fresh(); auto candidates = range(1, 2); candidates.identity = true;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA}));
    const auto exception = crc({1, 0x83, 2});
    reply(1, exception);
    assert(!app->owner.needsRecovery());
    reply(2, crc({2, 0x83, 2})); waitSettled();
    assert(scan().outcome == ScanOutcome::COMPLETE && scan().restored && scan().requests == 3 && scan().count == 2);
    const auto& rejected = scan().findings[0];
    assert(rejected.probe.outcome == ProbeOutcome::RESPONDER && rejected.probe.rawModel == 0x4EEA);
    assert(rejected.identityAttempted && !rejected.identityKnown && !rejected.identityAmbiguous);
    assert(rejected.identityEvidence.status.code == MotorControlRS::Err::EXCEPTION && rejected.identityEvidence.status.detail == 2);
    assert(rejected.identityEvidence.length == exception.size() &&
        !std::memcmp(rejected.identityEvidence.raw, exception.data(), exception.size()));
    assert(!rejected.identity.operationId && !app->identity.operationId);

    // CRC failure, wrong endpoint, malformed length and late traffic must each
    // preserve the first probe and stop before another address is queried.
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        fresh(); assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
        reply(0, words(1, {0x4EEA})); waitTx(1); reviewedFrame(1, 4);
        if (scenario == 0) {
            auto broken = words(1, {0x4EEA, 0x29, 1, 0}); broken.back() ^= 1; reply(1, broken);
        } else if (scenario == 1) {
            reply(1, words(2, {0x4EEA, 0x29, 2, 0}));
        } else if (scenario == 2) {
            reply(1, words(1, {0x4EEA, 0x29, 1}));
        } else {
            scheduleReply(hardware.writeStarted + 8 * hardware.txCharacterUs +
                app->serial.timing.responseTimeoutUs + 1000, words(1, {0x4EEA, 0x29, 1, 0}), hardware.txCharacterUs);
        }
        waitSettled();
        assert(scan().outcome == ScanOutcome::TRANSPORT_FAULT && scan().phase == ScanPhase::INTERLOCK);
        assert(scan().owned && !scan().restored && scan().count == 1 && scan().requests == 2);
        const auto& finding = scan().findings[0];
        assert(finding.probe.outcome == ProbeOutcome::RESPONDER && finding.probe.rawModel == 0x4EEA);
        assert(finding.identityAttempted && !finding.identityKnown && !finding.identityAmbiguous && !finding.identity.operationId);
        assert(finding.identityEvidence.first == 0 && finding.identityEvidence.count == 4 && !finding.identityEvidence.status);
        if (scenario == 0) assert(finding.identityEvidence.status.code == MotorControlRS::Err::CRC_ERROR &&
            finding.identityEvidence.frameError == ESS::FrameError::CRC);
        if (scenario == 1) assert(finding.identityEvidence.status.code == MotorControlRS::Err::FRAME_ERROR &&
            finding.identityEvidence.frameError == ESS::FrameError::ADDRESS);
        if (scenario == 2) assert(finding.identityEvidence.transportDetail == static_cast<int32_t>(Rtu::Reason::LENGTH));
        if (scenario == 3) assert(finding.identityEvidence.transportDetail == static_cast<int32_t>(Rtu::Reason::NO_RESPONSE));
        assert(app->owner.needsRecovery());
        const unsigned configurations = hardware.configCalls;
        pump(1000); assert(hardware.writes == 2 && hardware.configCalls == configurations && scan().count == 1);
    }
}
void cancellationDuringIdentitySettlesQueuedAndInFlightWork() {
    // Harvest the probe without another owner service so identity starts queued.
    // Then cancel in the queue, pre-TX setup, TX and receive phases separately.
    for (unsigned phase = 0; phase < 4; ++phase) {
        fresh(); auto candidates = range(1, 2); candidates.identity = true;
        assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
        waitTx(0);
        scheduleReply(std::max(hardware.writeStarted + 8 * 87 + 1000, hardware.time + 1000), words(1, {0x4EEA}));
        const auto probe = app->discoveryRequest;
        for (unsigned i = 0; i < 25000 && !app->owner.result(probe); ++i) {
            advanceHardware(hardware.time + 10); app->owner.service(uart.sample());
        }
        assert(app->owner.result(probe));
        serviceDiscovery(*app, uart.sample());
        assert(scan().phase == ScanPhase::IDENTITY && scan().count == 1 && scan().requests == 2 && hardware.writes == 1);
        if (phase == 1) {
            for (unsigned i = 0; i < 10000 && app->runner.phase() != Rtu::Phase::SETUP; ++i) step();
            assert(app->runner.phase() == Rtu::Phase::SETUP && hardware.writes == 1);
        } else if (phase >= 2) {
            waitTx(1); reviewedFrame(1, 4);
            if (phase == 3) {
                for (unsigned i = 0; i < 10000 && app->runner.phase() != Rtu::Phase::RECEIVE; ++i) step();
                assert(app->runner.phase() == Rtu::Phase::RECEIVE);
            } else assert(app->runner.transmitEnabled());
        }
        assert(control(Probe::DiscoveryCommandKind::CANCEL) == Probe::Action::OK);
        if (phase >= 2) reply(1, words(1, {0x4EEA, 0x29, 1, 0}));
        waitSettled();
        assert(scan().outcome == ScanOutcome::CANCELLED && scan().restored && scan().count == 1 && scan().requests == 2);
        const auto& finding = scan().findings[0];
        assert(finding.probe.outcome == ProbeOutcome::RESPONDER && finding.identityAttempted && !finding.identityAmbiguous);
        if (phase < 2) {
            assert(!finding.identityKnown && finding.identityEvidence.event == ReadEventKind::CANCEL);
            assert(!finding.identityEvidence.txAccepted && !finding.identityEvidence.receivedLength);
        } else {
            assert(finding.identityKnown && finding.identity.rawModel == 0x4EEA && finding.identityEvidence.txAccepted == 8);
        }
        assert(!app->owner.needsRecovery() && !app->runner.transmitEnabled());
        pump(1000); assert(hardware.writes == (phase < 2 ? 1U : 2U));
    }
}
void explicitRecoveryCannotResumeUnharvestedScan() {
    // The owner may finish recovery before the cooperative scan next runs.
    // Recovery still cancels future scan work, independently of that timing.
    for (unsigned scenario = 0; scenario < 3; ++scenario) {
        fresh(); auto candidates = range(1, 2); candidates.identity = true;
        assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
        if (scenario) {
            waitTx(0);
            if (scenario == 1) {
                scheduleReply(std::max(hardware.writeStarted + 8 * 87 + 1000, hardware.time + 1000), words(1, {0x4EEA}));
                for (unsigned i = 0; i < 25000 && !app->owner.result(app->discoveryRequest); ++i) {
                    advanceHardware(hardware.time + 10); app->owner.service(uart.sample());
                }
                assert(app->owner.result(app->discoveryRequest) && !scan().count);
            }
        }
        const unsigned before = hardware.writes;
        uint32_t recoveryId = 0;
        assert(host(app).recover(app, 99, recoveryId) == Probe::Action::OK);
        // Deliberately service the owner without scan delivery until recovery
        // has settled; normal loop delay can create the same ordering.
        for (unsigned i = 0; i < 10000 && app->owner.recovering(); ++i) {
            advanceHardware(hardware.time + 1000);
            if (!app->runner.busy() && !app->runner.transmitEnabled() && hardware.time >= app->recoveryGuardUntilUs) {
                if (!app->recovery.prepared) app->recovery.prepared = uart.clear();
            }
            app->owner.service(uart.sample(), app->recovery.prepared);
        }
        assert(!app->owner.recovering() && !app->owner.needsRecovery() && !uart.needsRecovery());
        serviceDiscovery(*app, uart.sample());
        assert(scan().requests == (scenario ? 1 : 0));
        waitSettled();
        assert(scan().outcome == ScanOutcome::CANCELLED && scan().restored);
        assert(scan().requests == (scenario ? 1 : 0));
        assert(scan().count == (scenario ? 1 : 0));
        if (scenario == 1) assert(scan().findings[0].probe.outcome == ProbeOutcome::RESPONDER);
        assert(!scan().findings[0].identityAttempted && hardware.writes == before);
        pump(1000); assert(hardware.writes == before);
    }
}
void urgentStopRestoresOriginalTupleBeforeDispatch() {
    fresh(); const auto original = app->serial.active;
    const auto originalActualBaud = uart.stats().actualBaud;
    uint32_t probeId = 0;
    assert(host(app).startProbe(app, 1, 1, probeId) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA}));
    assert(app->knownTargets[0] & 2);
    const auto originalGeneration = app->serial.generation;
    auto candidates = range(2, 3); candidates.tupleCount = 1;
    candidates.tuples[0].baud = 9600;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    waitTx(1); reviewedFrame(2, 1);
    assert(app->serial.active.baud == 9600 && !(app->knownTargets[0] & 2));
    MotorControlRS::ActionRequest stop; stop.kind = MotorControlRS::ActionKind::STOP;
    stop.stop.behavior = MotorControlRS::StopBehavior::CONFIGURED_DECELERATION;
    uint32_t stopId = 0;
    assert(host(app).startAction(app, 2, 1, stop, stopId) == Probe::Action::OK);
    auto* record = findRecord(*app, stopId); assert(record && record->discoveryStop);
    const auto deadline = record->deadlineUs;
    assert(sameTuple(record->serialTuple, original) && record->serialGeneration == originalGeneration);
    assert(scan().outcome == ScanOutcome::PREEMPTED && hardware.writes == 2);
    reply(1, words(2, {0x4EEA}));
    waitTx(2);
    assert(scan().restored && !scan().owned && sameTuple(app->serial.active, original));
    assert(hardware.tx[0] == 1 && hardware.tx[1] == 6 &&
        static_cast<uint32_t>(hardware.config.baud_rate) == original.baud && uart.stats().actualBaud == originalActualBaud);
    assert(record->deadlineUs == deadline && sameTuple(record->serialTuple, original));
    assert(record->serialGeneration == app->serial.generation && record->serialGeneration > originalGeneration);
    const auto echo = hardware.tx; writeResponseConfirmed = true;
    reply(2, echo); reply(3, words(1, {0, 0}));
    Probe::ResultView result;
    assert(host(app).result(app, stopId, result) && !result.pending && result.actionContext);
    assert(result.actionContext->execution == MotorControlRS::ActionExecution::ACKNOWLEDGED);
    assert(result.actionContext->completion == MotorControlRS::ActionCompletion::OBSERVED);
    assert(sameTuple(result.serialTuple, original) && result.serialGeneration == app->serial.generation);
    assert(scan().count == 1 && scan().requests == 1 && scan().findings[0].request.target.address == 2);
    assert(scan().findings[0].request.activeSerial.baud == 9600 && scan().findings[0].probe.rawModel == 0x4EEA);
    assert(scan().outcome == ScanOutcome::PREEMPTED && !axisReserved(*app, 1) && hardware.writes == 4);
    pump(1000); assert(hardware.writes == 4 && !app->owner.needsRecovery());
}
}
// Wired bench 19: discovery past a silent address. One tuple, each address
// once; a silent probe gets one bounded settle counted as a request; any late
// byte ends the scan; silence ends needing recover, restore and finish.
Probe::DiscoverySettings pastSilent(uint8_t first, uint8_t last) {
    auto value = range(first, last); value.pastSilent = true; return value;
}
bool silentAt(uint8_t address) { return (scan().silent[address / 8] & (1U << (address % 8))) != 0; }
void waitScanTx(unsigned before) { // TX only; the reply and the next request follow the scan.
    for (unsigned i = 0; i < 200000 && hardware.writes == before; ++i) step();
    assert(hardware.writes == before + 1);
}
void waitPhase(ScanPhase phase) {
    for (unsigned i = 0; i < 200000 && scan().phase != phase; ++i) step();
    assert(scan().phase == phase);
}
// Explicit operator boundary: recover, then restore, then finish.
void crossBoundary() {
    assert(scan().phase == ScanPhase::INTERLOCK && scan().boundaryRequired && !scan().boundaryCrossed);
    assert(control(Probe::DiscoveryCommandKind::RESTORE) == Probe::Action::RECOVERY_REQUIRED);
    assert(scan().phase == ScanPhase::INTERLOCK && !scan().restored);
    Serial.output.clear(); Serial.input = "@99 recover" + std::string(1, char(10)); // Delivered like an operator's.
    for (unsigned i = 0; i < 1000 && !Serial.input.empty(); ++i) step();
    assert(Serial.input.empty() && app->recovery.operationId);
    for (unsigned i = 0; i < 10000 && app->owner.recovering(); ++i) step(1000);
    pump();
    assert(!app->owner.needsRecovery() && scan().boundaryCrossed && app->recovery.delivered);
    assert(control(Probe::DiscoveryCommandKind::RESTORE) == Probe::Action::OK);
    waitSettled(); assert(scan().phase == ScanPhase::TERMINAL && scan().restored);
    assert(control(Probe::DiscoveryCommandKind::FINISH) == Probe::Action::OK);
}
void pastSilentContinuesAfterSilence() {
    fresh(); const auto candidates = pastSilent(1, 3);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA}));
    waitScanTx(1); reviewedFrame(2, 1);            // Address 2 never answers.
    reply(2, words(3, {0x4EEA})); reviewedFrame(3, 1);
    waitSettled();
    assert(scan().outcome == ScanOutcome::COMPLETE && scan().count == 2 && scan().requests == 4);
    assert(scan().settles == 1 && scan().settle == Probe::DiscoverySettle::RECOVERED && !scan().lateBytes);
    assert(scan().silentCount == 1 && silentAt(2) && !silentAt(1) && !silentAt(3));
    assert(scan().findings[0].request.target.address == 1 && scan().findings[1].request.target.address == 3);
    assert(scan().findings[0].probe.outcome == ProbeOutcome::RESPONDER && scan().findings[1].probe.outcome == ProbeOutcome::RESPONDER);
    // The runner is healthy, but silence needs the explicit operator boundary.
    assert(!app->owner.needsRecovery() && hardware.writes == 3);
    uint32_t unchanged = 123;
    assert(host(app).startProbe(app, 4, 2, unchanged) == Probe::Action::BUSY && unchanged == 123);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::BUSY); // No reuse of 2.
    pump(1000); assert(hardware.writes == 3);
    crossBoundary();
    assert(host(app).startProbe(app, 5, 2, unchanged) == Probe::Action::OK && hardware.writes == 3);
    waitScanTx(3); reviewedFrame(2, 1);
}
void pastSilentAtLastAddressAndSilenceOnly() {
    fresh(); auto candidates = pastSilent(1, 2);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA}));
    waitScanTx(1); reviewedFrame(2, 1); waitSettled();
    assert(scan().outcome == ScanOutcome::COMPLETE && scan().count == 1 && scan().requests == 2 && !scan().settles);
    assert(scan().silentCount == 1 && silentAt(2) && app->owner.needsRecovery());
    crossBoundary(); assert(hardware.writes == 2);
    // A later past-silent scan releases the delivered recovery result and runs.
    candidates = pastSilent(3, 4);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK && !app->recovery.operationId);
    waitScanTx(2); reviewedFrame(3, 1);
    waitScanTx(3); reviewedFrame(4, 1); waitSettled();
    assert(scan().outcome == ScanOutcome::COMPLETE && !scan().count && scan().silentCount == 2 && scan().settles == 1);
    assert(scan().requests == 3 && silentAt(3) && silentAt(4));
}
void pastSilentNeverContinuesAfterBytesOrFaults() {
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        fresh(); const auto candidates = pastSilent(1, 3);
        assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
        reply(0, words(1, {0x4EEA}));
        if (scenario == 0) reply(1, {2, 3});                    // Partial bytes stay terminal.
        else if (scenario == 1) { auto broken = words(2, {60}); broken.back() ^= 1; reply(1, broken); }
        else if (scenario == 2) reply(1, words(4, {60}));        // Another address answers: foreign.
        else {                                                  // A frame that closes after its deadline.
            waitTx(1);
            scheduleReply(hardware.writeStarted + 8 * hardware.txCharacterUs + app->serial.timing.responseTimeoutUs - 300,
                words(2, {60}), hardware.txCharacterUs);
        }
        waitSettled();
        assert(scan().outcome == ScanOutcome::TRANSPORT_FAULT && scan().phase == ScanPhase::INTERLOCK);
        assert(scan().count == 2 && !scan().silentCount && !scan().settles && hardware.writes == 2);
        assert(scan().findings[1].probe.outcome != ProbeOutcome::RESPONDER && app->owner.needsRecovery());
    }
}
void pastSilentLateFrameDuringSettleEndsScan() {
    fresh(); const auto candidates = pastSilent(1, 3);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA}));
    waitTx(1);
    // Starts after the response timeout (NO_RESPONSE), lands in the settle's drain.
    scheduleReply(hardware.writeStarted + 8 * hardware.txCharacterUs + app->serial.timing.responseTimeoutUs + 400,
        words(2, {60}), hardware.txCharacterUs);
    waitSettled();
    assert(scan().outcome == ScanOutcome::TRANSPORT_FAULT && scan().phase == ScanPhase::INTERLOCK);
    assert(scan().settles == 1 && scan().settle == Probe::DiscoverySettle::LATE_BYTES && scan().lateBytes == 7);
    assert(scan().silentCount == 1 && silentAt(2) && scan().count == 1 && hardware.writes == 2); // 3 never probed.
    assert(!app->owner.needsRecovery()); // The settle recovered, the boundary still holds.
    crossBoundary(); assert(hardware.writes == 2);
}
void pastSilentDelayedFrameAtNextAddressIsForeign() {
    for (bool beforeTx : {false, true}) {
        fresh(); const auto candidates = pastSilent(2, 4);
        assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
        waitScanTx(0); reviewedFrame(2, 1);               // Silent.
        if (beforeTx) {
            // The delayed reply from 2 arrives while the probe of 3 waits for bus idle.
            for (unsigned i = 0; i < 200000 && app->runner.phase() != Rtu::Phase::WAIT_BUS; ++i) step();
            assert(app->runner.phase() == Rtu::Phase::WAIT_BUS && hardware.writes == 1 && scan().settles == 1);
            scheduleReply(hardware.time + 10, words(2, {0x4EEA}), hardware.txCharacterUs);
            waitScanTx(1); reviewedFrame(3, 1);
            reply(1, words(3, {0x4EEA}));
        } else {
            reply(1, words(2, {0x4EEA}));                 // Delayed reply from 2 during the probe of 3.
            reviewedFrame(3, 1);
        }
        waitSettled();
        assert(scan().outcome == ScanOutcome::TRANSPORT_FAULT && scan().phase == ScanPhase::INTERLOCK);
        assert(scan().silentCount == 1 && silentAt(2) && scan().count == 1 && hardware.writes == 2);
        assert(scan().findings[0].request.target.address == 3);
        if (beforeTx) { // 3's own checked reply is kept; the discarded bytes end the scan.
            assert(scan().settle == Probe::DiscoverySettle::LATE_BYTES && scan().lateBytes == 7);
            assert(scan().findings[0].probe.outcome == ProbeOutcome::RESPONDER);
        } else {         // The delayed frame from 2 is foreign, never a responder at 3.
            assert(scan().findings[0].probe.outcome == ProbeOutcome::MISMATCH && app->owner.needsRecovery());
        }
    }
}
void pastSilentRefusesReuseBeforeTx() {
    fresh(); const unsigned configurations = hardware.configCalls;
    auto candidates = pastSilent(1, 2); candidates.tupleCount = 2;
    candidates.tuples[0].baud = 9600; candidates.tuples[1].baud = 115200;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::INVALID);
    assert(!hardware.writes && hardware.configCalls == configurations && !scan().operationId);
    candidates.tupleCount = 0; // The default single tuple is admitted.
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
}
void pastSilentBudgetsCountSettles() {
    fresh(); auto candidates = pastSilent(1, 3); candidates.requestLimit = 2;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA}));
    waitScanTx(1); waitSettled();                         // No request left for a settle.
    assert(scan().outcome == ScanOutcome::REQUEST_LIMIT && scan().requests == 2 && !scan().settles);
    assert(scan().silentCount == 1 && app->owner.needsRecovery() && scan().phase == ScanPhase::INTERLOCK);

    fresh(); candidates.requestLimit = 3;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    reply(0, words(1, {0x4EEA}));
    waitScanTx(1); waitSettled();                         // The settle used the third request.
    assert(scan().outcome == ScanOutcome::REQUEST_LIMIT && scan().requests == 3 && scan().settles == 1);
    assert(hardware.writes == 2 && scan().phase == ScanPhase::INTERLOCK && !app->owner.needsRecovery());

}
void pastSilentCancellationDuringSettle() {
    fresh(); const auto candidates = pastSilent(1, 3);
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    waitScanTx(0); waitPhase(ScanPhase::SETTLE);
    for (unsigned i = 0; i < 200000 && !scan().settleId; ++i) step();
    assert(scan().settleId && app->owner.recovering());
    assert(control(Probe::DiscoveryCommandKind::CANCEL) == Probe::Action::OK);
    assert(app->owner.recovering()); // The settle is not cut short.
    waitSettled();
    assert(scan().outcome == ScanOutcome::CANCELLED && scan().settle == Probe::DiscoverySettle::RECOVERED);
    assert(scan().settles == 1 && scan().silentCount == 1 && hardware.writes == 1 && scan().phase == ScanPhase::INTERLOCK);
    crossBoundary(); assert(hardware.writes == 1);
}
void pastSilentRestoresTupleAfterBoundary() {
    fresh(); auto candidates = pastSilent(1, 2); candidates.tupleCount = 1; candidates.tuples[0].baud = 9600;
    assert(control(Probe::DiscoveryCommandKind::BEGIN, &candidates) == Probe::Action::OK);
    waitScanTx(0); assert(app->serial.active.baud == 9600);
    reply(1, words(2, {0x4EEA})); waitSettled();
    assert(scan().outcome == ScanOutcome::COMPLETE && scan().count == 1 && silentAt(1) && scan().settles == 1);
    assert(scan().phase == ScanPhase::INTERLOCK && app->serial.active.baud == 9600);
    crossBoundary(); assert(app->serial.active.baud == 115200 && hardware.writes == 2);
}
void pastSilentConsoleParity() {
    fresh();
    const auto command = [](const std::string& text) {
        Serial.output.clear(); Serial.input = text;
        for (unsigned i = 0; i < 1000 && !Serial.input.empty(); ++i) step();
        assert(Serial.input.empty()); pump(1000);
    };
    command("@1 discover addresses 1 2 past-silent tuple 9600 8N1 tuple 19200 8N1\n");
    assert(Serial.output.find("past_silent_one_tuple") != std::string::npos && !hardware.writes);
    command("@2 discover addresses 1 2 past-silent past-silent\n");
    assert(Serial.output.find("duplicate_option") != std::string::npos && !hardware.writes);
    command("@3 discover addresses 1 2 past-silent\n");
    assert(scan().settings.pastSilent && Serial.output.find("\"past_silent\":true") != std::string::npos);
    waitScanTx(0); reply(1, words(2, {0x4EEA})); waitSettled();
    command("@4 discover inspect\n");
    assert(Serial.output.find("\"silent\":[1],\"silent_count\":1,\"settles\":1,\"settle\":\"recovered\",\"late_bytes\":0,"
        "\"boundary_required\":true,\"boundary_crossed\":false") != std::string::npos);
}
int main() {
    minimalCurrentTupleAndRetainedEvidence();
    rejectedCandidatesAndExclusiveAdmission(); finiteBudgetsAndCheckedExceptions();
    identityRefinementAndBudget();
    identityRefinementFailuresRetainProbeAndStopFaultedScans();
    cancellationDuringIdentitySettlesQueuedAndInFlightWork();
    partialFindingsStopOnTransportFault(); cancellationSettlesCurrentFrame();
    exactOriginalTupleRestoration(); restorationFailureAndExplicitRepair();
    timeoutOnCandidateNeedsExplicitRecovery(); overallDeadlineIncludesAdapterSetup();
    capturedClosureSurvivesLateCancellationDelivery(); urgentStopUsesReservedCapacityAndDoesNotReplayScan();
    consoleAndDirectProductionParity();
    deferredStopFailsPromptlyWhenScanIsInterlocked();
    interlockedScanRejectsStopWithoutChangingItsResult();
    failedScanRestorationEndsDeferredStopWithoutLaterTransmission();
    commissioningLeaseRejectsStopWithoutConsumingCapacity();
    ordinaryProbeCancellationSettlesBeforeNextReadAndStop(false);
    ordinaryProbeCancellationSettlesBeforeNextReadAndStop(true);
    fullOrdinaryResultsCannotConsumeScanOrStopStorage();
    explicitRecoveryCannotResumeUnharvestedScan();
    urgentStopRestoresOriginalTupleBeforeDispatch();
    pastSilentContinuesAfterSilence(); pastSilentAtLastAddressAndSilenceOnly();
    pastSilentNeverContinuesAfterBytesOrFaults(); pastSilentLateFrameDuringSettleEndsScan();
    pastSilentDelayedFrameAtNextAddressIsForeign(); pastSilentRefusesReuseBeforeTx();
    pastSilentBudgetsCountSettles(); pastSilentCancellationDuringSettle();
    pastSilentRestoresTupleAfterBoundary(); pastSilentConsoleParity();
    if (app) { app->~App(); std::free(app); app = nullptr; }
    std::puts("Discovery application tests passed");
}
