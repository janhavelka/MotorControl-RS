// SPDX-License-Identifier: MIT
// Board-declared terminal wiring (wired bench 02) through the actual application,
// its console and its public API, on a fake bus with simulated ESS drives. The
// Arduino variant applies BoardPins.h through setup(); the IDF variant applies
// the same table from (fake) menuconfig values through app_main(). The drive
// simulator and qualifications here say nothing about the real bench.
#if MOTORCONTROLRS_TEST_IDF
// The Kconfig.projbuild defaults; test/idf_config_test.py checks they match.
#define CONFIG_MOTORCONTROLRS_FIXTURE_WIRING 1
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT1_ADDRESS 1
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT1_INPUTS 0xF
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT1_OUTPUTS 0x0
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT2_ADDRESS 2
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT2_INPUTS 0xF
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT2_OUTPUTS 0x0
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT3_ADDRESS 4
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT3_INPUTS 0xF
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT3_OUTPUTS 0x1
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT4_ADDRESS 0
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT4_INPUTS 0x0
#define CONFIG_MOTORCONTROLRS_FIXTURE_SLOT4_OUTPUTS 0x0
#include "fakes/esp32_uart/FakeUsb.h"
#include "../examples/probe_cli/ProbeApp.cpp"
#include "fakes/esp32_uart/UsbConsoleFixture.h"
#include "../examples/probe_idf/main/IdfPlatform.cpp"
#include "../examples/probe_idf/main/main.cpp"
#include "../examples/common/BoardPins.h"
#else
#define MOTORCONTROLRS_FIXTURE_WIRING 1
#include "../examples/probe_cli/ProbeApp.cpp"
#include "fakes/esp32_uart/UsbConsoleFixture.h"
#include "../examples/probe_cli/ArduinoPlatform.cpp"
#include "../examples/probe_cli/main.cpp"
#endif
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
using namespace MotorControlRS;
using MotorControlRSExample::kFixtureWiring;
using MotorControlRSExample::kFixtureWiringCount;
constexpr auto U = InputWiring::UNKNOWN;
constexpr auto N = InputWiring::UNCONNECTED;
constexpr auto C = InputWiring::CONNECTED;

// One simulated ESS drive with plain registers.
struct Drive { uint16_t regs[0x140] = {}; };
std::map<uint8_t, Drive> drives;
std::vector<std::vector<uint8_t>> frames;
unsigned servedWrites = 0;

std::vector<uint8_t> crc(std::vector<uint8_t> bytes) {
    const uint16_t value = ESS::calcCrc16(bytes.data(), bytes.size());
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8));
    return bytes;
}
uint16_t word(const std::vector<uint8_t>& frame, std::size_t at) { return static_cast<uint16_t>(frame[at] << 8 | frame[at + 1]); }
void addDrive(uint8_t address) {
    Drive d;
    d.regs[0x0000] = 0x4EEA; d.regs[0x0001] = 0x0029; d.regs[0x0002] = address;
    d.regs[0x0007] = 1; // In position, enabled, no alarm.
    d.regs[0x0011] = 1000; d.regs[0x0013] = address; d.regs[0x0019] = 1;
    // Bench assignments: X0 origin, X1 positive limit, X2 negative limit, X3 none.
    d.regs[0x0040] = 0; d.regs[0x0041] = 1; d.regs[0x0042] = 2; d.regs[0x0043] = 3; d.regs[0x0044] = 0;
    d.regs[0x0100] = 2; d.regs[0x0101] = 4000;
    d.regs[0x0020] = 30; d.regs[0x0021] = 100; d.regs[0x0022] = 100; d.regs[0x0023] = 60;
    d.regs[0x0030] = 7; d.regs[0x0031] = 35; d.regs[0x0032] = 120; d.regs[0x0033] = 60; d.regs[0x0034] = 100;
    drives[address] = d;
}
void serveBus() {
    if (hardware.writes == servedWrites) return;
    servedWrites = hardware.writes;
    const auto frame = hardware.tx;
    frames.push_back(frame);
    if (frame.size() < 8) return;
    const auto found = drives.find(frame[0]);
    if (found == drives.end()) return;
    auto& d = found->second;
    const uint16_t first = word(frame, 2);
    std::vector<uint8_t> reply;
    if (frame[1] == 3) {
        const uint16_t count = word(frame, 4);
        reply = {frame[0], 3, static_cast<uint8_t>(count * 2)};
        for (uint16_t i = 0; i < count; ++i) {
            const uint16_t r = static_cast<uint16_t>(first + i);
            const uint16_t value = r < 0x140 ? d.regs[r] : 0;
            reply.push_back(static_cast<uint8_t>(value >> 8)); reply.push_back(static_cast<uint8_t>(value));
        }
        reply = crc(reply);
    } else if (frame[1] == 6) {
        if (first < 0x140) d.regs[first] = word(frame, 4);
        reply = frame;
    } else return;
    scheduleReply(std::max(hardware.writeStarted + frame.size() * 87 + 1000, hardware.time + 1000), reply);
}
void step(uint32_t us = 10) { advanceHardware(hardware.time + us); serviceApplication(); serveBus(); }
void pump(unsigned count = 64) { for (unsigned i = 0; i < count; ++i) step(); }
template <typename Done> void runUntil(Done done, unsigned limit = 400000) {
    for (unsigned i = 0; i < limit && !done(); ++i) step(20);
    assert(done());
}
void reset() {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = false; writeResponseConfirmed = false;
    drives.clear(); frames.clear(); servedWrites = 0;
    for (uint8_t address : {uint8_t(1), uint8_t(2), uint8_t(3), uint8_t(4)}) addDrive(address);
}
void started() {
    assert(app && uart.ready() && app->owner.valid() && hardware.writes == 0);
    hardware.txCharacterUs = 87;
    assert(uart.startCapture(20, timing().holdUs));
}
// The board's own startup: BoardPins.h (Arduino) or menuconfig (IDF).
void boardStartup() {
    reset();
#if MOTORCONTROLRS_TEST_IDF
    app_main();
    assert(hardware.taskCreates == 1 && hardware.createdTask);
    const auto task = hardware.createdTask;
    hardware.runningCore = hardware.taskCore; hardware.stopOnIdle = true;
    try { task(nullptr); assert(false); } catch (const FakeTaskStopped&) {}
    hardware.stopOnIdle = false;
#else
    setup();
#endif
    started();
}
// Another consumer's options: no table, or an explicit (possibly invalid) one.
void startWith(const ApplicationOptions& options) {
    reset();
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit, options));
    started();
}
ApplicationOptions table(std::initializer_list<FixtureWiringEntry> entries, std::size_t count) {
    ApplicationOptions options;
    options.fixtureWiringSupplied = true;
    std::size_t i = 0;
    for (const auto& entry : entries) if (i < FIXTURE_WIRING_CAPACITY) options.fixtureWiring[i++] = entry;
    options.fixtureWiringCount = static_cast<uint8_t>(count);
    return options;
}
void command(const std::string& text) {
    Serial.input = text; Serial.output.clear();
    for (unsigned i = 0; i < 500 && !Serial.input.empty(); ++i) step();
    assert(Serial.input.empty()); pump();
    // Long group views drain 64 bytes per service turn.
    for (unsigned i = 0; i < 2000 && (app->outputCount || Serial.output.empty() || Serial.output.back() != '\n'); ++i) step();
}
void contains(const char* text) {
    if (Serial.output.find(text) == std::string::npos) {
        std::fprintf(stderr, "missing %s in:\n%s\n", text, Serial.output.c_str()); assert(false);
    }
}
void lacks(const char* text) {
    if (Serial.output.find(text) != std::string::npos) {
        std::fprintf(stderr, "unexpected %s in:\n%s\n", text, Serial.output.c_str()); assert(false);
    }
}
// Wait for a console operation reply and its terminal line.
void settle() { runUntil([] { return !app->owner.active() && !app->owner.pending() && !reading(*app) && !acting(*app); }); pump(200); }
// Run one console operation to its end and free its retained result.
void consoleRead(const std::string& line) {
    command(line); settle();
    const auto at = Serial.output.find("\"operation_id\":");
    if (at == std::string::npos) return;
    const unsigned long id = std::strtoul(Serial.output.c_str() + at + 15, nullptr, 10);
    if (id) releaseOperation(static_cast<uint32_t>(id));
}
void freshReads(uint8_t address = 1) {
    const std::string a = std::to_string(address);
    for (const char* line : {"@100 read identity ", "@101 read config ", "@102 io read ", "@103 read state "}) {
        consoleRead(line + a + "\n");
        if (std::getenv("FIXTURE_TRACE")) std::fprintf(stderr, "%s\n", Serial.output.c_str());
        lacks("\"ok\":false");
    }
}
void expect(const InputWiring* actual, std::initializer_list<InputWiring> values) {
    std::size_t i = 0; for (const auto value : values) assert(actual[i++] == value);
}
void expectSources(const WiringSource* actual, std::size_t count, WiringSource value) {
    for (std::size_t i = 0; i < count; ++i) assert(actual[i] == value);
}
Probe::WiringSnapshot selected() { Probe::WiringSnapshot view; assert(copyWiring(view)); return view; }
uint32_t declare(const std::vector<uint8_t>& list) {
    uint32_t id = 0;
    assert(declareGroup(list.data(), list.size(), id));
    runUntil([&] { const auto* op = groupOperation(*app, id); return op && !op->result.pending; });
    return id;
}
Probe::GroupMemberView memberView(uint8_t address) {
    static Probe::GroupView view; // Fifteen members are too large for a small stack.
    assert(inspectGroup(view));
    for (uint8_t i = 0; i < view.count; ++i) if (view.members[i].address == address) return view.members[i];
    assert(false); return Probe::GroupMemberView();
}

// Absent table: every legacy default and source is unchanged.
void testAbsentTableKeepsLegacyDefaults() {
    startWith(ApplicationOptions());
    assert(app->fixtureState == FixtureState::ABSENT && !app->fixtureError && app->bootOriginPending);
    auto view = selected();
    expect(view.inputs, {N, N, N, N}); expect(view.outputs, {N, N});
    expectSources(view.inputSources, 4, WiringSource::APPLICATION_DEFAULT);
    expectSources(view.outputSources, 2, WiringSource::APPLICATION_DEFAULT);
    assert(!view.board.listed && view.fixture == FixtureState::ABSENT && !view.fixtureError);
    command("@1 wiring\n");
    contains("\"inputs\":[1,1,1,1],\"outputs\":[1,1],\"sources\":[\"default\",\"default\",\"default\",\"default\",\"default\",\"default\"],\"board\":null,\"fixture\":\"absent\",\"bus_traffic\":false}");
    lacks("fixture_error");
    command("@2 useaddr 4\n");
    view = selected();
    expect(view.inputs, {U, U, U, U}); expect(view.outputs, {U, U});
    expectSources(view.inputSources, 4, WiringSource::APPLICATION_DEFAULT);
    declare({1, 4});
    auto member = memberView(4);
    expect(member.inputs, {U, U, U, U}); expect(member.outputs, {U, U});
    expectSources(member.inputSources, 4, WiringSource::APPLICATION_DEFAULT);
    assert(!member.board.listed);
    // A supplied empty table is valid and different from an absent one.
    startWith(table({}, 0));
    assert(app->fixtureState == FixtureState::APPLIED && selected().inputs[0] == N);
    command("@3 wiring\n"); contains("\"board\":null,\"fixture\":\"applied\"");
}

// The board table applies at boot, after useaddr and to new members, with no traffic.
void testBoardTableAtBootUseaddrAndGroups() {
    boardStartup();
    assert(app->fixtureState == FixtureState::APPLIED && app->boardWiringCount == kFixtureWiringCount);
    assert(std::memcmp(app->boardWiring, kFixtureWiring, sizeof(kFixtureWiring)) == 0);
    // Applying the table is part of the initial state: the boot origin survives.
    assert(app->bootOriginPending && app->axis.generation == 1);
    auto view = selected();
    expect(view.inputs, {C, C, C, C}); expect(view.outputs, {N, N});
    expectSources(view.inputSources, 4, WiringSource::BOARD); expectSources(view.outputSources, 2, WiringSource::BOARD);
    assert(view.board.listed && view.fixture == FixtureState::APPLIED);
    command("@1 wiring\n");
    contains("\"address\":1,");
    contains("\"inputs\":[2,2,2,2],\"outputs\":[1,1],\"sources\":[\"board\",\"board\",\"board\",\"board\",\"board\",\"board\"],\"board\":{\"inputs\":[2,2,2,2],\"outputs\":[1,1]},\"fixture\":\"applied\",\"bus_traffic\":false}");
    command("@2 useaddr 4\n"); command("@3 wiring\n");
    contains("\"inputs\":[2,2,2,2],\"outputs\":[2,1],\"sources\":[\"board\",\"board\",\"board\",\"board\",\"board\",\"board\"],\"board\":{\"inputs\":[2,2,2,2],\"outputs\":[2,1]}");
    command("@4 useaddr 2\n");
    view = selected(); expect(view.inputs, {C, C, C, C}); expect(view.outputs, {N, N});
    command("@5 useaddr 3\n"); command("@6 wiring\n");
    contains("\"inputs\":[0,0,0,0],\"outputs\":[0,0],\"sources\":[\"default\",\"default\",\"default\",\"default\",\"default\",\"default\"],\"board\":null,\"fixture\":\"applied\"");
    command("@7 useaddr 1\n");
    view = selected(); expect(view.inputs, {C, C, C, C}); expectSources(view.inputSources, 4, WiringSource::BOARD);
    assert(hardware.writes == 0); // Declarations never send traffic.
    // New members take the board table; an unlisted member keeps the default.
    declare({1, 2, 4, 3});
    for (uint8_t address : {uint8_t(1), uint8_t(2), uint8_t(4)}) {
        const auto m = memberView(address);
        expect(m.inputs, {C, C, C, C}); expect(m.outputs, {address == 4 ? C : N, N});
        expectSources(m.inputSources, 4, WiringSource::BOARD); expectSources(m.outputSources, 2, WiringSource::BOARD);
        assert(m.board.listed && m.board.outputs[0] == (address == 4 ? C : N));
        const auto* member = groupMember(*app, address);
        assert(member->configuration.wiring[0] == C && member->configuration.wiring[3] == C); // Config read records it.
    }
    auto three = memberView(3);
    expect(three.inputs, {U, U, U, U}); expectSources(three.inputSources, 4, WiringSource::APPLICATION_DEFAULT);
    assert(!three.board.listed);
    command("@8 group show\n");
    contains("\"inputs\":[2,2,2,2],\"outputs\":[2,1],\"start_speed_relation_required\":false,\"input_sources\":[\"board\",\"board\",\"board\",\"board\"],\"output_sources\":[\"board\",\"board\"],\"board\":{\"inputs\":[2,2,2,2],\"outputs\":[2,1]}");
    contains("\"input_sources\":[\"default\",\"default\",\"default\",\"default\"],\"output_sources\":[\"default\",\"default\"],\"board\":null");
    // Kept members keep their session declaration and source; new ones take the board.
    command("@9 group wiring 2 x3 unknown\n"); contains("\"result\":\"done\"");
    auto two = memberView(2);
    assert(two.inputs[3] == U && two.inputSources[3] == WiringSource::HOST && two.board.inputs[3] == C);
    declare({2, 4}); // 2 and 4 stay members.
    two = memberView(2);
    assert(two.inputs[3] == U && two.inputSources[3] == WiringSource::HOST && two.inputSources[0] == WiringSource::BOARD);
    assert(memberView(4).inputSources[3] == WiringSource::BOARD);
    assert(clearGroup()); declare({2}); // A cleared group makes 2 a new member again.
    two = memberView(2);
    assert(two.inputs[3] == C && two.inputSources[3] == WiringSource::BOARD);
    // The selected axis keeps its own declaration.
    assert(selected().inputs[3] == C);
}

// Host overrides change only the session value; useaddr away and back restores the board.
void testHostOverrideAndBoardImmutability() {
    boardStartup();
    const auto generation = app->axis.generation;
    // An unchanged value keeps its source and costs nothing.
    command("@1 wiring x0 connected\n"); contains("\"result\":\"done\"");
    assert(selected().inputSources[0] == WiringSource::BOARD && app->axis.generation == generation && app->bootOriginPending);
    command("@2 wiring x3 unknown\n");
    contains("\"inputs\":[2,2,2,0],\"outputs\":[1,1],\"sources\":[\"board\",\"board\",\"board\",\"host\",\"board\",\"board\"],\"board\":{\"inputs\":[2,2,2,2],\"outputs\":[1,1]}");
    assert(app->axis.generation == generation + 1 && !app->bootOriginPending); // Today's invalidation.
    // Back to the board's value is still a host declaration.
    assert(declareWiring(3, C));
    auto view = selected();
    assert(view.inputs[3] == C && view.inputSources[3] == WiringSource::HOST);
    assert(declareWiring(4, C) && selected().outputSources[0] == WiringSource::HOST && selected().outputs[0] == C);
    assert(!declareWiring(6, C) && !declareWiring(0, static_cast<InputWiring>(3)));
    // A host UNCONNECTED over a board-connected terminal is the caller's fact; it is recorded.
    assert(declareWiring(1, N) && selected().inputs[1] == N && selected().inputSources[1] == WiringSource::HOST);
    assert(selected().board.inputs[1] == C);
    assert(std::memcmp(app->boardWiring, kFixtureWiring, sizeof(kFixtureWiring)) == 0); // Never written.
    command("@3 useaddr 2\n"); command("@4 useaddr 1\n");
    view = selected();
    expect(view.inputs, {C, C, C, C}); expect(view.outputs, {N, N});
    expectSources(view.inputSources, 4, WiringSource::BOARD); expectSources(view.outputSources, 2, WiringSource::BOARD);
    // A change while an operation is pending is refused without a change.
    uint32_t read = 0;
    assert(submitDriverRead(1, ESS::DriverGroup::IO, read) && acting(*app));
    const auto status = declareWiring(3, U);
    assert(!status && status.detail == static_cast<int32_t>(Probe::Action::BUSY) && selected().inputs[3] == C);
    Serial.input = "@5 wiring x3 unknown\n"; Serial.output.clear();
    for (unsigned i = 0; i < 100 && Serial.output.find("\"command\":\"wiring\"") == std::string::npos; ++i) step();
    pump(20);
    contains("\"result\":\"busy\""); contains("\"inputs\":[2,2,2,2]");
    settle();
    assert(hardware.writes && releaseOperation(read));
    for (const auto& frame : frames) assert(frame[1] == 3); // Only the read; declarations send nothing.
}

// Invalid tables: UNKNOWN with invalid_table everywhere, the first error named.
void testInvalidTablesMakeEveryDeclarationUnknown() {
    struct Case { ApplicationOptions options; const char* error; };
    const Case cases[] = {
        {table({{1, 0xF, 0}, {2, 0xF, 0}, {1, 0x1, 0}}, 3), "duplicate_address"},
        {table({{1, 0x10, 0}}, 1), "input_mask_out_of_range"},
        {table({{1, 0xF, 0x4}}, 1), "output_mask_out_of_range"},
        {table({{0, 0xF, 0}}, 1), "address_out_of_range"},
        {table({{248, 0xF, 0}}, 1), "address_out_of_range"},
        {table({{1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {4, 0, 0}}, 5), "count_over_capacity"}};
    for (const auto& c : cases) {
        startWith(c.options);
        assert(app->fixtureState == FixtureState::INVALID && !std::strcmp(app->fixtureError, c.error));
        assert(app->boardWiringCount == 0); // No entry is applied.
        auto view = selected();
        expect(view.inputs, {U, U, U, U}); expect(view.outputs, {U, U});
        expectSources(view.inputSources, 4, WiringSource::INVALID_TABLE);
        expectSources(view.outputSources, 2, WiringSource::INVALID_TABLE);
        assert(!view.board.listed && view.fixture == FixtureState::INVALID && !std::strcmp(view.fixtureError, c.error));
        command("@1 wiring\n");
        contains("\"sources\":[\"invalid_table\",\"invalid_table\",\"invalid_table\",\"invalid_table\",\"invalid_table\",\"invalid_table\"],\"board\":null,\"fixture\":\"invalid\",\"fixture_error\":\"");
        contains(c.error);
        command("@2 useaddr 2\n");
        view = selected(); expect(view.inputs, {U, U, U, U}); expectSources(view.inputSources, 4, WiringSource::INVALID_TABLE);
        declare({2, 4});
        const auto member = memberView(4);
        expect(member.inputs, {U, U, U, U}); expect(member.outputs, {U, U});
        expectSources(member.inputSources, 4, WiringSource::INVALID_TABLE); assert(!member.board.listed);
        command("@3 useaddr 1\n");
        // The operator can still declare in session.
        assert(declareWiring(0, C) && selected().inputSources[0] == WiringSource::HOST);
    }
}

// Declaration-dependent routes refuse before TX; diagnosis and stop stay available.
void testInvalidTableRefusesDependentRoutesButKeepsDiagnosis() {
    startWith(table({{1, 0xF, 0}, {1, 0xF, 0}}, 2));
    freshReads(1);
    consoleRead("@104 home read\n");
    consoleRead("@105 motion-profile read\n");
    const auto before = hardware.writes;
    command("@10 io set x3 16\n"); contains("\"command\":\"io\",\"ok\":false,\"result\":\"invalid\"");
    command("@11 io qualification install 1 x3 16 0 assign unknown triggers-inhibited\n");
    contains("\"ok\":false,\"result\":\"qualification_required\"");
    command("@12 motion-profile start-speed 31\n");
    contains("\"ok\":false,\"result\":\"qualification_required\""); contains("\"reason\":\"input_facts_required\"");
    // A switch method needs a declared connected origin; UNKNOWN counts no input.
    command("@13 home qualification install 19 60 30 100 mechanics rates ramp zero-offset auxiliary inputs\n");
    contains("\"ok\":false,\"result\":\"qualification_rejected\"");
    assert(hardware.writes == before && !app->homePrerequisites.target.id);
    // Diagnosis and stop remain.
    consoleRead("@14 read state\n"); contains("\"ok\":true");
    consoleRead("@15 probe\n"); contains("\"ok\":true");
    consoleRead("@16 stop normal\n"); contains("\"ok\":true");
    assert(hardware.writes > before);
    command("@17 wiring\n"); contains("\"fixture\":\"invalid\",\"fixture_error\":\"duplicate_address\"");
    // Control: the same facts install once the operator declares X3 in session.
    command("@18 wiring x3 connected\n"); contains("\"sources\":[\"invalid_table\",\"invalid_table\",\"invalid_table\",\"host\"");
    freshReads(1);
    command("@19 io qualification install 1 x3 16 0 assign unknown triggers-inhibited\n"); contains("\"ok\":true");
}

// Controls for the refusals above: the same reads admit the routes when the
// declaration permits them.
void testRefusalControls() {
    startWith(ApplicationOptions()); // Legacy UNCONNECTED: the passive start-speed route applies.
    freshReads(1); consoleRead("@1 motion-profile read\n");
    command("@2 motion-profile start-speed 31\n"); contains("\"ok\":true"); settle();
    assert(drives[1].regs[0x0020] == 31);
    boardStartup(); // Connected origin: method 19 counts it as an available input.
    freshReads(1); consoleRead("@3 home read\n");
    command("@4 home qualification install 19 60 30 100 mechanics rates ramp zero-offset auxiliary inputs\n");
    contains("\"ok\":true");
    assert(app->homePrerequisites.target.id && app->homePrerequisites.availableInputs == 7);
}

// A connected X3 needs ASSIGN facts; the passive unconnected route no longer applies.
void testConnectedTerminalNeedsAssignFacts() {
    boardStartup();
    freshReads(1);
    auto before = hardware.writes;
    command("@10 io set x3 16\n"); contains("\"command\":\"io\",\"ok\":false,\"result\":\"invalid\"");
    assert(hardware.writes == before && drives[1].regs[0x0044] == 0);
    command("@11 io qualification install 1 x3 16 0 assign unknown triggers-inhibited\n"); contains("\"ok\":true");
    command("@12 io set x3 16\n"); contains("\"ok\":true"); settle();
    assert(drives[1].regs[0x0044] == 16 && hardware.writes > before);
    // Restore with facts, then read back.
    consoleRead("@13 io read\n"); consoleRead("@14 read state\n");
    command("@15 io qualification install 1 x3 0 0 assign unknown triggers-inhibited\n"); contains("\"ok\":true");
    command("@16 io set x3 none\n"); contains("\"ok\":true"); settle();
    assert(drives[1].regs[0x0044] == 0);
    // Start speed needs CONFIGURE facts with functions 1-3 on connected inputs (wired bench 03).
    freshReads(1); consoleRead("@17 motion-profile read\n");
    before = hardware.writes;
    command("@18 motion-profile start-speed 31\n"); contains("\"ok\":false,\"result\":\"qualification_required\"");
    contains("\"reason\":\"input_facts_required\",\"purpose\":\"configure\",\"read_first\":\"read config\",\"terminal\":\"x0\"");
    assert(hardware.writes == before);
}

// Home 35 installs with connected X0-X2 at functions 1-3 and inactive inputs.
void testHome35WithConnectedLimits() {
    boardStartup();
    freshReads(1);
    consoleRead("@104 home read\n");
    command("@10 home qualification install 35 120 60 100 mechanics rates ramp zero-offset auxiliary inputs\n");
    contains("\"ok\":true");
    assert(app->homePrerequisites.target.id && app->homePrerequisites.availableInputs == 7);
    for (uint8_t i = 0; i < 4; ++i) assert(app->homePrerequisites.inputWiring[i] == C);
    for (const auto& frame : frames) assert(frame[1] == 3); // Reads only; installation sends nothing.
}
} // namespace

int main() {
    testAbsentTableKeepsLegacyDefaults();
    testBoardTableAtBootUseaddrAndGroups();
    testHostOverrideAndBoardImmutability();
    testInvalidTablesMakeEveryDeclarationUnknown();
    testInvalidTableRefusesDependentRoutesButKeepsDiagnosis();
    testRefusalControls();
    testConnectedTerminalNeedsAssignFacts();
    testHome35WithConnectedLimits();
    reset();
    std::puts("Fixture wiring application tests passed");
    return 0;
}
