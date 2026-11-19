// SPDX-License-Identifier: MIT
#include "HomeConsoleParser.h"
#include "ConsoleNumbers.h"
#include <cstring>

namespace MotorControlRSExample { namespace Probe { namespace HomeConsole {
bool parseWords(const char* const* args, MotorControlRS::ESS_RS::HomeRequest& request) noexcept {
    namespace Core = MotorControlRS;
    namespace Ess = MotorControlRS::ESS_RS;
    Core::Rational method;
    const char* digit = args[0]; if (*digit == '-') ++digit;
    bool integer = *digit != '\0';
    for (const char* p = digit; *p; ++p) integer = integer && *p >= '0' && *p <= '9';
    uint32_t search = 0, returning = 0, ramp = 0;
    if (!integer || !Core::parseExactNumber(args[0], method) || method.denominator != 1 ||
        method.numerator < -32768 || method.numerator > 32767 ||
        !ConsoleNumbers::number(args[1], search) || search < 5 || search > 3000 ||
        !ConsoleNumbers::number(args[2], returning) || returning < 5 || returning > 300 ||
        !ConsoleNumbers::number(args[3], ramp) || ramp < 30 || ramp > 2000)
        return false;
    request.method = static_cast<Ess::HomingMethod>(method.numerator);
    request.searchSpeed = static_cast<uint16_t>(search);
    request.returnSpeed = static_cast<uint16_t>(returning);
    request.rampTime = static_cast<uint16_t>(ramp);
    return true;
}
// `zero` or a signed decimal native offset within +-0x0FFFFFFF (p73 range).
// Admission of a nonzero offset belongs to the application and core.
bool parseOffset(const char* text, int64_t& offset) noexcept {
    if (!std::strcmp(text, "zero")) { offset = 0; return true; }
    const char* digit = text; if (*digit == '-') ++digit;
    if (!*digit || std::strlen(digit) > 9) return false;
    int64_t value = 0;
    for (const char* p = digit; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        value = value * 10 + (*p - '0');
    }
    if (value > 0x0FFFFFFF || value == 0) return false; // Zero is stated as `zero`.
    offset = digit == text ? value : -value;
    return true;
}
const char* parse(const char* const* args, std::size_t count,
                  MotorControlRS::ESS_RS::HomeRequest& request,
                  uint8_t& address, bool& explicitAddress) noexcept {
    int64_t offset = 0;
    if ((count != 5 && count != 6) || !parseOffset(args[4], offset))
        return "invalid_arguments";
    MotorControlRS::ESS_RS::HomeRequest parsed = request;
    if (!parseWords(args, parsed)) return "invalid_arguments";
    parsed.offset = offset;
    uint32_t selectedAddress = 0;
    if (count == 6 && (!ConsoleNumbers::number(args[5], selectedAddress) ||
                      selectedAddress < 1 || selectedAddress > 247))
        return "invalid_address";
    request = parsed;
    address = static_cast<uint8_t>(selectedAddress);
    explicitAddress = count == 6;
    return nullptr;
}
}}}
