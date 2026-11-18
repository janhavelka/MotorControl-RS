// SPDX-License-Identifier: MIT
// Settings routes with board-declared connected terminals (wired bench 03),
// through the actual application, its console and its public API, on a fake
// bus with a simulated ESS drive. Every route either refuses before TX with a
// named reason or runs its full prior-read/write/readback sequence with four
// stated CONFIGURE (or SEGMENT) records. The Arduino variant applies the board
// table through setup(); the IDF variant through (fake) menuconfig values.
// The simulator and the facts here say nothing about the real bench.
#if MOTORCONTROLRS_TEST_IDF
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
// The bench drive at power-up (wired bench 01 baseline), with word order 0.
void addDrive(uint8_t address) {
    Drive d;
    d.regs[0x0000] = 0x4EEA; d.regs[0x0001] = 0x0029; d.regs[0x0002] = address; d.regs[0x0003] = address;
    d.regs[0x0007] = 1; // In position, enabled, no alarm.
    d.regs[0x0011] = 1000; d.regs[0x0013] = 0; d.regs[0x0019] = 0;
    d.regs[0x0041] = 1; d.regs[0x0042] = 2; d.regs[0x0043] = 3; d.regs[0x0044] = 0; // X0 origin, X1/X2 limits, X3 none.
    d.regs[0x0100] = 3; d.regs[0x0101] = 4000;
    const uint16_t control[] = {3, 4000, 5600, 100, 40, 100, 40, 200};
    for (unsigned i = 0; i < 8; ++i) d.regs[0x0100 + i] = control[i];
    const uint16_t filters[] = {2, 5, 4000, 5, 10, 512};
    for (unsigned i = 0; i < 6; ++i) d.regs[0x0108 + i] = filters[i];
    const uint16_t profile[] = {30, 100, 100, 60, 0, 5000};
    for (unsigned i = 0; i < 6; ++i) d.regs[0x0020 + i] = profile[i];
    d.regs[0x0030] = 0; d.regs[0x0031] = 24; d.regs[0x0032] = 120; d.regs[0x0033] = 60; d.regs[0x0034] = 100;
    for (unsigned n = 0; n < 16; ++n) {
        const uint16_t pt[] = {0, 5000, 120, 100, 100};
        for (unsigned i = 0; i < 5; ++i) d.regs[0x0060 + 6 * n + i] = pt[i];
        for (unsigned i = 0; i < 3; ++i) d.regs[0x00C0 + 3 * n + i] = 100;
    }
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
    for (uint8_t address : {uint8_t(1), uint8_t(2), uint8_t(4)}) addDrive(address);
}
void started() {
    assert(app && uart.ready() && app->owner.valid() && hardware.writes == 0);
    hardware.txCharacterUs = 87;
    assert(uart.startCapture(20, timing().holdUs));
}
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
    assert(app->inputWiring[0] == InputWiring::CONNECTED && writeResponseConfirmed);
}
void startWith(const ApplicationOptions& options) {
    reset();
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit, options));
    started();
}
void command(const std::string& text) {
    Serial.input = text; Serial.output.clear();
    for (unsigned i = 0; i < 500 && !Serial.input.empty(); ++i) step();
    assert(Serial.input.empty()); pump();
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
void settle() { runUntil([] { return !app->owner.active() && !app->owner.pending() && !reading(*app) && !acting(*app); }); pump(200); }
uint32_t replyOperation() {
    const auto at = Serial.output.find("\"operation_id\":");
    return at == std::string::npos ? 0 : static_cast<uint32_t>(std::strtoul(Serial.output.c_str() + at + 15, nullptr, 10));
}
void consoleRead(const std::string& line) {
    command(line); settle();
    if (const auto id = replyOperation()) releaseOperation(id);
}
void freshReads() {
    for (const char* line : {"@100 read identity\n", "@101 read config\n", "@102 io read\n", "@103 read state\n"}) {
        consoleRead(line); lacks("\"ok\":false");
    }
}
std::size_t writes() { std::size_t n = 0; for (const auto& f : frames) n += f.size() > 1 && f[1] == 6; return n; }
// Four records with function and polarity taken from the simulated drive, as
// the harness helper takes them from read config; extra facts only on request.
void install(const char* purpose, const char* x3Extra = "", unsigned terminals = 4) {
    const auto& r = drives[1].regs;
    for (unsigned i = 0; i < terminals; ++i) {
        char line[128];
        std::snprintf(line, sizeof(line), "@110 io qualification install 1 x%u %u %u %s unknown%s%s\n", i,
            r[0x0041 + i], (r[0x0040] >> i) & 1u, purpose, i == 3 && *x3Extra ? " " : "", i == 3 ? x3Extra : "");
        command(line); contains("\"ok\":true");
    }
}
// One setter route: its prerequisite read, the candidate and the restoring value.
struct Route {
    const char* name;
    const char* read;       ///< Group read the core compares against (null: single setting).
    const char* set;        ///< Console setter with the candidate.
    const char* restore;    ///< Console setter back to the baseline.
    uint16_t reg, baseline, candidate;
    const char* purpose;
    const char* command;    ///< Reply command name.
};
const Route ROUTES[] = {
    {"start speed", nullptr, "motion-profile start-speed 31", "motion-profile start-speed 30", 0x0020, 30, 31, "configure", "motion-profile-start-speed"},
    {"homing auxiliary", nullptr, "home auxiliary 7", "home auxiliary 0", 0x0030, 0, 7, "configure", "home-auxiliary"},
    {"subdivision", "driver read", "driver set subdivision 1600", "driver set subdivision 1000", 0x0011, 1000, 1600, "configure", "driver"},
    {"position mode", "driver read", "driver set position-mode 1", "driver set position-mode 0", 0x0050, 0, 1, "configure", "driver"},
    {"input filter", "tuning filters read", "tuning filters set input-filter 3", "tuning filters set input-filter 2", 0x0108, 2, 3, "configure", "tuning"},
    {"threshold", "tuning filters read", "tuning filters set deviation-threshold 3999", "tuning filters set deviation-threshold 4000", 0x010A, 4000, 3999, "configure", "tuning"},
    {"PT record", "segment position 2 read", "segment position 2 set speed 121", "segment position 2 set speed 120", 0x0068, 120, 121, "segment", "segment"},
    {"PV record", "segment speed 2 read", "segment speed 2 set speed 101", "segment speed 2 set speed 100", 0x00C3, 100, 101, "segment", "segment"},
    {"start record", "segment start 2 read", "segment start 2 set value 1", "segment start 2 set value 0", 0x0131, 0, 1, "segment", "segment"},
};
void prepare(const Route& route) {
    freshReads();
    if (route.read) { consoleRead(std::string("@104 ") + route.read + "\n"); lacks("\"ok\":false"); }
}
// Refused at admission: zero frames, no operation, the named reason.
void refused(const Route& route, const char* reason, const char* purpose = nullptr, const char* terminal = nullptr) {
    const auto before = frames.size(); const auto next = app->nextOperationId; const auto stored = drives[1].regs[route.reg];
    command(std::string("@120 ") + route.set + "\n");
    contains("\"ok\":false,\"result\":\"qualification_required\""); contains("\"operation_id\":0");
    char text[96]; std::snprintf(text, sizeof(text), "\"reason\":\"%s\",\"purpose\":\"%s\"", reason, purpose ? purpose : route.purpose);
    contains(text);
    if (terminal) { std::snprintf(text, sizeof(text), "\"terminal\":\"%s\"", terminal); contains(text); }
    pump(200);
    if (frames.size() != before) { std::fprintf(stderr, "%s sent %zu frames\n", route.name, frames.size() - before); assert(false); }
    assert(app->nextOperationId == next && drives[1].regs[route.reg] == stored);
}
// Admitted: the complete sequence runs and the simulated drive stores the value.
void admitted(const Route& route, const char* line, uint16_t expected) {
    const auto before = frames.size(), fc06 = writes();
    command(std::string("@121 ") + line + "\n"); contains("\"ok\":true");
    const auto id = replyOperation(); assert(id);
    settle(); contains("\"outcome\":\"success\"");
    assert(drives[1].regs[route.reg] == expected && writes() == fc06 + 1);
    assert(frames.size() == before + (route.read ? 2u : 3u)); // [prior FC03,] FC06, FC03 readback.
    if (!route.read) { assert(frames[before][1] == 3 && word(frames[before], 2) == route.reg); }
    releaseOperation(id);
}

// Connected terminals without facts: every route refuses before TX and names it.
void testConnectedWithoutFactsRefusedBeforeTx() {
    boardStartup();
    for (const auto& route : ROUTES) { prepare(route); refused(route, "input_facts_required", nullptr, "x0"); }
    contains("\"read_first\":\"read config\"");
    // API: the same candidate, the same refusal; output ID and drive unchanged.
    freshReads();
    uint32_t id = 77; const auto before = frames.size();
    const auto status = submitPositionStartSpeed(31, id);
    assert(!status && status.detail == static_cast<int32_t>(Probe::Action::QUALIFICATION_REQUIRED) &&
           !std::strcmp(status.msg, "input_facts_required") && id == 77);
    Probe::SettingsRefusal refusal;
    assert(copySettingsRefusal(refusal) && !std::strcmp(refusal.reason, "input_facts_required") &&
           !std::strcmp(refusal.purpose, "configure") && !std::strcmp(refusal.readFirst, "read config") && refusal.terminal == 0);
    assert(frames.size() == before);
    ESS::DriverRequest drive; drive.fields = static_cast<uint32_t>(ESS::DriverField::POSITION_MODE);
    drive.positionMode = ESS::PositionMode::ABSOLUTE;
    consoleRead("@104 driver read\n");
    const auto afterRead = frames.size();
    const auto driveStatus = submitDriverSettings(1, drive, id);
    assert(!driveStatus && driveStatus.detail == static_cast<int32_t>(Probe::Action::QUALIFICATION_REQUIRED) &&
           !std::strcmp(driveStatus.msg, "input_facts_required") && id == 77);
    pump(200); assert(frames.size() == afterRead);
    // A refusal with another cause names nothing.
    ESS::DriverRequest bad; bad.fields = static_cast<uint32_t>(ESS::DriverField::SUBDIVISION); bad.subdivision = 399;
    assert(!submitDriverSettings(1, bad, id) && !copySettingsRefusal(refusal) && id == 77);
}

// Four current records admit every route; the restoration needs new records
// because each write clears them.
void testFactsAdmitEveryRoute() {
    boardStartup();
    for (const auto& route : ROUTES) {
        prepare(route); install(route.purpose);
        admitted(route, route.set, route.candidate);
        assert(!app->ioQualifications[0].target.id); // The write cleared the facts.
        prepare(route); refused(route, "input_facts_required");
        install(route.purpose);
        admitted(route, route.restore, route.baseline);
    }
    // Lock delay has no input condition (regression).
    freshReads(); consoleRead("@104 control read\n");
    command("@130 control set lock-delay 201\n"); contains("\"ok\":true"); settle();
    assert(drives[1].regs[0x0107] == 201);
}

// Revalidation after the admitted prior read: no FC06, the reason retained.
void testRevalidationAfterPriorRead() {
    const char* reasons[] = {"input_facts_required", "input_facts_stale", "input_active"};
    for (unsigned route = 0; route < 2; ++route)
        for (unsigned change = 0; change < 3; ++change) {
            boardStartup(); freshReads(); install("configure");
            const auto before = frames.size();
            Serial.input = std::string("@140 ") + ROUTES[route].set + "\n"; Serial.output.clear();
            for (unsigned i = 0; i < 500 && !Serial.input.empty(); ++i) step();
            assert(Serial.input.empty() && findRecord(*app, app->latestOperationId));
            const auto id = app->latestOperationId;
            assert(findRecord(*app, id)->driver.kind == ESS::DriverKind::READ);
            if (change == 0) assert(revokeIoQualification(1, 2, IoPurpose::CONFIGURE));
            else if (change == 1) app->ioQualifications[1].maximumAgeUs = 1; // The stated age expires.
            else app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)].value.rawInputs = 4; // X2 limit.
            settle();
            ESS::DriverContext c; assert(copySettingsResult(id, c));
            assert(c.outcome == ESS::DriverOutcome::UPDATE_NOT_ADMITTED && c.kind == ESS::DriverKind::READ &&
                   !std::strcmp(c.status.msg, reasons[change]));
            char text[64]; std::snprintf(text, sizeof(text), "\"reason\":\"%s\"", reasons[change]); contains(text);
            contains("\"outcome\":\"update_not_admitted\"");
            assert(frames.size() == before + 1 && frames[before][1] == 3 && writes() == 0);
            assert(drives[1].regs[ROUTES[route].reg] == ROUTES[route].baseline);
            releaseOperation(id);
        }
}

// Stale or incomplete facts refuse before TX.
void testStaleAndIncompleteFacts() {
    const Route& start = ROUTES[0];
    boardStartup(); freshReads(); install("configure");
    app->ioQualifications[2].maximumAgeUs = 1; pump(2); // Expired stated age.
    refused(start, "input_facts_stale", nullptr, "x2"); contains("\"read_first\":\"read config\"");
    // A configuration read that shows a changed function clears the facts.
    freshReads(); install("configure");
    drives[1].regs[0x0042] = 0; consoleRead("@105 read config\n"); consoleRead("@106 read state\n");
    assert(!app->ioQualifications[1].target.id);
    refused(start, "input_facts_required");
    drives[1].regs[0x0042] = 2;
    // A wiring change clears them and leaves the terminal UNKNOWN.
    freshReads(); install("configure");
    command("@107 wiring x2 unknown\n"); contains("\"result\":\"done\"");
    auto before = frames.size();
    command("@108 motion-profile start-speed 31\n"); contains("\"ok\":false"); assert(frames.size() == before);
    for (const auto& route : ROUTES) { prepare(route); refused(route, "input_facts_required"); }
    command("@109 io qualification install 1 x2 3 0 configure unknown\n"); contains("\"ok\":false");
    // useaddr and host recovery clear them too.
    for (const char* change : {"@111 useaddr 2\n", "@112 recover\n"}) {
        boardStartup(); freshReads(); install("configure");
        command(change); runUntil([] { return !app->owner.recovering() && !app->owner.needsRecovery(); }); settle();
        if (!std::strcmp(change, "@111 useaddr 2\n")) command("@113 useaddr 1\n");
        freshReads(); assert(!app->ioQualifications[0].target.id);
        refused(start, "input_facts_required");
    }
    // Three of four records, then records of the other purpose.
    boardStartup(); freshReads(); install("configure", "", 3);
    refused(start, "input_facts_required", nullptr, "x3");
    install("segment");
    refused(start, "input_facts_required", "configure", "x0");
    prepare(ROUTES[6]); install("configure");
    refused(ROUTES[6], "input_facts_required", "segment", "x0");
}

// Live input levels: an active limit refuses every route; function 7 needs
// the stated trigger inhibition.
void testActiveInputsAndTriggerFunctions() {
    boardStartup();
    for (const auto& route : ROUTES) {
        prepare(route); install(route.purpose);
        drives[1].regs[0x0008] = 2; consoleRead("@114 read state\n"); // X1 limit active.
        refused(route, "input_active", nullptr, "x1"); contains("\"read_first\":\"read state\"");
        drives[1].regs[0x0008] = 0;
    }
    boardStartup(); drives[1].regs[0x0044] = 7; freshReads();
    command("@115 io qualification install 1 x3 7 0 configure unknown\n");
    contains("\"ok\":false,\"result\":\"qualification_required\"");
    install("configure", "", 3);
    refused(ROUTES[0], "input_facts_required", nullptr, "x3");
    install("configure", "triggers-inhibited");
    admitted(ROUTES[0], ROUTES[0].set, ROUTES[0].candidate);
}

// The profile stage takes the same CONFIGURE facts: an equal archive writes nothing.
void testProfileStage() {
    boardStartup(); freshReads(); consoleRead("@116 motion-profile read\n");
    auto before = frames.size();
    command("@117 motion-profile stage 100 100 60 0 5000 0 1\n");
    contains("\"result\":\"qualification_required\""); contains("\"reason\":\"input_facts_required\"");
    assert(frames.size() == before);
    install("configure");
    before = frames.size();
    command("@118 motion-profile stage 100 100 60 0 5000 0 1\n"); contains("\"ok\":true"); settle();
    contains("\"unchanged\":true");
    assert(frames.size() == before + 1 && frames[before][1] == 3 && writes() == 0);
}

// No board table: the passive unconnected path admits as before, without facts.
void testPassivePathUnchanged() {
    startWith(ApplicationOptions());
    writeResponseConfirmed = true;
    for (const auto& route : ROUTES) {
        if (route.reg == 0x0050) continue; // DRIVE fields other than subdivision never had a passive path.
        prepare(route);
        admitted(route, route.set, route.candidate);
    }
    prepare(ROUTES[3]); refused(ROUTES[3], "input_facts_required");
}
} // namespace

int main() {
    testConnectedWithoutFactsRefusedBeforeTx();
    testFactsAdmitEveryRoute();
    testRevalidationAfterPriorRead();
    testStaleAndIncompleteFacts();
    testActiveInputsAndTriggerFunctions();
    testProfileStage();
    testPassivePathUnchanged();
    reset();
    std::puts("Connected-terminal setter application tests passed");
    return 0;
}
