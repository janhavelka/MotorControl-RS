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
const std::vector<uint8_t> READ_AUX = {1, 3, 0, 0x30, 0, 1, 0x84, 0x05};
const std::vector<uint8_t> WRITE_SEVEN = {1, 6, 0, 0x30, 0, 7, 0xC8, 0x07};
const std::vector<uint8_t> UNKNOWN_AUX = {1, 3, 2, 0xFF, 0xFF, 0xB9, 0xF4};
const std::vector<uint8_t> SEVEN_AUX = {1, 3, 2, 0, 7, 0xF9, 0x86};

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
    // supply physical timing or active auxiliary-policy qualification for the bench.
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
    assert(singleSettingPolicy(*app, hardware.time));
}
ESS::DriverContext setting(uint32_t id) {
    ESS::DriverContext result; assert(copySettingsResult(id, result)); return result;
}
uint32_t submit(uint16_t raw = 7) {
    uint32_t id = 0; assert(submitHomingAuxiliary(static_cast<ESS::HomingAuxiliary>(raw), id));
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
std::vector<uint8_t> writeChoice(uint16_t raw) {
    return crc({1, 6, 0, 0x30, 0, static_cast<uint8_t>(raw)});
}
void finish(uint32_t id, uint16_t raw = 7, uint16_t previous = 0xFFFF) {
    const uint64_t deadline = setting(id).deadlineUs;
    settingReply(id, READ_AUX, previous == 0xFFFF ? UNKNOWN_AUX : words({previous}));
    assert(setting(id).kind == ESS::DriverKind::UPDATE && setting(id).deadlineUs == deadline);
    const auto write = writeChoice(raw); settingReply(id, write, write);
    settingReply(id, READ_AUX, words({raw})); pump();
    const auto result = setting(id);
    assert(result.state == ReadState::SUCCEEDED && !result.uncertain && result.effects == 1 && result.deadlineUs == deadline);
    assert(result.prerequisites.previous.raw[0] == previous && result.progress[0].previous == previous);
    assert(result.progress[0].acknowledged && result.progress[0].readbackKnown && !result.progress[0].activeKnown);
    assert(result.progress[0].readback == raw && !axisReserved(*app, 1));
}
void testAdmissionAndCapacity() {
    fresh(); uint32_t unchanged = 77;
    assert(!submitHomingAuxiliary(ESS::HomingAuxiliary::KEEP_POSITION_SET_ZERO, unchanged) && unchanged == 77 && !hardware.writes);
    baseline(); const auto count = hardware.writes;
    for (uint16_t bad : {uint16_t(12), uint16_t(0xFFFF)})
        assert(!submitHomingAuxiliary(static_cast<ESS::HomingAuxiliary>(bad), unchanged) && unchanged == 77 && hardware.writes == count);
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
        assert(!submitHomingAuxiliary(ESS::HomingAuxiliary::KEEP_POSITION_SET_ZERO, unchanged) && unchanged == 77);
        pump(); assert(hardware.writes == before && !app->owner.active() && !app->owner.pending());
    }
    fresh(1000); baseline(); advanceHardware(hardware.time + 1000001);
    const auto before = hardware.writes;
    assert(!submitHomingAuxiliary(ESS::HomingAuxiliary::KEEP_POSITION_SET_ZERO, unchanged) && unchanged == 77 && hardware.writes == before);
    fresh(); baseline(); ESS::DriverRequest stale;
    assert(ESS::prepareHomingAuxiliary(stale, ESS::HomingAuxiliary::KEEP_POSITION_SET_ZERO));
    stale.configurationGeneration = app->axis.generation + 1;
    assert(startDriver(app, 3, 1, ESS::DriverKind::UPDATE, stale, unchanged) != Probe::Action::OK && unchanged == 77);
    stale.configurationGeneration = app->axis.generation;
    assert(startDriver(app, 3, 2, ESS::DriverKind::UPDATE, stale, unchanged) != Probe::Action::OK && unchanged == 77);
    const auto id = submit();
    assert(!submitHomingAuxiliary(ESS::HomingAuxiliary::KEEP_POSITION_SET_ZERO, unchanged) && unchanged == 77);
    assert(cancel(app, id) == Probe::Action::OK); pump(); assert(releaseOperation(id));
    fresh(); baseline(); uint32_t retained[REQUEST_CAPACITY] = {}; const auto writes = hardware.writes;
    for (auto& retainedId : retained) {
        retainedId = submit(); assert(cancel(app, retainedId) == Probe::Action::OK); pump();
        assert(setting(retainedId).outcome == ESS::DriverOutcome::CANCELLED && !setting(retainedId).effects);
    }
    const auto refused = submitHomingAuxiliary(ESS::HomingAuxiliary::KEEP_POSITION_SET_ZERO, unchanged);
    assert(!refused && refused.detail == static_cast<int32_t>(Probe::Action::RESULTS_FULL) && unchanged == 77 && hardware.writes == writes);
    for (const auto retainedId : retained) assert(releaseOperation(retainedId));
}
void testAllChoicesAndPossibleEffects() {
    for (uint16_t raw = 0; raw < 12; ++raw) {
        fresh(); baseline(); const auto count = hardware.writes, generation = app->axis.generation;
        const auto desired = app->simple.desired; const auto config = app->configuration;
        app->axis.originKnown = app->axis.softLimitsKnown = true; app->axis.originNative = 123;
        app->axis.softMinimum = -1000; app->axis.softMaximum = 1000;
        const auto units = app->axis.units; app->coordinateReference.nativeKnown = true;
        app->homePrerequisites.auxiliaryQualified = app->homePrerequisites.zeroOffsetQualified =
            app->homePrerequisites.readinessQualified = true;
        const auto id = submit(raw); settingReply(id, READ_AUX, UNKNOWN_AUX);
        assert(app->axis.generation == generation && app->homePrerequisites.auxiliaryQualified);
        const auto write = writeChoice(raw); waitTx(id, write); pump();
        assert(app->axis.generation == generation + 1 && !app->homePrerequisites.auxiliaryQualified &&
            !app->homePrerequisites.zeroOffsetQualified && !app->homePrerequisites.readinessQualified && !app->coordinateReference.nativeKnown);
        assert(app->axis.originKnown && app->axis.originNative == 123 && app->axis.softLimitsKnown &&
            app->axis.softMinimum == -1000 && app->axis.softMaximum == 1000 && !std::memcmp(&app->axis.units, &units, sizeof(units)));
        assert(!app->configuration.operationId && !std::memcmp(&app->configuration.raw, &config.raw, sizeof(config.raw)));
        settingReply(id, write, write); settingReply(id, READ_AUX, words({raw})); pump();
        const auto result = setting(id);
        assert(result.configurationGeneration == generation && result.prerequisites.previous.raw[0] == 0xFFFF && result.progress[0].requested == raw);
        assert(result.state == ReadState::SUCCEEDED && !result.progress[0].activeKnown && !result.uncertain && result.effects == 1);
        assert(hardware.writes == count + 3 && app->simple.desired.speedRpm == desired.speedRpm &&
            app->simple.desired.acceleration == desired.acceleration && app->simple.desired.deceleration == desired.deceleration);
        assert(!app->homePrerequisites.auxiliaryQualified && !app->homePrerequisites.zeroOffsetQualified);
        const auto copied = result; command("status\n"); pump();
        assert(setting(id).observations[0].raw[3] == 0x30 && copied.observations[1].raw[4] == raw);
        assert(releaseOperation(id)); ESS::DriverContext missing; missing.operationId = 987;
        assert(!copySettingsResult(id, missing) && missing.operationId == 987 && !releaseOperation(id));
    }
}
void testFailuresAndChangedConfiguration() {
    for (unsigned failure = 0; failure < 7; ++failure) {
        fresh(); baseline(); const auto count = hardware.writes, generation = app->axis.generation;
        const auto id = submit(); const auto deadline = setting(id).deadlineUs;
        if (failure == 0) {
            settingReply(id, READ_AUX, crc({1, 0x83, 2}));
            assert(setting(id).kind == ESS::DriverKind::READ && !setting(id).effects && hardware.writes == count + 1);
        } else if (failure == 1 || failure == 2) {
            waitTx(id, READ_AUX); if (failure == 1) ++app->axis.generation; else ++app->bindingGeneration;
            settingReply(id, READ_AUX, UNKNOWN_AUX);
            assert(setting(id).kind == ESS::DriverKind::READ && setting(id).state == ReadState::FAILED && !setting(id).effects && hardware.writes == count + 1);
        } else {
            settingReply(id, READ_AUX, UNKNOWN_AUX);
            if (failure == 3) settingReply(id, WRITE_SEVEN, crc({1, 0x86, 3}));
            else if (failure == 4) {
                waitTx(id, WRITE_SEVEN);
                for (unsigned i = 0; i < 5000 && view(id).pending; ++i) step(1000);
                assert(!view(id).pending && setting(id).uncertain && hardware.writes == count + 2);
                const auto saved = setting(id); schedule(WRITE_SEVEN); pump(1000);
                assert(setting(id).outcome == saved.outcome && !setting(id).progress[0].acknowledged);
            } else {
                settingReply(id, WRITE_SEVEN, WRITE_SEVEN);
                if (failure == 5) settingReply(id, READ_AUX, words({6}));
                else { assert(cancel(app, id) == Probe::Action::OK); pump(1000); }
            }
            const auto result = setting(id);
            assert(result.kind == ESS::DriverKind::UPDATE && result.effects == 1 && result.state == ReadState::FAILED);
            assert(app->axis.generation == generation + 1 && !app->homePrerequisites.auxiliaryQualified);
            assert(result.prerequisites.previous.raw[0] == 0xFFFF && result.progress[0].requested == 7);
            if (failure == 3) assert(result.progress[0].execution == ActionExecution::REJECTED && !result.uncertain);
            if (failure == 5) assert(result.outcome == ESS::DriverOutcome::READBACK_MISMATCH && result.progress[0].readback == 6);
        }
        pump(); assert(setting(id).deadlineUs == deadline && setting(id).state == ReadState::FAILED);
        const auto transmissions = hardware.writes; pump(1000); assert(hardware.writes == transmissions && releaseOperation(id));
    }
}
void testConsoleAndHomeQualificationBoundary() {
    fresh(); baseline(); const auto count = hardware.writes;
    for (const char* invalid : {"@10 home auxiliary", "@11 home auxiliary -1", "@12 home auxiliary 0.5",
        "@13 home auxiliary 12", "@14 home auxiliary 7 extra", "@15 home auxiliary 65535", "@16 home auxiliary 65536"}) {
        command(std::string(invalid) + "\n"); assert(Serial.output.find("\"ok\":false") != std::string::npos && hardware.writes == count);
    }
    command("@17 home auxiliary 7\n"); const auto id = app->latestOperationId;
    assert(view(id).pending && !findRecord(*app, id)->programmaticSettings && Serial.output.find("\"result\":\"accepted\"") != std::string::npos);
    finish(id); assert(hardware.writes == count + 3 && Serial.output.find("\"driver_group\":\"homing_auxiliary\"") != std::string::npos);
    assert(Serial.output.find("\"active_policy_qualified\":false") != std::string::npos);
    command("@18 result " + std::to_string(id) + "\n"); assert(Serial.output.find("\"requested_choice\":7") != std::string::npos);
    assert(releaseOperation(id)); command("@19 result " + std::to_string(id) + "\n"); assert(Serial.output.find("\"ok\":false") != std::string::npos);
    baseline(); uint32_t read = 0; assert(submitHomingParameterRead(read));
    waitTx(read); schedule(words({7, 35, 60, 30, 100, 0, 0}));
    for (unsigned i = 0; i < 25000 && view(read).pending; ++i) step();
    pump(); assert(!view(read).pending && releaseOperation(read));
    assert(!app->homePrerequisites.auxiliaryQualified && !app->homePrerequisites.zeroOffsetQualified);
    const auto writes = hardware.writes; uint32_t unchanged = 77; ESS::HomeRequest home;
    home.configurationGeneration = app->axis.generation;
    // Isolate active-policy rejection: independent simulated qualification of
    // every other home prerequisite is not supplied by the setting or its read.
    auto& qualification = app->homePrerequisites;
    qualification.target = app->axis.target; qualification.configurationGeneration = app->axis.generation;
    qualification.methodQualified = qualification.nativeRatesQualified = qualification.nativeRampQualified =
        qualification.zeroOffsetQualified = qualification.inputsQualified = qualification.indexQualified =
        qualification.readinessQualified = qualification.referenceSemanticsQualified = true;
    qualification.qualifiedMethod = ESS::HomingMethod::METHOD_35;
    qualification.qualifiedSearchSpeed = 60; qualification.qualifiedReturnSpeed = 30; qualification.qualifiedRampTime = 100;
    qualification.observedUs = hardware.time; qualification.rawMotion = 1;
    ESS::HomeContext guarded; guarded.operationId = 987;
    auto rejected = ESS::prepareHome(guarded, app->axis, 50, home, qualification, hardware.time, hardware.time + 1000000);
    assert(!rejected && rejected.detail == static_cast<int32_t>(ESS::HomeError::AUXILIARY_REQUIRED) && guarded.operationId == 987);
    assert(startHome(app, 20, 1, home, unchanged) != Probe::Action::OK && unchanged == 77 && hardware.writes == writes);
    command("@21 home 35 60 30 100 zero\n"); assert(Serial.output.find("\"ok\":false") != std::string::npos && hardware.writes == writes);
    // A qualification copied from the preceding generation cannot admit a home.
    app->homePrerequisites.target = app->axis.target; app->homePrerequisites.configurationGeneration = app->axis.generation - 1;
    app->homePrerequisites.auxiliaryQualified = app->homePrerequisites.zeroOffsetQualified = true;
    rejected = ESS::prepareHome(guarded, app->axis, 50, home, qualification, hardware.time, hardware.time + 1000000);
    assert(!rejected && rejected.detail == static_cast<int32_t>(ESS::HomeError::STALE_CONFIGURATION) && guarded.operationId == 987);
    assert(startHome(app, 22, 1, home, unchanged) != Probe::Action::OK && unchanged == 77 && hardware.writes == writes);
    for (int method : {33, 34, 35}) assert(ESS::homeMethod(static_cast<ESS::HomingMethod>(method))->support == ESS::HomeSupport::IMPLEMENTED);
    for (int method : {-4, -3, -2, -1, 18}) {
        home.method = static_cast<ESS::HomingMethod>(method);
        assert(startHome(app, 23, 1, home, unchanged) == Probe::Action::UNRESOLVED && hardware.writes == writes);
    }
    home.method = ESS::HomingMethod::METHOD_35; home.offset = 1;
    assert(startHome(app, 24, 1, home, unchanged) == Probe::Action::UNRESOLVED && hardware.writes == writes);
    fresh(); baseline(); command("home auxiliary 7\n"); const auto humanId = app->latestOperationId; finish(humanId);
    assert(Serial.output.find("Homing auxiliary setting accepted") != std::string::npos &&
        Serial.output.find("Homing auxiliary setting stored readback confirmed") != std::string::npos &&
        Serial.output.find("Requested auxiliary choice: 7") != std::string::npos &&
        Serial.output.find("Prior auxiliary choice: 65535") != std::string::npos &&
        Serial.output.find("Readback: 7") != std::string::npos &&
        Serial.output.find("Active policy, offset interpretation and homing readiness remain unqualified") != std::string::npos);
    assert(!app->homePrerequisites.auxiliaryQualified);
}
void testPriorCompletionCannotRenewSession() {
    for (unsigned scenario = 0; scenario < 3; ++scenario) {
        fresh(scenario == 1 ? 1000 : 0); baseline(); const auto writes = hardware.writes, id = submit();
        const auto admitted = setting(id); const auto request = findRecord(*app, id)->requestId;
        waitTx(id, READ_AUX); schedule(UNKNOWN_AUX);
        for (unsigned i = 0; i < 25000 && !app->owner.result(request); ++i) { advanceHardware(hardware.time + 10); app->owner.service(uart.sample()); }
        const auto* completed = app->owner.result(request);
        assert(completed && completed->outcome == Rtu::Outcome::SUCCESS && completed->transport.closureQualified &&
            completed->transport.closureLatestUs <= admitted.deadlineUs);
        const auto closedAt = completed->transport.closureLatestUs;
        if (scenario == 0) advanceHardware(admitted.deadlineUs + 1);
        else if (scenario == 1) advanceHardware(hardware.time + 1000001);
        else ++app->axis.generation;
        serviceApplication(); pump(); const auto result = setting(id); const auto& prior = result.observations[0];
        assert(result.kind == ESS::DriverKind::READ && result.state == ReadState::FAILED &&
            result.outcome == ESS::DriverOutcome::UPDATE_NOT_ADMITTED && result.completedSteps == 1);
        assert(result.deadlineUs == admitted.deadlineUs && result.startedUs == admitted.startedUs && !result.effects && !result.uncertain);
        assert(prior.status && prior.qualified && prior.responseConfirmed && prior.txComplete && !prior.write && prior.reg == 0x0030 &&
            prior.count == 1 && prior.latestUs == closedAt && prior.length == UNKNOWN_AUX.size());
        assert(!std::memcmp(prior.raw, UNKNOWN_AUX.data(), UNKNOWN_AUX.size()));
        assert(hardware.writes == writes + 1 && hardware.tx == READ_AUX && !app->owner.active() && !app->owner.pending() && releaseOperation(id));
    }
}
void terminalFixtures() {
    for (const char* name : {"success", "prior_exception", "readback_mismatch", "lost_ack", "cancelled", "changed_generation"}) {
        fresh(); baseline(); command("@30 home auxiliary 7\n"); const auto id = app->latestOperationId; assert(view(id).pending);
        if (!std::strcmp(name, "prior_exception")) settingReply(id, READ_AUX, crc({1, 0x83, 2}));
        else if (!std::strcmp(name, "changed_generation")) { waitTx(id, READ_AUX); ++app->axis.generation; settingReply(id, READ_AUX, UNKNOWN_AUX); }
        else {
            settingReply(id, READ_AUX, UNKNOWN_AUX);
            if (!std::strcmp(name, "lost_ack")) {
                waitTx(id, WRITE_SEVEN); for (unsigned i = 0; i < 5000 && view(id).pending; ++i) step(1000);
                assert(!view(id).pending && setting(id).uncertain);
            } else {
                settingReply(id, WRITE_SEVEN, WRITE_SEVEN);
                if (!std::strcmp(name, "cancelled")) { assert(cancel(app, id) == Probe::Action::OK); pump(); }
                else settingReply(id, READ_AUX, !std::strcmp(name, "readback_mismatch") ? words({6}) : SEVEN_AUX);
            }
        }
        pump(1000); assert(!view(id).pending);
        const auto begin = Serial.output.find("{\"type\":\"home-auxiliary\""); assert(begin != std::string::npos);
        const auto end = Serial.output.find('\n', begin); assert(end != std::string::npos);
        const auto record = Serial.output.substr(begin, end - begin);
        std::printf("{\"case\":\"%s\",\"record\":%s}\n", name, record.c_str()); assert(releaseOperation(id));
    }
}
} // namespace
int main(int argc, char** argv) {
    if (argc == 2 && !std::strcmp(argv[1], "--auxiliary-fixtures")) { terminalFixtures(); return 0; }
    testAdmissionAndCapacity(); testAllChoicesAndPossibleEffects(); testFailuresAndChangedConfiguration();
    testConsoleAndHomeQualificationBoundary(); testPriorCompletionCannotRenewSession();
    if (app) { app->~App(); std::free(app); app = nullptr; }
}
