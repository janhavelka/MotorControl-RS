// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/profiles/ess_rs/Actions.h>
#include <cassert>

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
