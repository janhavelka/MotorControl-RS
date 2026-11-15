// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/profiles/ess_rs/Actions.h>
#include <cassert>

// Member copies preserve evidence, but need not preserve struct padding.
inline void checkSameActionEvidence(const MotorControlRS::ESS_RS::ActionEvidence& expected,
        const MotorControlRS::ESS_RS::ActionEvidence& actual) {
    assert(actual.step == expected.step && actual.event == expected.event);
    for (std::size_t i = 0; i < sizeof(actual.raw); ++i) assert(actual.raw[i] == expected.raw[i]);
    assert(actual.length == expected.length && actual.receivedLength == expected.receivedLength);
    assert(actual.txAccepted == expected.txAccepted && actual.txComplete == expected.txComplete);
    assert(actual.responseConfirmed == expected.responseConfirmed && actual.qualified == expected.qualified);
    assert(actual.executionUnknown == expected.executionUnknown);
    assert(actual.earliestUs == expected.earliestUs && actual.latestUs == expected.latestUs);
    assert(actual.deliveredUs == expected.deliveredUs && actual.transportDetail == expected.transportDetail);
    assert(actual.status.code == expected.status.code && actual.status.detail == expected.status.detail &&
        actual.status.msg == expected.status.msg && actual.frameError == expected.frameError);
}

// Compare the mechanical copy only. Parser status and sequence outcomes stay
// in each fixture, where their different policies are exercised.
inline void checkActionTransportEvidence(const MotorControlRS::ESS_RS::ActionEvidence& evidence,
        const MotorControlRS::ActionEvent& supplied, uint8_t step, uint64_t deliveredUs) {
    const auto& event = supplied.transport;
    assert(evidence.step == step && evidence.event == event.kind);
    assert(evidence.receivedLength == event.length);
    const std::size_t captured = event.length < sizeof(evidence.raw) ? event.length : sizeof(evidence.raw);
    assert(evidence.length == captured);
    for (std::size_t i = 0; i < captured; ++i) assert(evidence.raw[i] == event.frame[i]);
    for (std::size_t i = captured; i < sizeof(evidence.raw); ++i) assert(evidence.raw[i] == 0);
    assert(evidence.txAccepted == event.txAccepted && evidence.txComplete == supplied.txComplete);
    assert(evidence.responseConfirmed == supplied.responseConfirmed);
    assert(evidence.qualified == event.qualified && evidence.executionUnknown == event.executionUnknown);
    assert(evidence.earliestUs == event.earliestUs && evidence.latestUs == event.latestUs);
    assert(evidence.deliveredUs == deliveredUs && evidence.transportDetail == event.transportDetail);
}
