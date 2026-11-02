// SPDX-License-Identifier: MIT
// Actual console, application, owner and runner over the UART fake. Every claim is
// asserted against the bytes on the wire. Stated facts here are this fixture's own;
// nothing below qualifies the bench.
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
using Bytes = std::vector<uint8_t>;
const char HOME_FACTS[] = "mechanics rates ramp zero-offset auxiliary inputs";
std::vector<Bytes> wire;
std::size_t answered = 0;
unsigned seenWrites = 0;
void step(uint32_t us = 10) {
    advanceHardware(hardware.time + us); serviceApplication();
    if (hardware.writes != seenWrites) { seenWrites = hardware.writes; wire.push_back(hardware.tx); }
}
void pump(unsigned n = 64) { while (n--) step(); }
void fresh() {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = false;
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit));
    hardware.txCharacterUs = 87; assert(uart.startCapture(20, timing().holdUs));
    writeResponseConfirmed = true; // Simulated topology with no local echo.
    wire.clear(); answered = 0; seenWrites = 0; assert(app && !hardware.writes);
}
Bytes crc(Bytes v) {
    const uint16_t c = ESS::calcCrc16(v.data(), v.size());
    v.push_back(static_cast<uint8_t>(c)); v.push_back(static_cast<uint8_t>(c >> 8)); return v;
}
Bytes words(std::initializer_list<uint16_t> values) {
    Bytes result = {1, 3, static_cast<uint8_t>(values.size() * 2)};
    for (const auto value : values) { result.push_back(static_cast<uint8_t>(value >> 8)); result.push_back(static_cast<uint8_t>(value)); }
    return crc(result);
}
Bytes status(uint16_t motion, uint16_t inputs = 0) { return words({0, motion, inputs}); }
void command(const std::string& text) {
    Serial.input = text + "\n"; Serial.output.clear();
    for (unsigned i = 0; i < 500 && !Serial.input.empty(); ++i) step();
    assert(Serial.input.empty()); pump();
}
bool said(const std::string& text) { return Serial.output.find(text) != std::string::npos; }
void saying(const std::string& text) {
    if (!said(text)) std::fprintf(stderr, "Missing [%s] in [%s]\n", text.c_str(), Serial.output.c_str());
    assert(said(text));
}
// A home or move waiting out a poll interval: hop across it, nothing reaches the bus.
void crossPollWait() {
    for (const auto& r : app->records) {
        if (r.kind == App::OperationKind::HOME && r.home.state == ActionState::ACTIVE && !r.requestId.owner && r.home.eligibleUs > hardware.time)
            advanceHardware(r.home.eligibleUs);
    }
}
// Answer the next frame on the wire, optionally checking it first, and let it settle.
void exchange(const Bytes& reply, const Bytes& expected = {}) {
    crossPollWait();
    for (unsigned i = 0; i < 100000 && wire.size() <= answered; ++i) step();
    assert(wire.size() > answered);
    if (!expected.empty() && wire[answered] != expected) {
        std::fprintf(stderr, "Unexpected frame %u:", static_cast<unsigned>(answered));
        for (const auto byte : wire[answered]) std::fprintf(stderr, " %02X", byte);
        std::fprintf(stderr, "\n");
        assert(false);
    }
    ++answered;
    const uint64_t at = std::max(hardware.writeStarted + hardware.tx.size() * 87 + 1000, hardware.time + 1000);
    scheduleReply(at, reply);
    for (unsigned i = 0; i < 5000 && hardware.time < at + reply.size() * 87 + 1000; ++i) step();
    for (unsigned i = 0; i < 5000 && app->owner.active() && wire.size() <= answered; ++i) step();
    pump(8); // An idle turn takes about 1 ms here: short of the 20 ms poll interval.
}
Bytes writeReply(const Bytes& request) { return crc(Bytes(request.begin(), request.begin() + 6)); }
void noNewFrames() { assert(wire.size() == answered); }
// For a command that must land before the next 20 ms poll.
void feed(const std::string& text) {
    Serial.output.clear();
    for (const char c : text) app->console.feed(c);
    app->console.feed(static_cast<char>(10)); pump(2);
}
void readConfig() {
    command("@90 read config"); const auto id = app->latestOperationId;
    exchange(words({0, 1000})); exchange(words({0, 0, 0})); exchange(words({0, 0, 0}));
    exchange(words({0, 0, 0, 0, 0})); exchange(words({3, 4000}));
    assert(app->configuration.operationId == id);
    command("@91 release " + std::to_string(id));
}
void readState(uint16_t motion = 1) {
    command("@92 read state"); const auto id = app->latestOperationId;
    exchange(words({0, motion})); exchange(words({0, 0})); exchange(words({0, 0, 0}));
    command("@93 release " + std::to_string(id));
}
void readHome(uint16_t method, uint16_t search = 60, uint16_t returning = 30, uint16_t ramp = 100, uint16_t auxiliary = 7, uint16_t offsetLow = 0) {
    command("@94 home read"); const auto id = app->latestOperationId;
    exchange(words({auxiliary, method, search, returning, ramp, 0, offsetLow}));
    assert(app->homingParameters.operationId == id);
    command("@95 release " + std::to_string(id));
}
void readVelocity(uint16_t acceleration = 100, uint16_t deceleration = 100) {
    command("@96 velocity read"); const auto id = app->latestOperationId;
    exchange(words({60, acceleration, deceleration}));
    command("@97 release " + std::to_string(id));
}
void readProfile(uint16_t startSpeed) {
    command("@98 motion-profile read");
    exchange(words({startSpeed, 100, 100, 60, 0, 7}));
}
void homeReads(uint16_t method, uint16_t motion = 1) { readConfig(); readState(motion); readHome(method); }
std::string installHome(int method, const std::string& facts) {
    return "@10 home qualification install " + std::to_string(method) + " 60 30 100 " + facts;
}
Bytes staging(uint8_t method, uint8_t search = 60, uint8_t returning = 30, uint8_t ramp = 100) {
    return crc({1, 16, 0, 0x31, 0, 6, 12, 0, method, 0, search, 0, returning, 0, ramp, 0, 0, 0, 0});
}
const Bytes TRIGGER = crc({1, 6, 0, 0x27, 0, 16});
const Bytes HOME_POLL = crc({1, 3, 0, 6, 0, 3});
const Bytes ZERO_READ = crc({1, 3, 0, 10, 0, 2});

void homeFromFreshBootThroughConsole() {
    fresh();
    command("@1 home 35 60 30 100 zero"); saying("\"result\":\"qualification_required\""); noNewFrames();
    // The route names each read in the order it is taken, and reads alone install nothing.
    command(installHome(35, HOME_FACTS)); saying("\"result\":\"reads_required\""); saying("\"read_first\":\"read config\""); noNewFrames();
    readConfig();
    command(installHome(35, HOME_FACTS)); saying("\"read_first\":\"read state\"");
    readState();
    command(installHome(35, HOME_FACTS)); saying("\"read_first\":\"home read\"");
    readHome(35);
    command("@11 home qualification"); saying("\"present\":false"); noNewFrames();
    command("@1 home 35 60 30 100 zero"); saying("\"result\":\"qualification_required\""); noNewFrames();
    command(installHome(35, HOME_FACTS)); saying("\"command\":\"home-qualification-install\""); saying("\"result\":\"done\""); noNewFrames();
    command("@11 home qualification"); saying("\"present\":true"); saying("\"valid\":true");
    saying("\"qualification_maximum_age_us\":0"); saying("\"reference_semantics_qualified\":false"); noNewFrames();

    command("@2 home 35 60 30 100 zero"); saying("\"result\":\"accepted\""); const auto id = app->latestOperationId;
    const auto staged = staging(35);
    exchange(writeReply(staged), staged); exchange(TRIGGER, TRIGGER);
    exchange(status(3), HOME_POLL); exchange(words({0, 0}), ZERO_READ);
    Probe::ResultView result; assert(lookup(app, id, result));
    assert(!result.pending && result.homeContext->state == ActionState::SUCCEEDED && result.homeContext->completion == ActionCompletion::OBSERVED);
    saying("\"type\":\"home\""); saying("\"ok\":true"); saying("\"completion\":\"observed\"");
    assert(!app->coordinateReference.nativeKnown); // Without the reference fact none is published.
    command("@3 release " + std::to_string(id));
    noNewFrames(); assert(wire.size() >= 4);

    // Staging consumed the qualification. Another home needs new reads and a new install.
    command("@4 home 33 60 30 100 zero"); saying("\"result\":\"qualification_required\""); noNewFrames();
    command(installHome(33, std::string(HOME_FACTS) + " index")); saying("\"read_first\":\"read state\""); noNewFrames();
    readState(3);
    command(installHome(33, std::string(HOME_FACTS) + " index")); saying("\"read_first\":\"home read\""); noNewFrames();
    readHome(35); // Shows what 35 staged; it restores nothing and refuses nothing.
    command("@4 home 33 60 30 100 zero"); saying("\"result\":\"qualification_required\""); noNewFrames();
    command(installHome(33, std::string(HOME_FACTS) + " index reference")); saying("\"result\":\"done\""); noNewFrames();
    command("@5 home 33 60 30 100 zero"); saying("\"result\":\"accepted\""); const auto searching = app->latestOperationId;
    const auto staged33 = staging(33);
    exchange(writeReply(staged33), staged33); exchange(TRIGGER, TRIGGER);
    exchange(status(4), HOME_POLL); exchange(status(3), HOME_POLL); exchange(words({0, 0}), ZERO_READ);
    assert(lookup(app, searching, result) && !result.pending && result.homeContext->state == ActionState::SUCCEEDED);
    assert(result.homeContext->runningObserved && app->coordinateReference.nativeKnown);
    noNewFrames();
}
// The manual's factory words: method 24, 120/60/50, auxiliary 0, zero offset.
void homeFromFactoryWords() {
    fresh(); readConfig(); readState(); readHome(24, 120, 60, 50, 0);
    command(installHome(35, HOME_FACTS)); saying("\"result\":\"stored_auxiliary_differs\""); noNewFrames();
    command("@1 home auxiliary 7"); saying("\"result\":\"accepted\""); const auto setting = app->latestOperationId;
    const Bytes write = crc({1, 6, 0, 0x30, 0, 7});
    exchange(words({0})); exchange(write, write); exchange(words({7}));
    command("@2 release " + std::to_string(setting));
    readConfig(); readState(); readHome(24, 120, 60, 50);
    command(installHome(35, HOME_FACTS)); saying("\"result\":\"done\""); noNewFrames();
    // A repeat read of the unchanged factory words revokes nothing.
    readHome(24, 120, 60, 50);
    command("@11 home qualification"); saying("\"valid\":true"); noNewFrames();
    command("home 35 60 30 100 zero"); const auto first = app->latestOperationId;
    Serial.output.clear();
    const auto staged35 = staging(35);
    exchange(writeReply(staged35), staged35); exchange(TRIGGER, TRIGGER);
    exchange(status(3), HOME_POLL); exchange(words({0, 0}), ZERO_READ); pump(64);
    saying("For another home: read state, home read, then home qualification install again.");
    Probe::ResultView result; assert(lookup(app, first, result) && !result.pending && result.homeContext->state == ActionState::SUCCEEDED);

    readState(3); readHome(35);
    command("@3 home qualification install 33 90 40 200 mechanics rates ramp zero-offset auxiliary inputs index"); saying("\"result\":\"done\""); noNewFrames();
    command("@4 home 33 60 30 100 zero"); saying("\"result\":\"invalid\""); noNewFrames(); // Only the stated words.
    command("@5 home 33 90 40 200 zero"); saying("\"result\":\"accepted\""); const auto second = app->latestOperationId;
    const auto staged33 = staging(33, 90, 40, 200);
    exchange(writeReply(staged33), staged33); exchange(TRIGGER, TRIGGER);
    exchange(status(4), HOME_POLL); exchange(status(3), HOME_POLL); exchange(words({0, 0}), ZERO_READ); pump(64);
    assert(lookup(app, second, result) && !result.pending && result.homeContext->state == ActionState::SUCCEEDED);
    noNewFrames();

    // Staging would erase a stored offset, and never writes auxiliary: both still refuse.
    fresh(); readConfig(); readState(); readHome(24, 120, 60, 50, 7, 5);
    command(installHome(35, HOME_FACTS)); saying("\"result\":\"stored_offset_nonzero\""); noNewFrames();
    command("@6 home 35 60 30 100 zero"); saying("\"result\":\"qualification_required\""); noNewFrames();
    // After install, a read showing either contradiction revokes without any traffic of its own.
    for (const bool offset : {false, true}) {
        fresh(); readConfig(); readState(); readHome(24, 120, 60, 50);
        command(installHome(35, HOME_FACTS)); saying("\"result\":\"done\"");
        readHome(24, 120, 60, 50, offset ? 7 : 3, offset ? 5 : 0);
        command("@7 home 35 60 30 100 zero"); saying("\"result\":\"qualification_required\""); noNewFrames();
    }
    // A stored word changing between reads means something else wrote it.
    fresh(); readConfig(); readState(); readHome(24, 120, 60, 50);
    command(installHome(35, HOME_FACTS)); saying("\"result\":\"done\"");
    readHome(25, 120, 60, 50);
    command("@8 home 35 60 30 100 zero"); saying("\"result\":\"qualification_required\""); noNewFrames();
}
void homeFactsAreNamed() {
    fresh(); homeReads(33); const auto frames = wire.size();
    const std::vector<std::string> facts = {"mechanics", "rates", "ramp", "zero-offset", "auxiliary", "inputs", "index"};
    for (const auto& missing : facts) {
        std::string stated;
        for (const auto& fact : facts) if (fact != missing) stated += " " + fact;
        command("@20 home qualification install 33 60 30 100" + stated);
        saying("\"result\":\"missing_fact\""); saying("\"fact\":\"" + missing + "\"");
        assert(wire.size() == frames);
    }
    command(installHome(33, std::string(HOME_FACTS) + " index bogus")); saying("\"result\":\"unknown_fact\""); saying("\"fact\":\"bogus\"");
    command(installHome(33, std::string(HOME_FACTS) + " index all")); saying("\"result\":\"unknown_fact\""); saying("\"fact\":\"all\"");
    // A rejected token cannot alter the reply's JSON.
    command(installHome(33, std::string(HOME_FACTS) + " index \"x")); saying("\"fact\":\"unprintable\"");
    command(installHome(33, std::string(HOME_FACTS) + " index index")); saying("\"result\":\"duplicate_fact\""); saying("\"fact\":\"index\"");
    command(installHome(33, "")); saying("\"result\":\"missing_fact\""); saying("\"fact\":\"mechanics\"");
    for (const char* line : {"@21 home qualification install", "@22 home qualification install 33 60 30", "@23 home qualification install x 60 30 100",
                             "@24 home qualification install 33 4 30 100", "@25 home qualification revoke extra", "@26 home qualification all"}) {
        command(line); saying("\"result\":\"invalid_arguments\"");
    }
    command("@27 home qualification install 18 60 30 100 mechanics"); saying("\"result\":\"unresolved\"");
    command("@28 home qualification install 15 60 30 100 mechanics"); saying("\"result\":\"invalid\"");
    assert(wire.size() == frames);
    command("@29 home qualification"); saying("\"present\":false");

    fresh(); homeReads(35);
    command(installHome(35, std::string(HOME_FACTS) + " index")); saying("\"result\":\"fact_not_applicable\""); saying("\"fact\":\"index\"");
    command(installHome(35, HOME_FACTS)); saying("\"result\":\"done\"");
    command("@30 home qualification revoke"); saying("\"command\":\"home-qualification-revoke\""); saying("\"result\":\"done\"");
    command("@31 home 35 60 30 100 zero"); saying("\"result\":\"qualification_required\""); noNewFrames();
    command("home 35 60 30 100 zero"); saying("home qualification install"); noNewFrames(); // Human reply names the route.

    // Stored auxiliary and offset are named rather than left to a generic refusal.
    fresh(); readConfig(); readState(); readHome(35, 60, 30, 100, 0);
    command(installHome(35, HOME_FACTS)); saying("\"result\":\"stored_auxiliary_differs\"");
    fresh(); readConfig(); readState(4); readHome(35);
    command(installHome(35, HOME_FACTS)); saying("\"result\":\"drive_not_ready\""); saying("\"read_first\":\"read state\"");
}
void longestHomeLineFits() {
    fresh(); readConfig(); readState(); readHome(33, 3000, 300, 2000);
    const std::string line = "@4294967295 home qualification install 33 3000 300 2000 mechanics rates ramp zero-offset auxiliary inputs index reference";
    assert(line.size() < Probe::LINE_CAPACITY);
    command(line); saying("\"result\":\"done\""); noNewFrames();
}
void configurationChangeRevokes() {
    fresh(); homeReads(35); command(installHome(35, HOME_FACTS)); saying("\"result\":\"done\"");
    command("@40 wiring x0 connected"); noNewFrames();
    command("@41 home 35 60 30 100 zero"); saying("\"result\":\"qualification_required\""); noNewFrames();

    fresh(); homeReads(35); command(installHome(35, HOME_FACTS)); saying("\"result\":\"done\"");
    command("@42 home auxiliary 7"); saying("\"result\":\"accepted\""); const auto id = app->latestOperationId;
    const Bytes write = crc({1, 6, 0, 0x30, 0, 7});
    exchange(words({7})); exchange(write, write); exchange(words({7}));
    command("@43 home 35 60 30 100 zero"); saying("\"result\":\"qualification_required\""); noNewFrames();
    command("@44 release " + std::to_string(id));
    // A later matching read does not reinstate what the change revoked.
    homeReads(35); command("@45 home qualification"); saying("\"present\":false");
}
void startSearch() {
    homeReads(33);
    command(installHome(33, std::string(HOME_FACTS) + " index")); saying("\"result\":\"done\"");
}
void tenSecondSearchSucceeds() {
    fresh(); startSearch();
    command("@50 home 33 60 30 100 zero"); saying("\"result\":\"accepted\""); const auto id = app->latestOperationId;
    const auto& context = findRecord(*app, id)->home;
    assert(context.deadlineUs - context.startedUs == 30000000); // ApplicationOptions::moveTimeoutMs.
    assert(context.options.pollIntervalUs == 20000 && context.options.runningPollIntervalUs == 500000);
    const auto staged = staging(33);
    exchange(writeReply(staged), staged); exchange(TRIGGER, TRIGGER);
    const uint64_t start = hardware.time;
    exchange(status(4), HOME_POLL);
    // The poll after the first RUNNING still comes promptly; then polling slows.
    assert(findRecord(*app, id)->home.eligibleUs - hardware.time <= 20000);
    exchange(status(4), HOME_POLL);
    assert(findRecord(*app, id)->home.eligibleUs - hardware.time > 400000);
    while (hardware.time - start < 10000000) exchange(status(4), HOME_POLL);
    Probe::ResultView result; assert(lookup(app, id, result) && result.pending);
    exchange(status(3), HOME_POLL); exchange(words({0, 0}), ZERO_READ);
    assert(lookup(app, id, result) && !result.pending && result.homeContext->state == ActionState::SUCCEEDED);
    assert(result.homeContext->polls < ESS::ACTION_MAX_POLLS && result.homeContext->polls >= 20 && !result.homeContext->uncertain);
    saying("\"completion\":\"observed\""); noNewFrames();
}
void searchBeyondBudgetEndsUncertainWithNoFurtherFrames() {
    fresh(); startSearch();
    command("home 33 60 30 100 zero"); const auto id = app->latestOperationId; Serial.output.clear();
    const auto staged = staging(33);
    exchange(writeReply(staged), staged); exchange(TRIGGER, TRIGGER);
    Probe::ResultView result;
    while (lookup(app, id, result) && result.pending) {
        crossPollWait();
        for (unsigned i = 0; i < 2000 && wire.size() <= answered && result.pending; ++i) { step(); assert(lookup(app, id, result)); }
        if (wire.size() > answered) exchange(status(4), HOME_POLL);
    }
    const auto& context = *result.homeContext;
    assert(context.state == ActionState::FAILED && context.uncertain && context.runningObserved);
    assert(context.outcome == ActionOutcome::DEADLINE && context.polls < ESS::ACTION_MAX_POLLS);
    pump(500);
    saying("The drive may still be moving"); saying("Uncertain: yes");
    const auto frames = wire.size();
    advanceHardware(hardware.time + 20000000); serviceApplication(); pump(1000);
    assert(wire.size() == frames); // No automatic stop or observation.
    for (const auto& frame : wire) assert(!(frame[1] == 6 && frame[3] == 0x27 && (frame[4] == 1 || frame[4] == 2)));
    command("@51 result " + std::to_string(id)); saying("\"uncertain\":true"); saying("\"outcome\":\"deadline\"");
    // An uncertain search interlocks the axis until an explicit stop.
    command(installHome(33, std::string(HOME_FACTS) + " index")); saying("\"result\":\"axis_conflict\"");
    command("@52 stop fast"); saying("\"result\":\"accepted\"");
    const Bytes stop = crc({1, 6, 0, 0x27, 2, 0});
    exchange(stop, stop); exchange(words({0, 1}));
    assert(!axisReserved(*app, 1));
}
void velocityThroughConsole() {
    fresh();
    const char run[] = "@60 velocity 30 rpm native 200 configured normal";
    command(run); saying("\"result\":\"qualification_required\""); noNewFrames();
    command("@61 velocity qualification install rpm ramp inputs"); saying("\"read_first\":\"read config\""); noNewFrames();
    readConfig();
    command("@61 velocity qualification install rpm ramp inputs"); saying("\"read_first\":\"velocity read\""); noNewFrames();
    readVelocity(); readState();
    const auto frames = wire.size();
    command("@62 velocity qualification install ramp inputs"); saying("\"result\":\"missing_fact\""); saying("\"fact\":\"rpm\"");
    command("@62 velocity qualification install rpm inputs"); saying("\"result\":\"missing_fact\""); saying("\"fact\":\"ramp\"");
    command("@62 velocity qualification install rpm ramp"); saying("\"result\":\"missing_fact\""); saying("\"fact\":\"inputs\"");
    command("@63 velocity qualification install rpm ramp inputs bogus"); saying("\"result\":\"unknown_fact\""); saying("\"fact\":\"bogus\"");
    command("@64 velocity qualification install"); saying("\"result\":\"missing_fact\"");
    assert(wire.size() == frames);
    command(run); saying("\"result\":\"qualification_required\"");

    command("@65 velocity qualification install rpm ramp inputs"); saying("\"result\":\"done\""); noNewFrames();
    // The stated facts do not widen the application's finite envelope or the sign.
    command("@66 velocity 61 rpm native 200 configured normal"); saying("\"result\":\"invalid\""); noNewFrames();
    command("@67 velocity 30 rpm native 1001 configured normal"); saying("\"result\":\"invalid_arguments\""); noNewFrames();
    command("@68 velocity -30 rpm native 200 configured normal"); saying("\"result\":\"invalid\""); noNewFrames();

    command(run); saying("\"result\":\"accepted\""); const auto id = app->latestOperationId;
    const Bytes setup = crc({1, 16, 0, 0x1D, 0, 3, 6, 0, 30, 0, 100, 0, 100});
    const Bytes start = crc({1, 6, 0, 0x27, 0, 2});
    exchange(writeReply(setup), setup); exchange(start, start); exchange(words({0, 4}));
    Probe::ResultView result; assert(lookup(app, id, result));
    advanceHardware(result.velocityContext->stopDueUs); serviceApplication();
    const Bytes stop = crc({1, 6, 0, 0x27, 1, 0});
    exchange(stop, stop); exchange(words({0, 1}));
    assert(lookup(app, id, result) && !result.pending && result.velocityContext->state == ActionState::SUCCEEDED);
    assert(result.velocityContext->completion == ActionCompletion::OBSERVED);
    command("@69 release " + std::to_string(id));
    command("@70 velocity qualification revoke"); saying("\"command\":\"velocity-qualification-revoke\""); saying("\"result\":\"done\"");
    command(run); saying("\"result\":\"qualification_required\""); noNewFrames();

    // A later read reporting different ramp words revokes the statement.
    fresh(); readConfig(); readVelocity(); readState();
    command("@71 velocity qualification install rpm ramp inputs negative"); saying("\"result\":\"done\"");
    readVelocity(200, 100);
    command(run); saying("\"result\":\"qualification_required\""); noNewFrames();
}
void positionClearThroughConsole() {
    fresh(); readConfig(); readState();
    command("@80 position-clear"); saying("\"result\":\"qualification_required\""); noNewFrames();
    command("position-clear"); saying("position-clear qualification install clear-semantics"); noNewFrames();
    command("@81 position-clear qualification install"); saying("\"result\":\"missing_fact\""); saying("\"fact\":\"clear-semantics\"");
    command("@82 position-clear qualification install bogus"); saying("\"result\":\"unknown_fact\""); saying("\"fact\":\"bogus\"");
    command("@83 position-clear qualification"); saying("\"result\":\"invalid_arguments\""); noNewFrames();
    command("@84 position-clear qualification install clear-semantics"); saying("\"result\":\"done\""); noNewFrames();
    // Every other check stands: the fact does not replace fresh stopped state.
    command("@85 position-clear 2"); saying("\"result\":\"unavailable\""); noNewFrames();
    command("@86 position-clear"); saying("\"result\":\"accepted\""); const auto id = app->latestOperationId;
    const Bytes clear = crc({1, 6, 0, 0x2D, 0, 0x31});
    exchange(clear, clear); exchange(words({0, 1})); exchange(words({0, 0}), crc({1, 3, 0, 10, 0, 2}));
    Probe::ResultView result; assert(lookup(app, id, result) && !result.pending && result.actionContext->completion == ActionCompletion::OBSERVED);
    command("@87 release " + std::to_string(id));
    command("@88 position-clear"); saying("\"result\":\"unavailable\""); noNewFrames(); // Its own effect expired the state.
    readState();
    command("@89 position-clear qualification revoke"); saying("\"command\":\"position-clear-qualification-revoke\""); saying("\"result\":\"done\"");
    command("@89 position-clear"); saying("\"result\":\"qualification_required\""); noNewFrames();
}
void startSpeedRelationRestoresMoves() {
    fresh(); readConfig(); readState();
    const Bytes read = crc({1, 3, 0, 0x20, 0, 1}), write = crc({1, 6, 0, 0x20, 0, 30});
    command("@1 motion-profile start-speed 30"); saying("\"result\":\"accepted\""); const auto id = app->latestOperationId;
    exchange(words({10}), read); exchange(write, write); exchange(words({30}), read);
    command("@2 release " + std::to_string(id));
    assert(app->positionStartSpeedRelationRequired);
    const char move[] = "@3 move relative 20 steps native 60 configured";
    // The latch holds after fresh reads, and after restoring reads alone.
    command("@4 motion-profile start-speed relation 30 20"); saying("\"result\":\"reads_required\""); saying("\"read_first\":\"read config\""); noNewFrames();
    readConfig(); readState();
    command("@4 motion-profile start-speed relation 30 20"); saying("\"read_first\":\"motion-profile read\""); noNewFrames();
    readProfile(30);
    command(move); saying("\"result\":\"unavailable\""); noNewFrames();
    command("@5 motion-profile start-speed relation 31 20"); saying("\"result\":\"stored_word_differs\""); noNewFrames();
    for (const char* line : {"@5 motion-profile start-speed relation 30 0", "@5 motion-profile start-speed relation 30 3001",
                             "@5 motion-profile start-speed relation 3001 20", "@5 motion-profile start-speed relation 30",
                             "@5 motion-profile start-speed relation 30 20 extra"}) {
        command(line); saying("\"result\":\"invalid_arguments\"");
    }
    noNewFrames(); command(move); saying("\"result\":\"unavailable\""); noNewFrames();
    command("@6 motion-profile start-speed relation 30 61"); saying("\"command\":\"motion-profile-start-speed-relation\""); saying("\"result\":\"done\""); noNewFrames();
    command(move); saying("\"result\":\"unavailable\""); noNewFrames(); // 60 rpm is below the stated minimum.
    command("@6 motion-profile start-speed relation 30 20"); saying("\"result\":\"done\"");
    command(move); saying("\"result\":\"accepted\""); const auto started = app->latestOperationId;
    Probe::ResultView result; assert(lookup(app, started, result));
    assert(result.moveContext->prerequisites.startSpeedKnown && result.moveContext->prerequisites.startSpeed == 20);
    const Bytes staged = crc({1, 16, 0, 0x21, 0, 5, 10, 0, 100, 0, 100, 0, 60, 0, 0, 0, 20});
    exchange(writeReply(staged), staged);
    assert(cancel(app, started) == Probe::Action::OK); pump(1000); noNewFrames(); // No trigger after cancel.
    command("@8 release " + std::to_string(started));

    // A restart forgets the RAM latch: the stored word goes back to the startup policy.
    fresh(); assert(!app->positionStartSpeedRelationRequired);
    readConfig(); readState(); readProfile(30);
    command(move); saying("\"result\":\"accepted\"");
    assert(lookup(app, app->latestOperationId, result) && result.moveContext->prerequisites.startSpeed == 30);
    exchange(writeReply(staged), staged);
}
// A running original and its stored replacement, then a checked stop.
const Bytes MOVE_STAGING = crc({1, 16, 0, 0x21, 0, 5, 10, 0, 100, 0, 100, 0, 60, 0, 0, 0, 20});
const Bytes MOVE_TRIGGER = crc({1, 6, 0, 0x27, 0, 1});
const Bytes MOVE_INTERRUPT = crc({1, 6, 0, 0x27, 0, 9});
const Bytes NORMAL_STOP = crc({1, 6, 0, 0x27, 1, 0});
void interruptRun(bool json, bool answerReplacement, uint32_t& original, uint32_t& replacement) {
    readConfig(); readState(); readProfile(10);
    feed(std::string(json ? "@200 " : "") + "move relative 20 steps native 60 configured"); original = app->latestOperationId;
    exchange(writeReply(MOVE_STAGING), MOVE_STAGING); exchange(MOVE_TRIGGER, MOVE_TRIGGER);
    feed(std::string(json ? "@201 " : "") + "position interrupt " + std::to_string(original)); replacement = app->latestOperationId;
    if (replacement == original) std::fputs(Serial.output.c_str(), stderr);
    assert(replacement != original);
    if (answerReplacement) exchange(MOVE_INTERRUPT, MOVE_INTERRUPT);
    else {
        for (unsigned i = 0; i < 100000 && (wire.size() <= answered || findRecord(*app, replacement)->move.state == ActionState::ACTIVE); ++i) step();
        assert(findRecord(*app, replacement)->move.state == ActionState::FAILED);
        ++answered;
    }
    pump(1000);
}
void settledInterruptPairIsRecycled() {
    // In human mode: delivered, settled and only superseded, so both slots come back.
    // Six cycles need twelve records from the eight slots, so a kept pair would refuse a move.
    fresh(); uint32_t previousOriginal = 0, previousReplacement = 0;
    for (unsigned cycle = 0; cycle < 6; ++cycle) {
        uint32_t original = 0, replacement = 0;
        interruptRun(false, true, original, replacement);
        if (cycle) assert(!findRecord(*app, previousOriginal) && !findRecord(*app, previousReplacement));
        // Before a checked stop the motion is unresolved, so both are kept.
        recycleInteractiveResults(app);
        assert(findRecord(*app, original) && findRecord(*app, replacement));
        command("stop normal"); exchange(NORMAL_STOP, NORMAL_STOP); exchange(words({0, 1})); pump(1000);
        previousOriginal = original; previousReplacement = replacement;
    }
    // A correlated JSON session owns explicit release.
    fresh(); uint32_t original = 0, replacement = 0;
    interruptRun(true, true, original, replacement);
    command("@10 stop normal"); exchange(NORMAL_STOP, NORMAL_STOP); exchange(words({0, 1})); pump(1000);
    recycleInteractiveResults(app);
    assert(findRecord(*app, original) && findRecord(*app, replacement));
    // A lost replacement reply leaves uncertain execution: retained even after a stop.
    fresh(); interruptRun(false, false, original, replacement);
    assert(findRecord(*app, replacement)->move.uncertain);
    command("stop normal"); exchange(NORMAL_STOP, NORMAL_STOP); exchange(words({0, 1})); pump(1000);
    recycleInteractiveResults(app);
    assert(findRecord(*app, original) && findRecord(*app, replacement));
}
}
int main() {
    homeFromFreshBootThroughConsole();
    homeFromFactoryWords();
    homeFactsAreNamed();
    longestHomeLineFits();
    configurationChangeRevokes();
    tenSecondSearchSucceeds();
    searchBeyondBudgetEndsUncertainWithNoFurtherFrames();
    velocityThroughConsole();
    positionClearThroughConsole();
    startSpeedRelationRestoresMoves();
    settledInterruptPairIsRecycled();
    return 0;
}
