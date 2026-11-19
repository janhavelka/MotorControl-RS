// SPDX-License-Identifier: MIT
#pragma once
#include "VelocityLimits.h"

namespace MotorControlRSExample { namespace Probe {

/** Promoted velocity cells (wired bench 14, D6): runs that passed their phase C
 * qualification on model 0x4EEA, firmware 0x0029. `velocity limits raise`
 * admits these and velocityPromotedAdmission's interpolation between them;
 * nothing else above the legacy envelope. Each entry changes only in its own
 * commit with its evidence (report and run folder). Empty: no cell promoted. */
constexpr std::size_t VELOCITY_PROMOTED_COUNT = 0;
constexpr const VelocityCell* VELOCITY_PROMOTED = nullptr;

}} // namespace MotorControlRSExample::Probe
