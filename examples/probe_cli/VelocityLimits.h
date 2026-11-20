// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>

namespace MotorControlRSExample { namespace Probe {

/** Velocity limits of the standalone application (wired bench 14, decisions D6
 * and D12). Without a call the legacy envelope applies: +-60 native rpm and
 * 1..1000 ms. Two explicit calls raise it for the selected axis only, and every
 * raise ends with `velocity limits reset`, `useaddr`, `recover`, a host serial
 * change or a controller restart:
 *
 * - a qualification cell names one exact run (signed rpm, both ramp words, the
 *   subdivision, duration and stop policy) and admits exactly one run that
 *   matches it; the admitted run consumes it, a refusal before any frame keeps it;
 * - `raise` admits the promoted table (VelocityPromoted.h) and its
 *   interpolation rule (velocityPromotedAdmission).
 *
 * A cell and a raise are exclusive: `qualify` refuses while raised, `raise`
 * while a cell is armed or its run is still active. Raised limits are
 * serial-only: the JOG setter and input-started runs keep the legacy speed.
 *
 * These are application admission rules of a free-shaft bench, not drive
 * limits or a physical speed qualification. The core still admits +-3000 rpm
 * and ramp words up to 2000. */
constexpr uint16_t VELOCITY_LEGACY_MAX_RPM = 60;
constexpr uint32_t VELOCITY_LEGACY_MAX_DURATION_MS = 1000;
constexpr uint16_t VELOCITY_CELL_MAX_RPM = 3000;            ///< The core's signed range.
constexpr uint32_t VELOCITY_MAX_DURATION_MS = 12000;        ///< Longest run a cell or table entry may name.
constexpr uint16_t VELOCITY_MAX_RAMP_WORD = 2000;           ///< Native ramp words the core admits.
constexpr uint16_t VELOCITY_MIN_SUBDIVISION = 400, VELOCITY_MAX_SUBDIVISION = 51200; ///< 0x0011 (p69).
constexpr uint32_t VELOCITY_POLLS = 64;                     ///< ESS::ACTION_MAX_POLLS.
constexpr uint32_t VELOCITY_FAST_POLL_MS = 20, VELOCITY_NORMAL_POLL_MS = 50;
/** Allowance in every stop budget for the last poll to settle and one bounded
 * recovery to finish before the held end stop is sent: STOP_RECOVERY_US of the
 * ordinary stops (one request plus RECOVERY_BUDGET_US). Wired bench 18: without
 * it a direct stop expired unsent after a late failed poll and a full recovery. */
constexpr uint32_t VELOCITY_STOP_RECOVERY_MS = 2500;
/** Stop budgets: the time from the run's scheduled stop due time (start plus
 * duration) to its deadline, both fixed at admission. The normal budget covers
 * a 2000 ms deceleration word plus two requests (NORMAL_STOP_US), the direct
 * one the 2 s used since the first velocity route; both add
 * VELOCITY_STOP_RECOVERY_MS. */
constexpr uint32_t VELOCITY_FAST_STOP_BUDGET_MS = 2000 + VELOCITY_STOP_RECOVERY_MS;
constexpr uint32_t VELOCITY_NORMAL_STOP_BUDGET_MS = 3000 + VELOCITY_STOP_RECOVERY_MS;

enum class VelocityStop : uint8_t { NORMAL, FAST };
/** One exact run, as a qualification cell or a promoted table entry names it. */
struct VelocityCell {
    int16_t rpm = 0;                         ///< Signed native rpm, nonzero.
    uint16_t accelerationTime = 0, decelerationTime = 0; ///< Native 0x001E/0x001F words, 0..2000.
    uint16_t subdivision = 0;                ///< 0x0011, 400..51200.
    uint32_t durationMs = 0;                 ///< 1..VELOCITY_MAX_DURATION_MS.
    VelocityStop stop = VelocityStop::NORMAL;
    /** Promoted table only: a cell that never serves as an interpolation end
     * (the HW-14 rate cells of phase C3 stand alone). */
    bool standalone = false;
};
enum class VelocityLimitsSource : uint8_t { LEGACY, CELL, RAISED };
/** How one run was admitted, or REFUSED. */
enum class VelocityAdmission : uint8_t { LEGACY, CELL, PROMOTED, INTERPOLATED, REFUSED };
enum class VelocityLimitsCommandKind : uint8_t { SHOW, QUALIFY, RAISE, RESET };
struct VelocityLimitsCommand {
    VelocityLimitsCommandKind kind = VelocityLimitsCommandKind::SHOW;
    VelocityCell cell; ///< QUALIFY.
};
/** Active velocity limits of the selected axis. maximumRpm, maximumDurationMs
 * and maximumRate (increments/s at the selected axis's checked subdivision,
 * 0 when unknown) describe the envelope a run may use: the legacy envelope, the
 * armed cell's exact run, or the largest promoted entry once raised (serial
 * runs only; input-started runs and the JOG setter keep the legacy speed).
 * pollMs and stopBudgetMs are those of the longest admitted run. */
struct VelocityLimitsView {
    VelocityLimitsSource source = VelocityLimitsSource::LEGACY;
    uint8_t address = 0;
    uint16_t maximumRpm = VELOCITY_LEGACY_MAX_RPM;
    uint32_t maximumDurationMs = VELOCITY_LEGACY_MAX_DURATION_MS;
    uint32_t maximumRate = 0;
    uint16_t subdivision = 0; ///< Checked 0x0011 of the selected axis, 0 when unknown.
    uint32_t pollMs = VELOCITY_NORMAL_POLL_MS, stopBudgetMs = VELOCITY_NORMAL_STOP_BUDGET_MS;
    bool cellArmed = false, raised = false;
    VelocityCell cell;
    uint32_t consumedOperationId = 0; ///< The run that consumed the latest cell, 0 if none.
    uint8_t promotedCount = 0;
    const char* reason = "none"; ///< Refusal reason of this call.
};

/** Commanded rate, increments per second: |rpm| x subdivision / 60, rounded up. */
inline uint32_t velocityRate(int32_t rpm, uint32_t subdivision) {
    const uint64_t magnitude = static_cast<uint64_t>(rpm < 0 ? -static_cast<int64_t>(rpm) : rpm);
    return static_cast<uint32_t>((magnitude * subdivision + 59) / 60);
}
/** Poll interval of a run: the policy's base interval, or longer so that
 * VELOCITY_POLLS polls cover the duration (the core requires it). */
inline uint32_t velocityPollMs(uint32_t durationMs, VelocityStop stop) {
    const uint32_t base = stop == VelocityStop::NORMAL ? VELOCITY_NORMAL_POLL_MS : VELOCITY_FAST_POLL_MS;
    const uint32_t cover = (durationMs + VELOCITY_POLLS - 1) / VELOCITY_POLLS;
    return cover > base ? cover : base;
}
/** Stop budget after the run's own stop for the stop policy. */
inline uint32_t velocityStopBudgetMs(VelocityStop stop) {
    return stop == VelocityStop::NORMAL ? VELOCITY_NORMAL_STOP_BUDGET_MS : VELOCITY_FAST_STOP_BUDGET_MS;
}
/** A cell or table entry inside the ranges the core and the drive document. */
inline bool velocityCellValid(const VelocityCell& c) {
    const int32_t magnitude = c.rpm < 0 ? -static_cast<int32_t>(c.rpm) : c.rpm;
    return c.rpm && magnitude <= VELOCITY_CELL_MAX_RPM && c.accelerationTime <= VELOCITY_MAX_RAMP_WORD &&
        c.decelerationTime <= VELOCITY_MAX_RAMP_WORD && c.subdivision >= VELOCITY_MIN_SUBDIVISION &&
        c.subdivision <= VELOCITY_MAX_SUBDIVISION && c.durationMs >= 1 && c.durationMs <= VELOCITY_MAX_DURATION_MS &&
        c.stop <= VelocityStop::FAST;
}
/** Inside the legacy envelope: +-60 rpm, 1..1000 ms (any ramp, subdivision, stop). */
inline bool velocityLegacyRun(const VelocityCell& run) {
    return run.rpm && run.rpm >= -static_cast<int32_t>(VELOCITY_LEGACY_MAX_RPM) &&
        run.rpm <= static_cast<int32_t>(VELOCITY_LEGACY_MAX_RPM) &&
        run.durationMs >= 1 && run.durationMs <= VELOCITY_LEGACY_MAX_DURATION_MS;
}
/** The exact run a cell names. */
inline bool velocityCellMatches(const VelocityCell& cell, const VelocityCell& run) {
    return cell.rpm == run.rpm && cell.accelerationTime == run.accelerationTime &&
        cell.decelerationTime == run.decelerationTime && cell.subdivision == run.subdivision &&
        cell.durationMs == run.durationMs && cell.stop == run.stop;
}
/** Promoted admission of one run (prompt 14 Implement 5). An entry with the
 * same ramps, subdivision, stop policy and direction, a duration no shorter than
 * the run's and the same |rpm| admits it (PROMOTED). An untested |rpm| is admitted
 * only strictly between two such entries that are not standalone and at most a
 * factor of two apart, the run no longer than either (INTERPOLATED). Nothing is
 * inferred above the highest entry, below the lowest, for another ramp, direction,
 * subdivision or stop policy, or between standalone (HW-14 rate) entries; a
 * standalone entry admits only its exact duration (its stop point is part of
 * the cell). */
inline VelocityAdmission velocityPromotedAdmission(const VelocityCell* table, std::size_t count, const VelocityCell& run) {
    if (!table || !velocityCellValid(run)) return VelocityAdmission::REFUSED;
    const int32_t want = run.rpm < 0 ? -static_cast<int32_t>(run.rpm) : run.rpm;
    const auto same = [&](const VelocityCell& e) {
        return velocityCellValid(e) && (e.rpm < 0) == (run.rpm < 0) && e.accelerationTime == run.accelerationTime &&
            e.decelerationTime == run.decelerationTime && e.subdivision == run.subdivision && e.stop == run.stop &&
            (e.standalone ? e.durationMs == run.durationMs : e.durationMs >= run.durationMs);
    };
    for (std::size_t i = 0; i < count; ++i)
        if (same(table[i]) && (table[i].rpm < 0 ? -static_cast<int32_t>(table[i].rpm) : table[i].rpm) == want)
            return VelocityAdmission::PROMOTED;
    for (std::size_t i = 0; i < count; ++i) {
        if (!same(table[i]) || table[i].standalone) continue;
        const int32_t low = table[i].rpm < 0 ? -static_cast<int32_t>(table[i].rpm) : table[i].rpm;
        if (low >= want) continue;
        for (std::size_t j = 0; j < count; ++j) {
            if (!same(table[j]) || table[j].standalone) continue;
            const int32_t high = table[j].rpm < 0 ? -static_cast<int32_t>(table[j].rpm) : table[j].rpm;
            if (high > want && high <= 2 * low) return VelocityAdmission::INTERPOLATED;
        }
    }
    return VelocityAdmission::REFUSED;
}
/// Console and harness name of a limits source.
inline const char* velocityLimitsSourceName(VelocityLimitsSource source) {
    switch (source) {
    case VelocityLimitsSource::LEGACY: return "legacy";
    case VelocityLimitsSource::CELL: return "cell";
    case VelocityLimitsSource::RAISED: return "raised";
    }
    return "unknown";
}

}} // namespace MotorControlRSExample::Probe
