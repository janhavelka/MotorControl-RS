// SPDX-License-Identifier: MIT
#pragma once
#include "MotorControlRS/profiles/ess_rs/Actions.h"
#include <cstring>

namespace MotorControlRS { namespace ESS_RS { namespace Detail {
// The sequence has validated the envelope and default-initialized its local
// evidence object. Copy before parsing/publication so a frame borrowed from
// retained context evidence stays readable. Status and frameError stay local
// to the sequence; this helper owns no validation or outcome policy.
inline void copyActionEvidence(ActionEvidence& evidence, const ActionEvent& supplied,
                               uint8_t step, uint64_t deliveredUs) noexcept {
    const ReadEvent& event = supplied.transport;
    evidence.step = step;
    evidence.event = event.kind;
    evidence.txAccepted = event.txAccepted;
    evidence.txComplete = supplied.txComplete;
    evidence.responseConfirmed = supplied.responseConfirmed;
    evidence.executionUnknown = event.executionUnknown;
    evidence.qualified = event.qualified;
    evidence.earliestUs = event.earliestUs;
    evidence.latestUs = event.latestUs;
    evidence.deliveredUs = deliveredUs;
    evidence.transportDetail = event.transportDetail;
    evidence.receivedLength = event.length;
    evidence.length = event.length < ACTION_MAX_REPLY_BYTES ? event.length : ACTION_MAX_REPLY_BYTES;
    if (evidence.length) std::memcpy(evidence.raw, event.frame, evidence.length);
}
}}} // namespace MotorControlRS::ESS_RS::Detail
