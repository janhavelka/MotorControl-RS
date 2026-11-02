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
/** Shared classification of one settled ESS write reply. Evidence needs only the
 * transport fields common to ActionEvidence and DriverEvidence. An echo proves
 * acknowledgement only from a confirmed qualified frame without supplied execution
 * uncertainty. Function manual p12 documents 01..07, but 05 is a read-range error:
 * it is an unexpected reply to a write and does not establish that the drive refused
 * it. Undocumented codes likewise retain UNKNOWN. onTime is the caller's own closure
 * rule; modules without one treat a late checked reply as evidence of that reply. */
template <class Evidence>
inline ActionExecution classifyWrite(const Evidence& evidence, bool onTime = true) noexcept {
    if (!evidence.txAccepted && !evidence.executionUnknown) return ActionExecution::NOT_TRANSMITTED;
    if (evidence.event == ReadEventKind::FRAME && evidence.qualified && evidence.responseConfirmed &&
        !evidence.executionUnknown && onTime) {
        if (evidence.status) return ActionExecution::ACKNOWLEDGED;
        if (evidence.status.code == Err::EXCEPTION && evidence.status.detail >= 1 &&
            evidence.status.detail <= 7 && evidence.status.detail != 5)
            return ActionExecution::REJECTED;
    }
    return ActionExecution::UNKNOWN;
}
}}} // namespace MotorControlRS::ESS_RS::Detail
