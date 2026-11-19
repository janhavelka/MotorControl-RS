// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.
// Position limits of the selected axis (wired bench 16, D6/D12; PositionLimitsRoute.h).
// The core defaults apply until an explicit, named install. An install is bound
// to the selected target, its binding generation, the configuration generation
// and the host serial generation, so `recover`, `useaddr`, a configuration
// change and a host tuple change end it, as do `position limits default` and a
// restart (RAM only). No call sends a frame.

bool positionLimitsCurrent(const App& a) {
    const auto& s = a.positionLimits;
    return s.installed && a.axis.target.id && s.targetId == a.axis.target.id && s.address == a.axis.target.address &&
        s.bindingGeneration == a.bindingGeneration && a.axis.generation && s.configurationGeneration == a.axis.generation &&
        s.serialGeneration == a.serial.generation;
}
void copyPositionLimitsCase(char (&out)[Probe::POSITION_LIMITS_CASE_CAPACITY], const char* name) {
    std::size_t i = 0;
    for (; name && i + 1 < Probe::POSITION_LIMITS_CASE_CAPACITY && name[i]; ++i) out[i] = name[i];
    for (; i < Probe::POSITION_LIMITS_CASE_CAPACITY; ++i) out[i] = 0;
}
// A selected-axis finite move or stage uses the install while it is current.
// Group members, input-started moves and homing never call this: they keep the
// core defaults. The admitted record keeps the case name for its result.
void applyPositionLimits(const App& a, ESS::MovePrerequisites& prerequisites) {
    if (positionLimitsCurrent(a)) prerequisites.limits = a.positionLimits.limits;
}
void notePositionLimitsCase(const App& a, App::Record& record) {
    copyPositionLimitsCase(record.positionLimitsCase, positionLimitsCurrent(a) ? a.positionLimits.caseName : "");
}
void positionLimitsView(const App& a, Probe::PositionLimitsView& v) {
    v = Probe::PositionLimitsView();
    const bool current = positionLimitsCurrent(a);
    v.address = a.axis.target.address; v.subdivision = selectedSubdivision(a);
    v.source = current ? Probe::PositionLimitsSource::INSTALLED : Probe::PositionLimitsSource::DEFAULT;
    if (current) {
        v.limits = a.positionLimits.limits;
        copyPositionLimitsCase(v.caseName, a.positionLimits.caseName);
        v.installBinding = a.positionLimits.bindingGeneration;
        v.installConfiguration = a.positionLimits.configurationGeneration;
    }
    v.maximumRpm = ESS::positionSpeedLimit(v.subdivision, v.limits);
    v.defaultRpm = ESS::positionSpeedLimit(v.subdivision);
}
// The work of installPositionLimits, inspectPositionLimits and revokePositionLimits.
Probe::Action positionLimitsWork(App& a, const Probe::PositionLimitsCommand* command, Probe::PositionLimitsView& out) {
    using Kind = Probe::PositionLimitsCommandKind;
    const char* reason = "none";
    Probe::Action result = Probe::Action::OK;
    auto& s = a.positionLimits;
    if (!positionLimitsCurrent(a)) s = App::PositionLimitsState();
    if (command && command->kind == Kind::DEFAULT) {
        s = App::PositionLimitsState();
    } else if (command && command->kind == Kind::SET) {
        if (!Probe::positionLimitsValid(command->limits)) { result = Probe::Action::INVALID; reason = "outside_wire_ranges"; }
        else if (!Probe::positionLimitsCaseValid(command->caseName)) { result = Probe::Action::INVALID; reason = "invalid_case"; }
        else if (!a.axis.target.id || !a.axis.generation) { result = Probe::Action::UNAVAILABLE; reason = "target_unknown"; }
        else {
            s.installed = true; s.limits = command->limits;
            copyPositionLimitsCase(s.caseName, command->caseName);
            s.targetId = a.axis.target.id; s.address = a.axis.target.address;
            s.bindingGeneration = a.bindingGeneration; s.configurationGeneration = a.axis.generation;
            s.serialGeneration = a.serial.generation;
        }
    }
    positionLimitsView(a, out);
    out.reason = reason;
    return result;
}
// Console route `position limits`: the same public functions upper firmware calls.
Probe::Action positionLimitsCommand(void*, const Probe::PositionLimitsCommand* command, Probe::PositionLimitsView& out) {
    using Kind = Probe::PositionLimitsCommandKind;
    if (command && command->kind == Kind::SET) {
        const auto installed = installPositionLimits(*command, out);
        return installed ? Probe::Action::OK : static_cast<Probe::Action>(installed.detail);
    }
    if (command && command->kind == Kind::DEFAULT) revokePositionLimits();
    inspectPositionLimits(out);
    return Probe::Action::OK;
}
// The case an admitted move was prepared with; "" for the core defaults.
const char* positionLimitsCaseOf(void* context, uint32_t operationId) {
    for (const auto& r : static_cast<App*>(context)->records)
        if (r.operationId == operationId && r.kind == App::OperationKind::MOVE) return r.positionLimitsCase;
    return "";
}
