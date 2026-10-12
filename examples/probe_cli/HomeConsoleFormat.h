// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/profiles/ess_rs/Homing.h>
#include <cstddef>

namespace MotorControlRSExample { namespace Probe { namespace HomeConsole {
// Borrow evidence for this call only. Success produces a complete JSON body;
// failure leaves a partial unpublished buffer. Serial metadata and emission
// remain the caller's responsibility. No pointers are retained.
bool formatResult(char* output, std::size_t capacity, std::size_t& used,
                  uint32_t id, uint32_t commandId, uint32_t operationId,
                  const MotorControlRS::ESS_RS::HomeContext& context,
                  bool inspection, bool interruptedByStop) noexcept;
bool formatMethods(char* output, std::size_t capacity, std::size_t& used,
                   uint32_t id) noexcept;
}}}
