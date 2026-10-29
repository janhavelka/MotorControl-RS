// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

namespace MotorControlRSExample {
/** Copied host handoff identity, never proof of physical interruption. Both
 * records retain these IDs independently. Superseded becomes true only after
 * the old local observation continuation is cancelled for this replacement. */
struct MoveRelation {
    uint32_t originalOperationId = 0, replacementOperationId = 0;
    bool observationSuperseded = false;
};
}
