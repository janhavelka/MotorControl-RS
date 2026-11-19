// SPDX-License-Identifier: MIT
#pragma once
#include "IoQualification.h"
#include "ConsoleNumbers.h"
#include <cstddef>
#include <cstring>
#include <initializer_list>

namespace MotorControlRSExample { namespace Probe { namespace IoQualificationConsole {
inline const char* purposeName(IoPurpose purpose) noexcept {
    switch (purpose) {
    case IoPurpose::ASSIGN: return "assign";
    case IoPurpose::CONFIGURE: return "configure";
    case IoPurpose::FINITE_MOVE: return "finite-move";
    case IoPurpose::SEGMENT: return "segment";
    }
    return "unknown";
}
inline const char* mappingName(MotorControlRS::ESS_RS::IoStatusMapping mapping) noexcept {
    using Mapping = MotorControlRS::ESS_RS::IoStatusMapping;
    switch (mapping) {
    case Mapping::UNKNOWN: return "unknown";
    case Mapping::BEFORE_INVERSION: return "before";
    case Mapping::AFTER_INVERSION: return "after";
    }
    return "invalid";
}
inline const char* loadName(MotorControlRS::ESS_RS::IoOutputLoad load) noexcept {
    using Load = MotorControlRS::ESS_RS::IoOutputLoad;
    switch (load) {
    case Load::UNDECLARED: return "undeclared";
    case Load::NO_LOAD: return "none";
    case Load::INDICATOR: return "indicator";
    case Load::CONTROL: return "control";
    }
    return "invalid";
}
inline bool purpose(const char* token, IoPurpose& value) noexcept {
    for (const auto candidate : {IoPurpose::ASSIGN, IoPurpose::CONFIGURE, IoPurpose::FINITE_MOVE, IoPurpose::SEGMENT})
        if (!std::strcmp(token, purposeName(candidate))) { value = candidate; return true; }
    return false;
}
inline bool mapping(const char* token, MotorControlRS::ESS_RS::IoStatusMapping& value) noexcept {
    using Mapping = MotorControlRS::ESS_RS::IoStatusMapping;
    for (const auto candidate : {Mapping::UNKNOWN, Mapping::BEFORE_INVERSION, Mapping::AFTER_INVERSION})
        if (!std::strcmp(token, mappingName(candidate))) { value = candidate; return true; }
    return false;
}
inline bool word(const char* token, uint16_t limit, uint16_t& value) noexcept {
    uint32_t parsed = 0;
    if (!ConsoleNumbers::number(token, parsed) || parsed > limit) return false;
    value = static_cast<uint16_t>(parsed); return true;
}
inline const char* factName(const char* token) noexcept {
    if (!*token) return "unprintable";
    for (const char* p = token; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_'))
            return "unprintable";
    return token;
}
/** Tokens start with install, inspect or revoke. Rejection leaves output unchanged. */
inline const char* parse(const char* const* tokens, std::size_t count, IoQualificationCommand& output,
                         const char*& named) noexcept {
    using Kind = IoQualificationCommand::Kind;
    namespace Ess = MotorControlRS::ESS_RS;
    named = nullptr;
    if (count < 4) return "invalid_arguments";
    IoQualificationCommand request;
    if (!std::strcmp(tokens[0], "install")) request.kind = Kind::INSTALL;
    else if (!std::strcmp(tokens[0], "inspect")) request.kind = Kind::INSPECT;
    else if (!std::strcmp(tokens[0], "revoke")) request.kind = Kind::REVOKE;
    else return "invalid_arguments";
    std::size_t first = 1;
    if (!std::strcmp(tokens[first], "member")) { request.member = true; ++first; }
    if (count < first + 3) return "invalid_arguments";
    tokens += first; count -= first;
    uint32_t address = 0;
    if (!ConsoleNumbers::number(tokens[0], address) || !address || address > 247) return "invalid_address";
    request.address = static_cast<uint8_t>(address);
    const char* terminal = tokens[1];
    if (std::strlen(terminal) != 2 || terminal[1] < '0' ||
        !((terminal[0] == 'x' && terminal[1] <= '3') || (terminal[0] == 'y' && terminal[1] <= '1')))
        return "invalid_arguments";
    request.terminal = static_cast<uint8_t>(terminal[1] - '0' + (terminal[0] == 'y' ? 4 : 0));
    const bool input = request.terminal < 4;
    if (request.kind != Kind::INSTALL) {
        if (count != 3 || !purpose(tokens[2], request.purpose) || (!input && request.purpose != IoPurpose::ASSIGN) ||
            (request.member && (!input || request.purpose != IoPurpose::FINITE_MOVE)))
            return "invalid_arguments";
        output = request; return nullptr;
    }
    if (count < (input ? 6U : 9U) || !word(tokens[2], input ? 17 : 11, request.function) ||
        !word(tokens[3], 1, request.polarity)) return "invalid_arguments";
    if (input) {
        // `recorded` defers the mapping to the application record (wired bench 05).
        request.recordedMapping = !std::strcmp(tokens[5], "recorded");
        if (!purpose(tokens[4], request.purpose) ||
            (!request.recordedMapping && !mapping(tokens[5], request.input.statusMapping)) ||
            (request.member && request.purpose != IoPurpose::FINITE_MOVE)) return "invalid_arguments";
        request.input.supplied = true;
        for (std::size_t i = 6; i < count; ++i) {
            bool* fact = nullptr;
            if (!std::strcmp(tokens[i], "triggers-inhibited")) fact = &request.input.externalTriggersInhibited;
            else if (!std::strcmp(tokens[i], "protection-removal")) {
                if (request.purpose != IoPurpose::ASSIGN) { named = "protection-removal"; return "fact_not_applicable"; }
                fact = &request.input.protectionRemovalQualified;
            } else { named = factName(tokens[i]); return "unknown_fact"; }
            if (*fact) { named = tokens[i]; return "duplicate_fact"; }
            *fact = true;
        }
    } else {
        if (request.function == 11) return "unresolved";
        if (request.function > 5 && request.function < 9) return "invalid_arguments";
        if (request.member || count != 9 || !word(tokens[4], 1, request.custom) ||
            !purpose(tokens[5], request.purpose) || request.purpose != IoPurpose::ASSIGN ||
            !mapping(tokens[6], request.output.statusMapping)) return "invalid_arguments";
        request.output.supplied = true;
        if (!std::strcmp(tokens[7], "none")) request.output.load = Ess::IoOutputLoad::NO_LOAD;
        else if (!std::strcmp(tokens[7], "indicator")) request.output.load = Ess::IoOutputLoad::INDICATOR;
        else if (!std::strcmp(tokens[7], "control")) request.output.load = Ess::IoOutputLoad::CONTROL;
        else return "invalid_arguments";
        if (!std::strcmp(tokens[8], "inactive")) request.output.allowedLevels = 1;
        else if (!std::strcmp(tokens[8], "active")) request.output.allowedLevels = 2;
        else if (!std::strcmp(tokens[8], "either")) request.output.allowedLevels = 3;
        else return "invalid_arguments";
    }
    output = request; return nullptr;
}
}}} // namespace MotorControlRSExample::Probe::IoQualificationConsole
