// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace, only in an
// image built with MOTORCONTROLRS_WB10_EXPERIMENT (an experimental image).
// Production images do not contain this admission.
//
// Named experiment admission, decision D4, wired bench 10, 19 November 2026,
// under the AGENTS experimental-window exception. What remains after the
// prompt 10 promotion is the stored PT target experiment, kept for prompt 13's
// W0 witness (13 adds its own candidate words and removes this when it promotes
// or refuses the PT windows):
// - PAIR_EXPERIMENT: one FC10 of two words to a ledger experiment window (the
//   sixteen PT target pairs; Codec buildExperimentWrite), then one two-word
//   FC03 readback; no replay. Words: one of the fixed candidates below for
//   that window and drive, or the exact words of an earlier checked read of the
//   same window, drive, binding and host tuple (D7 restoration; the first read
//   of the binding is pinned, three later reads rotate).
//   Drive 4 for every candidate; drives 1 and 2 only for segment 1 (P4).
//   Requires a fresh stopped, alarm-free state, a fresh configuration read with
//   0x0018 known 0 and no input assigned function 12 (PT trigger).
// The soft-limit pair candidates, the 0x0018 = 1 admission (E3) and the
// E1/E2 home offset admission of the experimental images A-A3 were removed
// with the promotion and the ledger record (report of wired bench 10).
constexpr uint8_t PAIR_DRIVE_1 = 1, PAIR_DRIVE_2 = 2, PAIR_DRIVE_4 = 4;
struct PairCandidate {
    uint16_t first;
    uint16_t words[2];
    uint8_t drives; ///< Bit mask of PAIR_DRIVE_* that may receive it.
};
constexpr uint8_t PAIR_ALL = PAIR_DRIVE_1 | PAIR_DRIVE_2 | PAIR_DRIVE_4;
// Wired bench 10's fixed list (P2-P4) and the 01 baseline words [0,5000]
// (wired_bench_baseline.json, 18 November) as named restoration targets.
const PairCandidate PAIR_CANDIDATES[] = {
    {0x0060, {0x0001, 0x1001}, PAIR_ALL}, {0x0060, {0xFFFF, 0xFE0C}, PAIR_DRIVE_4},
    {0x0066, {0x0002, 0x1002}, PAIR_DRIVE_4}, {0x006C, {0x0003, 0x1003}, PAIR_DRIVE_4},
    {0x0072, {0x0004, 0x1004}, PAIR_DRIVE_4}, {0x0078, {0x0005, 0x1005}, PAIR_DRIVE_4},
    {0x007E, {0x0006, 0x1006}, PAIR_DRIVE_4}, {0x0084, {0x0007, 0x1007}, PAIR_DRIVE_4},
    {0x008A, {0x0008, 0x1008}, PAIR_DRIVE_4}, {0x0090, {0x0009, 0x1009}, PAIR_DRIVE_4},
    {0x0096, {0x000A, 0x100A}, PAIR_DRIVE_4}, {0x009C, {0x000B, 0x100B}, PAIR_DRIVE_4},
    {0x00A2, {0x000C, 0x100C}, PAIR_DRIVE_4}, {0x00A8, {0x000D, 0x100D}, PAIR_DRIVE_4},
    {0x00AE, {0x000E, 0x100E}, PAIR_DRIVE_4}, {0x00B4, {0x000F, 0x100F}, PAIR_DRIVE_4},
    {0x00BA, {0x0010, 0x1010}, PAIR_DRIVE_4},
    {0x0060, {0x0000, 0x1388}, PAIR_ALL}, {0x0066, {0x0000, 0x1388}, PAIR_DRIVE_4},
    {0x006C, {0x0000, 0x1388}, PAIR_DRIVE_4}, {0x0072, {0x0000, 0x1388}, PAIR_DRIVE_4},
    {0x0078, {0x0000, 0x1388}, PAIR_DRIVE_4}, {0x007E, {0x0000, 0x1388}, PAIR_DRIVE_4},
    {0x0084, {0x0000, 0x1388}, PAIR_DRIVE_4}, {0x008A, {0x0000, 0x1388}, PAIR_DRIVE_4},
    {0x0090, {0x0000, 0x1388}, PAIR_DRIVE_4}, {0x0096, {0x0000, 0x1388}, PAIR_DRIVE_4},
    {0x009C, {0x0000, 0x1388}, PAIR_DRIVE_4}, {0x00A2, {0x0000, 0x1388}, PAIR_DRIVE_4},
    {0x00A8, {0x0000, 0x1388}, PAIR_DRIVE_4}, {0x00AE, {0x0000, 0x1388}, PAIR_DRIVE_4},
    {0x00B4, {0x0000, 0x1388}, PAIR_DRIVE_4}, {0x00BA, {0x0000, 0x1388}, PAIR_DRIVE_4},
};

uint8_t pairDriveBit(uint8_t address) {
    return address == 1 ? PAIR_DRIVE_1 : address == 2 ? PAIR_DRIVE_2 : address == 4 ? PAIR_DRIVE_4 : 0;
}
// Slot of a PT window: 0..15 for segments 1..16.
int pairSlot(uint16_t first) {
    if (first >= 0x0060 && first <= 0x00BA && (first - 0x0060) % 6 == 0) return (first - 0x0060) / 6;
    return -1;
}
void rememberPairWords(App& a, uint16_t first, uint32_t operationId, const MotorControlRS::ReadTarget& target, const uint16_t* words) {
    const int slot = pairSlot(first);
    if (slot < 0) return;
    // Slot 0 keeps the first checked read of the binding (wired bench 06's
    // pinned-slot rule); later reads rotate through slots 1..3 (wired bench 10
    // P1 first run: a single latest slot lost the run's source read).
    auto* ring = a.pairSnapshots[slot];
    const bool pin = !ring[0].operationId || !Probe::sameTarget(ring[0].target, target) ||
        ring[0].serialGeneration != a.serial.generation;
    uint8_t& next = a.pairNext[slot];
    if (next < 1 || next > 3) next = 1;
    auto& s = pin ? ring[0] : ring[next];
    if (!pin) next = static_cast<uint8_t>(next < 3 ? next + 1 : 1);
    s = App::PairSnapshot();
    s.operationId = operationId; s.target = target; s.serialGeneration = a.serial.generation;
    s.words[0] = words[0]; s.words[1] = words[1];
}
const App::PairSnapshot* findPairSnapshot(const App& a, uint16_t first, uint32_t operationId) {
    const int slot = pairSlot(first);
    if (slot < 0 || !operationId) return nullptr;
    for (const auto& s : a.pairSnapshots[slot]) if (s.operationId == operationId) return &s;
    return nullptr;
}
// A checked PT segment read of the selected binding: remember the pulse words
// and settle an unknown experiment write of that window delivered before it.
void notePairRead(App& a, const App::Record& record) {
    const auto& c = record.driver;
    if (c.kind != ESS::DriverKind::READ || c.state != MotorControlRS::ReadState::SUCCEEDED ||
        c.target.generation != a.bindingGeneration || record.address != a.axis.target.address ||
        c.group != ESS::DriverGroup::POSITION_SEGMENT) return;
    if (!ESS::getDriver(c, a.driverObserved)) return;
    const auto& o = a.driverObserved;
    const uint16_t first = static_cast<uint16_t>(0x0060 + 6 * (o.segmentIndex - 1));
    const uint16_t pulses[2] = {o.raw[3], o.raw[4]};
    rememberPairWords(a, first, record.operationId, c.target, pulses);
    auto& v = a.parameterWrite.view;
    if (v.unsettled && v.kind == Probe::ParameterWriteKind::PAIR_EXPERIMENT && v.target.address == record.address &&
        v.first == first && o.provenance[0].attemptedUs > v.writeEvidence.deliveredUs) v.unsettled = false;
}
bool freshConfiguration(const App& a, uint64_t now) {
    const auto& c = a.configuration;
    if (!c.operationId || !Probe::sameTarget(c.target, a.axis.target)) return false;
    for (const auto& p : c.provenance)
        if (!p.qualified || !p.status || p.latestUs > now ||
            !MotorControlRS::evidenceAgeValid(p.attemptedUs, now, a.observationAgeUs())) return false;
    return true;
}
bool noPtTrigger(const App& a) {
    for (uint8_t i = 0; i < 4; ++i)
        if (!a.configuration.inputFunctionKnown[i] || a.configuration.raw.inputFunctions[i] == 12) return false;
    return true;
}
// Fills v.first/count/words; OK or the refusal. No traffic, no state change.
Probe::Action pairExperimentAdmission(App& a, const Probe::ParameterWriteRequest& r, Probe::ParameterWriteView& v, uint64_t now) {
    const uint8_t drive = pairDriveBit(a.axis.target.address);
    if (!drive || !ESS::isExperimentWindow(r.first, 2)) return Probe::Action::UNSUPPORTED;
    if (!freshConfiguration(a, now) || !a.configuration.softLimitEnableKnown || a.configuration.raw.softLimitEnable != 0 ||
        !noPtTrigger(a)) return Probe::Action::UNAVAILABLE;
    if (r.snapshotId) {
        const auto* s = findPairSnapshot(a, r.first, r.snapshotId);
        if (!s || !Probe::sameTarget(s->target, a.axis.target) || s->serialGeneration != a.serial.generation)
            return Probe::Action::UNAVAILABLE;
        v.words[0] = s->words[0]; v.words[1] = s->words[1];
    } else {
        bool listed = false;
        for (const auto& c : PAIR_CANDIDATES)
            listed = listed || (c.first == r.first && c.words[0] == r.words[0] && c.words[1] == r.words[1] && (c.drives & drive));
        if (!listed) return Probe::Action::UNSUPPORTED;
        v.words[0] = r.words[0]; v.words[1] = r.words[1];
    }
    v.first = r.first; v.count = 2;
    return Probe::Action::OK;
}
