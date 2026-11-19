// SPDX-License-Identifier: MIT
// Included inside the application's private namespace.
IoQualification* retainedIo(App& a, uint8_t address, uint8_t terminal, bool member) {
    if (member) {
        auto* m = groupMember(a, address);
        return m && terminal < 4 ? &m->ioQualifications[terminal] : nullptr;
    }
    return address == a.axis.target.address && terminal < 6 ? &a.ioQualifications[terminal] : nullptr;
}
void clearIoQualifications(App& a, uint8_t address) {
    if (address == a.axis.target.address)
        for (auto& q : a.ioQualifications) q = IoQualification();
    if (auto* m = groupMember(a, address))
        for (auto& q : m->ioQualifications) q = IoQualification();
}
bool ioIdle(const App& a) {
    return !a.owner.active() && !a.owner.pending() && !a.owner.recovering() &&
        !a.owner.configurationOwned() && !a.owner.commissioningOwned() && !a.discovery.owned &&
        !a.serial.configuring && !a.serial.blocked && !axisReserved(a) && !reading(a) && !acting(a) &&
        !groupSequencing(a);
}
// The raw status/polarity order is not specified by the manual. Only default
// unchanged polarity needs no mapping claim. Never invert a known AFTER value twice.
bool effectiveInput(uint16_t reported, uint8_t terminal, uint16_t oldPolarity,
                    uint16_t newPolarity, ESS::IoStatusMapping mapping, bool& active) {
    if (mapping > ESS::IoStatusMapping::AFTER_INVERSION || oldPolarity > 1 || newPolarity > 1) return false;
    const bool bit = (reported & (1u << terminal)) != 0;
    if (mapping == ESS::IoStatusMapping::UNKNOWN) {
        if (oldPolarity || newPolarity) return false;
        active = bit;
    } else if (mapping == ESS::IoStatusMapping::BEFORE_INVERSION) active = bit != bool(newPolarity);
    else active = bit != bool(oldPolarity ^ newPolarity);
    return true;
}
bool protectionFunction(uint16_t function) { return function >= 2 && function <= 6; }
// A checked identity read of this exact target and binding, within the
// observation age, as the control-settings model gate requires.
bool identityFresh(const App& a, const ESS::IdentityObservation& id, const MotorControlRS::ReadTarget& target, uint64_t now) {
    return id.operationId && Probe::sameTarget(id.target, target) && id.provenance.qualified && id.provenance.status &&
        id.provenance.latestUs <= now && MotorControlRS::evidenceAgeValid(id.provenance.attemptedUs, now, a.observationAgeUs());
}
enum class RecordLookup : uint8_t { FOUND, IDENTITY_REQUIRED, NOT_RECORDED };
// The recorded status mapping (IoStatusRecord.h) of one input of the selected
// axis or a group member. Only a fresh checked identity of that exact target
// selects a record; another model or firmware has none.
RecordLookup recordedInput(const App& a, uint8_t address, uint8_t terminal, bool member, uint64_t now,
                           ESS::IoStatusMapping& output) {
    if (terminal >= 4) return RecordLookup::NOT_RECORDED;
    const auto* m = member ? groupMember(const_cast<App&>(a), address) : nullptr;
    if (member ? !m : address != a.axis.target.address) return RecordLookup::IDENTITY_REQUIRED;
    const auto& target = m ? m->axis.target : a.axis.target;
    const ESS::IdentityObservation* identity = identityFresh(a, a.identity, target, now) ? &a.identity :
        m && identityFresh(a, m->identity, target, now) ? &m->identity : nullptr;
    if (!identity) return RecordLookup::IDENTITY_REQUIRED;
    for (const auto& record : RECORDED_INPUT_MAPPINGS)
        if (record.model == identity->rawModel && record.firmware == identity->rawVersion &&
            record.inputs[terminal] != ESS::IoStatusMapping::UNKNOWN) {
            output = record.inputs[terminal]; return RecordLookup::FOUND;
        }
    return RecordLookup::NOT_RECORDED;
}
// The recorded output status mapping (RECORDED_OUTPUT_MAPPINGS) of output 0..1
// of the selected axis, under the same fresh-identity rule as inputs. Group
// members have input records only.
RecordLookup recordedOutput(const App& a, uint8_t address, uint8_t output, uint64_t now,
                            ESS::IoStatusMapping& mapping) {
    if (output >= 2) return RecordLookup::NOT_RECORDED;
    if (address != a.axis.target.address) return RecordLookup::IDENTITY_REQUIRED;
    if (!identityFresh(a, a.identity, a.axis.target, now)) return RecordLookup::IDENTITY_REQUIRED;
    for (const auto& record : RECORDED_OUTPUT_MAPPINGS)
        if (record.model == a.identity.rawModel && record.firmware == a.identity.rawVersion &&
            record.outputs[output] != ESS::IoStatusMapping::UNKNOWN) {
            mapping = record.outputs[output]; return RecordLookup::FOUND;
        }
    return RecordLookup::NOT_RECORDED;
}
// One lookup for terminals 0..5 (X0..X3, Y0..Y1).
RecordLookup recordedMapping(const App& a, uint8_t address, uint8_t terminal, bool member, uint64_t now,
                             ESS::IoStatusMapping& output) {
    if (terminal < 4) return recordedInput(a, address, terminal, member, now, output);
    return member ? RecordLookup::NOT_RECORDED : recordedOutput(a, address, terminal - 4, now, output);
}
bool ioFactsValid(const IoQualification& q, uint64_t now) {
    using namespace MotorControlRS;
    if (!q.target.id || !q.configurationGeneration || q.purpose > IoPurpose::SEGMENT || q.terminal >= 6 ||
        q.wiring == InputWiring::UNKNOWN || q.wiring > InputWiring::CONNECTED || q.qualifiedUs > now ||
        !evidenceAgeValid(q.qualifiedUs, now, q.maximumAgeUs) || q.polarity > 1 || q.custom > 1) return false;
    if (q.terminal >= 4) {
        return q.purpose == IoPurpose::ASSIGN && q.output.supplied && !q.input.supplied &&
            q.output.statusMapping <= ESS::IoStatusMapping::AFTER_INVERSION &&
            q.output.load > ESS::IoOutputLoad::UNDECLARED && q.output.load <= ESS::IoOutputLoad::CONTROL &&
            q.output.allowedLevels && q.output.allowedLevels <= 3 &&
            (q.wiring != InputWiring::CONNECTED || q.output.load != ESS::IoOutputLoad::NO_LOAD) &&
            (q.wiring != InputWiring::UNCONNECTED || q.output.load == ESS::IoOutputLoad::NO_LOAD) &&
            (q.function <= 5 || q.function == 9 || q.function == 10);
    }
    if (!q.input.supplied || q.output.supplied || q.function > 17 || q.custom ||
        q.input.statusMapping > ESS::IoStatusMapping::AFTER_INVERSION) return false;
    if (q.purpose != IoPurpose::ASSIGN && (q.function != q.previousFunction || q.polarity != q.previousPolarity ||
        q.input.protectionRemovalQualified)) return false;
    if (q.purpose == IoPurpose::ASSIGN && q.wiring == InputWiring::CONNECTED && protectionFunction(q.previousFunction) &&
        (q.function != q.previousFunction || q.polarity != q.previousPolarity) && !q.input.protectionRemovalQualified) return false;
    bool active = false;
    // A no-function terminal may be inverted explicitly without a status mapping,
    // connected or not (wired bench 05): previous and final function are both 0,
    // so the level has no drive effect. Facts stay required and are checked here.
    if (!(q.function == 0 && q.previousFunction == 0) &&
        !effectiveInput(q.reportedInputs, q.terminal, q.previousPolarity, q.polarity, q.input.statusMapping, active)) return false;
    if (q.purpose == IoPurpose::ASSIGN) {
        if (q.function >= 4 && q.function <= 13 && active) return false;
        if ((q.function >= 4 || q.previousFunction >= 4) && !q.input.externalTriggersInhibited) return false;
        return true; // Core checks the entire FC06 sequence before any write.
    }
    // Wired bench 14: a FINITE_MOVE record of a speed, JOG or PV input (8-10, 13)
    // states its continued inhibition for the velocity and PV arms and for serial
    // velocity; serial moves still refuse those functions (moveInputPolicy).
    if (q.function >= 2 && q.function <= 13 && active) return false;
    // CONFIGURE and SEGMENT are separate stated purposes. Both require sources
    // inhibited for active controls; neither is inferred from move readiness.
    return q.function < 7 || q.input.externalTriggersInhibited;
}
bool ioSnapshot(App& a, uint8_t address, uint8_t terminal, IoPurpose purpose, bool member,
                uint64_t now, IoQualification& output) {
    if (terminal >= 6 || purpose > IoPurpose::SEGMENT || (terminal >= 4 && purpose != IoPurpose::ASSIGN) ||
        (member && (terminal >= 4 || purpose != IoPurpose::FINITE_MOVE))) return false;
    const auto* m = member ? groupMember(a, address) : nullptr;
    if (member ? !m : address != a.axis.target.address) return false;
    const auto& axis = m ? m->axis : a.axis;
    const auto& state = m ? m->state : a.stateCache;
    const auto& io = state.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    if (!axis.generation || axis.target.generation != a.bindingGeneration ||
        !Probe::fresh(io, axis.target, now, a.observationAgeUs()) || io.value.unknownInputBits || io.value.unknownOutputBits) return false;
    IoQualification q;
    q.target = axis.target; q.configurationGeneration = axis.generation;
    q.terminal = terminal; q.member = member; q.purpose = purpose;
    q.wiring = terminal < 4 ? (m ? m->inputWiring[terminal] : a.inputWiring[terminal]) : a.outputWiring[terminal - 4];
    q.stateOperationId = io.value.operationId;
    q.reportedInputs = io.value.rawInputs; q.reportedOutputs = io.value.rawOutputs;
    q.observedEarliestUs = io.observedEarliestUs; q.observedLatestUs = io.observedLatestUs;
    q.qualifiedUs = now; q.maximumAgeUs = a.observationAgeUs();
    if (purpose == IoPurpose::ASSIGN) {
        const auto& settings = a.ioSettings;
        if (!settings.operationId || settings.group != ESS::DriverGroup::IO ||
            settings.configurationGeneration != axis.generation || !Probe::sameTarget(settings.target, axis.target) ||
            (settings.raw[7] & ~15u) || (settings.raw[12] & ~3u) || (settings.raw[15] & ~3u)) return false;
        for (uint8_t i = 0; i < 3; ++i) {
            const auto& p = settings.provenance[i];
            if (!p.qualified || !p.status || p.latestUs > now ||
                !MotorControlRS::evidenceAgeValid(p.attemptedUs, now, a.observationAgeUs())) return false;
        }
        q.settingsOperationId = settings.operationId;
        q.previousFunction = settings.raw[terminal < 4 ? 8 + terminal : 13 + terminal - 4];
        q.previousPolarity = (settings.raw[terminal < 4 ? 7 : 12] >> (terminal < 4 ? terminal : terminal - 4)) & 1;
        if (terminal >= 4) q.previousCustom = (settings.raw[15] >> (terminal - 4)) & 1;
    } else {
        const auto& config = m ? m->configuration : a.configuration;
        if (!config.operationId || !Probe::sameTarget(config.target, axis.target) || config.unknownPolarityBits ||
            !config.inputFunctionKnown[terminal]) return false;
        for (const auto& p : config.provenance)
            if (!p.qualified || !p.status || p.latestUs > now ||
                !MotorControlRS::evidenceAgeValid(p.attemptedUs, now, a.observationAgeUs())) return false;
        q.settingsOperationId = config.operationId;
        q.previousFunction = config.raw.inputFunctions[terminal];
        q.previousPolarity = (config.raw.inputPolarity >> terminal) & 1;
    }
    q.function = q.previousFunction; q.polarity = q.previousPolarity; q.custom = q.previousCustom;
    output = q; return true;
}
bool sameIoBaseline(const IoQualification& a, const IoQualification& b, bool exactRead) {
    return Probe::sameTarget(a.target, b.target) && a.configurationGeneration == b.configurationGeneration &&
        a.terminal == b.terminal && a.member == b.member && a.purpose == b.purpose && a.wiring == b.wiring &&
        a.previousFunction == b.previousFunction && a.previousPolarity == b.previousPolarity && a.previousCustom == b.previousCustom &&
        (!exactRead || (a.settingsOperationId == b.settingsOperationId && a.stateOperationId == b.stateOperationId &&
          a.reportedInputs == b.reportedInputs && a.reportedOutputs == b.reportedOutputs &&
          a.observedEarliestUs == b.observedEarliestUs && a.observedLatestUs == b.observedLatestUs));
}
bool ioQualificationCurrent(App& a, const IoQualification& q, uint64_t now, bool exactRead = false) {
    IoQualification current;
    // Assignment uses the exact reads that admitted its candidate. Other purposes
    // may refresh live levels without renewing the supplied facts.
    return ioSnapshot(a, q.target.address, q.terminal, q.purpose, q.member, now, current) &&
        sameIoBaseline(q, current, exactRead || q.purpose == IoPurpose::ASSIGN) && ioFactsValid(q, now);
}
uint8_t moveIoMask(const IoQualification* facts) {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < 4; ++i)
        if (facts[i].target.id && facts[i].purpose == IoPurpose::FINITE_MOVE) mask |= 1u << i;
    return mask;
}
void copyIoTimes(ESS::DriverPrerequisites& p, const IoQualification& q) {
    // Keep the origin/age pair with the earliest expiry. Mixing one terminal's
    // origin with another terminal's age would invent a different lifetime.
    if (MotorControlRS::evidenceAgeDeadline(q.qualifiedUs, q.maximumAgeUs) <
        MotorControlRS::evidenceAgeDeadline(p.ioQualificationUs, p.ioQualificationMaxAgeUs)) {
        p.ioQualificationUs = q.qualifiedUs;
        p.ioQualificationMaxAgeUs = q.maximumAgeUs;
    } else if (!q.maximumAgeUs && !p.ioQualificationMaxAgeUs)
        p.ioQualificationUs = std::min(p.ioQualificationUs, q.qualifiedUs);
}
void liveIoPrerequisites(const App& a, ESS::DriverPrerequisites& p, uint64_t now) {
    const auto& io = a.stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    p.ioLevelsQualified = Probe::fresh(io, a.axis.target, now, a.observationAgeUs()) &&
        !io.value.unknownInputBits && !io.value.unknownOutputBits;
    p.ioTarget = a.axis.target; p.ioConfigurationGeneration = a.axis.generation;
    p.actualInputs = io.value.rawInputs; p.actualOutputs = io.value.rawOutputs;
    p.ioEarliestUs = io.observedEarliestUs; p.ioLatestUs = io.observedLatestUs;
}
// Whether the selected axis's four input records of one purpose admit a settings
// write, and if not, the first cause in this order: a terminal has no record of
// the purpose, a record no longer matches current reads/binding/age, an assigned
// limit/stop/control input reads active. Facts are never inferred from a read;
// only the live level is taken from the latest state read. p changes only on
// PERMITTED.
enum class SettingsInputs : uint8_t { PERMITTED, FACTS_REQUIRED, FACTS_STALE, INPUT_ACTIVE };
struct SettingsInputCheck {
    SettingsInputs result = SettingsInputs::FACTS_REQUIRED;
    uint8_t terminal = 0;
    const char* readFirst = "read config";
};
SettingsInputCheck settingsInputFacts(App& a, IoPurpose purpose, ESS::DriverPrerequisites& p, uint64_t now) {
    SettingsInputCheck check;
    for (uint8_t i = 0; i < 4; ++i) {
        const auto& q = a.ioQualifications[i];
        if (!q.target.id || q.purpose != purpose) { check.terminal = i; return check; }
    }
    const auto& io = a.stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    for (uint8_t i = 0; i < 4; ++i) {
        if (ioQualificationCurrent(a, a.ioQualifications[i], now)) continue;
        const auto& config = a.configuration;
        bool configured = config.operationId && Probe::sameTarget(config.target, a.axis.target);
        for (const auto& e : config.provenance)
            configured = configured && e.qualified && e.status && e.latestUs <= now &&
                MotorControlRS::evidenceAgeValid(e.attemptedUs, now, a.observationAgeUs());
        check.result = SettingsInputs::FACTS_STALE; check.terminal = i;
        check.readFirst = configured && !Probe::fresh(io, a.axis.target, now, a.observationAgeUs()) ? "read state" : "read config";
        return check;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        const auto& q = a.ioQualifications[i];
        bool active = false;
        // Without a status mapping only unchanged default polarity has a known level.
        if (!effectiveInput(io.value.rawInputs, i, q.polarity, q.polarity, q.input.statusMapping, active)) {
            check.terminal = i; return check;
        }
        if (q.function >= 2 && q.function <= 13 && active) {
            check.result = SettingsInputs::INPUT_ACTIVE; check.terminal = i; check.readFirst = "read state";
            return check;
        }
    }
    liveIoPrerequisites(a, p, now);
    p.ioQualificationUs = now;
    for (uint8_t i = 0; i < 4; ++i) {
        p.inputFacts[i] = a.ioQualifications[i].input;
        p.inputWiring[i] = a.inputWiring[i]; copyIoTimes(p, a.ioQualifications[i]);
    }
    if (p.ioLevelsQualified) check.result = SettingsInputs::PERMITTED;
    else { check.result = SettingsInputs::FACTS_STALE; check.readFirst = "read state"; }
    return check;
}
bool anySettingsRecord(const App& a, IoPurpose purpose) {
    for (uint8_t i = 0; i < 4; ++i)
        if (a.ioQualifications[i].target.id && a.ioQualifications[i].purpose == purpose) return true;
    return false;
}
// Record the cause for the console and the API. The passive unconnected path
// keeps its unnamed refusal unless the terminals need facts or some exist.
void noteSettingsRefusal(App& a, const SettingsInputCheck& check, IoPurpose purpose) {
    a.settingsRefusal = Probe::SettingsRefusal();
    if (check.result == SettingsInputs::PERMITTED) return;
    a.settingsRefusal.reason = check.result == SettingsInputs::FACTS_REQUIRED ? "input_facts_required" :
        check.result == SettingsInputs::FACTS_STALE ? "input_facts_stale" : "input_active";
    a.settingsRefusal.readFirst = check.readFirst;
    a.settingsRefusal.purpose = purpose == IoPurpose::SEGMENT ? "segment" : "configure";
    a.settingsRefusal.terminal = static_cast<int8_t>(check.terminal);
}
Probe::QualificationReply ioQualificationCommand(void*, const Probe::IoQualificationCommand& command,
                                                 IoQualification& output, bool& valid) {
    using Probe::Action;
    using Kind = Probe::IoQualificationCommand::Kind;
    valid = false;
    if (command.kind == Kind::INSPECT) {
        if (!copyIoQualification(command.address, command.terminal, command.purpose, output, valid, command.member))
            output = IoQualification();
        return Probe::QualificationReply(Action::OK);
    }
    if (command.kind == Kind::REVOKE) {
        const auto result = revokeIoQualification(command.address, command.terminal, command.purpose, command.member);
        return Probe::QualificationReply(result ? Action::OK : Action::INVALID);
    }
    IoQualification q;
    if (!inspectIoQualification(command.address, command.terminal, command.purpose, q, command.member)) {
        Probe::QualificationReply result(Action::QUALIFICATION_REQUIRED);
        result.reason = "reads_required";
        result.readFirst = command.member ? "group read state" : command.purpose == IoPurpose::ASSIGN ? "io read" : "read config";
        return result;
    }
    q.function = command.function; q.polarity = command.polarity; q.custom = command.custom;
    q.input = command.input; q.output = command.output;
    auto& stated = command.terminal < 4 ? q.input.statusMapping : q.output.statusMapping;
    if (command.recordedMapping || stated != ESS::IoStatusMapping::UNKNOWN) {
        ESS::IoStatusMapping recorded = ESS::IoStatusMapping::UNKNOWN;
        const auto found = app ? recordedMapping(*app, command.address, command.terminal, command.member, uart.sample(), recorded) :
            RecordLookup::IDENTITY_REQUIRED;
        if (command.recordedMapping && found != RecordLookup::FOUND) {
            Probe::QualificationReply result(Action::QUALIFICATION_REQUIRED);
            result.reason = "mapping_not_recorded";
            if (found == RecordLookup::IDENTITY_REQUIRED) result.readFirst = "read identity";
            return result;
        }
        if (!command.recordedMapping && found == RecordLookup::FOUND && recorded != stated) {
            Probe::QualificationReply result(Action::QUALIFICATION_REQUIRED);
            result.reason = "mapping_contradicts_record";
            return result;
        }
        if (command.recordedMapping) stated = recorded;
    }
    const auto result = installIoQualification(q);
    return Probe::QualificationReply(result ? Action::OK :
        result.detail == static_cast<int32_t>(Action::BUSY) ? Action::BUSY : Action::QUALIFICATION_REQUIRED);
}
