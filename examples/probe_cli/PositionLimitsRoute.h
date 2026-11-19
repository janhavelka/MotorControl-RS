// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/profiles/ess_rs/Position.h>
#include <cstddef>
#include <cstdint>

namespace MotorControlRSExample { namespace Probe {

/** Host position limits of the selected axis (wired bench 16, decisions D6 and
 * D12): the explicit, named opt-in call for finite moves above the core
 * defaults (2000 rpm, 50,000 command increments/s, ramps of at least 100 ms).
 * Without a call the defaults apply. One install names a case and binds the
 * selected target, its binding generation, the configuration generation and
 * the host serial generation; `recover`, `useaddr`, a configuration change, a
 * host tuple change, `position limits default` and a controller restart end it
 * (RAM only). It applies to selected-axis finite moves and staging; an interrupt
 * keeps the limits of its original move; group moves, input-started moves and
 * homing keep the defaults.
 *
 * These are application admission values whose risk the caller owns, not drive
 * ratings or a physical qualification. The firmware 0x0029 defects above the
 * defaults (HW-14 creep after a late stop normal, persistent RUNNING at high
 * subdivision) are not prevented by an install. */
constexpr std::size_t POSITION_LIMITS_CASE_CAPACITY = 16; ///< Case name bytes including the terminator.
constexpr uint16_t POSITION_LIMITS_MAX_RPM = 3000;        ///< 0x0023 wire range (p70).
constexpr uint16_t POSITION_LIMITS_MAX_RAMP = 2000;       ///< 0x0021/0x0022 wire range (p70).
/** 3000 rpm at subdivision 51200; a larger rate cannot raise any ceiling. */
constexpr uint32_t POSITION_LIMITS_MAX_RATE = 2560000;

enum class PositionLimitsSource : uint8_t { DEFAULT, INSTALLED };
enum class PositionLimitsCommandKind : uint8_t { SHOW, SET, DEFAULT };
/** One local call: SHOW copies, SET installs `limits` under `caseName`, DEFAULT
 * returns to the core defaults. */
struct PositionLimitsCommand {
    PositionLimitsCommandKind kind = PositionLimitsCommandKind::SHOW;
    MotorControlRS::ESS_RS::PositionLimits limits; ///< SET only.
    char caseName[POSITION_LIMITS_CASE_CAPACITY] = {}; ///< SET only; positionLimitsCaseValid.
};
/** Active position limits of the selected axis after one call. maximumRpm is
 * the speed ceiling at the checked subdivision (0 when unknown); defaultRpm the
 * ceiling the core defaults give there. installBinding and installConfiguration
 * are the generations an active install is bound to (0 for the defaults). */
struct PositionLimitsView {
    PositionLimitsSource source = PositionLimitsSource::DEFAULT;
    uint8_t address = 0;
    MotorControlRS::ESS_RS::PositionLimits limits;
    char caseName[POSITION_LIMITS_CASE_CAPACITY] = {};
    uint16_t subdivision = 0, maximumRpm = 0, defaultRpm = 0;
    uint32_t installBinding = 0, installConfiguration = 0;
    const char* reason = "none"; ///< Refusal reason of this call.
};

/** Wire ranges of an install: rpm 1..3000, rate 1..POSITION_LIMITS_MAX_RATE,
 * minimum ramp 0..2000. The core keeps enforcing each move's own words. */
inline bool positionLimitsValid(const MotorControlRS::ESS_RS::PositionLimits& l) {
    return l.maximumRpm >= 1 && l.maximumRpm <= POSITION_LIMITS_MAX_RPM && l.maximumCommandRate >= 1 &&
        l.maximumCommandRate <= POSITION_LIMITS_MAX_RATE && l.minimumRampTime <= POSITION_LIMITS_MAX_RAMP;
}
/** A case name: 1..15 characters of a-z, 0-9, '-' and '_', starting with a
 * letter or digit, NUL-terminated within the capacity. */
inline bool positionLimitsCaseValid(const char* name) {
    if (!name) return false;
    std::size_t i = 0;
    for (; i < POSITION_LIMITS_CASE_CAPACITY && name[i]; ++i) {
        const char c = name[i];
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!alnum && (i == 0 || (c != '-' && c != '_'))) return false;
    }
    return i >= 1 && i < POSITION_LIMITS_CASE_CAPACITY;
}
/** Two limit sets are the same values. */
inline bool samePositionLimits(const MotorControlRS::ESS_RS::PositionLimits& a,
                               const MotorControlRS::ESS_RS::PositionLimits& b) {
    return a.maximumRpm == b.maximumRpm && a.maximumCommandRate == b.maximumCommandRate &&
        a.minimumRampTime == b.minimumRampTime;
}
/// Console and harness name of a limits source.
inline const char* positionLimitsSourceName(PositionLimitsSource source) {
    return source == PositionLimitsSource::INSTALLED ? "installed" : "default";
}

}} // namespace MotorControlRSExample::Probe
