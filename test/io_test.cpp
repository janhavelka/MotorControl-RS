// SPDX-License-Identifier: MIT
#include <MotorControlRS/profiles/ess_rs/DriverSettings.h>
#include <cassert>
#include <cstring>
#include <vector>

using namespace MotorControlRS;
namespace E = MotorControlRS::ESS_RS;
namespace {
ReadTarget target() { ReadTarget t; t.id = 4; t.address = 1; t.generation = 7; return t; }
uint32_t mask(uint8_t index) { return static_cast<uint32_t>(E::driverFieldAt(index)); }
template<class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof(value)); }
    void check(const T& value) const { assert(!std::memcmp(bytes, &value, sizeof(value))); }
};
void crc(std::vector<uint8_t>& b) {
    uint16_t c = 0xFFFF;
    for (uint8_t v : b) { c ^= v; for (unsigned i = 0; i < 8; ++i) c = c & 1 ? (c >> 1) ^ 0xA001 : c >> 1; }
    b.push_back(static_cast<uint8_t>(c)); b.push_back(static_cast<uint8_t>(c >> 8));
}
std::vector<uint8_t> reply(std::initializer_list<uint16_t> words) {
    std::vector<uint8_t> b = {1, 3, static_cast<uint8_t>(words.size() * 2)};
    for (auto v : words) { b.push_back(static_cast<uint8_t>(v >> 8)); b.push_back(static_cast<uint8_t>(v)); }
    crc(b); return b;
}
ActionEvent local(const E::DriverContext& c, ReadEventKind kind) {
    ActionEvent e; e.transport.target = c.target; e.transport.operationId = c.operationId;
    e.transport.step = c.step; e.transport.kind = kind; return e;
}
ActionEvent frame(const E::DriverContext& c, const std::vector<uint8_t>& b, bool confirmed = true) {
    auto e = local(c, ReadEventKind::FRAME); e.transport.frame = b.data(); e.transport.length = b.size();
    e.transport.txAccepted = 8; e.txComplete = e.transport.qualified = true; e.responseConfirmed = confirmed;
    e.transport.earliestUs = c.servicedUs + 1; e.transport.latestUs = c.servicedUs + 2; return e;
}
void supply(E::DriverContext& c, const std::vector<uint8_t>& b, bool confirmed = true) {
    const auto e = frame(c, b, confirmed); assert(E::advanceDriver(c, e, c.servicedUs + 3));
}
E::PreparedDriver work(const E::DriverContext& c) {
    E::PreparedDriver w; assert(E::nextDriver(c, c.servicedUs, w)); return w;
}
void ack(E::DriverContext& c, bool confirmed = true) {
    const auto w = work(c); assert(w.write); supply(c, {w.bytes, w.bytes + w.length}, confirmed);
}
E::DriverContext read(uint16_t inputMask = 0, uint16_t outputMask = 0, uint16_t custom = 0,
                      uint16_t x0 = 1, uint16_t y0 = 0, uint16_t y1 = 0) {
    E::DriverContext c; assert(E::prepareDriverRead(c, target(), 11, 17, 100, 10000, E::DriverGroup::IO));
    const uint16_t regs[] = {0x40, 0x4B, 0x4F}, counts[] = {5, 3, 1};
    const std::vector<uint8_t> replies[] = {reply({inputMask, x0, 2, 3, 0}), reply({outputMask, y0, y1}), reply({custom})};
    for (uint8_t i = 0; i < 3; ++i) {
        const auto w = work(c); assert(!w.write && w.reg == regs[i] && w.count == counts[i]);
        assert(w.reg + w.count - 1 != 0x4E); supply(c, replies[i]);
    }
    assert(c.state == ReadState::SUCCEEDED); return c;
}
E::DriverRequest request(uint8_t index = 8) {
    E::DriverRequest r; r.group = E::DriverGroup::IO; r.configurationGeneration = 17; r.fields = mask(index); return r;
}
E::DriverPrerequisites prerequisites(const E::DriverRequest& r, const E::DriverContext& baseline = read()) {
    E::DriverPrerequisites p; p.configurationGeneration = 17; assert(E::getDriver(baseline, p.previous));
    p.stationaryQualified = true; p.stationaryTarget = target(); p.maxAgeUs = 10000;
    p.stationaryEarliestUs = 110; p.stationaryLatestUs = 120;
    p.ioLevelsQualified = true; p.ioTarget = target(); p.ioConfigurationGeneration = 17;
    p.ioEarliestUs = 110; p.ioLatestUs = 120;
    for (auto& w : p.inputWiring) w = InputWiring::UNCONNECTED;
    for (auto& w : p.outputWiring) w = InputWiring::UNCONNECTED;
    for (auto& f : p.inputFacts) {
        f.supplied = true; f.statusMapping = E::IoStatusMapping::BEFORE_INVERSION;
        f.externalTriggersInhibited = true; f.protectionRemovalQualified = true;
    }
    for (auto& f : p.outputFacts) {
        f.supplied = true; f.statusMapping = E::IoStatusMapping::BEFORE_INVERSION;
        f.load = E::IoOutputLoad::NO_LOAD; f.allowedLevels = 3;
    }
    p.ioEffectsQualifiedFields = r.fields; p.qualifiedIo = r;
    return p;
}
E::DriverContext update(E::DriverRequest r = request(), bool echoReadback = false) {
    E::DriverContext c; auto p = prerequisites(r); p.allowEchoReadback = echoReadback;
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000)); return c;
}
void rejected(E::DriverRequest r, E::DriverPrerequisites p, Err code = Err::INVALID_CONFIG) {
    E::DriverContext c; c.operationId = 999; Saved<E::DriverContext> saved(c);
    const auto s = E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000);
    assert(!s && s.code == code); saved.check(c);
}
void testChoicesAndDisable() {
    for (uint8_t terminal = 0; terminal < 4; ++terminal) for (uint16_t choice = 0; choice <= 17; ++choice) {
        auto r = request(); r.fields = 0;
        assert(E::prepareInputFunction(r, terminal, static_cast<E::InputFunction>(choice)));
        auto c = update(r); const auto w = work(c);
        assert(w.reg == 0x41 + terminal && w.value == choice);
        ack(c); supply(c, reply({choice})); assert(c.state == ReadState::SUCCEEDED && !c.uncertain);
    }
    for (uint8_t terminal = 0; terminal < 2; ++terminal) for (uint16_t choice : {0, 1, 2, 3, 4, 5, 9, 10}) {
        auto r = request(); r.fields = 0;
        assert(E::prepareOutputFunction(r, terminal, static_cast<E::OutputFunction>(choice)));
        auto c = update(r); assert(work(c).reg == 0x4C + terminal && work(c).value == choice);
        ack(c); supply(c, reply({choice})); assert(c.state == ReadState::SUCCEEDED);
    }
    auto r = request(); Saved<E::DriverRequest> saved(r);
    assert(!E::prepareInputFunction(r, 4, E::InputFunction::UNDEFINED)); saved.check(r);
    assert(!E::prepareInputFunction(r, 0, static_cast<E::InputFunction>(18))); saved.check(r);
    assert(!E::prepareOutputFunction(r, 2, E::OutputFunction::UNDEFINED)); saved.check(r);
    for (uint16_t value : {6, 7, 8, 11, 12, 65535}) {
        assert(!E::prepareOutputFunction(r, 0, static_cast<E::OutputFunction>(value))); saved.check(r);
        auto candidate = request(13); candidate.outputFunctions[0] = static_cast<E::OutputFunction>(value);
        rejected(candidate, prerequisites(candidate), value == 11 ? Err::UNSUPPORTED : Err::INVALID_CONFIG);
    }
    assert(!static_cast<uint32_t>(E::driverFieldAt(16)));
    for (uint8_t index : {uint8_t(7), uint8_t(12), uint8_t(15)}) {
        auto candidate = request(index); candidate.inputPolarity = 16; candidate.outputPolarity = 4; candidate.customOutput = 4;
        rejected(candidate, prerequisites(candidate));
        candidate = request(index);
        rejected(candidate, prerequisites(candidate, read(index == 7 ? 16 : 0, index == 12 ? 4 : 0, index == 15 ? 4 : 0)));
    }
}
void testRawAndImmutable() {
    auto c = read(0xF010, 0x84, 0x80, 99, 11, 8); E::IoObservation io;
    assert(E::getIo(c, io) && io.rawInputFunctions[0] == 99 && io.rawOutputFunctions[0] == 11);
    assert(io.knownInputFunctions == 14 && io.knownOutputFunctions == 0);
    assert(io.unknownInputPolarityBits == 0xF010 && io.unknownOutputPolarityBits == 0x84 && io.unknownCustomOutputBits == 0x80);
    Saved<E::IoObservation> saved(io);
    E::DriverContext partial; assert(E::prepareDriverRead(partial, target(), 11, 17, 100, 10000, E::DriverGroup::IO));
    supply(partial, reply({0, 1, 2, 3, 0})); auto bad = reply({0, 0, 0}); bad.back() ^= 1; supply(partial, bad);
    assert(partial.state == ReadState::FAILED && !E::getIo(partial, io)); saved.check(io);
    E::DriverContext drive; assert(E::prepareDriverRead(drive, target(), 11, 17, 100, 10000));
    assert(!E::getIo(drive, io)); saved.check(io);
}
void testPrerequisitesAndMasks() {
    auto r = request(); auto p = prerequisites(r);
    p.inputsPermit = false; E::DriverContext c;
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000)); // Optional external I/O is not globally required.
    for (uint8_t i = 1; i < 4; ++i) p.inputWiring[i] = InputWiring::UNKNOWN;
    for (auto& w : p.outputWiring) w = InputWiring::UNKNOWN;
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    p.inputWiring[0] = InputWiring::UNKNOWN; rejected(r, p);
    p = prerequisites(r); p.inputWiring[0] = InputWiring::CONNECTED;
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000)); // Explicit externally qualified connected case.
    p.allowEchoReadback = true; rejected(r, p);
    p = prerequisites(r); p.ioEffectsQualifiedFields = 0; rejected(r, p);
    p = prerequisites(r); p.qualifiedIo.inputFunctions[0] = E::InputFunction::STOP; rejected(r, p);
    p = prerequisites(r); p.qualifiedIo.configurationGeneration++; rejected(r, p);
    p = prerequisites(r); p.qualifiedIo.fields |= mask(9); rejected(r, p);
    p = prerequisites(r); p.ioTarget.generation++; rejected(r, p);
    p = prerequisites(r); p.ioConfigurationGeneration++; rejected(r, p);
    p = prerequisites(r); p.ioLevelsQualified = false; rejected(r, p);
    p = prerequisites(r); p.actualInputs = 16; rejected(r, p);
    p = prerequisites(r); p.actualOutputs = 4; rejected(r, p);
    p = prerequisites(r); p.ioLatestUs = 201; rejected(r, p);
    p = prerequisites(r); p.maxAgeUs = 100; rejected(r, p);
    p = prerequisites(r); p.previous.raw[8] = 0; rejected(r, p);
    p = prerequisites(r); p.previous.provenance[0].responseConfirmed = false; rejected(r, p);
    p = prerequisites(r); r.fields |= 1; rejected(r, p);
    r = request(); r.group = E::DriverGroup::DRIVE; rejected(r, prerequisites(r));
    r = request(7); r.inputPolarity = 1; p = prerequisites(r); p.inputWiring[0] = InputWiring::UNKNOWN; rejected(r, p);
    p = prerequisites(r); p.actualInputs = 1; // Qualified pre-inversion status becomes inactive after the write.
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000)); assert(work(c).value == 1);
    p.ioEffectsQualifiedFields = 0; rejected(r, p);
    auto alias = update(); const Saved<E::DriverContext> before(alias);
    assert(!E::prepareDriverSettings(alias, target(), 12, alias.prerequisites.qualifiedIo, prerequisites(request()), 200, 9000));
    before.check(alias);
}
void testFullUpdatePartialAndCustom() {
    auto r = request(); r.fields = 0x3FE00; r.inputPolarity = 15; r.outputPolarity = 3; r.customOutput = 3;
    r.outputFunctions[0] = E::OutputFunction::CUSTOM_0; r.outputFunctions[1] = E::OutputFunction::CUSTOM_1;
    const auto fullUpdate = [&r]() {
        auto p = prerequisites(r); p.actualInputs = 15;
        E::DriverContext c; assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000)); return c;
    };
    auto c = fullUpdate(); assert(c.fieldCount == 9);
    const uint16_t registers[] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x4B, 0x4C, 0x4D, 0x4F};
    for (uint8_t step = 0; step < 9; ++step) {
        const auto w = work(c); assert(w.write && w.count == 1 && w.reg == registers[step]);
        assert(c.order[step] == step + 7);
        ack(c); assert(c.uncertain); supply(c, reply({w.value})); assert(!c.uncertain);
    }
    assert(c.state == ReadState::SUCCEEDED && c.completedSteps == 18 && c.effects == r.fields);
    for (uint8_t boundary = 0; boundary < 18; ++boundary) {
        auto partial = fullUpdate();
        while (partial.step < boundary) {
            const auto next = work(partial);
            if (next.write) ack(partial); else supply(partial, reply({partial.progress[partial.order[partial.step / 2]].requested}));
        }
        auto cancelled = local(partial, ReadEventKind::CANCEL);
        if ((boundary & 1) == 0) { cancelled.transport.txAccepted = 8; cancelled.txComplete = true; }
        assert(E::advanceDriver(partial, cancelled, partial.servicedUs + 1));
        assert(partial.outcome == E::DriverOutcome::CANCELLED && partial.uncertain);
        const Saved<E::DriverContext> saved(partial);
        assert(!E::advanceDriver(partial, cancelled, partial.servicedUs + 1)); saved.check(partial);
    }
    r = request(15); r.customOutput = 1; rejected(r, prerequisites(r));
    auto p = prerequisites(r, read(0, 0, 0, 1, 9, 0));
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    assert(work(c).reg == 0x4F && work(c).value == 1);
    rejected(r, prerequisites(r, read(0, 0, 0, 1, 10, 0))); // Cross-mapped custom selection lacks documented correspondence.
    r.customOutput = 2; rejected(r, prerequisites(r, read(0, 0, 0, 1, 10, 0)));
    r = request(); r.fields |= mask(9); c = update(r); ack(c); supply(c, reply({0})); ack(c);
    supply(c, reply({17})); assert(c.outcome == E::DriverOutcome::READBACK_MISMATCH && c.uncertain);
    assert(c.effects == r.fields && c.progress[8].readbackKnown && c.progress[9].acknowledged);
    assert(c.prerequisites.previous.raw[8] == 1 && c.request.inputFunctions[0] == E::InputFunction::UNDEFINED);
    assert(work(c).kind == E::ActionWork::DONE);
    c = update(); auto failed = local(c, ReadEventKind::TRANSPORT_FAILURE); failed.transport.txAccepted = 8; failed.txComplete = true;
    assert(E::advanceDriver(c, failed, 300) && c.uncertain && c.effects == mask(8));
    const Saved<E::DriverContext> retained(c); assert(!E::advanceDriver(c, failed, 301)); retained.check(c);
}
void testAdmissionPrecedence() {
    auto r = request(15); r.customOutput = 1;
    auto p = prerequisites(r, read(0, 0, 4));
    auto c = update(); const Saved<E::DriverContext> retained(c);
    const auto refuse = [&](E::DriverError detail, const char* message) {
        const auto status = E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000);
        assert(status.code == Err::INVALID_CONFIG && status.detail == static_cast<int32_t>(detail));
        assert(!std::strcmp(status.msg, message)); retained.check(c);
    };
    p.ioLevelsQualified = false; p.ioEffectsQualifiedFields = 0; p.outputWiring[0] = InputWiring::UNKNOWN;
    refuse(E::DriverError::INVALID_CANDIDATE, "reserved stored IO mask bits cannot be overwritten");
    assert(E::getDriver(read(), p.previous));
    refuse(E::DriverError::IO_EVIDENCE_REQUIRED, "fresh qualified actual IO levels required");
    p.ioLevelsQualified = true; p.qualifiedIo.customOutput = 0;
    refuse(E::DriverError::IO_EFFECTS_REQUIRED, "exact external-effect qualification required");
    p.ioEffectsQualifiedFields = r.fields;
    refuse(E::DriverError::IO_EFFECTS_REQUIRED, "qualified IO values differ from candidate");
    p.qualifiedIo = r;
    refuse(E::DriverError::WIRING_REQUIRED, "affected output wiring must be declared");
    p.outputWiring[0] = InputWiring::CONNECTED; p.allowEchoReadback = true;
    refuse(E::DriverError::WIRING_REQUIRED, "unconfirmed echo verification requires affected terminals unconnected");
    p.outputWiring[0] = InputWiring::UNCONNECTED;
    refuse(E::DriverError::CUSTOM_DEPENDENCY, "custom bits require corresponding terminal's canonical custom function; cross-mapping unresolved");
}
void testEchoReadbackAndFaults() {
    auto c = update(); ack(c, false); assert(c.outcome == E::DriverOutcome::UNCONFIRMED_RESPONSE && c.uncertain);
    c = update(request(), true); ack(c, false);
    assert(c.state == ReadState::ACTIVE && !c.progress[8].acknowledged && c.progress[8].execution == ActionExecution::UNKNOWN);
    assert(!c.observations[0].responseConfirmed); supply(c, reply({0}));
    assert(c.state == ReadState::SUCCEEDED && !c.uncertain && !c.progress[8].activeKnown && !c.progress[8].acknowledged);
    auto noop = request(); noop.inputFunctions[0] = E::InputFunction::ORIGIN;
    c = update(noop, true); ack(c, false); supply(c, reply({1})); assert(c.state == ReadState::SUCCEEDED && c.progress[8].execution == ActionExecution::UNKNOWN);
    c = update(request(), true); ack(c, false); supply(c, reply({1})); assert(c.uncertain && c.outcome == E::DriverOutcome::READBACK_MISMATCH);
    c = update(request(), true); ack(c, false); supply(c, reply({0}), false); assert(c.uncertain && c.outcome == E::DriverOutcome::UNCONFIRMED_RESPONSE);
    c = update(request(), true); std::vector<uint8_t> exception = {1, 0x86, 3}; crc(exception); supply(c, exception, false);
    assert(c.uncertain && c.outcome == E::DriverOutcome::REPLY_ERROR && c.step == 0);
    c = update(request(), true); const auto w = work(c); auto bad = std::vector<uint8_t>(w.bytes, w.bytes + 8); bad.back() ^= 1;
    supply(c, bad, false); assert(c.uncertain && c.step == 0 && c.status.code == Err::CRC_ERROR);
    c = update(); auto e = local(c, ReadEventKind::CANCEL); assert(E::advanceDriver(c, e, 201)); assert(!c.uncertain && !c.effects);
    c = update(); e = local(c, ReadEventKind::CANCEL); e.transport.txAccepted = 3; e.transport.executionUnknown = true;
    assert(E::advanceDriver(c, e, 201) && c.uncertain && c.effects == mask(8));
    c = update(); auto p = c.prerequisites; p.maxAgeUs = 200; p.ioEarliestUs = 95;
    assert(E::prepareDriverSettings(c, target(), 12, request(), p, 200, 9000)); assert(work(c).deadlineUs == 295);
    E::PreparedDriver untouched; untouched.reg = 99; const Saved<E::PreparedDriver> saved(untouched);
    assert(!E::nextDriver(c, 295, untouched)); saved.check(untouched);
    e = local(c, ReadEventKind::DEADLINE); assert(E::advanceDriver(c, e, 295) && !c.effects && c.deadlineUs == 9000);
}
void testInputCommissioningMatrix() {
    for (uint16_t function = 0; function <= 17; ++function)
        for (auto wiring : {InputWiring::UNKNOWN, InputWiring::UNCONNECTED, InputWiring::CONNECTED})
            for (uint16_t level : {0, 1}) {
                auto r = request(); r.inputFunctions[0] = static_cast<E::InputFunction>(function);
                auto p = prerequisites(r); p.inputWiring[0] = wiring; p.actualInputs = level;
                const bool accepted = wiring != InputWiring::UNKNOWN && !(level && function >= 4 && function <= 13);
                if (!accepted) { rejected(r, p); continue; }
                E::DriverContext c; assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
                assert(work(c).reg == 0x41 && work(c).value == function);
                ack(c); supply(c, reply({function})); assert(c.state == ReadState::SUCCEEDED);
                p.inputFacts[0].supplied = false;
                if (wiring == InputWiring::CONNECTED || function >= 4) rejected(r, p);
                else assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
            }
    for (uint16_t function = 4; function <= 17; ++function) {
        auto r = request(); r.inputFunctions[0] = static_cast<E::InputFunction>(function);
        auto p = prerequisites(r); p.inputFacts[0].externalTriggersInhibited = false; rejected(r, p);
    }
    for (uint16_t oldFunction = 2; oldFunction <= 6; ++oldFunction) {
        auto r = request(); auto p = prerequisites(r, read(0, 0, 0, oldFunction));
        p.inputWiring[0] = InputWiring::CONNECTED; p.inputFacts[0].protectionRemovalQualified = false;
        rejected(r, p); p.inputFacts[0].protectionRemovalQualified = true;
        E::DriverContext c; assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
        r = request(7); r.inputPolarity = 1;
        p = prerequisites(r, read(0, 0, 0, oldFunction)); p.inputWiring[0] = InputWiring::CONNECTED;
        p.actualInputs = 1; p.inputFacts[0].protectionRemovalQualified = false;
        rejected(r, p); // Clearing an asserted protection through inversion also needs explicit intent.
        p.inputFacts[0].protectionRemovalQualified = true;
        assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    }
}
void testInputPolarityOrder() {
    auto r = request(7); r.fields |= mask(8); r.inputPolarity = 1;
    auto p = prerequisites(r, read(0, 0, 0, 7));
    // Final NONE is safe, but inversion runs first while the old trigger exists.
    rejected(r, p);
    p.inputFacts[0].statusMapping = E::IoStatusMapping::UNKNOWN; rejected(r, p);
    p.inputFacts[0].statusMapping = E::IoStatusMapping::AFTER_INVERSION; rejected(r, p);
    p.actualInputs = 1; E::DriverContext c;
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    assert(work(c).reg == 0x40);
    // Separate observed disable permits the legacy unconnected NONE inversion.
    p = prerequisites(r, read(0, 0, 0, 0)); p.inputFacts[0] = E::IoInputFacts();
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    r = request(); r.inputFunctions[0] = E::InputFunction::STOP;
    p = prerequisites(r, read(1)); p.inputFacts[0].statusMapping = E::IoStatusMapping::UNKNOWN; rejected(r, p);
    p.inputFacts[0].statusMapping = E::IoStatusMapping::AFTER_INVERSION;
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000)); // Do not invert an after-inversion read twice.
    p.inputFacts[0].statusMapping = E::IoStatusMapping::BEFORE_INVERSION; rejected(r, p);
    p.actualInputs = 1;
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    p.inputFacts[0].statusMapping = static_cast<E::IoStatusMapping>(3); rejected(r, p);
}
void testOutputLoadsAndOrder() {
    for (uint16_t function : {0, 1, 2, 3, 4, 5, 9, 10}) {
        auto r = request(13); r.outputFunctions[0] = static_cast<E::OutputFunction>(function);
        auto p = prerequisites(r); p.outputWiring[0] = InputWiring::CONNECTED;
        p.outputFacts[0].load = E::IoOutputLoad::INDICATOR;
        E::DriverContext c; assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
        ack(c); supply(c, reply({function})); assert(c.state == ReadState::SUCCEEDED);
        assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
        ack(c, false); assert(c.outcome == E::DriverOutcome::UNCONFIRMED_RESPONSE && c.uncertain);
        p.allowEchoReadback = true; rejected(r, p); p.allowEchoReadback = false;
        p.outputFacts[0].supplied = false; rejected(r, p);
    }
    auto r = request(12); r.fields |= mask(15); r.outputPolarity = 1; r.customOutput = 1;
    auto p = prerequisites(r, read(0, 0, 0, 1, 9)); p.outputWiring[0] = InputWiring::CONNECTED;
    p.outputFacts[0].load = E::IoOutputLoad::CONTROL; p.outputFacts[0].allowedLevels = 1;
    rejected(r, p); // Final inactive does not excuse the earlier inverted active custom output.
    p.outputFacts[0].allowedLevels = 3; E::DriverContext c;
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    p.outputFacts[0].statusMapping = E::IoStatusMapping::UNKNOWN; rejected(r, p);
    r = request(12); r.outputPolarity = 1;
    p = prerequisites(r, read(0, 0, 1, 1, 9)); p.outputWiring[0] = InputWiring::CONNECTED;
    p.outputFacts[0].load = E::IoOutputLoad::CONTROL; p.outputFacts[0].allowedLevels = 1;
    // Stored custom=1 predicts inactive after inversion, but the fresh status=0
    // predicts activation. Preserve both possibilities instead of trusting storage.
    rejected(r, p);
    r = request(15); p = prerequisites(r, read(0, 0, 0, 1, 9));
    p.outputWiring[0] = InputWiring::CONNECTED; p.outputFacts[0].load = E::IoOutputLoad::CONTROL;
    p.allowEchoReadback = true; rejected(r, p); // Same-value custom writes still address the connected terminal.
    r = request(13); r.outputFunctions[0] = E::OutputFunction::ALARM;
    p = prerequisites(r); p.outputWiring[0] = InputWiring::CONNECTED;
    p.outputFacts[0].load = E::IoOutputLoad::CONTROL; p.outputFacts[0].allowedLevels = 1;
    rejected(r, p); // A current inactive sample does not promise a stable alarm output.
    p.outputFacts[0].allowedLevels = 3;
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
}
void testCommissioningAge() {
    const auto r = request(); auto p = prerequisites(r);
    p.ioQualificationUs = 90; p.ioQualificationMaxAgeUs = 150;
    E::DriverContext c; assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    assert(E::driverWriteDeadline(c) == 240);
    p.ioEarliestUs = 195; p.ioLatestUs = 199;
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    assert(E::driverWriteDeadline(c) == 240); // Refresh never renews the caller fact.
    p.ioQualificationMaxAgeUs = 100; rejected(r, p);
    p.ioQualificationUs = 201; rejected(r, p);
    E::DriverContext prior;
    assert(E::prepareDriverRead(prior, target(), 11, 17, 100, 10000));
    supply(prior, reply({0, 400})); supply(prior, reply({0, 0, 0}));
    supply(prior, reply({0, 0, 0, 0})); supply(prior, reply({0, 0}));
    E::DriverRequest drive; drive.configurationGeneration = 17; drive.fields = mask(1); drive.subdivision = 800;
    p = prerequisites(drive, prior); p.inputsPermit = true;
    p.ioQualificationUs = 90; p.ioQualificationMaxAgeUs = 150;
    assert(E::prepareDriverSettings(c, target(), 12, drive, p, 200, 9000));
    assert(E::driverWriteDeadline(c) == 240);
    p.ioTarget.generation++; rejected(drive, p);
}
void testPolarityReplySource() {
    for (uint8_t index : {uint8_t(7), uint8_t(12)}) for (uint16_t value : {0, 1}) {
        auto r = request(index);
        if (index == 7) r.inputPolarity = value; else r.outputPolarity = value;
        auto p = prerequisites(r, read(0, 0, 0, 0)); p.allowEchoReadback = true;
        // Bit 1 is unchanged, but the whole word is sent. Same-value masks
        // must obey the same confirmed-response rule as changed masks.
        auto* wiring = index == 7 ? p.inputWiring : p.outputWiring;
        for (auto state : {InputWiring::CONNECTED, InputWiring::UNKNOWN}) {
            wiring[1] = state; rejected(r, p);
        }
        p.allowEchoReadback = false; wiring[1] = InputWiring::CONNECTED;
        E::DriverContext c;
        assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
        ack(c, false); assert(c.outcome == E::DriverOutcome::UNCONFIRMED_RESPONSE && c.uncertain);
        wiring[1] = InputWiring::UNCONNECTED; p.allowEchoReadback = true;
        assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
        ack(c, false); supply(c, reply({value}));
        assert(c.state == ReadState::SUCCEEDED && !c.uncertain && !c.progress[index].acknowledged);
        assert(c.progress[index].execution == ActionExecution::UNKNOWN);
    }
}
// Wired bench 05: with UNKNOWN mapping a connected no-function terminal may be
// inverted with supplied facts; any nonzero function before or after, absent
// facts or a candidate that also assigns a function stay refused.
E::DriverContext readInputs(uint16_t polarity, const uint16_t (&functions)[4]) {
    E::DriverContext c; assert(E::prepareDriverRead(c, target(), 11, 17, 100, 10000, E::DriverGroup::IO));
    const std::vector<uint8_t> replies[] = {reply({polarity, functions[0], functions[1], functions[2], functions[3]}),
                                            reply({0, 0, 0}), reply({0})};
    for (const auto& r : replies) supply(c, r);
    assert(c.state == ReadState::SUCCEEDED); return c;
}
E::DriverPrerequisites noFunction(const E::DriverRequest& r, uint16_t polarity, uint8_t terminal,
                                  uint16_t stored = 0, InputWiring wiring = InputWiring::CONNECTED) {
    uint16_t functions[4] = {}; functions[terminal] = stored; // Only terminal t's bit changes.
    auto p = prerequisites(r, readInputs(polarity, functions));
    for (auto& w : p.inputWiring) w = wiring;
    for (auto& f : p.inputFacts) {
        f.statusMapping = E::IoStatusMapping::UNKNOWN; f.protectionRemovalQualified = false;
        f.externalTriggersInhibited = false;
    }
    return p;
}
void testConnectedNoFunctionInversion() {
    for (uint8_t terminal = 0; terminal < 4; ++terminal) {
        const uint16_t bitMask = uint16_t(1) << terminal;
        for (uint16_t from : {uint16_t(0), bitMask}) {
            auto r = request(7); r.inputPolarity = from ^ bitMask;
            auto p = noFunction(r, from, terminal);
            // After the forward inversion the status bit reads 1 (AFTER) or 0 (BEFORE);
            // the inverse write is admitted for both outcomes.
            for (uint16_t level : {uint16_t(0), bitMask}) {
                p.actualInputs = from ? level : 0;
                E::DriverContext c; assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
                const auto w = work(c); assert(w.write && w.reg == 0x40 && w.value == (from ^ bitMask));
                ack(c); supply(c, reply({uint16_t(from ^ bitMask)}));
                assert(c.state == ReadState::SUCCEEDED && !c.uncertain);
            }
            auto q = p; q.inputFacts[terminal].supplied = false; rejected(r, q); // Connected: facts required.
            q = p; q.allowEchoReadback = true; rejected(r, q);                 // Connected: confirmed reply only.
            q = p; q.inputWiring[terminal] = InputWiring::UNKNOWN; rejected(r, q);
            for (uint16_t function : {1, 2, 7, 14}) {
                // A stored nonzero function refuses the inversion, connected or not.
                auto pf = noFunction(r, from, terminal, function);
                for (auto& f : pf.inputFacts) { f.protectionRemovalQualified = true; f.externalTriggersInhibited = true; }
                rejected(r, pf);
                for (auto& w : pf.inputWiring) w = InputWiring::UNCONNECTED;
                rejected(r, pf);
                // A nonzero final function in the same candidate refuses it too.
                auto rc = r; rc.fields |= mask(8 + terminal);
                rc.inputFunctions[terminal] = static_cast<E::InputFunction>(function);
                auto pc = noFunction(rc, from, terminal);
                for (auto& f : pc.inputFacts) f.externalTriggersInhibited = true;
                rejected(rc, pc);
                for (auto& w : pc.inputWiring) w = InputWiring::UNCONNECTED;
                rejected(rc, pc);
                if (function == 14) {
                    // With a known mapping the PIN selector inversion is admitted (14-17 are passive).
                    for (auto& w : pc.inputWiring) w = InputWiring::CONNECTED;
                    for (auto& f : pc.inputFacts) f.statusMapping = E::IoStatusMapping::AFTER_INVERSION;
                    E::DriverContext c; assert(E::prepareDriverSettings(c, target(), 12, rc, pc, 200, 9000));
                }
            }
        }
    }
    // Unconnected behavior unchanged: the legacy no-function inversion with and
    // without facts is admitted; a stored function under UNKNOWN mapping is refused.
    auto r = request(7); r.inputPolarity = 1;
    auto p = noFunction(r, 0, 0, 0, InputWiring::UNCONNECTED);
    E::DriverContext c; assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    p.inputFacts[0] = E::IoInputFacts(); assert(E::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    p = noFunction(r, 0, 0, 1, InputWiring::UNCONNECTED); rejected(r, p);
}
} // namespace
static void testOptionalAgePolicy() {
    const auto r = request(); auto p = prerequisites(r); p.maxAgeUs = 0;
    E::DriverContext c;
    assert(E::prepareDriverSettings(c, target(), 12, r, p, 100000, 110000));
    assert(work(c).deadlineUs == 110000);
    const Saved<E::DriverContext> saved(c);
    p.maxAgeUs = 10000;
    assert(!E::prepareDriverSettings(c, target(), 12, r, p, 100000, 110000)); saved.check(c);
    p.maxAgeUs = 0; p.previous.target.generation++;
    assert(!E::prepareDriverSettings(c, target(), 12, r, p, 100000, 110000)); saved.check(c);
}
static void testSuppliedUncertaintyNeedsReadbackPolicy() {
    for (bool allowed : {false, true}) {
        auto c = update(request(), allowed); const auto w = work(c);
        const std::vector<uint8_t> bytes(w.bytes, w.bytes + w.length);
        auto e = frame(c, bytes); e.transport.executionUnknown = true;
        assert(E::advanceDriver(c, e, c.servicedUs + 3));
        assert(c.progress[8].execution == ActionExecution::UNKNOWN && !c.progress[8].acknowledged && c.uncertain);
        const Saved<E::DriverEvidence> retained(c.observations[0]);
        if (!allowed) {
            assert(c.state == ReadState::FAILED && c.outcome == E::DriverOutcome::UNCONFIRMED_RESPONSE);
            assert(work(c).kind == E::ActionWork::DONE);
        } else {
            assert(c.state == ReadState::ACTIVE && !work(c).write);
            supply(c, reply({0}));
            assert(c.state == ReadState::SUCCEEDED && !c.uncertain && c.progress[8].readbackKnown);
            assert(c.progress[8].execution == ActionExecution::UNKNOWN && !c.progress[8].acknowledged);
            retained.check(c.observations[0]);
        }
    }
}
int main() {
    testInputCommissioningMatrix(); testInputPolarityOrder(); testOutputLoadsAndOrder(); testCommissioningAge();
    testPolarityReplySource(); testConnectedNoFunctionInversion();
    testSuppliedUncertaintyNeedsReadbackPolicy();
    testOptionalAgePolicy();
    testAdmissionPrecedence();
    testChoicesAndDisable(); testRawAndImmutable(); testPrerequisitesAndMasks();
    testFullUpdatePartialAndCustom(); testEchoReadbackAndFaults();
}
