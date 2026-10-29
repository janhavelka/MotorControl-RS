// SPDX-License-Identifier: MIT
// Real owner/application/console with independently scripted SDK wire evidence.
// Every accepting baseline below comes from checked reads, not injected success.
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
#include <cstring>
#include <string>
#include <vector>

namespace {
using namespace MotorControlRS;
const std::vector<uint8_t> READ_START = {1, 3, 0, 0x20, 0, 1, 0x85, 0xC0};
const std::vector<uint8_t> WRITE_MAX = {1, 6, 0, 0x20, 0x0B, 0xB8, 0x8F, 0x42};
const std::vector<uint8_t> WRITE_ZERO = {1, 6, 0, 0x20, 0, 0, 0x88, 0};
const std::vector<uint8_t> UNKNOWN_START = {1, 3, 2, 0xFF, 0xFF, 0xB9, 0xF4};
const std::vector<uint8_t> MAX_START = {1, 3, 2, 0x0B, 0xB8, 0xBF, 0x06};

void step(uint32_t us = 10) { advanceHardware(hardware.time + us); serviceApplication(); }
void pump(unsigned turns = 64) { for (unsigned i = 0; i < turns; ++i) step(); }
void fresh(uint32_t maximumAgeMs = 0) {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = writeResponseConfirmed = false;
    ApplicationOptions options; options.observationMaxAgeMs = maximumAgeMs;
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit, options));
    assert(app && uart.ready() && app->owner.valid() && !hardware.writes);
    hardware.txCharacterUs = 87; assert(uart.startCapture(20, timing().holdUs));
    // This scripted fixture explicitly excludes local FC06 echo. It does not
    // supply physical timing or active start-speed qualification for the bench.
    writeResponseConfirmed = true;
}
std::vector<uint8_t> crc(std::vector<uint8_t> bytes) {
    uint16_t value = 0xFFFF;
    for (const auto byte : bytes) {
        value ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit) value = value & 1 ? (value >> 1) ^ 0xA001 : value >> 1;
    }
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8)); return bytes;
}
std::vector<uint8_t> words(std::initializer_list<uint16_t> values) {
    std::vector<uint8_t> bytes = {1, 3, static_cast<uint8_t>(values.size() * 2)};
    for (const auto value : values) { bytes.push_back(static_cast<uint8_t>(value >> 8)); bytes.push_back(static_cast<uint8_t>(value)); }
    return crc(bytes);
}
Probe::ResultView view(uint32_t id) {
    Probe::ResultView result; assert(host(app).result(app, id, result)); return result;
}
void command(const std::string& line) {
    Serial.input = line; Serial.output.clear();
    for (unsigned i = 0; i < 500 && !Serial.input.empty(); ++i) step();
    assert(Serial.input.empty()); pump();
}
void waitTx(uint32_t id, const std::vector<uint8_t>& expected = {}) {
    auto* record = findRecord(*app, id); assert(record);
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(record->requestId); ++i) step();
    assert(app->owner.txAccepted(record->requestId));
    if (!expected.empty()) assert(hardware.tx == expected);
}
void schedule(const std::vector<uint8_t>& response) {
    scheduleReply(std::max(hardware.writeStarted + hardware.tx.size() * 87 + 1000, hardware.time + 1000), response);
}
void readReply(uint32_t id, const std::vector<uint8_t>& response) {
    const uint8_t token = view(id).typedRead->step; waitTx(id); schedule(response);
    for (unsigned i = 0; i < 25000 && view(id).pending && view(id).typedRead->step == token; ++i) step();
    assert(!view(id).pending || view(id).typedRead->step != token);
}
void baseline() {
    command("@1 read config\n"); const auto configuration = app->latestOperationId;
    assert(configuration && view(configuration).typedRead);
    readReply(configuration, words({0, 1000})); readReply(configuration, words({0, 0, 0}));
    readReply(configuration, words({0, 0, 0})); readReply(configuration, words({0, 0, 0, 0, 0}));
    readReply(configuration, words({3, 4000})); pump();
    assert(app->configuration.operationId == configuration && view(configuration).typedRead->state == ReadState::SUCCEEDED);
    ESS::DriverContext wrongKind; wrongKind.operationId = 987;
    assert(!copySettingsResult(configuration, wrongKind) && wrongKind.operationId == 987);
    assert(releaseOperation(configuration));
    command("@2 read state\n"); const auto state = app->latestOperationId;
    readReply(state, words({0, 1})); readReply(state, words({0, 0})); readReply(state, words({0, 0, 0}));
    pump(); assert(view(state).typedRead->state == ReadState::SUCCEEDED && releaseOperation(state));
    assert(positionSettingPolicy(*app, hardware.time));
}
ESS::DriverContext setting(uint32_t id) {
    ESS::DriverContext result; assert(copySettingsResult(id, result)); return result;
}
uint32_t submit(uint32_t native = 3000) {
    uint32_t id = 0; assert(submitPositionStartSpeed(native, id));
    assert(id && view(id).pending && axisReserved(*app, 1) && !releaseOperation(id)); return id;
}
void settingReply(uint32_t id, const std::vector<uint8_t>& expected, const std::vector<uint8_t>& response) {
    const auto before = setting(id); waitTx(id, expected); schedule(response);
    for (unsigned i = 0; i < 25000; ++i) {
        step(); const auto& current = findRecord(*app, id)->driver;
        if (current.state != ReadState::ACTIVE || current.kind != before.kind || current.step != before.step) break;
    }
    const auto after = setting(id);
    assert(after.state != ReadState::ACTIVE || after.kind != before.kind || after.step != before.step);
}
void finish(uint32_t id, bool maximum = true, uint16_t previous = 0xFFFF) {
    const uint64_t deadline = setting(id).deadlineUs;
    settingReply(id, READ_START, previous == 0xFFFF ? UNKNOWN_START : words({previous}));
    assert(setting(id).kind == ESS::DriverKind::UPDATE && setting(id).deadlineUs == deadline);
    settingReply(id, maximum ? WRITE_MAX : WRITE_ZERO, maximum ? WRITE_MAX : WRITE_ZERO);
    settingReply(id, READ_START, maximum ? MAX_START : words({0})); pump();
    const auto result = setting(id);
    assert(result.state == ReadState::SUCCEEDED && !result.uncertain && result.effects == 1 && result.deadlineUs == deadline);
    assert(result.prerequisites.previous.raw[0] == previous && result.progress[0].previous == previous);
    assert(result.progress[0].acknowledged && result.progress[0].readbackKnown && !result.progress[0].activeKnown);
    assert(result.progress[0].readback == (maximum ? 3000 : 0) && !axisReserved(*app, 1));
}
void testAdmissionAndCapacity() {
    fresh(); uint32_t unchanged = 77;
    assert(!submitPositionStartSpeed(0, unchanged) && unchanged == 77 && !hardware.writes);
    baseline(); const auto count = hardware.writes;
    app->motionProfile.view.restoreUnsettled = true;
    assert(!app->motionProfile.view.pending && !app->motionProfile.request.owner);
    const auto unsettled = submitPositionStartSpeed(0, unchanged);
    assert(!unsettled && unsettled.detail == static_cast<int32_t>(Probe::Action::BUSY) && unchanged == 77 && hardware.writes == count);
    app->motionProfile.view.restoreUnsettled = false;
    for (uint32_t bad : {3001u, 65535u, 65536u, UINT32_MAX})
        assert(!submitPositionStartSpeed(bad, unchanged) && unchanged == 77 && hardware.writes == count);
    for (unsigned gate = 0; gate < 10; ++gate) {
        fresh(); baseline(); const auto before = hardware.writes;
        if (gate == 0) app->configuration.provenance[0].qualified = false;
        else if (gate == 1) app->configuration.target.generation++;
        else if (gate == 2) app->stateCache.blocks[0].value.rawMotion = 4;
        else if (gate == 3) app->stateCache.blocks[0].value.rawAlarm = 1;
        else if (gate == 4) app->stateCache.blocks[1].value.unknownInputBits = 16;
        else if (gate == 5) app->stateCache.blocks[1].value.rawInputs = 1;
        else if (gate == 6) app->stateCache.blocks[2].value.rawSpeed = 1;
        else if (gate == 7) app->configuration.raw.inputFunctions[0] = 4;
        else if (gate == 8) { app->configuration.raw.inputFunctions[0] = 1; app->inputWiring[0] = InputWiring::UNKNOWN; }
        else app->knownTargets[0] &= ~2u;
        assert(!submitPositionStartSpeed(3000, unchanged) && unchanged == 77);
        pump(); assert(hardware.writes == before && !app->owner.active() && !app->owner.pending());
    }
    fresh(1000); baseline(); advanceHardware(hardware.time + 1000001);
    const auto before = hardware.writes;
    assert(!submitPositionStartSpeed(0, unchanged) && unchanged == 77 && hardware.writes == before);
    fresh(); baseline();
    ESS::DriverRequest stale; assert(ESS::preparePositionStartSpeed(stale, 0));
    stale.configurationGeneration = app->axis.generation + 1;
    assert(startDriver(app, 3, 1, ESS::DriverKind::UPDATE, stale, unchanged) != Probe::Action::OK && unchanged == 77);
    stale.configurationGeneration = app->axis.generation;
    assert(startDriver(app, 3, 2, ESS::DriverKind::UPDATE, stale, unchanged) != Probe::Action::OK && unchanged == 77);
    const auto active = submit();
    assert(!submitPositionStartSpeed(0, unchanged) && unchanged == 77);
    assert(cancel(app, active) == Probe::Action::OK); pump(); assert(releaseOperation(active));

    fresh(); baseline(); uint32_t retained[REQUEST_CAPACITY] = {};
    const auto baselineWrites = hardware.writes;
    for (auto& id : retained) {
        id = submit(); assert(cancel(app, id) == Probe::Action::OK); pump();
        assert(setting(id).outcome == ESS::DriverOutcome::CANCELLED && !setting(id).effects);
    }
    const auto refused = submitPositionStartSpeed(0, unchanged);
    assert(!refused && refused.detail == static_cast<int32_t>(Probe::Action::RESULTS_FULL) && unchanged == 77);
    assert(hardware.writes == baselineWrites);
    for (const auto id : retained) assert(setting(id).state == ReadState::FAILED && releaseOperation(id));
}
void testSuccessEvidenceAndEffects() {
    for (const bool maximum : {false, true}) {
        fresh(); baseline(); const auto writes = hardware.writes;
        const auto configuration = app->configuration;
        const auto desired = app->simple.desired; const auto generation = app->axis.generation;
        app->axis.originKnown = app->axis.softLimitsKnown = true; app->axis.originNative = 123;
        app->axis.softMinimum = -1000; app->axis.softMaximum = 1000;
        app->coordinateReference.nativeKnown = true; app->coordinateReference.target = app->axis.target;
        app->coordinateReference.configurationGeneration = generation; app->coordinateReference.observedUs = hardware.time;
        const auto units = app->axis.units; app->movePrerequisites.readinessQualified = true;
        app->rememberedMoveGeneration = generation;
        app->motionProfile.view.saved = app->motionProfile.view.ok = true;
        app->motionProfile.view.generation = generation; app->motionProfile.view.original[0] = 30;
        const auto id = submit(maximum ? 3000 : 0);
        settingReply(id, READ_START, UNKNOWN_START);
        assert(app->axis.generation == generation && app->coordinateReference.nativeKnown);
        waitTx(id, maximum ? WRITE_MAX : WRITE_ZERO); pump();
        assert(app->axis.generation == generation + 1 && !app->coordinateReference.nativeKnown);
        assert(app->axis.originKnown && app->axis.originNative == 123 && app->axis.softLimitsKnown &&
            app->axis.softMinimum == -1000 && app->axis.softMaximum == 1000);
        assert(!std::memcmp(&app->axis.units, &units, sizeof(units)));
        assert(!app->movePrerequisites.readinessQualified && !app->rememberedMoveGeneration);
        assert(!app->configuration.operationId && configuration.operationId &&
            !std::memcmp(&app->configuration.raw, &configuration.raw, sizeof(configuration.raw)) &&
            !std::memcmp(app->configuration.provenance, configuration.provenance, sizeof(configuration.provenance)));
        assert(app->motionProfile.view.saved && app->motionProfile.view.original[0] == 30 &&
            app->motionProfile.view.generation == generation);
        // Complete the already transmitted write; this is not a second attempt.
        settingReply(id, maximum ? WRITE_MAX : WRITE_ZERO, maximum ? WRITE_MAX : WRITE_ZERO);
        settingReply(id, READ_START, maximum ? MAX_START : words({0})); pump();
        const auto result = setting(id);
        assert(result.state == ReadState::SUCCEEDED && result.configurationGeneration == generation && !result.uncertain);
        assert(result.prerequisites.previous.raw[0] == 0xFFFF && result.progress[0].previous == 0xFFFF);
        assert(result.observations[0].write && !result.observations[1].write && !result.progress[0].activeKnown);
        assert(hardware.writes == writes + 3 && app->simple.desired.speedRpm == desired.speedRpm &&
            app->simple.desired.acceleration == desired.acceleration && app->simple.desired.deceleration == desired.deceleration);
        uint32_t move = 99; assert(!moveBy(Rational(10), PositionUnit::STEPS, move) && move == 99);
        command("status\n"); pump(); ESS::DriverContext retained; assert(copySettingsResult(id, retained));
        assert(releaseOperation(id)); ESS::DriverContext old; old.operationId = 987;
        assert(!copySettingsResult(id, old) && old.operationId == 987 && !releaseOperation(id));
    }
}
void testFailuresAndChangedConfiguration() {
    for (unsigned failure = 0; failure < 7; ++failure) {
        fresh(); baseline(); const auto count = hardware.writes; const auto generation = app->axis.generation;
        const auto id = submit(); const auto deadline = setting(id).deadlineUs;
        if (failure == 0) {
            settingReply(id, READ_START, crc({1, 0x83, 2}));
            assert(setting(id).kind == ESS::DriverKind::READ && !setting(id).effects && hardware.writes == count + 1);
        } else if (failure == 1 || failure == 2) {
            waitTx(id, READ_START);
            if (failure == 1) ++app->axis.generation; else ++app->bindingGeneration;
            settingReply(id, READ_START, UNKNOWN_START);
            assert(setting(id).kind == ESS::DriverKind::READ && setting(id).state == ReadState::FAILED &&
                !setting(id).effects && hardware.writes == count + 1);
        } else {
            settingReply(id, READ_START, UNKNOWN_START);
            if (failure == 3) settingReply(id, WRITE_MAX, crc({1, 0x86, 3}));
            else if (failure == 4) {
                waitTx(id, WRITE_MAX);
                for (unsigned i = 0; i < 5000 && view(id).pending; ++i) step(1000);
                assert(!view(id).pending && setting(id).uncertain && hardware.writes == count + 2);
                const auto saved = setting(id); schedule(WRITE_MAX); pump(1000);
                assert(setting(id).outcome == saved.outcome && setting(id).progress[0].acknowledged == saved.progress[0].acknowledged);
            } else {
                settingReply(id, WRITE_MAX, WRITE_MAX);
                if (failure == 5) settingReply(id, READ_START, words({2999}));
                else { assert(cancel(app, id) == Probe::Action::OK); pump(1000); }
            }
            const auto result = setting(id);
            assert(result.kind == ESS::DriverKind::UPDATE && result.effects == 1 && result.state == ReadState::FAILED);
            assert(app->axis.generation == generation + 1 && !app->movePrerequisites.readinessQualified);
            assert(result.prerequisites.previous.raw[0] == 0xFFFF && result.progress[0].requested == 3000);
            if (failure == 3) assert(result.progress[0].execution == ActionExecution::REJECTED && !result.uncertain);
            if (failure == 5) assert(result.outcome == ESS::DriverOutcome::READBACK_MISMATCH && result.progress[0].readback == 2999);
        }
        pump(); assert(setting(id).deadlineUs == deadline && setting(id).state == ReadState::FAILED);
        const auto transmissions = hardware.writes; pump(1000); assert(hardware.writes == transmissions);
        assert(releaseOperation(id));
    }
}
void testConsoleAndRefreshedMoveRelationship() {
    fresh(); baseline(); const auto count = hardware.writes;
    for (const char* invalid : {"@10 motion-profile start-speed", "@11 motion-profile start-speed -1",
        "@12 motion-profile start-speed 0.5", "@13 motion-profile start-speed 3001",
        "@14 motion-profile start-speed 0 extra", "@15 motion-profile start-speed 65536"}) {
        command(std::string(invalid) + "\n");
        assert(Serial.output.find("\"ok\":false") != std::string::npos && hardware.writes == count);
    }
    command("@16 motion-profile start-speed 3000\n"); const auto id = app->latestOperationId;
    if (!view(id).pending || findRecord(*app, id)->programmaticSettings || Serial.output.find("\"result\":\"accepted\"") == std::string::npos)
        std::fprintf(stderr, "Start-speed CLI result: %s\n", Serial.output.c_str());
    assert(view(id).pending && !findRecord(*app, id)->programmaticSettings &&
        Serial.output.find("\"result\":\"accepted\"") != std::string::npos);
    finish(id); assert(hardware.writes == count + 3);
    assert(Serial.output.find("\"driver_group\":\"position_start_speed\"") != std::string::npos);
    command("@17 result " + std::to_string(id) + "\n");
    assert(Serial.output.find("\"active_settings_known\":false") != std::string::npos);
    assert(releaseOperation(id)); command("@18 result " + std::to_string(id) + "\n");
    assert(Serial.output.find("\"ok\":false") != std::string::npos);
    baseline();
    Probe::MotionProfileView profile;
    assert(motionProfileCommand(app, Probe::MotionProfileCommand::SNAPSHOT, profile) == Probe::Action::OK);
    const auto request = app->motionProfile.request;
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(request); ++i) step();
    assert(app->owner.txAccepted(request)); schedule(words({3000, 100, 100, 60, 0, 7}));
    for (unsigned i = 0; i < 25000 && app->motionProfile.view.pending; ++i) step();
    assert(app->motionProfile.view.ok); const auto writes = hardware.writes;
    uint32_t untouched = 77;
    assert(!moveBy(Rational(10), PositionUnit::STEPS, untouched, 60) && untouched == 77 && hardware.writes == writes);
    assert(app->simple.desired.speedRpm == 60 && app->motionProfile.view.current[0] == 3000);
}
void testCompatibleMoveAndHumanTerminal() {
    fresh(); baseline(); const auto id = submit(30);
    const auto write30 = crc({1, 6, 0, 0x20, 0, 30});
    settingReply(id, READ_START, UNKNOWN_START); settingReply(id, write30, write30);
    settingReply(id, READ_START, words({30})); pump();
    uint32_t move = 77; const auto before = hardware.writes;
    assert(!moveBy(Rational(10), PositionUnit::STEPS, move, 60) && move == 77 && hardware.writes == before);
    baseline(); // Setter readback cannot stand in for rebuilt move prerequisites.
    Probe::MotionProfileView profile;
    assert(motionProfileCommand(app, Probe::MotionProfileCommand::SNAPSHOT, profile) == Probe::Action::OK);
    uint32_t untouched = 99;
    assert(!submitPositionStartSpeed(0, untouched) && untouched == 99); // Snapshot owns the bus until harvest.
    const auto request = app->motionProfile.request;
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(request); ++i) step();
    assert(app->owner.txAccepted(request)); schedule(words({30, 100, 100, 60, 0, 7}));
    for (unsigned i = 0; i < 25000 && app->motionProfile.view.pending; ++i) step();
    assert(app->motionProfile.view.ok && moveBy(Rational(10), PositionUnit::STEPS, move, 60));
    assert(view(move).moveContext->prerequisites.startSpeedKnown && view(move).moveContext->prerequisites.startSpeed == 30);
    waitTx(move);
    const auto staged = crc({1, 16, 0, 0x21, 0, 5, 10, 0, 100, 0, 100, 0, 60, 0, 0, 0, 10});
    assert(hardware.tx == staged); const auto writes = hardware.writes;
    schedule(crc({1, 16, 0, 0x21, 0, 5}));
    for (unsigned i = 0; i < 25000 && view(move).moveContext->step == 0; ++i) step();
    assert(view(move).moveContext->step == 1 && cancel(app, move) == Probe::Action::OK);
    pump(1000); assert(!view(move).pending && hardware.writes == writes); // No trigger follows local cancellation.
    assert(releaseMove(move) && releaseOperation(id));

    fresh(); baseline(); command("motion-profile start-speed 0\n"); const auto human = app->latestOperationId;
    assert(Serial.output.find("Position start-speed setting accepted") != std::string::npos);
    finish(human, false);
    assert(Serial.output.find("Position start-speed stored readback confirmed") != std::string::npos);
    assert(Serial.output.find("0 native") != std::string::npos && Serial.output.find("65535 native") != std::string::npos);
    assert(Serial.output.find("Physical scaling, active behavior and persistence remain unqualified") != std::string::npos);
    assert(Serial.output.find(" rpm") == std::string::npos && releaseOperation(human));
}
void testPriorCompletionCannotRenewOrRequalifySession() {
    for (unsigned scenario = 0; scenario < 3; ++scenario) {
        fresh(scenario == 1 ? 1000 : 0); baseline();
        const auto writes = hardware.writes; const auto id = submit();
        const auto admitted = setting(id); const auto request = findRecord(*app, id)->requestId;
        waitTx(id, READ_START); schedule(UNKNOWN_START);
        // Close the real transaction on time, deliberately withholding its
        // application harvest. Delivery cannot renew the admitted write budget.
        for (unsigned i = 0; i < 25000 && !app->owner.result(request); ++i) {
            advanceHardware(hardware.time + 10); app->owner.service(uart.sample());
        }
        const auto* completed = app->owner.result(request);
        assert(completed && completed->outcome == Rtu::Outcome::SUCCESS &&
            completed->transport.closureQualified && completed->transport.closureLatestUs <= admitted.deadlineUs);
        assert(setting(id).state == ReadState::ACTIVE && !setting(id).completedSteps);
        const auto closedAt = completed->transport.closureLatestUs;
        if (scenario == 0) advanceHardware(admitted.deadlineUs + 1);
        else if (scenario == 1) advanceHardware(hardware.time + 1000001); // State/input policy expires before the session deadline.
        else ++app->axis.generation;
        serviceApplication(); pump();
        const auto result = setting(id); const auto& prior = result.observations[0];
        assert(result.kind == ESS::DriverKind::READ && result.state == ReadState::FAILED &&
            result.outcome == ESS::DriverOutcome::UPDATE_NOT_ADMITTED && result.completedSteps == 1);
        assert(result.deadlineUs == admitted.deadlineUs && result.startedUs == admitted.startedUs && !result.effects && !result.uncertain);
        assert(prior.status && prior.qualified && prior.responseConfirmed && prior.txComplete && !prior.write &&
            prior.reg == 0x0020 && prior.count == 1 && prior.latestUs == closedAt && prior.length == UNKNOWN_START.size());
        assert(!std::memcmp(prior.raw, UNKNOWN_START.data(), UNKNOWN_START.size()));
        assert(hardware.writes == writes + 1 && hardware.tx == READ_START && !app->owner.active() && !app->owner.pending());
        if (scenario == 0) assert(prior.deliveredUs > result.deadlineUs);
        else assert(prior.deliveredUs < result.deadlineUs);
        assert(releaseOperation(id));
    }
}
void testLegacyRestoreSettlesOnlyItsFiveWrittenWords() {
    fresh(); baseline();
    const auto profileReply = [](const std::vector<uint8_t>& expected, const std::vector<uint8_t>& response) {
        const auto phase = app->motionProfile.view.phase; const auto request = app->motionProfile.request;
        for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(request); ++i) step();
        assert(app->owner.txAccepted(request) && hardware.tx == expected); schedule(response);
        for (unsigned i = 0; i < 25000 && app->motionProfile.view.pending && app->motionProfile.view.phase == phase; ++i) step();
        assert(!app->motionProfile.view.pending || app->motionProfile.view.phase != phase);
    };
    const auto readProfile = crc({1, 3, 0, 0x20, 0, 6});
    command("@40 motion-profile read\n"); assert(app->motionProfile.view.pending);
    profileReply(readProfile, words({30, 100, 200, 60, 1, 0x2345}));
    assert(app->motionProfile.view.saved && app->motionProfile.view.ok && app->motionProfile.view.original[0] == 30);
    const auto id = submit(); finish(id, true, 30); baseline();
    const auto before = hardware.writes;
    command("@41 motion-profile restore\n");
    assert(app->motionProfile.view.pending && app->motionProfile.view.phase == Probe::MotionProfilePhase::RESTORE);
    const auto tailWrite = crc({1, 16, 0, 0x21, 0, 5, 10, 0, 100, 0, 200, 0, 60, 0, 1, 0x23, 0x45});
    profileReply(tailWrite, crc({1, 16, 0, 0x21, 0, 5}));
    assert(app->motionProfile.view.restoreUnsettled && app->motionProfile.view.pending);
    profileReply(readProfile, words({3000, 100, 200, 60, 1, 0x2345})); pump();
    const auto& restored = app->motionProfile.view;
    assert(restored.ok && restored.restored && !restored.pending && !restored.restoreUnsettled);
    assert(restored.original[0] == 30 && restored.current[0] == 3000 &&
        !std::memcmp(restored.original + 1, restored.current + 1, 5 * sizeof(uint16_t)));
    assert(hardware.writes == before + 2); // Restore never writes 0x0020 or triggers movement.
    command("@42 motion-profile forget\n");
    assert(!app->motionProfile.view.saved && !app->motionProfile.view.restoreUnsettled && hardware.writes == before + 2);
    assert(setting(id).progress[0].requested == 3000 && releaseOperation(id));
}
void terminalFixtures() {
    for (const char* name : {"success", "prior_exception", "readback_mismatch", "lost_ack", "cancelled", "changed_generation"}) {
        fresh(); baseline(); command("@30 motion-profile start-speed 3000\n"); const auto id = app->latestOperationId;
        assert(view(id).pending);
        if (!std::strcmp(name, "prior_exception")) settingReply(id, READ_START, crc({1, 0x83, 2}));
        else if (!std::strcmp(name, "changed_generation")) {
            waitTx(id, READ_START); ++app->axis.generation; settingReply(id, READ_START, UNKNOWN_START);
        } else {
            settingReply(id, READ_START, UNKNOWN_START);
            if (!std::strcmp(name, "lost_ack")) {
                waitTx(id, WRITE_MAX);
                for (unsigned i = 0; i < 5000 && view(id).pending; ++i) step(1000);
                assert(!view(id).pending && setting(id).uncertain);
            } else {
                settingReply(id, WRITE_MAX, WRITE_MAX);
                if (!std::strcmp(name, "cancelled")) { assert(cancel(app, id) == Probe::Action::OK); pump(); }
                else settingReply(id, READ_START, !std::strcmp(name, "readback_mismatch") ? words({2999}) : MAX_START);
            }
        }
        pump(1000); assert(!view(id).pending);
        const auto begin = Serial.output.find("{\"type\":\"motion-profile-start-speed\"");
        assert(begin != std::string::npos); const auto end = Serial.output.find('\n', begin);
        assert(end != std::string::npos);
        const auto record = Serial.output.substr(begin, end - begin);
        std::printf("{\"case\":\"%s\",\"record\":%s}\n", name, record.c_str());
        assert(releaseOperation(id));
    }
}
} // namespace
int main(int argc, char** argv) {
    if (argc == 2 && !std::strcmp(argv[1], "--start-speed-fixtures")) { terminalFixtures(); return 0; }
    testAdmissionAndCapacity(); testSuccessEvidenceAndEffects();
    testFailuresAndChangedConfiguration(); testConsoleAndRefreshedMoveRelationship();
    testCompatibleMoveAndHumanTerminal(); testPriorCompletionCannotRenewOrRequalifySession();
    testLegacyRestoreSettlesOnlyItsFiveWrittenWords();
    if (app) { app->~App(); std::free(app); app = nullptr; }
}
