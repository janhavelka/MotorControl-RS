// SPDX-License-Identifier: MIT
// Board fixture wiring table: validation, lookup and the bench table itself.
#include "../examples/common/FixtureWiring.h"
#include "../examples/common/BoardPins.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace {
using namespace MotorControlRSExample;
using MotorControlRS::InputWiring;

bool error(const FixtureWiringEntry* entries, std::size_t count, const char* expected) {
    const char* found = fixtureWiringError(entries, count);
    return found && expected && !std::strcmp(found, expected) && !validFixtureWiring(entries, count);
}

void testValidAndEmptyTables() {
    const FixtureWiringEntry table[] = {{1, 0xF, 0x0}, {247, 0x0, 0x3}, {4, 0x5, 0x1}, {2, 0x0, 0x0}};
    assert(validFixtureWiring(table, 4) && !fixtureWiringError(table, 4));
    assert(validFixtureWiring(table, 0) && validFixtureWiring(nullptr, 0)); // Empty is valid, not absent.
}

void testEveryValidationError() {
    const FixtureWiringEntry duplicate[] = {{1, 0xF, 0}, {2, 0xF, 0}, {1, 0x1, 0}};
    assert(error(duplicate, 3, "duplicate_address"));
    const FixtureWiringEntry zero[] = {{0, 0xF, 0}};
    assert(error(zero, 1, "address_out_of_range"));
    const FixtureWiringEntry high[] = {{248, 0xF, 0}};
    assert(error(high, 1, "address_out_of_range"));
    const FixtureWiringEntry inputs[] = {{1, 0x10, 0}};
    assert(error(inputs, 1, "input_mask_out_of_range"));
    const FixtureWiringEntry outputs[] = {{1, 0xF, 0x4}};
    assert(error(outputs, 1, "output_mask_out_of_range"));
    const FixtureWiringEntry five[] = {{1, 0, 0}, {2, 0, 0}, {3, 0, 0}, {4, 0, 0}, {5, 0, 0}};
    assert(error(five, 5, "count_over_capacity"));
    assert(validFixtureWiring(five, 4));
    assert(error(nullptr, 1, "entries_missing"));
    // The first error in table order is named.
    const FixtureWiringEntry first[] = {{1, 0x10, 0x4}, {0, 0, 0}};
    assert(error(first, 2, "input_mask_out_of_range"));
}

void testLookupFillsListedAddressesOnly() {
    const FixtureWiringEntry table[] = {{4, 0x9, 0x2}};
    InputWiring in[4] = {InputWiring::UNKNOWN, InputWiring::UNKNOWN, InputWiring::UNKNOWN, InputWiring::UNKNOWN};
    InputWiring out[2] = {InputWiring::UNKNOWN, InputWiring::UNKNOWN};
    assert(!fixtureWiringFor(table, 1, 3, in, out));
    for (auto value : in) assert(value == InputWiring::UNKNOWN); // Unlisted: outputs unchanged.
    assert(!fixtureWiringFor(table, 0, 4, in, out) && !fixtureWiringFor(nullptr, 1, 4, in, out));
    assert(fixtureWiringFor(table, 1, 4, in, out));
    assert(in[0] == InputWiring::CONNECTED && in[1] == InputWiring::UNCONNECTED && in[2] == InputWiring::UNCONNECTED &&
           in[3] == InputWiring::CONNECTED && out[0] == InputWiring::UNCONNECTED && out[1] == InputWiring::CONNECTED);
}

void testBenchTable() {
    // Owner's wiring of 18 November 2026: X0..X3 on drives 1, 2 and 4, Y0 on drive 4 only.
    assert(kFixtureWiringCount == 3 && kFixtureWiringCount <= FIXTURE_WIRING_CAPACITY);
    assert(validFixtureWiring(kFixtureWiring, kFixtureWiringCount));
    for (uint8_t address : {uint8_t(1), uint8_t(2), uint8_t(4)}) {
        InputWiring in[4], out[2];
        assert(fixtureWiringFor(kFixtureWiring, kFixtureWiringCount, address, in, out));
        for (auto value : in) assert(value == InputWiring::CONNECTED);
        assert(out[0] == (address == 4 ? InputWiring::CONNECTED : InputWiring::UNCONNECTED));
        assert(out[1] == InputWiring::UNCONNECTED);
    }
    InputWiring in[4], out[2];
    assert(!fixtureWiringFor(kFixtureWiring, kFixtureWiringCount, 3, in, out));
}

void testNames() {
    assert(!std::strcmp(wiringSourceName(WiringSource::APPLICATION_DEFAULT), "default"));
    assert(!std::strcmp(wiringSourceName(WiringSource::BOARD), "board"));
    assert(!std::strcmp(wiringSourceName(WiringSource::HOST), "host"));
    assert(!std::strcmp(wiringSourceName(WiringSource::INVALID_TABLE), "invalid_table"));
    assert(!std::strcmp(fixtureStateName(FixtureState::ABSENT), "absent"));
    assert(!std::strcmp(fixtureStateName(FixtureState::APPLIED), "applied"));
    assert(!std::strcmp(fixtureStateName(FixtureState::INVALID), "invalid"));
}
} // namespace

int main() {
    testValidAndEmptyTables();
    testEveryValidationError();
    testLookupFillsListedAddressesOnly();
    testBenchTable();
    testNames();
    std::puts("Fixture wiring table tests passed");
    return 0;
}
