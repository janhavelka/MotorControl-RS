// SPDX-License-Identifier: MIT
// Wired bench 10 experiment admission (decision D4) in the experimental image
// (MOTORCONTROLRS_WB10_EXPERIMENT), through the actual application and its
// console on a fake bus with a simulated ESS drive: the pair experiment
// windows and candidate words, D7 snapshot restoration, the E3 soft-limit
// enable, the E1/E2 home offset admission and their refusals before TX. The
// production image refuses every one of these routes (home_app_test).
// The simulator says nothing about the real bench.
#define MOTORCONTROLRS_FIXTURE_WIRING 1
#define MOTORCONTROLRS_WB10_EXPERIMENT 1
#include "../examples/probe_cli/ProbeApp.cpp"
#include "fakes/esp32_uart/UsbConsoleFixture.h"
#include "../examples/probe_cli/ArduinoPlatform.cpp"
#include "../examples/probe_cli/main.cpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
using namespace MotorControlRS;

struct Drive {
    uint16_t regs[0x140] = {};
    int homePolls = -1;
    bool dropNextWrite = false, alterNextWrite = false;
};
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
    d.regs[0x0000] = 0x4EEA; d.regs[0x0001] = 0x0029; d.regs[0x0002] = address; d.regs[0x0003] = address;
    d.regs[0x0007] = 1;
    d.regs[0x0011] = 1000;
    d.regs[0x0041] = 1; d.regs[0x0042] = 2; d.regs[0x0043] = 3; d.regs[0x0044] = 0;
    const uint16_t control[] = {3, 4000, 5600, 100, 40, 100, 40, 200};
    for (unsigned i = 0; i < 8; ++i) d.regs[0x0100 + i] = control[i];
    d.regs[0x0031] = 24; d.regs[0x0032] = 120; d.regs[0x0033] = 60; d.regs[0x0034] = 100;
    for (unsigned n = 0; n < 16; ++n) {
        const uint16_t pt[] = {0, 5000, 120, 100, 100};
        for (unsigned i = 0; i < 5; ++i) d.regs[0x0060 + 6 * n + i] = pt[i];
    }
    drives[address] = d;
}
// Method 35 in the simulator: HOMED clears at the trigger and sets two polls
// later; the coordinate is 0 (auxiliary 7) or the offset (6, 3) read in the
// active 0x0019 order. Auxiliary 3 reports RUNNING meanwhile.
void homeDone(Drive& d) {
    const bool low = d.regs[0x0019] != 0;
    const uint32_t offset = low ? (uint32_t(d.regs[0x0036]) << 16 | d.regs[0x0035]) : (uint32_t(d.regs[0x0035]) << 16 | d.regs[0x0036]);
    const uint32_t position = d.regs[0x0030] == 7 ? 0 : offset;
    d.regs[0x000A] = low ? static_cast<uint16_t>(position) : static_cast<uint16_t>(position >> 16);
    d.regs[0x000B] = low ? static_cast<uint16_t>(position >> 16) : static_cast<uint16_t>(position);
    d.regs[0x0007] = static_cast<uint16_t>((d.regs[0x0007] & ~4u) | 3u);
    d.homePolls = -1;
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
        if (d.homePolls >= 0 && first <= 0x0007 && first + count > 0x0007 && d.homePolls-- == 0) homeDone(d);
        reply = {frame[0], 3, static_cast<uint8_t>(count * 2)};
        for (uint16_t i = 0; i < count; ++i) {
            const uint16_t r = static_cast<uint16_t>(first + i);
            const uint16_t value = r < 0x140 ? d.regs[r] : 0;
            reply.push_back(static_cast<uint8_t>(value >> 8)); reply.push_back(static_cast<uint8_t>(value));
        }
        reply = crc(reply);
    } else if (frame[1] == 6) {
        if (d.dropNextWrite) { d.dropNextWrite = false; return; }
        if (first == 0x0027 && word(frame, 4) == 16) {
            d.regs[0x0007] = static_cast<uint16_t>((d.regs[0x0007] & ~2u) | (d.regs[0x0030] == 3 ? 4u : 0u));
            d.homePolls = 2;
        } else if (first < 0x140) d.regs[first] = word(frame, 4);
        reply = frame;
    } else if (frame[1] == 0x10) {
        if (d.dropNextWrite) { d.dropNextWrite = false; return; }
        const uint16_t count = word(frame, 4);
        for (uint16_t i = 0; i < count; ++i) d.regs[first + i] = static_cast<uint16_t>(word(frame, 7 + 2 * i) ^ (d.alterNextWrite && i == 1 ? 1u : 0u));
        d.alterNextWrite = false;
        reply = crc(std::vector<uint8_t>(frame.begin(), frame.begin() + 6));
    } else return;
    scheduleReply(std::max(hardware.writeStarted + frame.size() * 87 + 1000, hardware.time + 1000), reply);
}
void step(uint32_t us = 10) { advanceHardware(hardware.time + us); serviceApplication(); serveBus(); }
void pump(unsigned count = 64) { for (unsigned i = 0; i < count; ++i) step(); }
template <typename Done> void runUntil(Done done, unsigned limit = 400000) {
    for (unsigned i = 0; i < limit && !done(); ++i) step(20);
    assert(done());
}
void boardStartup() {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = false; writeResponseConfirmed = false;
    drives.clear(); frames.clear(); servedWrites = 0;
    for (uint8_t address : {uint8_t(1), uint8_t(2), uint8_t(4)}) addDrive(address);
    setup();
    assert(app && uart.ready() && app->owner.valid() && hardware.writes == 0);
    hardware.txCharacterUs = 87;
    assert(uart.startCapture(20, timing().holdUs));
    assert(app->inputWiring[0] == InputWiring::CONNECTED && writeResponseConfirmed);
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
void settle() { runUntil([] { return !app->owner.active() && !app->owner.pending() && !reading(*app) && !acting(*app) &&
                                     !app->parameterWrite.view.pending; }); pump(200); }
uint32_t replyOperation() {
    const auto at = Serial.output.find("\"operation_id\":");
    return at == std::string::npos ? 0 : static_cast<uint32_t>(std::strtoul(Serial.output.c_str() + at + 15, nullptr, 10));
}
uint32_t consoleRead(const std::string& line) {
    command(line); const auto id = replyOperation(); settle();
    if (id) releaseOperation(id);
    return id;
}
void selectAndRead(uint8_t address) {
    command("@90 useaddr " + std::to_string(address) + "\n");
    for (const char* line : {"@100 read identity\n", "@101 read config\n", "@103 read state\n"}) consoleRead(line);
}
uint32_t driverRead() { return consoleRead("@104 driver read\n"); }
// Refused at admission: the reply names the result, zero frames, nothing stored.
void refused(const std::string& line, const char* result, uint8_t address = 4) {
    const auto before = frames.size(); const Drive saved = drives[address];
    command("@120 " + line + "\n");
    char text[64]; std::snprintf(text, sizeof(text), "\"ok\":false,\"result\":\"%s\"", result); contains(text);
    pump(200);
    if (frames.size() != before) { std::fprintf(stderr, "%s sent %zu frames\n", line.c_str(), frames.size() - before); assert(false); }
    assert(std::memcmp(drives[address].regs, saved.regs, sizeof(saved.regs)) == 0);
}
// Admitted: one FC10 (or FC06) and one FC03 of the window; returns the view.
Probe::ParameterWriteView admitted(const std::string& line) {
    const auto before = frames.size();
    command("@121 " + line + "\n"); contains("\"ok\":true");
    settle();
    Probe::ParameterWriteView v = app->parameterWrite.view;
    assert(!v.pending && frames.size() == before + 2);
    return v;
}

// Four CONFIGURE records with function and polarity from the simulated drive.
void installConfigure(uint8_t address) {
    const auto& r = drives[address].regs;
    for (unsigned i = 0; i < 4; ++i) {
        char line[128];
        std::snprintf(line, sizeof(line), "@110 io qualification install %u x%u %u %u configure unknown\n", address, i,
            r[0x0041 + i], (r[0x0040] >> i) & 1u);
        command(line); contains("\"ok\":true");
    }
}
std::size_t fc10Frames(std::size_t from) {
    std::size_t n = 0;
    for (std::size_t i = from; i < frames.size(); ++i) n += frames[i].size() > 1 && frames[i][1] == 0x10;
    return n;
}

// The PT experiment of the experimental image: candidate windows and words,
// the soft-limit windows no longer experimental (promoted), refusals before TX.
void testPtExperimentAndRefusals() {
    boardStartup(); selectAndRead(4);
    auto v = admitted("driver experiment pair 96 1 4097");
    const std::vector<uint8_t> expected = crc({4, 0x10, 0, 0x60, 0, 2, 4, 0, 1, 0x10, 0x01});
    assert(frames[frames.size() - 2] == expected && frames.back() == crc({4, 3, 0, 0x60, 0, 2}));
    assert(v.ok && v.kind == Probe::ParameterWriteKind::PAIR_EXPERIMENT && v.first == 0x60 && v.count == 2 &&
           v.readbackKnown && v.readback[0] == 1 && v.readback[1] == 0x1001 && !std::strcmp(v.error, "none"));
    // The possible write expired the configuration: a new write needs fresh reads.
    refused("driver experiment pair 96 65535 65036", "unavailable");
    selectAndRead(4);
    assert(admitted("driver experiment pair 96 65535 65036").ok && drives[4].regs[0x61] == 0xFE0C); // -500 at order 0.
    selectAndRead(4);
    assert(admitted("driver experiment pair 186 16 4112").ok && drives[4].regs[0xBB] == 0x1010); // Segment 16.
    selectAndRead(4);
    assert(admitted("driver experiment pair 186 0 5000").ok && drives[4].regs[0xBB] == 5000); // Baseline restoration.
    selectAndRead(4);
    refused("driver experiment pair 55 1 2", "unsupported");     // 0x0037/2 is a reviewed production window now.
    refused("driver experiment pair 57 65535 65036", "unsupported");
    refused("driver experiment pair 53 0 500", "unsupported");   // 0x0035 home offset alone.
    refused("driver experiment pair 36 0 5000", "unsupported");  // 0x0024.
    refused("driver experiment pair 96 1 4098", "unsupported");  // Not a listed candidate.
    refused("driver experiment pair 97 1 4097", "unsupported");  // Pair half.
    refused("driver experiment pair 102 1 4097", "unsupported"); // Segment 2 with segment 1 words.
    command("@122 driver experiment pair 96 1\n"); contains("invalid_arguments");
    command("@123 driver experiment limit-enable\n"); contains("invalid_arguments"); // E3's admission was removed.
    selectAndRead(1);
    refused("driver experiment pair 102 2 4098", "unsupported", 1); // Segment 2: drive 4 only.
    assert(admitted("driver experiment pair 96 1 4097").ok && drives[1].regs[0x61] == 0x1001); // P4 form on drive 1.
    drives[4].regs[0x0018] = 1; selectAndRead(4);
    refused("driver experiment pair 96 1 4097", "unavailable");
    drives[4].regs[0x0018] = 0; drives[4].regs[0x0044] = 12; selectAndRead(4);
    refused("driver experiment pair 96 1 4097", "unavailable");
    drives[4].regs[0x0044] = 0; drives[4].regs[0x0007] = 5; selectAndRead(4);
    refused("driver experiment pair 96 1 4097", "unavailable");
    drives[4].regs[0x0007] = 1; selectAndRead(4);
    command("@92 useaddr 1\n"); command("@93 useaddr 4\n"); // A rebinding expires the reads.
    refused("driver experiment pair 96 1 4097", "unavailable");
}

void testSnapshotRestoreAndUncertainty() {
    boardStartup(); selectAndRead(4);
    drives[4].regs[0x6C] = 0x1234; drives[4].regs[0x6D] = 0x5678;
    const uint32_t source = consoleRead("@105 segment position 3 read\n");
    assert(source && app->pairSnapshots[2][0].operationId == source && app->pairSnapshots[2][0].words[0] == 0x1234);
    selectAndRead(4); assert(admitted("driver experiment pair 108 3 4099").ok);
    const uint32_t later = consoleRead("@105 segment position 3 read\n"); // The pinned slot keeps the first read.
    assert(app->pairSnapshots[2][0].operationId == source && app->pairSnapshots[2][1].operationId == later);
    consoleRead("@105 segment position 3 read\n"); consoleRead("@105 segment position 3 read\n");
    assert(findPairSnapshot(*app, 0x6C, later) && findPairSnapshot(*app, 0x6C, source));
    consoleRead("@105 segment position 3 read\n");
    assert(!findPairSnapshot(*app, 0x6C, later) && findPairSnapshot(*app, 0x6C, source));
    selectAndRead(4);
    refused("driver experiment pair 108 restore " + std::to_string(source + 1000), "unavailable");
    refused("driver experiment pair 96 restore " + std::to_string(source), "unavailable"); // Another window's source.
    auto v = admitted("driver experiment pair 108 restore " + std::to_string(source));
    assert(v.ok && v.snapshotId == source && drives[4].regs[0x6C] == 0x1234 && drives[4].regs[0x6D] == 0x5678);
    // A new binding pins its own first read; the old source is stale.
    command("@91 useaddr 1\n"); selectAndRead(4);
    refused("driver experiment pair 108 restore " + std::to_string(source), "unavailable");
    // Readback mismatch: reported, never repaired by another write.
    selectAndRead(4); drives[4].alterNextWrite = true;
    v = admitted("driver experiment pair 96 1 4097");
    assert(!v.ok && !std::strcmp(v.error, "readback_mismatch") && !v.unsettled && v.readback[1] == 0x1000);
    pump(2000); assert(frames.back()[1] == 3);
    // Lost acknowledgement: unknown, never resent; the next write waits for a covering read.
    selectAndRead(4); drives[4].dropNextWrite = true;
    const auto before = frames.size();
    command("@124 driver experiment pair 96 1 4097\n"); contains("\"ok\":true");
    runUntil([] { return !app->parameterWrite.view.pending; });
    v = app->parameterWrite.view;
    assert(!v.ok && v.unsettled && v.execution == ActionExecution::UNKNOWN && !std::strcmp(v.error, "write_unknown"));
    pump(5000); assert(frames.size() == before + 1);
    uint32_t recovery = 0; assert(recover(app, 0, recovery) == Probe::Action::OK); // Host recovery is not a readback.
    runUntil([recovery] { Probe::ResultView r; return host(app).result(app, recovery, r) && !r.pending; });
    releaseOperation(recovery);
    assert(app->parameterWrite.view.unsettled);
    selectAndRead(4); refused("driver experiment pair 96 1 4097", "busy");
    consoleRead("@106 segment position 2 read\n"); // Does not cover the window.
    assert(app->parameterWrite.view.unsettled);
    consoleRead("@106 segment position 1 read\n"); assert(!app->parameterWrite.view.unsettled);
    selectAndRead(4); assert(admitted("driver experiment pair 96 1 4097").ok);
}

// Production soft-limit setter (wired bench 10 promotion): one FC10 of two words
// and one two-word FC03 per limit, at 0x0019 and 0x0018 known 0, CONFIGURE
// records for the connected terminals; decoded reads at order 0 only.
void testSoftLimitSetter() {
    boardStartup(); selectAndRead(4); installConfigure(4); driverRead();
    std::size_t before = frames.size();
    command("@130 driver set positive-limit 300 negative-limit -300\n"); contains("\"ok\":true");
    settle();
    assert(fc10Frames(before) == 2 && drives[4].regs[0x37] == 0 && drives[4].regs[0x38] == 300 &&
           drives[4].regs[0x39] == 0xFFFF && drives[4].regs[0x3A] == 0xFED4);
    bool sawPositive = false, sawNegative = false;
    for (std::size_t i = before; i < frames.size(); ++i) {
        sawPositive = sawPositive || frames[i] == crc({4, 0x10, 0, 0x37, 0, 2, 4, 0, 0, 0x01, 0x2C});
        sawNegative = sawNegative || frames[i] == crc({4, 0x10, 0, 0x39, 0, 2, 4, 0xFF, 0xFF, 0xFE, 0xD4});
    }
    assert(sawPositive && sawNegative);
    selectAndRead(4); command("@131 driver read\n"); settle();
    contains("\"limits_decoded\":true"); contains("\"positive_limit\":300"); contains("\"negative_limit\":-300");
    contains("\"signed_limits\":\"measured_order_0\"");
    // 0x0018 known 1, order 1, out of range, pair plus geometry: refused before TX.
    drives[4].regs[0x18] = 1; selectAndRead(4); installConfigure(4); driverRead();
    before = frames.size();
    command("@132 driver set positive-limit 0\n"); pump(500); assert(fc10Frames(before) == 0);
    drives[4].regs[0x18] = 0; drives[4].regs[0x19] = 1; selectAndRead(4); installConfigure(4); driverRead();
    contains("\"limits_decoded\":false");
    command("@133 driver set positive-limit 0\n"); contains("\"result\":\"unsupported\""); pump(500); assert(fc10Frames(before) == 0);
    drives[4].regs[0x19] = 0; selectAndRead(4); installConfigure(4); driverRead();
    command("@134 driver set positive-limit 268435456\n"); pump(500); assert(fc10Frames(before) == 0);
    command("@135 driver set positive-limit 0 direction 1\n"); pump(500); assert(fc10Frames(before) == 0);
    // Restore through the same setter, read back.
    command("@136 driver set positive-limit 0 negative-limit 0\n"); contains("\"ok\":true"); settle();
    assert(drives[4].regs[0x38] == 0 && drives[4].regs[0x3A] == 0 && fc10Frames(before) == 2);
}

void qualifyHome(const char* facts) {
    consoleRead("@107 home read\n");
    command(std::string("@108 home qualification install 35 60 30 100 mechanics rates ramp zero-offset auxiliary inputs reference ") + facts + "\n");
}
Probe::ResultView homeResult(uint32_t id) { Probe::ResultView v; assert(host(app).result(app, id, v)); return v; }
// Production nonzero home offset (wired bench 10 E1): method 35, stored
// auxiliary 6, order 0, the `offset` fact; auxiliary 3 and order 1 refused.
void testHomeOffset() {
    boardStartup(); selectAndRead(4);
    drives[4].regs[0x30] = 6; // Stored auxiliary 6 (`home auxiliary 6`).
    qualifyHome(""); contains("stored_auxiliary_differs");
    qualifyHome("offset-motion"); contains("unknown_fact");
    qualifyHome("offset"); contains("\"ok\":true");
    command("@125 home 35 60 30 100 268435456\n"); contains("invalid_arguments");
    for (const char* offset : {"500", "-500", "123456"}) {
        selectAndRead(4); qualifyHome("offset"); contains("\"ok\":true");
        command(std::string("@126 home 35 60 30 100 ") + offset + "\n"); contains("\"ok\":true");
        const auto id = replyOperation(); settle();
        const auto v = homeResult(id);
        assert(v.homeContext && v.homeContext->state == ActionState::SUCCEEDED && v.homeContext->endKnown &&
               v.homeContext->endPosition == std::atoi(offset));
        releaseOperation(id);
    }
    assert(drives[4].regs[0x35] == 0x0001 && drives[4].regs[0x36] == 0xE240); // 123456 high word first.
    // Order 1: refused (measured: 0x0019 = 1 is not applied to the offset in RAM).
    drives[4].regs[0x19] = 1; selectAndRead(4); qualifyHome("offset"); contains("\"ok\":true");
    refused("home 35 60 30 100 500", "unresolved", 4);
    // Auxiliary 3 is refused at qualification (E2), any other drive works the same way.
    drives[4].regs[0x19] = 0; drives[4].regs[0x30] = 3; selectAndRead(4);
    qualifyHome("offset"); contains("stored_auxiliary_differs");
    drives[1].regs[0x30] = 6; selectAndRead(1); qualifyHome("offset"); contains("\"ok\":true");
    command("@127 home 35 60 30 100 -500 1\n"); contains("\"ok\":true");
    const auto id = replyOperation(); settle();
    assert(homeResult(id).homeContext->state == ActionState::SUCCEEDED && drives[1].regs[0x36] == 0xFE0C);
    releaseOperation(id);
}
} // namespace

int main() {
    testPtExperimentAndRefusals();
    testSnapshotRestoreAndUncertainty();
    testSoftLimitSetter();
    testHomeOffset();
    std::puts("paired values app test passed");
    return 0;
}
