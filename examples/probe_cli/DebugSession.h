// SPDX-License-Identifier: MIT
#pragma once
#include "ProbeConsole.h"

namespace MotorControlRSExample { namespace Probe {
/** Application-owned diagnostics for the ordinary runner and console. No second
 * transport, operation queue or execution policy belongs to this session. */
struct DebugSession {
    DebugSession() = default;
    DebugSession(const DebugSession&) = delete;
    DebugSession& operator=(const DebugSession&) = delete;
    DebugSession(DebugSession&&) = delete;
    DebugSession& operator=(DebugSession&&) = delete;

    /** Inspect or change display mode. Disabling flushes partial RX before the
     * display cursor skips retained records; other readers remain independent. */
    Action command(const DebugMode* requested, DebugSnapshot& out);
    /** Copy at most one event and emit at most one optional line. The application
     * reserves output slots for ordinary results through outputReserved. */
    void service(Console& console, bool outputReserved);

    static constexpr std::size_t CAPACITY = 16;
    MotorControlRS::TrafficRecord records[CAPACITY], request, scratch;
    // Construct in final application storage: capture refers to records and the
    // runner retains capture's address for the complete application lifetime.
    MotorControlRS::TrafficCapture capture{records, CAPACITY};
    DebugSnapshot state;
    uint64_t cursor = 0;
    bool requestKnown = false;
private:
    bool copyRecord();
};
}} // namespace MotorControlRSExample::Probe
