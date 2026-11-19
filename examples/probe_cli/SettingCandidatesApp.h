// SPDX-License-Identifier: MIT
// Included inside the standalone application's private namespace.
// Control and tuning candidates (wired bench 15, SettingCandidates.h).
//
// Pins: the first complete checked read of each control and tuning group per
// drive address and host serial session is kept until the host tuple changes
// or the application restarts (RAM only). A host recovery or `useaddr` does not
// replace it, so the source of a D7 exact restore and the base of the percent
// rows stay the state before this session's first write. Later reads never
// replace a pin; restore a drive before working on another one.
//
// Decision for one `control set` / `tuning GROUP set` field: its row first,
// then the D7 exact restore when the value equals the pinned word. Requests
// with more than one field are refused (each candidate is alone).

uint8_t settingGroupIndex(ESS::DriverGroup group) {
    switch (group) {
    case ESS::DriverGroup::CONTROL_SETTINGS: return 0;
    case ESS::DriverGroup::FILTERS: return 1;
    case ESS::DriverGroup::CURRENT_LOOP: return 2;
    case ESS::DriverGroup::LA: return 3;
    case ESS::DriverGroup::COLLISION: return 4;
    default: return 255;
    }
}
const ESS::DriverObservation* settingPin(const App& a, ESS::DriverGroup group, uint8_t address) {
    const uint8_t g = settingGroupIndex(group);
    if (g == 255) return nullptr;
    for (const auto& pin : a.settingPins[g])
        if (pin.observation.operationId && pin.observation.target.address == address &&
            pin.serialGeneration == a.serial.generation) return &pin.observation;
    return nullptr;
}
// A complete checked read of a control or tuning group: pinned when its drive
// has no pin of this group and host session and a slot is free.
void pinSettingRead(App& a, const ESS::DriverObservation& observed) {
    const uint8_t g = settingGroupIndex(observed.group);
    if (g == 255 || settingPin(a, observed.group, observed.target.address)) return;
    for (auto& pin : a.settingPins[g])
        if (!pin.observation.operationId || pin.serialGeneration != a.serial.generation) {
            pin.observation = observed; pin.serialGeneration = a.serial.generation; return;
        }
}
bool thresholdCallCurrent(const App& a) {
    const auto& t = a.thresholdCall;
    return t.value > Probe::DEVIATION_THRESHOLD_ORDINARY_MAX && t.address == a.axis.target.address &&
        t.bindingGeneration == a.bindingGeneration && t.serialGeneration == a.serial.generation;
}
uint8_t requestSlot(const ESS::DriverRequest& r) {
    const uint8_t count = r.group == ESS::DriverGroup::CONTROL_SETTINGS ? 8 : ESS::tuningFieldCount(r.group);
    for (uint8_t i = 0; i < count; ++i)
        if (r.fields == static_cast<uint32_t>(ESS::driverFieldAt(i, r.group))) return i;
    return 255;
}
uint16_t requestWord(const ESS::DriverRequest& r, uint8_t slot) {
    if (r.group != ESS::DriverGroup::CONTROL_SETTINGS) return r.tuningValues[slot];
    switch (slot) {
    case 0: return static_cast<uint16_t>(r.controlAlgorithm);
    case 1: return r.encoderResolution;
    case 2: return r.maximumEffectiveCurrentMa;
    case 3: return r.closedMaximumPercent;
    case 4: return r.closedBasePercent;
    case 5: return r.openMaximumPercent;
    case 6: return r.lockPercent;
    default: return r.lockDelayMs;
    }
}
App::CandidateDecision decideCandidate(const App& a, const ESS::DriverRequest& r,
                                       const ESS::DriverObservation& previous, uint8_t address) {
    App::CandidateDecision d;
    const uint8_t slot = requestSlot(r);
    const Probe::CandidateRow* row = slot == 255 ? nullptr : Probe::candidateRow(r.group, slot);
    if (!row) { d.reason = "one_field_per_request"; return d; }
    d.pin = settingPin(a, r.group, address);
    const uint16_t value = requestWord(r, slot);
    const auto percentOfPin = [&](bool& matched) {
        matched = false;
        for (uint8_t i = 0; d.pin && i < row->count; ++i)
            if (Probe::candidatePercent(d.pin->raw[slot], row->values[i]) == value) matched = true;
    };
    bool matched = false;
    switch (row->rule) {
    case Probe::CandidateRule::REFUSED: break;
    case Probe::CandidateRule::RANGE:
        matched = value >= row->minimum && value <= row->maximum;
        // D6/D12: above 4000 only the value the call names, for its session.
        if (r.group == ESS::DriverGroup::FILTERS && slot == 2 && !matched)
            matched = thresholdCallCurrent(a) && value == a.thresholdCall.value;
        break;
    case Probe::CandidateRule::SET:
        for (uint8_t i = 0; i < row->count; ++i) matched = matched || row->values[i] == value;
        break;
    case Probe::CandidateRule::PERCENT_OF_PIN:
        percentOfPin(matched);
        break;
    case Probe::CandidateRule::LISTED_CURRENT:
        // The pin and the prior read both hold the algorithm word the row was listed for.
        if (d.pin && d.pin->raw[0] == row->algorithm && previous.raw[0] == row->algorithm) percentOfPin(matched);
        d.listed = matched;
        break;
    }
    if (matched) { d.admitted = true; d.rule = d.listed ? "listed_current" : "row"; return d; }
    // D7: write back the pinned word of the same drive, whatever its range,
    // except where this application keeps an opt-in or a mode restriction:
    // a deviation threshold above 4000 still needs its D6 call, and the
    // algorithm word is written back only as the bench's baseline 3.
    if (d.pin && value == d.pin->raw[slot]) {
        const bool threshold = r.group == ESS::DriverGroup::FILTERS && slot == 2 &&
            value > Probe::DEVIATION_THRESHOLD_ORDINARY_MAX && !(thresholdCallCurrent(a) && value == a.thresholdCall.value);
        const bool algorithm = r.group == ESS::DriverGroup::CONTROL_SETTINGS && slot == 0 && value != 3;
        if (threshold) { d.reason = "threshold_needs_call"; return d; }
        if (algorithm) { d.reason = "algorithm_refused"; return d; }
        d.admitted = d.restore = true; d.rule = "exact_restore"; return d;
    }
    d.reason = row->rule == Probe::CandidateRule::LISTED_CURRENT && !d.pin ? "pin_required" :
        row->rule == Probe::CandidateRule::PERCENT_OF_PIN && !d.pin ? "pin_required" : row->reason;
    return d;
}
// Fill the control/tuning qualification fields of one update from the decision.
void applyCandidateDecision(ESS::DriverPrerequisites& p, const App::CandidateDecision& d,
                            const ESS::DriverRequest& r) {
    if (d.listed) {
        p.listedCurrentQualified = true; p.listedCurrentField = requestSlot(r);
        p.listedCurrentValue = requestWord(r, p.listedCurrentField);
        p.listedCurrentAlgorithm = Probe::candidateRow(r.group, p.listedCurrentField)->algorithm;
        p.listedCurrentSourceId = p.previous.operationId;
    }
    if (d.restore) { p.exactRestore = true; p.restoreSource = *d.pin; }
}
