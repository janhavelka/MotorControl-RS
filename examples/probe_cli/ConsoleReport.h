// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/ActionOperation.h>
#include <cstddef>
#include <cstdarg>
#include <cstdio>

namespace MotorControlRSExample { namespace Probe { namespace ConsoleReport {
namespace Core = MotorControlRS;
// Append complete fragments or fail; never publish truncated JSON.
inline bool append(char* output, std::size_t capacity, std::size_t& used, const char* format, ...) {
    if (used >= capacity) return false;
    va_list arguments; va_start(arguments, format);
    const int written = std::vsnprintf(output + used, capacity - used, format, arguments);
    va_end(arguments);
    if (written < 0 || static_cast<std::size_t>(written) >= capacity - used) return false;
    used += static_cast<std::size_t>(written); return true;
}

inline const char* boolean(bool value) { return value ? "true" : "false"; }
inline const char* executionName(Core::ActionExecution execution) {
    switch (execution) { case Core::ActionExecution::NOT_TRANSMITTED: return "not_transmitted";
    case Core::ActionExecution::ACKNOWLEDGED: return "acknowledged";
    case Core::ActionExecution::REJECTED: return "rejected"; case Core::ActionExecution::UNKNOWN: return "unknown"; } return "unknown";
}
inline const char* motorOutcome(Core::ActionOutcome outcome) {
    switch (outcome) { case Core::ActionOutcome::NONE: return "none"; case Core::ActionOutcome::OBSERVED: return "observed";
    case Core::ActionOutcome::REPLY_ERROR: return "reply_error"; case Core::ActionOutcome::TRANSPORT_ERROR: return "transport_error";
    case Core::ActionOutcome::CANCELLED: return "cancelled"; case Core::ActionOutcome::DEADLINE: return "deadline";
    case Core::ActionOutcome::TIMING_UNQUALIFIED: return "timing_unqualified";
    case Core::ActionOutcome::UNCONFIRMED_RESPONSE: return "unconfirmed_response";
    case Core::ActionOutcome::ACKNOWLEDGED: return "acknowledged";
    case Core::ActionOutcome::OBSERVATION_LIMIT: return "observation_limit"; } return "unknown";
}
inline void hex(const uint8_t* bytes, std::size_t length, char* output, std::size_t capacity) {
    static const char DIGITS[] = "0123456789ABCDEF";
    const std::size_t count = bytes ? (length < (capacity - 1) / 2 ? length : (capacity - 1) / 2) : 0;
    for (std::size_t i = 0; i < count; ++i) {
        output[2 * i] = DIGITS[bytes[i] >> 4];
        output[2 * i + 1] = DIGITS[bytes[i] & 15];
    }
    output[2 * count] = '\0';
}
}}}
