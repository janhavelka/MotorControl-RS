// SPDX-License-Identifier: MIT
#pragma once
#include "RtuBusOwner.h"

namespace MotorControlRSExample { namespace Rtu {
/** ESS FC03/06/reviewed FC10 checks using the shipped profile builders/parsers.
 * This validates raw codec access and acknowledgements, not motion readiness or
 * write value meaning. No I/O or retained storage. Console integration is separate. */
Validator essValidator() noexcept;
/** The same checks, except that FC10 uses the separate experiment builder and
 * parser (ledger experiment windows, decision D4) instead of the reviewed
 * windows. Only a named application experiment may submit through it; a
 * reviewed FC10 window fails this validator, and every experiment window
 * fails essValidator(). */
Validator essExperimentValidator() noexcept;
}}
