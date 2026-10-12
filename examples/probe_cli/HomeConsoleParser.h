// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/profiles/ess_rs/Homing.h>
#include <cstddef>

namespace MotorControlRSExample { namespace Probe { namespace HomeConsole {
// Arguments exclude the command name. Tokens are borrowed for this call only.
// Returns the existing console error reason, or nullptr on success. The caller
// supplies the default address and configuration generation after parsing.
const char* parse(const char* const* arguments, std::size_t count,
                  MotorControlRS::ESS_RS::HomeRequest& request,
                  uint8_t& address, bool& explicitAddress) noexcept;
}}}
