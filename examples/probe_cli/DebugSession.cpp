// SPDX-License-Identifier: MIT
#include "DebugSession.h"

namespace MotorControlRSExample { namespace Probe {
namespace {
void addCount(uint64_t& value, uint64_t amount = 1) {
    value = amount > UINT64_MAX - value ? UINT64_MAX : value + amount;
}
}

bool DebugSession::copyRecord() {
    const uint64_t previous = cursor;
    if (!capture.copyAfter(cursor, scratch)) return false;
    if (cursor > previous && cursor - previous > 1)
        addCount(state.missed, cursor - previous - 1);
    return true;
}

Action DebugSession::command(const DebugMode* requested, DebugSnapshot& out) {
    if (requested) {
        if (*requested > DebugMode::DECODED) return Action::INVALID;
        if (*requested != state.mode) {
            // Disabling first publishes any partial RX copy. Account for every
            // retained copy intentionally skipped by this display-mode change;
            // other readers retain independent cursors and the same raw records.
            capture.setEnabled(false);
            for (std::size_t i = 0; i < CAPACITY && copyRecord(); ++i)
                addCount(state.skipped);
            requestKnown = false;
            state.mode = *requested;
            capture.setEnabled(*requested != DebugMode::OFF);
        }
    }
    state.cursor = cursor;
    state.overwritten = capture.overwritten();
    state.captureDropped = capture.dropped();
    state.retained = capture.size();
    state.capacity = CAPACITY;
    out = state;
    return Action::OK;
}

void DebugSession::service(Console& console, bool outputReserved) {
    if (state.mode == DebugMode::OFF || !copyRecord()) return;
    // At most one copied event and one optional formatted line per loop.
    addCount(state.observed);
    const auto& record = scratch;
    if (record.kind == MotorControlRS::TrafficKind::TX) {
        request = record;
        requestKnown = true;
    }
    const auto* matchingRequest = requestKnown && request.transaction == record.transaction ?
        &request : nullptr;
    // Preserve ordinary replies/results under output pressure. A dropped display
    // copy is separate from a record overwritten before this reader copied it.
    if (outputReserved || console.outputPending() ||
        !console.reportTraffic(record, state.mode, matchingRequest))
        addCount(state.dropped);
    else addCount(state.emitted);
    state.cursor = cursor;
}
}} // namespace MotorControlRSExample::Probe
