// SPDX-License-Identifier: MIT
/** @file SettingCandidates.h
 * @brief Admitted control and tuning candidates of the standalone application
 * (wired bench 15). One row per field: what the ordinary `control set` and
 * `tuning GROUP set` routes admit, why, and the effect the bench can observe.
 * Example code, not part of the reusable profile.
 */
#pragma once
#include <MotorControlRS/profiles/ess_rs/DriverSettings.h>
#include <cstddef>
#include <cstdint>
#include "IoStatusRecord.h"

namespace MotorControlRSExample { namespace Probe {

/** How a row admits a value. Every row also admits the D7 exact restore: the
 * same field's word of the pinned read (the first checked read of the group on
 * that drive in this host serial session). */
enum class CandidateRule : uint8_t {
    REFUSED,        ///< No value (the exact restore only).
    RANGE,          ///< minimum..maximum.
    SET,            ///< One of values[0..count).
    PERCENT_OF_PIN, ///< round(pin word x percents[i] / 100).
    LISTED_CURRENT  ///< As PERCENT_OF_PIN, alone, with the pin and the prior read at algorithm word `algorithm`.
};
/** One admitted-candidate row (wired bench 15). */
struct CandidateRow {
    MotorControlRS::ESS_RS::DriverGroup group;
    uint8_t slot;           ///< Control slot 0..7 or tuning local slot.
    const char* name;       ///< Console field name.
    CandidateRule rule;
    uint16_t minimum, maximum;
    const uint16_t* values; ///< SET values or PERCENT_OF_PIN / LISTED_CURRENT percents.
    uint8_t count;
    uint16_t algorithm;     ///< LISTED_CURRENT only.
    const char* reason;     ///< Refusal token for values outside the row.
    const char* effect;     ///< What the bench can observe (or why not).
};
/** The documented default of 0x010A is the upper end of the ordinary range
 * (D12): the ordinary route can only lower the tolerance or restore it. */
constexpr uint16_t DEVIATION_THRESHOLD_ORDINARY_MAX = 4000;
constexpr uint8_t SETTING_PIN_SLOTS = 4; ///< Drives with a pinned read per group and host serial session.

namespace CandidateTables {
constexpr uint16_t CLOSED_LOOP_PERCENTS[] = {50, 25, 10};
constexpr uint16_t LOCK_PERCENTS[] = {50};
constexpr uint16_t GAIN_PERCENTS[] = {75, 125};
constexpr uint16_t COLLISION_THRESHOLDS[] = {200};
constexpr uint16_t COLLISION_CURRENTS[] = {50};
using G = MotorControlRS::ESS_RS::DriverGroup;
constexpr CandidateRow ROWS[] = {
    {G::CONTROL_SETTINGS, 0, "algorithm", CandidateRule::REFUSED, 0, 0, nullptr, 0, 0, "algorithm_refused",
     "control mode and the meaning of 0x000A; no current tuple is qualified for algorithm 1 or 2 (T10, T11 NOT RUN)"},
    {G::CONTROL_SETTINGS, 1, "encoder-resolution", CandidateRule::REFUSED, 0, 0, nullptr, 0, 0, "encoder_change_deferred",
     "feedback scale; the change waits for the owner (T12, D15)"},
    {G::CONTROL_SETTINGS, 2, "maximum-effective-current", CandidateRule::REFUSED, 0, 0, nullptr, 0, 0, "current_ceiling_unresolved",
     "current; needs an instrument (standalone 08)"},
    {G::CONTROL_SETTINGS, 3, "closed-maximum-current", CandidateRule::LISTED_CURRENT, 0, 0, CLOSED_LOOP_PERCENTS, 3, 3,
     "current_not_listed", "lag or alarm 5 at 600 rpm (T9); no current claim"},
    {G::CONTROL_SETTINGS, 4, "closed-base-current", CandidateRule::REFUSED, 0, 0, nullptr, 0, 0, "current_not_listed",
     "current; needs an instrument (standalone 08)"},
    {G::CONTROL_SETTINGS, 5, "open-maximum-current", CandidateRule::REFUSED, 0, 0, nullptr, 0, 0, "current_not_listed",
     "applies in open loop only (T10 needs T11)"},
    {G::CONTROL_SETTINGS, 6, "lock-current", CandidateRule::LISTED_CURRENT, 0, 0, LOCK_PERCENTS, 1, 3,
     "current_not_listed", "stored word only (T8); holding torque needs an instrument"},
    {G::CONTROL_SETTINGS, 7, "lock-delay", CandidateRule::RANGE, 0, 20000, nullptr, 0, 0, "outside_candidates",
     "brake output (Y0 = 5) after a stop (T7)"},
    {G::FILTERS, 0, "input-filter", CandidateRule::SET, 0, 0, MEASURED_INPUT_FILTERS,
     sizeof(MEASURED_INPUT_FILTERS) / sizeof(MEASURED_INPUT_FILTERS[0]), 0, "outside_candidates",
     "input debounce (wired bench 05)"},
    // 0 is documented but unmeasured; 1..1024 ran in T4.
    {G::FILTERS, 1, "pulse-low-pass", CandidateRule::RANGE, 1, 1024, nullptr, 0, 0, "outside_candidates",
     "pulse input only; serial moves compared (T4)"},
    {G::FILTERS, 2, "deviation-threshold", CandidateRule::RANGE, 1, DEVIATION_THRESHOLD_ORDINARY_MAX, nullptr, 0, 0,
     "threshold_needs_call", "alarm 5 tolerance (T3); above 4000 only after `candidates threshold VALUE` (D6)"},
    {G::FILTERS, 3, "arrival-window", CandidateRule::RANGE, 1, 256, nullptr, 0, 0, "outside_candidates",
     "in-position output and bit (T1)"},
    {G::FILTERS, 4, "arrival-time", CandidateRule::RANGE, 0, 200, nullptr, 0, 0, "outside_candidates",
     "in-position delay (T2)"},
    // Documented 0..512, but a written 0 reads back 1 (wired bench 15, drive 4,
    // twice): 0 would leave a readback mismatch, so the row starts at 1.
    {G::FILTERS, 5, "pulse-mean", CandidateRule::RANGE, 1, 512, nullptr, 0, 0, "outside_candidates",
     "pulse input only; serial moves compared (T4)"},
    {G::CURRENT_LOOP, 0, "multiplier", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates",
     "move completion and timing (T5); bandwidth needs an instrument"},
    {G::CURRENT_LOOP, 1, "kp", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates",
     "move completion and timing (T5)"},
    {G::CURRENT_LOOP, 2, "ki", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates",
     "move completion and timing (T5)"},
    {G::CURRENT_LOOP, 3, "kc", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates",
     "move completion and timing (T5)"},
    {G::LA, 0, "kp1", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates", "move completion and timing (T6)"},
    {G::LA, 1, "kv1", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates", "move completion and timing (T6)"},
    {G::LA, 2, "node1", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates", "move completion and timing (T6)"},
    {G::LA, 3, "kp2", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates", "move completion and timing (T6)"},
    {G::LA, 4, "kv2", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates", "move completion and timing (T6)"},
    {G::LA, 5, "node2", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates", "move completion and timing (T6)"},
    {G::LA, 6, "kvf", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates", "move completion and timing (T6)"},
    {G::LA, 7, "position-ki", CandidateRule::PERCENT_OF_PIN, 0, 0, GAIN_PERCENTS, 2, 0, "outside_candidates", "move completion and timing (T6)"},
    {G::COLLISION, 0, "threshold", CandidateRule::SET, 0, 0, COLLISION_THRESHOLDS, 1, 0, "outside_candidates",
     "stored only (T13); collision needs a mechanical stop"},
    {G::COLLISION, 1, "current", CandidateRule::SET, 0, 0, COLLISION_CURRENTS, 1, 0, "outside_candidates",
     "stored only (T13); collision needs a mechanical stop"},
};
} // namespace CandidateTables

/** The row of one control or tuning field, or nullptr. */
inline const CandidateRow* candidateRow(MotorControlRS::ESS_RS::DriverGroup group, uint8_t slot) noexcept {
    for (const auto& row : CandidateTables::ROWS)
        if (row.group == group && row.slot == slot) return &row;
    return nullptr;
}
/** round(word x percent / 100), or -1 above 65535. */
inline int32_t candidatePercent(uint16_t word, uint16_t percent) noexcept {
    const uint32_t value = (static_cast<uint32_t>(word) * percent + 50) / 100;
    return value > 0xFFFF ? -1 : static_cast<int32_t>(value);
}

enum class CandidatesCommandKind : uint8_t { SHOW, CHECK, THRESHOLD, RESET };
/** `candidates [check GROUP FIELD VALUE [address] | threshold VALUE | reset]`. */
struct CandidatesCommand {
    CandidatesCommandKind kind = CandidatesCommandKind::SHOW;
    MotorControlRS::ESS_RS::DriverGroup group = MotorControlRS::ESS_RS::DriverGroup::CONTROL_SETTINGS; ///< CHECK.
    uint8_t slot = 0;      ///< CHECK.
    uint16_t value = 0;    ///< CHECK candidate or THRESHOLD value (4001..65535).
    uint8_t address = 0;   ///< CHECK; 0 means the selected axis.
};
/** Result of one call. A CHECK runs the ordinary admission of `control set` or
 * `tuning GROUP set` for one field without any frame: admitted, the rule that
 * admitted it ("row", "listed_current", "exact_restore") or the refusal reason,
 * and the pinned read that an exact restore would use. */
struct CandidatesView {
    uint8_t address = 0;           ///< Selected axis.
    uint8_t checkedAddress = 0;    ///< CHECK: the drive whose admission was checked.
    bool thresholdActive = false;  ///< D6 call current for the selected axis, binding and host session.
    uint16_t thresholdValue = 0;
    bool admitted = false;
    const char* rule = "none";
    const char* reason = "none";
    bool pinKnown = false;
    uint32_t pinOperationId = 0;   ///< Pinned read of the field's group on this drive, 0 if none.
    uint16_t pinWord = 0;
    int32_t coreCode = 0, coreDetail = 0; ///< Core admission status of a CHECK (0 when admitted).
};

}} // namespace MotorControlRSExample::Probe
