// SPDX-License-Identifier: MIT
#pragma once
#include "../common/Esp32S3Uart.h"
#include "MoveRelation.h"
#include <MotorControlRS/profiles/ess_rs/Position.h>
#include <MotorControlRS/profiles/ess_rs/Reads.h>
#include <MotorControlRS/profiles/ess_rs/DriverSettings.h>

namespace MotorControlRSExample {
/** Age-only expiry is opt-in. Zero keeps checked observations until invalidated
 * by an event/configuration change; it does not prove the drive stayed powered.
 * Transaction deadlines, missing evidence and generation checks always apply. */
struct ApplicationOptions {
    uint32_t observationMaxAgeMs = 0;
    uint32_t moveTimeoutMs = 30000; ///< 1..30000 ms, finite movement observation deadline.
    ApplicationOptions() {
        // Usable nominal convention, explicitly assumed rather than calibrated.
        positionUnits.commandStepsPerMotorTurn = MotorControlRS::UnitScale(1000, 1, MotorControlRS::ScaleSource::ASSUMED);
    }
    MotorControlRS::UnitConfig positionUnits; ///< Host scales; default 1000 command steps/turn is ASSUMED. No device settings write.
};
/** Construct the shared standalone application in required PSRAM and initialize
 * its one UART/capture owner. Failure is explicit; there is no internal-RAM
 * fallback. Pins/topology are supplied by the selected example configuration.
 * Call once from the same task that subsequently calls serviceApplication().
 */
bool beginApplication(const Esp32S3Uart::Pins&, bool receiverDisabledDuringTransmit,
                      const ApplicationOptions& = ApplicationOptions());
/// One bounded cooperative console/owner service turn, including an idle yield.
void serviceApplication();

/** Submit through the same owner/admission/polling path as console moves.
 * Existing configuration, state and motion-profile reads establish readiness.
 * Call from the serviceApplication task. Output ID is unchanged on rejection.
 * moveBy requests relative motion: steps are native command
 * increments; other units use the motor frame, except millimetres (load frame).
 * Scales come from ApplicationOptions::positionUnits or host axis configuration.
 * Optional setup policy defaults to full write. USE_STORED requires a matching
 * remembered profile/binding in this example; it does not detect an unseen reset.
 */
// Convenience targets round to nearest command step, ties to even, at most
// half a step. Use submitMove for explicit EXACT or another rounding policy.
MotorControlRS::Status moveBy(MotorControlRS::Rational value, MotorControlRS::PositionUnit unit,
                            uint32_t& operationId, uint16_t speedRpm = 60,
                            MotorControlRS::MoveSetup = MotorControlRS::MoveSetup::WRITE_ALL);
/// Absolute target. Engineering coordinates require their configured origin/scales.
MotorControlRS::Status moveTo(MotorControlRS::Rational value, MotorControlRS::PositionUnit unit,
                            uint32_t& operationId, uint16_t speedRpm = 60,
                            MotorControlRS::MoveSetup = MotorControlRS::MoveSetup::WRITE_ALL);
/// Advanced units/frame/rounding/absolute request; configuration generation is checked, not rebound.
MotorControlRS::Status submitMove(const MotorControlRS::PositionRequest&, uint16_t speedRpm,
                            uint32_t& operationId,
                            MotorControlRS::MoveSetup = MotorControlRS::MoveSetup::WRITE_ALL);
/// Copy current host coordinates/scales/generation for an advanced request; no bus traffic.
bool positionConfiguration(MotorControlRS::AxisConfig&);
/** Small copied progress/result. Observation flags describe drive reports;
 * success requires the existing new RUNNING then ARRIVED sequence. */
struct MoveProgress {
    bool pending = false, runningObserved = false, interruptedByStop = false, uncertain = false;
    bool observationKnown = false;
    uint16_t rawAlarm = 0, rawMotion = 0;
    MotorControlRS::ActionOutcome outcome = MotorControlRS::ActionOutcome::NONE;
    MotorControlRS::ActionExecution execution = MotorControlRS::ActionExecution::NOT_TRANSMITTED;
    MotorControlRS::ActionCompletion completion = MotorControlRS::ActionCompletion::NOT_OBSERVED;
    MotorControlRS::Status status;
    int64_t effectiveNative = 0;
};
/// Cached inspection only; serviceApplication performs actual bus polling.
bool moveProgress(uint32_t operationId, MoveProgress&);
/// Explicitly free a completed programmatic move result; does not stop the motor.
bool releaseMove(uint32_t operationId);
/** Explicit native stored-profile start-or-interrupt, through the ordinary
 * owner. Original must be an active acknowledged programmatic observed move,
 * with native unwrapped steps and no consumed reference/soft-limit dependence.
 * Simple-motion parent/child IDs are unsupported. command must request
 * INTERRUPT/USE_STORED and exactly match the independently retained original
 * qualification. Its .stored field is ignored: this application supplies that
 * witness, never the new candidate. Rejection preserves operationId and old work.
 * A separate ordinary result is reserved before superseding old observation;
 * in-flight work settles first. A known completed original cancels the unsent
 * replacement. A physical finish-before-TX can still produce a new stored start.
 * ACK is not arrival; possible motion remains interlocked until checked stop.
 * Call from the service task. No retry, automatic stop or result recycling. */
MotorControlRS::Status submitPositionInterrupt(uint32_t originalOperationId,
    const MotorControlRS::ESS_RS::PositionCommand&, uint32_t targetBits, bool relative,
    uint32_t& operationId);
/** Copy pending/terminal programmatic MOVE evidence and relation without traffic
 * or consumption. Missing/released/wrong-kind/console-owned IDs preserve both
 * outputs. Explicit release is blocked while the related peer remains active. */
bool copyMoveResult(uint32_t operationId, MotorControlRS::ESS_RS::MoveContext&, MoveRelation&);
/** Submit one non-changing velocity-parameter read for the selected axis through
 * the normal owner. Call from the service task; rejection preserves operationId.
 * No console is needed. Result storage remains reserved until explicit release;
 * reading does not stage parameters or establish motion qualification. */
MotorControlRS::Status submitVelocityParameterRead(uint32_t& operationId);
/** Submit one seven-word non-changing homing-parameter read for the selected
 * axis. Same service-task, rejection, retention and release contract as velocity
 * reads. Raw parameters do not qualify homing, offset decoding or an origin. */
MotorControlRS::Status submitHomingParameterRead(uint32_t& operationId);
/** Copy pending, failed or completed typed-read state, including owned evidence.
 * Historical target/configuration generations remain those admitted. Missing,
 * released or wrong-kind IDs preserve output. No traffic or result consumption. */
bool copyReadResult(uint32_t operationId, MotorControlRS::ESS_RS::ReadContext& output);
/** Explicit selected-axis native start-speed setting (0..3000). Requires checked
 * configuration, stopped state, feedback and passive input/trigger policy before
 * any traffic. One reserved result owns prior FC03, FC06 and separate readback
 * under one immutable deadline. No motion, save, retry or rollback. Call from
 * the service task; rejection preserves ID. Possible writes expire readiness,
 * not the fixed host origin. Readback proves stored state, not physical RPM or
 * activation. Retained results require releaseOperation, including failures.
 */
MotorControlRS::Status submitPositionStartSpeed(uint32_t native, uint32_t& operationId);
/** Independently qualified relation between an exact stored start-speed word
 * and the minimum compatible speed in the existing move request's RPM units.
 * No conversion formula or physical calibration is inferred. qualifiedUs is
 * the original application-monotonic qualification time, never readback time.
 * ApplicationOptions::observationMaxAgeMs also bounds this witness (zero keeps
 * it until invalidation). Target/configuration and raw-word matching always apply.
 */
struct PositionStartSpeedRelation {
    MotorControlRS::ReadTarget target;
    uint32_t configurationGeneration = 0;
    uint16_t rawStartSpeed = 0; ///< Exact observed native word, 0..3000.
    uint16_t minimumPositionSpeedRpm = 0; ///< Qualified minimum legal move request, 1..3000; zero is invalid.
    uint64_t qualifiedUs = 0;
};
/** Install caller qualification from the service task while idle, against a
 * fresh matching full position-profile read. Rejection preserves the previous
 * witness; no bus traffic or motion occurs. A possible start-speed write or a
 * contradictory observation revokes it. Fresh reads alone never recreate it.
 * Required after a possible start-speed change before ordinary moves resume.
 */
MotorControlRS::Status installPositionStartSpeedRelation(const PositionStartSpeedRelation&);
/** Explicit selected-axis homing auxiliary choice (0..11 enumeration). Uses the
 * same stopped/input/trigger admission, immutable prior/update/readback session
 * and retained result contract as the start-speed setter. A possible write
 * expires dependent home qualification/prepared work and current references;
 * fixed host origins/scales and desired values remain. Reading back 7 does not
 * qualify its active policy, zero offset, any method or a mechanical origin.
 * No home, parameter staging, I/O reassignment, save, retry or rollback follows.
 */
MotorControlRS::Status submitHomingAuxiliary(MotorControlRS::ESS_RS::HomingAuxiliary choice, uint32_t& operationId);
/** Copy the current/terminal settings context directly into caller storage.
 * kind=READ identifies an unfinished/failed prior read; UPDATE retains its prior
 * in prerequisites.previous. UPDATE_NOT_ADMITTED retains a checked prior read
 * when changed prerequisites/deadline prevent the update. No write follows it.
 * Missing/released/wrong-kind IDs preserve output.
 * No large temporary copy, traffic or result consumption. */
bool copySettingsResult(uint32_t operationId, MotorControlRS::ESS_RS::DriverContext& output);
/// Release any terminal ordinary operation after service has retained its result; no motor stop.
bool releaseOperation(uint32_t operationId);
}
