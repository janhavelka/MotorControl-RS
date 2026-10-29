// SPDX-License-Identifier: MIT
#include <MotorControlRS/profiles/ess_rs/DriverSettings.h>
#include <MotorControlRS/profiles/ess_rs/Registers.h>
#include <cassert>
#include <cstring>
#include <limits>
#include <vector>

using namespace MotorControlRS;
namespace Ess = MotorControlRS::ESS_RS;
namespace {
ReadTarget target() { ReadTarget t; t.id = 4; t.address = 1; t.generation = 7; return t; }
uint16_t field(Ess::DriverField f) { return static_cast<uint16_t>(f); }
template<class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof(value)); }
    void check(const T& value) const { assert(std::memcmp(bytes, &value, sizeof(value)) == 0); }
};
void crc(std::vector<uint8_t>& bytes) {
    uint16_t value = 0xFFFF;
    for (uint8_t b : bytes) {
        value ^= b;
        for (unsigned i = 0; i < 8; ++i) value = (value & 1) ? (value >> 1) ^ 0xA001 : value >> 1;
    }
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8));
}
std::vector<uint8_t> reply(std::initializer_list<uint16_t> words) {
    std::vector<uint8_t> bytes = {1, 3, static_cast<uint8_t>(words.size() * 2)};
    for (uint16_t value : words) { bytes.push_back(static_cast<uint8_t>(value >> 8)); bytes.push_back(static_cast<uint8_t>(value)); }
    crc(bytes); return bytes;
}
ActionEvent local(const Ess::DriverContext& c, ReadEventKind kind) {
    ActionEvent e; e.transport.target = c.target; e.transport.operationId = c.operationId;
    e.transport.step = c.step; e.transport.kind = kind; return e;
}
ActionEvent frame(const Ess::DriverContext& c, const uint8_t* bytes, std::size_t length, uint64_t at) {
    ActionEvent e = local(c, ReadEventKind::FRAME); e.transport.frame = bytes; e.transport.length = length;
    e.transport.txAccepted = 8; e.txComplete = e.responseConfirmed = e.transport.qualified = true;
    e.transport.earliestUs = at; e.transport.latestUs = at + 1; return e;
}
void supply(Ess::DriverContext& c, const std::vector<uint8_t>& bytes, uint64_t at = 0) {
    if (!at) at = c.servicedUs + 2;
    assert(Ess::advanceDriver(c, frame(c, bytes.data(), bytes.size(), at), at + 2));
}
Ess::DriverObservation snapshot(uint16_t order = 0, uint16_t enable = 0, uint16_t direction = 0) {
    Ess::DriverContext c; assert(Ess::prepareDriverRead(c, target(), 11, 17, 100, 10000));
    supply(c, reply({direction, 1600})); supply(c, reply({0, enable, order}));
    supply(c, reply({0x1234, 0x5678, 0x9ABC, 0xDEF0})); supply(c, reply({0, 1}));
    Ess::DriverObservation value; assert(Ess::getDriver(c, value)); return value;
}
Ess::DriverPrerequisites prerequisites(uint16_t enable = 0) {
    Ess::DriverPrerequisites p; p.configurationGeneration = 17; p.previous = snapshot(0, enable);
    p.stationaryQualified = p.inputsPermit = true; p.stationaryTarget = target();
    p.stationaryEarliestUs = 110; p.stationaryLatestUs = 120; p.maxAgeUs = 10000;
    return p;
}
Ess::DriverRequest request(uint16_t mask = 1) {
    Ess::DriverRequest r; r.configurationGeneration = 17; r.fields = mask;
    r.direction = Ess::DefaultDirection::REVERSED; r.subdivision = 3200; return r;
}
Ess::DriverContext update(uint16_t mask = 1, uint64_t deadline = 9000) {
    Ess::DriverContext c; assert(Ess::prepareDriverSettings(c, target(), 12, request(mask), prerequisites(), 200, deadline));
    return c;
}
Ess::PreparedDriver work(const Ess::DriverContext& c, uint64_t at = 0) {
    Ess::PreparedDriver w; assert(Ess::nextDriver(c, at ? at : c.servicedUs, w)); return w;
}
void ack(Ess::DriverContext& c, uint64_t at = 0) {
    const Ess::PreparedDriver w = work(c); assert(w.write);
    supply(c, std::vector<uint8_t>(w.bytes, w.bytes + w.length), at);
}
void checkUnchangedReject(Ess::DriverRequest r, Ess::DriverPrerequisites p, Err expected = Err::INVALID_CONFIG) {
    Ess::DriverContext c; c.operationId = 999; const Saved<Ess::DriverContext> saved(c);
    const Status s = Ess::prepareDriverSettings(c, target(), 12, r, p, 200, 9000);
    assert(!s && s.code == expected); saved.check(c);
}
void testReadAndPairOrder() {
    const auto high = snapshot(); assert(high.knownFields == 127 && high.pairKnown);
    assert(high.positiveBits == 0x12345678 && high.negativeBits == 0x9ABCDEF0);
    const auto low = snapshot(1); assert(low.positiveBits == 0x56781234 && low.negativeBits == 0xDEF09ABC);
    const auto unknown = snapshot(5, 7, 8);
    assert(!unknown.pairKnown && unknown.positiveBits == 0 && unknown.positiveWords[0] == 0x1234);
    assert(!(unknown.knownFields & (1 | 4 | 8)) && unknown.raw[0] == 8 && unknown.raw[2] == 5);
    Ess::DriverContext c; assert(Ess::prepareDriverRead(c, target(), 3, 17, 100, 500));
    const Saved<Ess::DriverObservation> immutable(high);
    const uint16_t regs[] = {0x10, 0x17, 0x37, 0x50}, counts[] = {2, 3, 4, 2};
    const auto w = work(c); assert(w.reg == regs[0] && w.count == counts[0] && !w.write);
    const auto again = work(c); assert(!std::memcmp(w.bytes, again.bytes, 8));
    supply(c, reply({0, 1600})); supply(c, reply({0, 0, 0}));
    auto corrupt = reply({1, 2, 3, 4}); corrupt.back() ^= 1; supply(c, corrupt);
    assert(c.state == ReadState::FAILED && c.outcome == Ess::DriverOutcome::REPLY_ERROR && c.completedSteps == 2);
    auto previous = high; assert(!Ess::getDriver(c, previous)); immutable.check(previous);
    Ess::PreparedDriver done; assert(Ess::nextDriver(c, c.servicedUs, done) && done.kind == Ess::ActionWork::DONE && !done.length);
    Ess::DriverContext d; assert(Ess::prepareDriverRead(d, target(), 3, 17, 100, 130));
    supply(d, reply({0, 1600})); supply(d, reply({0, 0, 0})); supply(d, reply({1, 2, 3, 4}));
    const auto bytes = reply({0, 1}); auto e = frame(d, bytes.data(), bytes.size(), 125);
    assert(Ess::advanceDriver(d, e, 1000) && d.state == ReadState::SUCCEEDED);
    for (unsigned i = 0; i < 4; ++i) {
        Ess::DriverContext n; assert(Ess::prepareDriverRead(n, target(), 3, 17, 100, 1000));
        while (n.step < i) {
            if (n.step == 0) supply(n, reply({0, 1600}));
            else if (n.step == 1) supply(n, reply({0, 0, 0}));
            else supply(n, reply({1, 2, 3, 4}));
        }
        const auto p = work(n); assert(p.reg == regs[i] && p.count == counts[i] && p.count <= 16);
    }
}
void testWholeCandidateAndPrerequisites() {
    auto r = request(); auto p = prerequisites();
    r.fields = 0; checkUnchangedReject(r, p);
    r = request(0x8000); checkUnchangedReject(r, p);
    for (unsigned i = 0; i < 7; ++i) {
        r = request(static_cast<uint16_t>(1u << i));
        switch (i) {
        case 0: r.direction = static_cast<Ess::DefaultDirection>(2); break;
        case 1: r.subdivision = 399; break;
        case 2: r.wordOrder = static_cast<Ess::WordOrder>(2); break;
        case 3: r.softLimitEnable = static_cast<Ess::SoftLimitEnable>(2); break;
        case 4: r.overLimitStop = static_cast<Ess::OverLimitStop>(2); break;
        case 5: r.interruption = static_cast<Ess::PvTriggerMode>(2); break;
        default: r.positionMode = static_cast<Ess::PositionMode>(2); break;
        }
        checkUnchangedReject(r, p);
    }
    r = request(3); r.subdivision = 51201; checkUnchangedReject(r, p);
    for (uint16_t native : {uint16_t(400), uint16_t(51200)}) {
        r = request(2); r.subdivision = native; Ess::DriverContext c;
        assert(Ess::prepareDriverSettings(c, target(), 3, r, p, 200, 1000) && work(c).value == native);
    }
    r = request(1 | 128 | 256); r.positiveLimit = std::numeric_limits<int64_t>::max();
    r.negativeLimit = std::numeric_limits<int64_t>::min(); checkUnchangedReject(r, p, Err::UNSUPPORTED);
    r = request(); p.previous.target.address = 2; checkUnchangedReject(r, p);
    p = prerequisites(); p.previous.configurationGeneration++; checkUnchangedReject(r, p);
    p = prerequisites(); p.configurationGeneration++; checkUnchangedReject(r, p);
    p = prerequisites(); p.previous.raw[0] = 1; checkUnchangedReject(r, p);
    p = prerequisites(); p.maxAgeUs = 90; checkUnchangedReject(r, p);
    p = prerequisites(); p.maxAgeUs = 100;
    // Closure happened after 100, but the first read was eligible at 100.
    // It cannot become newly fresh by waiting until the final reply arrived.
    p.stationaryEarliestUs = p.stationaryLatestUs = 199; checkUnchangedReject(r, p);
    p = prerequisites(); p.inputsPermit = false; checkUnchangedReject(r, p);
    p = prerequisites(); p.allowEchoReadback = true; checkUnchangedReject(r, p);
    p = prerequisites(); p.stationaryQualified = false; checkUnchangedReject(r, p);
    p = prerequisites(); p.stationaryTarget.generation++; checkUnchangedReject(r, p);
    p = prerequisites(); p.stationaryLatestUs = 201; checkUnchangedReject(r, p);
    for (uint16_t invalid : {uint16_t(4), uint16_t(8), uint16_t(0x8000)}) {
        p = prerequisites(); p.rawMotion = invalid; checkUnchangedReject(r, p);
    }
    p = prerequisites(); p.rawAlarm = 1; checkUnchangedReject(r, p);
    // Released is a state flag, not evidence of running; settings still need the explicit stationary/input qualification.
    p = prerequisites(); p.rawMotion = 16; Ess::DriverContext c;
    assert(Ess::prepareDriverSettings(c, target(), 3, r, p, 200, 1000));
    const Saved<Ess::DriverContext> saved(c);
    assert(!Ess::prepareDriverSettings(c, target(), 4, c.request, c.prerequisites, 200, 1000)); saved.check(c);
}
void testAdmissionPrecedenceAndAliasing() {
    auto c = update(); const Saved<Ess::DriverContext> retained(c);
    auto r = request(); auto p = prerequisites(); auto t = target();
    uint32_t operationId = 12; uint64_t deadline = 9000;
    const auto refuse = [&](Err code, Ess::DriverError detail, const char* message) {
        const Status status = Ess::prepareDriverSettings(c, t, operationId, r, p, 200, deadline);
        assert(status.code == code && status.detail == static_cast<int32_t>(detail));
        assert(!std::strcmp(status.msg, message)); retained.check(c);
    };
    // Leave later failures present while removing each earlier rejection.
    t.id = 0; operationId = 0; deadline = 200; r.fields = 0;
    refuse(Err::INVALID_CONFIG, Ess::DriverError::INVALID_TARGET, "invalid driver-settings target");
    t = target();
    refuse(Err::INVALID_CONFIG, Ess::DriverError::INVALID_OPERATION, "zero operation or configuration generation");
    operationId = 12;
    refuse(Err::INVALID_CONFIG, Ess::DriverError::INVALID_DEADLINE, "expired driver-settings deadline");
    deadline = 9000; p.allowEchoReadback = true;
    refuse(Err::INVALID_CONFIG, Ess::DriverError::INVALID_CANDIDATE, "invalid or empty driver-settings selection");
    r = request(1 | 128); r.direction = static_cast<Ess::DefaultDirection>(2);
    refuse(Err::INVALID_CONFIG, Ess::DriverError::INVALID_CANDIDATE, "echo/readback policy requires IO or inhibited segment settings");
    p.allowEchoReadback = false;
    refuse(Err::UNSUPPORTED, Ess::DriverError::PAIR_WRITE_UNSUPPORTED, "software-limit pair has no reviewed FC10 window; split writes are unavailable");
    r.fields = 1; p.previous.configurationGeneration++;
    refuse(Err::INVALID_CONFIG, Ess::DriverError::INVALID_CANDIDATE, "illegal driver-setting enum or native range");
    r = request(); p.previous.provenance[0].txComplete = false; p.previous.raw[0]++;
    p.stationaryQualified = false;
    refuse(Err::INVALID_CONFIG, Ess::DriverError::STALE_SETTINGS, "previous driver-settings context does not match");
    p.previous.configurationGeneration--;
    refuse(Err::INVALID_CONFIG, Ess::DriverError::STALE_SETTINGS, "previous driver-settings provenance is invalid");
    p.previous.provenance[0].txComplete = true;
    refuse(Err::INVALID_CONFIG, Ess::DriverError::STALE_SETTINGS, "previous settings disagree with checked raw provenance");
    p.previous.raw[0]--; p.maxAgeUs = 100;
    refuse(Err::INVALID_CONFIG, Ess::DriverError::STATIONARY_REQUIRED, "fresh qualified stopped-state and input policy required");
    p.stationaryQualified = true; p.stationaryEarliestUs = p.stationaryLatestUs = 199;
    refuse(Err::INVALID_CONFIG, Ess::DriverError::STALE_SETTINGS, "previous driver-settings observation is stale");

    p = prerequisites();
    Status status = Ess::prepareDriverSettings(c, target(), 0, c.request, p, 200, 9000);
    assert(status.detail == static_cast<int32_t>(Ess::DriverError::INVALID_OPERATION)); retained.check(c);
    status = Ess::prepareDriverSettings(c, target(), 12, c.request, p, 200, 9000);
    assert(status.code == Err::INVALID_CONFIG && status.detail == static_cast<int32_t>(Ess::DriverError::INVALID_CANDIDATE));
    assert(!std::strcmp(status.msg, "preparation inputs must not alias output members")); retained.check(c);
    status = Ess::prepareDriverSettings(c, target(), 12, r, c.prerequisites, 200, 9000);
    assert(status.code == Err::INVALID_CONFIG && !std::strcmp(status.msg, "preparation inputs must not alias output members")); retained.check(c);
    // A target reference inside output is copied before its in-place reinitialization.
    assert(Ess::prepareDriverSettings(c, c.target, 13, r, p, 200, 9000));
    assert(c.target.id == target().id && c.target.address == target().address && c.target.generation == target().generation);
    assert(Ess::prepareDriverSettings(c, c.prerequisites.previous.target, 14, r, p, 200, 9000));
    assert(c.target.id == target().id && work(c).reg == Ess::Registers::DEFAULT_DIRECTION);
    assert(Ess::prepareDriverRead(c, c.target, 15, 17, 200, 9000));
    assert(c.target.id == target().id && c.operationId == 15 && !work(c).write);
}
void testLimitDependencies() {
    auto r = request(1); auto p = prerequisites(1); checkUnchangedReject(r, p);
    r.fields |= field(Ess::DriverField::SOFT_LIMIT_ENABLE); r.softLimitEnable = Ess::SoftLimitEnable::LIMITS_OFF;
    Ess::DriverContext c; assert(Ess::prepareDriverSettings(c, target(), 3, r, p, 200, 9000));
    assert(work(c).reg == 0x18 && work(c).value == 0); ack(c); supply(c, reply({0}));
    assert(work(c).reg == 0x10); assert(c.prerequisites.previous.raw[3] == 1);
    r = request(8); r.softLimitEnable = Ess::SoftLimitEnable::AFTER_HOMING; p = prerequisites();
    checkUnchangedReject(r, p);
    p.limitSemanticsQualified = p.homedReferenceQualified = true; p.referenceTarget = target();
    p.referenceConfigurationGeneration = 17; p.rawMotion = 2;
    std::memcpy(p.qualifiedPositiveWords, p.previous.positiveWords, sizeof(p.qualifiedPositiveWords));
    std::memcpy(p.qualifiedNegativeWords, p.previous.negativeWords, sizeof(p.qualifiedNegativeWords));
    p.positiveLimit = 100; p.negativeLimit = -100;
    assert(Ess::prepareDriverSettings(c, target(), 3, r, p, 200, 9000));
    p.qualifiedPositiveWords[0]++; checkUnchangedReject(r, p); p.qualifiedPositiveWords[0]--;
    p.qualifiedNegativeWords[1]++; checkUnchangedReject(r, p); p.qualifiedNegativeWords[1]--;
    p.referenceConfigurationGeneration++; checkUnchangedReject(r, p); p.referenceConfigurationGeneration--;
    p.rawMotion = 0; checkUnchangedReject(r, p); p.rawMotion = 2;
    p.rawMotion = 0x12; checkUnchangedReject(r, p); p.rawMotion = 2;
    p.positiveLimit = p.negativeLimit; checkUnchangedReject(r, p);
    p.positiveLimit = 0x10000000; checkUnchangedReject(r, p);
    p.positiveLimit = 100; p.negativeLimit = -0x10000000; checkUnchangedReject(r, p);
    p.negativeLimit = -100; p.referenceTarget.generation++; checkUnchangedReject(r, p);
    p.referenceTarget = target(); r.fields |= 2; checkUnchangedReject(r, p);
}
void testProgressNoRollbackAndCopiedStorage() {
    auto p = prerequisites(); auto r = request(127); r.wordOrder = Ess::WordOrder::LOW_WORD_FIRST;
    Ess::DriverContext c; assert(Ess::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
    p.previous.raw[0] = 99; r.direction = Ess::DefaultDirection::NORMAL;
    assert(c.prerequisites.previous.raw[0] == 0 && c.request.direction == Ess::DefaultDirection::REVERSED);
    for (unsigned i = 0; i < 7; ++i) {
        const auto w = work(c); const unsigned index = c.order[c.step / 2];
        assert(w.write && w.reg == c.progress[index].reg && w.count == 1);
        ack(c); assert(c.progress[index].acknowledged && c.uncertain);
        const auto rd = work(c); assert(!rd.write && rd.reg == w.reg && rd.count == 1);
        supply(c, reply({w.value})); assert(!c.uncertain && !c.progress[index].activeKnown);
    }
    assert(c.state == ReadState::SUCCEEDED && c.completedSteps == 14 && c.effects == 127);
    assert(!Ess::advanceDriver(c, local(c, ReadEventKind::CANCEL), c.servicedUs));
    Ess::DriverContext partial = update(3); ack(partial); supply(partial, reply({1}));
    ack(partial); supply(partial, reply({3199}));
    assert(partial.outcome == Ess::DriverOutcome::READBACK_MISMATCH && partial.effects == 3 && partial.uncertain);
    assert(partial.progress[0].readbackKnown && partial.progress[0].readback == 1);
    assert(partial.progress[1].acknowledged && partial.progress[1].readback == 3199);
    assert(partial.prerequisites.previous.raw[1] == 1600 && partial.request.subdivision == 3200);
    assert(work(partial).kind == Ess::ActionWork::DONE);
    auto rejected = update(3); ack(rejected); supply(rejected, reply({1}));
    std::vector<uint8_t> exception = {1, 0x86, 3}; crc(exception); supply(rejected, exception);
    assert(rejected.outcome == Ess::DriverOutcome::REPLY_ERROR && rejected.progress[1].execution == ActionExecution::REJECTED);
    assert(rejected.effects == 3 && !rejected.uncertain && rejected.progress[0].readbackKnown);
    auto unknownException = update(); exception = {1, 0x86, 0x7F}; crc(exception); supply(unknownException, exception);
    assert(unknownException.uncertain && unknownException.progress[0].execution == ActionExecution::UNKNOWN);
}
void testFaultsCancellationAndExpiry() {
    for (unsigned step = 0; step < 4; ++step) {
        auto c = update(3);
        while (c.step < step) { if ((c.step & 1) == 0) ack(c); else supply(c, reply({1})); }
        auto e = local(c, ReadEventKind::CANCEL);
        if ((step & 1) == 0) { e.transport.txAccepted = 3; e.transport.executionUnknown = true; }
        assert(Ess::advanceDriver(c, e, c.servicedUs + 1));
        assert(c.outcome == Ess::DriverOutcome::CANCELLED);
        assert(c.uncertain);
        const Saved<Ess::DriverContext> retained(c);
        assert(!Ess::advanceDriver(c, e, c.servicedUs + 1)); retained.check(c);
    }
    auto unsent = update(); assert(Ess::advanceDriver(unsent, local(unsent, ReadEventKind::CANCEL), 201));
    assert(!unsent.effects && !unsent.uncertain);
    auto settledPartial = update(3); ack(settledPartial); supply(settledPartial, reply({1}));
    assert(Ess::advanceDriver(settledPartial, local(settledPartial, ReadEventKind::CANCEL), settledPartial.servicedUs + 1));
    assert(settledPartial.effects == 1 && !settledPartial.uncertain && !settledPartial.progress[1].acknowledged);
    auto lost = update(); auto timeout = local(lost, ReadEventKind::TRANSPORT_FAILURE);
    timeout.transport.txAccepted = 8; timeout.txComplete = true;
    assert(Ess::advanceDriver(lost, timeout, 300)); assert(lost.uncertain && lost.effects == 1);
    auto wrong = update(); const auto w = work(wrong); auto bytes = std::vector<uint8_t>(w.bytes, w.bytes + w.length);
    bytes[5] ^= 1; bytes.resize(6); crc(bytes); supply(wrong, bytes);
    assert(wrong.outcome == Ess::DriverOutcome::REPLY_ERROR && wrong.uncertain);
    auto echo = update(); const auto ew = work(echo); auto e = frame(echo, ew.bytes, 8, 220); e.responseConfirmed = false;
    assert(Ess::advanceDriver(echo, e, 222) && echo.outcome == Ess::DriverOutcome::UNCONFIRMED_RESPONSE && echo.uncertain);
    auto crcBad = update(); const auto cw = work(crcBad); bytes.assign(cw.bytes, cw.bytes + 8); bytes[7] ^= 1;
    supply(crcBad, bytes); assert(crcBad.status.code == Err::CRC_ERROR && crcBad.uncertain);
    auto foreign = update(); bytes.assign(cw.bytes, cw.bytes + 6); bytes[0] = 2; crc(bytes);
    supply(foreign, bytes); assert(foreign.status.code == Err::FRAME_ERROR && foreign.uncertain);
    auto invalid = update(); const Saved<Ess::DriverContext> saved(invalid);
    e = local(invalid, ReadEventKind::CANCEL); e.transport.target.generation++;
    assert(!Ess::advanceDriver(invalid, e, 201)); saved.check(invalid);
    e = local(invalid, ReadEventKind::CANCEL); e.transport.operationId++;
    assert(!Ess::advanceDriver(invalid, e, 201)); saved.check(invalid);
    e = local(invalid, ReadEventKind::CANCEL); e.transport.step++;
    assert(!Ess::advanceDriver(invalid, e, 201)); saved.check(invalid);
    e = local(invalid, ReadEventKind::CANCEL); e.transport.txAccepted = 9;
    assert(!Ess::advanceDriver(invalid, e, 201)); saved.check(invalid);
    e = local(invalid, ReadEventKind::DEADLINE); assert(!Ess::advanceDriver(invalid, e, 201)); saved.check(invalid);
    auto boundary = update(1, 300); ack(boundary); bytes = reply({1}); e = frame(boundary, bytes.data(), bytes.size(), 298);
    assert(Ess::advanceDriver(boundary, e, 9000) && boundary.state == ReadState::SUCCEEDED && !boundary.uncertain);
    auto late = update(1, 300); ack(late); e = frame(late, bytes.data(), bytes.size(), 300);
    assert(Ess::advanceDriver(late, e, 9000) && late.outcome == Ess::DriverOutcome::DEADLINE && late.uncertain);
    auto partialLate = update(3, 300); ack(partialLate); e = frame(partialLate, bytes.data(), bytes.size(), 298);
    assert(Ess::advanceDriver(partialLate, e, 9000) && partialLate.outcome == Ess::DriverOutcome::DEADLINE);
    assert(!partialLate.uncertain && partialLate.effects == 1 && work(partialLate).kind == Ess::ActionWork::DONE);
    auto p = prerequisites(); p.maxAgeUs = 200; p.stationaryEarliestUs = 95; Ess::DriverContext capped;
    assert(Ess::prepareDriverSettings(capped, target(), 3, request(), p, 200, 9000));
    assert(work(capped).deadlineUs == 295 && capped.deadlineUs == 9000);
    Ess::PreparedDriver pending; assert(!Ess::nextDriver(capped, 295, pending));
    assert(Ess::advanceDriver(capped, local(capped, ReadEventKind::DEADLINE), 295));
    assert(capped.outcome == Ess::DriverOutcome::DEADLINE && !capped.effects);
    assert(Ess::prepareDriverSettings(capped, target(), 3, request(), p, 200, 9000));
    e = local(capped, ReadEventKind::DEADLINE); e.transport.txAccepted = 8; e.txComplete = true;
    assert(Ess::advanceDriver(capped, e, 400) && capped.uncertain && capped.effects == 1);
    assert(capped.deadlineUs == 9000);
    assert(Ess::prepareDriverSettings(capped, target(), 3, request(), p, 200, 9000));
    const auto capWork = work(capped); e = frame(capped, capWork.bytes, 8, 310);
    assert(Ess::advanceDriver(capped, e, 400) && capped.outcome == Ess::DriverOutcome::DEADLINE);
    assert(!capped.progress[0].acknowledged && capped.progress[0].execution == ActionExecution::UNKNOWN && capped.uncertain && !capped.progress[0].activeKnown);
}
void testUnconfirmedReadEvidence() {
    const auto historical = snapshot();
    const Saved<Ess::DriverObservation> retained(historical);
    for (uint8_t rejectedStep = 0; rejectedStep < Ess::DRIVER_READ_STEPS; ++rejectedStep) {
        Ess::DriverContext c; assert(Ess::prepareDriverRead(c, target(), 3, 17, 100, 1000));
        const std::vector<uint8_t> frames[] = {
            reply({0, 1600}), reply({0, 0, 0}), reply({1, 2, 3, 4}), reply({0, 1})
        };
        for (uint8_t i = 0; i < rejectedStep; ++i) supply(c, frames[i]);
        const auto& bytes = frames[rejectedStep];
        auto e = frame(c, bytes.data(), bytes.size(), c.servicedUs + 2);
        e.responseConfirmed = false;
        assert(Ess::advanceDriver(c, e, c.servicedUs + 4));
        assert(c.outcome == Ess::DriverOutcome::UNCONFIRMED_RESPONSE && c.state == ReadState::FAILED);
        assert(c.completedSteps == rejectedStep && !c.observations[rejectedStep].responseConfirmed);
        assert(work(c).kind == Ess::ActionWork::DONE);
        auto previous = historical;
        assert(!Ess::getDriver(c, previous)); retained.check(previous);
    }
    // A retained observation from a formerly permissive reader must not become
    // valid preparation evidence merely because its CRC and decoded values match.
    for (uint8_t i = 0; i < Ess::DRIVER_READ_STEPS; ++i) {
        auto p = prerequisites(); p.previous.provenance[i].responseConfirmed = false;
        checkUnchangedReject(request(), p);
    }
    for (uint16_t mask : {uint16_t(1), uint16_t(3)}) {
        auto c = update(mask); const Saved<Ess::DriverObservation> previous(c.prerequisites.previous); ack(c);
        const auto bytes = reply({1}); auto e = frame(c, bytes.data(), bytes.size(), c.servicedUs + 2);
        e.responseConfirmed = false;
        assert(Ess::advanceDriver(c, e, c.servicedUs + 4));
        assert(c.outcome == Ess::DriverOutcome::UNCONFIRMED_RESPONSE && c.state == ReadState::FAILED);
        assert(c.completedSteps == 1 && c.effects == 1 && c.uncertain);
        assert(c.progress[0].acknowledged && !c.progress[0].readbackKnown);
        assert(!c.progress[1].acknowledged && work(c).kind == Ess::ActionWork::DONE);
        previous.check(c.prerequisites.previous);
    }
    // Parser errors retain precedence over the source-confirmation failure.
    auto c = update(); ack(c); auto bytes = reply({1}); bytes.back() ^= 1;
    auto e = frame(c, bytes.data(), bytes.size(), c.servicedUs + 2); e.responseConfirmed = false;
    assert(Ess::advanceDriver(c, e, c.servicedUs + 4));
    assert(c.outcome == Ess::DriverOutcome::REPLY_ERROR && c.status.code == Err::CRC_ERROR && c.uncertain);
}
Ess::DriverRequest positionStartRequest(uint32_t native = 3000) {
    Ess::DriverRequest r; r.configurationGeneration = 17;
    assert(Ess::preparePositionStartSpeed(r, native)); return r;
}
Ess::DriverObservation positionStartSnapshot(uint16_t raw = 0xFFFF) {
    Ess::DriverContext c;
    assert(Ess::prepareDriverRead(c, target(), 11, 17, 100, 1000, Ess::DriverGroup::POSITION_START_SPEED));
    const uint8_t expected[] = {1, 3, 0, 0x20, 0, 1, 0x85, 0xC0};
    const auto pending = work(c);
    assert(!pending.write && pending.reg == 0x0020 && pending.count == 1 && pending.length == sizeof(expected));
    assert(!std::memcmp(pending.bytes, expected, sizeof(expected)));
    auto bytes = reply({raw}); supply(c, bytes);
    bytes.assign(bytes.size(), 0); // The producing read owns its returned evidence.
    Ess::DriverObservation value; assert(Ess::getDriver(c, value));
    assert(value.raw[0] == raw && value.knownFields == (raw <= 3000 ? 1u : 0u));
    assert(value.provenance[0].reg == 0x0020 && value.provenance[0].count == 1 && value.provenance[0].length == 7);
    assert(c.completedSteps == 1 && work(c).kind == Ess::ActionWork::DONE);
    return value;
}
Ess::DriverPrerequisites positionStartPrerequisites() {
    Ess::DriverPrerequisites p; p.configurationGeneration = 17; p.previous = positionStartSnapshot();
    p.stationaryQualified = p.inputsPermit = p.externalTriggerInhibitedQualified = true;
    p.stationaryTarget = p.triggerTarget = target(); p.triggerConfigurationGeneration = 17;
    p.stationaryEarliestUs = p.triggerEarliestUs = 110;
    p.stationaryLatestUs = p.triggerLatestUs = 120; p.maxAgeUs = 10000;
    return p;
}
Ess::DriverContext positionStartUpdate(uint64_t deadline = 9000) {
    Ess::DriverContext c;
    assert(Ess::prepareDriverSettings(c, target(), 12, positionStartRequest(), positionStartPrerequisites(), 200, deadline));
    return c;
}
void testPositionStartSpeedSelectionAndAdmission() {
    // ESS-RS function manual physical p70: one RW word, native domain 0..3000.
    // Its conflicting default 30 (60 r/min) supplies no physical scaling here.
    Ess::DriverRequest r; r.configurationGeneration = 17;
    for (uint32_t native : {0u, 1u, 2999u, 3000u}) {
        assert(Ess::preparePositionStartSpeed(r, native));
        assert(r.group == Ess::DriverGroup::POSITION_START_SPEED && r.fields == 1 &&
            r.positionStartSpeed == native && r.configurationGeneration == 17);
    }
    const Saved<Ess::DriverRequest> saved(r);
    for (uint32_t native : {3001u, 65535u, 65536u, UINT32_MAX}) {
        assert(!Ess::preparePositionStartSpeed(r, native)); saved.check(r);
    }
    auto mixed = request(); const Saved<Ess::DriverRequest> retained(mixed);
    assert(!Ess::preparePositionStartSpeed(mixed, 1)); retained.check(mixed);
    r.fields = 3; const Saved<Ess::DriverRequest> extra(r);
    assert(!Ess::preparePositionStartSpeed(r, 1)); extra.check(r);
    r = positionStartRequest();
    assert(static_cast<uint32_t>(Ess::driverFieldAt(0, r.group)) == 1);
    assert(static_cast<uint32_t>(Ess::driverFieldAt(1, r.group)) == 0);
    for (uint16_t raw : {uint16_t(0), uint16_t(3000), uint16_t(3001), uint16_t(0xFFFF)}) positionStartSnapshot(raw);

    auto p = positionStartPrerequisites();
    for (uint16_t native : {uint16_t(0), uint16_t(3000)}) {
        r = positionStartRequest(native); Ess::DriverContext c;
        assert(Ess::prepareDriverSettings(c, target(), 12, r, p, 200, 9000));
        assert(c.fieldCount == 1 && c.progress[0].selected && c.progress[0].previous == 0xFFFF);
        assert(work(c).reg == 0x0020 && work(c).write && work(c).value == native && work(c).count == 1);
    }
    r = positionStartRequest();
    for (uint16_t bad : {uint16_t(3001), uint16_t(0xFFFF)}) {
        r.positionStartSpeed = bad; checkUnchangedReject(r, p);
    }
    r = positionStartRequest(); r.fields = 3; checkUnchangedReject(r, p);
    r = positionStartRequest(); r.group = static_cast<Ess::DriverGroup>(255); checkUnchangedReject(r, p);
    r = positionStartRequest();
    p.allowEchoReadback = true; checkUnchangedReject(r, p); p.allowEchoReadback = false;
    p.previous.configurationGeneration++; checkUnchangedReject(r, p); p.previous.configurationGeneration--;
    p.previous.target.generation++; checkUnchangedReject(r, p); p.previous.target.generation--;
    p.previous.raw[0]--; checkUnchangedReject(r, p); p.previous.raw[0]++;
    p.inputsPermit = false; checkUnchangedReject(r, p); p.inputsPermit = true;
    p.stationaryQualified = false; checkUnchangedReject(r, p); p.stationaryQualified = true;
    p.rawMotion = 4; checkUnchangedReject(r, p); p.rawMotion = 0;
    p.rawAlarm = 1; checkUnchangedReject(r, p); p.rawAlarm = 0;
    p.externalTriggerInhibitedQualified = false; checkUnchangedReject(r, p); p.externalTriggerInhibitedQualified = true;
    p.triggerTarget.address++; checkUnchangedReject(r, p); p.triggerTarget.address--;
    p.triggerConfigurationGeneration++; checkUnchangedReject(r, p); p.triggerConfigurationGeneration--;
    p.triggerLatestUs = 201; checkUnchangedReject(r, p); p.triggerLatestUs = 120;
    p.triggerEarliestUs = 121; checkUnchangedReject(r, p); p.triggerEarliestUs = 110;
    p.maxAgeUs = 200; p.triggerEarliestUs = 0; checkUnchangedReject(r, p);

    Ess::DriverContext c; c.operationId = 999; const Saved<Ess::DriverContext> old(c);
    auto t = target(); t.address = 0;
    assert(!Ess::prepareDriverSettings(c, t, 12, r, positionStartPrerequisites(), 200, 9000)); old.check(c);
    assert(!Ess::prepareDriverRead(c, target(), 12, 17, 100, 1000, static_cast<Ess::DriverGroup>(255))); old.check(c);
}
void testPositionStartSpeedEvidenceAndErrors() {
    const uint8_t write[] = {1, 6, 0, 0x20, 0x0B, 0xB8, 0x8F, 0x42};
    const uint8_t read[] = {1, 3, 0, 0x20, 0, 1, 0x85, 0xC0};
    auto c = positionStartUpdate(); const auto pending = work(c);
    assert(pending.length == sizeof(write) && !std::memcmp(pending.bytes, write, sizeof(write)));
    auto response = std::vector<uint8_t>(write, write + sizeof(write)); supply(c, response);
    response.assign(response.size(), 0);
    assert(c.progress[0].acknowledged && c.progress[0].execution == ActionExecution::ACKNOWLEDGED && c.uncertain);
    const auto readback = work(c);
    assert(!readback.write && readback.length == sizeof(read) && !std::memcmp(readback.bytes, read, sizeof(read)));
    assert(!std::memcmp(c.observations[0].raw, write, sizeof(write)));
    auto stored = reply({3000}); supply(c, stored); stored.assign(stored.size(), 0);
    assert(c.state == ReadState::SUCCEEDED && c.completedSteps == 2 && c.effects == 1 && !c.uncertain);
    assert(c.prerequisites.previous.raw[0] == 0xFFFF && c.request.positionStartSpeed == 3000);
    assert(c.progress[0].readbackKnown && c.progress[0].readback == 3000 && !c.progress[0].activeKnown);
    assert(c.observations[0].write && !c.observations[1].write && c.observations[1].reg == 0x0020);
    assert(c.prerequisites.previous.provenance[0].raw[3] == 0xFF && c.observations[1].raw[3] == 0x0B);
    assert(work(c).kind == Ess::ActionWork::DONE && !work(c).length);
    Ess::DriverObservation output = positionStartSnapshot(123); const Saved<Ess::DriverObservation> saved(output);
    assert(!Ess::getDriver(c, output)); saved.check(output); // An update is not a producing prior read.

    for (unsigned fault = 0; fault < 8; ++fault) {
        auto failed = positionStartUpdate(); auto bytes = std::vector<uint8_t>(write, write + 6);
        switch (fault) {
        case 0: bytes[0] = 2; crc(bytes); break;
        case 1: bytes[1] = 16; crc(bytes); break;
        case 2: bytes[3] = 0x21; crc(bytes); break;
        case 3: bytes[5]++; crc(bytes); break;
        case 4: crc(bytes); bytes.pop_back(); break;
        case 5: crc(bytes); bytes.push_back(0); break;
        case 6: crc(bytes); bytes.back() ^= 1; break;
        default: bytes = {1, 0x86, 3}; crc(bytes); break;
        }
        supply(failed, bytes);
        assert(failed.state == ReadState::FAILED && failed.outcome == Ess::DriverOutcome::REPLY_ERROR);
        assert(failed.effects == 1 && !failed.progress[0].acknowledged && !failed.progress[0].readbackKnown);
        assert(failed.progress[0].execution == (fault == 7 ? ActionExecution::REJECTED : ActionExecution::UNKNOWN));
        assert(failed.uncertain == (fault != 7) && work(failed).kind == Ess::ActionWork::DONE);
    }
    for (unsigned fault = 0; fault < 8; ++fault) {
        auto failed = positionStartUpdate(); supply(failed, std::vector<uint8_t>(write, write + sizeof(write)));
        auto bytes = std::vector<uint8_t>{1, 3, 2, 0x0B, 0xB8};
        switch (fault) {
        case 0: bytes[0] = 2; crc(bytes); break;
        case 1: bytes[1] = 4; crc(bytes); break;
        case 2: bytes[2] = 4; crc(bytes); break;
        case 3: crc(bytes); bytes.pop_back(); break;
        case 4: crc(bytes); bytes.push_back(0); break;
        case 5: crc(bytes); bytes.back() ^= 1; break;
        case 6: bytes = {1, 0x83, 3}; crc(bytes); break;
        default: bytes = reply({2999}); break;
        }
        supply(failed, bytes);
        assert(failed.state == ReadState::FAILED && failed.effects == 1 && failed.uncertain);
        assert(failed.progress[0].acknowledged && failed.progress[0].readbackKnown == (fault == 7));
        assert(failed.outcome == (fault == 7 ? Ess::DriverOutcome::READBACK_MISMATCH : Ess::DriverOutcome::REPLY_ERROR));
        assert(work(failed).kind == Ess::ActionWork::DONE);
    }
    auto unconfirmed = positionStartUpdate(); auto e = frame(unconfirmed, write, sizeof(write), 220);
    e.responseConfirmed = false;
    assert(Ess::advanceDriver(unconfirmed, e, 222));
    assert(unconfirmed.outcome == Ess::DriverOutcome::UNCONFIRMED_RESPONSE && unconfirmed.uncertain && unconfirmed.effects == 1);
    assert(work(unconfirmed).kind == Ess::ActionWork::DONE);
}
void testPositionStartSpeedCancellationAndTiming() {
    const uint8_t write[] = {1, 6, 0, 0x20, 0x0B, 0xB8, 0x8F, 0x42};
    auto c = positionStartUpdate(); const Saved<Ess::DriverContext> retained(c);
    for (unsigned field = 0; field < 5; ++field) {
        auto e = frame(c, write, sizeof(write), 220);
        if (field == 0) e.transport.target.id++;
        else if (field == 1) e.transport.target.address++;
        else if (field == 2) e.transport.target.generation++;
        else if (field == 3) e.transport.operationId++;
        else e.transport.step++;
        assert(!Ess::advanceDriver(c, e, 222)); retained.check(c);
    }
    for (ReadEventKind kind : {ReadEventKind::CANCEL, ReadEventKind::TRANSPORT_FAILURE, ReadEventKind::DEADLINE}) {
        for (std::size_t accepted : {std::size_t(0), std::size_t(3), std::size_t(8)}) {
            auto failed = positionStartUpdate(300); auto e = local(failed, kind);
            e.transport.txAccepted = accepted; e.txComplete = accepted == 8;
            e.transport.executionUnknown = accepted != 0;
            assert(Ess::advanceDriver(failed, e, kind == ReadEventKind::DEADLINE ? 300 : 220));
            assert(failed.state == ReadState::FAILED && failed.effects == (accepted ? 1u : 0u) && failed.uncertain == (accepted != 0));
            assert(!failed.progress[0].acknowledged && !failed.progress[0].readbackKnown);
            const Saved<Ess::DriverContext> terminal(failed);
            assert(!Ess::advanceDriver(failed, frame(failed, write, sizeof(write), 220), 301)); terminal.check(failed);
            assert(work(failed).kind == Ess::ActionWork::DONE);
        }
    }
    auto cancelledReadback = positionStartUpdate(); supply(cancelledReadback, std::vector<uint8_t>(write, write + sizeof(write)));
    assert(Ess::advanceDriver(cancelledReadback, local(cancelledReadback, ReadEventKind::CANCEL), 230));
    assert(cancelledReadback.effects == 1 && cancelledReadback.uncertain && cancelledReadback.progress[0].acknowledged);

    auto p = positionStartPrerequisites(); p.maxAgeUs = 200; p.triggerEarliestUs = p.triggerLatestUs = 90;
    assert(Ess::prepareDriverSettings(c, target(), 12, positionStartRequest(), p, 200, 9000));
    assert(Ess::driverWriteDeadline(c) == 290 && c.deadlineUs == 9000);
    Ess::PreparedDriver rejected; rejected.reg = 123; const Saved<Ess::PreparedDriver> pending(rejected);
    assert(!Ess::nextDriver(c, 290, rejected)); pending.check(rejected);
    auto e = frame(c, write, sizeof(write), 290);
    assert(Ess::advanceDriver(c, e, 292) && c.outcome == Ess::DriverOutcome::DEADLINE && c.uncertain);
    assert(c.deadlineUs == 9000 && !c.progress[0].acknowledged);

    auto ontime = positionStartUpdate(300); supply(ontime, std::vector<uint8_t>(write, write + sizeof(write)));
    auto bytes = reply({3000}); e = frame(ontime, bytes.data(), bytes.size(), 299);
    assert(Ess::advanceDriver(ontime, e, 9000) && ontime.state == ReadState::SUCCEEDED && !ontime.uncertain);
    assert(ontime.deadlineUs == 300 && ontime.observations[1].latestUs == 300 && ontime.observations[1].deliveredUs == 9000);
    auto late = positionStartUpdate(300); supply(late, std::vector<uint8_t>(write, write + sizeof(write)));
    e = frame(late, bytes.data(), bytes.size(), 300);
    assert(Ess::advanceDriver(late, e, 9000) && late.outcome == Ess::DriverOutcome::DEADLINE && late.uncertain);
    auto delayedAck = positionStartUpdate(300); e = frame(delayedAck, write, sizeof(write), 220);
    assert(Ess::advanceDriver(delayedAck, e, 9000) && delayedAck.outcome == Ess::DriverOutcome::DEADLINE);
    assert(delayedAck.progress[0].acknowledged && delayedAck.uncertain && work(delayedAck).kind == Ess::ActionWork::DONE);
}
Ess::DriverRequest auxiliaryRequest(uint16_t raw = 7) {
    Ess::DriverRequest r; r.configurationGeneration = 17;
    assert(Ess::prepareHomingAuxiliary(r, static_cast<Ess::HomingAuxiliary>(raw))); return r;
}
Ess::DriverObservation auxiliarySnapshot(uint16_t raw = 0xFFFF) {
    Ess::DriverContext c;
    assert(Ess::prepareDriverRead(c, target(), 11, 17, 100, 1000, Ess::DriverGroup::HOMING_AUXILIARY));
    const uint8_t request[] = {1, 3, 0, 0x30, 0, 1, 0x84, 0x05};
    const auto pending = work(c);
    assert(!pending.write && pending.reg == 0x0030 && pending.count == 1 && pending.length == sizeof(request));
    assert(!std::memcmp(pending.bytes, request, sizeof(request)));
    auto response = reply({raw}); supply(c, response); response.assign(response.size(), 0);
    Ess::DriverObservation observed; assert(Ess::getDriver(c, observed));
    assert(observed.raw[0] == raw && observed.knownFields == (raw <= 11 ? 1u : 0u));
    assert(observed.provenance[0].reg == 0x0030 && observed.provenance[0].count == 1 && observed.provenance[0].length == 7);
    assert(c.completedSteps == 1 && work(c).kind == Ess::ActionWork::DONE);
    return observed;
}
Ess::DriverPrerequisites auxiliaryPrerequisites() {
    auto p = positionStartPrerequisites(); p.previous = auxiliarySnapshot(); return p;
}
Ess::DriverContext auxiliaryUpdate(uint16_t raw = 7, uint64_t deadline = 9000) {
    Ess::DriverContext c;
    assert(Ess::prepareDriverSettings(c, target(), 12, auxiliaryRequest(raw), auxiliaryPrerequisites(), 200, deadline));
    return c;
}
void testHomingAuxiliaryChoicesAndAdmission() {
    // Original ESS-RS physical p72: these are complete scalar choices, not flags.
    const Ess::HomingAuxiliary choices[] = {
        Ess::HomingAuxiliary::MOVE_OFFSET_SET_OFFSET, Ess::HomingAuxiliary::MOVE_OFFSET_SET_ZERO,
        Ess::HomingAuxiliary::MOVE_OFFSET_SET_NEGATIVE_OFFSET, Ess::HomingAuxiliary::MOVE_OFFSET_KEEP_ACTUAL,
        Ess::HomingAuxiliary::MOVE_OFFSET_ADD_OFFSET, Ess::HomingAuxiliary::MOVE_OFFSET_SUBTRACT_OFFSET,
        Ess::HomingAuxiliary::KEEP_POSITION_SET_OFFSET, Ess::HomingAuxiliary::KEEP_POSITION_SET_ZERO,
        Ess::HomingAuxiliary::KEEP_POSITION_SET_NEGATIVE_OFFSET, Ess::HomingAuxiliary::KEEP_POSITION_KEEP_ACTUAL,
        Ess::HomingAuxiliary::KEEP_POSITION_ADD_OFFSET, Ess::HomingAuxiliary::KEEP_POSITION_SUBTRACT_OFFSET
    };
    Ess::DriverRequest r; r.configurationGeneration = 17;
    for (unsigned i = 0; i < 12; ++i) {
        assert(static_cast<unsigned>(choices[i]) == i && Ess::prepareHomingAuxiliary(r, choices[i]));
        assert(r.group == Ess::DriverGroup::HOMING_AUXILIARY && r.fields == 1 && r.homingAuxiliary == choices[i] && r.configurationGeneration == 17);
        auto c = auxiliaryUpdate(static_cast<uint16_t>(i)); const auto pending = work(c);
        auto expected = std::vector<uint8_t>{1, 6, 0, 0x30, 0, static_cast<uint8_t>(i)}; crc(expected);
        assert(pending.length == 8 && pending.write && pending.count == 1 && pending.reg == 0x0030);
        assert(!std::memcmp(pending.bytes, expected.data(), expected.size()));
        supply(c, expected); assert(c.progress[0].previous == 0xFFFF && c.progress[0].requested == i && c.progress[0].acknowledged);
        const auto readback = work(c); assert(!readback.write && readback.reg == 0x0030 && readback.count == 1);
        auto returned = reply({static_cast<uint16_t>(i)}); supply(c, returned);
        expected.assign(expected.size(), 0); returned.assign(returned.size(), 0);
        assert(c.state == ReadState::SUCCEEDED && c.effects == 1 && !c.uncertain && c.completedSteps == 2);
        assert(c.progress[0].readbackKnown && c.progress[0].readback == i && !c.progress[0].activeKnown);
        assert(c.prerequisites.previous.raw[0] == 0xFFFF && c.prerequisites.previous.provenance[0].raw[3] == 0xFF);
        assert(c.observations[0].raw[3] == 0x30 && c.observations[1].raw[4] == i && work(c).kind == Ess::ActionWork::DONE);
    }
    const Saved<Ess::DriverRequest> selected(r);
    for (uint16_t invalid : {uint16_t(12), uint16_t(0xFFFF)}) {
        assert(!Ess::prepareHomingAuxiliary(r, static_cast<Ess::HomingAuxiliary>(invalid))); selected.check(r);
        auto bad = auxiliaryRequest(); bad.homingAuxiliary = static_cast<Ess::HomingAuxiliary>(invalid);
        checkUnchangedReject(bad, auxiliaryPrerequisites());
    }
    auto mixed = positionStartRequest(); const Saved<Ess::DriverRequest> mixedSaved(mixed);
    assert(!Ess::prepareHomingAuxiliary(mixed, choices[7])); mixedSaved.check(mixed);
    r.fields = 3; const Saved<Ess::DriverRequest> extra(r);
    assert(!Ess::prepareHomingAuxiliary(r, choices[7])); extra.check(r);
    r = auxiliaryRequest();
    assert(static_cast<uint32_t>(Ess::driverFieldAt(0, r.group)) == 1 && static_cast<uint32_t>(Ess::driverFieldAt(1, r.group)) == 0);
    for (uint16_t raw : {uint16_t(0), uint16_t(11), uint16_t(12), uint16_t(0xFFFF)}) auxiliarySnapshot(raw);
    auto p = auxiliaryPrerequisites(); p.allowEchoReadback = true; checkUnchangedReject(r, p);
    p = auxiliaryPrerequisites(); p.inputsPermit = false; checkUnchangedReject(r, p);
    p = auxiliaryPrerequisites(); p.stationaryQualified = false; checkUnchangedReject(r, p);
    p = auxiliaryPrerequisites(); p.rawMotion = 4; checkUnchangedReject(r, p);
    p = auxiliaryPrerequisites(); p.externalTriggerInhibitedQualified = false; checkUnchangedReject(r, p);
    p = auxiliaryPrerequisites(); p.triggerConfigurationGeneration++; checkUnchangedReject(r, p);
    p = auxiliaryPrerequisites(); p.triggerTarget.generation++; checkUnchangedReject(r, p);
    p = auxiliaryPrerequisites(); p.previous.target.address++; checkUnchangedReject(r, p);
    p = auxiliaryPrerequisites(); p.previous.configurationGeneration++; checkUnchangedReject(r, p);
    p = auxiliaryPrerequisites(); p.previous.raw[0]--; checkUnchangedReject(r, p);
    Ess::DriverContext c; c.operationId = 987; const Saved<Ess::DriverContext> unchanged(c);
    auto badTarget = target(); badTarget.generation = 0;
    assert(!Ess::prepareDriverSettings(c, badTarget, 12, r, auxiliaryPrerequisites(), 200, 9000)); unchanged.check(c);
}
void testHomingAuxiliaryErrorsAndTiming() {
    const uint8_t write[] = {1, 6, 0, 0x30, 0, 7, 0xC8, 0x07};
    for (unsigned fault = 0; fault < 9; ++fault) {
        auto c = auxiliaryUpdate(); auto response = std::vector<uint8_t>(write, write + 6);
        switch (fault) {
        case 0: response[0] = 2; crc(response); break;
        case 1: response[1] = 16; crc(response); break;
        case 2: response[3] = 0x31; crc(response); break;
        case 3: response[5] = 6; crc(response); break;
        case 4: crc(response); response.pop_back(); break;
        case 5: crc(response); response.push_back(0); break;
        case 6: crc(response); response.back() ^= 1; break;
        case 7: response = {1, 0x86, 3}; crc(response); break;
        default: crc(response); break;
        }
        auto e = frame(c, response.data(), response.size(), 220); if (fault == 8) e.responseConfirmed = false;
        assert(Ess::advanceDriver(c, e, 222));
        assert(c.state == ReadState::FAILED && c.effects == 1 && !c.progress[0].acknowledged && !c.progress[0].readbackKnown);
        assert(c.progress[0].execution == (fault == 7 ? ActionExecution::REJECTED : ActionExecution::UNKNOWN));
        assert(c.uncertain == (fault != 7) && work(c).kind == Ess::ActionWork::DONE);
    }
    for (unsigned fault = 0; fault < 8; ++fault) {
        auto c = auxiliaryUpdate(); supply(c, std::vector<uint8_t>(write, write + sizeof(write)));
        auto response = std::vector<uint8_t>{1, 3, 2, 0, 7};
        switch (fault) {
        case 0: response[0] = 2; crc(response); break;
        case 1: response[1] = 4; crc(response); break;
        case 2: response[2] = 4; crc(response); break;
        case 3: crc(response); response.pop_back(); break;
        case 4: crc(response); response.push_back(0); break;
        case 5: crc(response); response.back() ^= 1; break;
        case 6: response = {1, 0x83, 3}; crc(response); break;
        default: response = reply({6}); break;
        }
        supply(c, response);
        assert(c.state == ReadState::FAILED && c.effects == 1 && c.uncertain && c.progress[0].acknowledged);
        assert(c.progress[0].readbackKnown == (fault == 7));
        assert(c.outcome == (fault == 7 ? Ess::DriverOutcome::READBACK_MISMATCH : Ess::DriverOutcome::REPLY_ERROR));
        assert(work(c).kind == Ess::ActionWork::DONE);
    }
    auto c = auxiliaryUpdate(); const Saved<Ess::DriverContext> retained(c);
    for (unsigned changed = 0; changed < 5; ++changed) {
        auto e = frame(c, write, sizeof(write), 220);
        if (changed == 0) e.transport.target.id++; else if (changed == 1) e.transport.target.address++;
        else if (changed == 2) e.transport.target.generation++; else if (changed == 3) e.transport.operationId++;
        else e.transport.step++;
        assert(!Ess::advanceDriver(c, e, 222)); retained.check(c);
    }
    for (ReadEventKind kind : {ReadEventKind::CANCEL, ReadEventKind::TRANSPORT_FAILURE, ReadEventKind::DEADLINE})
        for (std::size_t accepted : {std::size_t(0), std::size_t(3), std::size_t(8)}) {
            auto failed = auxiliaryUpdate(7, 300); auto e = local(failed, kind);
            e.transport.txAccepted = accepted; e.txComplete = accepted == 8; e.transport.executionUnknown = accepted != 0;
            assert(Ess::advanceDriver(failed, e, kind == ReadEventKind::DEADLINE ? 300 : 220));
            assert(failed.state == ReadState::FAILED && failed.effects == (accepted ? 1u : 0u) && failed.uncertain == (accepted != 0));
            const Saved<Ess::DriverContext> finished(failed);
            assert(!Ess::advanceDriver(failed, frame(failed, write, sizeof(write), 220), 301)); finished.check(failed);
            assert(work(failed).kind == Ess::ActionWork::DONE);
        }
    auto p = auxiliaryPrerequisites(); p.maxAgeUs = 200; p.triggerEarliestUs = p.triggerLatestUs = 90;
    assert(Ess::prepareDriverSettings(c, target(), 12, auxiliaryRequest(), p, 200, 9000));
    assert(Ess::driverWriteDeadline(c) == 290 && c.deadlineUs == 9000);
    Ess::PreparedDriver rejected; rejected.reg = 987; const Saved<Ess::PreparedDriver> pending(rejected);
    assert(!Ess::nextDriver(c, 290, rejected)); pending.check(rejected);
    auto ontime = auxiliaryUpdate(7, 300); supply(ontime, std::vector<uint8_t>(write, write + sizeof(write)));
    const auto response = reply({7}); auto e = frame(ontime, response.data(), response.size(), 299);
    assert(Ess::advanceDriver(ontime, e, 9000) && ontime.state == ReadState::SUCCEEDED && ontime.deadlineUs == 300);
    auto late = auxiliaryUpdate(7, 300); supply(late, std::vector<uint8_t>(write, write + sizeof(write)));
    e = frame(late, response.data(), response.size(), 300);
    assert(Ess::advanceDriver(late, e, 9000) && late.outcome == Ess::DriverOutcome::DEADLINE && late.uncertain);
    auto delayed = auxiliaryUpdate(7, 300); e = frame(delayed, write, sizeof(write), 220);
    assert(Ess::advanceDriver(delayed, e, 9000) && delayed.outcome == Ess::DriverOutcome::DEADLINE && delayed.uncertain);
}
} // namespace
static void testOptionalAgePolicy() {
    auto p = prerequisites(); p.maxAgeUs = 0;
    Ess::DriverContext c;
    assert(Ess::prepareDriverSettings(c, target(), 12, request(), p, 100000, 110000));
    assert(work(c).deadlineUs == 110000);
    const Saved<Ess::DriverContext> saved(c);
    p.maxAgeUs = 10000;
    assert(!Ess::prepareDriverSettings(c, target(), 12, request(), p, 100000, 110000)); saved.check(c);
    p.maxAgeUs = 0; p.previous.target.generation++;
    assert(!Ess::prepareDriverSettings(c, target(), 12, request(), p, 100000, 110000)); saved.check(c);
    p = prerequisites(); p.maxAgeUs = 0; p.stationaryQualified = false;
    assert(!Ess::prepareDriverSettings(c, target(), 12, request(), p, 100000, 110000)); saved.check(c);
    p.stationaryQualified = true; p.stationaryLatestUs = 100001;
    assert(!Ess::prepareDriverSettings(c, target(), 12, request(), p, 100000, 110000)); saved.check(c);
    assert(Ess::advanceDriver(c, local(c, ReadEventKind::DEADLINE), 110000));
    assert(c.state == ReadState::FAILED && c.outcome == Ess::DriverOutcome::DEADLINE);
    assert(work(c).kind == Ess::ActionWork::DONE && !work(c).length);
}
int main() {
    testOptionalAgePolicy();
    testAdmissionPrecedenceAndAliasing();
    testReadAndPairOrder(); testWholeCandidateAndPrerequisites(); testLimitDependencies();
    testProgressNoRollbackAndCopiedStorage(); testFaultsCancellationAndExpiry();
    testUnconfirmedReadEvidence();
    testPositionStartSpeedSelectionAndAdmission();
    testPositionStartSpeedEvidenceAndErrors();
    testPositionStartSpeedCancellationAndTiming();
    testHomingAuxiliaryChoicesAndAdmission(); testHomingAuxiliaryErrorsAndTiming();
}
