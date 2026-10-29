// SPDX-License-Identifier: MIT
// Public typed reads, using independent literal FC03 requests/replies.
#include <MotorControlRS/profiles/ess_rs/Reads.h>
#include <cassert>
#include <cstring>
#include <limits>
#include <initializer_list>

using namespace MotorControlRS;
using namespace MotorControlRS::ESS_RS;
namespace {
const uint8_t IDENTITY[] = {1, 3, 8, 0x4E, 0xEA, 0x12, 0x34, 0, 1, 0xA5, 0x81, 3, 0xE3};
const uint8_t DIRECTION[] = {1, 3, 4, 0, 1, 6, 0x40, 0xA9, 0xA3};
const uint8_t SERIAL[] = {1, 3, 6, 0, 42, 0, 2, 0, 3, 0xD9, 0x72};
const uint8_t ORDER[] = {1, 3, 6, 0, 1, 0, 1, 0, 1, 0x8C, 0xB5};
const uint8_t INPUTS[] = {1, 3, 10, 0xA5, 0xF5, 0, 0, 0, 1, 0, 2, 0, 17, 0x55, 0x7C};
const uint8_t ENCODER[] = {1, 3, 4, 0, 2, 0x10, 0, 0x56, 0x33};
const uint8_t EXCEPTION[] = {1, 0x83, 0xE7, 1, 0x7A};
struct Fixture { uint16_t first, count; const uint8_t* frame; std::size_t length; uint8_t request[8]; };
const Fixture CONFIG[] = {
    {0x10, 2, DIRECTION, sizeof(DIRECTION), {1, 3, 0, 0x10, 0, 2, 0xC5, 0xCE}},
    {0x13, 3, SERIAL, sizeof(SERIAL), {1, 3, 0, 0x13, 0, 3, 0xF4, 0x0E}},
    {0x17, 3, ORDER, sizeof(ORDER), {1, 3, 0, 0x17, 0, 3, 0xB5, 0xCF}},
    {0x40, 5, INPUTS, sizeof(INPUTS), {1, 3, 0, 0x40, 0, 5, 0x84, 0x1D}},
    {0x100, 2, ENCODER, sizeof(ENCODER), {1, 3, 1, 0, 0, 2, 0xC5, 0xF7}}
};
ReadTarget target() { ReadTarget t; t.id = 7; t.address = 1; t.generation = 9; return t; }
ActiveSerialTuple active() {
    ActiveSerialTuple t; t.known = true; t.baud = 115200; t.dataBits = 8;
    t.parity = SerialParity::NONE; t.stopBits = 1; return t;
}
template<class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof(T)); }
    void check(const T& value) const { assert(std::memcmp(bytes, &value, sizeof(T)) == 0); }
};
ReadContext identity(uint64_t deadline = 1000) {
    ReadContext c; assert(prepareIdentity(c, target(), 12, 100, deadline, active())); return c;
}
ReadContext config(uint64_t deadline = 1000, const InputWiring* wiring = nullptr) {
    ReadContext c; assert(prepareConfig(c, target(), 12, 100, deadline, active(), wiring)); return c;
}
ReadEvent frame(const ReadContext& c, const uint8_t* bytes, std::size_t length,
                uint64_t earliest = 200, uint64_t latest = 220) {
    ReadEvent e; e.target = c.target; e.operationId = c.operationId; e.step = c.step;
    e.frame = bytes; e.length = length; e.qualified = true; e.txAccepted = 8;
    e.earliestUs = earliest; e.latestUs = latest; return e;
}
ReadEvent control(const ReadContext& c, ReadEventKind kind) {
    ReadEvent e; e.target = c.target; e.operationId = c.operationId; e.step = c.step; e.kind = kind; return e;
}
void consumeStep(ReadContext& c, unsigned index) {
    const uint64_t earliest = 200 + index * 100;
    assert(advanceRead(c, frame(c, CONFIG[index].frame, CONFIG[index].length, earliest, earliest + 20), earliest + 30));
}
ConfigObservation completedConfig() {
    ReadContext c = config(); for (unsigned i = 0; i < 5; ++i) consumeStep(c, i);
    ConfigObservation out; assert(getConfig(c, out)); return out;
}
// Independent CRC fixture generator is used only when testing future/raw codes.
void seal(uint8_t* bytes, std::size_t size) {
    uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < size - 2; ++i) {
        crc ^= bytes[i];
        for (unsigned b = 0; b < 8; ++b) crc = (crc >> 1) ^ ((crc & 1) ? 0xA001 : 0);
    }
    bytes[size - 2] = static_cast<uint8_t>(crc); bytes[size - 1] = static_cast<uint8_t>(crc >> 8);
}
void testIdentityAndOwnedProvenance() {
    ReadContext c = identity(); PreparedRead r;
    assert(nextRead(c, 100, r));
    const uint8_t expected[] = {1, 3, 0, 0, 0, 4, 0x44, 9};
    assert(r.first == 0 && r.count == 4 && r.length == 8 && r.deadlineUs == 1000);
    assert(r.operationId == 12 && r.step == 0 && r.target.id == 7 && r.target.generation == 9);
    assert(std::memcmp(r.bytes, expected, 8) == 0);
    const Saved<ReadContext> prepared(c); PreparedRead repeated;
    assert(nextRead(c, 150, repeated)); prepared.check(c);
    assert(std::memcmp(repeated.bytes, r.bytes, 8) == 0);
    uint8_t borrowed[sizeof(IDENTITY)]; std::memcpy(borrowed, IDENTITY, sizeof(borrowed));
    assert(advanceRead(c, frame(c, borrowed, sizeof(borrowed)), 230));
    std::memset(borrowed, 0, sizeof(borrowed)); // Context cannot retain caller frame memory.
    assert(c.state == ReadState::SUCCEEDED && c.outcome == ReadOutcome::SUCCESS && c.completedSteps == 1);
    IdentityObservation out; assert(getIdentity(c, out));
    assert(out.rawModel == 0x4EEA && out.rawVersion == 0x1234 && out.rawActiveNode == 1 && out.rawDip == 0xA581);
    assert(out.activeNodeKnown && out.activeNode == 1);
    assert(out.modelResolution == ReadResolution::MODEL_MAPPING_UNRESOLVED);
    assert(out.versionResolution == ReadResolution::VERSION_MAPPING_UNRESOLVED);
    assert(out.dipResolution == ReadResolution::DIP_MAPPING_UNRESOLVED);
    assert(out.activeSerial.known && out.activeSerial.baud == 115200);
    assert(out.provenance.length == sizeof(IDENTITY) && std::memcmp(out.provenance.raw, IDENTITY, sizeof(IDENTITY)) == 0);
    assert(out.provenance.earliestUs == 200 && out.provenance.latestUs == 220 && out.provenance.deliveredUs == 230);
    assert(out.provenance.txAccepted == 8 && !out.provenance.executionUnknown);
    const Saved<PreparedRead> noNext(r); assert(!nextRead(c, 240, r)); noNext.check(r);
    const Saved<ReadContext> terminal(c); assert(!advanceRead(c, frame(c, IDENTITY, sizeof(IDENTITY)), 240)); terminal.check(c);
}
void testConfigWindowsPublicationAndIndependentEvidence() {
    const InputWiring declaration[] = {InputWiring::UNCONNECTED, InputWiring::CONNECTED, InputWiring::UNKNOWN, InputWiring::UNCONNECTED};
    InputWiring borrowed[4]; std::memcpy(borrowed, declaration, sizeof(borrowed));
    ReadContext c = config(1000, borrowed); borrowed[0] = InputWiring::CONNECTED;
    ConfigObservation prior = completedConfig(); prior.raw.subdivision = 9999;
    const Saved<ConfigObservation> previous(prior);
    for (unsigned i = 0; i < 5; ++i) {
        PreparedRead r; assert(nextRead(c, c.servicedUs, r));
        assert(r.first == CONFIG[i].first && r.count == CONFIG[i].count && r.step == i && r.length == 8);
        assert(std::memcmp(r.bytes, CONFIG[i].request, 8) == 0 && isReadRangeValid(r.first, r.count));
        assert(r.count <= MAX_READ_REGISTERS && r.bytes[1] == 3 && r.deadlineUs == 1000);
        assert(!getConfig(c, prior)); previous.check(prior);
        consumeStep(c, i);
    }
    assert(c.state == ReadState::SUCCEEDED && c.completedSteps == 5 && getConfig(c, prior));
    assert(prior.raw.direction == 1 && prior.raw.subdivision == 1600 && prior.raw.customNode == 42);
    assert(prior.directionKnown && prior.direction == DefaultDirection::REVERSED);
    assert(prior.baudKnown && prior.baud == BaudRateCode::BAUD_19200 && prior.format == SerialFormatCode::FORMAT_8O1);
    assert(prior.activeSerial.baud == 115200 && prior.activeSerial.parity == SerialParity::NONE);
    assert(prior.raw.wordOrder == 1 && prior.wordOrderKnown && prior.wordOrder == WordOrder::LOW_WORD_FIRST);
    assert(prior.raw.encoderResolution == 4096 && prior.algorithmKnown && prior.algorithm == ControlAlgorithm::ALGORITHM_1);
    assert(prior.unknownPolarityBits == 0xA5F0);
    for (unsigned i = 0; i < 4; ++i) {
        assert(prior.wiring[i] == declaration[i] && !prior.inputLevelKnown[i]);
        assert(prior.inputFunctionKnown[i]); assert(prior.inputInverted[i] == (i == 0 || i == 2));
    }
    assert(prior.raw.inputFunctions[0] == 0); // An unconnected declaration did not rewrite an assignment.
    assert(prior.raw.inputFunctions[1] == 1 && prior.raw.inputFunctions[2] == 2 && prior.raw.inputFunctions[3] == 17);
    assert(prior.subdivisionResolution == ReadResolution::SCALE_UNRESOLVED);
    assert(prior.units.commandStepsPerMotorTurn.numerator == 0 && prior.units.commandStepsPerMotorTurn.source == ScaleSource::UNKNOWN);
    assert(prior.units.encoder.countsPerUnit.numerator == 4096 && prior.units.encoder.countsPerUnit.source == ScaleSource::READBACK);
    for (unsigned i = 0; i < 5; ++i) {
        assert(prior.provenance[i].first == CONFIG[i].first && prior.provenance[i].count == CONFIG[i].count);
        assert(prior.provenance[i].length == CONFIG[i].length);
        assert(std::memcmp(prior.provenance[i].raw, CONFIG[i].frame, CONFIG[i].length) == 0);
    }
    assert(!isReadRangeValid(0x10, 10) && !isReadRangeValid(0x13, 7)); // Documented gaps are never coalesced.
}
void testPreparationAndPublicationFailuresPreserveOutputs() {
    ReadContext out = identity(); const Saved<ReadContext> original(out);
    ReadTarget t = target();
    for (unsigned i = 0; i < 5; ++i) {
        t = target();
        if (i == 0) t.id = 0; else if (i == 1) t.address = 0; else if (i == 2) t.address = 248;
        else if (i == 3) t.generation = 0; else t.address = 255;
        assert(!prepareIdentity(out, t, 12, 100, 1000)); original.check(out);
        assert(!prepareConfig(out, t, 12, 100, 1000)); original.check(out);
    }
    assert(!prepareIdentity(out, target(), 0, 100, 1000)); original.check(out);
    assert(!prepareIdentity(out, target(), 12, 100, 100)); original.check(out);
    assert(!prepareConfig(out, target(), 12, 100, 99)); original.check(out);
    ActiveSerialTuple bad = active(); bad.baud = 0;
    assert(!prepareIdentity(out, target(), 12, 100, 1000, bad)); original.check(out);
    InputWiring wiring[4] = {}; wiring[2] = static_cast<InputWiring>(99);
    assert(!prepareConfig(out, target(), 12, 100, 1000, active(), wiring)); original.check(out);
    IdentityObservation identityOut; identityOut.rawModel = 0xA55A;
    const Saved<IdentityObservation> oldIdentity(identityOut);
    assert(!getIdentity(out, identityOut)); oldIdentity.check(identityOut);
    ConfigObservation configOut; configOut.raw.subdivision = 0xA55A;
    const Saved<ConfigObservation> oldConfig(configOut);
    assert(advanceRead(out, frame(out, IDENTITY, sizeof(IDENTITY)), 230));
    assert(!getConfig(out, configOut)); oldConfig.check(configOut);
    ReadContext empty; PreparedRead prepared;
    prepared.first = 0xA55A; const Saved<PreparedRead> oldPrepared(prepared);
    assert(!nextRead(empty, 100, prepared)); oldPrepared.check(prepared);
}
void testWrongEnvelopesAndSameLengthStepsRejectWithoutMutation() {
    ReadContext c = config();
    for (unsigned i = 0; i < 9; ++i) {
        ReadEvent e = frame(c, DIRECTION, sizeof(DIRECTION));
        if (i == 0) ++e.operationId; else if (i == 1) ++e.target.id;
        else if (i == 2) ++e.target.address; else if (i == 3) ++e.target.generation;
        else if (i == 4) ++e.step; else if (i == 5) e.kind = static_cast<ReadEventKind>(99);
        else if (i == 6) e.latestUs = e.earliestUs - 1;
        else if (i == 7) { e.frame = nullptr; e.length = sizeof(DIRECTION); }
        else { e.kind = ReadEventKind::CANCEL; }
        const Saved<ReadContext> unchanged(c);
        assert(!advanceRead(c, e, 230)); unchanged.check(c);
    }
    consumeStep(c, 0); consumeStep(c, 1);
    ReadEvent duplicate = frame(c, SERIAL, sizeof(SERIAL), 400, 420); duplicate.step = 1;
    const Saved<ReadContext> unchanged(c);
    assert(!advanceRead(c, duplicate, 430)); unchanged.check(c); // Same count as current order window.
    PreparedRead next; assert(nextRead(c, 430, next)); assert(next.first == 0x17 && next.count == 3);
    consumeStep(c, 2); consumeStep(c, 3);
    ReadEvent oldDirection = frame(c, DIRECTION, sizeof(DIRECTION), 600, 620); oldDirection.step = 0;
    const Saved<ReadContext> noMix(c);
    assert(!advanceRead(c, oldDirection, 630)); noMix.check(c); // Same count as current algorithm window.
    consumeStep(c, 4); assert(c.state == ReadState::SUCCEEDED);
}
void testPartialFailuresPreservePreviousObservation() {
    for (unsigned failedStep = 0; failedStep < 5; ++failedStep) {
        for (unsigned failure = 0; failure < 4; ++failure) {
            ReadContext c = config(); for (unsigned i = 0; i < failedStep; ++i) consumeStep(c, i);
            unsigned char earlierEvidence[sizeof(c.observations)];
            std::memcpy(earlierEvidence, c.observations, sizeof(c.observations));
            ConfigObservation previous = completedConfig(); previous.raw.subdivision = 9999;
            const Saved<ConfigObservation> saved(previous);
            uint8_t corrupt[READ_MAX_REPLY_BYTES]; std::memcpy(corrupt, CONFIG[failedStep].frame, CONFIG[failedStep].length);
            corrupt[CONFIG[failedStep].length - 1] ^= 1;
            const uint64_t earliest = 200 + failedStep * 100;
            ReadEvent event = failure == 0 ? frame(c, corrupt, CONFIG[failedStep].length, earliest, earliest + 20) :
                failure == 1 ? frame(c, EXCEPTION, sizeof(EXCEPTION), earliest, earliest + 20) :
                control(c, failure == 2 ? ReadEventKind::TRANSPORT_FAILURE : ReadEventKind::CANCEL);
            assert(advanceRead(c, event, earliest + 30));
            assert(c.state == ReadState::FAILED && c.completedSteps == failedStep);
            assert(c.outcome == (failure < 2 ? ReadOutcome::REPLY_ERROR : failure == 2 ? ReadOutcome::TRANSPORT_ERROR : ReadOutcome::CANCELLED));
            if (failure == 0) assert(c.status.code == Err::CRC_ERROR);
            if (failure == 1) assert(c.status.code == Err::EXCEPTION && c.status.detail == 0xE7);
            assert(!getConfig(c, previous)); saved.check(previous);
            PreparedRead noRetry; const Saved<PreparedRead> noWork(noRetry);
            assert(!nextRead(c, earliest + 40, noRetry)); noWork.check(noRetry);
            const Saved<ReadContext> terminal(c); assert(!advanceRead(c, event, earliest + 40)); terminal.check(c);
            assert(std::memcmp(earlierEvidence, c.observations,
                failedStep * sizeof(ReadStepObservation)) == 0);
            if (failure < 2) {
                assert(c.observations[failedStep].length == event.length);
                assert(std::memcmp(c.observations[failedStep].raw, event.frame, event.length) == 0);
            }
        }
    }
}
void testDeadlineUsesQualifiedClosureAndNeverRenewsBudget() {
    ReadContext c = identity(); assert(advanceRead(c, frame(c, IDENTITY, sizeof(IDENTITY), 980, 1000), 5000));
    IdentityObservation out; assert(getIdentity(c, out));
    assert(out.provenance.latestUs == 1000 && out.provenance.deliveredUs == 5000 && c.deadlineUs == 1000);
    for (unsigned kind = 0; kind < 3; ++kind) {
        c = identity(); ReadEvent e = frame(c, IDENTITY, sizeof(IDENTITY), 990, 1001);
        if (kind == 1) { e.qualified = false; e.earliestUs = e.latestUs = 0; }
        else if (kind == 2) e = control(c, ReadEventKind::DEADLINE);
        assert(advanceRead(c, e, 5000)); assert(c.state == ReadState::FAILED);
        assert(c.outcome == (kind == 1 ? ReadOutcome::TIMING_UNQUALIFIED : ReadOutcome::DEADLINE));
    }
    c = config(); assert(advanceRead(c, frame(c, DIRECTION, sizeof(DIRECTION), 980, 990), 1000));
    assert(c.state == ReadState::FAILED && c.outcome == ReadOutcome::DEADLINE);
    assert(c.completedSteps == 1 && c.observations[0].latestUs == 990 && c.deadlineUs == 1000);
    c = config(); PreparedRead pending; pending.first = 0xA55A; const Saved<PreparedRead> saved(pending);
    assert(!nextRead(c, 1000, pending)); saved.check(pending); assert(c.state == ReadState::ACTIVE);
    assert(advanceRead(c, control(c, ReadEventKind::DEADLINE), 1000)); assert(c.state == ReadState::FAILED);
    c = config(); for (unsigned i = 0; i < 4; ++i) consumeStep(c, i);
    assert(advanceRead(c, frame(c, ENCODER, sizeof(ENCODER), 980, 999), 5000));
    ConfigObservation configOut; assert(getConfig(c, configOut)); assert(configOut.provenance[4].deliveredUs == 5000);
}
void testUnknownEnumsAndZeroEncoderRemainRawEvidence() {
    ReadContext c = config();
    for (unsigned i = 0; i < 5; ++i) {
        uint8_t raw[READ_MAX_REPLY_BYTES]; std::memcpy(raw, CONFIG[i].frame, CONFIG[i].length);
        for (unsigned word = 0; word < CONFIG[i].count; ++word) { raw[3 + 2 * word] = 0xFF; raw[4 + 2 * word] = 0xFF; }
        if (i == 4) raw[5] = raw[6] = 0; // Zero is a read value, not a usable scale.
        seal(raw, CONFIG[i].length);
        assert(advanceRead(c, frame(c, raw, CONFIG[i].length, 200 + i * 100, 220 + i * 100), 230 + i * 100));
    }
    ConfigObservation out; assert(getConfig(c, out));
    assert(!out.directionKnown && !out.baudKnown && !out.formatKnown && !out.wordOrderKnown);
    assert(!out.overLimitStopKnown && !out.softLimitEnableKnown && !out.algorithmKnown);
    assert(out.raw.direction == 0xFFFF && out.raw.subdivision == 0xFFFF && out.raw.customNode == 0xFFFF);
    assert(out.raw.wordOrder == 0xFFFF && out.raw.algorithm == 0xFFFF && out.raw.encoderResolution == 0);
    for (unsigned i = 0; i < 4; ++i) assert(!out.inputFunctionKnown[i] && out.raw.inputFunctions[i] == 0xFFFF && !out.inputLevelKnown[i]);
    assert(out.unknownPolarityBits == 0xFFF0 && out.encoderResolution == ReadResolution::ZERO_ENCODER_SCALE);
    assert(out.units.commandStepsPerMotorTurn.numerator == 0 && out.units.encoder.countsPerUnit.numerator == 0);
    UnitConversion converted(99, 88); const Saved<UnitConversion> unchanged(converted);
    assert(!convertDisplacement(1, PositionUnit::ENCODER_COUNTS, PositionUnit::TURNS, out.units, converted)); unchanged.check(converted);
}
void testMalformedRepliesRetainFailureWithoutPublishing() {
    for (unsigned failure = 0; failure < 6; ++failure) {
        ReadContext c = identity();
        uint8_t reply[40] = {}; std::memcpy(reply, IDENTITY, sizeof(IDENTITY));
        std::size_t length = sizeof(IDENTITY);
        FrameError expected = FrameError::NONE;
        if (failure == 0) { reply[0] = 2; seal(reply, length); expected = FrameError::ADDRESS; }
        else if (failure == 1) { reply[1] = 4; seal(reply, length); expected = FrameError::FUNCTION; }
        else if (failure == 2) { reply[2] = 6; seal(reply, length); expected = FrameError::BYTE_COUNT; }
        else if (failure == 3) { --length; expected = FrameError::LENGTH; }
        else if (failure == 4) { length = 0; expected = FrameError::LENGTH; }
        else { length = sizeof(reply); expected = FrameError::LENGTH; }
        IdentityObservation previous; previous.rawModel = 0xA55A; const Saved<IdentityObservation> unchanged(previous);
        assert(advanceRead(c, frame(c, reply, length), 230));
        assert(c.state == ReadState::FAILED && c.outcome == ReadOutcome::REPLY_ERROR && c.completedSteps == 0);
        assert(c.observations[0].frameError == expected && c.observations[0].receivedLength == length);
        assert(c.observations[0].length == (length > READ_MAX_REPLY_BYTES ? READ_MAX_REPLY_BYTES : length));
        assert(!getIdentity(c, previous)); unchanged.check(previous);
    }
    ReadContext c = config(); consumeStep(c, 0);
    ReadEvent impossible = frame(c, SERIAL, sizeof(SERIAL), 150, 180);
    const Saved<ReadContext> unchanged(c);
    assert(!advanceRead(c, impossible, 330)); unchanged.check(c);
    PreparedRead next; const Saved<PreparedRead> unchangedRequest(next);
    assert(!nextRead(c, c.servicedUs - 1, next)); unchangedRequest.check(next);
    ReadEvent earlierDelivery = frame(c, SERIAL, sizeof(SERIAL), 240, 250);
    assert(!advanceRead(c, earlierDelivery, c.servicedUs - 1)); unchanged.check(c);
    ReadEvent transport = control(c, ReadEventKind::TRANSPORT_FAILURE); transport.transportDetail = -77;
    assert(advanceRead(c, transport, 330));
    assert(c.outcome == ReadOutcome::TRANSPORT_ERROR && c.observations[1].transportDetail == -77);
}
void testTransportProvenanceIsBoundedOwnedAndNotAcknowledgement() {
    ReadContext c = config(); consumeStep(c, 0);
    uint8_t prefix[40]; for (unsigned i = 0; i < sizeof(prefix); ++i) prefix[i] = static_cast<uint8_t>(i + 1);
    ReadEvent failed = control(c, ReadEventKind::TRANSPORT_FAILURE);
    failed.frame = prefix; failed.length = sizeof(prefix); failed.transportDetail = -123;
    failed.txAccepted = 5; failed.executionUnknown = true;
    ConfigObservation previous = completedConfig(); const Saved<ConfigObservation> retained(previous);
    assert(advanceRead(c, failed, 330));
    assert(c.state == ReadState::FAILED && c.outcome == ReadOutcome::TRANSPORT_ERROR && c.completedSteps == 1);
    const ReadStepObservation& evidence = c.observations[1];
    assert(evidence.receivedLength == 40 && evidence.length == 37 && evidence.transportDetail == -123);
    assert(evidence.txAccepted == 5 && evidence.executionUnknown && !evidence.qualified);
    std::memset(prefix, 0, sizeof(prefix));
    for (unsigned i = 0; i < evidence.length; ++i) assert(evidence.raw[i] == i + 1);
    assert(!getConfig(c, previous)); retained.check(previous);
    for (unsigned kind = 0; kind < 2; ++kind) {
        c = identity(); ReadEvent local = control(c, kind ? ReadEventKind::DEADLINE : ReadEventKind::CANCEL);
        local.txAccepted = 8; local.executionUnknown = true; local.transportDetail = 17;
        uint8_t partial[] = {1, 3, 8}; local.frame = partial; local.length = sizeof(partial);
        assert(advanceRead(c, local, kind ? 1000 : 230));
        assert(c.state == ReadState::FAILED && c.observations[0].txAccepted == 8 && c.observations[0].executionUnknown);
        assert(c.observations[0].transportDetail == 17 && c.observations[0].length == 3 && c.observations[0].receivedLength == 3);
        std::memset(partial, 0, sizeof(partial));
        assert(c.observations[0].raw[0] == 1 && c.observations[0].raw[1] == 3 && c.observations[0].raw[2] == 8);
        assert(!c.observations[0].qualified && c.observations[0].earliestUs == 0 && c.observations[0].latestUs == 0);
    }
    c = identity(); ReadEvent impossible = frame(c, IDENTITY, sizeof(IDENTITY)); impossible.txAccepted = 9;
    const Saved<ReadContext> unchanged(c); assert(!advanceRead(c, impossible, 230)); unchanged.check(c);
    impossible.txAccepted = 8; impossible.transportDetail = 99;
    assert(advanceRead(c, impossible, 230));
    assert(c.state == ReadState::SUCCEEDED && c.outcome == ReadOutcome::SUCCESS);
    assert(c.observations[0].txAccepted == 8 && c.observations[0].transportDetail == 99);
}
void testUnknownIdentityAndBoundaryTargetsArePreserved() {
    ReadContext c = identity(); uint8_t future[sizeof(IDENTITY)]; std::memcpy(future, IDENTITY, sizeof(future));
    std::memset(future + 3, 0xFF, 8); seal(future, sizeof(future));
    assert(advanceRead(c, frame(c, future, sizeof(future)), 230));
    IdentityObservation out; assert(getIdentity(c, out));
    assert(out.rawModel == 0xFFFF && out.rawVersion == 0xFFFF && out.rawDip == 0xFFFF);
    assert(out.rawActiveNode == 0xFFFF && !out.activeNodeKnown);
    ReadTarget boundary = target(); boundary.id = boundary.generation = std::numeric_limits<uint32_t>::max(); boundary.address = 247;
    assert(prepareIdentity(c, boundary, std::numeric_limits<uint32_t>::max(), 100, 1000));
    PreparedRead r; assert(nextRead(c, 100, r)); assert(r.bytes[0] == 247 && r.target.address == 247);
    assert(r.target.id == boundary.id && r.target.generation == boundary.generation && r.operationId == boundary.id);
    uint8_t reply[sizeof(IDENTITY)]; std::memcpy(reply, IDENTITY, sizeof(reply)); reply[0] = 247; seal(reply, sizeof(reply));
    assert(advanceRead(c, frame(c, reply, sizeof(reply)), 230) && getIdentity(c, out));
    assert(out.target.address == 247 && out.rawActiveNode == 1); // Reported active-node register remains independent evidence.
}
const uint8_t MOTION_REPLY[] = {1,3,4,0,5,0xAB,0x7F,0xD4,0xE2};
const uint8_t IO_REPLY[] = {1,3,4,0xCA,0x05,0xF0,0x02,0x10,0x2B};
const uint8_t FEEDBACK_REPLY[] = {1,3,6,0x12,0x34,0xAB,0xCD,0xFE,0xDC,0x63,0xE1};
const Fixture STATE[] = {
    {6,2,MOTION_REPLY,sizeof(MOTION_REPLY),{1,3,0,6,0,2,0x24,0x0A}},
    {8,2,IO_REPLY,sizeof(IO_REPLY),{1,3,0,8,0,2,0x45,0xC9}},
    {10,3,FEEDBACK_REPLY,sizeof(FEEDBACK_REPLY),{1,3,0,10,0,3,0x25,0xC9}}
};
ReadContext stateRead(const ConfigObservation* config = nullptr) {
    ReadContext context; assert(prepareState(context,target(),12,100,1000,active(),config)); return context;
}
void stateStep(ReadContext& context, uint8_t index) {
    const uint64_t earliest = 200 + index * 100;
    uint8_t bytes[11]; std::memcpy(bytes, STATE[index].frame, STATE[index].length);
    assert(advanceRead(context,frame(context,bytes,STATE[index].length,earliest,earliest+20),earliest+30));
}
void testStateWindowsBitfieldsAndAbsentFeedback() {
    ReadContext context = stateRead(); StateObservation previous;
    previous.rawAlarm = 0xA55A; const Saved<StateObservation> unchanged(previous);
    assert(!getStateBlock(context,0,previous)); unchanged.check(previous);
    for (uint8_t i=0;i<3;++i) {
        PreparedRead request; assert(nextRead(context,context.servicedUs,request));
        assert(request.first==STATE[i].first && request.count==STATE[i].count);
        assert(std::memcmp(request.bytes,STATE[i].request,8)==0);
        stateStep(context,i);
        assert(getStateBlock(context,i,previous));
        assert(previous.block==static_cast<StateBlock>(i) && previous.target.generation==9);
        assert(previous.provenance.attemptedUs==(i ? 130ULL+i*100 : 100ULL));
    }
    assert(context.state==ReadState::SUCCEEDED && context.completedSteps==3);
    StateObservation motion; assert(getStateBlock(context,0,motion));
    assert(motion.rawAlarm==5 && motion.alarmKnown && motion.alarm==AlarmCode::POSITION_TOLERANCE);
    assert(motion.rawMotion==0xAB7F && motion.unknownMotionBits==0xAB00);
    assert(motion.inPosition && motion.homingComplete && motion.running && motion.alarmFlag);
    assert(motion.released && !motion.enabled && motion.positiveSoftLimit && motion.negativeSoftLimit);
    StateObservation io; assert(getStateBlock(context,1,io));
    assert(io.rawInputs==0xCA05 && io.unknownInputBits==0xCA00);
    assert(io.inputs[0] && !io.inputs[1] && io.inputs[2] && !io.inputs[3]);
    assert(io.rawOutputs==0xF002 && io.unknownOutputBits==0xF000 && !io.outputs[0] && io.outputs[1]);
    StateObservation feedback; assert(getStateBlock(context,2,feedback));
    assert(feedback.rawPositionWords[0]==0x1234 && feedback.rawPositionWords[1]==0xABCD && feedback.rawSpeed==0xFEDC);
    assert(!feedback.pairKnown && feedback.positionSource==PositionSource::UNRESOLVED);
    assert(feedback.wordOrderResolution==ReadResolution::WORD_ORDER_UNRESOLVED);
    assert(feedback.speedUnitResolution==ReadResolution::UNIT_UNSPECIFIED);
    const Saved<StateObservation> saved(feedback); assert(!getStateBlock(context,3,feedback)); saved.check(feedback);
}
void testStateConfiguredPairsAndUnknownAlarms() {
    ConfigObservation config = completedConfig();
    for (unsigned order=0;order<2;++order) for(unsigned algorithm=1;algorithm<=2;++algorithm) {
        config.wordOrder = static_cast<WordOrder>(order); config.algorithm=static_cast<ControlAlgorithm>(algorithm);
        ReadContext context = stateRead(&config);
        config.wordOrder = WordOrder::LOW_WORD_FIRST; // preparation owns its snapshot
        for(uint8_t i=0;i<3;++i) stateStep(context,i);
        StateObservation feedback; assert(getStateBlock(context,2,feedback));
        assert(feedback.pairKnown && feedback.rawPosition==(order ? 0xABCD1234U : 0x1234ABCDU));
        assert(feedback.configOperationId==config.operationId && feedback.wordOrderResolution==ReadResolution::RESOLVED);
        assert(feedback.positionSource==(algorithm==1 ? PositionSource::COMMAND_GIVEN : PositionSource::SUBDIVISION_EQUIVALENT_FEEDBACK));
        assert(feedback.positionSignedResolution==ReadResolution::SIGNED_ENCODING_UNRESOLVED);
        assert(feedback.positionScaleResolution==ReadResolution::SCALE_UNRESOLVED);
    }
    for(const uint16_t alarm : {uint16_t(0),uint16_t(1),uint16_t(2),uint16_t(3),uint16_t(4),uint16_t(5),uint16_t(0xBEEF)}) {
        ReadContext context=stateRead(); uint8_t bytes[9]={1,3,4,static_cast<uint8_t>(alarm>>8),static_cast<uint8_t>(alarm),0,0,0,0};
        seal(bytes,sizeof(bytes)); assert(advanceRead(context,frame(context,bytes,sizeof(bytes)),230));
        StateObservation motion; assert(getStateBlock(context,0,motion));
        assert(motion.rawAlarm==alarm && motion.alarmKnown==(alarm<=3 || alarm==5));
        assert(motion.enabled && !motion.released && !motion.running); // polarity from original table
    }
    config.wordOrderKnown=false;config.algorithmKnown=false;
    ReadContext context=stateRead(&config);for(uint8_t i=0;i<3;++i)stateStep(context,i);
    StateObservation feedback;assert(getStateBlock(context,2,feedback));
    assert(!feedback.pairKnown && feedback.positionSource==PositionSource::UNRESOLVED);
}
void testStatePartialFailureAndConfigurationCorrelation() {
    StateObservation previous;previous.rawPosition=0xAABBCCDD;const Saved<StateObservation> saved(previous);
    for(uint8_t failing=0;failing<3;++failing) {
        ReadContext context=stateRead();for(uint8_t i=0;i<failing;++i)stateStep(context,i);
        const uint64_t time=200+failing*100;
        assert(advanceRead(context,frame(context,EXCEPTION,sizeof(EXCEPTION),time,time+20),time+30));
        assert(context.state==ReadState::FAILED && context.completedSteps==failing);
        assert(!getStateBlock(context,failing,previous));saved.check(previous);
        for(uint8_t i=0;i<failing;++i){StateObservation block;assert(getStateBlock(context,i,block));}
        ConfigObservation configOutput;assert(!getConfig(context,configOutput)); // wrong kind cannot publish config
    }
    ConfigObservation config=completedConfig();ReadContext context=stateRead();const Saved<ReadContext> unchanged(context);
    ++config.target.generation;assert(!prepareState(context,target(),13,100,1000,active(),&config));unchanged.check(context);
    config=completedConfig();++config.target.id;assert(!prepareState(context,target(),13,100,1000,active(),&config));unchanged.check(context);
    config=completedConfig();config.target.address=2;assert(!prepareState(context,target(),13,100,1000,active(),&config));unchanged.check(context);
    config=completedConfig();config.wordOrder=static_cast<WordOrder>(7);assert(!prepareState(context,target(),13,100,1000,active(),&config));unchanged.check(context);
    ReadEvent stale=frame(context,MOTION_REPLY,sizeof(MOTION_REPLY));++stale.target.generation;
    assert(!advanceRead(context,stale,230));unchanged.check(context);
}
// Function manual physical p70: one contiguous JOG window. Distinct words catch
// ordering mistakes. Ramp 0..2000 is a native range, not a physical conversion;
// the malformed speed range/default annotation does not settle speed encoding.
const uint8_t VELOCITY_PARAMETERS[] = {1,3,6,0,60,0,50,0,100,0xD1,0x54};
const uint8_t VELOCITY_UNKNOWN[] = {1,3,6,0xFF,0xFF,7,0xD0,7,0xD1,0xE3,0x8F};
const uint8_t VELOCITY_BOUNDARY[] = {1,3,6,0,0,0,0,7,0xD0,0x22,0xD9};
const uint8_t VELOCITY_MAX_RAW[] = {1,3,6,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x20,0xFA};
const uint8_t VELOCITY_ANNOTATED_DEFAULT[] = {1,3,6,0,120,0,50,0,50,0xA1,0x65};
ReadContext velocityParameters(uint64_t deadline = 1000) {
    ReadContext c;
    assert(prepareVelocityParameters(c,target(),12,23,100,deadline,active()));
    return c;
}
void testVelocityParameterWindowAndOwnedObservation() {
    ReadContext c = velocityParameters(); PreparedRead request;
    const uint8_t expected[] = {1,3,0,0x1D,0,3,0x95,0xCD};
    assert(nextRead(c,100,request));
    assert(c.kind == ReadKind::VELOCITY_PARAMETERS && c.configurationGeneration == 23);
    assert(request.first == 0x001D && request.count == 3 && request.length == 8);
    assert(request.bytes[1] == 3 && std::memcmp(request.bytes,expected,sizeof(expected)) == 0);
    assert(request.target.id == 7 && request.target.address == 1 && request.target.generation == 9);
    assert(request.operationId == 12 && request.step == 0 && request.deadlineUs == 1000);
    assert(expectedReadRegistersLen(request.count) == 11 && isReadRangeValid(0x001D,3));
    const Saved<ReadContext> prepared(c); PreparedRead repeated;
    assert(nextRead(c,150,repeated)); prepared.check(c);
    assert(std::memcmp(repeated.bytes,expected,sizeof(expected)) == 0);
    VelocityParametersObservation out; out.rawSpeed = 0xA55A;
    const Saved<VelocityParametersObservation> unchanged(out);
    assert(!getVelocityParameters(c,out)); unchanged.check(out);
    uint8_t borrowed[sizeof(VELOCITY_PARAMETERS)];
    std::memcpy(borrowed,VELOCITY_PARAMETERS,sizeof(borrowed));
    assert(advanceRead(c,frame(c,borrowed,sizeof(borrowed)),230));
    std::memset(borrowed,0,sizeof(borrowed));
    assert(c.state == ReadState::SUCCEEDED && c.outcome == ReadOutcome::SUCCESS && c.completedSteps == 1);
    assert(c.step == 1 && getVelocityParameters(c,out));
    assert(out.target.id == 7 && out.target.address == 1 && out.target.generation == 9);
    assert(out.operationId == 12 && out.configurationGeneration == 23 && c.configOperationId == 0);
    assert(out.rawSpeed == 60 && out.rawAccelerationTime == 50 && out.rawDecelerationTime == 100);
    assert(out.accelerationTimeInRange && out.decelerationTimeInRange);
    assert(out.speedSignedResolution == ReadResolution::SIGNED_ENCODING_UNRESOLVED);
    assert(out.speedScaleResolution == ReadResolution::SCALE_UNRESOLVED);
    assert(out.rampScaleResolution == ReadResolution::SCALE_UNRESOLVED);
    assert(out.provenance.first == 0x001D && out.provenance.count == 3 && out.provenance.length == 11);
    assert(out.provenance.qualified && out.provenance.txAccepted == 8 && !out.provenance.executionUnknown);
    assert(out.provenance.earliestUs == 200 && out.provenance.latestUs == 220 && out.provenance.deliveredUs == 230);
    assert(std::memcmp(out.provenance.raw,VELOCITY_PARAMETERS,sizeof(VELOCITY_PARAMETERS)) == 0);
    std::memset(c.observations[0].raw,0,sizeof(c.observations[0].raw));
    assert(std::memcmp(out.provenance.raw,VELOCITY_PARAMETERS,sizeof(VELOCITY_PARAMETERS)) == 0);
    const Saved<PreparedRead> noWork(request);
    assert(!nextRead(c,240,request)); noWork.check(request); // No second read or write.
}
void testVelocityParameterUnknownAndNativeBoundaryWords() {
    struct RawCase { const uint8_t* reply; uint16_t speed, acceleration, deceleration; bool accelerationInRange, decelerationInRange; };
    const RawCase cases[] = {
        {VELOCITY_UNKNOWN,0xFFFF,2000,2001,true,false},
        {VELOCITY_BOUNDARY,0,0,2000,true,true},
        {VELOCITY_MAX_RAW,0xFFFF,0xFFFF,0xFFFF,false,false},
        {VELOCITY_ANNOTATED_DEFAULT,120,50,50,true,true}
    };
    for (const auto& raw : cases) {
        ReadContext c = velocityParameters();
        assert(advanceRead(c,frame(c,raw.reply,11),230));
        VelocityParametersObservation out; assert(getVelocityParameters(c,out));
        assert(out.rawSpeed == raw.speed && out.rawAccelerationTime == raw.acceleration && out.rawDecelerationTime == raw.deceleration);
        assert(out.accelerationTimeInRange == raw.accelerationInRange && out.decelerationTimeInRange == raw.decelerationInRange);
        assert(out.speedSignedResolution == ReadResolution::SIGNED_ENCODING_UNRESOLVED);
        assert(out.speedScaleResolution == ReadResolution::SCALE_UNRESOLVED);
        assert(out.rampScaleResolution == ReadResolution::SCALE_UNRESOLVED);
        assert(c.outcome == ReadOutcome::SUCCESS); // Unknown raw values are observations, not motion permission.
    }
}
void testVelocityParameterPreparationAndGetterPreservation() {
    ReadContext output = velocityParameters(); const Saved<ReadContext> unchanged(output);
    for (unsigned i = 0; i < 7; ++i) {
        ReadTarget selected = target();
        if (i == 0) selected.id = 0;
        else if (i == 1) selected.address = 0;
        else if (i == 2) selected.address = 248;
        else if (i == 3) selected.generation = 0;
        assert(!prepareVelocityParameters(output,selected,i == 4 ? 0 : 12,
            i == 5 ? 0 : 23,100,i == 6 ? 100 : 1000,active()));
        unchanged.check(output);
    }
    ActiveSerialTuple bad = active(); bad.stopBits = 3;
    assert(!prepareVelocityParameters(output,target(),12,23,100,1000,bad)); unchanged.check(output);
    assert(!prepareVelocityParameters(output,target(),12,23,100,99)); unchanged.check(output);
    VelocityParametersObservation prior; prior.rawSpeed = 0xA55A;
    const Saved<VelocityParametersObservation> saved(prior);
    ReadContext wrong = identity();
    assert(advanceRead(wrong,frame(wrong,IDENTITY,sizeof(IDENTITY)),230));
    assert(!getVelocityParameters(wrong,prior)); saved.check(prior);
    assert(!getVelocityParameters(output,prior)); saved.check(prior);
    assert(prepareVelocityParameters(output,target(),12,std::numeric_limits<uint32_t>::max(),100,1000));
    assert(advanceRead(output,frame(output,VELOCITY_PARAMETERS,sizeof(VELOCITY_PARAMETERS)),230));
    assert(getVelocityParameters(output,prior) && prior.configurationGeneration == std::numeric_limits<uint32_t>::max());
    ConfigObservation configOutput; configOutput.raw.subdivision = 0xA55A;
    const Saved<ConfigObservation> savedConfig(configOutput);
    assert(!getConfig(output,configOutput)); savedConfig.check(configOutput);
}
void testVelocityParameterMalformedRepliesAndException() {
    const uint8_t wrongAddress[] = {2,3,6,0,60,0,50,0,100,0xC5,0xA4};
    const uint8_t wrongFunction[] = {1,4,6,0,60,0,50,0,100,0x90,0xB2};
    const uint8_t shortCount[] = {1,3,4,0,60,0,50,0xBB,0xEA};
    const uint8_t longCount[] = {1,3,8,0,60,0,50,0,100,0,1,0xD0,0x0F};
    const uint8_t exception[] = {1,0x83,2,0xC0,0xF1};
    uint8_t badCount[11]; std::memcpy(badCount,VELOCITY_PARAMETERS,sizeof(badCount));
    badCount[2] = 4; seal(badCount,sizeof(badCount));
    uint8_t badCrc[11]; std::memcpy(badCrc,VELOCITY_PARAMETERS,sizeof(badCrc)); badCrc[10] ^= 1;
    struct Rejected { const uint8_t* frame; std::size_t length; FrameError error; };
    const Rejected cases[] = {
        {wrongAddress,sizeof(wrongAddress),FrameError::ADDRESS},
        {wrongFunction,sizeof(wrongFunction),FrameError::FUNCTION},
        {shortCount,sizeof(shortCount),FrameError::LENGTH},
        {longCount,sizeof(longCount),FrameError::LENGTH},
        {badCount,sizeof(badCount),FrameError::BYTE_COUNT},
        {badCrc,sizeof(badCrc),FrameError::CRC},
        {VELOCITY_PARAMETERS,sizeof(VELOCITY_PARAMETERS)-1,FrameError::LENGTH},
        {VELOCITY_PARAMETERS,0,FrameError::LENGTH},
        {exception,sizeof(exception),FrameError::EXCEPTION}
    };
    for (const auto& rejected : cases) {
        ReadContext c = velocityParameters();
        VelocityParametersObservation prior; prior.rawSpeed = 0xA55A;
        const Saved<VelocityParametersObservation> saved(prior);
        assert(advanceRead(c,frame(c,rejected.frame,rejected.length),230));
        assert(c.state == ReadState::FAILED && c.outcome == ReadOutcome::REPLY_ERROR && c.completedSteps == 0);
        assert(c.observations[0].frameError == rejected.error);
        assert(c.observations[0].receivedLength == rejected.length && c.observations[0].length == rejected.length);
        assert(std::memcmp(c.observations[0].raw,rejected.frame,rejected.length) == 0);
        if (rejected.error == FrameError::EXCEPTION) assert(c.status.code == Err::EXCEPTION && c.status.detail == 2);
        assert(!getVelocityParameters(c,prior)); saved.check(prior);
        PreparedRead noReplay; const Saved<PreparedRead> unchanged(noReplay);
        assert(!nextRead(c,240,noReplay)); unchanged.check(noReplay);
    }
}
void testVelocityParameterCorrelationDeadlineAndCancellation() {
    for (unsigned i = 0; i < 5; ++i) {
        ReadContext c = velocityParameters(); const Saved<ReadContext> unchanged(c);
        ReadEvent stale = frame(c,VELOCITY_PARAMETERS,sizeof(VELOCITY_PARAMETERS));
        if (i == 0) ++stale.target.id;
        else if (i == 1) ++stale.target.address;
        else if (i == 2) ++stale.target.generation;
        else if (i == 3) ++stale.operationId;
        else ++stale.step;
        assert(!advanceRead(c,stale,230)); unchanged.check(c);
    }
    ReadContext c = velocityParameters(); PreparedRead request;
    request.first = 0xA55A; const Saved<PreparedRead> noRequest(request);
    assert(!nextRead(c,1000,request)); noRequest.check(request);
    assert(advanceRead(c,frame(c,VELOCITY_PARAMETERS,sizeof(VELOCITY_PARAMETERS),980,1000),5000));
    VelocityParametersObservation out; assert(getVelocityParameters(c,out));
    assert(out.provenance.latestUs == 1000 && out.provenance.deliveredUs == 5000 && c.deadlineUs == 1000);
    for (unsigned i = 0; i < 5; ++i) {
        c = velocityParameters(); ReadEvent event = frame(c,VELOCITY_PARAMETERS,sizeof(VELOCITY_PARAMETERS),980,1001);
        if (i == 1) { event.qualified = false; event.earliestUs = event.latestUs = 0; }
        else if (i >= 2) event = control(c,i == 2 ? ReadEventKind::DEADLINE : i == 3 ? ReadEventKind::CANCEL : ReadEventKind::TRANSPORT_FAILURE);
        const Saved<VelocityParametersObservation> preserved(out);
        assert(advanceRead(c,event,i == 3 || i == 4 ? 230 : 1001));
        assert(c.state == ReadState::FAILED && c.completedSteps == 0 && c.deadlineUs == 1000);
        assert(c.outcome == (i == 1 ? ReadOutcome::TIMING_UNQUALIFIED : i == 3 ? ReadOutcome::CANCELLED :
            i == 4 ? ReadOutcome::TRANSPORT_ERROR : ReadOutcome::DEADLINE));
        assert(!getVelocityParameters(c,out)); preserved.check(out);
        assert(!nextRead(c,1100,request)); noRequest.check(request);
        const Saved<ReadContext> terminal(c);
        assert(!advanceRead(c,frame(c,VELOCITY_PARAMETERS,sizeof(VELOCITY_PARAMETERS)),1200)); terminal.check(c);
    }
}
// Function manual physical pp19/20/72/73: six homing records occupy seven
// contiguous words. Offset stays in address order, with unresolved sign/scale
// and configurable-word-order applicability (omitted from the p30 list).
const uint8_t HOMING_PARAMETERS[] = {1,3,14,0,7,0,24,0,60,0,30,0,100,0x12,0x34,0xAB,0xCD,0xEE,0x29};
const uint8_t HOMING_UNKNOWN[] = {1,3,14,0xFF,0xFF,0xFF,0xFC,0xF1,0x23,0xE2,0x34,0xD3,0x45,0xC4,0x56,0xB5,0x67,0x38,1};
const uint8_t HOMING_MAXIMUM[] = {1,3,14,0,11,0,35,0x0B,0xB8,1,0x2C,7,0xD0,0,0,0xFF,0xFF,0x9F,0x23};
const uint8_t HOMING_MAX_RAW[] = {1,3,14,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x11,0x95};
const uint8_t HOMING_ZERO_OFFSET[] = {1,3,14,0,7,0,35,0,60,0,30,0,100,0,0,0,0,0xCE,0x1E};
ReadContext homingParameters(uint64_t deadline = 1000) {
    ReadContext c; assert(prepareHomingParameters(c,target(),12,23,100,deadline,active())); return c;
}
void homingInterpretationUnresolved(const HomingParametersObservation& out) {
    assert(out.speedScaleResolution == ReadResolution::SCALE_UNRESOLVED);
    assert(out.rampScaleResolution == ReadResolution::SCALE_UNRESOLVED);
    assert(out.offsetWordOrderResolution == ReadResolution::WORD_ORDER_UNRESOLVED);
    assert(out.offsetSignedResolution == ReadResolution::SIGNED_ENCODING_UNRESOLVED);
    assert(out.offsetScaleResolution == ReadResolution::SCALE_UNRESOLVED);
}
void testHomingParameterWindowSevenWordsAndOwnedEvidence() {
    ReadContext c = homingParameters(); PreparedRead request;
    const uint8_t expected[] = {1,3,0,0x30,0,7,4,7};
    assert(nextRead(c,100,request));
    assert(c.kind == ReadKind::HOMING_PARAMETERS && c.configurationGeneration == 23);
    assert(request.first == 0x0030 && request.count == 7 && request.length == 8 && request.step == 0);
    assert(request.target.id == 7 && request.target.address == 1 && request.target.generation == 9);
    assert(request.operationId == 12 && request.deadlineUs == 1000 && request.bytes[1] == 3);
    assert(std::memcmp(request.bytes,expected,sizeof(expected)) == 0);
    assert(expectedReadRegistersLen(7) == 19 && isReadRangeValid(0x0030,7));
    const Saved<ReadContext> unchanged(c); PreparedRead repeated;
    assert(nextRead(c,150,repeated)); unchanged.check(c);
    assert(std::memcmp(repeated.bytes,expected,sizeof(expected)) == 0);
    HomingParametersObservation out; out.rawSearchSpeed = 0xA55A;
    const Saved<HomingParametersObservation> unpublished(out);
    assert(!getHomingParameters(c,out)); unpublished.check(out);
    uint8_t borrowed[sizeof(HOMING_PARAMETERS)]; std::memcpy(borrowed,HOMING_PARAMETERS,sizeof(borrowed));
    assert(advanceRead(c,frame(c,borrowed,sizeof(borrowed)),230));
    std::memset(borrowed,0,sizeof(borrowed));
    assert(c.state == ReadState::SUCCEEDED && c.outcome == ReadOutcome::SUCCESS && c.completedSteps == 1 && c.step == 1);
    assert(getHomingParameters(c,out));
    assert(out.target.id == 7 && out.target.address == 1 && out.target.generation == 9);
    assert(out.operationId == 12 && out.configurationGeneration == 23 && c.configOperationId == 0);
    assert(out.rawAuxiliary == 7 && out.rawMethod == 24 && out.rawSearchSpeed == 60 && out.rawReturnSpeed == 30);
    assert(out.rawRampTime == 100 && out.rawOffsetWords[0] == 0x1234 && out.rawOffsetWords[1] == 0xABCD);
    assert(out.auxiliaryKnown && out.auxiliary == HomingAuxiliary::KEEP_POSITION_SET_ZERO);
    assert(out.methodKnown && out.method == HomingMethod::METHOD_24 && out.methodSupport == HomeSupport::UNIMPLEMENTED);
    assert(out.searchSpeedInRange && out.returnSpeedInRange && out.rampTimeInRange);
    homingInterpretationUnresolved(out);
    assert(out.provenance.first == 0x0030 && out.provenance.count == 7 && out.provenance.length == 19);
    assert(out.provenance.qualified && out.provenance.txAccepted == 8 && out.provenance.receivedLength == 19);
    assert(out.provenance.earliestUs == 200 && out.provenance.latestUs == 220 && out.provenance.deliveredUs == 230);
    assert(std::memcmp(out.provenance.raw,HOMING_PARAMETERS,sizeof(HOMING_PARAMETERS)) == 0);
    std::memset(c.observations[0].raw,0,sizeof(c.observations[0].raw));
    assert(std::memcmp(out.provenance.raw,HOMING_PARAMETERS,sizeof(HOMING_PARAMETERS)) == 0);
    const Saved<PreparedRead> noFurtherWork(request);
    assert(!nextRead(c,240,request)); noFurtherWork.check(request); // No second step or motion write.
    const uint16_t raw[7] = {7,24,60,30,100,0x1234,0xABCD};
    assert(!validateWriteMultipleRegistersRequest(1,0x0030,raw,7));
    assert(!validateWriteMultipleRegistersRequest(1,0x0031,raw,7));
    assert(validateWriteMultipleRegistersRequest(1,0x0031,raw,6)); // Existing staging window only.
    assert(!validateWriteSingleRegisterRequest(1,0x0035,0x1234));
    assert(!validateWriteSingleRegisterRequest(1,0x0036,0xABCD));
}
void testHomingParameterRawCodesAndNativeBounds() {
    struct RawCase { const uint8_t* bytes; uint16_t auxiliary, method, search, returning, ramp, offset0, offset1; bool knownAux, knownMethod, ranges; };
    const RawCase cases[] = {
        {HOMING_UNKNOWN,0xFFFF,0xFFFC,0xF123,0xE234,0xD345,0xC456,0xB567,false,false,false},
        {HOMING_MAXIMUM,11,35,3000,300,2000,0,0xFFFF,true,true,true},
        {HOMING_MAX_RAW,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,false,false,false},
        {HOMING_ZERO_OFFSET,7,35,60,30,100,0,0,true,true,true}
    };
    for (const auto& raw : cases) {
        ReadContext c = homingParameters(); assert(advanceRead(c,frame(c,raw.bytes,19),230));
        HomingParametersObservation out; assert(getHomingParameters(c,out));
        assert(out.rawAuxiliary == raw.auxiliary && out.rawMethod == raw.method && out.rawSearchSpeed == raw.search);
        assert(out.rawReturnSpeed == raw.returning && out.rawRampTime == raw.ramp);
        assert(out.rawOffsetWords[0] == raw.offset0 && out.rawOffsetWords[1] == raw.offset1);
        assert(out.auxiliaryKnown == raw.knownAux && out.methodKnown == raw.knownMethod);
        assert(out.searchSpeedInRange == raw.ranges && out.returnSpeedInRange == raw.ranges && out.rampTimeInRange == raw.ranges);
        if (raw.knownMethod) assert(out.method == HomingMethod::METHOD_35 && out.methodSupport == HomeSupport::IMPLEMENTED);
        else assert(out.methodSupport == HomeSupport::UNRESOLVED);
        homingInterpretationUnresolved(out); // Even two raw zero offset words establish no origin or ordering.
    }
    struct Bounds { uint16_t search, returning, ramp; bool searchKnown, returnKnown, rampKnown; };
    const Bounds limits[] = {{0,0,0,false,false,false}, {4,4,29,false,false,false}, {5,5,30,true,true,true},
        {4,5,30,false,true,true}, {5,4,30,true,false,true}, {5,5,29,true,true,false},
        {3001,301,2001,false,false,false}, {3000,300,2000,true,true,true}};
    for (const auto& limit : limits) {
        uint8_t bytes[19]; std::memcpy(bytes,HOMING_ZERO_OFFSET,sizeof(bytes));
        bytes[7] = static_cast<uint8_t>(limit.search >> 8); bytes[8] = static_cast<uint8_t>(limit.search);
        bytes[9] = static_cast<uint8_t>(limit.returning >> 8); bytes[10] = static_cast<uint8_t>(limit.returning);
        bytes[11] = static_cast<uint8_t>(limit.ramp >> 8); bytes[12] = static_cast<uint8_t>(limit.ramp); seal(bytes,sizeof(bytes));
        ReadContext c = homingParameters(); assert(advanceRead(c,frame(c,bytes,sizeof(bytes)),230));
        HomingParametersObservation out; assert(getHomingParameters(c,out));
        assert(out.rawSearchSpeed == limit.search && out.rawReturnSpeed == limit.returning && out.rawRampTime == limit.ramp);
        assert(out.searchSpeedInRange == limit.searchKnown && out.returnSpeedInRange == limit.returnKnown && out.rampTimeInRange == limit.rampKnown);
    }
}
void testHomingParameterMembershipIsNotExecutableSupport() {
    // Independent source membership, including unlisted positive gaps and unresolved negative encodings.
    for (uint16_t code : {uint16_t(0),uint16_t(1),uint16_t(14),uint16_t(15),uint16_t(16),uint16_t(17),uint16_t(18),
        uint16_t(24),uint16_t(30),uint16_t(31),uint16_t(32),uint16_t(33),uint16_t(34),uint16_t(35),uint16_t(36),
        uint16_t(0x7FFF),uint16_t(0x8000),uint16_t(0xFFFC),uint16_t(0xFFFD),uint16_t(0xFFFE),uint16_t(0xFFFF)}) {
        uint8_t bytes[19]; std::memcpy(bytes,HOMING_ZERO_OFFSET,sizeof(bytes));
        bytes[5] = static_cast<uint8_t>(code >> 8); bytes[6] = static_cast<uint8_t>(code); seal(bytes,sizeof(bytes));
        ReadContext c = homingParameters(); assert(advanceRead(c,frame(c,bytes,sizeof(bytes)),230));
        HomingParametersObservation out; assert(getHomingParameters(c,out));
        const bool known = (code >= 1 && code <= 14) || (code >= 17 && code <= 30) || (code >= 33 && code <= 35);
        assert(out.rawMethod == code && out.methodKnown == known);
        if (known) {
            assert(static_cast<uint16_t>(out.method) == code);
            assert(out.methodSupport == (code >= 33 ? HomeSupport::IMPLEMENTED : code == 18 ? HomeSupport::UNRESOLVED : HomeSupport::UNIMPLEMENTED));
        } else assert(out.methodSupport == HomeSupport::UNRESOLVED);
    }
    for (uint16_t auxiliary : {uint16_t(0),uint16_t(7),uint16_t(11),uint16_t(12),uint16_t(0xFFFF)}) {
        uint8_t bytes[19]; std::memcpy(bytes,HOMING_ZERO_OFFSET,sizeof(bytes));
        bytes[3] = static_cast<uint8_t>(auxiliary >> 8); bytes[4] = static_cast<uint8_t>(auxiliary); seal(bytes,sizeof(bytes));
        ReadContext c = homingParameters(); assert(advanceRead(c,frame(c,bytes,sizeof(bytes)),230));
        HomingParametersObservation out; assert(getHomingParameters(c,out));
        assert(out.rawAuxiliary == auxiliary && out.auxiliaryKnown == (auxiliary <= 11));
        if (out.auxiliaryKnown) assert(static_cast<uint16_t>(out.auxiliary) == auxiliary);
    }
    unsigned implemented = 0, pending = 0, unresolved = 0;
    for (uint8_t i = 0; i < HOME_METHOD_COUNT; ++i) {
        const auto* method = homeMethodAt(i); assert(method);
        if (method->support == HomeSupport::IMPLEMENTED) ++implemented;
        else if (method->support == HomeSupport::UNIMPLEMENTED) ++pending; else ++unresolved;
    }
    assert(implemented == 3 && pending == 27 && unresolved == 5);
}
void testHomingParameterPreparationAndPublicationFailuresPreserveOutput() {
    ReadContext output = homingParameters(); const Saved<ReadContext> unchanged(output);
    for (unsigned i = 0; i < 7; ++i) {
        ReadTarget selected = target();
        if (i == 0) selected.id = 0; else if (i == 1) selected.address = 0;
        else if (i == 2) selected.address = 248; else if (i == 3) selected.generation = 0;
        assert(!prepareHomingParameters(output,selected,i == 4 ? 0 : 12,i == 5 ? 0 : 23,100,i == 6 ? 100 : 1000,active()));
        unchanged.check(output);
    }
    ActiveSerialTuple bad = active(); bad.baud = 0;
    assert(!prepareHomingParameters(output,target(),12,23,100,1000,bad)); unchanged.check(output);
    assert(!prepareHomingParameters(output,target(),12,23,100,99)); unchanged.check(output);
    HomingParametersObservation prior; prior.rawOffsetWords[1] = 0xA55A;
    const Saved<HomingParametersObservation> preserved(prior);
    assert(!getHomingParameters(output,prior)); preserved.check(prior);
    ReadContext other = velocityParameters(); assert(advanceRead(other,frame(other,VELOCITY_PARAMETERS,sizeof(VELOCITY_PARAMETERS)),230));
    assert(!getHomingParameters(other,prior)); preserved.check(prior);
    assert(prepareHomingParameters(output,target(),12,std::numeric_limits<uint32_t>::max(),100,1000));
    assert(advanceRead(output,frame(output,HOMING_PARAMETERS,sizeof(HOMING_PARAMETERS)),230));
    assert(getHomingParameters(output,prior) && prior.configurationGeneration == std::numeric_limits<uint32_t>::max());
    VelocityParametersObservation velocity; velocity.rawSpeed = 0xA55A; const Saved<VelocityParametersObservation> oldVelocity(velocity);
    assert(!getVelocityParameters(output,velocity)); oldVelocity.check(velocity);
}
void testHomingParameterMalformedRepliesRetainWithoutPublication() {
    const uint8_t shortCount[] = {1,3,12,0,7,0,24,0,60,0,30,0,100,0x12,0x34,0x3B,0xAD};
    const uint8_t longCount[] = {1,3,16,0,7,0,24,0,60,0,30,0,100,0x12,0x34,0xAB,0xCD,0x56,0x78,0xDA,0x5A};
    const uint8_t wrongAddress[] = {2,3,14,0,7,0,24,0,60,0,30,0,100,0x12,0x34,0xAB,0xCD,0x1E,0xD9};
    const uint8_t wrongFunction[] = {1,4,14,0,7,0,24,0,60,0,30,0,100,0x12,0x34,0xAB,0xCD,0xAC,0x1B};
    const uint8_t exception[] = {1,0x83,2,0xC0,0xF1};
    uint8_t badCount[19]; std::memcpy(badCount,HOMING_PARAMETERS,sizeof(badCount)); badCount[2] = 12; seal(badCount,sizeof(badCount));
    uint8_t badCrc[19]; std::memcpy(badCrc,HOMING_PARAMETERS,sizeof(badCrc)); badCrc[18] ^= 1;
    struct Rejected { const uint8_t* bytes; std::size_t length; FrameError error; };
    const Rejected failures[] = {{shortCount,sizeof(shortCount),FrameError::LENGTH}, {longCount,sizeof(longCount),FrameError::LENGTH},
        {wrongAddress,sizeof(wrongAddress),FrameError::ADDRESS}, {wrongFunction,sizeof(wrongFunction),FrameError::FUNCTION},
        {badCount,sizeof(badCount),FrameError::BYTE_COUNT}, {badCrc,sizeof(badCrc),FrameError::CRC},
        {HOMING_PARAMETERS,18,FrameError::LENGTH}, {HOMING_PARAMETERS,0,FrameError::LENGTH}, {exception,sizeof(exception),FrameError::EXCEPTION}};
    for (const auto& failure : failures) {
        ReadContext c = homingParameters(); HomingParametersObservation prior; prior.rawAuxiliary = 0xA55A;
        const Saved<HomingParametersObservation> unchanged(prior);
        assert(advanceRead(c,frame(c,failure.bytes,failure.length),230));
        assert(c.state == ReadState::FAILED && c.outcome == ReadOutcome::REPLY_ERROR && c.completedSteps == 0);
        assert(c.observations[0].frameError == failure.error && c.observations[0].receivedLength == failure.length);
        assert(c.observations[0].length == failure.length && std::memcmp(c.observations[0].raw,failure.bytes,failure.length) == 0);
        if (failure.error == FrameError::EXCEPTION) assert(c.status.code == Err::EXCEPTION && c.status.detail == 2);
        assert(!getHomingParameters(c,prior)); unchanged.check(prior);
        PreparedRead request; const Saved<PreparedRead> noRetry(request);
        assert(!nextRead(c,240,request)); noRetry.check(request);
    }
}
void testHomingParameterCorrelationDeadlineAndCancellation() {
    for (unsigned i = 0; i < 5; ++i) {
        ReadContext c = homingParameters(); const Saved<ReadContext> unchanged(c);
        ReadEvent stale = frame(c,HOMING_PARAMETERS,sizeof(HOMING_PARAMETERS));
        if (i == 0) ++stale.target.id; else if (i == 1) ++stale.target.address;
        else if (i == 2) ++stale.target.generation; else if (i == 3) ++stale.operationId; else ++stale.step;
        assert(!advanceRead(c,stale,230)); unchanged.check(c); // Same 19-byte reply cannot satisfy a wrong step.
    }
    ReadContext c = homingParameters(); PreparedRead request; request.first = 0xA55A;
    const Saved<PreparedRead> noLateTraffic(request);
    assert(!nextRead(c,1000,request)); noLateTraffic.check(request);
    assert(advanceRead(c,frame(c,HOMING_PARAMETERS,sizeof(HOMING_PARAMETERS),980,1000),5000));
    HomingParametersObservation out; assert(getHomingParameters(c,out));
    assert(out.provenance.latestUs == 1000 && out.provenance.deliveredUs == 5000 && c.deadlineUs == 1000);
    for (unsigned i = 0; i < 5; ++i) {
        c = homingParameters(); ReadEvent event = frame(c,HOMING_PARAMETERS,sizeof(HOMING_PARAMETERS),980,1001);
        if (i == 1) { event.qualified = false; event.earliestUs = event.latestUs = 0; }
        else if (i >= 2) event = control(c,i == 2 ? ReadEventKind::DEADLINE : i == 3 ? ReadEventKind::CANCEL : ReadEventKind::TRANSPORT_FAILURE);
        if (i == 3 || i == 4) { event.frame = HOMING_PARAMETERS; event.length = 10; event.txAccepted = 8; }
        const Saved<HomingParametersObservation> preserved(out);
        assert(advanceRead(c,event,i >= 3 ? 230 : 1001));
        assert(c.state == ReadState::FAILED && c.completedSteps == 0 && c.deadlineUs == 1000);
        assert(c.outcome == (i == 1 ? ReadOutcome::TIMING_UNQUALIFIED : i == 3 ? ReadOutcome::CANCELLED :
            i == 4 ? ReadOutcome::TRANSPORT_ERROR : ReadOutcome::DEADLINE));
        if (i >= 3) assert(c.observations[0].length == 10 && c.observations[0].txAccepted == 8);
        assert(!getHomingParameters(c,out)); preserved.check(out);
        assert(!nextRead(c,1100,request)); noLateTraffic.check(request);
        const Saved<ReadContext> terminal(c);
        assert(!advanceRead(c,frame(c,HOMING_PARAMETERS,sizeof(HOMING_PARAMETERS)),1200)); terminal.check(c);
    }
}
void testCapabilitiesAndFixedStorage() {
    const ReadCapabilities caps = readCapabilities();
    assert(caps.probe && caps.identity && caps.config && caps.state && caps.velocityParameters && caps.homingParameters && caps.maxSteps == 5 && caps.maxReplyBytes <= 37);
    static_assert(READ_MAX_STEPS == 5 && READ_INPUT_COUNT == 4 && READ_MAX_REPLY_BYTES == 37, "Reviewed read bounds");
    static_assert(sizeof(ReadContext) < 2048 && sizeof(ConfigObservation) < 2048, "Fixed bounded read storage");
}
}
int main() {
    testIdentityAndOwnedProvenance(); testConfigWindowsPublicationAndIndependentEvidence();
    testPreparationAndPublicationFailuresPreserveOutputs(); testWrongEnvelopesAndSameLengthStepsRejectWithoutMutation();
    testPartialFailuresPreservePreviousObservation(); testDeadlineUsesQualifiedClosureAndNeverRenewsBudget();
    testUnknownEnumsAndZeroEncoderRemainRawEvidence(); testMalformedRepliesRetainFailureWithoutPublishing();
    testTransportProvenanceIsBoundedOwnedAndNotAcknowledgement(); testUnknownIdentityAndBoundaryTargetsArePreserved();
    testCapabilitiesAndFixedStorage();
    testStateWindowsBitfieldsAndAbsentFeedback();testStateConfiguredPairsAndUnknownAlarms();
    testStatePartialFailureAndConfigurationCorrelation();
    testVelocityParameterWindowAndOwnedObservation(); testVelocityParameterUnknownAndNativeBoundaryWords();
    testVelocityParameterPreparationAndGetterPreservation(); testVelocityParameterMalformedRepliesAndException();
    testVelocityParameterCorrelationDeadlineAndCancellation();
    testHomingParameterWindowSevenWordsAndOwnedEvidence(); testHomingParameterRawCodesAndNativeBounds();
    testHomingParameterMembershipIsNotExecutableSupport(); testHomingParameterPreparationAndPublicationFailuresPreserveOutput();
    testHomingParameterMalformedRepliesRetainWithoutPublication(); testHomingParameterCorrelationDeadlineAndCancellation();
}
