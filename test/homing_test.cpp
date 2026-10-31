// SPDX-License-Identifier: MIT
#include <MotorControlRS/profiles/ess_rs/Homing.h>
#include "ActionEvidenceChecks.h"
#include <cassert>
#include <cstring>
#include <limits>
#include <vector>
using namespace MotorControlRS;
namespace Ess = MotorControlRS::ESS_RS;
namespace {
template<class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& c) { std::memcpy(bytes, &c, sizeof(T)); }
    void check(const T& c) const { assert(std::memcmp(bytes, &c, sizeof(T)) == 0); }
};
void crc(std::vector<uint8_t>& b) {
    uint16_t value = 0xFFFF;
    for (auto byte : b) {
        value ^= byte;
        for (unsigned n = 0; n < 8; ++n) value = (value & 1) ? (value >> 1) ^ 0xA001 : value >> 1;
    }
    b.push_back(static_cast<uint8_t>(value)); b.push_back(static_cast<uint8_t>(value >> 8));
}
AxisConfig axis() { AxisConfig a; a.target.id = 3; a.target.address = 1; a.target.generation = 4; a.generation = 7; return a; }
Ess::HomeRequest request(Ess::HomingMethod method = Ess::HomingMethod::METHOD_35) {
    Ess::HomeRequest r; r.configurationGeneration = 7; r.method = method; return r;
}
Ess::HomePrerequisites prerequisites(Ess::HomingMethod method = Ess::HomingMethod::METHOD_35) {
    Ess::HomePrerequisites p; p.target = axis().target; p.configurationGeneration = 7;
    p.qualifiedMethod = method; p.qualifiedSearchSpeed = 60;
    p.qualifiedReturnSpeed = 30; p.qualifiedRampTime = 100;
    p.methodQualified = p.nativeRatesQualified = p.nativeRampQualified = p.zeroOffsetQualified = true;
    p.auxiliaryQualified = p.inputsQualified = p.indexQualified = p.readinessQualified = true;
    p.referenceSemanticsQualified = true; p.observedUs = 80; p.maximumAgeUs = 1000; p.rawMotion = 1; return p;
}
ActionOptions options() { ActionOptions o; o.pollIntervalUs = 100; o.maxPolls = 6; return o; }
Ess::HomeContext home(Ess::HomingMethod method = Ess::HomingMethod::METHOD_35, uint16_t baseline = 1, uint64_t deadline = 10000) {
    Ess::HomeContext c; auto p = prerequisites(method); p.rawMotion = baseline;
    assert(Ess::prepareHome(c, axis(), 12, request(method), p, 100, deadline, options())); return c;
}
ActionEvent local(const Ess::HomeContext& c, ReadEventKind kind) {
    ActionEvent e; e.transport.target = c.target; e.transport.operationId = c.operationId;
    e.transport.step = c.step; e.transport.kind = kind; return e;
}
ActionEvent frame(const Ess::HomeContext& c, const std::vector<uint8_t>& b, uint64_t at) {
    auto e = local(c, ReadEventKind::FRAME); e.transport.frame = b.data(); e.transport.length = b.size();
    e.transport.txAccepted = c.step == 0 ? 21 : 8;
    e.transport.qualified = e.responseConfirmed = e.txComplete = true;
    e.transport.earliestUs = at; e.transport.latestUs = at + 10; return e;
}
std::vector<uint8_t> reply(const Ess::HomeContext& c, uint16_t flags = 1, uint16_t alarm = 0) {
    std::vector<uint8_t> b;
    if (c.step == 0) b = {1, 16, 0, 0x31, 0, 6};
    else if (c.step == 1) b = {1, 6, 0, 0x27, 0, 16};
    else if (c.phase == Ess::HomePhase::ZERO_CHECK) b = {1, 3, 4, 0, 0, 0, 0};
    else b = {1, 3, 4, static_cast<uint8_t>(alarm >> 8), static_cast<uint8_t>(alarm),
        static_cast<uint8_t>(flags >> 8), static_cast<uint8_t>(flags)};
    if (c.step >= 2 && c.phase != Ess::HomePhase::ZERO_CHECK && c.prerequisites.observeInputs) {
        b[2] = 6; b.push_back(0); b.push_back(0);
    }
    crc(b); return b;
}
void consume(Ess::HomeContext& c, uint16_t flags = 1, uint16_t alarm = 0) {
    const auto b = reply(c, flags, alarm); const auto at = c.eligibleUs + 20;
    assert(Ess::advanceHome(c, frame(c, b, at), at + 20));
}
void trigger(Ess::HomeContext& c) { consume(c); consume(c); assert(c.step == 2 && c.phase == Ess::HomePhase::OBSERVING); }
void done(const Ess::HomeContext& c) {
    Ess::PreparedHome p; assert(Ess::nextHome(c, c.servicedUs, p));
    assert(p.kind == Ess::ActionWork::DONE && p.length == 0);
}
void testDescriptorsAndNoTraffic() {
    const int values[] = {-4,-3,-2,-1,1,2,3,4,5,6,7,8,9,10,11,12,13,14,17,18,19,20,21,22,23,24,25,26,27,28,29,30,33,34,35};
    const uint8_t inputs[] = {0,0,0,0,4,2,1,1,1,1,3,3,3,3,5,5,5,5,4,2,1,1,1,1,3,3,3,3,5,5,5,5,0,0,0};
    auto c = home();
    for (uint8_t i = 0; i < Ess::HOME_METHOD_COUNT; ++i) {
        const auto* d = Ess::homeMethodAt(i); assert(d && static_cast<int>(d->method) == values[i]);
        assert(Ess::homeMethod(d->method) == d && d->requiredInputs == inputs[i] && d->reason && *d->reason);
        assert(d->requiresIndex == ((values[i] >= 1 && values[i] <= 14) || values[i] == 33 || values[i] == 34));
        assert(d->moves == (values[i] != 35));
        if (values[i] >= 33 || values[i] == 1 || values[i] == 2 || values[i] == 17)
            assert(d->support == Ess::HomeSupport::IMPLEMENTED);
        else {
            const Saved<Ess::HomeContext> saved(c);
            const auto status = Ess::prepareHome(c, axis(), 12, request(d->method), prerequisites(), 100, 10000);
            assert(!status && status.code == Err::UNSUPPORTED); saved.check(c);
            assert(d->support == (values[i] < 0 || values[i] == 18 ? Ess::HomeSupport::UNRESOLVED : Ess::HomeSupport::UNIMPLEMENTED));
        }
    }
    assert(!Ess::homeMethodAt(35));
    for (int value : {0,15,16,31,32,36,-5}) assert(!Ess::homeMethod(static_cast<Ess::HomingMethod>(value)));
}
void testPreparationAndBounds() {
    auto c = home();
    for (unsigned fault = 0; fault < 30; ++fault) {
        auto a = axis(); auto r = request(); auto p = prerequisites(); auto o = options();
        uint32_t id = 12; uint64_t deadline = 10000;
        switch (fault) {
        case 0: a.target.address = 248; break;
        case 1: a.target.id = 0; break;
        case 2: a.target.generation = 0; break;
        case 3: id = 0; break;
        case 4: deadline = 100; break;
        case 5: o.maxPolls = 0; break;
        case 6: o.maxPolls = 65; break;
        case 7: o.pollIntervalUs = 0; break;
        case 8: ++p.target.id; break;
        case 9: ++p.target.address; break;
        case 10: ++p.target.generation; break;
        case 11: ++p.configurationGeneration; break;
        case 12: ++r.configurationGeneration; break;
        case 13: r.offset = 1; break;
        case 14: p.zeroOffsetQualified = false; break;
        case 15: p.auxiliaryQualified = false; break;
        case 16: p.auxiliary = Ess::HomingAuxiliary::MOVE_OFFSET_SET_ZERO; break;
        case 17: r.searchSpeed = 4; break;
        case 18: r.searchSpeed = 3001; break;
        case 19: r.returnSpeed = 4; break;
        case 20: r.returnSpeed = 301; break;
        case 21: r.rampTime = 29; break;
        case 22: r.rampTime = 2001; break;
        case 23: p.nativeRatesQualified = false; break;
        case 24: p.nativeRampQualified = false; break;
        case 25: p.inputsQualified = false; break;
        case 26: p.methodQualified = false; break;
        case 27: p.readinessQualified = false; break;
        case 28: p.observedUs = 101; break;
        case 29: p.maximumAgeUs = 20; break;
        }
        const Saved<Ess::HomeContext> saved(c);
        assert(!Ess::prepareHome(c, a, id, r, p, 100, deadline, o)); saved.check(c);
    }
    for (int64_t offset : {std::numeric_limits<int64_t>::min(), int64_t(-0xFFFFFFF), int64_t(-1), int64_t(1), int64_t(0xFFFFFFF), std::numeric_limits<int64_t>::max()}) {
        auto r = request(); r.offset = offset; const Saved<Ess::HomeContext> saved(c);
        const auto s = Ess::prepareHome(c, axis(), 12, r, prerequisites(), 100, 10000);
        assert(!s && s.detail == static_cast<int32_t>(Ess::HomeError::UNRESOLVED_OFFSET)); saved.check(c);
    }
    for (uint16_t bad : {4,8,16,32,64}) {
        auto p = prerequisites(); p.rawMotion = bad; const Saved<Ess::HomeContext> saved(c);
        assert(!Ess::prepareHome(c, axis(), 12, request(), p, 100, 10000)); saved.check(c);
    }
    auto p = prerequisites(); p.indexQualified = false;
    for (auto method : {Ess::HomingMethod::METHOD_33, Ess::HomingMethod::METHOD_34}) {
        p.qualifiedMethod = method;
        const Saved<Ess::HomeContext> saved(c);
        const auto missingIndex = Ess::prepareHome(c, axis(), 12, request(method), p, 100, 10000);
        assert(!missingIndex && missingIndex.detail == static_cast<int32_t>(Ess::HomeError::INDEX_REQUIRED)); saved.check(c);
    }
    // Current mechanical origin needs no Z or external input fixture.
    p.qualifiedMethod = Ess::HomingMethod::METHOD_35;
    assert(Ess::prepareHome(c, axis(), 12, request(), p, 100, 10000));
    auto r = request(); r.searchSpeed = 3000; r.returnSpeed = 300; r.rampTime = 2000;
    p = prerequisites(); p.qualifiedSearchSpeed = r.searchSpeed;
    p.qualifiedReturnSpeed = r.returnSpeed; p.qualifiedRampTime = r.rampTime;
    assert(Ess::prepareHome(c, axis(), 12, r, p, 100, 10000));
    r.searchSpeed = r.returnSpeed = 5; r.rampTime = 30;
    p.qualifiedSearchSpeed = r.searchSpeed; p.qualifiedReturnSpeed = r.returnSpeed; p.qualifiedRampTime = r.rampTime;
    assert(Ess::prepareHome(c, axis(), 12, r, p, 100, 10000));
}
void testExactQualificationCannotBeReused() {
    auto c = home(); const auto p = prerequisites();
    for (unsigned change = 0; change < 5; ++change) {
        auto r = request();
        switch (change) {
        case 0: r.method = Ess::HomingMethod::METHOD_33; break;
        case 1: r.method = Ess::HomingMethod::METHOD_34; break;
        case 2: r.searchSpeed = 3000; break;
        case 3: r.returnSpeed = 300; break;
        case 4: r.rampTime = 2000; break;
        }
        const Saved<Ess::HomeContext> saved(c);
        const auto status = Ess::prepareHome(c, axis(), 12, r, p, 100, 10000);
        assert(!status && status.detail == static_cast<int32_t>(Ess::HomeError::QUALIFICATION_MISMATCH)); saved.check(c);
    }
    // Even all true flags do not give default-zero word witnesses a meaning.
    auto missing = p; missing.qualifiedSearchSpeed = missing.qualifiedReturnSpeed = missing.qualifiedRampTime = 0;
    const Saved<Ess::HomeContext> saved(c);
    const auto status = Ess::prepareHome(c, axis(), 12, request(), missing, 100, 10000);
    assert(!status && status.detail == static_cast<int32_t>(Ess::HomeError::QUALIFICATION_MISMATCH)); saved.check(c);
    // Independent qualification can admit the exact reviewed request afterward.
    auto qualified = prerequisites(Ess::HomingMethod::METHOD_33);
    qualified.qualifiedSearchSpeed = 3000; qualified.qualifiedReturnSpeed = 300; qualified.qualifiedRampTime = 2000;
    auto r = request(Ess::HomingMethod::METHOD_33); r.searchSpeed = 3000; r.returnSpeed = 300; r.rampTime = 2000;
    assert(Ess::prepareHome(c, axis(), 12, r, qualified, 100, 10000));
    assert(c.prerequisites.qualifiedMethod == r.method && c.words[1] == qualified.qualifiedSearchSpeed &&
        c.words[2] == qualified.qualifiedReturnSpeed && c.words[3] == qualified.qualifiedRampTime);
}
void testExactWireAndReference() {
    auto c = home(); Ess::PreparedHome work;
    assert(Ess::nextHome(c, 100, work));
    const uint8_t expected[] = {1,16,0,0x31,0,6,12,0,35,0,60,0,30,0,100,0,0,0,0};
    assert(work.length == 21 && work.reg == 0x31 && work.count == 6 && work.write);
    assert(std::memcmp(work.bytes, expected, sizeof(expected)) == 0);
    Ess::PreparedHome repeated; assert(Ess::nextHome(c, 100, repeated));
    assert(repeated.step == work.step && std::memcmp(repeated.bytes, work.bytes, work.length) == 0);
    Ess::HomeContext common;
    assert(MotorControlRS::prepareHome(common, axis(), 12, request(), prerequisites(), 100, 10000, options()));
    assert(Ess::nextHome(common, 100, repeated) && std::memcmp(repeated.bytes, work.bytes, work.length) == 0);
    consume(c); assert(c.stagingApplied && c.setupExecution == ActionExecution::ACKNOWLEDGED);
    assert(Ess::nextHome(c, c.servicedUs, work) && work.reg == 0x27 && work.value == 16);
    consume(c); assert(c.execution == ActionExecution::ACKNOWLEDGED && c.completion == ActionCompletion::NOT_OBSERVED);
    AxisReference ref; ref.nativePosition = 123; const Saved<AxisReference> saved(ref);
    assert(!Ess::getHomeReference(c, c.servicedUs, 1000, ref)); saved.check(ref);
    assert(Ess::nextHome(c, c.servicedUs, work) && work.kind == Ess::ActionWork::WAIT && !work.length);
    const uint64_t completeReadEligible = c.eligibleUs;
    consume(c, 3); assert(c.phase == Ess::HomePhase::ZERO_CHECK && !c.runningObserved);
    assert(c.rawMotion == 3 && c.completionObservedUs == completeReadEligible);
    assert(Ess::nextHome(c, c.eligibleUs, work) && work.reg == 0xA && work.count == 2);
    consume(c); assert(c.state == ActionState::SUCCEEDED && c.completion == ActionCompletion::OBSERVED); done(c);
    assert(Ess::getHomeReference(c, c.servicedUs, 1000, ref));
    assert(ref.nativePosition == 0 && ref.nativeKnown && ref.stationary && ref.idle && ref.observedUs == completeReadEligible);
    assert(ref.configurationGeneration == 7 && ref.target.id == 3 && ref.source == ScaleSource::QUALIFIED);
    assert(!axis().originKnown); // Host origin remains separate.
    assert(Ess::getHomeReference(c, completeReadEligible + 100000, 0, ref));
    assert(ref.maximumAgeUs == 0 && ref.observedUs == completeReadEligible);
    assert(!Ess::getHomeReference(c, c.servicedUs - 1, 0, ref));
    const Saved<AxisReference> retained(ref);
    assert(!Ess::getHomeReference(c, completeReadEligible + 1000, 1000, ref)); retained.check(ref);
    c.prerequisites.referenceSemanticsQualified = false;
    assert(!Ess::getHomeReference(c, c.servicedUs, 1000, ref)); retained.check(ref);
}
void testCorrelatedTransitions() {
    auto c = home(Ess::HomingMethod::METHOD_35, 3); trigger(c);
    consume(c, 3); assert(c.state == ActionState::ACTIVE && !c.homedLowObserved);
    const auto firstLowEligibility = c.eligibleUs;
    consume(c, 1); const auto firstLow = c.lowEvidence;
    consume(c, 1); assert(c.lowEvidence.step == firstLow.step && c.lowEvidence.latestUs == firstLow.latestUs);
    assert(c.lowObservedUs == firstLowEligibility);
    consume(c, 3); assert(c.phase == Ess::HomePhase::ZERO_CHECK); consume(c); done(c);
    for (auto method : {Ess::HomingMethod::METHOD_33, Ess::HomingMethod::METHOD_34}) {
        c = home(method, 3); trigger(c);
        consume(c, 3); assert(!c.runningObserved && !c.homedLowObserved);
        consume(c, 6); // Running with stale latched HOMED does not prove a new home.
        consume(c, 3); assert(c.phase == Ess::HomePhase::OBSERVING);
        consume(c, 4); assert(c.lowEvidence.length == 9 && c.lowEvidence.step == c.step - 1);
        consume(c, 3); assert(c.phase == Ess::HomePhase::ZERO_CHECK);
        consume(c); assert(c.state == ActionState::SUCCEEDED); done(c);
        c = home(method); trigger(c); consume(c, 1); consume(c, 3);
        assert(c.phase == Ess::HomePhase::OBSERVING); // No new search activity.
    }
    c = home(Ess::HomingMethod::METHOD_35, 3); trigger(c);
    for (unsigned n = 0; n < c.options.maxPolls; ++n) consume(c, 3);
    assert(c.outcome == ActionOutcome::OBSERVATION_LIMIT && c.completion == ActionCompletion::NOT_OBSERVED); done(c);
    c = home(Ess::HomingMethod::METHOD_33); trigger(c); consume(c, 4); consume(c, 1);
    assert(c.state == ActionState::ACTIVE); // Interrupted search cannot establish a reference.
    auto e = local(c, ReadEventKind::CANCEL); assert(Ess::advanceHome(c, e, c.servicedUs));
    assert(c.uncertain && c.completion == ActionCompletion::NOT_OBSERVED); done(c);
    // Method35 has no search trajectory. Unexpected activity can never be
    // turned into a reference by a later homed/stopped report.
    for (uint16_t flags : {4, 6}) {
        c = home(); trigger(c); consume(c, flags);
        assert(c.state == ActionState::FAILED && c.outcome == ActionOutcome::REPLY_ERROR);
        assert(c.status.detail == static_cast<int32_t>(Ess::HomeError::UNEXPECTED_ACTIVITY));
        assert(c.uncertain && c.rawMotion == flags && c.completion == ActionCompletion::NOT_OBSERVED);
        AxisReference reference; reference.nativePosition = 123; const Saved<AxisReference> saved(reference);
        assert(!Ess::getHomeReference(c, c.servicedUs, 1000, reference)); saved.check(reference);
        const auto after = reply(c, 3); const auto event = frame(c, after, c.servicedUs + 20);
        const Saved<Ess::HomeContext> failed(c);
        assert(!Ess::advanceHome(c, event, c.servicedUs + 40)); failed.check(c); done(c);
    }
}
void testFailuresAndEnvelopes() {
    auto c = home(); const auto b = reply(c); auto e = frame(c, b, 120);
    for (unsigned fault = 0; fault < 10; ++fault) {
        auto bad = e;
        switch (fault) {
        case 0: ++bad.transport.target.id; break;
        case 1: ++bad.transport.target.generation; break;
        case 2: ++bad.transport.operationId; break;
        case 3: ++bad.transport.step; break;
        case 4: bad.transport.txAccepted = 22; break;
        case 5: bad.txComplete = false; break;
        case 6: bad.transport.frame = nullptr; break;
        case 7: bad.transport.earliestUs = 99; break;
        case 8: bad.transport.latestUs = 200; break;
        case 9: bad.transport.kind = static_cast<ReadEventKind>(99); break;
        }
        const Saved<Ess::HomeContext> saved(c); assert(!Ess::advanceHome(c, bad, 150)); saved.check(c);
    }
    // Lost setup ACK never stages a trigger and records possible partial application.
    e = local(c, ReadEventKind::TRANSPORT_FAILURE); e.transport.txAccepted = 10;
    const uint8_t partialRx[] = {0x11, 0x22, 0x33};
    e.transport.frame = partialRx; e.transport.length = sizeof(partialRx); e.transport.transportDetail = -12345;
    assert(Ess::advanceHome(c, e, 150)); assert(c.setupExecution == ActionExecution::UNKNOWN && c.uncertain); done(c);
    checkActionTransportEvidence(c.stagingEvidence, e, 0, 150);
    checkActionTransportEvidence(c.failureEvidence, e, 0, 150);
    assert(c.stagingEvidence.frameError == Ess::FrameError::NONE);
    assert(c.stagingEvidence.status.code == Err::ILLEGAL_VALUE);
    assert(c.stagingEvidence.status.detail == static_cast<int32_t>(Ess::HomeError::TRANSPORT_FAILURE));
    assert(std::strcmp(c.stagingEvidence.status.msg, "home transport failed") == 0);
    c = home(); consume(c); e = local(c, ReadEventKind::TRANSPORT_FAILURE);
    e.transport.txAccepted = 8; e.txComplete = true;
    assert(Ess::advanceHome(c, e, c.servicedUs + 20)); assert(c.execution == ActionExecution::UNKNOWN && c.stagingApplied); done(c);
    for (uint16_t flag : {8,16,32,64}) {
        c = home(); trigger(c); consume(c, flag);
        assert(c.status.detail == static_cast<int32_t>(Ess::HomeError::DRIVE_FAULT)); done(c);
    }
    c = home(); trigger(c); consume(c, 3, 0x8000); assert(c.rawAlarm == 0x8000 && c.uncertain); done(c);
    c = home(); trigger(c); consume(c, 3);
    auto nonzero = reply(c); nonzero.resize(7); nonzero[6] = 1; crc(nonzero);
    e = frame(c, nonzero, c.eligibleUs + 20);
    assert(Ess::advanceHome(c, e, c.eligibleUs + 40));
    assert(c.status.detail == static_cast<int32_t>(Ess::HomeError::ZERO_NOT_OBSERVED) && c.completion == ActionCompletion::NOT_OBSERVED); done(c);
    c = home(); auto corrupt = reply(c); corrupt.back() ^= 1; e = frame(c, corrupt, 120);
    assert(Ess::advanceHome(c, e, 150)); assert(c.outcome == ActionOutcome::REPLY_ERROR && c.uncertain); done(c);
    checkActionTransportEvidence(c.stagingEvidence, e, 0, 150);
    assert(c.stagingEvidence.status.code == Err::CRC_ERROR && c.stagingEvidence.frameError == Ess::FrameError::CRC);
    assert(c.stagingEvidence.status.detail == static_cast<int32_t>(Ess::FrameError::CRC));
    assert(std::strcmp(c.stagingEvidence.status.msg, "reply does not match request") == 0);
    c = home(); auto unchecked = reply(c); e = frame(c, unchecked, 120); e.responseConfirmed = false;
    assert(Ess::advanceHome(c, e, 150)); assert(c.outcome == ActionOutcome::UNCONFIRMED_RESPONSE); done(c);
    checkActionTransportEvidence(c.stagingEvidence, e, 0, 150);
    assert(c.stagingEvidence.status && c.stagingEvidence.frameError == Ess::FrameError::NONE);
    assert(c.stagingEvidence.status.detail == 0 && !*c.stagingEvidence.status.msg);
}
void testBorrowedAndOversizedEvidence() {
    auto c = home(); auto bytes = reply(c);
    std::memset(c.stagingEvidence.raw, 0xA5, sizeof(c.stagingEvidence.raw));
    std::memcpy(c.stagingEvidence.raw, bytes.data(), bytes.size());
    auto e = frame(c, bytes, 120); e.transport.frame = c.stagingEvidence.raw;
    e.transport.transportDetail = -12345; e.transport.executionUnknown = true;
    assert(Ess::advanceHome(c, e, 140));
    checkActionTransportEvidence(c.stagingEvidence, e, 0, 140);
    assert(std::memcmp(c.stagingEvidence.raw, bytes.data(), bytes.size()) == 0);
    assert(c.stagingEvidence.status && c.stagingEvidence.status.detail == 0);
    assert(!*c.stagingEvidence.status.msg);
    assert(c.stagingEvidence.frameError == Ess::FrameError::NONE);
    assert(c.setupExecution == ActionExecution::ACKNOWLEDGED);
    c = home(); bytes = reply(c); bytes.resize(40, 0xA5); e = frame(c, bytes, 120);
    assert(Ess::advanceHome(c, e, 140));
    checkActionTransportEvidence(c.stagingEvidence, e, 0, 140);
    checkActionTransportEvidence(c.failureEvidence, e, 0, 140);
    assert(c.stagingEvidence.status.code == Err::FRAME_ERROR);
    assert(c.stagingEvidence.frameError == Ess::FrameError::LENGTH);
    assert(c.stagingEvidence.status.detail == static_cast<int32_t>(Ess::FrameError::LENGTH));
    assert(std::strcmp(c.stagingEvidence.status.msg, "reply does not match request") == 0);
    const Saved<Ess::ActionEvidence> retained(c.failureEvidence);
    std::memset(bytes.data(), 0xAA, bytes.size()); retained.check(c.failureEvidence);
}
void testReadinessAndDelayedEvidence() {
    auto c = home(); consume(c);
    Ess::PreparedHome work; assert(Ess::nextHome(c, c.servicedUs, work) && work.deadlineUs == 1080);
    const Saved<Ess::PreparedHome> saved(work); assert(!Ess::nextHome(c, 1080, work)); saved.check(work);
    auto e = local(c, ReadEventKind::DEADLINE); assert(Ess::advanceHome(c, e, 1080));
    assert(c.stagingApplied && c.execution == ActionExecution::NOT_TRANSMITTED && c.uncertain); done(c);
    c = home(Ess::HomingMethod::METHOD_35, 1, 600); trigger(c); consume(c, 3);
    auto b = reply(c); e = frame(c, b, c.eligibleUs + 20);
    assert(e.transport.latestUs < c.deadlineUs);
    assert(Ess::advanceHome(c, e, 1000)); assert(c.state == ActionState::SUCCEEDED); done(c);
    checkActionTransportEvidence(c.zeroEvidence, e, e.transport.step, 1000);
    assert(c.zeroEvidence.status && c.zeroEvidence.status.detail == 0 && !*c.zeroEvidence.status.msg);
    assert(c.zeroEvidence.frameError == Ess::FrameError::NONE);
    AxisReference reference; assert(!Ess::getHomeReference(c, 1000, 500, reference));
    c = home(Ess::HomingMethod::METHOD_35, 1, 600); trigger(c);
    b = reply(c, 3); e = frame(c, b, c.eligibleUs + 20);
    assert(Ess::advanceHome(c, e, 1000));
    assert(c.state == ActionState::FAILED && c.completion == ActionCompletion::NOT_OBSERVED); done(c);
}
void testUnknownMotionCannotEstablishReference() {
    for (const auto method : {Ess::HomingMethod::METHOD_33, Ess::HomingMethod::METHOD_34, Ess::HomingMethod::METHOD_35}) {
        for (const bool observeInputs : {false, true}) {
            auto p = prerequisites(method);
            p.observeInputs = observeInputs; p.ioObservedUs = p.readbackObservedUs = 80;
            p.rawMotion = 0x8001;
            auto unchanged = home(method);
            const Saved<Ess::HomeContext> saved(unchanged);
            const auto rejected = Ess::prepareHome(unchanged, axis(), 12, request(method), p, 100, 10000, options());
            assert(!rejected && rejected.detail == static_cast<int32_t>(Ess::HomeError::READINESS));
            saved.check(unchanged);
            // Unknown bits cannot supply the low, activity or completion witness.
            for (const uint16_t flags : {uint16_t(0x8000), uint16_t(0x8004), uint16_t(0x8003)}) {
                p.rawMotion = 3; // Require a genuinely new low even for method35.
                Ess::HomeContext c;
                assert(Ess::prepareHome(c, axis(), 12, request(method), p, 100, 10000, options()));
                trigger(c);
                if (flags == 0x8004) consume(c, 1);
                if (flags == 0x8003) consume(c, method == Ess::HomingMethod::METHOD_35 ? 1 : 4);
                const bool priorLow = c.homedLowObserved, priorActivity = c.runningObserved;
                consume(c, flags);
                assert(c.state == ActionState::FAILED && c.outcome == ActionOutcome::REPLY_ERROR &&
                    c.status.code == Err::ILLEGAL_VALUE && c.status.detail == static_cast<int32_t>(Ess::HomeError::READINESS));
                assert(c.rawMotion == flags && c.observationKnown && c.uncertain &&
                    c.homedLowObserved == priorLow && c.runningObserved == priorActivity &&
                    c.completion == ActionCompletion::NOT_OBSERVED && !c.completionEvidence.length);
                assert(c.lastObservation.status && c.failureEvidence.status &&
                    c.failureEvidence.length == (observeInputs ? 11 : 9) &&
                    c.failureEvidence.raw[5] == 0x80 && c.failureEvidence.raw[6] == uint8_t(flags));
                AxisReference reference; reference.nativePosition = 123;
                assert(!Ess::getHomeReference(c, c.servicedUs, 0, reference) && reference.nativePosition == 123);
                done(c); // Terminal contexts yield no replay or zero-check work.
            }
            for (const bool alarmCode : {false, true}) {
                Ess::HomeContext c;
                assert(Ess::prepareHome(c, axis(), 12, request(method), p, 100, 10000, options()));
                trigger(c); consume(c, alarmCode ? 0x8003 : 0x8008, alarmCode ? 1 : 0);
                assert(c.status.detail == static_cast<int32_t>(Ess::HomeError::DRIVE_FAULT) &&
                    c.completion == ActionCompletion::NOT_OBSERVED); // Known faults retain precedence.
            }
        }
    }
}
void testMaximumPollBoundary() {
    for (const bool observeInputs : {false, true}) {
        auto p = prerequisites(Ess::HomingMethod::METHOD_33);
        p.observeInputs = observeInputs; p.ioObservedUs = p.readbackObservedUs = 80;
        auto o = options(); o.maxPolls = 64;
        Ess::HomeContext c;
        assert(Ess::prepareHome(c, axis(), 12, request(Ess::HomingMethod::METHOD_33), p, 100, 10000, o));
        trigger(c);
        for (unsigned i = 0; i < 63; ++i) consume(c, 4);
        consume(c, 3);
        assert(c.polls == 64 && c.step == 66 && c.phase == Ess::HomePhase::ZERO_CHECK);
        Ess::PreparedHome work;
        assert(Ess::nextHome(c, c.eligibleUs, work) && work.step == 66 && work.count == 2 && work.length == 8);
        consume(c); assert(c.state == ActionState::SUCCEEDED && c.polls == 64 && c.zeroEvidence.step == 66);
        done(c);
        assert(Ess::prepareHome(c, axis(), 13, request(Ess::HomingMethod::METHOD_33), p, 100, 10000, o));
        trigger(c);
        for (unsigned i = 0; i < 64; ++i) consume(c, 4);
        assert(c.outcome == ActionOutcome::OBSERVATION_LIMIT && c.polls == 64 && c.step == 65 && !c.zeroEvidence.length);
        done(c);
    }
}
// Source-derived vectors: original function PDF physical p32 (method1), p33
// (method2), p50-51 (method17). CRC literals were calculated independently of
// the production builders. These simulated samples do not observe direction,
// a physical edge, or internal Z; the drive owns the documented trajectories.
struct LimitCase {
    Ess::HomingMethod method;
    uint16_t function;
    uint8_t inputRole;
    bool index;
    uint8_t staging[21];
};
const LimitCase LIMIT_CASES[] = {
    // Initially inactive: -H to negative rising, stop/reverse +L; active:
    // directly +L. Both stop at the first internal Z after negative falling.
    {Ess::HomingMethod::METHOD_1, 3, 4, true,
        {1,16,0,49,0,6,12,0,1,0,60,0,30,0,100,0,0,0,0,0x55,0x67}},
    // Initially inactive: +H to positive rising, stop/reverse -L; active:
    // directly -L. Both stop at the first internal Z after positive falling.
    {Ess::HomingMethod::METHOD_2, 2, 2, true,
        {1,16,0,49,0,6,12,0,2,0,60,0,30,0,100,0,0,0,0,0x5A,0x23}},
    // Independently reviewed p50-51: inactive -H to negative rising then +L;
    // active directly +L. Stop at negative falling, with no Z dependency.
    {Ess::HomingMethod::METHOD_17, 3, 4, false,
        {1,16,0,49,0,6,12,0,17,0,60,0,30,0,100,0,0,0,0,0x01,0xA7}}
};
Ess::HomePrerequisites limitPrerequisites(const LimitCase& source, bool initiallyActive = false) {
    auto p = prerequisites(source.method);
    p.observeInputs = true; p.ioObservedUs = p.readbackObservedUs = 80;
    p.inputFunctions[0] = source.function; p.inputWiring[0] = InputWiring::CONNECTED;
    p.availableInputs = source.inputRole; p.indexQualified = source.index;
    p.rawInputs = initiallyActive ? 1 : 0;
    // Polarity is already applied by the drive; logical validity must not invert.
    p.inputPolarity = 1; p.rawMotion = 3;
    return p;
}
Ess::HomeContext limitHome(const LimitCase& source, bool initiallyActive = false) {
    Ess::HomeContext c;
    assert(Ess::prepareHome(c, axis(), 12, request(source.method), limitPrerequisites(source, initiallyActive),
        100, 10000, options()));
    return c;
}
void consumeInputs(Ess::HomeContext& c, uint16_t flags, uint16_t inputs) {
    auto bytes = reply(c, flags); bytes.resize(bytes.size() - 2);
    assert(bytes.size() == 9 && bytes[2] == 6);
    bytes[7] = static_cast<uint8_t>(inputs >> 8); bytes[8] = static_cast<uint8_t>(inputs);
    crc(bytes); const auto at = c.eligibleUs + 20;
    assert(Ess::advanceHome(c, frame(c, bytes, at), at + 20));
}
void testLimitBranchesAndVectors() {
    for (const auto& source : LIMIT_CASES) for (const bool initiallyActive : {false, true}) {
        auto c = limitHome(source, initiallyActive); Ess::PreparedHome work;
        assert(c.prerequisites.rawInputs == (initiallyActive ? 1 : 0));
        assert(c.prerequisites.indexQualified == source.index && !c.homedLowObserved);
        assert(Ess::nextHome(c, 100, work));
        assert(work.length == sizeof(source.staging) && work.write && work.reg == 0x31 && work.count == 6);
        assert(std::memcmp(work.bytes, source.staging, sizeof(source.staging)) == 0);
        consume(c);
        const uint8_t triggerBytes[] = {1,6,0,0x27,0,16,0x38,0x0D};
        assert(Ess::nextHome(c, c.eligibleUs, work));
        assert(work.length == sizeof(triggerBytes) && std::memcmp(work.bytes, triggerBytes, sizeof(triggerBytes)) == 0);
        consume(c);
        assert(c.execution == ActionExecution::ACKNOWLEDGED && c.completion == ActionCompletion::NOT_OBSERVED);
        const uint8_t pollBytes[] = {1,3,0,6,0,3,0xE5,0xCA};
        assert(Ess::nextHome(c, c.eligibleUs, work));
        assert(work.length == sizeof(pollBytes) && std::memcmp(work.bytes, pollBytes, sizeof(pollBytes)) == 0);
        if (!initiallyActive) consumeInputs(c, 4, 0);
        consumeInputs(c, 4, 1); // Expected physical limit activation is not a soft-limit fault.
        assert(c.state == ActionState::ACTIVE && c.runningObserved && c.homedLowObserved);
        consumeInputs(c, 4, 0); // Effective release sample; this is not a wire-edge witness.
        consumeInputs(c, 3, 0); assert(c.phase == Ess::HomePhase::ZERO_CHECK);
        assert(c.activityEvidence.length == 11 && c.lowEvidence.length == 11 && c.completionEvidence.length == 11);
        consume(c); assert(c.state == ActionState::SUCCEEDED && !c.uncertain);
        AxisReference reference; assert(Ess::getHomeReference(c, c.servicedUs, 0, reference));
        assert(reference.nativeKnown && reference.nativePosition == 0); done(c);

        // A sampled input transition can be missed completely. Qualified fresh
        // status transitions plus zero still complete; no invented Z/edge proof.
        // Alarm, motion and input words do not promise an atomic snapshot, so
        // an unchanged active input sample alone cannot contradict completion.
        for (const uint16_t sampledInputs : {uint16_t(0), uint16_t(1)}) {
            c = limitHome(source, initiallyActive); trigger(c);
            consumeInputs(c, 4, sampledInputs); consumeInputs(c, 3, sampledInputs); consume(c);
            assert(c.state == ActionState::SUCCEEDED && c.inputsObserved && c.rawInputs == sampledInputs); done(c);
        }
    }
}
void testLimitAdmission() {
    for (const auto& source : LIMIT_CASES) {
        auto c = limitHome(source);
        for (unsigned fault = 0; fault < 22; ++fault) {
            auto p = limitPrerequisites(source); auto r = request(source.method);
            switch (fault) {
            case 0: p.observeInputs = false; break;
            case 1: p.availableInputs = 0; break;
            case 2: p.inputFunctions[0] = 0; break;
            case 3: p.inputFunctions[0] = source.function == 3 ? 2 : 3; break;
            case 4: p.inputWiring[0] = InputWiring::UNCONNECTED; break;
            case 5: p.inputWiring[0] = InputWiring::UNKNOWN; break;
            case 6: p.inputPolarity = 16; break;
            case 7: p.rawInputs = 16; break;
            case 8: p.ioObservedUs = 0; p.maximumAgeUs = 100; break;
            case 9: p.ioObservedUs = 101; break;
            case 10: p.readbackObservedUs = 0; p.maximumAgeUs = 100; break;
            case 11: ++p.target.generation; break;
            case 12: ++p.configurationGeneration; break;
            case 13: ++r.configurationGeneration; break;
            case 14: r.offset = 1; break;
            case 15: r.searchSpeed = 3001; break;
            case 16: r.returnSpeed = 301; break;
            case 17: r.rampTime = 29; break;
            case 18: ++r.searchSpeed; break;
            case 19: p.inputsQualified = false; break;
            case 20: p.methodQualified = false; break;
            case 21: p.qualifiedUs = 0; p.qualificationMaximumAgeUs = 100; break;
            }
            const Saved<Ess::HomeContext> saved(c);
            assert(!Ess::prepareHome(c, axis(), 12, r, p, 100, 10000, options())); saved.check(c);
        }
        auto p = limitPrerequisites(source); p.indexQualified = false;
        const auto result = Ess::prepareHome(c, axis(), 12, request(source.method), p, 100, 10000, options());
        if (source.index) assert(!result && result.detail == static_cast<int32_t>(Ess::HomeError::INDEX_REQUIRED));
        else assert(result && !c.prerequisites.indexQualified); // Method17 must never inherit a Z prerequisite.
    }
}
void testLimitIncompleteAndFailedEvidence() {
    for (const auto& source : LIMIT_CASES) {
        for (const uint16_t stuckFlags : {uint16_t(3),uint16_t(4),uint16_t(6),uint16_t(1)}) {
            auto c = limitHome(source, true); trigger(c);
            // Old HOMED/ARRIVED, stuck RUNNING, RUNNING with stale HOMED, or
            // no observed search cannot prove a reference, even with active IO.
            for (unsigned poll = 0; poll < c.options.maxPolls; ++poll) consumeInputs(c, stuckFlags, 1);
            assert(c.outcome == ActionOutcome::OBSERVATION_LIMIT && c.uncertain);
            assert(c.completion == ActionCompletion::NOT_OBSERVED && !c.zeroEvidence.length); done(c);
        }
        for (unsigned fault = 0; fault < 6; ++fault) {
            auto c = limitHome(source); trigger(c); auto bytes = reply(c, 4);
            bytes.resize(bytes.size() - 2);
            if (fault == 0) bytes[0] = 2;
            if (fault == 1) bytes[1] = 4;
            if (fault == 2) bytes[2] = 4;
            if (fault == 3) bytes.pop_back();
            if (fault == 4) bytes.push_back(0);
            crc(bytes); if (fault == 5) bytes.back() ^= 1;
            const auto at = c.eligibleUs + 20;
            assert(Ess::advanceHome(c, frame(c, bytes, at), at + 20));
            assert(c.outcome == ActionOutcome::REPLY_ERROR && !c.runningObserved && !c.homedLowObserved);
            assert(c.completion == ActionCompletion::NOT_OBSERVED); done(c);
        }
        for (const uint16_t flags : {uint16_t(8),uint16_t(16),uint16_t(32),uint16_t(64),uint16_t(0x8004)}) {
            auto c = limitHome(source); trigger(c); consumeInputs(c, flags, 1);
            assert(c.state == ActionState::FAILED && c.uncertain && c.completion == ActionCompletion::NOT_OBSERVED);
            assert(c.status.detail == static_cast<int32_t>(flags == 0x8004 ? Ess::HomeError::READINESS : Ess::HomeError::DRIVE_FAULT)); done(c);
        }
        auto c = limitHome(source); trigger(c); consumeInputs(c, 4, 16);
        assert(c.status.detail == static_cast<int32_t>(Ess::HomeError::INPUT_REQUIRED)); done(c);
        c = limitHome(source); trigger(c); consumeInputs(c, 4, 0); consumeInputs(c, 3, 0);
        auto bytes = reply(c); bytes.resize(bytes.size() - 2); bytes[6] = 1; crc(bytes);
        auto at = c.eligibleUs + 20;
        assert(Ess::advanceHome(c, frame(c, bytes, at), at + 20));
        assert(c.status.detail == static_cast<int32_t>(Ess::HomeError::ZERO_NOT_OBSERVED)); done(c);
        c = limitHome(source); trigger(c);
        assert(Ess::advanceHome(c, local(c, ReadEventKind::DEADLINE), c.deadlineUs));
        assert(c.outcome == ActionOutcome::DEADLINE && c.uncertain); done(c);
        c = limitHome(source); trigger(c); consumeInputs(c, 4, 1);
        assert(Ess::advanceHome(c, local(c, ReadEventKind::CANCEL), c.servicedUs));
        assert(c.outcome == ActionOutcome::CANCELLED && c.uncertain); done(c);
        for (const bool setupAcknowledged : {false, true}) {
            c = limitHome(source); if (setupAcknowledged) consume(c);
            auto event = local(c, ReadEventKind::TRANSPORT_FAILURE);
            event.transport.txAccepted = setupAcknowledged ? 8 : 21; event.txComplete = true;
            assert(Ess::advanceHome(c, event, c.servicedUs + 20));
            assert(c.uncertain && c.completion == ActionCompletion::NOT_OBSERVED &&
                (setupAcknowledged ? c.execution : c.setupExecution) == ActionExecution::UNKNOWN); done(c);
            const Saved<Ess::HomeContext> saved(c); bytes = reply(c);
            assert(!Ess::advanceHome(c, frame(c, bytes, c.servicedUs + 20), c.servicedUs + 40)); saved.check(c);
        }
        c = limitHome(source); trigger(c); bytes = reply(c, 4); at = c.eligibleUs + 20;
        auto oldEvent = frame(c, bytes, at); --oldEvent.transport.operationId;
        const Saved<Ess::HomeContext> saved(c);
        assert(!Ess::advanceHome(c, oldEvent, at + 20)); saved.check(c);
    }
}
}
static void testOptionalAgePolicy() {
    const uint64_t now = 100000, deadline = 110000;
    auto p = prerequisites(); p.maximumAgeUs = 0;
    Ess::HomeContext c;
    assert(Ess::prepareHome(c, axis(), 12, request(), p, now, deadline, options()));
    Ess::PreparedHome work;
    assert(Ess::nextHome(c, now, work));
    assert(work.deadlineUs == deadline);
    const Saved<Ess::HomeContext> saved(c);
    p.maximumAgeUs = 1000; assert(!(Ess::prepareHome(c, axis(), 12, request(), p, now, deadline, options()))); saved.check(c);
    p.maximumAgeUs = 0; p.target.generation++;
    assert(!(Ess::prepareHome(c, axis(), 12, request(), p, now, deadline, options()))); saved.check(c);
    p = prerequisites(); p.maximumAgeUs = 0; p.readinessQualified = false;
    assert(!(Ess::prepareHome(c, axis(), 12, request(), p, now, deadline, options()))); saved.check(c);
    p.readinessQualified = true; p.observedUs = now + 1;
    assert(!(Ess::prepareHome(c, axis(), 12, request(), p, now, deadline, options()))); saved.check(c);
    assert(Ess::advanceHome(c, local(c, ReadEventKind::DEADLINE), deadline));
    assert(c.state == ActionState::FAILED && c.outcome == ActionOutcome::DEADLINE);
    assert(Ess::nextHome(c, deadline, work) && work.kind == Ess::ActionWork::DONE && !work.length);
}
int main() {
    // Independent fixture age cannot be renewed by fresh motion observations.
    auto p = prerequisites(); p.qualifiedUs = 50; p.qualificationMaximumAgeUs = 100;
    p.observeInputs = true; p.ioObservedUs = 75; p.readbackObservedUs = 60;
    assert(Ess::checkHomeQualification(p, 149));
    assert(!Ess::checkHomeQualification(p, 150));
    Ess::HomeContext admitted;
    assert(Ess::prepareHome(admitted, axis(), 12, request(), p, 100, 10000, options()));
    Ess::PreparedHome work; assert(Ess::nextHome(admitted, 100, work) && work.deadlineUs == 150);
    p.qualificationMaximumAgeUs = 0; p.qualifiedUs = 101;
    assert(!Ess::checkHomeQualification(p, 100));
    p.qualifiedUs = 50; p.inputFunctions[0] = 2; p.inputWiring[0] = InputWiring::CONNECTED; p.availableInputs = 2;
    p.qualifiedMethod = Ess::HomingMethod::METHOD_2; p.rawInputs = 1; p.inputPolarity = 1;
    assert(Ess::checkHomeQualification(p, 100)); // Required physical limit is not a soft-limit fault; no second inversion.
    p.inputWiring[0] = InputWiring::UNCONNECTED; assert(!Ess::checkHomeQualification(p, 100));
    p.inputWiring[0] = InputWiring::CONNECTED; p.inputFunctions[0] = 3; assert(!Ess::checkHomeQualification(p, 100));
    p.inputFunctions[0] = 2; p.inputPolarity = 16; assert(!Ess::checkHomeQualification(p, 100));
    // Every phase retains the settled event while revocation wins over success.
    for (unsigned phase = 0; phase < 4; ++phase) {
        p = prerequisites(); p.observeInputs = true; p.ioObservedUs = p.readbackObservedUs = 80;
        Ess::HomeContext c;
        assert(Ess::prepareHome(c, axis(), 12, request(), p, 100, 10000, options()));
        if (phase >= 1) consume(c);
        if (phase >= 2) consume(c);
        if (phase >= 3) {
            assert(Ess::nextHome(c, c.eligibleUs, work) && work.reg == 6 && work.count == 3);
            const uint8_t expected[] = {1,3,0,6,0,3,0xE5,0xCA};
            assert(work.length == sizeof(expected) && std::memcmp(work.bytes, expected, sizeof(expected)) == 0);
            consume(c, 3); assert(c.inputsObserved && c.phase == Ess::HomePhase::ZERO_CHECK);
        }
        const auto bytes = reply(c, 3); const uint64_t at = c.eligibleUs + 20;
        auto e = frame(c, bytes, at);
        assert(Ess::revokeHome(c)); assert(Ess::revokeHome(c));
        const Saved<Ess::PreparedHome> saved(work);
        assert(!Ess::nextHome(c, at, work)); saved.check(work);
        assert(Ess::advanceHome(c, e, at + 20));
        assert(c.outcome == ActionOutcome::CANCELLED && c.qualificationRevoked && c.failureEvidence.status);
        assert(c.completion == ActionCompletion::NOT_OBSERVED && c.uncertain);
        if (phase == 3) assert(c.zeroEvidence.length == 9 && c.zeroEvidence.responseConfirmed);
        AxisReference reference; reference.nativePosition = 123;
        assert(!Ess::getHomeReference(c, at + 20, 0, reference) && reference.nativePosition == 123);
        const Saved<Ess::HomeContext> retained(c); assert(Ess::revokeHome(c)); retained.check(c);
        done(c);
    }
    testOptionalAgePolicy();
    testDescriptorsAndNoTraffic(); testPreparationAndBounds(); testExactQualificationCannotBeReused(); testExactWireAndReference();
    testCorrelatedTransitions(); testFailuresAndEnvelopes(); testReadinessAndDelayedEvidence();
    testUnknownMotionCannotEstablishReference();
    testMaximumPollBoundary();
    testBorrowedAndOversizedEvidence();
    testLimitBranchesAndVectors(); testLimitAdmission(); testLimitIncompleteAndFailedEvidence();
}
