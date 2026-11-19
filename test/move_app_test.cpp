// SPDX-License-Identifier: MIT
// Exercise the actual cooperative application with simulated SDK/wire evidence.
// Qualification below belongs solely to this fake fixture, never the real bench.
// Both platform variants run these same scenarios and assertions.
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
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

namespace {
using namespace MotorControlRS;
void fresh(uint32_t maximumAgeMs = 0, const UnitConfig& units = ApplicationOptions().positionUnits) {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = false; writeResponseConfirmed = false;
    ApplicationOptions options; options.observationMaxAgeMs = maximumAgeMs;
    options.positionUnits = units;
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit, options));
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
    assert(app->owner.txAccepted(findRecord(*app, operation)->requestId));
}
std::vector<uint8_t> acknowledgement() {
    assert(hardware.tx.size() >= 8);
    if (hardware.tx[1] == 6) return hardware.tx;
    assert(hardware.tx[1] == 16);
    return crc(std::vector<uint8_t>(hardware.tx.begin(), hardware.tx.begin() + 6));
}
void moveStep(uint32_t operation, const std::vector<uint8_t>& supplied = {}) {
    const uint8_t token = view(operation).moveContext->step;
    waitTx(operation);
    const auto response = supplied.empty() ? acknowledgement() : supplied;
    scheduleReply(std::max(hardware.writeStarted + hardware.tx.size() * 87 + 1000, hardware.time + 1000), response);
    for (unsigned i = 0; i < 25000 && view(operation).pending && view(operation).moveContext->step == token; ++i) step();
    assert(!view(operation).pending || view(operation).moveContext->step != token);
}
void actionStep(uint32_t operation, const std::vector<uint8_t>& supplied = {}) {
    const uint8_t token = view(operation).actionContext->step;
    waitTx(operation);
    const auto response = supplied.empty() ? acknowledgement() : supplied;
    scheduleReply(std::max(hardware.writeStarted + 8 * 87 + 1000, hardware.time + 1000), response);
    for (unsigned i = 0; i < 25000 && view(operation).pending && view(operation).actionContext->step == token; ++i) step();
    assert(!view(operation).pending || view(operation).actionContext->step != token);
}
void finishProbe(uint32_t operation, uint8_t address) {
    waitTx(operation); scheduleReply(hardware.time + 2000, registers(address, {0x4EEA}));
    for (unsigned i = 0; i < 25000 && view(operation).pending; ++i) step();
    assert(!view(operation).pending && view(operation).probe.outcome == Rtu::Outcome::SUCCESS);
}
MoveRequest request() {
    MoveRequest r; r.position.value = Rational(20); r.position.configurationGeneration = app->axis.generation;
    r.speedRpm = 60; r.ramp = MoveRamp::VERIFIED_CONFIGURED; return r;
}
void qualify() {
    // Deliberately simulated qualifications. Production has no corresponding
    // console control and starts with every verification flag false.
    writeResponseConfirmed = true;
    app->knownTargets[0] |= 2;
    app->axis.nativeMinimum = -1000; app->axis.nativeMaximum = 1000;
    app->axis.supportedRelativeBases = 1;
    auto& p = app->movePrerequisites;
    p.subdivision = 1000; p.target = app->axis.target; p.configurationGeneration = app->axis.generation;
    p.commandUnitsVerified = p.relativeBasisVerified = p.negativeTwosComplementVerified = true;
    p.configuredRampVerified = p.serialInputsPermit = p.readinessQualified = true;
    p.accelerationTime = p.decelerationTime = 100;
    p.wordOrderKnown = p.startSpeedKnown = true; p.startSpeed = 10;
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
}
uint32_t admitMove(uint32_t commandId = 1) {
    uint32_t id = 0;
    assert(host(app).startMove(app, commandId, 1, request(), id) == Probe::Action::OK);
    assert(id && view(id).moveContext && axisReserved(*app, 1)); return id;
}
uint32_t admitStop(uint32_t commandId = 50) {
    ActionRequest stop; stop.kind = ActionKind::STOP; stop.stop.behavior = StopBehavior::CONFIGURED_DECELERATION;
    uint32_t id = 0;
    assert(host(app).startAction(app, commandId, 1, stop, id) == Probe::Action::OK); return id;
}
Rtu::BusRequest configurationWrite(uint8_t* bytes, std::size_t capacity) {
    Rtu::BusRequest r;
    r.expected.address = 1; r.expected.target = 1; r.expected.targetGeneration = app->bindingGeneration;
    r.expected.function = 6; r.expected.first = 0x10; r.expected.count = r.expected.value = 1;
    r.wire.length = ESS::buildWriteSingleRegister(1, 0x10, 1, bytes, capacity);
    r.wire.bytes = bytes; r.wire.replyLength = 8; r.wire.responseTimeoutUs = app->serial.timing.responseTimeoutUs;
    r.wire.replyGapUs = app->serial.timing.replyGapUs; r.wire.deadlineUs = nowUs() + REQUEST_US;
    r.validator = Rtu::essValidator(); return r;
}
void testProductionGateAndImmutableAdmission() {
    fresh(); uint32_t unchanged = 77;
    assert(host(app).startMove(app, 1, 1, request(), unchanged) != Probe::Action::OK);
    assert(unchanged == 77 && hardware.writes == 0 && !app->owner.pending());
    command("@1 move relative 20 steps native 60 configured\n");
    assert(hardware.writes == 0 && !app->owner.pending());
    qualify(); auto r = request(); r.speedRpm = 3001;
    assert(host(app).startMove(app, 2, 1, r, unchanged) == Probe::Action::INVALID);
    assert(unchanged == 77 && hardware.writes == 0 && !axisReserved(*app, 1));
    r = request(); ++app->axis.generation; // Caller intent must not be rebound silently.
    app->movePrerequisites.configurationGeneration = app->axis.generation;
    assert(host(app).startMove(app, 3, 1, r, unchanged) == Probe::Action::INVALID);
    assert(unchanged == 77 && hardware.writes == 0 && !axisReserved(*app, 1));
}
void testFailedPreparationAndCrossKindSlotReuse() {
    fresh(); qualify();
    auto& slot = app->records[0];
    const uint32_t next = app->nextOperationId;
    uint32_t unchanged = 77;
    assert(typedRead(app, 1, 1, static_cast<ESS::ReadKind>(255), unchanged) == Probe::Action::INVALID);
    auto invalid = request(); invalid.speedRpm = 3001;
    assert(startMove(app, 2, 1, invalid, unchanged) == Probe::Action::INVALID);
    assert(unchanged == 77 && app->nextOperationId == next && !slot.operationId);
    assert(slot.kind == App::OperationKind::EMPTY && !slot.requestId.owner);
    assert(!app->owner.pending() && !axisReserved(*app, 1) && !hardware.writes);

    Probe::ResultView reused;
    const auto trackedAdmission = [&](const char* text) {
        const auto nextId = app->nextOperationId;
        // Feed the production console without servicing the owner: it tracks
        // terminal delivery while the admitted transaction remains unsent.
        for (const char* at = text; *at; ++at) app->console.feed(*at);
        assert(app->latestOperationId == nextId && slot.operationId == nextId);
        return nextId;
    };
    const auto settleAndRelease = [&](uint32_t operation) {
        assert(findRecord(*app, operation) == &slot);
        assert(lookup(app, operation, reused) && reused.pending);
        assert(cancel(app, operation) == Probe::Action::OK);
        pump(1000);
        assert(lookup(app, operation, reused) && !reused.pending);
        assert(release(app, operation) == Probe::Action::OK);
        assert(!slot.operationId && slot.kind == App::OperationKind::EMPTY);
        const auto retainedId = reused.operationId;
        assert(!lookup(app, operation, reused) && reused.operationId == retainedId);
        assert(!app->owner.pending() && !axisReserved(*app, 1) && !hardware.writes);
    };

    const uint32_t moving = trackedAdmission("@3 move relative 20 steps native 60 configured\n");
    assert(slot.kind == App::OperationKind::MOVE);
    settleAndRelease(moving);
    const auto enabling = trackedAdmission("@4 enable\n");
    assert(enabling == moving + 1 && slot.kind == App::OperationKind::ACTION);
    assert(lookup(app, enabling, reused) && reused.actionContext && !reused.moveContext);
    settleAndRelease(enabling);
    const auto reading = trackedAdmission("@5 read identity 1\n");
    assert(reading == enabling + 1 && slot.kind == App::OperationKind::TYPED_READ);
    assert(lookup(app, reading, reused) && reused.typedRead && !reused.moveContext && !reused.actionContext);
    settleAndRelease(reading);
    const auto driver = trackedAdmission("@6 driver read 1\n");
    assert(driver == reading + 1 && slot.kind == App::OperationKind::DRIVER);
    assert(lookup(app, driver, reused) && reused.driverContext && !reused.typedRead);
    settleAndRelease(driver);
    const auto capture = trackedAdmission("@7 capture-read 1\n");
    assert(capture == driver + 1 && slot.kind == App::OperationKind::TRANSACTION && slot.captureRead);
    assert(lookup(app, capture, reused) && reused.captureRead && !reused.driverContext && !reused.typedRead);
    settleAndRelease(capture);
    const auto probing = trackedAdmission("@8 probe 1\n");
    assert(probing == capture + 1 && slot.kind == App::OperationKind::TRANSACTION && !slot.captureRead);
    finishProbe(probing, 1); pump(1000);
    assert(release(app, probing) == Probe::Action::OK && slot.kind == App::OperationKind::EMPTY);
    assert(hardware.writes == 1 && !axisReserved(*app, 1));
}
void testIndependentWritesRespectQualificationAndMoveStartup() {
    fresh(); uint8_t bytes[8] = {};
    const auto unqualified = configurationWrite(bytes, sizeof(bytes));
    assert(checkAxisWrite(*app, unqualified) == Probe::Action::UNAVAILABLE);
    qualify(); const auto configured = configurationWrite(bytes, sizeof(bytes));
    const uint32_t generation = app->axis.generation;
    auto stale = configured; --stale.expected.targetGeneration;
    assert(checkAxisWrite(*app, stale) == Probe::Action::INVALID);
    stale = configured; stale.expected.target = 2;
    assert(checkAxisWrite(*app, stale) == Probe::Action::INVALID);
    assert(checkAxisWrite(*app, configured) == Probe::Action::OK);
    // Policy checks alone neither admit a write nor invalidate observations.
    assert(hardware.writes == 0 && !app->owner.pending());
    assert(app->axis.generation == generation && app->configuration.operationId);
}

void testStageReservationAndFreshCompletion() {
    fresh(); qualify(); const uint32_t operation = admitMove();
    moveStep(operation);
    assert(view(operation).moveContext->stagingApplied && view(operation).moveContext->step == 1);
    assert(hardware.tx.size() == 19 && hardware.tx[3] == 0x21 && hardware.tx[5] == 5);
    const unsigned writes = hardware.writes;
    const auto pending = app->owner.pending();
    for (uint16_t reg : {uint16_t(0x24), uint16_t(0x23), uint16_t(0x10)}) {
        uint8_t bytes[19] = {}; Rtu::BusRequest competing;
        competing.expected.address = 1; competing.expected.target = 1;
        competing.expected.targetGeneration = app->bindingGeneration;
        competing.expected.first = reg; competing.expected.count = reg == 0x24 ? 2 : 1;
        competing.expected.function = reg == 0x24 ? 16 : 6; competing.expected.value = 1;
        const uint16_t words[2] = {0, 999};
        competing.wire.length = reg == 0x24 ? ESS::buildWriteMultipleRegisters(1, reg, words, 2, bytes, sizeof(bytes)) :
            ESS::buildWriteSingleRegister(1, reg, 1, bytes, sizeof(bytes));
        competing.wire.bytes = bytes; competing.wire.replyLength = 8;
        competing.wire.responseTimeoutUs = app->serial.timing.responseTimeoutUs; competing.wire.deadlineUs = nowUs() + REQUEST_US;
        competing.validator = Rtu::essValidator();
        assert(checkAxisWrite(*app, competing) == Probe::Action::AXIS_CONFLICT);
        assert(app->owner.pending() == pending && hardware.writes == writes);
    }
    moveStep(operation); assert(hardware.tx[1] == 6 && hardware.tx[3] == 0x27 && hardware.tx[5] == 1);
    assert(view(operation).moveContext->execution == ActionExecution::ACKNOWLEDGED);
    moveStep(operation, registers(1, {0, 1})); // Old in-place condition cannot complete a fresh move.
    assert(view(operation).pending && !view(operation).moveContext->runningObserved);
    uint32_t unrelated = 0; assert(probe(app, 7, 2, unrelated) == Probe::Action::OK);
    finishProbe(unrelated, 2); assert(axisReserved(*app, 1));
    moveStep(operation, registers(1, {0, 4}));
    assert(view(operation).pending && view(operation).moveContext->runningObserved);
    moveStep(operation, registers(1, {0, 1}));
    assert(!view(operation).pending && view(operation).moveContext->completion == ActionCompletion::OBSERVED);
    assert(!axisReserved(*app, 1) && !app->owner.needsRecovery());
    assert(view(operation).moveContext->prepared.effectiveNative == 20);
}
// Wired bench 07. X3 = function F, its I/O sample raw, the observation read
// 0x0006..0x0008. A running poll 500 ms apart is reached by advancing to its
// eligible time; nothing else changes.
void stopInputs(uint16_t function, uint16_t raw) {
    app->configuration.raw.inputFunctions[3] = function;
    auto& io = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    io.valid = true; io.value.target = app->axis.target; io.value.rawInputs = raw; io.value.inputs[3] = (raw & 8) != 0;
    io.observedEarliestUs = io.observedLatestUs = hardware.time;
}
void pollStep(uint32_t operation, std::initializer_list<uint16_t> words) {
    const auto eligible = view(operation).moveContext->eligibleUs;
    if (hardware.time + 1000 < eligible) { advanceHardware(eligible - 1000); serviceApplication(); }
    moveStep(operation, registers(1, words));
}
void testStopInputEndsMoveShortWithoutPollingToDeadline() {
    // Refused before TX with a named cause while the input reads active (Implement 1).
    struct Refusal { uint16_t function; const char* reason; };
    for (const auto& r : {Refusal{5, "stop_input_active"}, Refusal{6, "stop_input_active"}, Refusal{4, "release_input_active"}}) {
        fresh(); qualify(); stopInputs(r.function, 8);
        uint32_t untouched = 77;
        assert(host(app).startMove(app, 1, 1, request(), untouched) == Probe::Action::UNAVAILABLE);
        assert(untouched == 77 && !hardware.writes && !axisReserved(*app, 1));
        Probe::SettingsRefusal refusal;
        assert(copySettingsRefusal(refusal) && !std::strcmp(refusal.reason, r.reason) && refusal.terminal == 3);
        command("@4 move relative 20 steps native 60 configured\n");
        assert(!hardware.writes && Serial.output.find(std::string("\"reason\":\"") + r.reason) != std::string::npos);
    }
    fresh(); qualify(); stopInputs(5, 0);
    app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)].valid = false;
    uint32_t untouched = 77;
    assert(host(app).startMove(app, 1, 1, request(), untouched) == Probe::Action::UNAVAILABLE && !hardware.writes);
    Probe::SettingsRefusal stale;
    assert(copySettingsRefusal(stale) && !std::strcmp(stale.reason, "input_state_stale"));
    // Recorded words, pilot wb07_pilot_s1_f5c (drive 4, 0x4EEA/0x0029, X3 = 5):
    // running+homed 6 with inputs 0, then 6 with X3 active (8), then 2 (homed,
    // not running, not in position) with 8. At HEAD the move polled to its
    // deadline (outcome deadline after 60 polls); it now ends STOP_INPUT.
    fresh(); qualify(); stopInputs(5, 0);
    auto id = admitMove();
    assert(view(id).moveContext->prerequisites.stopInputMask == 8);
    moveStep(id); moveStep(id);
    waitTx(id); assert(hardware.tx[1] == 3 && hardware.tx[3] == 6 && hardware.tx[5] == 3);
    pollStep(id, {0, 6, 0}); pollStep(id, {0, 6, 8});
    assert(view(id).pending && view(id).moveContext->stopInputObserved);
    pollStep(id, {0, 2, 8});
    auto c = *view(id).moveContext;
    assert(!view(id).pending && c.state == ActionState::FAILED && c.completion == ActionCompletion::NOT_OBSERVED);
    assert(c.status.detail == static_cast<int32_t>(MoveError::STOP_INPUT) && c.rawInputs == 8);
    // As any failure after an acknowledged start: uncertain, and the axis waits
    // for an explicit stop to reconcile it. Nothing is resent.
    assert(c.uncertain && axisReserved(*app, 1));
    assert(c.stopInputEvidence.length && c.polls == 3);
    // The pulse fell between polls: stopped without arrival twice ends STOPPED_SHORT.
    fresh(); qualify(); stopInputs(5, 0);
    id = admitMove(); moveStep(id); moveStep(id);
    pollStep(id, {0, 6, 0}); pollStep(id, {0, 2, 0});
    assert(view(id).pending && view(id).moveContext->stoppedPolls == 1);
    pollStep(id, {0, 2, 0});
    c = *view(id).moveContext;
    assert(!view(id).pending && c.state == ActionState::FAILED && !c.stopInputObserved);
    assert(c.status.detail == static_cast<int32_t>(MoveError::STOPPED_SHORT) && c.completion == ActionCompletion::NOT_OBSERVED);
    // Without a stop/release assignment the observation stays two words; the
    // same short stop still ends STOPPED_SHORT, and a restart resets the count.
    fresh(); qualify();
    id = admitMove(); moveStep(id); moveStep(id);
    waitTx(id); assert(hardware.tx[5] == 2);
    pollStep(id, {0, 4}); pollStep(id, {0, 0}); pollStep(id, {0, 4}); pollStep(id, {0, 0});
    assert(view(id).pending && view(id).moveContext->stoppedPolls == 1);
    pollStep(id, {0, 1});
    assert(!view(id).pending && view(id).moveContext->completion == ActionCompletion::OBSERVED);
    // Natural completion (recorded: 6 then 3 in one sample) is unchanged.
    fresh(); qualify(); stopInputs(5, 0);
    id = admitMove(); moveStep(id); moveStep(id);
    pollStep(id, {0, 6, 0}); pollStep(id, {0, 3, 0});
    assert(!view(id).pending && view(id).moveContext->state == ActionState::SUCCEEDED);
}
// Console terminals of the wired bench 07 outcomes, for the Python validators
// (test/stop_input_parity_test.py): each move is issued through the console.
void emitTerminal(const char* name) {
    const auto start = Serial.output.find("{\"type\":\"move\"");
    assert(start != std::string::npos);
    const auto end = Serial.output.find('\n', start);
    std::printf("{\"case\":\"%s\",\"record\":%s}\n", name, Serial.output.substr(start, end - start).c_str());
}
void emitStopInputResults() {
    struct Trace { const char* name; uint16_t function; std::vector<std::vector<uint16_t>> polls; };
    const Trace traces[] = {
        {"stop_input", 5, {{0, 6, 0}, {0, 6, 8}, {0, 2, 8}}},
        {"stopped_short", 5, {{0, 6, 0}, {0, 2, 0}, {0, 2, 0}}},
        {"stopped_short_unassigned", 0, {{0, 4}, {0, 0}, {0, 0}}},
        {"completion_with_stop_input", 6, {{0, 6, 0}, {0, 3, 0}}},
    };
    for (const auto& t : traces) {
        fresh(); qualify(); stopInputs(t.function, 0);
        command("@4 move relative 20 steps native 60 configured\n");
        const auto id = app->latestOperationId;
        moveStep(id); moveStep(id);
        for (const auto& words : t.polls) {
            const auto eligible = view(id).moveContext->eligibleUs;
            if (hardware.time + 1000 < eligible) { advanceHardware(eligible - 1000); serviceApplication(); }
            std::vector<uint8_t> reply = {1, 3, static_cast<uint8_t>(words.size() * 2)};
            for (const auto w : words) { reply.push_back(static_cast<uint8_t>(w >> 8)); reply.push_back(static_cast<uint8_t>(w)); }
            moveStep(id, crc(reply));
        }
        pump(200);
        assert(!view(id).pending);
        emitTerminal(t.name);
    }
}
// Wired bench 07 Implement 4 (R3): the release input outranks a serial enable,
// so enable is refused before TX while it reads active. Stop inputs do not
// keep the drive released and leave enable admitted; so does an unknown level.
void testEnableRefusedWhileReleaseInputActive() {
    ActionRequest enable; enable.kind = ActionKind::ENABLE;
    fresh(); qualify(); stopInputs(4, 8);
    uint32_t untouched = 77;
    assert(host(app).startAction(app, 1, 1, enable, untouched) == Probe::Action::UNAVAILABLE);
    assert(untouched == 77 && !hardware.writes && !axisReserved(*app, 1));
    Probe::SettingsRefusal refusal;
    assert(copySettingsRefusal(refusal) && !std::strcmp(refusal.reason, "release_input_active") &&
           !std::strcmp(refusal.purpose, "motion") && refusal.terminal == 3);
    command("@7 enable 1\n");
    assert(!hardware.writes);
    assert(Serial.output.find("\"reason\":\"release_input_active\"") != std::string::npos);
    // A stop is never refused by the input, and it names no reason.
    ActionRequest stop; stop.kind = ActionKind::STOP; stop.stop.behavior = StopBehavior::DIRECT;
    uint32_t stopId = 0;
    assert(host(app).startAction(app, 2, 1, stop, stopId) == Probe::Action::OK && stopId);
    for (const auto& c : {std::pair<uint16_t, uint16_t>{4, 0}, {5, 8}, {6, 8}}) {
        fresh(); qualify(); stopInputs(c.first, c.second);
        uint32_t id = 0;
        assert(host(app).startAction(app, 1, 1, enable, id) == Probe::Action::OK && id);
        Probe::SettingsRefusal none; assert(!copySettingsRefusal(none));
    }
    // No fresh input status: admitted as before (the drive decides).
    fresh(); qualify(); stopInputs(4, 8);
    app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)].valid = false;
    uint32_t id = 0;
    assert(host(app).startAction(app, 1, 1, enable, id) == Probe::Action::OK && id);
}
void testCommonAndProfileCliUseExactPreparation() {
    for (bool native : {false, true}) {
        fresh(); qualify();
        app->axis.units.commandStepsPerMotorTurn = UnitScale(1000, 1, ScaleSource::ASSUMED);
        command(native ? "@1 move relative 20 steps native 60 configured\n" :
            "@1 move relative 0.02 turn motor 60 configured\n");
        if (Serial.output.find("\"result\":\"accepted\"") == std::string::npos)
            std::fprintf(stderr, "Move CLI rejected: %s\n", Serial.output.c_str());
        assert(Serial.output.find("\"result\":\"accepted\"") != std::string::npos);
        const uint32_t operation = view(0).operationId;
        assert(view(operation).moveContext->prepared.effectiveNative == 20);
        moveStep(operation); moveStep(operation); moveStep(operation, registers(1, {0, 4}));
        moveStep(operation, registers(1, {0, 1})); pump(1000);
        assert(Serial.output.find("\"type\":\"move\"") != std::string::npos);
        assert(Serial.output.find("\"effective_native\":20") != std::string::npos);
        assert(!view(operation).pending);
    }
}
void testWrongStageEchoAndLostTriggerNeverReplay() {
    for (bool lostTrigger : {false, true}) {
        fresh(); qualify(); const uint32_t operation = admitMove();
        if (lostTrigger) {
            moveStep(operation); waitTx(operation);
            for (unsigned i = 0; i < 30000 && view(operation).pending; ++i) step();
        } else {
            waitTx(operation); auto bytes = acknowledgement(); bytes[5] = 4;
            bytes.resize(6); moveStep(operation, crc(bytes));
        }
        assert(!view(operation).pending && view(operation).moveContext->uncertain);
        const auto retained = *view(operation).moveContext;
        assert(axisReserved(*app, 1) && app->owner.needsRecovery());
        const unsigned writes = hardware.writes; pump(1000); assert(hardware.writes == writes);
        uint32_t recovery = 0; assert(recover(app, 8, recovery) == Probe::Action::OK);
        for (unsigned i = 0; i < 80000 && app->owner.recovering(); ++i) step();
        assert(!app->owner.needsRecovery() && axisReserved(*app, 1));
        assert(view(operation).moveContext->execution == retained.execution);
        assert(view(operation).moveContext->outcome == retained.outcome && hardware.writes == writes);
    }
}
void coordinates(int64_t native);
void testCancellationAndStopAtSequenceBoundaries() {
    for (unsigned phase = 0; phase < 8; ++phase) {
        fresh(); qualify(); coordinates(0);
        const auto origin = app->axis.originNative;
        const auto generation = app->axis.generation;
        const uint32_t operation = admitMove();
        if (phase == 1) {
            for (unsigned i = 0; i < 1000 && app->runner.phase() != Rtu::Phase::SETUP; ++i) step();
            assert(app->runner.phase() == Rtu::Phase::SETUP);
        }
        if ((phase >= 2 && phase <= 4) || phase >= 6) moveStep(operation);
        if ((phase >= 3 && phase <= 4) || phase == 7) moveStep(operation);
        if (phase == 4) moveStep(operation, registers(1, {0, 4}));
        if (phase >= 5) waitTx(operation); // Settle the already accepted physical frame before stop.
        const unsigned previousWrites = hardware.writes;
        const uint32_t stopping = admitStop();
        if (phase == 5 || phase == 6) moveStep(operation);
        if (phase == 7) moveStep(operation, registers(1, {0, 4}));
        for (unsigned i = 0; i < 1000 && view(operation).pending; ++i) step();
        assert(!view(operation).pending && view(operation).interruptedByStop);
        assert(view(operation).moveContext->outcome == ActionOutcome::CANCELLED);
        const auto cancelled = *view(operation).moveContext;
        actionStep(stopping); actionStep(stopping, registers(1, {0, 1}));
        assert(view(stopping).actionContext->completion == ActionCompletion::OBSERVED);
        assert(view(operation).moveContext->outcome == cancelled.outcome && !axisReserved(*app, 1));
        assert(app->axis.originKnown && app->axis.originNative == origin && app->axis.generation == generation);
        assert(!app->coordinateReference.nativeKnown); // A new move must read its actual starting position.
        assert(hardware.writes == previousWrites + 2);
    }
    fresh(); qualify(); const uint32_t unsent = admitMove();
    assert(cancel(app, unsent) == Probe::Action::OK); pump();
    assert(!view(unsent).pending && hardware.writes == 0 && !axisReserved(*app, 1));
    fresh(); qualify(); const uint32_t transmitting = admitMove(); waitTx(transmitting);
    const uint64_t physicalEnd = hardware.writeStarted + hardware.tx.size() * 87;
    assert(cancel(app, transmitting) == Probe::Action::OK);
    assert(!app->owner.needsRecovery()); // Local cancellation leaves the frame to settle.
    for (unsigned i = 0; i < 30000 && view(transmitting).pending; ++i) step();
    assert(!view(transmitting).pending && view(transmitting).moveContext->uncertain);
    assert(hardware.deReleasedAt >= physicalEnd && hardware.writes == 1 && axisReserved(*app, 1));
    fresh(); qualify(); const uint32_t moving = admitMove();
    moveStep(moving); moveStep(moving); moveStep(moving, registers(1, {0, 4}));
    assert(cancel(app, moving) == Probe::Action::OK); pump();
    assert(!view(moving).pending && view(moving).moveContext->uncertain && axisReserved(*app, 1));
    const unsigned writes = hardware.writes; pump(1000); assert(hardware.writes == writes);
    assert(triggeredMotion(*app, 1));
    const uint32_t stopping = admitStop();
    actionStep(stopping); actionStep(stopping, registers(1, {0, 1}));
    assert(!triggeredMotion(*app, 1) && view(moving).moveContext->uncertain);
    qualify(); const auto later = admitMove();
    moveStep(later, crc({1, 0x90, 2}));
    assert(view(later).moveContext->uncertain && axisReserved(*app, 1));
    assert(!triggeredMotion(*app, 1)); // An old stopped trigger cannot explain later external movement.
}
void testFullRetainedResultsStillReserveStop() {
    fresh(); qualify();
    uint32_t retained[7] = {};
    for (unsigned i = 0; i < 7; ++i) {
        assert(probe(app, 10 + i, 2, retained[i]) == Probe::Action::OK); finishProbe(retained[i], 2);
    }
    const uint32_t operation = admitMove(); moveStep(operation); moveStep(operation);
    uint32_t unchanged = 123;
    assert(probe(app, 30, 2, unchanged) == Probe::Action::RESULTS_FULL && unchanged == 123);
    const uint32_t stopping = admitStop(); pump();
    assert(!view(operation).pending && view(operation).moveContext->uncertain);
    actionStep(stopping); actionStep(stopping, registers(1, {0, 1}));
    assert(view(stopping).actionContext->completion == ActionCompletion::OBSERVED);
    for (const auto id : retained) assert(view(id).probe.outcome == Rtu::Outcome::SUCCESS);
    assert(!axisReserved(*app, 1));
}
void testConsoleResetPreservesUncertainTriggerUnderBackpressure() {
    fresh(); qualify();
    command("@1 move relative 20 steps native 60 configured\n");
    const uint32_t operation = view(0).operationId;
    moveStep(operation); waitTx(operation); // Trigger accepted, acknowledgement absent.
    assert(hardware.tx[1] == 6 && hardware.tx[3] == 0x27 && hardware.tx[5] == 1);
    const uint64_t physicalEnd = hardware.writeStarted + hardware.tx.size() * 87;
    const auto releasedBefore = hardware.deReleasedAt;
    const auto writes = hardware.writes, resets = hardware.rxResets;
    const auto binding = app->bindingGeneration, configuration = app->axis.generation;
    const auto serial = app->serial.generation;
    const auto trigger = hardware.tx;
    assert(hardware.time < physicalEnd && app->runner.transmitEnabled() && hardware.de == 1);

    Serial.output.clear(); Serial.writeCapacity = 0;
    Serial.input = "@2 cancel " + std::to_string(operation) + "\n@3 stats reset\n";
    step();
    assert(Serial.input.empty() && Serial.output.empty() && app->outputCount);
    assert(app->runner.stats().started == 0); // The ordinary console reset executed.
    assert(view(operation).pending && app->runner.transmitEnabled() && hardware.de == 1);
    assert(hardware.deReleasedAt == releasedBefore && hardware.writes == writes && hardware.tx == trigger);
    assert(app->bindingGeneration == binding && app->axis.generation == configuration && app->serial.generation == serial);

    // Local cancellation leaves the in-flight trigger to settle naturally; its
    // lost reply is the genuine transport fault, recorded as such.
    for (unsigned i = 0; i < 30000 && view(operation).pending; ++i) step();
    assert(!view(operation).pending && axisReserved(*app, 1));
    const auto retained = *view(operation).moveContext;
    assert(retained.outcome == ActionOutcome::TRANSPORT_ERROR && retained.uncertain);
    assert(retained.execution == ActionExecution::UNKNOWN && retained.triggerEvidence.txAccepted == trigger.size());
    assert(app->owner.needsRecovery());
    assert(hardware.deReleasedAt >= physicalEnd && !app->runner.transmitEnabled() && hardware.de == 0);
    const auto releasedAfter = hardware.deReleasedAt;

    // Output remains blocked while reset runs again against the retained unknown
    // trigger. Counter reset is neither transport recovery nor a motor stop.
    command("@4 stats reset\n");
    assert(Serial.output.empty() && app->runner.stats().started == 0);
    assert(view(operation).moveContext->outcome == retained.outcome && view(operation).moveContext->uncertain);
    assert(view(operation).moveContext->execution == retained.execution && axisReserved(*app, 1));
    assert(app->bindingGeneration == binding && app->axis.generation == configuration && app->serial.generation == serial);
    assert(hardware.writes == writes && hardware.tx == trigger && hardware.rxResets == resets);
    assert(hardware.deReleasedAt == releasedAfter && !app->runner.transmitEnabled() && hardware.de == 0);
    assert(!app->owner.pending() && !app->owner.recovering() && !app->persistenceInvocations);
    assert(app->owner.needsRecovery());

    Serial.writeCapacity = 4096; pump(3000);
    assert(Serial.output.find("\"id\":3,\"command\":\"stats\",\"ok\":true") != std::string::npos);
    assert(Serial.output.find("\"id\":4,\"command\":\"stats\",\"ok\":true") != std::string::npos);
    assert(hardware.writes == writes && view(operation).moveContext->uncertain && axisReserved(*app, 1));
    assert(app->owner.needsRecovery());
}
void testConsoleStopUnderFullUsbBackpressure() {
    fresh(); qualify();
    command("@1 move relative 20 steps native 60 configured\n");
    const uint32_t operation = view(0).operationId;
    moveStep(operation); moveStep(operation); moveStep(operation, registers(1, {0, 4}));
    assert(view(operation).pending && view(operation).moveContext->runningObserved);
    const unsigned writes = hardware.writes;
    Serial.output.clear(); Serial.writeCapacity = 0;
    // Fill real output storage with cached queries. Stop uses its reserved
    // admission/output handoff even when ordinary responses cannot be queued.
    Serial.input.clear();
    for (unsigned id = 10; id < 19; ++id)
        Serial.input += "@" + std::to_string(id) + " status\n";
    Serial.input += "@50 stop fast\n";
    for (unsigned i = 0; i < 50 && !Serial.input.empty(); ++i) step();
    assert(Serial.input.empty() && Serial.output.empty() && app->outputCount == OUTPUT_LINES);
    const uint32_t stopping = view(0).operationId;
    assert(stopping != operation && findRecord(*app, stopping)->commandId == 50);
    // Input is fed after sequence service; the admitted stop's cancellation
    // handoff is applied on the next cooperative turn, while USB stays blocked.
    for (unsigned i = 0; i < 100 && view(operation).pending; ++i) step();
    assert(!view(operation).pending && view(operation).interruptedByStop);
    actionStep(stopping); actionStep(stopping, registers(1, {0, 1}));
    assert(!view(stopping).pending && view(stopping).actionContext->completion == ActionCompletion::OBSERVED);
    assert(hardware.writes == writes + 2 && !app->runner.transmitEnabled() && !axisReserved(*app, 1));
    const auto retainedMove = *view(operation).moveContext;
    const auto retainedStop = *view(stopping).actionContext;
    pump(100);
    assert(Serial.output.empty() && view(operation).moveContext->outcome == retainedMove.outcome);
    assert(view(stopping).actionContext->completion == retainedStop.completion);
    Serial.writeCapacity = 4096; pump(3000);
    assert(!app->outputCount && !app->console.outputPending());
    const auto count = [](const std::string& text, const char* token) {
        unsigned total = 0; std::size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::string::npos) { ++total; pos += std::strlen(token); }
        return total;
    };
    assert(count(Serial.output, "\"type\":\"move\"") == 1);
    assert(count(Serial.output, "\"type\":\"action\"") == 1);
    assert(Serial.output.find("\"id\":50,\"command\":\"stop\"") != std::string::npos);
    assert(view(operation).moveContext->uncertain && view(stopping).actionContext->completion == ActionCompletion::OBSERVED);
    assert(hardware.writes == writes + 2); // Output draining never replays motor work.
}
void deliverChangedConfiguration(unsigned field) {
    // Supply a completed checked read, then exercise the actual application's
    // delivery/invalidation/cancelUnsent path. These are synthetic core events,
    // not a claim that five reads fit between two adjacent wire transactions.
    auto raw = app->configuration.raw;
    if (field == 0) raw.inputPolarity = 1;
    if (field == 1) raw.inputFunctions[0] = 0;
    if (field == 2) raw.overLimitStop = 1;
    if (field == 3) raw.softLimitEnable = 1;
    auto& record = app->records[REQUEST_CAPACITY - 1];
    assert(!record.operationId);
    record.operationId = record.commandId = 1000; record.address = 1; record.kind = App::OperationKind::TYPED_READ;
    record.serialTuple = app->serial.active; record.serialGeneration = app->serial.generation;
    record.configurationGeneration = app->axis.generation;
    assert(ESS::prepareConfig(record.read, app->axis.target, 1000, hardware.time - 1000, hardware.time + REQUEST_US));
    const std::vector<uint8_t> replies[] = {
        registers(1, {raw.direction, raw.subdivision}),
        registers(1, {raw.customNode, raw.baud, raw.format}),
        registers(1, {raw.overLimitStop, raw.softLimitEnable, raw.wordOrder}),
        registers(1, {raw.inputPolarity, raw.inputFunctions[0], raw.inputFunctions[1], raw.inputFunctions[2], raw.inputFunctions[3]}),
        registers(1, {raw.algorithm, raw.encoderResolution})
    };
    for (uint8_t i = 0; i < 5; ++i) {
        const uint64_t suppliedTime = hardware.time - 900 + i * 100;
        ReadEvent e; e.target = app->axis.target; e.operationId = 1000; e.step = i;
        e.frame = replies[i].data(); e.length = replies[i].size(); e.txAccepted = 8;
        e.qualified = true; e.earliestUs = suppliedTime - 20; e.latestUs = suppliedTime - 10;
        assert(ESS::advanceRead(record.read, e, suppliedTime));
    }
    assert(record.read.state == ReadState::SUCCEEDED); deliver(*app);
    assert(record.observed && app->configuration.operationId == 1000);
}
void testConfigurationChangesCancelContinuations() {
    for (unsigned field = 0; field < 4; ++field) {
        fresh(); qualify(); const uint32_t operation = admitMove(); moveStep(operation);
        const uint32_t generation = app->axis.generation;
        assert(view(operation).moveContext->step == 1 && hardware.writes == 1);
        deliverChangedConfiguration(field); pump();
        assert(app->axis.generation == generation + 1);
        assert(!view(operation).pending && view(operation).moveContext->outcome == ActionOutcome::CANCELLED);
        assert(view(operation).moveContext->execution == ActionExecution::NOT_TRANSMITTED);
        assert(view(operation).moveContext->uncertain && hardware.writes == 1);
    }
    fresh(); qualify(); const uint32_t moving = admitMove();
    moveStep(moving); moveStep(moving); waitTx(moving);
    deliverChangedConfiguration(0);
    assert(view(moving).pending); // Already transmitted observation must settle.
    moveStep(moving, registers(1, {0, 4})); pump();
    assert(!view(moving).pending && view(moving).moveContext->outcome == ActionOutcome::CANCELLED);
    assert(view(moving).moveContext->execution == ActionExecution::ACKNOWLEDGED);
    assert(view(moving).moveContext->uncertain && hardware.writes == 3);
}
void testReadinessBoundsActualOwnerWrites() {
    // Admission reserves a result, but does not authorize a stale queued write.
    fresh(); qualify(); app->movePrerequisites.maximumAgeUs = 20000;
    const uint32_t queued = admitMove();
    const auto* queuedMove = view(queued).moveContext;
    const uint64_t cap = queuedMove->prerequisites.observedUs + queuedMove->prerequisites.maximumAgeUs;
    const uint64_t operationDeadline = queuedMove->deadlineUs;
    advanceHardware(cap + 1000); serviceApplication(); pump();
    assert(!view(queued).pending && hardware.writes == 0 && !axisReserved(*app, 1));
    assert(view(queued).moveContext->outcome == ActionOutcome::DEADLINE);
    assert(view(queued).moveContext->status.detail == static_cast<int32_t>(MoveError::READINESS));
    assert(view(queued).moveContext->deadlineUs == operationDeadline);

    fresh(); qualify(); app->movePrerequisites.maximumAgeUs = 20000;
    const uint32_t setup = admitMove();
    for (unsigned i = 0; i < 25000 && !app->runner.transmitEnabled(); ++i) step();
    assert(app->runner.transmitEnabled() && hardware.writes == 0);
    const uint64_t setupCap = view(setup).moveContext->prerequisites.observedUs + 20000;
    advanceHardware(setupCap + 1000); serviceApplication(); pump();
    assert(!view(setup).pending && hardware.writes == 0 && !view(setup).moveContext->uncertain);
    assert(view(setup).moveContext->status.detail == static_cast<int32_t>(MoveError::READINESS));

    // Expiry during physical TX settles the accepted frame; it never cuts TX
    // short or turns its unknown device effects into a safe automatic replay.
    fresh(); qualify(); app->movePrerequisites.maximumAgeUs = 8000;
    hardware.txCharacterUs = 500;
    const uint32_t transmitting = admitMove(); waitTx(transmitting);
    const uint64_t physicalEnd = hardware.writeStarted + hardware.tx.size() * 500;
    const uint64_t txCap = view(transmitting).moveContext->prerequisites.observedUs + 8000;
    assert(physicalEnd > txCap);
    advanceHardware(txCap + 100); serviceApplication();
    assert(app->runner.transmitEnabled() && app->owner.needsRecovery());
    for (unsigned i = 0; i < 5000 && app->runner.transmitEnabled(); ++i) step();
    assert(!view(transmitting).pending && hardware.deReleasedAt >= physicalEnd);
    assert(view(transmitting).moveContext->status.detail == static_cast<int32_t>(MoveError::READINESS));
    assert(view(transmitting).moveContext->uncertain && hardware.writes == 1);
    assert(view(transmitting).moveContext->setupExecution == ActionExecution::UNKNOWN);

    // The capture timer closes an on-time staging response independently of
    // owner service. Applied staging remains uncertain; no stale trigger follows.
    fresh(); qualify(); app->movePrerequisites.maximumAgeUs = 20000;
    const uint32_t staging = admitMove(); waitTx(staging);
    const uint64_t stageCap = view(staging).moveContext->prerequisites.observedUs + 20000;
    scheduleReply(hardware.writeStarted + hardware.tx.size() * 87 + 1000, acknowledgement());
    advanceHardware(stageCap + 1000); serviceApplication(); pump();
    const auto* settled = view(staging).moveContext;
    assert(!view(staging).pending && settled->stagingApplied && settled->uncertain);
    assert(settled->stagingEvidence.latestUs < stageCap && settled->stagingEvidence.deliveredUs > stageCap);
    assert(settled->status.detail == static_cast<int32_t>(MoveError::READINESS));
    assert(settled->execution == ActionExecution::NOT_TRANSMITTED && hardware.writes == 1);
    const unsigned writes = hardware.writes; pump(1000); assert(hardware.writes == writes);

    // A trigger physically acknowledged before its cap may be delivered later;
    // completion observations use the immutable, longer operation deadline.
    fresh(); qualify(); app->movePrerequisites.maximumAgeUs = 20000;
    const uint32_t triggered = admitMove(); moveStep(triggered); waitTx(triggered);
    const uint64_t triggerCap = view(triggered).moveContext->prerequisites.observedUs + 20000;
    scheduleReply(hardware.writeStarted + hardware.tx.size() * 87 + 1000, acknowledgement());
    advanceHardware(triggerCap + 1000); serviceApplication();
    const auto* active = view(triggered).moveContext;
    assert(view(triggered).pending && active->step == 2 && hardware.writes == 2);
    assert(active->triggerEvidence.latestUs < triggerCap && active->triggerEvidence.deliveredUs > triggerCap);
    assert(active->execution == ActionExecution::ACKNOWLEDGED);
    moveStep(triggered, registers(1, {0, 4})); moveStep(triggered, registers(1, {0, 1}));
    assert(!view(triggered).pending && view(triggered).moveContext->completion == ActionCompletion::OBSERVED);
}
void testCheckedStagingExceptionRetainsPartialSetupUncertainty() {
    fresh(); qualify(); const uint32_t operation = admitMove();
    moveStep(operation, crc({1, 0x90, 3}));
    const auto* rejected = view(operation).moveContext;
    assert(!view(operation).pending && rejected->outcome == ActionOutcome::REPLY_ERROR);
    assert(rejected->stagingEvidence.status.code == Err::EXCEPTION && rejected->stagingEvidence.status.detail == 3);
    assert(rejected->setupExecution == ActionExecution::REJECTED && !rejected->stagingApplied);
    assert(rejected->uncertain && rejected->execution == ActionExecution::NOT_TRANSMITTED);
    assert(axisReserved(*app, 1) && !app->owner.needsRecovery());
    pump(1000); assert(hardware.writes == 1);
}
void testDeferredTriggerExpiresBeforeAdmission() {
    fresh(); qualify(); app->movePrerequisites.maximumAgeUs = 20000;
    const uint32_t operation = admitMove(); waitTx(operation);
    scheduleReply(hardware.writeStarted + hardware.tx.size() * 87 + 1000, acknowledgement());
    auto* record = findRecord(*app, operation);
    // Settle only the real owner so the application can deliver staging while
    // ordinary pending storage is occupied by other, read-only producers.
    for (unsigned i = 0; i < 25000 && !app->owner.result(record->requestId); ++i) {
        advanceHardware(hardware.time + 10); app->owner.service(uart.sample());
    }
    assert(app->owner.result(record->requestId));
    uint32_t producers[4] = {};
    for (unsigned i = 0; i < 4; ++i)
        assert(probe(app, 20 + i, 2, producers[i]) == Probe::Action::OK);
    advanceActions(*app, nowUs());
    assert(view(operation).pending && view(operation).moveContext->step == 1 && !record->requestId.owner);
    const uint64_t cap = view(operation).moveContext->prerequisites.observedUs + 20000;
    advanceHardware(cap + 1000); advanceActions(*app, nowUs());
    assert(!view(operation).pending && hardware.writes == 1);
    assert(view(operation).moveContext->outcome == ActionOutcome::DEADLINE);
    assert(view(operation).moveContext->status.detail == static_cast<int32_t>(MoveError::READINESS));
    assert(view(operation).moveContext->stagingApplied && view(operation).moveContext->uncertain);
    assert(view(operation).moveContext->execution == ActionExecution::NOT_TRANSMITTED);
    assert(axisReserved(*app, 1)); // The applied staging uncertainty is retained.
}
void coordinates(int64_t native = 0) {
    auto& a = app->axis; a.originKnown = true; a.originSource = ScaleSource::QUALIFIED;
    a.units.commandStepsPerMotorTurn = UnitScale(1000, 1, ScaleSource::QUALIFIED);
    a.units.fullStepsPerMotorTurn = UnitScale(200, 1, ScaleSource::DOCUMENTED);
    a.units.motorTurnsPerLoadTurn = UnitScale(1, 1, ScaleSource::ASSUMED);
    a.units.millimetresPerLoadTurn = UnitScale(4, 1, ScaleSource::ASSUMED);
    auto& ref = app->coordinateReference; ref.target = a.target; ref.configurationGeneration = a.generation;
    ref.nativeKnown = ref.stationary = ref.idle = true; ref.nativePosition = native;
    ref.source = ScaleSource::QUALIFIED; ref.observedUs = hardware.time; ref.maximumAgeUs = 1000000;
}
void testAbsoluteAngleApiCliAndLostReference() {
    for (const char* line : {"@1 move absolute 20 steps native 60 configured\n",
        "@1 move absolute 7.2 deg motor 60 configured\n",
        "@1 move absolute 4 fullsteps motor 60 configured\n",
        "@1 move absolute 0.02 turn motor 60 configured\n",
        "@1 move absolute 0.08 mm load 60 configured\n",
        "@1 move absolute 0.1256637061435917 rad motor 60 configured round nearest 1 approx 0.00001\n",
        "@1 move angle 7.2 deg motor positive reject 60 configured\n",
        "@1 move angle 7.2 deg motor shortest reject 60 configured\n"}) {
        fresh(); qualify(); coordinates();
        auto r = request(); r.position.relative = false;
        ESS::MoveContext direct;
        assert(MotorControlRS::prepareMoveAbsolute(direct, app->axis, &app->coordinateReference, 99,
            r, app->movePrerequisites, nowUs(), nowUs() + 3000000));
        command(line);
        assert(Serial.output.find("\"result\":\"accepted\"") != std::string::npos);
        const auto operation = view(0).operationId;
        assert(view(operation).moveContext->prepared.effectiveNative == direct.prepared.effectiveNative);
        moveStep(operation); moveStep(operation);
        assert(hardware.tx[1] == 6 && hardware.tx[3] == 0x27 && hardware.tx[5] == 5);
        assert(!app->coordinateReference.nativeKnown); // Trigger makes prior pose unavailable immediately.
        const auto retainedReference = view(operation).moveContext->reference;
        advanceHardware(retainedReference.observedUs + retainedReference.maximumAgeUs + 1);
        step(); assert(view(operation).pending); // Starting-reference age cannot cancel a triggered move.
        moveStep(operation, registers(1, {0, 4})); moveStep(operation, registers(1, {0, 1}));
        assert(!view(operation).pending && view(operation).moveContext->state == ActionState::SUCCEEDED);
        assert(app->axis.originKnown && !app->coordinateReference.nativeKnown); // Fixed zero survives; current pose does not.
        assert(view(operation).moveContext->reference.nativePosition == retainedReference.nativePosition);
    }
    // Preserve a multi-turn endpoint and its small relative displacement.
    fresh(); qualify(); coordinates(1990); app->axis.nativeMaximum = 10000;
    command("@2 move absolute 720 deg motor 60 configured\n");
    assert(view(0).moveContext->prepared.effectiveNative == 2000 && view(0).moveContext->prepared.displacementNative == 10);

    fresh(); qualify(); coordinates(); const unsigned writes = hardware.writes;
    for (const char* invalid : {"@3 move absolute 20 steps motor 60 configured\n",
        "@4 move angle 90 deg motor positive reject 60 configured\n"}) {
        app->coordinateReference.nativeKnown = false; app->axis.originKnown = false; command(invalid);
        assert(hardware.writes == writes && !axisReserved(*app, 1));
    }
    coordinates(); ++app->coordinateReference.configurationGeneration;
    command("@5 move angle 90 deg motor positive reject 60 configured\n");
    assert(hardware.writes == writes && !axisReserved(*app, 1));
}
void testOriginIsHostOnlyAndConfidenceInvalidates() {
    fresh(); qualify(); coordinates(50);
    const auto observed = app->coordinateReference.observedUs;
    command("@1 axis origin 10\n");
    assert(Serial.output.find("\"ok\":true") != std::string::npos);
    assert(app->axis.originKnown && app->axis.originNative == 10 && hardware.writes == 0);
    assert(app->coordinateReference.observedUs == observed && app->coordinateReference.nativePosition == 50);
    command("@2 status\n"); command("@3 axis config\n");
    assert(app->coordinateReference.observedUs == observed && hardware.writes == 0);
    advanceHardware(observed + app->coordinateReference.maximumAgeUs + 1); step();
    assert(app->axis.originKnown && app->axis.originNative == 10 && !app->coordinateReference.nativeKnown && hardware.writes == 0);
    qualify(); coordinates();
    auto& motion = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    motion.value.released = true; step();
    assert(!app->axis.originKnown && !app->coordinateReference.nativeKnown);
    qualify(); coordinates(); motion.value.running = true; motion.value.released = false; step();
    assert(!app->axis.originKnown && !app->coordinateReference.nativeKnown);
}
void testHostPreferencesPreserveReferenceAge() {
    for (const char* setting : {"position-unit deg", "velocity-unit rpm", "acceleration-unit deg/s2",
        "native-limits -900 900", "soft-limits -900 900", "relative-bases 3"}) {
        fresh(); qualify(); coordinates(50);
        const auto observed = app->coordinateReference.observedUs;
        const auto maximumAge = app->coordinateReference.maximumAgeUs;
        const auto generation = app->axis.generation;
        command(std::string("@1 axis config set ") + setting + "\n");
        assert(Serial.output.find("\"ok\":true") != std::string::npos);
        assert(app->axis.generation == generation + 1 && app->axis.originKnown);
        assert(app->coordinateReference.nativeKnown && app->coordinateReference.nativePosition == 50);
        assert(app->coordinateReference.configurationGeneration == app->axis.generation);
        assert(app->coordinateReference.observedUs == observed && app->coordinateReference.maximumAgeUs == maximumAge);
        command("@2 prepare angle 36 deg motor shortest reject\n");
        assert(Serial.output.find("\"ok\":true") != std::string::npos);
        assert(hardware.writes == 0);
        const bool limitsKnown = app->axis.softLimitsKnown;
        advanceHardware(observed + maximumAge + 1); step();
        assert(app->axis.originKnown && !app->coordinateReference.nativeKnown && app->axis.softLimitsKnown == limitsKnown);
        assert(hardware.writes == 0);
    }
    fresh(); qualify(); coordinates();
    command("@1 axis config set command 1000\n"); // Same magnitude but explicit source changes QUALIFIED to ASSUMED.
    assert(!app->coordinateReference.nativeKnown && !app->axis.originKnown);
    coordinates(); app->axis.units.commandStepsPerMotorTurn.source = ScaleSource::ASSUMED;
    const auto observed = app->coordinateReference.observedUs;
    command("@2 axis config set command 1000\n"); // Idempotent interpretation keeps the original witness.
    assert(app->coordinateReference.nativeKnown && app->axis.originKnown && app->coordinateReference.observedUs == observed);
    advanceHardware(observed + app->coordinateReference.maximumAgeUs + 1); step();
    assert(app->axis.originKnown && !app->coordinateReference.nativeKnown);
    qualify(); coordinates();
    command("@1 axis config set polarity -1\n");
    assert(Serial.output.find("\"ok\":true") != std::string::npos);
    assert(!app->axis.originKnown && !app->coordinateReference.nativeKnown && hardware.writes == 0);
}
void testPositionClearApiCliAndUncertainInvalidation() {
    for (bool lost : {false, true}) {
        fresh(); qualify(); coordinates(); uint32_t unchanged = 77;
        ActionRequest clear; clear.kind = ActionKind::CLEAR_POSITION; clear.positionClearQualified = true;
        assert(host(app).startAction(app, 1, 1, clear, unchanged) == Probe::Action::QUALIFICATION_REQUIRED);
        assert(unchanged == 77 && hardware.writes == 0); // Caller cannot bypass app qualification.
        assert(installPositionClearQualification());
        clear.devicePosition = 1;
        assert(host(app).startAction(app, 2, 1, clear, unchanged) == Probe::Action::UNSUPPORTED);
        assert(unchanged == 77 && hardware.writes == 0);
        command(lost ? "@3 position-clear\n" : "@3 position-clear\n");
        assert(Serial.output.find("\"result\":\"accepted\"") != std::string::npos);
        const auto operation = view(0).operationId; waitTx(operation); step();
        assert(hardware.tx[3] == 0x2D && hardware.tx[5] == 0x31);
        assert(!app->axis.originKnown && !app->coordinateReference.nativeKnown && axisReserved(*app, 1));
        if (lost) {
            for (unsigned i = 0; i < 30000 && view(operation).pending; ++i) step();
            assert(view(operation).actionContext->execution == ActionExecution::UNKNOWN);
            const auto retained = *view(operation).actionContext;
            pump(); assert(hardware.writes == 1 && !app->axis.originKnown);
            uint32_t recovery = 0; assert(recover(app, 8, recovery) == Probe::Action::OK);
            for (unsigned i = 0; i < 80000 && app->owner.recovering(); ++i) step();
            assert(view(operation).actionContext->execution == retained.execution && !app->axis.originKnown);
            assert(hardware.writes == 1 && axisReserved(*app, 1));
        } else {
            actionStep(operation); actionStep(operation, registers(1, {0, 1}));
            assert(view(operation).pending && view(operation).actionContext->rawPosition == 1);
            assert(hardware.tx[3] == 0xA && hardware.tx[5] == 2);
            actionStep(operation, registers(1, {0, 0}));
            assert(!view(operation).pending && view(operation).actionContext->completion == ActionCompletion::OBSERVED);
            assert(!app->axis.originKnown && !app->coordinateReference.nativeKnown);
            assert(!axisReserved(*app, 1)); // Zero readback establishes neither host origin nor mapping.
        }
    }
}
void testExternalFeedbackInvalidatesOnlyCurrentTarget() {
    for (bool sameTarget : {false, true}) {
        // Test historical feedback target isolation without a separate native
        // zero witness, which itself detects fresh nonzero feedback.
        fresh(); qualify(); coordinates(50);
        auto& old = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
        old.valid = true; old.value.target = app->axis.target; old.value.pairKnown = true;
        old.value.rawPosition = 50; old.observedLatestUs = old.observedEarliestUs = nowUs();
        if (!sameTarget) { old.value.target.address = 2; old.value.target.id = 2; }
        uint32_t operation = 0;
        assert(startTypedRead(app, 1, 1, ESS::ReadKind::STATE, operation, false) == Probe::Action::OK);
        const std::vector<uint8_t> responses[] = {registers(1, {0, 1}), registers(1, {0, 0}), registers(1, {0, 51, 0})};
        for (unsigned block = 0; block < 3; ++block) {
            waitTx(operation); scheduleReply(hardware.time + 2000, responses[block]);
            for (unsigned i = 0; i < 25000 && view(operation).pending && view(operation).typedRead->step == block; ++i) step();
            assert(!view(operation).pending || view(operation).typedRead->step != block);
        }
        assert(!view(operation).pending && hardware.writes == 3);
        assert(app->axis.originKnown != sameTarget && app->coordinateReference.nativeKnown != sameTarget);
    }
}
void testExternalFeedbackDuringStopInvalidatesCoordinates() {
    for (bool runningOnly : {false, true}) {
        fresh(); qualify(); coordinates();
        auto& old = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::FEEDBACK)];
        old.valid = true; old.value.target = app->axis.target; old.value.pairKnown = true;
        old.value.rawPosition = 0; old.observedLatestUs = old.observedEarliestUs = nowUs();
        const uint32_t stop = admitStop(); actionStep(stop);
        uint32_t read = 0;
        assert(startTypedRead(app, 1, 1, ESS::ReadKind::STATE, read, false) == Probe::Action::OK);
        // The stop remains pending while eligible observation work shares the bus.
        // Each actual TX gets its own checked fake response, including stop polls.
        unsigned answered = hardware.writes;
        for (unsigned i = 0; i < 50000 && view(read).pending; ++i) {
            step();
            if (hardware.writes == answered) continue;
            answered = hardware.writes;
            const auto* stopRecord = findRecord(*app, stop);
            const bool stopTx = app->owner.txAccepted(stopRecord->requestId);
            const uint16_t reg = uint16_t(hardware.tx[2]) << 8 | hardware.tx[3];
            const auto reply = stopTx ? registers(1, {0, 4}) : reg == 6 ? registers(1, {0, uint16_t(runningOnly ? 4 : 1)}) :
                reg == 8 ? registers(1, {0, 0}) : registers(1, {0, uint16_t(runningOnly ? 0 : 1), 0});
            scheduleReply(hardware.time + hardware.tx.size() * 87 + 1000, reply);
        }
        assert(!view(read).pending && view(stop).pending && axisReserved(*app, 1));
        assert(!app->axis.originKnown && !app->coordinateReference.nativeKnown);
    }
}
void testPositionClearBadObservationKeepsConflict() {
    fresh(); qualify(); coordinates(); assert(installPositionClearQualification());
    command("@1 position-clear\n");
    const uint32_t operation = view(0).operationId;
    actionStep(operation);
    auto corrupt = registers(1, {0, 0}); corrupt.back() ^= 1;
    actionStep(operation, corrupt);
    assert(!view(operation).pending && hardware.writes == 2);
    const auto retained = *view(operation).actionContext;
    assert(retained.execution == ActionExecution::ACKNOWLEDGED && retained.completion == ActionCompletion::NOT_OBSERVED);
    assert(retained.status.code == Err::CRC_ERROR && axisReserved(*app, 1));
    assert(!app->axis.originKnown && !app->coordinateReference.nativeKnown);
    pump(); assert(release(app, operation) == Probe::Action::OK);
    assert(axisReserved(*app, 1));
    uint32_t recovery = 0; assert(recover(app, 8, recovery) == Probe::Action::OK);
    for (unsigned i = 0; i < 80000 && app->owner.recovering(); ++i) step();
    assert(!app->owner.needsRecovery() && axisReserved(*app, 1) && hardware.writes == 2);
    assert(!app->axis.originKnown && !app->coordinateReference.nativeKnown);
}
} // namespace

// Establish admission through actual typed reads and the normal profile snapshot.
// No qualify() flags or fabricated cache entries are used on the accepting path.
static void readProductionMoveBaseline() {
    const auto read = [](ESS::ReadKind kind, const std::vector<std::vector<uint8_t>>& responses) {
        uint32_t operation = 0;
        assert(startTypedRead(app, 1, 1, kind, operation, false) == Probe::Action::OK);
        for (const auto& response : responses) {
            const auto token = view(operation).typedRead->step;
            waitTx(operation);
            scheduleReply(hardware.time + 2000, response);
            for (unsigned i = 0; i < 25000 && view(operation).pending &&
                 view(operation).typedRead->step == token; ++i) step();
            assert(!view(operation).pending || view(operation).typedRead->step != token);
        }
        assert(!view(operation).pending && view(operation).typedRead->state == ReadState::SUCCEEDED);
        pump(); // Retain the completed read; profile/move admission must coexist with it.
    };
    read(ESS::ReadKind::CONFIG, {registers(1, {0, 1000}), registers(1, {0, 0, 0}),
        registers(1, {0, 0, 0}), registers(1, {0, 1, 2, 3, 0}), registers(1, {3, 4000})});
    read(ESS::ReadKind::STATE, {registers(1, {0, 1}), registers(1, {0, 0}), registers(1, {0, 0, 0})});
    Probe::MotionProfileView profile;
    assert(motionProfileCommand(app, Probe::MotionProfileCommand::SNAPSHOT, profile) == Probe::Action::OK);
    const auto ownerRequest = app->motionProfile.request;
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(ownerRequest); ++i) step();
    assert(app->owner.txAccepted(ownerRequest));
    scheduleReply(hardware.time + 2000, registers(1, {30, 100, 100, 60, 0, 250}));
    for (unsigned i = 0; i < 25000 && app->motionProfile.view.pending; ++i) step();
    assert(!app->motionProfile.view.pending && app->motionProfile.view.saved && app->motionProfile.view.ok);
    assert(!app->movePrerequisites.commandUnitsVerified);
}
static void testProductionMoveOptionalObservationAge() {
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        fresh(scenario == 1 ? 5000 : 0);
        readProductionMoveBaseline();
        const auto observed = app->stateCache.blocks[0].observedEarliestUs;
        advanceHardware(hardware.time + 31000000);
        if (scenario == 2) app->stateCache.blocks[1].valid = false; // Missing required input evidence.
        if (scenario == 3) invalidateAxis(*app); // Configuration invalidation still revokes the snapshot.
        const auto writes = hardware.writes;
        uint32_t operation = 99;
        const auto admitted = host(app).startMove(app, 2, 1, request(), operation);
        if (scenario) {
            assert(admitted != Probe::Action::OK && operation == 99 && hardware.writes == writes);
            assert(!app->owner.active() && !app->owner.pending());
            continue;
        }
        assert(admitted == Probe::Action::OK);
        assert(view(operation).moveContext->prerequisites.maximumAgeUs == 0);
        assert(view(operation).moveContext->prerequisites.observedUs == observed);
        moveStep(operation); moveStep(operation);
        moveStep(operation, registers(1, {0, 4})); moveStep(operation, registers(1, {0, 1}));
        assert(!view(operation).pending && view(operation).moveContext->completion == ActionCompletion::OBSERVED);
        assert(hardware.writes == writes + 4 && !app->owner.needsRecovery());
    }
}
static void testUserMoveInUnits() {
    UnitConfig units;
    units.commandStepsPerMotorTurn = UnitScale(1000, 1, ScaleSource::ASSUMED);
    units.fullStepsPerMotorTurn = UnitScale(200, 1, ScaleSource::ASSUMED);
    units.motorTurnsPerLoadTurn = UnitScale(1, 1, ScaleSource::ASSUMED);
    units.millimetresPerLoadTurn = UnitScale(2, 1, ScaleSource::ASSUMED);
    units.encoder.countsPerUnit = UnitScale(4000, 1, ScaleSource::ASSUMED);
    units.encoder.sourceId = 1;
    const PositionUnit kinds[] = {PositionUnit::STEPS, PositionUnit::DEGREES, PositionUnit::TURNS,
        PositionUnit::FULL_STEPS, PositionUnit::MILLIMETRES, PositionUnit::ENCODER_COUNTS};
    const Rational values[] = {Rational(100), Rational(36), Rational(1, 10),
        Rational(20), Rational(1, 5), Rational(400)};
    for (unsigned i = 0; i < 6; ++i) {
        fresh(0, units); readProductionMoveBaseline();
        if (i == 0) Serial.writeCapacity = 0; // Programmatic results never depend on a writable console.
        uint32_t id = 0;
        assert(MotorControlRSExample::moveBy(values[i], kinds[i], id));
        MoveProgress progress;
        assert(moveProgress(id, progress) && progress.pending && progress.effectiveNative == 100);
        assert(view(id).moveContext->request.position.unit == kinds[i]);
        assert(!releaseMove(id));
        uint32_t unchanged = 999;
        assert(!MotorControlRSExample::moveBy(Rational(10), PositionUnit::STEPS, unchanged) && unchanged == 999);
        moveStep(id);
        assert(hardware.tx[15] == 0 && hardware.tx[16] == 100); // Same checked staging for every unit.
        moveStep(id); moveStep(id, registers(1, {0, 4}));
        assert(moveProgress(id, progress) && progress.pending && progress.runningObserved);
        moveStep(id, registers(1, {0, 1})); pump();
        assert(moveProgress(id, progress) && !progress.pending && !progress.uncertain);
        assert(progress.completion == ActionCompletion::OBSERVED);
        assert(Serial.output.find("\"type\":\"move\"") == std::string::npos);
        assert(releaseMove(id) && !moveProgress(id, progress) && !releaseMove(id));
    }
    fresh(0, UnitConfig()); readProductionMoveBaseline();
    uint32_t id = 999; const auto writes = hardware.writes;
    assert(!MotorControlRSExample::moveBy(Rational(36), PositionUnit::DEGREES, id));
    assert(id == 999 && hardware.writes == writes); // No guessed scale.
    assert(MotorControlRSExample::moveBy(Rational(100), PositionUnit::STEPS, id));
    moveStep(id); moveStep(id); moveStep(id, registers(1, {0, 4}));
    const uint32_t stop = admitStop();
    actionStep(stop); actionStep(stop, registers(1, {0, 0})); pump();
    MoveProgress interrupted;
    assert(moveProgress(id, interrupted) && !interrupted.pending && interrupted.interruptedByStop);
    assert(interrupted.completion == ActionCompletion::NOT_OBSERVED);
    assert(releaseMove(id) && !axisReserved(*app, 1));

    fresh(0, units); readProductionMoveBaseline();
    AxisConfig configured;
    assert(positionConfiguration(configured) && configured.generation == app->axis.generation);
    PositionRequest stale; stale.configurationGeneration = configured.generation + 1;
    stale.value = Rational(100);
    const auto beforeRejected = hardware.writes;
    assert(!submitMove(stale, 60, id) && hardware.writes == beforeRejected);
    id = 999;
    assert(!moveTo(Rational(36), PositionUnit::DEGREES, id) && id == 999); // No invented origin.
    app->axis.originKnown = true; app->axis.originSource = ScaleSource::ASSUMED;
    assert(moveTo(Rational(36), PositionUnit::DEGREES, id));
    moveStep(id); moveStep(id);
    assert(hardware.tx[5] == 5); // Absolute command, never substituted with relative.
    moveStep(id, registers(1, {0, 4})); moveStep(id, registers(1, {0, 1})); pump();
    assert(moveProgress(id, interrupted) && interrupted.effectiveNative == 100);
    assert(releaseMove(id));

    fresh(0, units); readProductionMoveBaseline();
    PositionRequest radians; radians.configurationGeneration = app->axis.generation;
    radians.unit = PositionUnit::RADIANS; radians.frame = CoordinateFrame::MOTOR;
    radians.value = Rational(6283185, 10000000); radians.approximate = true;
    radians.rounding = Rounding::NEAREST; radians.maximumQuantizationError = 0.5;
    radians.maximumApproximationError = 0.01;
    assert(submitMove(radians, 60, id));
    assert(moveProgress(id, interrupted) && interrupted.effectiveNative == 100);
    moveStep(id); moveStep(id); moveStep(id, registers(1, {0, 4})); moveStep(id, registers(1, {0, 1})); pump();
    assert(releaseMove(id));
}
static void testRepeatedMovePolicies() {
    fresh(); readProductionMoveBaseline();
    const auto refreshState=[]() {
        command("@99 read state\n"); const uint32_t read=view(0).operationId;
        assert(view(read).typedRead && view(read).pending);
        for(const auto& response : {registers(1,{0,1}),registers(1,{0,0}),registers(1,{0,100,0})}) {
            const auto token=view(read).typedRead->step;waitTx(read);scheduleReply(hardware.time+2000,response);
            for(unsigned i=0;i<25000 && view(read).pending && view(read).typedRead->step==token;++i)step();
        }
        assert(!view(read).pending);pump();assert(release(app,read)==Probe::Action::OK);
    };
    uint32_t id=0;
    assert(MotorControlRSExample::moveBy(100,PositionUnit::STEPS,id,60,MoveSetup::VERIFY_AND_UPDATE));
    moveStep(id,registers(1,{100,100,60,0,100}));
    assert(view(id).moveContext->step==2 && !view(id).moveContext->stagingApplied);
    assert(app->rememberedMoveWords[4]==100);
    moveStep(id); assert(hardware.tx[1]==6 && hardware.tx[3]==0x27);
    moveStep(id,registers(1,{0,4}));moveStep(id,registers(1,{0,1}));pump();
    assert(releaseMove(id));
    refreshState();
    // A caller explicitly chooses cached reuse, and can repeat without profile reads.
    assert(MotorControlRSExample::moveBy(100,PositionUnit::STEPS,id,60,MoveSetup::USE_STORED));
    moveStep(id); assert(hardware.tx[1]==6 && hardware.tx[3]==0x27);
    moveStep(id,registers(1,{0,4}));moveStep(id,registers(1,{0,1}));pump();
    assert(releaseMove(id));
    refreshState();
    id=777;const auto writes=hardware.writes;
    assert(!MotorControlRSExample::moveBy(101,PositionUnit::STEPS,id,60,MoveSetup::USE_STORED));
    assert(id==777 && hardware.writes==writes);
    assert(MotorControlRSExample::moveBy(101,PositionUnit::STEPS,id,60,MoveSetup::VERIFY_AND_UPDATE));
    moveStep(id,registers(1,{100,100,60,0,100}));
    assert(view(id).moveContext->setupOffset==3 && view(id).moveContext->setupCount==2);
    moveStep(id); assert(hardware.tx[1]==16 && hardware.tx[3]==0x24);
    assert(app->rememberedMoveWords[4]==101);
    moveStep(id);moveStep(id,registers(1,{0,4}));moveStep(id,registers(1,{0,1}));pump();
    assert(releaseMove(id));
    ++app->axis.generation;
    assert(!MotorControlRSExample::moveBy(101,PositionUnit::STEPS,id,60,MoveSetup::USE_STORED));
    // A stopped/interrupted verification owns the same reservation and never starts.
    fresh();readProductionMoveBaseline();
    assert(MotorControlRSExample::moveBy(100,PositionUnit::STEPS,id,60,MoveSetup::VERIFY_AND_UPDATE));
    waitTx(id);const auto stop=admitStop();
    moveStep(id,registers(1,{100,100,60,0,100}));
    actionStep(stop);actionStep(stop,registers(1,{0,0}));pump();
    MoveProgress progress;assert(moveProgress(id,progress) && !progress.pending && progress.interruptedByStop);
    assert(view(id).moveContext->triggerEvidence.txAccepted==0);
}
static void testVerificationReadDoesNotAdoptNewGeneration() {
    fresh(); readProductionMoveBaseline();
    const auto previousGeneration = app->rememberedMoveGeneration;
    const auto previousTarget = app->rememberedMoveWords[4];
    uint32_t id = 0;
    assert(MotorControlRSExample::moveBy(100, PositionUnit::STEPS, id, 60, MoveSetup::VERIFY_AND_UPDATE));
    waitTx(id);
    const unsigned writes = hardware.writes;
    // An external observation/recovery may invalidate assumptions while the
    // already transmitted read settles. Its result keeps its original binding.
    invalidateAxis(*app);
    assert(app->axis.generation != previousGeneration);
    moveStep(id, registers(1, {100, 100, 60, 0, 100}));
    pump();
    const auto& result = *view(id).moveContext;
    assert(!view(id).pending && result.outcome == ActionOutcome::CANCELLED);
    assert(result.verificationKnown && !result.uncertain);
    assert(!result.stagingEvidence.txAccepted && !result.triggerEvidence.txAccepted);
    assert(hardware.writes == writes && !axisReserved(*app, 1));
    assert(app->rememberedMoveGeneration == previousGeneration);
    assert(app->rememberedMoveWords[4] == previousTarget);
    assert(app->rememberedMoveGeneration != app->axis.generation);
}
static void testLocalMoveCancellationInvalidatesRememberedSettings() {
    fresh(); qualify();
    const uint32_t id = admitMove(); waitTx(id);
    auto* record = findRecord(*app, id);
    scheduleReply(hardware.writeStarted + hardware.tx.size() * 87 + 1000, acknowledgement());
    // Occupy ordinary queue slots before delivering the actual stage reply,
    // leaving the next move step unadmitted for direct local cancellation.
    for (unsigned i = 0; i < 25000 && !app->owner.result(record->requestId); ++i) {
        advanceHardware(hardware.time + 10); app->owner.service(uart.sample());
    }
    assert(app->owner.result(record->requestId));
    uint32_t producers[4] = {};
    for (unsigned i = 0; i < 4; ++i)
        assert(probe(app, 20 + i, 2, producers[i]) == Probe::Action::OK);
    advanceActions(*app, nowUs());
    assert(record->move.stagingApplied && record->move.step == record->move.triggerStep);
    assert(!record->requestId.owner && app->rememberedMoveGeneration == app->axis.generation);
    assert(cancel(app, id) == Probe::Action::OK);
    assert(!view(id).pending && record->move.uncertain && record->move.outcome == ActionOutcome::CANCELLED);
    assert(!app->rememberedMoveGeneration && !record->move.triggerEvidence.txAccepted);
    assert(hardware.writes == 1 && axisReserved(*app, 1));
}
void testPositionLimitUsesDriveSubdivision() {
    fresh(); qualify(); app->configuration.raw.subdivision=51200;
    auto r=request(); r.speedRpm=59;
    uint32_t id=999; const auto before=hardware.writes;
    assert(host(app).startMove(app,1,1,r,id)==Probe::Action::INVALID);
    assert(id==999 && hardware.writes==before && !app->owner.pending());
    r.speedRpm=58;
    assert(host(app).startMove(app,2,1,r,id)==Probe::Action::OK);
    assert(findRecord(*app,id)->move.prerequisites.subdivision==51200);
    assert(findRecord(*app,id)->move.words[2]==58);
    assert(host(app).cancel(app,id)==Probe::Action::OK);
    pump(); assert(hardware.writes==before);
}
void testQueuedPositionClearInvalidation() {
    for (unsigned invalidation = 0; invalidation < 3; ++invalidation) {
        fresh(); qualify(); coordinates(); assert(installPositionClearQualification());
        ActionRequest clear; clear.kind = ActionKind::CLEAR_POSITION; uint32_t id = 999;
        assert(startAction(app, 1, 1, clear, id) == Probe::Action::OK && !hardware.writes);
        if (invalidation == 0) {
            uint32_t recovery = 0; assert(recover(app, 2, recovery) == Probe::Action::OK);
        } else if (invalidation == 1) {
            // A newer checked configuration invalidates the pending clear.
            invalidateAxis(*app);
        } else {
            // Fresh contradictory status cannot let a queued clear leave the owner.
            auto& motion = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
            motion.value.running = true; motion.value.rawMotion = 4;
            motion.observedEarliestUs = motion.observedLatestUs = hardware.time;
        }
        pump();
        const auto result = view(id);
        assert(!result.pending && result.actionContext->outcome == ActionOutcome::CANCELLED);
        assert(result.actionContext->execution == ActionExecution::NOT_TRANSMITTED && !hardware.writes);
        assert(result.actionContext->request.positionClearQualified); // Frozen admitted fact remains copied.
    }
}
void testMonitorReferenceRefusal() {
    fresh(); qualify(); coordinates();
    app->monitorState.settings.enabled = true; app->monitorState.remaining = 1;
    app->monitorState.nextDueUs = hardware.time + 1000000;
    auto requestValue = request(); uint32_t id = 999;
    const auto status = submitMove(requestValue.position, requestValue.speedRpm, id);
    assert(!status && status.detail == static_cast<int32_t>(Probe::Action::REFERENCE_NOT_READY));
    assert(id == 999 && !hardware.writes && !app->owner.pending());
    command("@400 move relative 20 steps native 60 configured\n");
    assert(Serial.output.find("\"result\":\"reference_not_ready\"") != std::string::npos);
    assert(!hardware.writes && !app->owner.pending());
    // Earlier malformed intent still wins over the reference diagnostic.
    requestValue.speedRpm = 0;
    assert(startMove(app, 401, 1, requestValue, id) == Probe::Action::INVALID && id == 999);
    app->monitorState.settings.enabled = false;
    assert(submitMove(requestValue.position, 60, id));
    assert(cancel(app, id) == Probe::Action::OK); pump(); assert(!hardware.writes);
}
// Wired bench 11: limit inputs (X1 = 2, X2 = 3 from qualify()) and the
// soft-limit state of SoftLimitApp.h. Every refusal is before TX.
void limitStatus(uint16_t raw, uint16_t polarity = 0) {
    app->configuration.raw.inputPolarity = polarity;
    auto& io = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    io.valid = true; io.value.target = app->axis.target; io.value.rawInputs = raw;
    for (uint8_t i = 0; i < 4; ++i) io.value.inputs[i] = (raw >> i) & 1;
    io.observedEarliestUs = io.observedLatestUs = hardware.time;
}
void bindSoftLimits(int64_t minimum, int64_t maximum) {
    app->configuration.raw.softLimitEnable = 1; app->configuration.softLimitEnableKnown = true;
    auto& b = app->softLimitBinding; b = App::SoftLimitBinding();
    b.target = app->axis.target; b.operationId = 50; b.axisGeneration = app->axis.generation;
    b.bindingGeneration = app->bindingGeneration; b.minimum = minimum; b.maximum = maximum;
    app->configuration.operationId = 99;
    app->axis.softLimitsKnown = true; app->axis.softMinimum = minimum; app->axis.softMaximum = maximum;
    auto& motion = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    motion.value.homingComplete = true; motion.value.rawMotion = 3;
    motion.observedEarliestUs = motion.observedLatestUs = hardware.time;
}
void refusedMove(const MoveRequest& r, const char* reason, int8_t terminal = -1) {
    uint32_t untouched = 77; const auto next = app->nextOperationId;
    assert(host(app).startMove(app, 1, 1, r, untouched) == Probe::Action::UNAVAILABLE);
    assert(untouched == 77 && app->nextOperationId == next && !hardware.writes && !axisReserved(*app, 1));
    Probe::SettingsRefusal refusal;
    assert(copySettingsRefusal(refusal) && !std::strcmp(refusal.reason, reason) && !std::strcmp(refusal.purpose, "motion"));
    assert(terminal < 0 || refusal.terminal == terminal);
}
MoveRequest moveBy(int64_t steps) { auto r = request(); r.position.value = Rational(steps); return r; }
MoveRequest moveTo(int64_t target) { auto r = moveBy(target); r.position.relative = false; return r; }
void testLimitInputsAndSoftLimitState() {
    // A start toward an active limit refuses; a start away refuses too (L5b not run).
    for (const auto steps : {int64_t(20), int64_t(-20)}) {
        fresh(); qualify(); limitStatus(2); refusedMove(moveBy(steps), "limit_input_active", 1);
        fresh(); qualify(); limitStatus(4); refusedMove(moveBy(steps), "limit_input_active", 2);
    }
    command("@3 move relative 20 steps native 60 configured\n");
    assert(!hardware.writes && Serial.output.find("\"reason\":\"limit_input_active\"") != std::string::npos);
    // Inverted X1 without a recorded mapping: the level is unknown.
    fresh(); qualify(); limitStatus(0, 2); refusedMove(moveBy(20), "input_mapping_unknown", 1);
    // Inactive limits: admitted; connected limit terminals add the 0x0008 witness.
    fresh(); qualify(); limitStatus(0);
    app->configuration.wiring[1] = app->configuration.wiring[2] = InputWiring::CONNECTED;
    auto id = admitMove();
    assert(view(id).moveContext->prerequisites.limitInputMask == 6);
    assert(cancel(app, id) == Probe::Action::OK); pump();
    fresh(); qualify(); limitStatus(0);
    id = admitMove(); assert(view(id).moveContext->prerequisites.limitInputMask == 0); // Undeclared wiring: no witness read.
    assert(cancel(app, id) == Probe::Action::OK); pump();
    // 0x0018 = 1 without this application's binding: unknown, refused.
    fresh(); qualify(); limitStatus(0); coordinates();
    app->configuration.raw.softLimitEnable = 1; app->configuration.softLimitEnableKnown = true;
    refusedMove(moveBy(20), "soft_limit_state_unknown");
    // Bound: an endpoint inside the host soft limits is admitted, outside refused.
    fresh(); qualify(); limitStatus(0); coordinates(); bindSoftLimits(-500, 500);
    assert(softLimitState(*app, nowUs()) == SoftLimitState::BOUND);
    refusedMove(moveTo(700), "soft_limit_endpoint");
    id = admitMove(); assert(cancel(app, id) == Probe::Action::OK); pump();
    // Each transition makes the state unknown and every start refuse.
    for (unsigned transition = 0; transition < 6; ++transition) {
        fresh(); qualify(); limitStatus(0); coordinates(); bindSoftLimits(-500, 500);
        auto& motion = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
        App::Record write; write.kind = App::OperationKind::DRIVER; write.address = 1; write.operationId = 60;
        write.driver.kind = ESS::DriverKind::UPDATE; write.driver.group = ESS::DriverGroup::DRIVE;
        write.driver.request.fields = static_cast<uint32_t>(ESS::DriverField::SOFT_LIMIT_ENABLE);
        if (transition == 0) invalidateAxis(*app); // Position clear, release, a reference loss.
        if (transition == 1) { motion.value.homingComplete = false; motion.value.rawMotion = 1; } // HOMED low.
        if (transition == 2) { // A lost enable ACK: the write's effects, then its uncertain end.
            write.driver.request.softLimitEnable = ESS::SoftLimitEnable::AFTER_HOMING;
            write.driver.state = MotorControlRS::ReadState::FAILED; write.driver.uncertain = true;
            invalidateDriverAssumptions(*app, 1, static_cast<uint32_t>(ESS::DriverField::SOFT_LIMIT_ENABLE), nowUs(), &write);
            noteSoftLimitWrite(*app, write);
            app->configuration.operationId = 120; // Even a later read showing 1 does not bind.
        }
        if (transition == 3) { // A lost disable ACK.
            write.driver.request.softLimitEnable = ESS::SoftLimitEnable::LIMITS_OFF;
            write.driver.state = MotorControlRS::ReadState::FAILED; write.driver.uncertain = true;
            invalidateDriverAssumptions(*app, 1, static_cast<uint32_t>(ESS::DriverField::SOFT_LIMIT_ENABLE), nowUs(), &write);
            noteSoftLimitWrite(*app, write); app->configuration.operationId = 120;
        }
        if (transition == 4) { // A pair change.
            invalidateDriverAssumptions(*app, 1, static_cast<uint32_t>(ESS::DriverField::POSITIVE_LIMIT), nowUs());
            app->configuration.operationId = 120;
        }
        if (transition == 5) app->softLimitBinding.bindingGeneration++; // A rebind.
        assert(softLimitState(*app, nowUs()) == SoftLimitState::UNKNOWN);
        auto r = moveBy(20); r.position.configurationGeneration = app->axis.generation;
        uint32_t untouched = 77;
        assert(host(app).startMove(app, 1, 1, r, untouched) != Probe::Action::OK && untouched == 77 && !hardware.writes);
    }
    // Known disabled: admitted as before.
    fresh(); qualify(); limitStatus(0);
    assert(softLimitState(*app, nowUs()) == SoftLimitState::KNOWN_DISABLED);
    id = admitMove(); assert(cancel(app, id) == Probe::Action::OK); pump();
}
// The enable route takes its prerequisites only from the application's own
// evidence; a confirmed readback installs the binding and the host limits.
void softLimitEnableSetup() {
    fresh(); qualify(); limitStatus(0); coordinates();
    app->identity.rawModel = 0x4EEA; app->identity.rawVersion = 0x0029;
    auto& h = app->softLimitHome; h.target = app->axis.target; h.operationId = 10;
    h.axisGeneration = app->axis.generation; h.bindingGeneration = app->bindingGeneration; h.stated = true;
    auto& d = app->driverSettings; d.operationId = 20; d.target = app->axis.target; d.group = ESS::DriverGroup::DRIVE;
    d.configurationGeneration = app->axis.generation; d.pairKnown = d.limitsDecoded = true;
    d.positiveLimit = 500; d.negativeLimit = -500;
    d.positiveWords[0] = 0; d.positiveWords[1] = 500; d.negativeWords[0] = 0xFFFF; d.negativeWords[1] = 0xFE0C;
}
void softLimitEnablePrepared(bool expected) {
    ESS::DriverPrerequisites p; const MotorControlRS::ReadTarget target = app->axis.target;
    softLimitEnablePrerequisites(*app, p, target, app->axis.generation);
    assert(p.limitSemanticsQualified == expected && p.homedReferenceQualified == expected);
    assert((app->softLimitPending.target.id != 0) == expected);
    if (expected) assert(p.positiveLimit == 500 && p.negativeLimit == -500 && p.qualifiedNegativeWords[0] == 0xFFFF);
}
void testSoftLimitEnableRoute() {
    softLimitEnableSetup(); softLimitEnablePrepared(true);
    softLimitEnableSetup(); app->softLimitHome.stated = false; softLimitEnablePrepared(false);        // No soft-limits home.
    softLimitEnableSetup(); app->softLimitHome.axisGeneration++; softLimitEnablePrepared(false);      // Another generation.
    softLimitEnableSetup(); app->softLimitHome.bindingGeneration++; softLimitEnablePrepared(false);   // Another binding.
    softLimitEnableSetup(); app->driverSettings.operationId = 5; softLimitEnablePrepared(false);      // Pairs read before the home.
    softLimitEnableSetup(); app->driverSettings.limitsDecoded = false; softLimitEnablePrepared(false); // No qualified decoder.
    softLimitEnableSetup(); app->identity.rawVersion = 0x0030; softLimitEnablePrepared(false);        // Another firmware.
    // Confirmed readback: binding and host limits after it, never before.
    softLimitEnableSetup(); softLimitEnablePrepared(true);
    assert(!app->axis.softLimitsKnown);
    app->softLimitPending.operationId = 70;
    App::Record write; write.kind = App::OperationKind::DRIVER; write.address = 1; write.operationId = 70;
    write.driver.kind = ESS::DriverKind::UPDATE; write.driver.group = ESS::DriverGroup::DRIVE;
    write.driver.request.fields = static_cast<uint32_t>(ESS::DriverField::SOFT_LIMIT_ENABLE);
    write.driver.request.softLimitEnable = ESS::SoftLimitEnable::AFTER_HOMING;
    write.driver.state = MotorControlRS::ReadState::SUCCEEDED;
    invalidateDriverAssumptions(*app, 1, static_cast<uint32_t>(ESS::DriverField::SOFT_LIMIT_ENABLE), nowUs(), &write);
    noteSoftLimitWrite(*app, write);
    assert(app->axis.softLimitsKnown && app->axis.softMinimum == -500 && app->axis.softMaximum == 500);
    assert(softLimitState(*app, nowUs()) == SoftLimitState::UNKNOWN); // No configuration read after the write yet.
    app->configuration.operationId = 99; app->configuration.target = app->axis.target;
    app->configuration.raw.softLimitEnable = 1; app->configuration.softLimitEnableKnown = true;
    auto& motion = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    motion.value.homingComplete = true; motion.observedEarliestUs = motion.observedLatestUs = hardware.time;
    assert(softLimitState(*app, nowUs()) == SoftLimitState::BOUND);
    // Image C (run soft_b): the first checked read after our own enable reports
    // 0x0018 changed against the pre-write cache; that read keeps the binding.
    App::Record read; read.kind = App::OperationKind::TYPED_READ; read.address = 1; read.operationId = 100;
    const auto before = app->axis.generation;
    invalidateDriverAssumptions(*app, 1, static_cast<uint32_t>(ESS::DriverField::SOFT_LIMIT_ENABLE), nowUs(), &read);
    assert(app->axis.generation != before && app->axis.softLimitsKnown && app->softLimitBinding.axisGeneration == app->axis.generation);
    app->configuration.operationId = 101; app->configuration.target = app->axis.target;
    motion.observedEarliestUs = motion.observedLatestUs = hardware.time;
    assert(softLimitState(*app, nowUs()) == SoftLimitState::BOUND);
    // Image D (run soft_d): the published configuration read differs from the
    // previous one in 0x0018 only: rebased; any other difference is not.
    ESS::RawConfig previous = app->configuration.raw; previous.softLimitEnable = 0;
    const auto enableOnly = static_cast<uint32_t>(ESS::DriverField::SOFT_LIMIT_ENABLE);
    assert(softLimitReadRebase(*app, previous, app->configuration.raw, enableOnly));
    assert(softLimitReadRebase(*app, previous, app->configuration.raw, 0));
    assert(!softLimitReadRebase(*app, previous, app->configuration.raw, enableOnly | 1u));
    previous.overLimitStop = 1; assert(!softLimitReadRebase(*app, previous, app->configuration.raw, enableOnly));
    previous.overLimitStop = app->configuration.raw.overLimitStop;
    { ESS::RawConfig zero = app->configuration.raw; zero.softLimitEnable = 0;
      assert(!softLimitReadRebase(*app, app->configuration.raw, zero, enableOnly)); }
    // A read that reports any other field changed ends it.
    invalidateDriverAssumptions(*app, 1, static_cast<uint32_t>(ESS::DriverField::SOFT_LIMIT_ENABLE) |
        static_cast<uint32_t>(ESS::DriverField::POSITIVE_LIMIT), nowUs(), &read);
    app->configuration.operationId = 102; app->configuration.target = app->axis.target;
    assert(softLimitState(*app, nowUs()) == SoftLimitState::UNKNOWN);
    softLimitEnableSetup(); softLimitEnablePrepared(true); app->softLimitPending.operationId = 70;
    invalidateDriverAssumptions(*app, 1, static_cast<uint32_t>(ESS::DriverField::SOFT_LIMIT_ENABLE), nowUs(), &write);
    write.operationId = 70; write.driver.request.softLimitEnable = ESS::SoftLimitEnable::AFTER_HOMING;
    noteSoftLimitWrite(*app, write); assert(app->softLimitBinding.operationId == 70);
    write.operationId = 71; write.driver.request.softLimitEnable = ESS::SoftLimitEnable::LIMITS_OFF;
    noteSoftLimitWrite(*app, write);
    assert(!app->softLimitBinding.operationId);
    // A disable write ends the binding whatever its result.
    write.operationId = 71; write.driver.request.softLimitEnable = ESS::SoftLimitEnable::LIMITS_OFF;
    noteSoftLimitWrite(*app, write);
    assert(!app->softLimitBinding.operationId && softLimitState(*app, nowUs()) == SoftLimitState::UNKNOWN);
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--emit-stop-input-results") { emitStopInputResults(); return 0; }
    testQueuedPositionClearInvalidation(); testMonitorReferenceRefusal();
    testLimitInputsAndSoftLimitState(); testSoftLimitEnableRoute();
    testFailedPreparationAndCrossKindSlotReuse();
    testPositionLimitUsesDriveSubdivision();
    testVerificationReadDoesNotAdoptNewGeneration();
    testLocalMoveCancellationInvalidatesRememberedSettings();
    testRepeatedMovePolicies();
    testUserMoveInUnits();
    testProductionMoveOptionalObservationAge();
    testProductionGateAndImmutableAdmission(); testIndependentWritesRespectQualificationAndMoveStartup();
    testStageReservationAndFreshCompletion();
    testStopInputEndsMoveShortWithoutPollingToDeadline();
    testEnableRefusedWhileReleaseInputActive();
    testCommonAndProfileCliUseExactPreparation();
    testWrongStageEchoAndLostTriggerNeverReplay(); testCancellationAndStopAtSequenceBoundaries();
    testFullRetainedResultsStillReserveStop();
    testConsoleResetPreservesUncertainTriggerUnderBackpressure();
    testConsoleStopUnderFullUsbBackpressure();
    testConfigurationChangesCancelContinuations();
    testReadinessBoundsActualOwnerWrites(); testDeferredTriggerExpiresBeforeAdmission();
    testCheckedStagingExceptionRetainsPartialSetupUncertainty();
    testAbsoluteAngleApiCliAndLostReference(); testOriginIsHostOnlyAndConfidenceInvalidates();
    testHostPreferencesPreserveReferenceAge();
    testPositionClearApiCliAndUncertainInvalidation();
    testExternalFeedbackInvalidatesOnlyCurrentTarget();
    testExternalFeedbackDuringStopInvalidatesCoordinates(); testPositionClearBadObservationKeepsConflict();
    std::puts("Actual move application tests passed");
    return 0;
}
