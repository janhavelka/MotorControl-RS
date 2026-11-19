// SPDX-License-Identifier: MIT
// Wired bench 16: the named position-limits install (D6/D12) through the real
// application, owner and console with scripted wire evidence. These fixtures
// qualify no hardware; they show admission, binding and expiry only.
#include "../examples/probe_cli/ProbeApp.cpp"
#include "fakes/esp32_uart/UsbConsoleFixture.h"
#include "../examples/probe_cli/ArduinoPlatform.cpp"
#include "../examples/probe_cli/main.cpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

namespace {
using namespace MotorControlRS;
const std::vector<uint8_t> RELATIVE = {1,6,0,0x27,0,1,0xF8,0x01};
const std::vector<uint8_t> INTERRUPT_RELATIVE = {1,6,0,0x27,0,9,0xF9,0xC7};
void step(uint32_t us = 10) { advanceHardware(hardware.time + us); serviceApplication(); }
void pump(unsigned count = 64) { for (unsigned i = 0; i < count; ++i) step(); }
void fresh() {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = writeResponseConfirmed = false;
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit, ApplicationOptions()));
    hardware.txCharacterUs = 87; assert(uart.startCapture(20, timing().holdUs));
    writeResponseConfirmed = true; // Supplied fake topology, never a bench observation.
}
std::vector<uint8_t> crc(std::vector<uint8_t> bytes) {
    uint16_t value = 0xFFFF;
    for (const uint8_t byte : bytes) {
        value ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit) value = value & 1 ? (value >> 1) ^ 0xA001 : value >> 1;
    }
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8)); return bytes;
}
std::vector<uint8_t> words(std::initializer_list<uint16_t> values) {
    std::vector<uint8_t> bytes = {1,3,static_cast<uint8_t>(values.size()*2)};
    for (const uint16_t value : values) { bytes.push_back(static_cast<uint8_t>(value >> 8)); bytes.push_back(static_cast<uint8_t>(value)); }
    return crc(bytes);
}
Probe::ResultView view(uint32_t id) { Probe::ResultView result; assert(lookup(app,id,result)); return result; }
void feed(const std::string& line) { for (const char c : line) app->console.feed(c); }
void command(const std::string& line) { Serial.output.clear(); feed(line); pump(300); }
bool said(const std::string& text) { return Serial.output.find(text) != std::string::npos; }
void waitTx(uint32_t id, const std::vector<uint8_t>& expected = {}) {
    auto* record = findRecord(*app,id); assert(record);
    for (unsigned i=0; i<100000 && !app->owner.txAccepted(record->requestId) && view(id).pending; ++i) step();
    assert(app->owner.txAccepted(record->requestId));
    if (!expected.empty()) assert(hardware.tx == expected);
}
void schedule(const std::vector<uint8_t>& response) {
    scheduleReply(std::max(hardware.writeStarted + hardware.tx.size()*87 + 1000, hardware.time + 1000), response);
}
void readReply(uint32_t id, const std::vector<uint8_t>& response) {
    const auto token = view(id).typedRead->step; waitTx(id); schedule(response);
    for (unsigned i=0; i<25000; ++i) {
        step(); const auto* record=findRecord(*app,id);
        if (!record || record->read.state!=ReadState::ACTIVE || record->read.step!=token) return;
    }
    assert(false);
}
// Checked configuration (subdivision 4000, the HW-14 row), stopped state and a profile read.
void baseline(uint16_t subdivision = 4000) {
    command("@1 read config\n"); const uint32_t config = app->latestOperationId;
    readReply(config,words({0,subdivision})); readReply(config,words({0,0,0}));
    readReply(config,words({0,0,0}));
    readReply(config,words({0,1,2,3,0})); readReply(config,words({3,4000}));
    pump(300); assert(app->configuration.operationId==config && releaseOperation(config));
    command("@2 read state\n"); const uint32_t state = app->latestOperationId;
    readReply(state,words({0,1})); readReply(state,words({0,0})); readReply(state,words({0,0,0}));
    pump(300); assert(releaseOperation(state));
    Probe::MotionProfileView profile;
    motionProfileCommand(app,Probe::MotionProfileCommand::FORGET,profile); // A repeated baseline reads a new snapshot.
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,profile)==Probe::Action::OK);
    for (unsigned i=0; i<25000 && !app->owner.txAccepted(app->motionProfile.request); ++i) step();
    assert(app->owner.txAccepted(app->motionProfile.request));
    schedule(words({10,500,500,60,0,20}));
    for (unsigned i=0; i<25000 && app->motionProfile.view.pending; ++i) step();
    assert(motionProfileBound(*app) && app->motionProfile.view.ok && app->configuration.raw.subdivision==subdivision);
}
Probe::PositionLimitsCommand hw14(const char* name = "hw14") {
    Probe::PositionLimitsCommand c; c.kind = Probe::PositionLimitsCommandKind::SET;
    c.limits.maximumRpm = 2000; c.limits.maximumCommandRate = 133334; c.limits.minimumRampTime = 100;
    std::strncpy(c.caseName, name, sizeof(c.caseName) - 1);
    return c;
}
bool defaults(const Probe::PositionLimitsView& v) {
    const ESS::PositionLimits d;
    return v.source == Probe::PositionLimitsSource::DEFAULT && Probe::samePositionLimits(v.limits, d) && !v.caseName[0];
}
Probe::PositionLimitsView inspected() { Probe::PositionLimitsView v; assert(inspectPositionLimits(v)); return v; }
// 2000 rpm at subdivision 4000 is 133,333 increments/s: refused by the
// default 50,000 ceiling (750 rpm there), admitted by the hw14 install.
bool fastMoveAdmitted(uint32_t& id) {
    const auto writes = hardware.writes, next = app->nextOperationId;
    id = 0;
    const bool admitted = static_cast<bool>(moveBy(Rational(20), PositionUnit::STEPS, id, 2000));
    if (!admitted) assert(!id && hardware.writes == writes && app->nextOperationId == next && !app->owner.pending());
    return admitted;
}
void startMove(uint32_t id) {
    const uint8_t token = view(id).moveContext->step; waitTx(id); schedule(crc({1,16,0,0x21,0,5}));
    for (unsigned i=0; i<100000 && view(id).pending && view(id).moveContext->step==token; ++i) step();
    const uint8_t trigger = view(id).moveContext->step; waitTx(id,RELATIVE); schedule(RELATIVE);
    for (unsigned i=0; i<100000 && view(id).pending && view(id).moveContext->step==trigger; ++i) step();
    assert(view(id).pending && view(id).moveContext->execution==ActionExecution::ACKNOWLEDGED);
}

void defaultsUnchangedAndLocalShow() {
    fresh(); baseline();
    const auto writes = hardware.writes;
    assert(defaults(inspected()) && inspected().maximumRpm == 750 && inspected().defaultRpm == 750);
    command("@10 position limits\n");
    assert(said("\"command\":\"position-limits\",\"ok\":true") && said("\"bus_traffic\":false") &&
        said("\"source\":\"default\"") && said("\"max_rpm\":2000,\"max_rate\":50000,\"min_ramp\":100") &&
        said("\"maximum_position_rpm\":750"));
    assert(hardware.writes == writes && !app->owner.pending());
    uint32_t id = 0;
    assert(!fastMoveAdmitted(id)); // The default ceiling refuses before any frame.
    assert(moveBy(Rational(20), PositionUnit::STEPS, id, 750) && id); // 750 is the default ceiling here.
    assert(Probe::samePositionLimits(view(id).moveContext->prerequisites.limits, ESS::PositionLimits()));
    assert(!findRecord(*app, id)->positionLimitsCase[0]);
}
// The settings reply (here its pending first line, before its own reads) names the source.
void settingsShowsSource() {
    for (const bool installed : {false, true}) {
        fresh(); baseline();
        Probe::PositionLimitsView v;
        if (installed) assert(installPositionLimits(hw14(), v));
        command(installed ? "@20 settings\n" : "@11 settings\n");
        assert(said("\"default_maximum_position_rpm\":"));
        assert(said(installed ? "\"position_limits\":{\"source\":\"installed\",\"case\":\"hw14\",\"max_rpm\":2000,\"max_rate\":133334,\"min_ramp\":100}" :
            "\"position_limits\":{\"source\":\"default\",\"case\":\"\",\"max_rpm\":2000,\"max_rate\":50000,\"min_ramp\":100}"));
    }
}
void installAdmitsAndEchoes() {
    fresh(); baseline();
    uint32_t id = 0;
    assert(!fastMoveAdmitted(id));
    const auto writes = hardware.writes;
    Probe::PositionLimitsView v;
    assert(installPositionLimits(hw14(), v) && v.source == Probe::PositionLimitsSource::INSTALLED);
    assert(!std::strcmp(v.caseName, "hw14") && v.maximumRpm == 2000 && v.defaultRpm == 750 && hardware.writes == writes);
    assert(fastMoveAdmitted(id));
    const auto& admitted = view(id).moveContext->prerequisites.limits;
    assert(admitted.maximumRpm == 2000 && admitted.maximumCommandRate == 133334 && admitted.minimumRampTime == 100);
    assert(!std::strcmp(findRecord(*app, id)->positionLimitsCase, "hw14"));
    assert(cancel(app, id) == Probe::Action::OK); pump(300);
    // The console route echoes the case and values in the move result.
    command("@21 position limits set 2000 133334 100 case hw14\n");
    assert(said("\"ok\":true,\"result\":\"done\",\"request\":\"set\"") && said("\"case\":\"hw14\""));
    feed("@22 move relative 20 steps native 2000 configured\n"); pump(300);
    const auto consoleMove = app->latestOperationId;
    assert(consoleMove != id && view(consoleMove).pending);
    // Cancelled after its staging frame: the terminal move line still reports its admission.
    Serial.output.clear();
    assert(cancel(app, consoleMove) == Probe::Action::OK);
    const auto lineDone = [] {
        const auto begin = Serial.output.find("\"type\":\"move\"");
        return begin != std::string::npos && Serial.output.find('\n', begin) != std::string::npos;
    };
    for (unsigned i = 0; i < 400000 && !lineDone(); ++i) step();
    assert(said("\"position_limits\":{\"source\":\"installed\",\"case\":\"hw14\",\"max_rpm\":2000,\"max_rate\":133334,\"min_ramp\":100}"));
    assert(revokePositionLimits() && defaults(inspected()));
}
void refusalsBeforeAnyFrame() {
    fresh(); baseline();
    const auto writes = hardware.writes;
    Probe::PositionLimitsView v;
    assert(installPositionLimits(hw14("keep"), v));
    for (unsigned which = 0; which < 10; ++which) {
        auto c = hw14();
        switch (which) {
        case 0: c.limits.maximumRpm = 0; break;
        case 1: c.limits.maximumRpm = 3001; break;
        case 2: c.limits.maximumCommandRate = 0; break;
        case 3: c.limits.maximumCommandRate = Probe::POSITION_LIMITS_MAX_RATE + 1; break;
        case 4: c.limits.minimumRampTime = 2001; break;
        case 5: c.caseName[0] = 0; break;
        case 6: std::strcpy(c.caseName, "HW14"); break;
        case 7: std::strcpy(c.caseName, "-hw14"); break;
        case 8: std::strcpy(c.caseName, "hw 14"); break;
        case 9: c.kind = Probe::PositionLimitsCommandKind::SHOW; break;
        }
        const auto refused = installPositionLimits(c, v);
        assert(!refused && refused.detail == static_cast<int32_t>(Probe::Action::INVALID));
        assert(!std::strcmp(inspected().caseName, "keep")); // The previous install stays.
    }
    for (const char* bad : {"@30 position limits set 2000 133334 100\n", "@31 position limits set 2000 133334 100 case\n",
                            "@32 position limits set 2000 133334 100 name hw14\n", "@33 position limits set -1 1 1 case a\n",
                            "@34 position limits set 2000 133334 100 case abcdefghijklmnop\n", "@35 position limits default x\n",
                            "@36 position limits other\n"}) {
        command(bad); assert(said("\"ok\":false") && said("invalid_arguments"));
    }
    command("@37 position limits set 3001 1 1 case a\n");
    assert(said("\"ok\":false,\"result\":\"invalid\"") && said("\"reason\":\"outside_wire_ranges\"") && said("\"case\":\"keep\""));
    command("@38 position limits set 1 1 1 case Bad\n"); assert(said("\"reason\":\"invalid_case\""));
    command("@39 position limits default\n"); assert(said("\"ok\":true") && said("\"source\":\"default\""));
    assert(hardware.writes == writes && !app->owner.pending());
}
void expiryPaths() {
    for (unsigned path = 0; path < 4; ++path) {
        fresh(); baseline();
        Probe::PositionLimitsView v;
        assert(installPositionLimits(hw14(), v));
        uint32_t id = 0;
        switch (path) {
        case 0: { // Recovery: a new binding generation.
            command("@40 recover\n"); const auto recovery = app->latestOperationId;
            for (unsigned i=0; i<100000 && view(recovery).pending; ++i) step();
            break;
        }
        case 1: command("@41 useaddr 2\n"); command("@44 useaddr 1\n"); break; // Rebinding (same address again).
        case 2: command("@42 wiring y1 connected\n"); break;       // Configuration change.
        case 3: command("@43 position limits default\n"); break;   // Explicit revoke.
        }
        assert(defaults(inspected()));
        baseline();
        assert(defaults(inspected()) && !fastMoveAdmitted(id));
        uint32_t slow = 0; assert(moveBy(Rational(20), PositionUnit::STEPS, slow, 750) && slow); // Readiness is back.
    }
}
void groupAndInputStartPathsKeepDefaults() {
    // Group members and armed input starts prepare through the shared
    // MotionFacts readiness, which never sees the selected-axis install.
    fresh(); baseline();
    Probe::PositionLimitsView v;
    assert(installPositionLimits(hw14(), v));
    MoveRequest request; request.position.value = Rational(20); request.position.configurationGeneration = app->axis.generation;
    request.speedRpm = 2000; request.ramp = MoveRamp::VERIFIED_CONFIGURED;
    ESS::MovePrerequisites shared;
    assert(moveRequirements(selectedFacts(*app), request, uart.sample(), shared));
    assert(Probe::samePositionLimits(shared.limits, ESS::PositionLimits()));
    assert(!ESS::checkPositionLimits(4000, 2000, 500, 500, shared.limits));
}
void interruptKeepsOriginalLimits() {
    fresh(); baseline();
    Probe::PositionLimitsView v;
    assert(installPositionLimits(hw14(), v));
    uint32_t old = 0; assert(fastMoveAdmitted(old)); startMove(old);
    assert(revokePositionLimits() && defaults(inspected()));
    feed("@50 position interrupt " + std::to_string(old) + "\n"); pump(300);
    const auto replacement = app->latestOperationId;
    assert(replacement != old && findRecord(*app, replacement));
    assert(!std::strcmp(findRecord(*app, replacement)->positionLimitsCase, "hw14"));
    // Its stored 2000 rpm passed the core check only under the original's limits.
    assert(findRecord(*app, replacement)->move.stored.profile.speed == 2000);
    assert(!ESS::checkPositionLimits(4000, 2000, 500, 500, ESS::PositionLimits()));
    waitTx(replacement, INTERRUPT_RELATIVE);
}
}
int main() {
    defaultsUnchangedAndLocalShow();
    installAdmitsAndEchoes();
    settingsShowsSource();
    refusalsBeforeAnyFrame();
    expiryPaths();
    groupAndInputStartPathsKeepDefaults();
    interruptKeepsOriginalLimits();
    if (app) { app->~App(); std::free(app); app = nullptr; }
    std::puts("position_limits_app_test passed");
    return 0;
}
