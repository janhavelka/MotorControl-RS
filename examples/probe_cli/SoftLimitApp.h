// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.
//
// Wired bench 11: the drive's soft-limit state for the selected axis, per
// target and configuration generation.
// - KNOWN_DISABLED: a current configuration read of this target shows 0x0018 = 0.
//   Every possibly accepted DRIVE write invalidates that read
//   (invalidateDriverAssumptions), so a current read is newer than any write.
// - BOUND: a current configuration read, newer than this application's own
//   enable write, shows 0x0018 = 1, and that write read back 1 with its
//   binding still current: the same target, binding and axis generation as the
//   write's completion, the home it was derived from (method 35 with the
//   `soft-limits` and `reference` facts), both pairs read back and decoded at
//   word order 0, the host axis soft limits installed after the readback, and
//   HOMED set and not released in a fresh state.
// - UNKNOWN: everything else: startup before a read, a lost ACK on 0x0018 or a
//   pair, any pair change, a position clear, a release, a home start, reference
//   loss, a geometry or generation change, and 0x0018 = 1 read without the binding.
// While UNKNOWN every motion start and arm refuses before TX; reads, stops, the
// disable write and the enable route stay admitted. A home needs KNOWN_DISABLED.
enum class SoftLimitState : uint8_t { UNKNOWN, KNOWN_DISABLED, BOUND };

SoftLimitState softLimitState(const App& a, uint64_t now) {
    const auto& c = a.configuration;
    if (!a.axis.generation || !c.operationId || !Probe::sameTarget(c.target, a.axis.target)) return SoftLimitState::UNKNOWN;
    if (c.raw.softLimitEnable == 0) return SoftLimitState::KNOWN_DISABLED;
    if (!c.softLimitEnableKnown) return SoftLimitState::UNKNOWN;
    const auto& b = a.softLimitBinding;
    const auto& motion = a.stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    const bool bound = b.operationId && Probe::sameTarget(b.target, a.axis.target) &&
        b.axisGeneration == a.axis.generation && b.bindingGeneration == a.bindingGeneration &&
        c.operationId > b.operationId && a.axis.softLimitsKnown &&
        a.axis.softMinimum == b.minimum && a.axis.softMaximum == b.maximum &&
        Probe::fresh(motion, a.axis.target, now, a.observationAgeUs()) &&
        motion.value.homingComplete && !motion.value.released;
    return bound ? SoftLimitState::BOUND : SoftLimitState::UNKNOWN;
}
// Record the named refusal of a motion start that the soft-limit state decided.
void noteSoftLimitRefusal(App& a, const char* reason) {
    a.settingsRefusal = Probe::SettingsRefusal();
    a.settingsRefusal.reason = reason;
    a.settingsRefusal.readFirst = "read config";
    a.settingsRefusal.purpose = "motion";
}
// Enable route (0x0018 = 1): derive the core's prerequisites from the
// application's own evidence only. The home is the last one that published a
// reference with the `soft-limits` fact in this binding and generation; the
// pair words are a checked `driver read` taken after that home and decoded with
// the measured order-0 encoding (wired bench 10). limitSemanticsQualified
// stands for the ledger record of 0x0018 on 0x4EEA/0x0029 (wired bench 10 E3,
// wired bench 11 S cases); other models or firmware leave it false. Nothing
// here writes; the core checks HOMED, release, ordering and range.
void softLimitEnablePrerequisites(App& a, ESS::DriverPrerequisites& p, const MotorControlRS::ReadTarget& target,
                                  uint32_t generation) {
    a.softLimitPending = App::SoftLimitBinding();
    const auto& h = a.softLimitHome;
    const auto& d = a.driverSettings;
    const bool model = a.identity.rawModel == 0x4EEA && a.identity.rawVersion == 0x0029;
    if (!model || !h.stated || !h.operationId || !Probe::sameTarget(h.target, target) || h.axisGeneration != generation ||
        h.bindingGeneration != a.bindingGeneration || !d.operationId || d.operationId < h.operationId ||
        !Probe::sameTarget(d.target, target) || d.group != ESS::DriverGroup::DRIVE ||
        d.configurationGeneration != generation || !d.pairKnown || !d.limitsDecoded) return;
    p.limitSemanticsQualified = p.homedReferenceQualified = true;
    p.referenceTarget = target; p.referenceConfigurationGeneration = generation;
    std::memcpy(p.qualifiedPositiveWords, d.positiveWords, sizeof(p.qualifiedPositiveWords));
    std::memcpy(p.qualifiedNegativeWords, d.negativeWords, sizeof(p.qualifiedNegativeWords));
    p.positiveLimit = d.positiveLimit; p.negativeLimit = d.negativeLimit;
    auto& pending = a.softLimitPending;
    pending.target = target; pending.bindingGeneration = a.bindingGeneration; pending.homeOperationId = h.operationId;
    pending.minimum = d.negativeLimit; pending.maximum = d.positiveLimit;
    std::memcpy(pending.positiveWords, d.positiveWords, sizeof(pending.positiveWords));
    std::memcpy(pending.negativeWords, d.negativeWords, sizeof(pending.negativeWords));
}
// A terminal DRIVE update that wrote 0x0018. Any such write ends the previous
// binding. Only this application's admitted enable with a confirmed readback
// of 1 installs a new one, and the host axis soft limits with it, after the
// write's own effects advanced the generation: never before the readback.
void noteSoftLimitWrite(App& a, const App::Record& r) {
    if (r.driver.kind != ESS::DriverKind::UPDATE || r.driver.group != ESS::DriverGroup::DRIVE ||
        !(r.driver.request.fields & static_cast<uint32_t>(ESS::DriverField::SOFT_LIMIT_ENABLE))) return;
    a.softLimitBinding = App::SoftLimitBinding();
    const bool enabled = r.driver.request.softLimitEnable == ESS::SoftLimitEnable::AFTER_HOMING &&
        r.driver.state == MotorControlRS::ReadState::SUCCEEDED && !r.driver.uncertain &&
        a.softLimitPending.operationId == r.operationId && r.address == a.axis.target.address &&
        Probe::sameTarget(a.softLimitPending.target, a.axis.target) && a.axis.generation;
    if (!enabled) {
        if (r.address == a.axis.target.address) a.axis.softLimitsKnown = false;
        if (a.softLimitPending.operationId == r.operationId) a.softLimitPending = App::SoftLimitBinding();
        return;
    }
    a.softLimitBinding = a.softLimitPending;
    a.softLimitBinding.axisGeneration = a.axis.generation;
    a.softLimitPending = App::SoftLimitBinding();
    a.axis.softLimitsKnown = true;
    a.axis.softMinimum = a.softLimitBinding.minimum; a.axis.softMaximum = a.softLimitBinding.maximum;
    if (!MotorControlRS::validateAxisConfig(a.axis)) { a.axis.softLimitsKnown = false; a.softLimitBinding = App::SoftLimitBinding(); }
}
// The binding still belongs to the current target, binding and generation.
bool softLimitBindingCurrent(const App& a) {
    const auto& b = a.softLimitBinding;
    return b.operationId && Probe::sameTarget(b.target, a.axis.target) && b.axisGeneration == a.axis.generation &&
        b.bindingGeneration == a.bindingGeneration && a.axis.softLimitsKnown;
}
// After a checked read reported only 0x0018 changed against a pre-write cache
// (invalidateDriverAssumptions): carry the binding to the new generation.
void rebaseSoftLimitBinding(App& a) {
    if (!a.axis.generation) { a.softLimitBinding = App::SoftLimitBinding(); return; }
    a.softLimitBinding.axisGeneration = a.axis.generation;
    a.axis.softLimitsKnown = true;
    a.axis.softMinimum = a.softLimitBinding.minimum; a.axis.softMaximum = a.softLimitBinding.maximum;
    if (!MotorControlRS::validateAxisConfig(a.axis)) { a.axis.softLimitsKnown = false; a.softLimitBinding = App::SoftLimitBinding(); }
}
// A published configuration read that differs from the previous one only in
// 0x0018 (and whose cache effects name nothing else) while the binding is current.
bool softLimitReadRebase(const App& a, const ESS::RawConfig& old, const ESS::RawConfig& updated, uint32_t effects) {
    ESS::RawConfig compared = old;
    compared.softLimitEnable = updated.softLimitEnable;
    return softLimitBindingCurrent(a) && updated.softLimitEnable == 1 &&
        !(effects & ~static_cast<uint32_t>(ESS::DriverField::SOFT_LIMIT_ENABLE)) &&
        std::memcmp(&compared, &updated, sizeof(compared)) == 0;
}
// A home that published a reference: remember it for the enable route. The
// `soft-limits` fact was stated in the home qualification that admitted it.
void noteSoftLimitHome(App& a, const App::Record& r) {
    a.softLimitHome = App::SoftLimitHome();
    if (!r.homeSoftLimits || r.home.request.method != ESS::HomingMethod::METHOD_35) return;
    a.softLimitHome.target = a.axis.target; a.softLimitHome.operationId = r.operationId;
    a.softLimitHome.axisGeneration = a.axis.generation; a.softLimitHome.bindingGeneration = a.bindingGeneration;
    a.softLimitHome.stated = true;
}
