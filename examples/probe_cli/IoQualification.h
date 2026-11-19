// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/profiles/ess_rs/DriverSettings.h>

namespace MotorControlRSExample {
/** One purpose per retained terminal record. Installing another purpose replaces
 * the previous record. Assignment does not qualify later motion. */
enum class IoPurpose : uint8_t { ASSIGN, CONFIGURE, FINITE_MOVE, SEGMENT };
/** One terminal's supplied facts and exact checked baseline. No physical fact is
 * inferred by inspectIoQualification. Input terminals are 0..3, outputs 4..5.
 * Candidate polarity/custom are single bits, not whole-register masks.
 * Caller age is independent of subsequent IO reads; zero maximum age disables
 * age-only expiry, not binding, configuration or explicit revocation checks.
 * ASSIGN stays bound to its exact settings/state reads. Other purposes may use
 * refreshed observations without renewing qualifiedUs. */
struct IoQualification {
    MotorControlRS::ReadTarget target;
    uint32_t configurationGeneration = 0, settingsOperationId = 0, stateOperationId = 0;
    uint8_t terminal = 0;
    bool member = false; ///< Group inputs support FINITE_MOVE only; never borrow selected-axis facts.
    IoPurpose purpose = IoPurpose::ASSIGN;
    MotorControlRS::InputWiring wiring = MotorControlRS::InputWiring::UNKNOWN;
    uint16_t previousFunction = 0, previousPolarity = 0, previousCustom = 0;
    uint16_t function = 0, polarity = 0, custom = 0;
    uint16_t reportedInputs = 0, reportedOutputs = 0;
    uint64_t observedEarliestUs = 0, observedLatestUs = 0;
    uint64_t qualifiedUs = 0, maximumAgeUs = 0;
    MotorControlRS::ESS_RS::IoInputFacts input;
    MotorControlRS::ESS_RS::IoOutputFacts output;
};
namespace Probe {
/** Local commissioning command. It never submits a motor write. */
struct IoQualificationCommand {
    enum class Kind : uint8_t { INSPECT, INSTALL, REVOKE };
    Kind kind = Kind::INSPECT;
    uint8_t address = 0, terminal = 0;
    bool member = false;
    IoPurpose purpose = IoPurpose::ASSIGN;
    uint16_t function = 0, polarity = 0, custom = 0;
    /** Input install only: take the status mapping from RECORDED_INPUT_MAPPINGS
     * for the target's fresh checked identity (console token `recorded`). */
    bool recordedMapping = false;
    MotorControlRS::ESS_RS::IoInputFacts input;
    MotorControlRS::ESS_RS::IoOutputFacts output;
};
} // namespace Probe
} // namespace MotorControlRSExample
