// SPDX-License-Identifier: MIT
// Exercise the private session boundary with the real console, without App or
// implementation inclusion. Faults enter through its ordinary output sink.
#include "../examples/probe_cli/DebugSession.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

namespace Probe = MotorControlRSExample::Probe;
using MotorControlRS::TrafficKind;
using MotorControlRS::TrafficRecord;

static_assert(!std::is_copy_constructible<Probe::DebugSession>::value, "capture binds final storage");
static_assert(!std::is_move_constructible<Probe::DebugSession>::value, "runner binds capture address");
static_assert(!std::is_copy_assignable<Probe::DebugSession>::value, "capture must not be rebound");
static_assert(!std::is_move_assignable<Probe::DebugSession>::value, "capture must not be rebound");

namespace {
const uint8_t PROBE[] = {1, 3, 0, 0, 0, 1, 0x84, 0x0A};
const uint8_t REPLY[] = {1, 3, 2, 0, 0x3C, 0xB8, 0x55};
struct Sink {
    bool blocked = false;
    unsigned attempts = 0;
    std::vector<std::string> lines;
    static bool emit(void* context, const char* bytes, std::size_t length) {
        auto& sink = *static_cast<Sink*>(context);
        ++sink.attempts;
        if (sink.blocked) return false;
        sink.lines.emplace_back(bytes, length);
        return true;
    }
    Probe::Host host() {
        Probe::Host value; value.context = this; value.emitLine = emit; return value;
    }
};
void mode(Probe::DebugSession& debug, Probe::DebugMode value) {
    Probe::DebugSnapshot result;
    assert(debug.command(&value, result) == Probe::Action::OK);
    assert(result.mode == value);
}
void boundedService(Probe::DebugMode selected) {
    Probe::DebugSession debug; Sink sink; Probe::Console console(sink.host(), Probe::Format::JSON);
    mode(debug, selected);
    debug.capture.begin(10);
    debug.capture.transmitted(PROBE, sizeof(PROBE), true, 20);
    for (unsigned i = 0; i < sizeof(REPLY); ++i) debug.capture.received(REPLY[i], 30+i, 31+i, 0, 31+i);
    debug.capture.flushReceived(true, 40);
    debug.capture.event(TrafficKind::END, 50);
    assert(debug.capture.size() == 3);
    uint64_t independent = 0; TrafficRecord copied;
    assert(debug.capture.copyAfter(independent, copied) && copied.kind == TrafficKind::TX);
    assert(copied.sequence == debug.records[0].sequence); // Capture uses its own final-storage array.
    for (unsigned i = 1; i <= 3; ++i) {
        debug.service(console, false);
        assert(debug.state.observed == i && debug.state.emitted == i);
        assert(debug.cursor == i && sink.lines.size() == i);
        assert(debug.capture.size() == 3 && independent == 1);
    }
    assert(sink.lines[1].find(selected == Probe::DebugMode::DECODED ?
        "\"expected_request_hex\":\"010300000001840A\"" :
        "\"expected_request_hex\":\"\"") != std::string::npos);
    debug.service(console, false);
    assert(debug.state.observed == 3 && sink.lines.size() == 3);
    assert(debug.capture.copyAfter(independent, copied) && copied.kind == TrafficKind::RX);
    // An unrelated transaction must not inherit the remembered request.
    debug.capture.begin(60); debug.capture.received(1, 60, 61, 0, 61); debug.capture.flushReceived(true, 62);
    debug.service(console, false);
    assert(sink.lines.back().find("\"expected_request_hex\":\"\"") != std::string::npos);
}
void pressureAndPendingOutput() {
    Probe::DebugSession debug; Sink sink; Probe::Console console(sink.host(), Probe::Format::JSON);
    mode(debug, Probe::DebugMode::RAW);
    for (unsigned i = 0; i < 4; ++i) debug.capture.event(TrafficKind::END, i);
    debug.service(console, true); // App reserves its last output slots for results.
    assert(debug.state.dropped == 1 && sink.attempts == 0 && debug.cursor == 1);
    sink.blocked = true;
    debug.service(console, false); // Sink refusal drops only this diagnostic.
    assert(debug.state.dropped == 2 && sink.attempts == 1 && !console.outputPending());
    for (const char* p = "@1 version\n"; *p; ++p) console.feed(*p);
    assert(console.outputPending());
    const auto attempts = sink.attempts;
    debug.service(console, false);
    assert(debug.state.dropped == 3 && sink.attempts == attempts && console.outputPending());
    sink.blocked = false; assert(console.serviceOutput());
    assert(sink.lines.size() == 1 && sink.lines[0].find("\"command\":\"version\"") != std::string::npos);
    debug.service(console, false);
    assert(debug.state.observed == 4 && debug.state.emitted == 1 && debug.state.dropped == 3);
    assert(sink.lines.size() == 2);
}
void lossSkipAndDisable() {
    Probe::DebugSession debug; Sink sink; Probe::Console console(sink.host());
    mode(debug, Probe::DebugMode::RAW);
    for (unsigned i = 0; i < Probe::DebugSession::CAPACITY + 4; ++i) debug.capture.event(TrafficKind::END, i);
    debug.capture.transmitted(nullptr, 1, true, 30); // Invalid capture input is a separate loss.
    debug.service(console, true);
    assert(debug.state.missed == 4 && debug.state.observed == 1 && debug.state.dropped == 1);
    debug.capture.received(0xAB, 40, 41, 1, 41);
    mode(debug, Probe::DebugMode::OFF); // Flush partial RX before intentional display skips.
    Probe::DebugSnapshot out; assert(debug.command(nullptr, out) == Probe::Action::OK);
    assert(out.skipped == 16 && out.missed == 4 && out.overwritten == 5 && out.captureDropped == 1);
    assert(out.observed + out.skipped + out.missed == out.cursor);
    assert(out.retained == Probe::DebugSession::CAPACITY && !debug.requestKnown);
    uint64_t a = 0, b = 0; TrafficRecord left, right;
    bool partial = false;
    while (debug.capture.copyAfter(a, left)) {
        assert(debug.capture.copyAfter(b, right) && left.sequence == right.sequence);
        if (left.kind == TrafficKind::RX) {
            assert(left.length == 1 && left.bytes[0] == 0xAB && !left.complete);
            partial = true;
        }
    }
    assert(partial);
    const auto cursor = debug.cursor;
    debug.service(console, false); debug.capture.event(TrafficKind::END, 50);
    assert(debug.cursor == cursor && sink.attempts == 0);
    mode(debug, Probe::DebugMode::DECODED);
    debug.capture.event(TrafficKind::END, 60); debug.service(console, false);
    assert(debug.cursor == cursor + 1 && debug.state.emitted == 1);
    // Invalid commands cannot reset diagnostic state or change capture mode.
    const auto invalid = static_cast<Probe::DebugMode>(255);
    out.cursor = 999;
    assert(debug.command(&invalid, out) == Probe::Action::INVALID && out.cursor == 999);
    assert(debug.capture.enabled() && debug.state.mode == Probe::DebugMode::DECODED);
}
void saturatingCounters() {
    Probe::DebugSession debug; Sink sink; Probe::Console console(sink.host());
    mode(debug, Probe::DebugMode::RAW);
    debug.state.observed = debug.state.emitted = debug.state.dropped = UINT64_MAX;
    debug.state.missed = debug.state.skipped = UINT64_MAX;
    for (unsigned i = 0; i < Probe::DebugSession::CAPACITY + 1; ++i) debug.capture.event(TrafficKind::END, i);
    debug.service(console, false); debug.service(console, true);
    mode(debug, Probe::DebugMode::OFF);
    assert(debug.state.observed == UINT64_MAX && debug.state.emitted == UINT64_MAX);
    assert(debug.state.dropped == UINT64_MAX && debug.state.missed == UINT64_MAX && debug.state.skipped == UINT64_MAX);
}
} // namespace
int main() {
    boundedService(Probe::DebugMode::RAW); boundedService(Probe::DebugMode::DECODED);
    pressureAndPendingOutput(); lossSkipAndDisable(); saturatingCounters();
    std::printf("Storage bytes: DebugSession=%zu TrafficRecord=%zu TrafficCapture=%zu\n",
        sizeof(Probe::DebugSession), sizeof(TrafficRecord), sizeof(MotorControlRS::TrafficCapture));
}
