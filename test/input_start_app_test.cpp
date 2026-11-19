// SPDX-License-Identifier: MIT
// Input-start route (wired bench 08) through the actual cooperative application,
// its console and public API, on a fake bus with simulated ESS drives and a fake
// BenchIo::Port. The simulator's function-7 behaviour (one start per rising
// edge, optional repeats) and its function-12 PT behaviour (the record the
// PIN levels select starts on the trigger edge) belong to this fixture only and
// say nothing about the real bench; prompts 09 and 13 measure that. Both
// platform variants run it.
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
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace MotorControlRS;
using Phase = Probe::InputStartPhase;
using Outcome = ESS::InputStartOutcome;

// ------------------------------------------------------------ fake bench port
const int8_t OUTPUT_TERMINAL[4] = {2, 1, 0, 3}; // relay1 X2, relay2 X1, mosfet1 X0, mosfet2 X3.
struct FakeBench final : BenchIo::Port {
    BenchIo::OutputView outputs[BenchIo::OUTPUTS];
    unsigned applies = 0, onCalls = 0, offCalls = 0, quiesces = 0;
    bool failOn = false, stuck = false;
    uint32_t inputEdges = 0, inputOverwritten = 0; // aux1 ring counters (BenchIo::InputView).
    std::function<void(uint8_t)> rising; // Simulator hook: the drives see an input edge.
    uint64_t now() const { return hardware.time; }
    void view(BenchIo::View& v) const {
        v = BenchIo::View(); v.ready = true; v.nowUs = now();
        for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i) v.outputs[i] = outputs[i];
        v.input.known = true; v.input.edges = inputEdges; v.input.overwritten = inputOverwritten;
    }
    void off(uint8_t i, BenchIo::OffReason reason) {
        if (!outputs[i].active || stuck) return;
        outputs[i].active = false; outputs[i].offUs = now(); outputs[i].offReason = reason;
    }
    BenchIo::Result apply(const BenchIo::Request& r, BenchIo::View& v) override {
        ++applies;
        if (r.op == BenchIo::Op::ON) {
            ++onCalls;
            if (failOn) { view(v); return BenchIo::Result::FAILED; }
            auto& o = outputs[r.output];
            const bool was = o.active;
            o.active = true; o.holdMs = r.holdMs; o.onUs = now(); o.dueUs = now() + r.holdMs * 1000ull;
            o.offReason = BenchIo::OffReason::NONE;
            if (!was) { ++o.ons; if (rising) rising(r.output); }
        } else if (r.op == BenchIo::Op::OFF) { ++offCalls; off(r.output, BenchIo::OffReason::REQUEST); }
        else if (r.op == BenchIo::Op::OFF_ALL) { ++offCalls; for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i) off(i, BenchIo::OffReason::REQUEST); }
        view(v);
        return BenchIo::Result::OK;
    }
    void quiesce(BenchIo::OffReason reason) override { ++quiesces; for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i) off(i, reason); }
    void service(uint64_t at) override {
        for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i)
            if (outputs[i].active && at >= outputs[i].dueUs) off(i, BenchIo::OffReason::LEASE);
    }
    uint16_t inputs() const {
        uint16_t bits = 0;
        for (uint8_t i = 0; i < BenchIo::OUTPUTS; ++i) if (outputs[i].active) bits |= static_cast<uint16_t>(1u << OUTPUT_TERMINAL[i]);
        return bits;
    }
} bench;

// ------------------------------------------------------- simulated ESS drives
struct Drive {
    bool present = true;
    uint16_t regs[0x140] = {};
    unsigned runningReads = 0, starts = 0, repeats = 0, restartIn = 0; ///< repeats: further starts while the input stays active.
    int32_t pending = 0;
    bool runForever = false;
    bool ptRepeat = false; ///< repeats restart the PT record while X0 stays active.
    uint8_t lastRecord = 0;
    // Wired bench 14: a speed, JOG or PV run, and whether 0x0008 reports the level after inversion.
    bool velocityRun = false, afterInversion = false;
    uint8_t velocityTerminal = 0;
    int32_t velocityStep = 0;
};
std::map<uint8_t, Drive> drives;
std::vector<std::vector<uint8_t>> frames;
std::function<uint32_t(const std::vector<uint8_t>&)> replyDelay;
unsigned servedWrites = 0;
std::vector<uint8_t> crc(std::vector<uint8_t> bytes) {
    const uint16_t value = ESS::calcCrc16(bytes.data(), bytes.size());
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8));
    return bytes;
}
uint16_t word(const std::vector<uint8_t>& frame, std::size_t at) { return static_cast<uint16_t>(frame[at] << 8 | frame[at + 1]); }
// Pairs follow the drive's 0x0019 word order (1: low word at the lower address).
int32_t pair(const Drive& d, uint16_t first) {
    const uint16_t high = d.regs[0x0019] ? d.regs[first + 1] : d.regs[first], low = d.regs[0x0019] ? d.regs[first] : d.regs[first + 1];
    return static_cast<int32_t>(static_cast<uint32_t>(high) << 16 | low);
}
int32_t position(const Drive& d) { return pair(d, 0x000A); }
void setPosition(Drive& d, int32_t value) {
    const uint16_t high = static_cast<uint16_t>(static_cast<uint32_t>(value) >> 16), low = static_cast<uint16_t>(value);
    d.regs[0x000A] = d.regs[0x0019] ? low : high; d.regs[0x000B] = d.regs[0x0019] ? high : low;
}
void addDrive(uint8_t address) {
    Drive d;
    d.regs[0x0000] = 0x4EEA; d.regs[0x0001] = 0x0029; d.regs[0x0002] = address;
    d.regs[0x0007] = 1;
    d.regs[0x0010] = 0; d.regs[0x0011] = 1000; d.regs[0x0013] = address; d.regs[0x0019] = 1;
    d.regs[0x0040] = 0; d.regs[0x0041] = 1; d.regs[0x0042] = 2; d.regs[0x0043] = 3; d.regs[0x0044] = 0;
    d.regs[0x0100] = 2; d.regs[0x0101] = 4000;
    d.regs[0x0020] = 10; d.regs[0x0021] = 100; d.regs[0x0022] = 100; d.regs[0x0023] = 60;
    setPosition(d, 500);
    drives[address] = d;
}
// Function 7 on the switched terminal starts the staged profile on the rising edge.
void startOn(Drive& d) {
    const int32_t target = pair(d, 0x0024);
    d.pending = d.regs[0x0050] ? target : position(d) + target;
    d.runningReads = 2; ++d.starts;
}
// Function 12 on the switched terminal starts the record the PIN0..PIN3 levels select.
void startPt(Drive& d) {
    const uint16_t inputs = bench.inputs();
    uint8_t record = 1;
    for (uint8_t t = 0; t < 4; ++t) {
        const uint16_t f = d.regs[0x0041 + t];
        if (f >= 14 && f <= 17 && ((inputs >> t) & 1u)) record = static_cast<uint8_t>(record + (1u << (f - 14)));
    }
    const int32_t target = pair(d, static_cast<uint16_t>(0x0060 + 6 * (record - 1)));
    d.pending = d.regs[0x0050] ? target : position(d) + target;
    d.runningReads = 2; ++d.starts; d.lastRecord = record;
}
// Functions 8-10 and 13 run while their input is active (13 with 0x0051 = 1:
// until a stop): 9 forward, 10 backward, 8 by the sign of 0x001D, 13 by the
// sign of the selected PV record (simulator only; wired bench 14 measures it).
void startVelocity(Drive& d, uint8_t terminal) {
    const uint16_t function = d.regs[0x0041 + terminal];
    int16_t speed = static_cast<int16_t>(d.regs[0x001D]);
    if (function == 13) {
        const uint16_t inputs = bench.inputs();
        uint8_t record = 1;
        for (uint8_t t = 0; t < 4; ++t) {
            const uint16_t f = d.regs[0x0041 + t];
            if (f >= 14 && f <= 17 && ((inputs >> t) & 1u)) record = static_cast<uint8_t>(record + (1u << (f - 14)));
        }
        speed = static_cast<int16_t>(d.regs[0x00C0 + 3 * (record - 1)]); d.lastRecord = record;
    }
    d.velocityStep = function == 9 ? 10 : function == 10 ? -10 : speed < 0 ? -10 : 10;
    d.velocityRun = true; d.velocityTerminal = terminal; ++d.starts;
}
void edge(uint8_t output) {
    for (auto& entry : drives) {
        auto& d = entry.second;
        const uint16_t function = d.regs[0x0041 + OUTPUT_TERMINAL[output]];
        if (d.present && function == 7) startOn(d);
        if (d.present && function == 12) startPt(d);
        if (d.present && ((function >= 8 && function <= 10) || function == 13)) startVelocity(d, static_cast<uint8_t>(OUTPUT_TERMINAL[output]));
    }
}
// The effective level of terminal t: the source, inverted by polarity 1.
bool effectiveLevel(const Drive& d, uint8_t t) { return (((bench.inputs() ^ d.regs[0x0040]) >> t) & 1u) != 0; }
uint16_t readWord(Drive& d, uint16_t reg) {
    if (reg == 0x0008)
        return static_cast<uint16_t>((d.afterInversion ? bench.inputs() ^ d.regs[0x0040] : bench.inputs()) | d.regs[0x0008]);
    if (reg == 0x0007 && d.velocityRun) {
        bool stopped = false;
        for (uint8_t t = 0; t < 4; ++t) stopped = stopped || (d.regs[0x0041 + t] == 5 && effectiveLevel(d, t));
        const bool edgePv = d.regs[0x0041 + d.velocityTerminal] == 13 && d.regs[0x0051] == 1;
        if (!stopped && (edgePv || effectiveLevel(d, d.velocityTerminal))) {
            setPosition(d, position(d) + d.velocityStep); d.regs[0x000C] = 60; return 4;
        }
        d.velocityRun = false; d.regs[0x000C] = 0; return 1;
    }
    if (reg == 0x0007 && d.restartIn) { // One stopped report, then the repeated start.
        if (--d.restartIn) return 1;
        if (d.ptRepeat) startPt(d); else startOn(d);
        return 4;
    }
    if (reg == 0x0007 && (d.runningReads || d.runForever)) {
        if (d.runForever) return 4;
        if (!--d.runningReads) {
            setPosition(d, d.pending);
            if (d.repeats && (bench.inputs() & (1u << (d.ptRepeat ? 0 : 3)))) { --d.repeats; d.restartIn = 2; }
            return 1;
        }
        return 4;
    }
    return reg < 0x140 ? d.regs[reg] : 0;
}
void writeWord(Drive& d, uint16_t reg, uint16_t value) {
    if (reg >= 0x140) return;
    d.regs[reg] = value;
    if (reg == 0x0027 && (value & 0x0300)) { d.runningReads = 0; d.runForever = false; d.velocityRun = false; d.regs[0x000C] = 0; } // Stop.
}
void serveBus() {
    if (hardware.writes == servedWrites) return;
    servedWrites = hardware.writes;
    const auto frame = hardware.tx;
    frames.push_back(frame);
    if (frame.size() < 8) return;
    const auto found = drives.find(frame[0]);
    if (found == drives.end() || !found->second.present) return;
    auto& d = found->second;
    const uint16_t first = word(frame, 2);
    std::vector<uint8_t> reply;
    if (frame[1] == 3) {
        const uint16_t count = word(frame, 4);
        reply = {frame[0], 3, static_cast<uint8_t>(count * 2)};
        for (uint16_t i = 0; i < count; ++i) {
            const uint16_t value = readWord(d, static_cast<uint16_t>(first + i));
            reply.push_back(static_cast<uint8_t>(value >> 8)); reply.push_back(static_cast<uint8_t>(value));
        }
        reply = crc(reply);
    } else if (frame[1] == 6) { writeWord(d, first, word(frame, 4)); reply = frame; }
    else if (frame[1] == 16) {
        const uint16_t count = word(frame, 4);
        for (uint16_t i = 0; i < count; ++i) writeWord(d, static_cast<uint16_t>(first + i), word(frame, 7 + 2 * i));
        reply = crc(std::vector<uint8_t>(frame.begin(), frame.begin() + 6));
    } else return;
    scheduleReply(std::max(hardware.writeStarted + frame.size() * 87 + 1000, hardware.time + 1000) +
        (replyDelay ? replyDelay(frame) : 0), reply);
}
void step(uint32_t us = 20) { advanceHardware(hardware.time + us); serviceApplication(); serveBus(); }
void pump(unsigned count = 64) { for (unsigned i = 0; i < count; ++i) step(); }
template <typename Done> void runUntil(Done done, unsigned limit = 600000) {
    for (unsigned i = 0; i < limit && !done(); ++i) step(20);
    assert(done());
}
void fresh(const std::vector<uint8_t>& present = {1, 2, 4, 5}) {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = false;
    drives.clear(); frames.clear(); replyDelay = nullptr; servedWrites = 0;
    bench.~FakeBench(); new (&bench) FakeBench(); bench.rising = edge;
    for (const auto address : present) addDrive(address);
    ApplicationOptions options; options.moveTimeoutMs = 3000; options.benchIo = &bench;
    options.fixtureWiringSupplied = true; options.fixtureWiringCount = 4;
    const FixtureWiringEntry table[4] = {{1, 0xF, 0x0}, {2, 0xF, 0x0}, {4, 0xF, 0x1}, {5, 0xF, 0x0}};
    for (unsigned i = 0; i < 4; ++i) options.fixtureWiring[i] = table[i];
    options.benchOutputTerminalsSupplied = true;
    for (unsigned i = 0; i < 4; ++i) options.benchOutputTerminals[i] = OUTPUT_TERMINAL[i];
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit, options));
    hardware.txCharacterUs = 87;
    assert(uart.startCapture(20, timing().holdUs));
    writeResponseConfirmed = true; // Simulated fixture topology only.
}
void command(const std::string& text) {
    Serial.input = text; Serial.output.clear();
    for (unsigned i = 0; i < 500 && !Serial.input.empty(); ++i) step();
    assert(Serial.input.empty()); pump();
}
bool said(const char* text) { return Serial.output.find(text) != std::string::npos; }
// Idle-only host commands (wiring, useaddr) wait for a gap between the session's samples.
void idleCommand(const std::string& text) {
    for (unsigned i = 0; i < 200; ++i) {
        command(text);
        if (said("\"ok\":true")) return;
        step(3000);
    }
    assert(false);
}
void contains(const char* text) {
    if (!said(text)) { std::fprintf(stderr, "missing %s in:\n%s\n", text, Serial.output.c_str()); assert(false); }
}
uint32_t latest() { return app->latestOperationId; }
void finish(uint32_t id) {
    runUntil([&] { const auto* r = findRecord(*app, id); return !r || terminal(*app, *r); });
    pump(); releaseOperation(id);
}
bool isStart(const std::vector<uint8_t>& f) { return f.size() >= 8 && f[1] == 6 && word(f, 2) == 0x0027 && (word(f, 4) & 1); }
bool isWrite(const std::vector<uint8_t>& f) { return f.size() >= 8 && (f[1] == 6 || f[1] == 16); }
unsigned count(const std::function<bool(const std::vector<uint8_t>&)>& match, std::size_t from = 0) {
    unsigned n = 0;
    for (std::size_t i = from; i < frames.size(); ++i) if (match(frames[i])) ++n;
    return n;
}
// Fresh facts of the selected axis as the guide's sequence: config, DRIVE
// settings (0x0050), motion profile, state, then the FINITE_MOVE record.
void selectedFacts(uint8_t address, uint16_t mode = 0, const char* facts = "triggers-inhibited") {
    if (app->axis.target.address != address) {
        command("@1 useaddr " + std::to_string(address) + "\n"); contains("\"ok\":true");
    }
    auto& drive = drives[address];
    if (drive.runningReads) { setPosition(drive, drive.pending); drive.runningReads = 0; } // An earlier move has ended.
    drive.regs[0x0044] = 7; drive.regs[0x0050] = mode;
    for (const char* line : {"@2 read config\n", "@3 driver read\n", "@4 motion-profile read\n", "@5 read state\n"}) {
        command(line);
        const auto id = latest(); finish(id);
    }
    command("@6 io qualification install " + std::to_string(address) + " x3 7 0 finite-move unknown " + facts + "\n");
}
const Probe::InputStartView& session() { return app->inputStart.view; }
uint32_t arm(const std::string& args) {
    command("@20 arm position " + args + "\n");
    if (!said("\"result\":\"accepted\"")) { std::fprintf(stderr, "arm refused: %s\n", Serial.output.c_str()); assert(false); }
    const auto id = session().operationId;
    runUntil([] { return session().phase != Phase::STAGING; });
    assert(session().phase == Phase::ARMED);
    return id;
}
void trigger(uint32_t id, const char* output = "mosfet2", uint32_t width = 200) {
    command("@21 arm trigger " + std::to_string(id) + " " + output + " " + std::to_string(width) + "\n");
    contains("\"result\":\"accepted\"");
    runUntil([] { return session().phase != Phase::TRIGGERING; });
}
void done() { runUntil([] { return session().phase == Phase::DONE; }, 2000000); pump(); }
const ESS::InputStartResult& result(uint8_t index = 0) { return session().members[index].result; }
void refusedBeforeTx(const std::string& line, const char* reason) {
    const auto before = frames.size(); const auto ons = bench.onCalls; const auto id = session().operationId;
    command(line);
    if (!said(reason)) { std::fprintf(stderr, "expected %s: %s\n", reason, Serial.output.c_str()); assert(false); }
    contains("\"ok\":false");
    pump(200);
    if (frames.size() != before || bench.onCalls != ons || session().operationId != id) {
        std::fprintf(stderr, "traffic after refusal of %s: %u frames, %u ons, session %u -> %u\n%s\n", line.c_str(),
            static_cast<unsigned>(frames.size() - before), bench.onCalls - ons, static_cast<unsigned>(id),
            static_cast<unsigned>(session().operationId), Serial.output.c_str());
        assert(false);
    }
}

void testSelectedRelativeStartIsAttributed() {
    fresh(); selectedFacts(1);
    const auto before = frames.size();
    const auto id = arm("x3 relative 1000 60");
    // Staged: one FC10 of 0x0021/5 and its FC03 readback; never a start word.
    assert(count([](const std::vector<uint8_t>& f) { return f[1] == 16 && word(f, 2) == 0x0021 && word(f, 4) == 5; }, before) == 1);
    assert(count([](const std::vector<uint8_t>& f) { return f[1] == 3 && word(f, 2) == 0x0021 && word(f, 4) == 5; }, before) == 1);
    assert(!count(isStart));
    assert(session().members[0].staged && session().members[0].expectedDisplacement == 1000);
    // The owned output refuses an ordinary bench on; off always works.
    command("@30 bench on mosfet2 100\n"); contains("\"result\":\"busy\""); assert(!bench.onCalls);
    command("@31 bench off mosfet2\n"); contains("\"ok\":true");
    trigger(id);
    const auto& v = session();
    assert(v.phase == Phase::OBSERVING && v.admitted && v.triggered && bench.onCalls == 1);
    assert(v.rowCount == 4 && v.rows[0].address == 2 && v.rows[1].address == 4 && v.rows[2].address == 5 && v.rows[3].address == 1);
    for (unsigned i = 0; i < 3; ++i) assert(!std::strcmp(v.rows[i].row, "L7") && v.rows[i].admit);
    assert(v.rows[3].member && !std::strcmp(v.rows[3].row, "M") && v.rows[3].stateAgeUs < 1000000);
    // The isolation reads were the last traffic before the ON: io then state per drive.
    done();
    const auto& r = result();
    assert(r.outcome == Outcome::ATTRIBUTED && !r.missing && r.positionChange == 1000 && r.positionMatches);
    assert(r.execution == ESS::InputStartExecution::OBSERVED && r.completion == ESS::InputStartCompletion::FINITE_STOPPED);
    assert(r.further == ESS::InputStartFurther::NONE_SEEN && r.changeStartUs <= v.onUs && r.changeEndUs > v.onUs);
    assert(r.spacingCount > 30 && r.spacingMinUs && r.spacingMaxUs <= 2 * 50000u && !r.lateSamples);
    assert(v.source == Probe::InputSourceState::HOLD_ELAPSED && bench.onCalls == 1 && drives[1].starts == 1);
    assert(!count(isStart)); // The drive input started it; the host sent no start.
    command("@40 arm inspect " + std::to_string(id) + "\n");
    contains("\"outcome\":\"attributed\""); contains("\"phase\":\"done\""); contains("\"row\":\"M\"");
    // A second trigger is refused; release frees the session.
    command("@41 arm trigger " + std::to_string(id) + " mosfet2 200\n"); contains("\"ok\":false");
    assert(bench.onCalls == 1 && releaseOperation(id) && !session().operationId);
    std::printf("input_start_app: session %u bytes, App %u bytes, %u contexts of %u bytes\n",
        static_cast<unsigned>(sizeof(App::InputStartSession)), static_cast<unsigned>(sizeof(App)),
        static_cast<unsigned>(Probe::INPUT_START_MEMBERS), static_cast<unsigned>(sizeof(ESS::InputStartContext)));
}
void testArmRefusalsBeforeTx() {
    fresh();
    refusedBeforeTx("@1 arm home x3\n", "home_staging_prompt_12");
    refusedBeforeTx("@1 arm segment x3 relative 1\n", "target_unknown"); // Wired bench 13: delivered, facts first.
    refusedBeforeTx("@1 arm velocity x3\n", "target_unknown"); // Wired bench 14: delivered, facts first.
    refusedBeforeTx("@1 arm pv x0 1 pin3\n", "target_unknown");
    selectedFacts(1);
    drives[1].regs[0x0044] = 0; command("@7 read config\n"); finish(latest());
    refusedBeforeTx("@8 arm position x3 relative 1000 60\n", "function_not_7");
    fresh(); selectedFacts(1, 1);
    refusedBeforeTx("@8 arm position x3 relative 1000 60\n", "position_mode_mismatch");
    fresh(); drives[1].regs[0x0044] = 7;
    for (const char* line : {"@2 read config\n", "@4 motion-profile read\n", "@5 read state\n"}) { command(line); finish(latest()); }
    command("@6 io qualification install 1 x3 7 0 finite-move unknown triggers-inhibited\n");
    refusedBeforeTx("@8 arm position x3 relative 1000 60\n", "position_mode_unknown");
    fresh(); selectedFacts(1, 0, "");
    refusedBeforeTx("@8 arm position x3 relative 1000 60\n", "triggers_inhibited_required");
    fresh(); drives[1].regs[0x0040] = 8; selectedFacts(1, 0, "triggers-inhibited");
    refusedBeforeTx("@8 arm position x3 relative 1000 60\n", "\"reason\":\"");
    fresh(); selectedFacts(1); drives[1].regs[0x0008] = 8; command("@9 read state\n"); finish(latest());
    refusedBeforeTx("@8 arm position x3 relative 1000 60\n", "input_active");
    for (const auto& c : std::vector<std::pair<std::function<void()>, const char*>>{
             {[] { drives[1].regs[0x0006] = 5; }, "alarm"}, {[] { drives[1].regs[0x0007] = 16; }, "released"},
             {[] { drives[1].regs[0x000C] = 7; }, "moving"}}) {
        fresh(); selectedFacts(1); c.first(); command("@9 read state\n"); finish(latest());
        refusedBeforeTx("@8 arm position x3 relative 1000 60\n", c.second);
    }
    // Wired bench 11: an arm needs the soft limits known disabled, and no active limit input.
    fresh(); drives[1].regs[0x0018] = 1; selectedFacts(1);
    refusedBeforeTx("@8 arm position x3 relative 1000 60\n", "soft_limit_state_unknown");
    fresh(); drives[1].regs[0x0042] = 2; drives[1].regs[0x0008] = 2; selectedFacts(1);
    refusedBeforeTx("@8 arm position x3 relative 1000 60\n", "limit_input_active");
    fresh(); selectedFacts(1);
    refusedBeforeTx("@8 arm position x3 relative 1000 60 group\n", "group_not_declared");
    command("@10 bench on mosfet2 5000\n"); contains("\"ok\":true");
    refusedBeforeTx("@8 arm position x3 relative 1000 60\n", "output_active");
}
void testTriggerRefusals() {
    fresh(); selectedFacts(1);
    command("@20 arm position x3 relative 1000 60\n"); contains("\"result\":\"accepted\"");
    const auto id = session().operationId;
    command("@21 arm trigger " + std::to_string(id) + " mosfet2 200\n"); // Still staging: its own readback continues.
    contains("not_armed"); assert(!bench.onCalls);
    runUntil([] { return session().phase == Phase::ARMED; });
    const auto sid = std::to_string(id);
    for (const auto& c : std::vector<std::pair<std::string, const char*>>{
             {sid + " mosfet2 19", "width_out_of_range"}, {sid + " mosfet2 5001", "width_out_of_range"},
             {sid + " relay1 200", "relay_output"}, {sid + " mosfet1 200", "output_not_on_terminal"},
             {"99 mosfet2 200", "unknown_session"}}) {
        const auto before = bench.onCalls;
        command("@22 arm trigger " + c.first + "\n");
        if (!said(c.second)) { std::fprintf(stderr, "expected %s: %s\n", c.second, Serial.output.c_str()); assert(false); }
        assert(bench.onCalls == before && session().phase == Phase::ARMED);
    }
    for (const char* bad : {"@23 arm trigger\n", "@23 arm trigger 1 mosfet2\n", "@23 arm trigger 1 mosfet2 200 9\n",
                            "@23 arm position x4 relative 1 60\n", "@23 arm position x3 sideways 1 60\n",
                            "@23 arm position x3 relative 1 60 group group\n", "@23 arm position x3 relative 1 60 poll-ms 19\n",
                            "@23 arm position x3 relative 1 60 poll-ms 50 poll-ms 50\n", "@23 arm position x3 absolute 1 60 group\n",
                            "@23 arm position x3 relative 1 0\n", "@23 arm position x3 relative 1 60 extra\n", "@23 disarm\n",
                            "@23 disarm 1 2\n", "@23 arm inspect\n", "@23 arm\n"}) {
        command(bad); contains("invalid_arguments");
    }
    // Wired bench 09 Implement 1: the shortest reliable start pulse measured on
    // drive 4 was 10 ms (A3: 3/3 single starts at 10..200 ms, none at 5 ms); the
    // route's minimum keeps a 10 ms margin and is admitted at exactly 20 ms.
    static_assert(Probe::INPUT_START_MIN_WIDTH_MS == 20, "minimum start pulse from the A3 sweep");
    trigger(id, "mosfet2", Probe::INPUT_START_MIN_WIDTH_MS);
    assert(bench.outputs[3].holdMs == Probe::INPUT_START_MIN_WIDTH_MS);
    command("@24 arm trigger " + sid + " mosfet2 200\n"); contains("already_triggered");
    assert(bench.onCalls == 1);
    done();
}
// ------------------------------------------- shared line-isolation truth table
struct Json {
    enum Kind { NUL, BOOL, NUM, STR, ARR, OBJ } kind = NUL;
    bool b = false; double n = 0; std::string s;
    std::vector<Json> a; std::map<std::string, Json> o;
    const Json& operator[](const std::string& key) const { static const Json none; auto f = o.find(key); return f == o.end() ? none : f->second; }
};
struct Parser {
    const std::string& t; std::size_t i = 0;
    explicit Parser(const std::string& text) : t(text) {}
    void ws() { while (i < t.size() && std::isspace(static_cast<unsigned char>(t[i]))) ++i; }
    Json value() {
        ws(); Json v;
        if (t[i] == '{') {
            v.kind = Json::OBJ; ++i; ws();
            if (t[i] == '}') { ++i; return v; }
            for (;;) { ws(); const auto key = value().s; ws(); assert(t[i] == ':'); ++i; v.o[key] = value(); ws(); if (t[i++] == '}') break; }
        } else if (t[i] == '[') {
            v.kind = Json::ARR; ++i; ws();
            if (t[i] == ']') { ++i; return v; }
            for (;;) { v.a.push_back(value()); ws(); if (t[i++] == ']') break; }
        } else if (t[i] == '"') {
            v.kind = Json::STR; ++i;
            while (t[i] != '"') { if (t[i] == '\\') ++i; v.s += t[i++]; }
            ++i;
        } else if (!t.compare(i, 4, "true")) { v.kind = Json::BOOL; v.b = true; i += 4; }
        else if (!t.compare(i, 5, "false")) { v.kind = Json::BOOL; i += 5; }
        else if (!t.compare(i, 4, "null")) i += 4;
        else { v.kind = Json::NUM; std::size_t used = 0; v.n = std::stod(t.substr(i), &used); i += used; }
        return v;
    }
};
Json loadCases() {
    std::ifstream in(MOTORCONTROLRS_LINE_ISOLATION_CASES);
    assert(in && "test/line_isolation_cases.json");
    std::stringstream text; text << in.rdbuf();
    const std::string body = text.str();
    Parser parser(body);
    return parser.value();
}
void applyCaseDrive(uint8_t address, const Json& d) {
    auto& drive = drives[address];
    if (d["silent"].b) { drive.present = false; return; }
    const auto& io = d["io"].a;
    for (unsigned i = 0; i < 9 && i < io.size(); ++i) drive.regs[0x0040 + i] = static_cast<uint16_t>(io[i].n);
    if (d["running"].b) drive.runForever = true;
    if (d["raw_alarm"].kind == Json::NUM) drive.regs[0x0006] = static_cast<uint16_t>(d["raw_alarm"].n);
    if (d["alarm_flag"].b) drive.regs[0x0007] |= 8;
}
// The member is drive 5 on the same line; drives 1, 2 and 4 carry the case words.
void testLineIsolationTruthTable() {
    const Json cases = loadCases();
    assert(cases["terminal"].s == "x3" && cases["output"].s == "mosfet2" && cases["max_age_ms"].n == 1000);
    unsigned run = 0;
    for (const auto& c : cases["cases"].a) {
        if (c["harness_only"].b) continue;
        const std::string id = c["id"].s;
        const auto& expect = c["expect"];
        fresh(); selectedFacts(5);
        const auto session_id = arm("x3 relative 1000 60");
        for (const auto& entry : c["drives"].o) applyCaseDrive(static_cast<uint8_t>(std::stoi(entry.first)), entry.second);
        if (id == "LI07") {
            // The firmware reads in address order, so its oldest reads are the first
            // drive's. The age backstop is checked through the same decision with
            // the case's age, then end to end below (testReadAgeBackstop).
            Probe::InputStartRow row; row.address = 2; row.ioKnown = row.stateKnown = true;
            row.io[0] = 0; row.io[1] = 1; row.io[2] = 2; row.io[3] = 3; row.io[4] = 0; row.state[1] = 1;
            row.ioStartedUs = row.stateStartedUs = 10000000;
            decideRow(*app, row, 3, 10000000 + 1200000, 0);
            assert(!row.admit && !std::strcmp(row.row, expect["row"].s.c_str()));
            row.ioStartedUs = row.stateStartedUs = 10000000 + 300000;
            decideRow(*app, row, 3, 10000000 + 1200000, 0);
            assert(row.admit && !std::strcmp(row.row, "L7"));
            ++run; continue;
        }
        if (!c["pending"].a.empty()) {
            // An accepted operation on drive 1 still pending when the trigger runs.
            uint32_t pending = 0;
            assert(typedRead(app, 0, 1, ESS::ReadKind::STATE, pending) == Probe::Action::OK);
            const auto result = triggerInputStartIn(*app, session_id, 3, 200);
            assert(result == Probe::Action::BUSY && !std::strcmp(session().decisionRow, expect["row"].s.c_str()));
            assert(session().decisionAddress == static_cast<uint8_t>(expect["address"].n) && !bench.onCalls);
            ++run; continue;
        }
        trigger(session_id);
        const bool admit = expect["decision"].s == "admit";
        if (admit != session().admitted || std::strcmp(session().decisionRow, expect["row"].s.c_str()) ||
            session().decisionAddress != static_cast<uint8_t>(expect["address"].n)) {
            std::fprintf(stderr, "%s: firmware %s %s %u (%s), table %s %s %u\n", id.c_str(), session().admitted ? "admit" : "refuse",
                session().decisionRow, session().decisionAddress, session().decisionReason, expect["decision"].s.c_str(),
                expect["row"].s.c_str(), static_cast<unsigned>(expect["address"].n));
            assert(false);
        }
        assert(bench.onCalls == (admit ? 1u : 0u));
        if (!admit) assert(session().phase == Phase::ARMED || session().phase == Phase::DONE);
        ++run;
    }
    assert(run == 10); // LI01-LI09 and LI14.
}
void testReadAgeBackstop() {
    fresh(); selectedFacts(5);
    const auto id = arm("x3 relative 1000 60");
    // Slow isolation replies: the first drive's reads are over 1000 ms old at the decision.
    replyDelay = [](const std::vector<uint8_t>& f) -> uint32_t { return f[1] == 3 && (word(f, 2) == 0x0040 || word(f, 4) == 7) ? 190000u : 0u; };
    trigger(id);
    assert(!session().admitted && !std::strcmp(session().decisionRow, "L1") && session().decisionAddress == 1 && !bench.onCalls);
}
void testStopOfInputStartedMotion() {
    fresh(); selectedFacts(1);
    const auto id = arm("x3 relative 1000 60");
    trigger(id, "mosfet2", 3000); // Held level.
    drives[1].runForever = true;
    runUntil([] { return result().runningObserved; });
    command("@50 stop fast\n"); contains("\"result\":\"accepted\"");
    const auto stopId = latest();
    // The stop quiesced the held output before its admission; no second ON follows.
    assert(!bench.outputs[3].active && bench.outputs[3].offReason == BenchIo::OffReason::STOP && bench.onCalls == 1);
    runUntil([] { return session().source == Probe::InputSourceState::QUIESCED_BY_STOP; });
    done();
    assert(result().outcome == Outcome::STOPPED && result().stopId == stopId && result().stoppedAfterStop);
    assert(session().members[0].stopId == stopId && !std::strcmp(session().members[0].stopResult, "confirmed"));
    assert(bench.onCalls == 1 && drives[1].regs[0x0044] == 7); // Assignment unchanged: removal stays explicit.
}
void testDisarmAndStuckPort() {
    fresh(); selectedFacts(1);
    auto id = arm("x3 relative 1000 60");
    command("@60 disarm " + std::to_string(id) + "\n"); contains("\"ok\":true");
    assert(session().phase == Phase::DONE && !std::strcmp(session().error, "disarmed") && !bench.onCalls);
    assert(result().outcome == Outcome::CANCELLED && drives[1].regs[0x0044] == 7);
    assert(releaseOperation(id));
    // Disarm during a held pulse turns the output off; it sends no stop.
    id = arm("x3 relative 1000 60");
    trigger(id, "mosfet2", 3000);
    const auto writes = count(isWrite);
    command("@61 disarm " + std::to_string(id) + "\n");
    assert(!bench.outputs[3].active && session().source == Probe::InputSourceState::QUIESCED_BY_DISARM);
    pump(2000); assert(count(isWrite) == writes);
    assert(releaseOperation(id));
    // A port that cannot quiesce: the result keeps the source active, never prevention.
    refusedBeforeTx("@63 arm position x3 relative 1000 60\n", "state_stale"); // The ON expired the cached state.
    selectedFacts(1);
    id = arm("x3 relative 1000 60");
    trigger(id, "mosfet2", 3000);
    bench.stuck = true;
    command("@62 disarm " + std::to_string(id) + "\n");
    if (!(bench.outputs[3].active && session().source == Probe::InputSourceState::ACTIVE && result().sourceActive))
        std::fprintf(stderr, "stuck: active %d source %s result.sourceActive %d phase %s\n%s\n", bench.outputs[3].active,
            Probe::inputSourceName(session().source), result().sourceActive, Probe::inputStartPhaseName(session().phase), Serial.output.c_str());
    assert(bench.outputs[3].active && session().source == Probe::InputSourceState::ACTIVE && result().sourceActive);
    bench.stuck = false;
}
void testInvalidation() {
    fresh(); selectedFacts(1);
    auto id = arm("x3 relative 1000 60");
    // A settings write to the armed drive is refused by the reservation, with no frame.
    const auto before = frames.size();
    command("@70 io set x3 0\n"); contains("\"ok\":false"); pump(200);
    assert(count(isWrite, before) == 0 && session().phase == Phase::ARMED);
    idleCommand("@71 wiring x3 unknown\n"); // A host wiring change ends the session.
    runUntil([] { return session().phase == Phase::DONE; });
    assert(!std::strcmp(session().error, "invalidated") && result().outcome == Outcome::CANCELLED);
    assert(releaseOperation(id));
    command("@72 wiring x3 connected\n");
    selectedFacts(1);
    id = arm("x3 relative 1000 60");
    idleCommand("@73 useaddr 2\n");
    runUntil([] { return session().phase == Phase::DONE; });
    assert(result().outcome == Outcome::CANCELLED && !bench.onCalls);
}
void testStagingEdgeFailsArm() {
    fresh(); selectedFacts(1);
    command("@80 arm position x3 relative 1000 60\n"); contains("\"result\":\"accepted\"");
    // An ON of the owned output that bypasses the console during staging.
    BenchIo::Request on; on.op = BenchIo::Op::ON; on.output = 3; on.holdMs = 50; BenchIo::View v;
    bench.apply(on, v);
    runUntil([] { return session().phase == Phase::DONE; });
    assert(!std::strcmp(session().error, "trigger_during_staging") && !bench.outputs[3].active);
}
void testSecondStartAndUnattributedMotion() {
    fresh(); selectedFacts(1);
    auto id = arm("x3 relative 1000 60");
    drives[1].repeats = 1; // A second run while the level is held.
    trigger(id, "mosfet2", 1500);
    done();
    assert(result().outcome == Outcome::SECOND_START && result().further == ESS::InputStartFurther::SEEN);
    assert(!bench.outputs[3].active && bench.onCalls == 1); // Quiesced at the end; never retriggered.
    assert(releaseOperation(id));
    selectedFacts(1);
    id = arm("x3 relative 1000 60");
    startOn(drives[1]); // Motion before any edge.
    done();
    assert(result().outcome == Outcome::UNATTRIBUTED_MOTION && bench.onCalls == 1); // No ON for this session.
}
void testDeadlineWithoutTrigger() {
    fresh(); selectedFacts(1);
    arm("x3 relative 1000 60 window-ms 300");
    done();
    assert(result().outcome == Outcome::DEADLINE && !bench.onCalls && !count(isStart));
}
void testAbsoluteAndAbsoluteBy() {
    fresh(); selectedFacts(1, 1);
    auto id = arm("x3 absolute 1500 60");
    assert(session().members[0].expectedEndpoint == 1500);
    trigger(id); done();
    if (result().outcome != Outcome::ATTRIBUTED || result().positionChange != 1000 || !result().positionMatches)
        std::fprintf(stderr, "absolute: %s missing %u change %lld expected %lld matches %d error %s\n",
            Probe::inputStartOutcomeName(result().outcome), result().missing, static_cast<long long>(result().positionChange),
            static_cast<long long>(result().expectedChange), result().positionMatches, session().error);
    assert(result().outcome == Outcome::ATTRIBUTED && result().positionChange == 1000 && result().positionMatches);
    assert(result().further == ESS::InputStartFurther::NOT_EXCLUDED);
    assert(releaseOperation(id));
    // After a configuration change, the old reads no longer admit an absolute arm.
    command("@90 wiring x3 unknown\n"); command("@91 wiring x3 connected\n");
    refusedBeforeTx("@92 arm position x3 absolute 1700 60\n", "\"ok\":false");
    // The documented sequence: read config, driver read, motion-profile read, read state, record.
    selectedFacts(1, 1);
    id = arm("x3 absolute-by 200 60");
    assert(session().members[0].expectedEndpoint == 1700);
    trigger(id); done();
    assert(result().outcome == Outcome::ATTRIBUTED && position(drives[1]) == 1700);
}
void testGroupMembers() {
    fresh();
    for (const auto address : {1, 2}) { drives[address].regs[0x0044] = 7; drives[address].regs[0x0050] = 0; }
    uint8_t list[2] = {1, 2}; uint32_t set = 0;
    assert(declareGroup(list, 2, set));
    runUntil([&] { return !groupOperation(*app, set)->result.pending; });
    assert(groupOperation(*app, set)->result.ok);
    uint32_t read = 0; assert(submitGroupStateRead(read));
    runUntil([&] { return !groupOperation(*app, read)->result.pending; });
    for (const char* line : {"@1 driver read 1\n", "@2 driver read 2\n"}) { command(line); finish(latest()); }
    // The selected axis is drive 1 as well; its own DRIVE cache is not a member fact.
    command("@3 io qualification install member 1 x3 7 0 finite-move unknown triggers-inhibited\n"); contains("\"ok\":true");
    command("@4 io qualification install member 2 x3 7 0 finite-move unknown triggers-inhibited\n"); contains("\"ok\":true");
    const auto id = arm("x3 relative 400 60 group");
    assert(session().count == 2 && session().members[0].staged && session().members[1].staged);
    assert(app->group.members[0].storedKnown && app->group.members[1].storedKnown);
    trigger(id);
    assert(session().rowCount == 4 && session().rows[0].address == 4 && session().rows[1].address == 5);
    assert(!std::strcmp(session().rows[2].row, "M") && !std::strcmp(session().rows[3].row, "M"));
    done();
    for (uint8_t i = 0; i < 2; ++i) assert(result(i).outcome == Outcome::ATTRIBUTED && result(i).positionChange == 400);
    assert(!count(isStart) && bench.onCalls == 1);
}
// Wired bench 09: the aux1 edge ring is never drained, so once it holds
// EDGE_CAPACITY edges its own overwrite counter grows with every new edge. That
// wrap-around is not lost session evidence; only session edges the ring no
// longer retains (more than EDGE_CAPACITY since arming, or a clear) degrade.
void testAux1RingWrapIsNotLostEvidence() {
    const uint32_t ring = static_cast<uint32_t>(BenchIo::EDGE_CAPACITY);
    // Recorded shape: 16 two-edge trials after `bench clear`, then every edge overwrites.
    fresh(); selectedFacts(1);
    bench.inputEdges = 2 * 16; bench.inputOverwritten = 0;
    auto id = arm("x3 relative 1000 60");
    trigger(id);
    bench.inputEdges += 2; bench.inputOverwritten += 2; // Y0 running on and off of this start.
    done();
    assert(result().outcome == Outcome::ATTRIBUTED && !result().missing && session().aux1Overwrites == 0);
    assert(releaseOperation(id));
    // A long-wrapped ring before arming changes nothing either.
    selectedFacts(1);
    bench.inputEdges = 500; bench.inputOverwritten = 468;
    id = arm("x3 relative 1000 60");
    trigger(id);
    bench.inputEdges += 2; bench.inputOverwritten += 2;
    done();
    assert(result().outcome == Outcome::ATTRIBUTED && !result().missing && session().aux1Overwrites == 0);
    assert(releaseOperation(id));
    // More session edges than the ring holds: the oldest of them are lost.
    selectedFacts(1);
    id = arm("x3 relative 1000 60");
    trigger(id);
    bench.inputEdges += ring + 3; bench.inputOverwritten += ring + 3;
    done();
    assert(result().outcome == Outcome::INDETERMINATE && (result().missing & ESS::INPUT_START_MISSING_OVERWRITE));
    assert(session().aux1Overwrites == 3);
    assert(releaseOperation(id));
    // A ring clear during the session loses its edges.
    selectedFacts(1);
    id = arm("x3 relative 1000 60");
    trigger(id);
    bench.inputEdges = 0; bench.inputOverwritten = 0;
    done();
    assert(result().outcome == Outcome::INDETERMINATE && (result().missing & ESS::INPUT_START_MISSING_OVERWRITE));
    assert(releaseOperation(id));
}
// --------------------------------------------- PT records (wired bench 13)
// Simulator only: X0 = 12 starts the record the PIN0..PIN2 levels select
// (record = PIN2*4 + PIN1*2 + PIN0 + 1, function manual p22-23) on the trigger's
// rising edge. Says nothing about the real drive; the bench measures that.
// PIN outputs: bit 0 relay2 (X1), bit 1 relay1 (X2), bit 2 mosfet2 (X3).
const uint8_t PIN_OUTPUT[3] = {1, 0, 3};
const char* const PIN_NAME[3] = {"relay2", "relay1", "mosfet2"};
void ptLayout(Drive& d) {
    d.regs[0x0041] = 12; d.regs[0x0042] = 14; d.regs[0x0043] = 15; d.regs[0x0044] = 16; d.regs[0x0019] = 0;
}
void setRecord(Drive& d, uint8_t n, int32_t target, uint16_t speed = 60) {
    const uint16_t base = static_cast<uint16_t>(0x0060 + 6 * (n - 1));
    d.regs[base] = static_cast<uint16_t>(static_cast<uint32_t>(target) >> 16); d.regs[base + 1] = static_cast<uint16_t>(target);
    d.regs[base + 2] = speed; d.regs[base + 3] = 100; d.regs[base + 4] = 100;
}
void selectPins(uint8_t record) {
    for (uint8_t b = 0; b < 3; ++b) {
        const bool want = ((record - 1u) >> b) & 1u;
        if (want && !bench.outputs[PIN_OUTPUT[b]].active) { command(std::string("@15 bench on ") + PIN_NAME[b] + " 60000\n"); contains("\"ok\":true"); }
        if (!want && bench.outputs[PIN_OUTPUT[b]].active) { command(std::string("@15 bench off ") + PIN_NAME[b] + "\n"); contains("\"ok\":true"); }
    }
}
// The fresh reads and FINITE_MOVE records a PT arm needs, as the bench helper sends them.
void ptFacts(uint8_t address, uint8_t record, uint16_t mode = 0, const char* facts = "triggers-inhibited") {
    if (app->axis.target.address != address) { command("@1 useaddr " + std::to_string(address) + "\n"); contains("\"ok\":true"); }
    auto& drive = drives[address];
    if (drive.runningReads) { setPosition(drive, drive.pending); drive.runningReads = 0; }
    // A mode changed behind the application: the DRIVE read invalidates the
    // configuration it differs from, so the configuration read follows it.
    const bool changed = drive.regs[0x0050] != mode;
    drive.regs[0x0050] = mode;
    if (changed) { command("@3 driver read\n"); finish(latest()); }
    for (const std::string& line : {std::string("@2 read config\n"), std::string("@3 driver read\n"),
                                   "@4 segment position " + std::to_string(record) + " read\n", std::string("@5 read state\n")}) {
        command(line); finish(latest());
    }
    for (uint8_t t = 0; t < 4; ++t)
        command("@6 io qualification install " + std::to_string(address) + " x" + std::to_string(t) + " " +
            std::to_string(drive.regs[0x0041 + t]) + " 0 finite-move unknown " + facts + "\n");
}
uint32_t armPt(const std::string& args) {
    command("@20 arm segment " + args + "\n");
    if (!said("\"result\":\"accepted\"")) { std::fprintf(stderr, "PT arm refused: %s\n", Serial.output.c_str()); assert(false); }
    assert(session().phase == Phase::ARMED); // No staging: a PT arm writes nothing.
    return session().operationId;
}
void testPtRecordsSelectedByPins() {
    fresh();
    auto& d = drives[1];
    ptLayout(d);
    for (uint8_t n = 1; n <= 16; ++n) setRecord(d, n, n <= 8 ? 100 * n : -1500, n <= 8 ? 60 : 15);
    // All eight PIN maps: each record runs its own target; records 9-16 never.
    for (uint8_t n = 1; n <= 8; ++n) {
        selectPins(n); ptFacts(1, n);
        const auto writes = count(isWrite);
        const auto id = armPt("x0 relative " + std::to_string(n));
        const auto& mv = session().members[0];
        assert(mv.record == n && mv.recordTarget == 100 * n && mv.expectedDisplacement == 100 * n && mv.function == 12);
        assert(mv.selectorTerminals[0] == 1 && mv.selectorTerminals[1] == 2 && mv.selectorTerminals[2] == 3);
        trigger(id, "mosfet1", 50);
        assert(session().admitted && !std::strcmp(session().rows[3].row, "M"));
        done();
        const auto& r = result();
        if (r.outcome != Outcome::ATTRIBUTED || r.positionChange != 100 * n || !r.positionMatches)
            std::fprintf(stderr, "PT record %u: %s change %lld error %s\n", n, Probe::inputStartOutcomeName(r.outcome),
                static_cast<long long>(r.positionChange), session().error);
        assert(r.outcome == Outcome::ATTRIBUTED && r.positionChange == 100 * n && r.positionMatches);
        assert(count(isWrite) == writes && !count(isStart)); // Neither the arm nor the trigger wrote.
        assert(releaseOperation(id));
    }
    assert(d.starts == 8);
    // Absolute mode refuses before any traffic: on the bench (wired bench 13 P2)
    // records moved their target from the trigger position with 0x0050 = 1.
    setRecord(d, 4, 900); selectPins(4); ptFacts(1, 4, 1);
    refusedBeforeTx("@20 arm segment x0 absolute 4\n", "pt_absolute_unresolved");
    std::printf("input_start_app: PT arm session %u bytes\n", static_cast<unsigned>(sizeof(App::InputStartSession)));
}
void testPtArmRefusalsBeforeTx() {
    const auto setup = [](uint8_t record, uint16_t mode) {
        fresh(); auto& d = drives[1]; ptLayout(d);
        for (uint8_t n = 1; n <= 16; ++n) setRecord(d, n, 100 * n);
        selectPins(record); ptFacts(1, record, mode);
    };
    setup(3, 0);
    refusedBeforeTx("@8 arm segment x0 relative 9\n", "record_unreachable"); // PIN3 does not exist with a trigger.
    refusedBeforeTx("@8 arm segment x0 relative 2\n", "pin_levels");          // Levels select record 3.
    refusedBeforeTx("@8 arm segment x0 absolute 3\n", "pt_absolute_unresolved");
    command("@9 segment position 2 read\n"); finish(latest());
    refusedBeforeTx("@8 arm segment x0 relative 3\n", "record_stale");        // The fresh read is of record 2.
    for (const char* bad : {"@8 arm segment x0 relative 0\n", "@8 arm segment x0 relative 17\n", "@8 arm segment x0 sideways 3\n",
                            "@8 arm segment x4 relative 3\n", "@8 arm segment x0 relative\n", "@8 arm segment x0 relative 3 group group\n"}) {
        command(bad); contains("invalid_arguments");
    }
    // Unknown 0x0050: no DRIVE read in this binding.
    fresh(); ptLayout(drives[1]); setRecord(drives[1], 1, 100);
    for (const char* line : {"@2 read config\n", "@4 segment position 1 read\n", "@5 read state\n"}) { command(line); finish(latest()); }
    for (uint8_t t = 0; t < 4; ++t)
        command("@6 io qualification install 1 x" + std::to_string(t) + " " + std::to_string(drives[1].regs[0x0041 + t]) +
            " 0 finite-move unknown triggers-inhibited\n");
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "position_mode_unknown");
    // Selector facts are required on every PIN terminal, not only the trigger.
    fresh(); ptLayout(drives[1]); setRecord(drives[1], 1, 100); ptFacts(1, 1, 0, "");
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "triggers_inhibited_required");
    // Layout: PIN3 instead of PIN2, or X0 not 12.
    fresh(); ptLayout(drives[1]); drives[1].regs[0x0044] = 17; setRecord(drives[1], 1, 100); ptFacts(1, 1);
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "selector_layout");
    fresh(); ptLayout(drives[1]); drives[1].regs[0x0041] = 7; setRecord(drives[1], 1, 100); ptFacts(1, 1);
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "function_not_12");
    // Trigger input already active at the arm.
    setup(1, 0); drives[1].regs[0x0008] = 1; command("@9 read state\n"); finish(latest());
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "input_active");
    // Execution rate policy: 2500 rpm at subdivision 1000 is over the 2000 rpm ceiling; storage admitted it.
    fresh(); ptLayout(drives[1]); setRecord(drives[1], 1, 100, 2500); ptFacts(1, 1);
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "record_rate_ceiling");
    fresh(); ptLayout(drives[1]); setRecord(drives[1], 1, 100, 0); ptFacts(1, 1);
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "record_speed_zero");
    // Endpoint beyond the native range, and a negative target outside the promoted claim.
    fresh(); ptLayout(drives[1]); setRecord(drives[1], 1, 0x0FFFFFFF); ptFacts(1, 1);
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "endpoint_outside_limits");
    fresh(); ptLayout(drives[1]); setRecord(drives[1], 1, -200); ptFacts(1, 1);
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "record_sign_unqualified");
    // Word order 1 and an unknown soft-limit state refuse.
    fresh(); ptLayout(drives[1]); drives[1].regs[0x0019] = 1; setRecord(drives[1], 1, 100); ptFacts(1, 1);
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "word_order_unqualified");
    fresh(); ptLayout(drives[1]); drives[1].regs[0x0018] = 1; setRecord(drives[1], 1, 100); ptFacts(1, 1);
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "soft_limit_state_unknown");
    // A held trigger output refuses before any traffic.
    setup(1, 0);
    command("@10 bench on mosfet1 5000\n"); contains("\"ok\":true");
    refusedBeforeTx("@8 arm segment x0 relative 1\n", "output_active");
}
void testPtTriggerChecks() {
    fresh(); auto& d = drives[1]; ptLayout(d);
    for (uint8_t n = 1; n <= 8; ++n) setRecord(d, n, 100 * n);
    // A PIN level changed after the arm: the member row refuses, no ON, and the session stays armed.
    selectPins(3); ptFacts(1, 3);
    auto id = armPt("x0 relative 3");
    command("@30 bench off relay1\n"); contains("\"ok\":true");
    trigger(id, "mosfet1", 50);
    assert(!session().admitted && !std::strcmp(session().decisionRow, "L4") && !bench.outputs[2].active && !d.starts);
    assert(!std::strcmp(session().decisionReason, "PIN levels differ from the armed record"));
    command("@31 disarm " + std::to_string(id) + "\n"); assert(releaseOperation(id));
    // A non-member whose trigger terminal holds a start function: L5, no ON.
    selectPins(3); drives[2].regs[0x0041] = 7; ptFacts(1, 3);
    id = armPt("x0 relative 3");
    trigger(id, "mosfet1", 50);
    assert(!session().admitted && !std::strcmp(session().decisionRow, "L5") && session().decisionAddress == 2 && !d.starts);
    command("@32 disarm " + std::to_string(id) + "\n"); assert(releaseOperation(id));
    // A held selector on a terminal where a non-member holds a start function (13 here).
    drives[2].regs[0x0041] = 1; drives[2].regs[0x0043] = 13; ptFacts(1, 3);
    id = armPt("x0 relative 3");
    trigger(id, "mosfet1", 50);
    assert(!session().admitted && !std::strcmp(session().decisionReason, "an output is on at a terminal with a start function"));
    assert(!d.starts && bench.outputs[0].active); // relay1 (PIN1) still held; no trigger ON.
    command("@33 disarm " + std::to_string(id) + "\n"); assert(releaseOperation(id));
    drives[2].regs[0x0043] = 3;
    // A PIN hold that ends before the pulse plus its margin: refused before the
    // ON, outputs left as they were, no write (the hold expires after the decision).
    ptFacts(1, 3);
    id = armPt("x0 relative 3");
    bench.outputs[0].dueUs = hardware.time + 95000; // relay1 (PIN1) still on at the sample, hold ends in 95 ms.
    const auto writes = count(isWrite); const auto ons = bench.onCalls; const auto offs = bench.offCalls;
    trigger(id, "mosfet1", 50);
    if (session().admitted || std::strcmp(session().decisionReason, "a PIN output's hold ends before the trigger pulse and its margin"))
        std::fprintf(stderr, "hold check: admitted %d row %s reason %s\n", session().admitted, session().decisionRow, session().decisionReason);
    assert(!session().admitted && !std::strcmp(session().decisionReason, "a PIN output's hold ends before the trigger pulse and its margin"));
    assert(bench.onCalls == ons && bench.offCalls == offs && count(isWrite) == writes && !d.starts && !bench.outputs[2].active);
    command("@34 disarm " + std::to_string(id) + "\n"); pump(200); assert(releaseOperation(id));
    pump(20000); selectPins(3); // The shortened hold has ended; PIN1 on again for the next case.
    // Completion between polls: one stopped sample shows the change; no RUNNING time is invented.
    ptFacts(1, 3);
    id = armPt("x0 relative 3 poll-ms 500");
    replyDelay = nullptr;
    trigger(id, "mosfet1", 50);
    d.runningReads = 1; // The first sample after the edge already reads stopped.
    done();
    assert(result().outcome == Outcome::ATTRIBUTED && result().positionChange == 300 && !result().runningObserved);
    assert(result().changeStartUs <= session().onUs && result().changeEndUs > session().onUs);
    assert(releaseOperation(id));
    // A second run after one edge: SECOND_START, never a retrigger.
    ptFacts(1, 3);
    id = armPt("x0 relative 3");
    d.repeats = 1; d.ptRepeat = true;
    trigger(id, "mosfet1", 1500);
    done();
    assert(result().outcome == Outcome::SECOND_START && result().further == ESS::InputStartFurther::SEEN);
    assert(releaseOperation(id));
}
void testPtStopAndRecordWrites() {
    fresh(); auto& d = drives[1]; ptLayout(d);
    for (uint8_t n = 1; n <= 8; ++n) setRecord(d, n, 100 * n);
    // Stop during a PT run: the stop request drops the selectors and the trigger first.
    selectPins(6); ptFacts(1, 6);
    auto id = armPt("x0 relative 6");
    trigger(id, "mosfet1", 50);
    d.runForever = true;
    runUntil([] { return result().runningObserved; });
    command("@50 stop fast\n"); contains("\"result\":\"accepted\"");
    for (uint8_t b = 0; b < 3; ++b) assert(!bench.outputs[PIN_OUTPUT[b]].active);
    assert(bench.outputs[1].offReason == BenchIo::OffReason::STOP && bench.outputs[3].offReason == BenchIo::OffReason::STOP);
    done();
    assert(result().outcome == Outcome::STOPPED && d.starts == 1);
    assert(releaseOperation(id));
    // A record write to the armed drive is refused by the reservation, with no frame.
    selectPins(2); ptFacts(1, 2);
    id = armPt("x0 relative 2");
    const auto before = frames.size();
    command("@51 segment position 2 set speed 61\n"); contains("\"ok\":false"); pump(200);
    assert(count(isWrite, before) == 0 && session().phase == Phase::ARMED);
    command("@52 disarm " + std::to_string(id) + "\n"); pump(200); assert(releaseOperation(id));
    // A lost record-write ACK: the uncertain write invalidates the record read; the next arm refuses.
    ptFacts(1, 2);
    command("@53 io qualification install 1 x0 12 0 segment unknown triggers-inhibited\n");
    command("@54 io qualification install 1 x1 14 0 segment unknown triggers-inhibited\n");
    command("@55 io qualification install 1 x2 15 0 segment unknown triggers-inhibited\n");
    command("@56 io qualification install 1 x3 16 0 segment unknown triggers-inhibited\n");
    replyDelay = [](const std::vector<uint8_t>& f) -> uint32_t { return f[1] == 6 && word(f, 2) == 0x0068 ? 900000u : 0u; };
    command("@57 segment position 2 set speed 61\n");
    const auto writeId = latest();
    finish(writeId);
    replyDelay = nullptr;
    pump(20000);
    if (app->owner.needsRecovery() || uart.needsRecovery()) { command("@58 recover\n"); finish(latest()); pump(2000); }
    // Fresh state and configuration again; the record read the arm needs is gone.
    for (const char* line : {"@59 read config\n", "@60 driver read\n", "@61 read state\n"}) { command(line); finish(latest()); }
    for (uint8_t t = 0; t < 4; ++t)
        command("@6 io qualification install 1 x" + std::to_string(t) + " " + std::to_string(d.regs[0x0041 + t]) +
            " 0 finite-move unknown triggers-inhibited\n");
    refusedBeforeTx("@62 arm segment x0 relative 2\n", "record_stale");
    // The promoted target setter (wired bench 13 W0a): one FC10 of the pair and a
    // two-word readback at order 0; a negative target is outside the claim, zero TX.
    for (uint8_t t = 0; t < 4; ++t)
        command("@63 io qualification install 1 x" + std::to_string(t) + " " + std::to_string(d.regs[0x0041 + t]) +
            " 0 segment unknown triggers-inhibited\n");
    command("@64 segment position 2 read\n"); finish(latest());
    auto mark = frames.size();
    command("@65 segment position 2 set target -200\n"); contains("\"ok\":false"); pump(200);
    assert(count(isWrite, mark) == 0);
    command("@66 segment position 2 read\n"); finish(latest());
    for (uint8_t t = 0; t < 4; ++t)
        command("@67 io qualification install 1 x" + std::to_string(t) + " " + std::to_string(d.regs[0x0041 + t]) +
            " 0 segment unknown triggers-inhibited\n");
    mark = frames.size();
    command("@68 segment position 2 set target 1200\n"); contains("\"ok\":true");
    finish(latest());
    assert(count([](const std::vector<uint8_t>& f) { return f[1] == 16 && word(f, 2) == 0x0066 && word(f, 4) == 2; }, mark) == 1);
    assert(pair(d, 0x0066) == 1200 && count(isWrite, mark) == 1);
}
void testPtGroupMembers() {
    fresh();
    for (const auto address : {1, 2}) {
        auto& d = drives[address]; ptLayout(d); d.regs[0x0050] = 0;
        for (uint8_t n = 1; n <= 8; ++n) setRecord(d, n, (address == 1 ? 400 : 200) * n);
    }
    uint8_t list[2] = {1, 2}; uint32_t set = 0;
    assert(declareGroup(list, 2, set));
    runUntil([&] { return !groupOperation(*app, set)->result.pending; });
    assert(groupOperation(*app, set)->result.ok);
    selectPins(5);
    uint32_t read = 0; assert(submitGroupStateRead(read));
    runUntil([&] { return !groupOperation(*app, read)->result.pending; });
    for (const char* line : {"@1 driver read 1\n", "@2 driver read 2\n", "@3 segment position 5 read 1\n", "@4 segment position 5 read 2\n"}) {
        command(line); finish(latest());
    }
    for (const auto address : {1, 2})
        for (uint8_t t = 0; t < 4; ++t)
            command("@5 io qualification install member " + std::to_string(address) + " x" + std::to_string(t) + " " +
                std::to_string(drives[address].regs[0x0041 + t]) + " 0 finite-move unknown triggers-inhibited\n");
    command("@20 arm segment x0 relative 5 group\n");
    if (!said("\"result\":\"accepted\"")) { std::fprintf(stderr, "group PT arm refused: %s\n", Serial.output.c_str()); assert(false); }
    const auto id = session().operationId;
    assert(session().count == 2 && session().members[0].recordTarget == 2000 && session().members[1].recordTarget == 1000);
    trigger(id, "mosfet1", 50);
    assert(!std::strcmp(session().rows[2].row, "M") && !std::strcmp(session().rows[3].row, "M"));
    done();
    assert(result(0).outcome == Outcome::ATTRIBUTED && result(0).positionChange == 2000);
    assert(result(1).outcome == Outcome::ATTRIBUTED && result(1).positionChange == 1000);
    assert(bench.onCalls == 2 && drives[1].starts == 1 && drives[2].starts == 1 && !count(isStart));
}
// ------------------------------------------------ wired bench 14: velocity and PV
// The fresh reads and FINITE_MOVE records a velocity or PV arm needs.
void velocityFacts(uint8_t address, const std::string& stored, const char* mapping = "unknown") {
    if (app->axis.target.address != address) { command("@1 useaddr " + std::to_string(address) + "\n"); contains("\"ok\":true"); }
    auto& d = drives[address];
    for (const std::string& line : {std::string("@2 read config\n"), std::string("@3 driver read\n"), stored, std::string("@5 read state\n")}) {
        command(line); finish(latest());
    }
    for (uint8_t t = 0; t < 4; ++t)
        command("@6 io qualification install " + std::to_string(address) + " x" + std::to_string(t) + " " +
            std::to_string(d.regs[0x0041 + t]) + " " + std::to_string((d.regs[0x0040] >> t) & 1u) + " finite-move " +
            (t == 3 ? mapping : "unknown") + " triggers-inhibited\n");
}
void setJog(Drive& d, int16_t speed) { d.regs[0x001D] = static_cast<uint16_t>(speed); d.regs[0x001E] = 100; d.regs[0x001F] = 100; }
uint32_t armVelocity(const std::string& args) {
    command("@20 arm " + args + "\n");
    if (!said("\"result\":\"accepted\"")) { std::fprintf(stderr, "velocity arm refused: %s\n", Serial.output.c_str()); assert(false); }
    assert(session().phase == Phase::ARMED); // No staging: the stored words are already there.
    return session().operationId;
}
void testVelocityInputArms() {
    // JOG+ (9) on X0, JOG- (10) on X3, speed input (8) with a negative 0x001D: direction, level end, no write.
    struct Case { uint8_t terminal; uint16_t function; int16_t stored; const char* output; int8_t direction; };
    for (const Case c : {Case{0, 9, -30, "mosfet1", 1}, Case{3, 10, 30, "mosfet2", -1}, Case{0, 8, -30, "mosfet1", -1}, Case{0, 8, 30, "mosfet1", 1}}) {
        fresh(); auto& d = drives[1];
        d.regs[0x0041 + c.terminal] = c.function; setJog(d, c.stored);
        velocityFacts(1, "@4 velocity read\n");
        const auto writes = count(isWrite);
        const auto id = armVelocity("velocity x" + std::to_string(c.terminal));
        const auto& mv = session().members[0];
        assert(session().kind == Probe::InputStartKind::VELOCITY && mv.function == c.function && mv.storedRpm == c.stored);
        assert(mv.expectedDirection == c.direction && mv.stagedWords[0] == static_cast<uint16_t>(c.stored) && mv.stagedWords[1] == 100);
        trigger(id, c.output, 300);
        assert(session().admitted && !std::strcmp(session().rows[3].row, "M"));
        done();
        const auto& r = result();
        if (r.outcome != Outcome::ATTRIBUTED) std::fprintf(stderr, "velocity arm %u: %s %s\n", c.function, Probe::inputStartOutcomeName(r.outcome), session().error);
        assert(r.outcome == Outcome::ATTRIBUTED && r.effect == ESS::InputStartEffect::VELOCITY_RUN);
        assert(r.completion == ESS::InputStartCompletion::VELOCITY_STOPPED && (r.positionChange > 0) == (c.direction > 0) && r.positionChange);
        assert(count(isWrite) == writes && !count(isStart) && d.starts == 1); // The drive input ran it; the host wrote nothing.
        assert(releaseOperation(id));
    }
    // Refusals before any frame.
    const auto setup = [](uint16_t function, int16_t stored) {
        fresh(); auto& d = drives[1]; d.regs[0x0041] = function; setJog(d, stored); velocityFacts(1, "@4 velocity read\n");
    };
    setup(9, 61); refusedBeforeTx("@8 arm velocity x0\n", "outside_velocity_limits");
    setup(9, 0); refusedBeforeTx("@8 arm velocity x0\n", "speed_zero");
    setup(7, 30); refusedBeforeTx("@8 arm velocity x0\n", "function_not_8_9_10");
    setup(9, 30); refusedBeforeTx("@8 arm velocity x0 group\n", "invalid_arguments");
    fresh(); drives[1].regs[0x0041] = 9; setJog(drives[1], 30);
    for (const char* line : {"@2 read config\n", "@3 driver read\n", "@4 velocity read\n", "@5 read state\n"}) { command(line); finish(latest()); }
    refusedBeforeTx("@8 arm velocity x0\n", "triggers_inhibited_required"); // No FINITE_MOVE record on the trigger.
    fresh(); drives[1].regs[0x0041] = 9; setJog(drives[1], 30); velocityFacts(1, "@4 read state\n");
    refusedBeforeTx("@8 arm velocity x0\n", "velocity_read_stale");
    fresh(); drives[1].regs[0x0041] = 9; drives[1].regs[0x0018] = 1; setJog(drives[1], 30); velocityFacts(1, "@4 velocity read\n");
    refusedBeforeTx("@8 arm velocity x0\n", "soft_limit_state_unknown");
    setup(9, 30); drives[1].regs[0x0008] = 1; command("@9 read state\n"); finish(latest());
    refusedBeforeTx("@8 arm velocity x0\n", "input_active");
    // Audit F2: raised limits are serial-only; an arm keeps the legacy speed while raised.
    setup(9, 120); refusedBeforeTx("@8 arm velocity x0\n", "outside_velocity_limits");
    static Probe::VelocityCell promoted[1];
    promoted[0].rpm = 120; promoted[0].accelerationTime = promoted[0].decelerationTime = 100;
    promoted[0].subdivision = 1000; promoted[0].durationMs = 2000;
    app->velocityPromoted = promoted; app->velocityPromotedCount = 1;
    command("@9 velocity limits raise\n"); contains("\"raised\":true");
    refusedBeforeTx("@8 arm velocity x0\n", "outside_velocity_limits");
    // A non-member with a start function on the trigger terminal: L5, no ON.
    setup(9, 30); drives[2].regs[0x0041] = 10;
    auto id = armVelocity("velocity x0");
    trigger(id, "mosfet1", 300);
    assert(!session().admitted && !std::strcmp(session().decisionRow, "L5") && !drives[1].starts);
    command("@11 disarm " + std::to_string(id) + "\n"); pump(200); assert(releaseOperation(id));
}
void testVelocityDeadman() {
    // X3 = 5 with polarity 1 (AFTER_INVERSION, wired bench 05): the stop reads active
    // while mosfet2 is off, so the arm refuses; mosfet2 held makes it inactive.
    fresh(); auto& d = drives[1];
    d.regs[0x0041] = 9; d.regs[0x0044] = 5; d.regs[0x0040] = 8; d.afterInversion = true; setJog(d, 30);
    command("@12 bench on mosfet2 60000\n"); contains("\"ok\":true");
    velocityFacts(1, "@4 velocity read\n", "after");
    command("@13 bench off mosfet2\n"); contains("\"ok\":true");
    command("@14 read state\n"); finish(latest());
    refusedBeforeTx("@8 arm velocity x0\n", "stop_input_active");
    command("@12 bench on mosfet2 60000\n"); contains("\"ok\":true");
    velocityFacts(1, "@4 velocity read\n", "after");
    const auto id = armVelocity("velocity x0");
    trigger(id, "mosfet1", 3000);
    assert(session().admitted); // The held dead-man output is on a terminal without a start function.
    runUntil([] { return result().runningObserved; });
    command("@13 bench off mosfet2\n"); contains("\"ok\":true"); // The dead-man lapses: the drive stops itself.
    runUntil([] { return !drives[1].velocityRun; });
    assert(bench.outputs[2].active); // The JOG level is still on; the stop input ended the run.
    done();
    assert(result().completion == ESS::InputStartCompletion::VELOCITY_STOPPED && result().positionChange > 0 && d.starts == 1);
    assert(releaseOperation(id));
}
void setPv(Drive& d, uint8_t n, int16_t speed) {
    const uint16_t base = static_cast<uint16_t>(0x00C0 + 3 * (n - 1));
    d.regs[base] = static_cast<uint16_t>(speed); d.regs[base + 1] = 100; d.regs[base + 2] = 100;
}
void testPvArms() {
    // pin3 layout (X0 = 13, X1-X3 PIN0-PIN2): every record its own speed, level mode ends with the trigger.
    fresh(); auto& d = drives[1];
    d.regs[0x0041] = 13; d.regs[0x0042] = 14; d.regs[0x0043] = 15; d.regs[0x0044] = 16; d.regs[0x0051] = 0;
    for (uint8_t n = 1; n <= 8; ++n) setPv(d, n, static_cast<int16_t>(5 * n));
    for (uint8_t n : {1, 6}) {
        selectPins(n); velocityFacts(1, "@4 segment speed " + std::to_string(n) + " read\n");
        const auto id = armVelocity("pv x0 " + std::to_string(n) + " pin3");
        const auto& mv = session().members[0];
        assert(session().kind == Probe::InputStartKind::PV && mv.record == n && mv.storedRpm == 5 * n && mv.selectorCount == 3);
        assert(mv.layout == Probe::InputStartLayout::PIN3 && mv.expectedDirection == 1);
        trigger(id, "mosfet1", 300);
        assert(session().admitted);
        done();
        assert(result().outcome == Outcome::ATTRIBUTED && result().positionChange > 0 && d.lastRecord == n);
        assert(releaseOperation(id));
    }
    // Refusals: a record the layout cannot reach, PIN levels, the record read, the sign, the layout and 0x0051.
    selectPins(2); velocityFacts(1, "@4 segment speed 2 read\n");
    refusedBeforeTx("@8 arm pv x0 9 pin3\n", "record_unreachable");
    refusedBeforeTx("@8 arm pv x0 3 pin3\n", "pin_levels");
    refusedBeforeTx("@8 arm pv x0 2 pv2-deadman\n", "selector_layout");
    command("@9 segment speed 3 read\n"); finish(latest());
    refusedBeforeTx("@8 arm pv x0 2 pin3\n", "record_stale");
    setPv(d, 2, -30); velocityFacts(1, "@4 segment speed 2 read\n");
    refusedBeforeTx("@8 arm pv x0 2 pin3\n", "record_sign_unqualified");
    setPv(d, 2, 61); velocityFacts(1, "@4 segment speed 2 read\n");
    refusedBeforeTx("@8 arm pv x0 2 pin3\n", "outside_velocity_limits");
    for (const char* bad : {"@8 arm pv x0 2\n", "@8 arm pv x0 0 pin3\n", "@8 arm pv x0 2 pin4\n", "@8 arm pv x0 2 pin3 group\n"}) {
        command(bad); contains("invalid_arguments");
    }
    // No DRIVE read in this binding: 0x0051 is unknown.
    fresh(); auto& u = drives[1];
    u.regs[0x0041] = 13; u.regs[0x0042] = 14; u.regs[0x0043] = 15; u.regs[0x0044] = 16; setPv(u, 1, 20);
    for (const char* line : {"@2 read config\n", "@4 segment speed 1 read\n", "@5 read state\n"}) { command(line); finish(latest()); }
    for (uint8_t t = 0; t < 4; ++t)
        command("@6 io qualification install 1 x" + std::to_string(t) + " " + std::to_string(u.regs[0x0041 + t]) +
            " 0 finite-move unknown triggers-inhibited\n");
    refusedBeforeTx("@8 arm pv x0 1 pin3\n", "pv_trigger_mode_unknown");
    // pv2-deadman (X3 = 5 inverted) in edge mode (0x0051 = 1): the run continues after the
    // trigger until a stop; the dead-man lapse stops it here.
    fresh(); auto& e = drives[1];
    e.regs[0x0041] = 13; e.regs[0x0042] = 14; e.regs[0x0043] = 15; e.regs[0x0044] = 5; e.regs[0x0040] = 8; e.regs[0x0051] = 1;
    e.afterInversion = true;
    for (uint8_t n = 1; n <= 4; ++n) setPv(e, n, static_cast<int16_t>(10 * n));
    selectPins(2); // relay2 (PIN0) on; PIN1 off; mosfet2 is the dead-man here, not PIN2.
    command("@12 bench on mosfet2 60000\n"); contains("\"ok\":true");
    velocityFacts(1, "@4 segment speed 2 read\n", "after");
    refusedBeforeTx("@8 arm pv x0 2 pin3\n", "selector_layout"); // The pin3 helper refuses X3 = 5.
    const auto id = armVelocity("pv x0 2 pv2-deadman window-ms 20000");
    assert(session().members[0].selectorCount == 2 && session().members[0].layout == Probe::InputStartLayout::PV2_DEADMAN);
    trigger(id, "mosfet1", 50);
    assert(session().admitted);
    runUntil([] { return result().runningObserved; });
    pump(20000);
    assert(e.velocityRun && !bench.outputs[2].active); // Edge mode: still running after the trigger ended.
    command("@13 bench off mosfet2\n"); contains("\"ok\":true");
    runUntil([] { return !drives[1].velocityRun; });
    done();
    assert(result().completion == ESS::InputStartCompletion::VELOCITY_STOPPED && e.lastRecord == 2);
    assert(releaseOperation(id));
    // pv-pt-mix (X3 = 12): the pv2-deadman helper refuses it. The mix needs 0x0051 = 1 (p29),
    // and an edge-mode arm needs the dead-man layout (audit F3): refused in both modes.
    fresh(); auto& m = drives[1];
    m.regs[0x0041] = 13; m.regs[0x0042] = 14; m.regs[0x0043] = 15; m.regs[0x0044] = 12; m.regs[0x0051] = 1;
    for (uint8_t n = 1; n <= 4; ++n) setPv(m, n, 20);
    selectPins(1); velocityFacts(1, "@4 segment speed 1 read\n");
    refusedBeforeTx("@8 arm pv x0 1 pv2-deadman\n", "selector_layout");
    refusedBeforeTx("@8 arm pv x0 1 pv-pt-mix\n", "pv_edge_needs_deadman");
    m.regs[0x0051] = 0; command("@3 driver read\n"); finish(latest()); // The changed mode invalidates the configuration.
    velocityFacts(1, "@4 segment speed 1 read\n");
    refusedBeforeTx("@8 arm pv x0 1 pv-pt-mix\n", "pv_pt_mix_needs_edge_mode");
    // Audit F3: edge mode in the three-selector layout (no dead-man) refuses before any frame or pulse.
    fresh(); auto& g = drives[1];
    g.regs[0x0041] = 13; g.regs[0x0042] = 14; g.regs[0x0043] = 15; g.regs[0x0044] = 16; g.regs[0x0051] = 1; setPv(g, 1, 20);
    velocityFacts(1, "@4 segment speed 1 read\n");
    refusedBeforeTx("@8 arm pv x0 1 pin3\n", "pv_edge_needs_deadman");
    g.regs[0x0051] = 0; command("@3 driver read\n"); finish(latest());
    velocityFacts(1, "@4 segment speed 1 read\n");
    const auto level = armVelocity("pv x0 1 pin3"); // Level mode ends with the trigger width.
    command("@14 disarm " + std::to_string(level) + "\n"); pump(200); assert(releaseOperation(level));
}
} // namespace

int main() {
    testSelectedRelativeStartIsAttributed();
    testArmRefusalsBeforeTx();
    testTriggerRefusals();
    testLineIsolationTruthTable();
    testReadAgeBackstop();
    testStopOfInputStartedMotion();
    testDisarmAndStuckPort();
    testInvalidation();
    testStagingEdgeFailsArm();
    testSecondStartAndUnattributedMotion();
    testDeadlineWithoutTrigger();
    testAbsoluteAndAbsoluteBy();
    testGroupMembers();
    testAux1RingWrapIsNotLostEvidence();
    testPtRecordsSelectedByPins();
    testPtArmRefusalsBeforeTx();
    testPtTriggerChecks();
    testPtStopAndRecordWrites();
    testPtGroupMembers();
    testVelocityInputArms();
    testVelocityDeadman();
    testPvArms();
    std::puts("input_start_app: PASS");
    return 0;
}
