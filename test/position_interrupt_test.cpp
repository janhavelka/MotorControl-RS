// SPDX-License-Identifier: MIT
#include <MotorControlRS/profiles/ess_rs/Position.h>
#include <cassert>
#include <cstring>
#include <limits>
#include <vector>

using namespace MotorControlRS;
namespace E = MotorControlRS::ESS_RS;
namespace {
// Function manual physical p16: start bit 0, absolute bit 2, interrupt bit 3.
// CRCs were independently derived with the Modbus polynomial, not the builder.
const uint8_t relativeIgnore[] = {1, 6, 0, 0x27, 0, 1, 0xF8, 0x01};
const uint8_t absoluteIgnore[] = {1, 6, 0, 0x27, 0, 5, 0xF9, 0xC2};
const uint8_t relativeInterrupt[] = {1, 6, 0, 0x27, 0, 9, 0xF9, 0xC7};
const uint8_t absoluteInterrupt[] = {1, 6, 0, 0x27, 0, 13, 0xF8, 0x04};
const uint32_t originalId = 40, replacementId = 41, targetBits = 0x12345678;

template<class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof(value)); }
    void check(const T& value) const { assert(!std::memcmp(bytes, &value, sizeof(value))); }
};
std::vector<uint8_t> withCrc(std::vector<uint8_t> bytes) {
    uint16_t crc = 0xFFFF;
    for (uint8_t byte : bytes) {
        crc ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001) : crc >> 1;
    }
    bytes.push_back(static_cast<uint8_t>(crc)); bytes.push_back(static_cast<uint8_t>(crc >> 8));
    return bytes;
}
E::PositionCommand command(bool relative = true, E::WordOrder order = E::WordOrder::HIGH_WORD_FIRST) {
    E::PositionCommand c;
    c.target.id = 7; c.target.address = 1; c.target.generation = 4;
    c.subdivision = 1000; c.speedRpm = 60; c.accelerationTime = 100; c.decelerationTime = 120;
    c.wordOrder = order; c.setup = MoveSetup::USE_STORED;
    c.interruption = E::PositionInterruption::INTERRUPT; c.configurationGeneration = 9;
    c.stored.target = c.target; c.stored.configurationGeneration = 9;
    c.stored.originalOperationId = originalId; c.stored.subdivision = 1000;
    c.stored.wordOrder = order; c.stored.relative = relative; c.stored.qualified = true;
    c.stored.qualifiedUs = 80; c.stored.maximumAgeUs = 920;
    c.stored.profile.startSpeed = 30; c.stored.profile.accelerationTime = 100;
    c.stored.profile.decelerationTime = 120; c.stored.profile.speed = 60;
    c.stored.profile.targetBits = targetBits;
    return c;
}
E::MoveContext prepare(const E::PositionCommand& c = command(), uint64_t now = 100, uint64_t deadline = 2000) {
    E::MoveContext out;
    const Status status = c.stored.relative ? c.prepareRelative(out, replacementId, targetBits, now, deadline) :
        c.prepareAbsolute(out, replacementId, targetBits, now, deadline);
    assert(status); return out;
}
ActionEvent local(const E::MoveContext& c, ReadEventKind kind) {
    ActionEvent event; event.transport.target = c.target;
    event.transport.operationId = c.operationId; event.transport.step = c.step;
    event.transport.kind = kind; return event;
}
ActionEvent frame(const E::MoveContext& c, const uint8_t* bytes, std::size_t length,
                  uint64_t earliest = 110, uint64_t latest = 120) {
    ActionEvent event = local(c, ReadEventKind::FRAME);
    event.transport.frame = bytes; event.transport.length = length;
    event.transport.txAccepted = 8; event.transport.qualified = true;
    event.transport.earliestUs = earliest; event.transport.latestUs = latest;
    event.txComplete = event.responseConfirmed = true; return event;
}
void noMoreWork(const E::MoveContext& c) {
    E::PreparedMove work; assert(E::nextMove(c, c.servicedUs, work));
    assert(work.kind == E::ActionWork::DONE && !work.length && !work.write);
    E::MoveContext copy = c;
    const Saved<E::MoveContext> before(copy);
    assert(!E::advanceMove(copy, local(copy, ReadEventKind::CANCEL), copy.servicedUs + 1));
    before.check(copy);
}
void rawFramesAndBounds() {
    for (bool relative : {false, true}) {
        for (E::PositionInterruption intent : {E::PositionInterruption::IGNORE, E::PositionInterruption::INTERRUPT}) {
            const uint8_t* expected = intent == E::PositionInterruption::IGNORE ?
                (relative ? relativeIgnore : absoluteIgnore) : (relative ? relativeInterrupt : absoluteInterrupt);
            uint8_t bytes[10]; std::memset(bytes, 0xA7, sizeof(bytes));
            for (std::size_t cap = 0; cap < 8; ++cap) {
                const Saved<decltype(bytes)> before(bytes);
                assert(!E::buildStartPosition(1, relative, intent, bytes, cap)); before.check(bytes);
            }
            assert(!E::buildStartPosition(1, relative, intent, nullptr, 8));
            for (uint8_t address : {0, 248, 255}) {
                const Saved<decltype(bytes)> before(bytes);
                assert(!E::buildStartPosition(address, relative, intent, bytes, sizeof(bytes))); before.check(bytes);
            }
            assert(E::buildStartPosition(1, relative, intent, bytes, sizeof(bytes)) == 8);
            assert(!std::memcmp(bytes, expected, 8) && bytes[8] == 0xA7 && bytes[9] == 0xA7);
        }
        uint8_t bytes[8];
        assert(E::buildStartPosition(1, relative, bytes, sizeof(bytes)) == 8);
        assert(!std::memcmp(bytes, relative ? relativeIgnore : absoluteIgnore, 8));
        for (unsigned invalid = 2; invalid <= 255; ++invalid) {
            const Saved<decltype(bytes)> before(bytes);
            assert(!E::buildStartPosition(1, relative, static_cast<E::PositionInterruption>(invalid), bytes, sizeof(bytes)));
            before.check(bytes);
        }
    }
    assert(E::PositionCommand().interruption == E::PositionInterruption::IGNORE);
}
void admittedNativeSnapshot() {
    for (bool relative : {false, true}) {
        for (E::WordOrder order : {E::WordOrder::HIGH_WORD_FIRST, E::WordOrder::LOW_WORD_FIRST}) {
            auto intent = command(relative, order); auto c = prepare(intent);
            const Saved<E::StoredPositionQualification> stored(c.stored);
            assert(c.admission == E::MoveAdmission::NATIVE_INTENT && c.interruption == E::PositionInterruption::INTERRUPT);
            assert(c.stored.originalOperationId == originalId && c.operationId == replacementId);
            assert(c.request.position.relative == relative && c.request.position.configurationGeneration == 9);
            assert(!c.prepared.endpointKnown && !c.prepared.displacementKnown && !c.reference.nativeKnown);
            assert(!c.prerequisites.readinessQualified && !c.observationKnown && !c.runningObserved);
            assert(c.words[0] == 100 && c.words[1] == 120 && c.words[2] == 60);
            assert(c.words[3] == (order == E::WordOrder::HIGH_WORD_FIRST ? 0x1234 : 0x5678));
            assert(c.words[4] == (order == E::WordOrder::HIGH_WORD_FIRST ? 0x5678 : 0x1234));
            intent.stored.qualified = false; intent.stored.maximumAgeUs = 1;
            intent.stored.profile.targetBits = 0; intent.interruption = E::PositionInterruption::IGNORE;
            E::PreparedMove work;
            assert(E::nextMove(c, 100, work) && work.kind == E::ActionWork::TRANSACTION);
            const uint8_t* expected = relative ? relativeInterrupt : absoluteInterrupt;
            assert(work.length == 8 && !std::memcmp(work.bytes, expected, 8));
            assert(work.write && work.function == 6 && work.reg == 0x27 && work.count == 1);
            assert(work.deadlineUs == 1000 && c.deadlineUs == 2000 && c.step == c.triggerStep && c.setupCount == 0);
            assert(E::advanceMove(c, frame(c, expected, 8), 130));
            assert(c.outcome == ActionOutcome::ACKNOWLEDGED && c.execution == ActionExecution::ACKNOWLEDGED);
            assert(c.completion == ActionCompletion::NOT_OBSERVED && !c.uncertain);
            assert(!c.stagingApplied && c.setupExecution == ActionExecution::NOT_TRANSMITTED);
            assert(!c.observationKnown && !c.runningObserved && !c.lastObservation.length && !c.polls);
            assert(!std::memcmp(c.triggerEvidence.raw, expected, 8));
            stored.check(c.stored); noMoreWork(c);
        }
    }
}
void rejectedAdmission() {
    for (unsigned failure = 0; failure < 28; ++failure) {
        auto intent = command(); uint32_t bits = targetBits;
        MoveError expected = MoveError::STALE_CONFIGURATION;
        switch (failure) {
        case 0: intent.setup = MoveSetup::WRITE_ALL; expected = MoveError::INVALID_OPTIONS; break;
        case 1: intent.setup = MoveSetup::VERIFY_AND_UPDATE; expected = MoveError::INVALID_OPTIONS; break;
        case 2: intent.setup = static_cast<MoveSetup>(255); expected = MoveError::INVALID_OPTIONS; break;
        case 3: intent.interruption = static_cast<E::PositionInterruption>(2); expected = MoveError::INVALID_OPTIONS; break;
        case 4: intent.configurationGeneration = 0; break;
        case 5: ++intent.configurationGeneration; break;
        case 6: ++intent.stored.target.id; break;
        case 7: ++intent.stored.target.address; break;
        case 8: ++intent.stored.target.generation; break;
        case 9: intent.stored.configurationGeneration = 0; break;
        case 10: intent.stored.originalOperationId = 0; break;
        case 11: intent.stored.originalOperationId = replacementId; break;
        case 12: ++intent.stored.subdivision; break;
        case 13: intent.stored.wordOrder = E::WordOrder::LOW_WORD_FIRST; break;
        case 14: intent.stored.wordOrder = static_cast<E::WordOrder>(2); break;
        case 15: intent.stored.relative = false; break;
        case 16: ++intent.stored.profile.accelerationTime; break;
        case 17: ++intent.stored.profile.decelerationTime; break;
        case 18: ++intent.stored.profile.speed; break;
        case 19: ++intent.stored.profile.targetBits; break;
        case 20: ++bits; break;
        case 21: intent.stored.profile.startSpeed = 3001; expected = MoveError::INVALID_REQUEST; break;
        case 22: intent.stored.qualified = false; expected = MoveError::READINESS; break;
        case 23: intent.stored.qualifiedUs = 101; expected = MoveError::READINESS; break;
        case 24: intent.stored.maximumAgeUs = 20; expected = MoveError::READINESS; break;
        case 25: intent.stored.maximumAgeUs = 19; expected = MoveError::READINESS; break;
        case 26: intent.accelerationTime = 99; expected = MoveError::OPERATING_LIMIT; break;
        case 27: intent.speedRpm = 2001; expected = MoveError::OPERATING_LIMIT; break;
        }
        E::MoveContext output; const Saved<E::MoveContext> before(output);
        const Status status = intent.prepareRelative(output, replacementId, bits, 100, 2000);
        assert(!status && status.detail == static_cast<int32_t>(expected)); before.check(output);
        E::PreparedMove work; const Saved<E::PreparedMove> beforeWork(work);
        assert(!E::nextMove(output, 100, work)); beforeWork.check(work);
    }
    // Start-speed range is raw only: qualification owns compatibility. No new
    // host relation between the retained raw start word and RPM is invented.
    auto intent = command(); intent.stored.profile.startSpeed = 3000;
    const auto c = prepare(intent); assert(c.stored.profile.startSpeed == 3000);
    intent.stored.qualifiedUs = 101; intent.stored.maximumAgeUs = 0;
    E::MoveContext out; assert(!intent.prepareRelative(out, replacementId, targetBits, 100, 2000));
}
void deadlineBoundaries() {
    auto c = prepare(); E::PreparedMove work; const Saved<E::PreparedMove> before(work);
    assert(!E::nextMove(c, 99, work)); before.check(work);
    assert(E::nextMove(c, 999, work) && work.deadlineUs == 1000);
    const Saved<E::PreparedMove> issued(work);
    assert(!E::nextMove(c, 1000, work)); issued.check(work);
    auto expired = local(c, ReadEventKind::DEADLINE); const Saved<E::MoveContext> pending(c);
    assert(!E::advanceMove(c, expired, 999)); pending.check(c);
    assert(E::advanceMove(c, expired, 1000));
    assert(c.outcome == ActionOutcome::DEADLINE && c.status.detail == static_cast<int32_t>(MoveError::READINESS));
    assert(c.execution == ActionExecution::NOT_TRANSMITTED && !c.uncertain); noMoreWork(c);
    for (uint64_t closure : {uint64_t(999), uint64_t(1000), uint64_t(1001)}) {
        c = prepare(); assert(E::advanceMove(c, frame(c, relativeInterrupt, 8, closure, closure), 2100));
        assert(c.outcome == (closure <= 1000 ? ActionOutcome::ACKNOWLEDGED : ActionOutcome::DEADLINE));
        assert(c.execution == ActionExecution::ACKNOWLEDGED && c.completion == ActionCompletion::NOT_OBSERVED);
        assert(c.uncertain == (closure > 1000)); noMoreWork(c);
    }
    auto intent = command(); intent.stored.maximumAgeUs = 0;
    c = prepare(intent, 900000, 900100); assert(E::nextMove(c, 900000, work) && work.deadlineUs == 900100);
    intent.stored.maximumAgeUs = 920;
    c = prepare(intent, 100, 500); assert(E::nextMove(c, 100, work) && work.deadlineUs == 500);
    const uint64_t maximum = std::numeric_limits<uint64_t>::max();
    intent.stored.qualifiedUs = maximum - 100; intent.stored.maximumAgeUs = 200;
    c = prepare(intent, maximum - 50, maximum); // Saturating expiry must not wrap.
    assert(E::nextMove(c, maximum - 1, work) && work.deadlineUs == maximum);
    assert(!E::nextMove(c, maximum, work));
}
void envelopesAndReplyEvidence() {
    for (unsigned mismatch = 0; mismatch < 5; ++mismatch) {
        auto c = prepare(); auto event = frame(c, relativeInterrupt, 8);
        if (mismatch == 0) event.transport.operationId = originalId;
        if (mismatch == 1) ++event.transport.target.id;
        if (mismatch == 2) ++event.transport.target.address;
        if (mismatch == 3) ++event.transport.target.generation;
        if (mismatch == 4) --event.transport.step;
        const Saved<E::MoveContext> before(c);
        const Status status = E::advanceMove(c, event, 130);
        assert(!status && status.detail == static_cast<int32_t>(MoveError::WRONG_CORRELATION)); before.check(c);
    }
    for (unsigned fault = 0; fault < 10; ++fault) {
        auto c = prepare(); std::vector<uint8_t> bytes(relativeInterrupt, relativeInterrupt + 8);
        if (fault == 0) bytes.back() ^= 1;
        if (fault == 1) bytes = withCrc({2, 6, 0, 0x27, 0, 9});
        if (fault == 2) bytes = withCrc({1, 16, 0, 0x27, 0, 9});
        if (fault == 3) bytes = withCrc({1, 6, 0, 0x26, 0, 9});
        if (fault == 4) bytes.assign(relativeIgnore, relativeIgnore + 8);
        if (fault == 5) bytes.assign(absoluteInterrupt, absoluteInterrupt + 8);
        if (fault == 6) bytes.pop_back();
        if (fault == 7) bytes.push_back(0);
        if (fault == 8) bytes = withCrc({1, 3, 4, 0, 0, 0x80, 5}); // Old HOMED/RUNNING/ARRIVED is no trigger ACK.
        if (fault == 9) bytes = withCrc({1, 0x86, 3});
        assert(E::advanceMove(c, frame(c, bytes.data(), bytes.size()), 130));
        assert(c.outcome == ActionOutcome::REPLY_ERROR && c.uncertain);
        assert(c.execution == (fault == 9 ? ActionExecution::REJECTED : ActionExecution::UNKNOWN));
        assert(c.completion == ActionCompletion::NOT_OBSERVED && !c.observationKnown && !c.runningObserved);
        assert(c.failureEvidence.length == bytes.size() && !std::memcmp(c.failureEvidence.raw, bytes.data(), bytes.size()));
        noMoreWork(c);
    }
    for (unsigned ambiguous = 0; ambiguous < 2; ++ambiguous) {
        auto c = prepare(); auto event = frame(c, relativeInterrupt, 8);
        if (!ambiguous) event.responseConfirmed = false; // Includes a bit-identical late ACK/local echo.
        else { event.transport.qualified = false; event.transport.earliestUs = event.transport.latestUs = 0; }
        assert(E::advanceMove(c, event, 130));
        assert(c.outcome == (!ambiguous ? ActionOutcome::UNCONFIRMED_RESPONSE : ActionOutcome::TIMING_UNQUALIFIED));
        assert(c.execution == ActionExecution::UNKNOWN && c.uncertain && c.completion == ActionCompletion::NOT_OBSERVED);
        noMoreWork(c);
    }
}
void cancellationAndLostAcknowledgement() {
    for (ReadEventKind kind : {ReadEventKind::CANCEL, ReadEventKind::TRANSPORT_FAILURE, ReadEventKind::DEADLINE}) {
        for (std::size_t transmitted : {std::size_t(0), std::size_t(3), std::size_t(8)}) {
            auto c = prepare(); auto event = local(c, kind);
            event.transport.txAccepted = transmitted; event.txComplete = transmitted == 8;
            assert(E::advanceMove(c, event, kind == ReadEventKind::DEADLINE ? 1000 : 130));
            assert(c.state == ActionState::FAILED && c.uncertain == (transmitted != 0));
            assert(c.execution == (transmitted ? ActionExecution::UNKNOWN : ActionExecution::NOT_TRANSMITTED));
            assert(c.completion == ActionCompletion::NOT_OBSERVED && c.triggerEvidence.txAccepted == transmitted);
            assert(!c.stagingApplied && c.setupExecution == ActionExecution::NOT_TRANSMITTED);
            noMoreWork(c);
        }
    }
    auto c = prepare(); auto event = local(c, ReadEventKind::CANCEL);
    event.transport.executionUnknown = true; // Owner can retain possible TX even without a byte count.
    assert(E::advanceMove(c, event, 130) && c.uncertain && c.execution == ActionExecution::UNKNOWN); noMoreWork(c);
}
void observedOriginalIsIndependent() {
    auto intent = command(); AxisConfig axis; axis.target = intent.target; axis.generation = 9;
    axis.supportedRelativeBases = 1;
    MoveRequest request; request.position.value = Rational(targetBits); request.position.configurationGeneration = 9;
    request.speedRpm = 60; request.ramp = MoveRamp::VERIFIED_CONFIGURED;
    E::MovePrerequisites prerequisites; prerequisites.target = axis.target; prerequisites.configurationGeneration = 9;
    prerequisites.subdivision = 1000; prerequisites.commandUnitsVerified = prerequisites.relativeBasisVerified = true;
    prerequisites.configuredRampVerified = prerequisites.serialInputsPermit = prerequisites.readinessQualified = true;
    prerequisites.wordOrderKnown = prerequisites.startSpeedKnown = true;
    prerequisites.accelerationTime = 100; prerequisites.decelerationTime = 120; prerequisites.startSpeed = 30;
    prerequisites.observedUs = 80; prerequisites.maximumAgeUs = 920;
    E::MoveContext original;
    assert(E::prepareMoveRelative(original, axis, nullptr, originalId, request, prerequisites, 100, 100000));
    assert(original.interruption == E::PositionInterruption::IGNORE);
    const uint8_t staged[] = {1, 16, 0, 0x21, 0, 5, 0x50, 0};
    auto stagingEvent = frame(original, staged, sizeof(staged)); stagingEvent.transport.txAccepted = 19;
    assert(E::advanceMove(original, stagingEvent, 130));
    assert(original.stagingApplied && original.setupExecution == ActionExecution::ACKNOWLEDGED);
    assert(E::advanceMove(original, frame(original, relativeIgnore, 8, 140, 150), 160));
    const auto running = withCrc({1, 3, 4, 0, 0, 0x80, 5});
    assert(E::advanceMove(original, frame(original, running.data(), running.size(), 10160, 10170), 10180));
    assert(original.runningObserved && original.state == ActionState::ACTIVE && original.execution == ActionExecution::ACKNOWLEDGED);
    const Saved<E::MoveContext> before(original);
    intent.stored.qualifiedUs = 10180; intent.stored.maximumAgeUs = 1000;
    auto replacement = prepare(intent, 10200, 12000); // The relation transfers no old observation.
    assert(!replacement.runningObserved && !replacement.observationKnown && !replacement.prepared.endpointKnown);
    assert(E::advanceMove(replacement, frame(replacement, relativeInterrupt, 8, 10210, 10220), 10230));
    assert(replacement.outcome == ActionOutcome::ACKNOWLEDGED && replacement.completion == ActionCompletion::NOT_OBSERVED);
    before.check(original);
    // Observed APIs remain stationary-only; there is no interrupt flag which can bypass this admission.
    prerequisites.rawMotion = 4;
    E::MoveContext rejected; const Saved<E::MoveContext> unchanged(rejected);
    assert(!E::prepareMoveRelative(rejected, axis, nullptr, 42, request, prerequisites, 100, 100000)); unchanged.check(rejected);
    request.position.relative = false;
    assert(!E::prepareMoveAbsolute(rejected, axis, nullptr, 42, request, prerequisites, 100, 100000)); unchanged.check(rejected);
    prerequisites.rawMotion = 0;
    AxisReference moving; moving.nativeKnown = true; moving.stationary = false;
    assert(!E::prepareMoveAbsolute(rejected, axis, &moving, 42, request, prerequisites, 100, 100000)); unchanged.check(rejected);
    prerequisites.rawMotion = 4;
    request.position.wrapped = true;
    assert(!E::prepareMoveAngle(rejected, axis, nullptr, 42, request, prerequisites, 100, 100000)); unchanged.check(rejected);
}
} // namespace
int main() {
    rawFramesAndBounds(); admittedNativeSnapshot(); rejectedAdmission(); deadlineBoundaries();
    envelopesAndReplyEvidence(); cancellationAndLostAcknowledgement(); observedOriginalIsIndependent();
}
