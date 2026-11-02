// SPDX-License-Identifier: MIT
// Typed console routes that state commissioning facts by name. A fact is the
// operator's statement about this installation; nothing here is inferred from a
// read, and no route can state every fact at once on the operator's behalf.
#pragma once
#include "HomeConsoleParser.h"
#include <cstring>

namespace MotorControlRSExample { namespace Probe {

enum class QualificationRoute : uint8_t { HOME, VELOCITY, POSITION_CLEAR, START_SPEED_RELATION };

/** One install or revoke request, consumed during the callback. */
struct QualificationCommand {
    QualificationRoute route = QualificationRoute::HOME;
    bool revoke = false;
    MotorControlRS::ESS_RS::HomeRequest home; ///< Home: method and native words.
    bool mechanics = false, rates = false, ramp = false, zeroOffset = false;
    bool auxiliary = false, inputs = false, index = false, reference = false; ///< Home facts; ramp and inputs are shared with velocity.
    bool rpm = false, negative = false; ///< Velocity facts.
    bool clearSemantics = false; ///< Position-clear fact.
    uint16_t rawStartSpeed = 0, minimumRpm = 0; ///< Start-speed relation.
};

enum class Action : uint8_t;
/** result comes from the application's own checks. reason replaces the generic result
 * name for a specific refusal; readFirst names the command that supplies missing evidence. */
struct QualificationReply {
    Action result;
    const char* reason = nullptr;
    const char* readFirst = nullptr;
    explicit QualificationReply(Action value) : result(value) {}
};

namespace QualificationFacts {
struct Fact {
    const char* name;
    bool QualificationCommand::* field;
    bool required;
};
// index is required by method descriptor, so it is checked after this table.
const Fact HOME[] = {
    {"mechanics", &QualificationCommand::mechanics, true},
    {"rates", &QualificationCommand::rates, true},
    {"ramp", &QualificationCommand::ramp, true},
    {"zero-offset", &QualificationCommand::zeroOffset, true},
    {"auxiliary", &QualificationCommand::auxiliary, true},
    {"inputs", &QualificationCommand::inputs, true},
    {"index", &QualificationCommand::index, false},
    {"reference", &QualificationCommand::reference, false},
};
const Fact VELOCITY[] = {
    {"rpm", &QualificationCommand::rpm, true},
    {"ramp", &QualificationCommand::ramp, true},
    {"inputs", &QualificationCommand::inputs, true},
    {"negative", &QualificationCommand::negative, false},
};
const Fact POSITION_CLEAR[] = {
    {"clear-semantics", &QualificationCommand::clearSemantics, true},
};

/** Returns nullptr, or the rejection result with fact naming the offending token.
 * A rejected token is echoed only when it cannot alter the JSON reply. */
inline const char* parse(const char* const* tokens, std::size_t count, const Fact* facts, std::size_t factCount,
                         QualificationCommand& command, const char*& named) noexcept {
    for (std::size_t i = 0; i < count; ++i) {
        const Fact* match = nullptr;
        for (std::size_t f = 0; f < factCount; ++f)
            if (!std::strcmp(tokens[i], facts[f].name)) match = facts + f;
        if (!match) {
            named = "unprintable";
            bool safe = tokens[i][0] != '\0';
            for (const char* p = tokens[i]; *p; ++p)
                safe = safe && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_');
            if (safe) named = tokens[i];
            return "unknown_fact";
        }
        if (command.*(match->field)) { named = match->name; return "duplicate_fact"; }
        command.*(match->field) = true;
    }
    for (std::size_t f = 0; f < factCount; ++f)
        if (facts[f].required && !(command.*(facts[f].field))) { named = facts[f].name; return "missing_fact"; }
    return nullptr;
}
} // namespace QualificationFacts

/** `install METHOD SEARCH RETURN RAMP FACT...`; tokens exclude the leading words. */
inline const char* parseHomeQualification(const char* const* tokens, std::size_t count,
                                          QualificationCommand& command, const char*& named) noexcept {
    namespace Ess = MotorControlRS::ESS_RS;
    if (count < 4 || !HomeConsole::parseWords(tokens, command.home)) return "invalid_arguments";
    const auto* method = Ess::homeMethod(command.home.method);
    if (!method) return "invalid";
    if (method->support == Ess::HomeSupport::UNRESOLVED) return "unresolved";
    if (method->support == Ess::HomeSupport::UNIMPLEMENTED) return "unimplemented";
    const std::size_t facts = sizeof(QualificationFacts::HOME) / sizeof(QualificationFacts::HOME[0]);
    if (const char* rejected = QualificationFacts::parse(tokens + 4, count - 4, QualificationFacts::HOME, facts, command, named))
        return rejected;
    named = "index";
    if (method->requiresIndex && !command.index) return "missing_fact";
    if (!method->requiresIndex && command.index) return "fact_not_applicable";
    named = nullptr;
    return nullptr;
}
}} // namespace MotorControlRSExample::Probe
