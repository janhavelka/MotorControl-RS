// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.
// Velocity limits of the selected axis (wired bench 14, D6/D12; VelocityLimits.h).
// The legacy envelope applies until an explicit call; a qualification cell or a
// raise is bound to the selected address, its binding and the host serial
// generation, so `useaddr`, `recover` and a host tuple change drop it, as do
// `velocity limits reset` and a restart (RAM only). No call sends a frame.

bool velocityLimitsCurrent(const App& a) {
    const auto& s = a.velocityLimits;
    return (s.cellArmed || s.raised) && s.address == a.axis.target.address &&
        s.bindingGeneration == a.bindingGeneration && s.serialGeneration == a.serial.generation;
}
// Checked subdivision of the selected axis, 0 when unknown.
uint16_t selectedSubdivision(const App& a) {
    const auto& c = a.configuration;
    return c.operationId && Probe::sameTarget(c.target, a.axis.target) ? c.raw.subdivision : 0;
}
uint16_t promotedMaximumRpm(const App& a) {
    uint16_t maximum = 0;
    for (std::size_t i = 0; a.velocityPromoted && i < a.velocityPromotedCount; ++i) {
        const auto& e = a.velocityPromoted[i];
        const uint16_t magnitude = static_cast<uint16_t>(e.rpm < 0 ? -e.rpm : e.rpm);
        if (Probe::velocityCellValid(e) && magnitude > maximum) maximum = magnitude;
    }
    return maximum;
}
/** Largest |rpm| an input-started run (functions 8-10, 13) and the JOG setter may
 * use: the legacy envelope only. Raised limits are serial-only (wired bench 14
 * audit F1/F2): promoted cells name a direction, ramps, a subdivision and a
 * duration that an input run or a stored JOG speed does not have, and no
 * input-specific promotion policy exists. A cell never widens them either. */
uint16_t inputVelocityMaximumRpm(const App&) { return Probe::VELOCITY_LEGACY_MAX_RPM; }
void velocityLimitsView(const App& a, Probe::VelocityLimitsView& v) {
    v = Probe::VelocityLimitsView();
    const auto& s = a.velocityLimits;
    const bool current = velocityLimitsCurrent(a);
    v.address = a.axis.target.address; v.subdivision = selectedSubdivision(a);
    v.cellArmed = current && s.cellArmed; v.raised = current && s.raised;
    if (v.cellArmed) v.cell = s.cell;
    v.consumedOperationId = s.consumedOperationId;
    v.promotedCount = static_cast<uint8_t>(a.velocityPromoted ? a.velocityPromotedCount : 0);
    v.source = v.cellArmed ? Probe::VelocityLimitsSource::CELL : v.raised ? Probe::VelocityLimitsSource::RAISED :
        Probe::VelocityLimitsSource::LEGACY;
    auto stop = Probe::VelocityStop::NORMAL;
    if (v.cellArmed) {
        const uint16_t magnitude = static_cast<uint16_t>(s.cell.rpm < 0 ? -s.cell.rpm : s.cell.rpm);
        v.maximumRpm = std::max<uint16_t>(v.maximumRpm, magnitude);
        v.maximumDurationMs = std::max(v.maximumDurationMs, s.cell.durationMs);
        stop = s.cell.stop;
    }
    if (v.raised) {
        v.maximumRpm = std::max(v.maximumRpm, promotedMaximumRpm(a));
        for (std::size_t i = 0; a.velocityPromoted && i < a.velocityPromotedCount; ++i)
            v.maximumDurationMs = std::max(v.maximumDurationMs, a.velocityPromoted[i].durationMs);
    }
    v.maximumRate = v.subdivision ? Probe::velocityRate(v.maximumRpm, v.subdivision) : 0;
    v.pollMs = Probe::velocityPollMs(v.maximumDurationMs, stop);
    v.stopBudgetMs = Probe::velocityStopBudgetMs(stop);
}
// The run the latest cell admitted is still active (audit F4: no raise meanwhile).
bool cellRunActive(const App& a) {
    const uint32_t id = a.velocityLimits.consumedOperationId;
    if (!id) return false;
    for (const auto& r : a.records)
        if (r.operationId == id && r.kind == App::OperationKind::VELOCITY && !terminal(a, r)) return true;
    return false;
}
void bindVelocityLimits(App& a) {
    auto& s = a.velocityLimits;
    s.address = a.axis.target.address; s.bindingGeneration = a.bindingGeneration; s.serialGeneration = a.serial.generation;
}
// Admission of one serial run: legacy, the armed cell's exact run, or (raised)
// the promoted table. exactMs: the request duration is a whole number of ms.
Probe::VelocityAdmission velocityRunAdmission(const App& a, const Probe::VelocityCell& run, bool exactMs) {
    if (Probe::velocityLegacyRun(run)) return Probe::VelocityAdmission::LEGACY;
    if (!exactMs || !run.subdivision || !velocityLimitsCurrent(a)) return Probe::VelocityAdmission::REFUSED;
    const auto& s = a.velocityLimits;
    if (s.cellArmed && Probe::velocityCellMatches(s.cell, run)) return Probe::VelocityAdmission::CELL;
    if (s.raised) return Probe::velocityPromotedAdmission(a.velocityPromoted, a.velocityPromotedCount, run);
    return Probe::VelocityAdmission::REFUSED;
}
// The admitted run consumes the cell: exactly one run per qualification.
void consumeVelocityCell(App& a, uint32_t operationId) {
    a.velocityLimits.cellArmed = false; a.velocityLimits.cell = Probe::VelocityCell();
    a.velocityLimits.consumedOperationId = operationId;
}
void noteVelocityLimitsRefusal(App& a) {
    a.settingsRefusal = Probe::SettingsRefusal();
    a.settingsRefusal.reason = "outside_velocity_limits"; a.settingsRefusal.purpose = "velocity_limits";
    a.settingsRefusal.readFirst = "velocity limits"; a.settingsRefusal.terminal = -1;
}
Probe::Action velocityLimitsCommand(void* context, const Probe::VelocityLimitsCommand* command, Probe::VelocityLimitsView& out) {
    App& a = *static_cast<App*>(context);
    using Kind = Probe::VelocityLimitsCommandKind;
    const char* reason = "none";
    Probe::Action result = Probe::Action::OK;
    auto& s = a.velocityLimits;
    if (!velocityLimitsCurrent(a)) { s.cellArmed = s.raised = false; s.cell = Probe::VelocityCell(); }
    if (command && command->kind == Kind::RESET) {
        s.cellArmed = s.raised = false; s.cell = Probe::VelocityCell();
    } else if (command && command->kind == Kind::QUALIFY) {
        const auto& c = command->cell;
        const uint16_t subdivision = selectedSubdivision(a);
        if (!Probe::velocityCellValid(c)) { result = Probe::Action::INVALID; reason = "invalid_cell"; }
        else if (Probe::velocityLegacyRun(c)) { result = Probe::Action::INVALID; reason = "inside_legacy_limits"; }
        else if (!a.axis.target.id || !subdivision) { result = Probe::Action::UNAVAILABLE; reason = "configuration_stale"; }
        else if (subdivision != c.subdivision) { result = Probe::Action::UNAVAILABLE; reason = "subdivision_mismatch"; }
        else if (s.raised) { result = Probe::Action::BUSY; reason = "raise_active"; } // Exclusive (audit F4).
        else if (s.cellArmed) { result = Probe::Action::BUSY; reason = "cell_armed"; }
        else { s.cell = c; s.cell.standalone = false; s.cellArmed = true; bindVelocityLimits(a); }
    } else if (command && command->kind == Kind::RAISE) {
        if (s.cellArmed) { result = Probe::Action::BUSY; reason = "cell_armed"; } // Exclusive (audit F4).
        else if (cellRunActive(a)) { result = Probe::Action::BUSY; reason = "cell_run_active"; }
        else if (!a.velocityPromoted || !a.velocityPromotedCount) { result = Probe::Action::UNAVAILABLE; reason = "no_promoted_cells"; }
        else if (!a.axis.target.id) { result = Probe::Action::UNAVAILABLE; reason = "target_unknown"; }
        else { s.raised = true; bindVelocityLimits(a); }
    }
    velocityLimitsView(a, out);
    out.reason = reason;
    return result;
}
// Wired bench 14: functions 8 (speed input), 9/10 (JOG) and 13 (PV) on a
// connected terminal act on stored speed words; a serial velocity run rewrites
// 0x001D. Each such terminal needs a current FINITE_MOVE record with triggers
// inhibited and a fresh inactive level. Null when clear; otherwise the reason.
const char* velocityInputBlocked(const App& a, uint64_t now) {
    const auto& c = a.configuration;
    const auto& io = a.stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
    for (uint8_t t = 0; t < 4; ++t) {
        const uint16_t f = c.raw.inputFunctions[t];
        if (!((f >= 8 && f <= 10) || f == 13)) continue;
        // Prompt 02: a route that reads a declaration refuses UNKNOWN (an address
        // outside the board table, or an invalid table); review F4.
        if (c.wiring[t] == MotorControlRS::InputWiring::UNKNOWN) return "wiring_unknown";
        if (c.wiring[t] != MotorControlRS::InputWiring::CONNECTED) continue;
        const auto& q = a.ioQualifications[t];
        const uint16_t polarity = (c.raw.inputPolarity >> t) & 1u;
        if (!q.target.id || q.purpose != IoPurpose::FINITE_MOVE || q.terminal != t || !Probe::sameTarget(q.target, a.axis.target) ||
            q.configurationGeneration != a.axis.generation || q.function != f || q.polarity != polarity ||
            !ioFactsValid(q, now) || !q.input.externalTriggersInhibited) return "triggers_inhibited_required";
        bool active = false;
        if (!Probe::fresh(io, a.axis.target, now, a.observationAgeUs()) || io.value.unknownInputBits ||
            !effectiveInput(io.value.rawInputs, t, polarity, polarity, q.input.statusMapping, active)) return "input_state_stale";
        if (active) return "input_active";
    }
    return nullptr;
}
