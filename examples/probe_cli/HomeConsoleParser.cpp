// SPDX-License-Identifier: MIT
#include "HomeConsoleParser.h"
#include "ConsoleNumbers.h"
#include <cstring>

namespace MotorControlRSExample { namespace Probe { namespace HomeConsole {
const char* parse(const char* const* args, std::size_t count,
                  MotorControlRS::ESS_RS::HomeRequest& request,
                  uint8_t& address, bool& explicitAddress) noexcept {
    namespace Core = MotorControlRS;
    namespace Ess = MotorControlRS::ESS_RS;
    if ((count != 5 && count != 6) || std::strcmp(args[4], "zero"))
        return "invalid_arguments";
    Core::Rational method;
    const char* digit = args[0]; if (*digit == '-') ++digit;
    bool integer = *digit != '\0';
    for (const char* p = digit; *p; ++p) integer = integer && *p >= '0' && *p <= '9';
    uint32_t search = 0, returning = 0, ramp = 0, selectedAddress = 0;
    if (!integer || !Core::parseExactNumber(args[0], method) || method.denominator != 1 ||
        method.numerator < -32768 || method.numerator > 32767 ||
        !ConsoleNumbers::number(args[1], search) || search < 5 || search > 3000 ||
        !ConsoleNumbers::number(args[2], returning) || returning < 5 || returning > 300 ||
        !ConsoleNumbers::number(args[3], ramp) || ramp < 30 || ramp > 2000)
        return "invalid_arguments";
    if (count == 6 && (!ConsoleNumbers::number(args[5], selectedAddress) ||
                      selectedAddress < 1 || selectedAddress > 247))
        return "invalid_address";
    request.method = static_cast<Ess::HomingMethod>(method.numerator);
    request.searchSpeed = static_cast<uint16_t>(search);
    request.returnSpeed = static_cast<uint16_t>(returning);
    request.rampTime = static_cast<uint16_t>(ramp);
    address = static_cast<uint8_t>(selectedAddress);
    explicitAddress = count == 6;
    return nullptr;
}
}}}
