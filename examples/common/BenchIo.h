// SPDX-License-Identifier: MIT
#pragma once
/**
 * @file BenchIo.h
 * @brief Example-only bench outputs and input for wired drive-terminal tests.
 *
 * The bench board has two relays, two low-side MOSFETs and one isolated 24 V
 * input. The standalone example uses them to drive ESS X inputs and to sense
 * one ESS Y output. They are test fixture controls, not motor library API.
 *
 * Every ON has a hold time of 1..MAX_HOLD_MS and turns OFF by itself. A motor
 * stop turns every output OFF. Wiring, voltages and drive assignments stay the
 * user's and the application's explicit declarations; see docs/bench_io.md.
 * This header configures no pin and has no platform dependency.
 */
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace MotorControlRSExample { namespace BenchIo {

constexpr uint8_t OUTPUTS = 4;          ///< relay1, relay2, mosfet1, mosfet2.
constexpr uint32_t MAX_HOLD_MS = 60000; ///< Longest ON before automatic OFF.
/// The one-shot timer may run up to this much before the recorded due time.
constexpr uint64_t EARLY_TIMER_US = 500;
/// Service-loop backstop if the timer has not turned an output OFF by then.
constexpr uint64_t LATE_OFF_US = 5000;
constexpr std::size_t EDGE_CAPACITY = 32; ///< Input edges retained by the interrupt.
constexpr std::size_t RECENT_EDGES = 16;  ///< Newest edges copied into a view.

/** Why an output is OFF. NONE while it is ON. */
enum class OffReason : uint8_t { NONE, BOOT, LEASE, LATE, REQUEST, STOP, FAULT };
enum class Op : uint8_t { SHOW, ON, OFF, OFF_ALL, CLEAR_EDGES };
enum class Result : uint8_t { OK, UNAVAILABLE, INVALID, FAILED };

/** One output; gpio -1 means absent. */
struct OutputPin { int gpio = -1; bool activeHigh = true; };
/** The sensed input; gpio -1 means absent. */
struct InputPin { int gpio = -1; bool activeLow = true; };
/** Outputs are indexed in OUTPUTS order: relay1, relay2, mosfet1, mosfet2. */
struct Pins { OutputPin outputs[OUTPUTS]; InputPin input; };

struct Request {
    Op op = Op::SHOW;
    uint8_t output = 0;  ///< ON and OFF only.
    uint32_t holdMs = 0; ///< ON only: 1..MAX_HOLD_MS.
};

/** Commanded state of one output. Times use the application microsecond clock. */
struct OutputView {
    int gpio = -1;
    bool active = false;
    uint32_t holdMs = 0; ///< Hold of the latest ON.
    uint32_t ons = 0;    ///< OFF-to-ON transitions since boot.
    uint64_t onUs = 0, offUs = 0, dueUs = 0;
    OffReason offReason = OffReason::NONE;
};
struct Edge { uint64_t atUs = 0; bool active = false; };
/** Sampled input level and edges seen by the interrupt (level read in the ISR). */
struct InputView {
    int gpio = -1;
    bool known = false, active = false;
    uint32_t edges = 0;       ///< Edges seen since boot or clear.
    uint32_t overwritten = 0; ///< Older edges no longer retained.
    std::size_t recent = 0;   ///< Valid entries in recentEdges, oldest first.
    Edge recentEdges[RECENT_EDGES];
};
struct View {
    bool ready = false;
    uint64_t nowUs = 0;
    OutputView outputs[OUTPUTS];
    InputView input;
};

inline const char* outputName(uint8_t index) {
    static const char* const names[OUTPUTS] = {"relay1", "relay2", "mosfet1", "mosfet2"};
    return index < OUTPUTS ? names[index] : "unknown";
}
inline bool outputIndex(const char* name, uint8_t& index) {
    for (uint8_t i = 0; name && i < OUTPUTS; ++i)
        if (!std::strcmp(name, outputName(i))) { index = i; return true; }
    return false;
}
inline const char* inputName() { return "aux1"; }
inline const char* offReasonName(OffReason reason) {
    switch (reason) {
    case OffReason::NONE: return "none";
    case OffReason::BOOT: return "boot";
    case OffReason::LEASE: return "hold_elapsed";
    case OffReason::LATE: return "hold_elapsed_late";
    case OffReason::REQUEST: return "request";
    case OffReason::STOP: return "motor_stop";
    case OffReason::FAULT: return "timer_fault";
    }
    return "unknown";
}

/** ESP32-S3 GPIOs usable here: not USB (19, 20), module flash/PSRAM (26..37)
 * or the absent 22..25. Strapping pins 0, 3, 45 and 46 are allowed; the board
 * must hold them at their boot levels until firmware drives them. */
inline bool usableGpio(int gpio) {
    return (gpio >= 0 && gpio <= 18) || gpio == 21 || (gpio >= 38 && gpio <= 48);
}
/** Every present pin is usable, distinct and not in reserved; at least one pin. */
inline bool validPins(const Pins& pins, const int* reserved, std::size_t reservedCount) {
    int used[OUTPUTS + 1];
    std::size_t count = 0;
    for (uint8_t i = 0; i < OUTPUTS; ++i) used[count++] = pins.outputs[i].gpio;
    used[count++] = pins.input.gpio;
    bool any = false;
    for (std::size_t i = 0; i < count; ++i) {
        if (used[i] < 0) { if (used[i] != -1) return false; continue; }
        any = true;
        if (!usableGpio(used[i])) return false;
        for (std::size_t j = 0; j < i; ++j) if (used[j] == used[i]) return false;
        for (std::size_t j = 0; reserved && j < reservedCount; ++j) if (reserved[j] == used[i]) return false;
    }
    return any;
}
inline bool validHold(uint32_t holdMs) { return holdMs >= 1 && holdMs <= MAX_HOLD_MS; }
/** Timer callback decision. A callback from an earlier ON fires before the
 * current due time and is ignored; renewal keeps the output ON. */
inline bool timerExpires(bool active, uint64_t dueUs, uint64_t nowUs) {
    return active && nowUs + EARLY_TIMER_US >= dueUs;
}
/** Service-loop backstop when the timer has not turned the output OFF. */
inline bool lateExpiry(bool active, uint64_t dueUs, uint64_t nowUs) {
    return active && nowUs >= dueUs + LATE_OFF_US;
}

/** Fixed edge ring. The owner serializes push, clear and copy. */
struct EdgeRing {
    Edge items[EDGE_CAPACITY];
    std::size_t next = 0, count = 0;
    uint32_t total = 0, overwritten = 0;
    void push(uint64_t atUs, bool active) {
        if (count == EDGE_CAPACITY) { if (overwritten != UINT32_MAX) ++overwritten; }
        else ++count;
        items[next].atUs = atUs; items[next].active = active;
        next = (next + 1) % EDGE_CAPACITY;
        if (total != UINT32_MAX) ++total;
    }
    void clear() { next = count = 0; total = overwritten = 0; }
    /** Newest RECENT_EDGES edges, oldest first, plus the counters. */
    void copy(InputView& output) const {
        const std::size_t recent = count < RECENT_EDGES ? count : RECENT_EDGES;
        for (std::size_t i = 0; i < recent; ++i)
            output.recentEdges[i] = items[(next + EDGE_CAPACITY - recent + i) % EDGE_CAPACITY];
        output.recent = recent;
        output.edges = total;
        output.overwritten = overwritten;
    }
};

/** Platform port used by the standalone application. Calls come from the
 * application task; the implementation owns its timer and interrupt context. */
class Port {
public:
    /** Apply one request and copy the resulting state, also on refusal. */
    virtual Result apply(const Request&, View&) = 0;
    /** Turn every output OFF now. Never refuses. */
    virtual void quiesce(OffReason) = 0;
    /** Backstop for an ON whose hold timer has not fired. */
    virtual void service(uint64_t nowUs) = 0;
protected:
    ~Port() = default;
};

}} // namespace MotorControlRSExample::BenchIo
