// SPDX-License-Identifier: MIT
#pragma once
/**
 * @file FixtureWiring.h
 * @brief Example-only board declaration of which drive terminals are wired.
 *
 * A board configuration may state, per ESS address, which X inputs and Y
 * outputs carry fixture wiring. The standalone application applies it at boot,
 * after `useaddr` and to new group members. A declaration is a caller fact: it
 * changes no drive assignment, infers nothing from observed levels and is not
 * motor library API. UNCONNECTED never disables a function; CONNECTED observes
 * no level. This header configures no pin and has no platform dependency.
 */
#include <MotorControlRS/ReadOperation.h>
#include <cstddef>
#include <cstdint>

namespace MotorControlRSExample {

/** One address of the board table. Bit n of connectedInputs is Xn (n = 0..3),
 * bit n of connectedOutputs is Yn (n = 0..1). A listed terminal outside the
 * mask is declared UNCONNECTED. Plain aggregate for brace initialization. */
struct FixtureWiringEntry {
    uint8_t address;          ///< ESS address 1..247.
    uint8_t connectedInputs;  ///< X0..X3 mask, at most 0xF.
    uint8_t connectedOutputs; ///< Y0..Y1 mask, at most 0x3.
};
/// Board table entries the standalone application accepts.
constexpr std::size_t FIXTURE_WIRING_CAPACITY = 4;

/** How the application received its board table. ABSENT keeps the legacy
 * defaults; INVALID makes every declaration UNKNOWN. */
enum class FixtureState : uint8_t { ABSENT, APPLIED, INVALID };
/** Origin of one effective terminal declaration. APPLICATION_DEFAULT is the
 * legacy value (UNCONNECTED at boot, UNKNOWN after `useaddr` or for a new group
 * member); its console name is `default`. Named so that Arduino's DEFAULT macro
 * cannot collide with it. */
enum class WiringSource : uint8_t { APPLICATION_DEFAULT, BOARD, HOST, INVALID_TABLE };

/** Board declaration of one address. listed is false when the table is absent,
 * invalid or has no entry for the address; the values are then not meaningful. */
struct BoardWiring {
    bool listed = false;
    MotorControlRS::InputWiring inputs[4] = {}, outputs[2] = {};
};

/** First validation error of a board table, or nullptr when it is valid. An
 * empty table (count 0) is valid. Checks count <= FIXTURE_WIRING_CAPACITY,
 * addresses 1..247 and unique, input mask <= 0xF and output mask <= 0x3. */
inline const char* fixtureWiringError(const FixtureWiringEntry* entries, std::size_t count) {
    if (count > FIXTURE_WIRING_CAPACITY) return "count_over_capacity";
    if (count && !entries) return "entries_missing";
    for (std::size_t i = 0; i < count; ++i) {
        const auto& entry = entries[i];
        if (entry.address < 1 || entry.address > 247) return "address_out_of_range";
        if (entry.connectedInputs > 0xF) return "input_mask_out_of_range";
        if (entry.connectedOutputs > 0x3) return "output_mask_out_of_range";
        for (std::size_t j = 0; j < i; ++j)
            if (entries[j].address == entry.address) return "duplicate_address";
    }
    return nullptr;
}
/// True when fixtureWiringError finds nothing.
inline bool validFixtureWiring(const FixtureWiringEntry* entries, std::size_t count) {
    return fixtureWiringError(entries, count) == nullptr;
}
/** Fill the declaration of one address from a validated table: a terminal in
 * the mask is CONNECTED, any other terminal UNCONNECTED. Returns false and
 * leaves both outputs unchanged when the address is not listed. */
inline bool fixtureWiringFor(const FixtureWiringEntry* entries, std::size_t count, uint8_t address,
                             MotorControlRS::InputWiring inputs[4], MotorControlRS::InputWiring outputs[2]) {
    using MotorControlRS::InputWiring;
    for (std::size_t i = 0; entries && i < count; ++i) {
        if (entries[i].address != address) continue;
        for (uint8_t t = 0; t < 4; ++t)
            inputs[t] = (entries[i].connectedInputs >> t) & 1 ? InputWiring::CONNECTED : InputWiring::UNCONNECTED;
        for (uint8_t t = 0; t < 2; ++t)
            outputs[t] = (entries[i].connectedOutputs >> t) & 1 ? InputWiring::CONNECTED : InputWiring::UNCONNECTED;
        return true;
    }
    return false;
}
/** Bench outputs in BenchIo order (relay1, relay2, mosfet1, mosfet2). The board
 * declares which drive input terminal each one drives (wired bench 08): the same
 * terminal on every listed drive whose input mask has it. */
constexpr std::size_t FIXTURE_BENCH_OUTPUTS = 4;
constexpr int8_t FIXTURE_OUTPUT_NONE = -1; ///< Output wired to no drive input.
/** First validation error of an output map, or nullptr: each entry -1..3 and no
 * terminal driven by two outputs. */
inline const char* fixtureOutputMapError(const int8_t* terminals, std::size_t count) {
    if (!terminals || count != FIXTURE_BENCH_OUTPUTS) return "output_map_size";
    for (std::size_t i = 0; i < count; ++i) {
        if (terminals[i] < FIXTURE_OUTPUT_NONE || terminals[i] > 3) return "output_terminal_out_of_range";
        for (std::size_t j = 0; j < i; ++j)
            if (terminals[i] >= 0 && terminals[j] == terminals[i]) return "output_terminal_duplicate";
    }
    return nullptr;
}
/// Console name of a declaration source.
inline const char* wiringSourceName(WiringSource source) {
    switch (source) {
    case WiringSource::APPLICATION_DEFAULT: return "default";
    case WiringSource::BOARD: return "board";
    case WiringSource::HOST: return "host";
    case WiringSource::INVALID_TABLE: return "invalid_table";
    }
    return "unknown";
}
/// Console name of the board table state.
inline const char* fixtureStateName(FixtureState state) {
    switch (state) {
    case FixtureState::ABSENT: return "absent";
    case FixtureState::APPLIED: return "applied";
    case FixtureState::INVALID: return "invalid";
    }
    return "unknown";
}

} // namespace MotorControlRSExample
