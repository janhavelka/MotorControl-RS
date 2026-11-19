// SPDX-License-Identifier: MIT
#pragma once
#include "../common/Esp32S3Uart.h"
#include "../common/RtuBusOwner.h"
#include "../common/BenchIo.h"
#include "../common/FixtureWiring.h"
#include "ProbeConsole.h"
#include "MoveRelation.h"
#include "DriveGroup.h"
#include "ProfileCandidate.h"
#include "QualificationConsole.h"
#include "IoQualification.h"
#include "ParameterWrite.h"
#include <MotorControlRS/profiles/ess_rs/Position.h>
#include <MotorControlRS/profiles/ess_rs/Reads.h>
#include <MotorControlRS/profiles/ess_rs/Velocity.h>
#include <MotorControlRS/profiles/ess_rs/DriverSettings.h>
#include <MotorControlRS/profiles/ess_rs/Homing.h>

namespace MotorControlRSExample {
/** Age-only expiry is opt-in. Zero keeps checked observations until invalidated
 * by an event/configuration change; it does not prove the drive stayed powered.
 * Transaction deadlines, missing evidence and generation checks always apply. */
struct ApplicationOptions {
    uint32_t observationMaxAgeMs = 0;
    uint32_t moveTimeoutMs = 30000; ///< 1..30000 ms, move and home observation deadline.
    ApplicationOptions() {
        // Usable nominal convention, explicitly assumed rather than calibrated.
        positionUnits.commandStepsPerMotorTurn = MotorControlRS::UnitScale(1000, 1, MotorControlRS::ScaleSource::ASSUMED);
    }
    MotorControlRS::UnitConfig positionUnits; ///< Host scales; default 1000 command steps/turn is ASSUMED. No device settings write.
    /** Optional bench outputs/input wired to drive terminals; null hides the
     * bench command. It must outlive the application. Every motor stop request
     * turns its outputs OFF before admission; no other motor command uses it. */
    BenchIo::Port* benchIo = nullptr;
    /** Optional board declaration of wired terminals (FixtureWiring.h). Ignored
     * unless fixtureWiringSupplied: an absent table keeps the legacy defaults
     * (UNCONNECTED at boot, UNKNOWN after `useaddr` and for new group members).
     * A supplied table is validated once at construction and kept, unchanged,
     * as the board declaration; listed addresses take it at boot, after
     * `useaddr` and as new group members, other addresses keep the defaults.
     * An empty supplied table is valid. A table that fails validation makes
     * every declaration of every address UNKNOWN; motor control still starts.
     * Host `wiring` and `group wiring` override only the session declaration. */
    FixtureWiringEntry fixtureWiring[FIXTURE_WIRING_CAPACITY] = {};
    uint8_t fixtureWiringCount = 0; ///< Entries used; more than FIXTURE_WIRING_CAPACITY is invalid.
    bool fixtureWiringSupplied = false; ///< Distinguishes an absent table from an empty one.
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

/** Copy a fresh terminal baseline with caller facts unset and declared wiring
 * copied. ASSIGN uses selected-axis IO settings; other purposes use CONFIG.
 * Group members support FINITE_MOVE inputs only. Rejection preserves output. */
MotorControlRS::Status inspectIoQualification(uint8_t address, uint8_t terminal, IoPurpose, IoQualification& output, bool member = false);
/** Validate and copy one exact supplied record while idle. No motor traffic.
 * Replaces the previous purpose for that terminal. Non-assignment purposes
 * require candidate == current assignment. */
MotorControlRS::Status installIoQualification(const IoQualification&);
/** Copy retained facts and their current validity. Assignment requires the same
 * settings/state read identities. Does not refresh the supplied fact age.
 * An absent record or different purpose returns false and preserves outputs. */
bool copyIoQualification(uint8_t address, uint8_t terminal, IoPurpose, IoQualification& output, bool& valid, bool member = false);
/** Revoke future use and cancel unsent dependent work. Local revocation is not a stop. */
MotorControlRS::Status revokeIoQualification(uint8_t address, uint8_t terminal, IoPurpose, bool member = false);
/** The recorded input status mapping (IoStatusRecord.h, wired bench 05) of
 * input terminal 0..3 of the selected axis or a group member. True only when
 * that exact target and binding has a fresh checked identity read whose model
 * and firmware a record names; output is unchanged otherwise. Console token
 * `recorded`; installIoQualification refuses a stated mapping that contradicts
 * an applicable record. No bus traffic. */
bool copyRecordedInputMapping(uint8_t address, uint8_t terminal, MotorControlRS::ESS_RS::IoStatusMapping& output,
                              bool member = false);
/** The recorded output status mapping (IoStatusRecord.h, wired bench 06) of
 * output 0 (Y0) or 1 (Y1) of the selected axis, under the same identity rule.
 * The record has Y0 after inversion for 0x4EEA/0x0029; Y1 has none. Console
 * token `recorded` in an output install; a stated mapping that contradicts it is
 * refused (mapping_contradicts_record). No bus traffic. */
bool copyRecordedOutputMapping(uint8_t address, uint8_t output, MotorControlRS::ESS_RS::IoStatusMapping& mapping);
/** Read one typed settings group through the ordinary owner. Segment groups
 * require index 1..16. Copy results with copySettingsResult; release explicitly.
 * Calls run in the service task and preserve operationId on rejection. */
MotorControlRS::Status submitDriverRead(uint8_t address, MotorControlRS::ESS_RS::DriverGroup,
                                      uint32_t& operationId, uint8_t segmentIndex = 0);
/** Submit an exact selected-axis settings candidate using retained checked reads
 * and supplied qualification. Same API as console settings writes; no replay.
 * DRIVE fields need four current CONFIGURE input records (io qualification,
 * installIoQualification), except subdivision alone on the passive unconnected
 * path. The tuning experiments (input filter 2/3, position-error threshold
 * 1..4000) take the passive path or the same CONFIGURE records; PT, PV and
 * start records take the passive path or four SEGMENT records. A refusal the
 * input facts decide returns detail QUALIFICATION_REQUIRED, msg the reason
 * (input_facts_required, input_facts_stale or input_active; copySettingsRefusal)
 * and sends no frame. */
MotorControlRS::Status submitDriverSettings(uint8_t address, const MotorControlRS::ESS_RS::DriverRequest&, uint32_t& operationId);
/** Restore the six homing words 0x0031..0x0036 (method, search and return
 * rates, ramp, raw offset pair) of the selected axis from an earlier checked
 * home read of the same drive and binding (snapshotId = that read's operation
 * ID; the first checked read of the binding and the seven most recent are
 * kept, so the state before the first write survives the restoration's own
 * reads): one FC10 to the reviewed 0x0031/6 window
 * with the exact words (D7, a nonzero offset included), no start, then one FC03
 * readback. Needs fresh stopped, alarm-free state with zero speed, a fresh
 * checked configuration and no input with function 11 unless a current
 * CONFIGURE record states triggers inhibited. The axis stays reserved until the
 * readback; a write with unknown execution keeps it reserved and refuses the
 * next write until a checked home read settles it. Nothing is resent. A
 * possible write expires home qualification and prepared work, as a stored
 * homing policy does; the position counter is untouched. Copy the session with
 * copyParameterWrite. Refusal detail is the Probe::Action and sends nothing. */
MotorControlRS::Status restoreHomingParameters(uint32_t snapshotId, uint32_t& operationId);
/** The same for the three velocity (JOG) words 0x001D..0x001F from an earlier
 * checked velocity read (0x001D/3 window); inputs with functions 8-10 refuse
 * unless inhibited; a velocity read settles an unknown write. */
MotorControlRS::Status restoreVelocityParameters(uint32_t snapshotId, uint32_t& operationId);
/// Copy the current or finished restore session; another ID preserves output.
bool copyParameterWrite(uint32_t operationId, Probe::ParameterWriteView& output);
/** Copy the input-facts cause of the settings or profile-stage refusal just
 * returned by a submit call or console command: reason, read_first, purpose and
 * terminal. Valid until the next serviceApplication turn or admission attempt.
 * False when the latest refusal had another cause. No traffic.
 * Wired bench 07: also a motion refusal of the shared stop/release rule
 * (purpose motion): moves, stages, group moves, velocity and home with an
 * assigned function 4-6 input active, stale or of unknown mapping, and enable
 * with an assigned release input active. */
bool copySettingsRefusal(Probe::SettingsRefusal& output);

/** Explicit stop on a selected or previously checked address, through the same
 * admission and owner path as the console. Rejection preserves operationId.
 * Two primary results and two same-address fast-escalation reserves are separate
 * from ordinary work. A never-transmitted stop may use one bounded recovery;
 * a transmitted stop is never replayed. Call from the service task. */
MotorControlRS::Status submitStop(uint8_t address, const MotorControlRS::StopPolicy&, uint32_t& operationId);
/** Copied application action evidence. Recovery and the zero-byte attempt before
 * recovery remain separate from the action's eventual write and observation. */
struct ActionResult {
    MotorControlRS::ESS_RS::ActionContext action;
    bool pending = false, interruptedByStop = false, stopEscalation = false;
    uint32_t replacedOperationId = 0, recoveryOperationId = 0;
    bool beforeRecoveryKnown = false;
    Rtu::Completion beforeRecovery;
    Rtu::RecoveryResult recovery;
};
/** Copy a programmatic stop or qualified-release result. A terminal copy marks
 * delivery, allowing explicit releaseOperation or documented stop-slot reuse.
 * Pending includes attached recovery, even after the action ends. Pending copies
 * do not acknowledge delivery or release storage. Wrong IDs/kinds preserve output. */
bool copyActionResult(uint32_t operationId, ActionResult& output);
struct ReleaseAfterStopPrerequisites {
    MotorControlRS::ReadTarget target;
    uint32_t configurationGeneration = 0;
    uint64_t qualifiedUs = 0;
    bool mechanicsPermitRelease = false, externalTriggersInhibited = false;
};
/** Inspect the current binding/configuration/time for an exact retained failed
 * transmitted stop on the selected axis. Supplied-fact flags are returned false.
 * No traffic, fixture inference or change to the failed stop occurs. */
MotorControlRS::Status inspectReleaseAfterStop(uint32_t failedStopId, ReleaseAfterStopPrerequisites& output);
/** Explicitly release winding drive after that failed stop. Supply the actual
 * mechanical and external-trigger facts against inspectReleaseAfterStop's
 * snapshot. Generation and observation-age policy are checked again before TX.
 * RELEASED does not prove standstill or clear possible-motion uncertainty.
 * Enable/alarm clear remain blocked; no restart or replay follows. */
MotorControlRS::Status submitReleaseAfterStop(uint32_t failedStopId,
    const ReleaseAfterStopPrerequisites&, uint32_t& operationId);

/** Submit through the same owner/admission/polling path as console moves.
 * Existing configuration, state and motion-profile reads establish readiness.
 * Call from the serviceApplication task. Output ID is unchanged on rejection.
 * moveBy requests relative motion: steps are native command
 * increments; other units use the motor frame, except millimetres (load frame).
 * Scales come from ApplicationOptions::positionUnits or host axis configuration.
 * Optional setup policy defaults to full write. USE_STORED requires a matching
 * remembered profile/binding in this example; it does not detect an unseen reset.
 * During the 1 s enable settling window, these API calls return BUSY without
 * traffic. The console's moveby/moveto commands wait through that window.
 * Each bus address retains its wait across selection, group changes and recovery;
 * fresh readiness reads do not shorten it.
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
 * INTERRUPT_CURRENT/USE_STORED and exactly match the independently retained original
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
 * configuration, stopped state, feedback and an input policy before any traffic:
 * the passive one (every input function 0, or <= 3 on an UNCONNECTED terminal,
 * none active) or four current CONFIGURE input records with no assigned
 * limit/stop/control input active. A refusal the input facts decide returns
 * QUALIFICATION_REQUIRED with the reason as msg (copySettingsRefusal) and sends
 * not even the prior read; the same check before the FC06 ends the result with
 * UPDATE_NOT_ADMITTED and the reason. One reserved result owns prior FC03, FC06 and separate readback
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
 * The requirement lives in RAM: a controller restart forgets it and uses the
 * stored word under the startup policy. The console route is
 * `motion-profile start-speed relation RAW_WORD MIN_RPM`.
 */
MotorControlRS::Status installPositionStartSpeedRelation(const PositionStartSpeedRelation&);
/** Explicit selected-axis homing auxiliary choice (0..11 enumeration). Uses the
 * same stopped/input/trigger admission (passive or CONFIGURE records, named
 * refusals), immutable prior/update/readback session
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
/** Install exact externally supplied fixture/method/native-word qualification
 * against current checked CONFIG, MOTION, IO and HOMING_PARAMETERS reads.
 * Call from the service task while idle. Stored auxiliary must be 7 and the stored
 * offset zero. Stored method, rates and ramp may differ, because home staging writes
 * them. Reads never supply mechanical, index or reference qualification.
 * Replacement during an active home rejects without changing its snapshot. */
MotorControlRS::Status installHomeQualification(const MotorControlRS::ESS_RS::HomePrerequisites&);
/** Passive future-slot inspection. Absent returns false and preserves outputs.
 * valid includes current binding, independent ages and checked live readback.
 * Own staging consumes this slot; copyHomeResult retains the admitted snapshot. */
bool inspectHomeQualification(MotorControlRS::ESS_RS::HomePrerequisites& output, bool& valid);
/** Clear future qualification and revoke every active home, including after
 * single-use consumption. Unsent work is cancelled; transmitted work settles
 * with its evidence intact. No physical stop or automatic replay occurs. */
MotorControlRS::Status revokeHomeQualification();
/** Install supplied velocity facts against a checked velocity-parameter read, from
 * the service task while idle. Rejection preserves the previous witness; no traffic
 * or motion occurs. Fresh stationary state is still checked at each submission, and
 * this application keeps its finite free-shaft speed and duration envelope. */
MotorControlRS::Status installVelocityQualification(const MotorControlRS::ESS_RS::VelocityPrerequisites&);
/// Clear future velocity admission. An admitted run keeps its snapshot and its stop.
MotorControlRS::Status revokeVelocityQualification();
/** State that zero-only clear semantics are qualified for the current exact
 * target, binding and configuration generation. Call from the service task while
 * idle. No traffic occurs; each clear still needs fresh stopped, enabled,
 * alarm-free state. Recovery, rebind and configuration changes expire the fact. */
MotorControlRS::Status installPositionClearQualification();
/** Passive inspection. Absent preserves both outputs; valid checks the current
 * target/binding/configuration, without claiming current stopped state. */
bool inspectPositionClearQualification(PositionClearQualification& output, bool& valid);
/// Revoke future and unsent clears. Transmitted actions settle with copied evidence.
MotorControlRS::Status revokePositionClearQualification();
/** Ordinary selected-axis home route. Zero generation selects current config;
 * nonzero is checked. Rejection preserves operationId. Results are retained
 * until releaseOperation; the console uses the same admission/owner path.
 * Returns BUSY without traffic during the 1 s enable settling window.
 * Application policy requires checked subdivision and both native rate words
 * times subdivision / 60 <= 50,000 increments/s. This is a nominal host ceiling,
 * not physical rate qualification or a vendor limit; values are never clamped. */
MotorControlRS::Status submitHome(const MotorControlRS::ESS_RS::HomeRequest&, uint32_t& operationId);
/// Copy pending/terminal programmatic home evidence. Wrong/missing IDs preserve output.
bool copyHomeResult(uint32_t operationId, MotorControlRS::ESS_RS::HomeContext& output);
/// Release any terminal ordinary or group operation after service has retained its result; no motor stop.
bool releaseOperation(uint32_t operationId);

/** Drive group: the same application functions as the console
 * `group` routes. Call from the serviceApplication task; rejection preserves
 * operationId. A group is a host selection of up to Probe::GROUP_CAPACITY
 * unicast drives, never a drive change, and leaves the selected axis unchanged.
 * declareGroup is idle-only (1..Probe::GROUP_CAPACITY addresses, start order).
 * Each member needs a fresh checked identity (this
 * exact node, model 0x4EEA, firmware 0x0029), configuration and position-profile
 * read; an absent or unexpected responder rejects the whole declaration. These
 * reads send no write. useaddr, recovery, a host tuple change or a broadcast
 * expire every member's facts; declare the group again. Copy terminal evidence
 * with copyGroupResult and free it with releaseOperation. */
MotorControlRS::Status declareGroup(const uint8_t* addresses, std::size_t count, uint32_t& operationId);
/// Idle-only; forgets members, but preserves enable waits and retained results.
MotorControlRS::Status clearGroup();
/** Same declaration as the selected axis's `wiring`, for one member (X0..X3 =
 * 0..3, Y0..Y1 = 4..5). No drive assignment changes; cached state expires.
 * A change sets the member terminal's source to HOST; the board declaration
 * is never changed. An unchanged value keeps its source. */
MotorControlRS::Status declareGroupWiring(uint8_t address, uint8_t terminal, MotorControlRS::InputWiring state);
/** Selected-axis declaration, the console `wiring` route (X0..X3 = 0..3,
 * Y0..Y1 = 4..5). Idle-only; no bus traffic and no drive assignment change.
 * A change sets the source to HOST, expires I/O facts, prepared work, cached
 * state and the boot origin, and bumps the configuration generation. An
 * unchanged value keeps its source and expires nothing. The board declaration
 * is never changed. Rejection detail is the Probe::Action. Service task only. */
MotorControlRS::Status declareWiring(uint8_t terminal, MotorControlRS::InputWiring state);
/** Copy the selected axis's effective declaration, the source of each
 * terminal, its board declaration and the board table state. No bus traffic. */
bool copyWiring(Probe::WiringSnapshot& output);
/** Copy cached member facts, state and group operation IDs; no bus traffic.
 * Each member view carries its declaration sources and board declaration.
 * The view holds up to 15 members: keep output in static or PSRAM storage, not on
 * a small task stack. The same applies to copyGroupResult. */
bool inspectGroup(Probe::GroupView& output);
/** One common relative displacement and speed. Every member uses its own facts
 * and the shared move preparation rules before any frame is
 * sent. STAGED (default): every member's five 0x0021 words are written and read
 * back first with no start; any staging failure sends no start to any member.
 * Then only the start words go out, back to back, each after the previous start
 * reply was checked, and each start relies on that member's checked staged words.
 * SEQUENTIAL: each member runs its ordinary move after the previous start resolved.
 * The failure policy is mandatory. After a failed or lost start no later start
 * is sent and nothing is resent; each member's outcome stays its own.
 * Stop policies require one free/reusable stop result per member and reserve
 * fallback stop/recovery IDs before writes. Existing undelivered results stay.
 * An explicit stop takes over its member's stop obligation; the group keeps its
 * recovery reserve until those stops settle. Results remain independently owned.
 * Any member's unexpired enable wait returns BUSY before staging or start. */
MotorControlRS::Status submitGroupMove(const Probe::GroupMoveRequest& request, uint32_t& operationId);
/** Copied per-member relative/absolute requests, using the same group executor.
 * Reject incomplete, duplicate, stale or unsupported intent before any write.
 * The setup, stop reservations and enable waits documented by submitGroupMove
 * also apply. Wrapped angles remain unsupported. Rejection preserves operationId. */
MotorControlRS::Status submitGroupMoves(const Probe::GroupMemberMoves& request, uint32_t& operationId);
/** Apply ordinary host axis configuration/origin rules to one member. No I/O.
 * Requires current stationary observations; origins require an installed reference.
 * QUERY copies only; failures preserve output. */
MotorControlRS::Status configureGroupAxis(uint8_t address, const Probe::AxisCommand& command, Probe::AxisView& output);
/// Install exact caller-qualified reference against current config/read IDs; no I/O.
MotorControlRS::Status installGroupReference(const Probe::GroupReference& reference);
/// Revoke future reference/origin use while idle; does not stop a motor.
MotorControlRS::Status revokeGroupReference(uint8_t address);
/** Bounded inert plan used by the console and upper firmware. Begin refuses an
 * existing plan; add rejects duplicates/non-members and captures current generation
 * if the supplied generation is zero. Only submit can send motor work. A rejected
 * submit preserves the plan; success consumes it. inspectGroup copies its state. */
MotorControlRS::Status beginGroupPlan(Probe::GroupStartMode start, Probe::GroupFailurePolicy onFailure);
/// Add one copied member intent to the inert plan; no motor traffic.
MotorControlRS::Status addGroupPlanMember(uint8_t address, const MotorControlRS::MoveRequest& request);
/// Forget only the inert plan. Does not cancel an admitted operation or stop a drive.
MotorControlRS::Status clearGroupPlan();
/// Submit a complete plan through submitGroupMoves admission; rejection preserves plan and output ID.
MotorControlRS::Status submitGroupPlan(uint32_t& operationId);
/// One urgent unicast stop per member, each with its own reservation and result.
MotorControlRS::Status submitGroupStop(MotorControlRS::StopBehavior behavior, uint32_t& operationId);
/// Refresh every member's cached state with one checked state read each.
MotorControlRS::Status submitGroupStateRead(uint32_t& operationId);
/// Copy pending or terminal group evidence without releasing it. Missing IDs preserve output.
bool copyGroupResult(uint32_t operationId, Probe::GroupResult& output);

/** Stage one finite move on the selected axis without starting it: the same
 * readiness and preparation as submitMove, then one FC10 write of the five 0x0021
 * words and one FC03 readback. No start word is sent and 0x0020 is not written.
 * A matching readback records the stored-word witness, so a following
 * submitMove(..., MoveSetup::USE_STORED) for the same request sends only the
 * start. Any later write, settings change, profile restore, broadcast, rebinding
 * or recovery ends the witness. A stop cancels staging; an in-flight reply
 * settles and its evidence remains, but cannot publish a stored-word witness.
 * An unexpired enable wait returns BUSY before the staging write.
 * One session: a new stage replaces a finished one. Rejection preserves ID. */
MotorControlRS::Status stagePositionMove(const MotorControlRS::PositionRequest& position, uint16_t speedRpm,
                                         uint32_t& operationId);
/// Copy the selected axis's pending or finished stage evidence; another ID preserves output.
bool copyPositionStage(uint32_t operationId, Probe::PositionStageView& output);
/** Stage untrusted raw profile values after fresh checked readiness and a saved
 * drive snapshot. Inputs follow submitPositionStartSpeed: the passive path or
 * four CONFIGURE records, with named refusals before any traffic.
 * Uses one three-second deadline and a fresh six-word prior
 * read. Wire-valid values may exceed motion policy; this operation never starts
 * motion. Output ID stays unchanged on refusal, including TARGET_RANGE.
 * A possible write revokes the address's start-speed relation, including for
 * groups. Matching readback does not publish a prepared-move witness.
 * Call from the service task; the retained slot requires explicit release. */
MotorControlRS::Status submitProfileCandidate(const Probe::ProfileCandidate&, uint32_t& operationId);
/// Copy pending or terminal candidate evidence. Missing IDs preserve output.
bool copyProfileCandidate(uint32_t operationId, Probe::ProfileCandidateResult& output);
/// Release a delivered terminal candidate only; no bus traffic or rollback.
bool releaseProfileCandidate(uint32_t operationId);
}
