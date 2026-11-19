/** @file DriverSettings.h
 * @brief Bounded ESS settings reads and stopped-state updates. No I/O or retries.
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "MotorControlRS/profiles/ess_rs/Actions.h"
#include "MotorControlRS/profiles/ess_rs/Reads.h"

namespace MotorControlRS { namespace ESS_RS {

constexpr uint8_t DRIVER_FIELD_COUNT = 16;
constexpr uint8_t DRIVER_READ_STEPS = 4;
constexpr uint8_t DRIVER_MAX_STEPS = 18;
constexpr std::size_t DRIVER_MAX_REPLY_BYTES = 15;
constexpr std::size_t DRIVER_MAX_REQUEST_BYTES = 13; ///< FC10 of one two-word pair: a soft limit or a stored PT target.
/** Soft-limit pair claims promoted by wired bench 10 (decision D10), model
 * 0x4EEA firmware 0x0029: FC10 0x0037/2 and 0x0039/2 at 0x0019 = 0, high word
 * at the lower address, two's complement, native position increments (the
 * 0x000A unit). Order 1 is not admitted. */
constexpr int32_t DRIVER_LIMIT_MAGNITUDE = 0x0FFFFFFF;
enum class DriverGroup : uint8_t { DRIVE, IO, POSITION_SEGMENT, SPEED_SEGMENT, SEGMENT_START_SPEED, CONTROL_SETTINGS,
    FILTERS, CURRENT_LOOP, LA, COLLISION, POSITION_START_SPEED, HOMING_AUXILIARY };
enum class DriverField : uint32_t {
    DIRECTION = 1, SUBDIVISION = 2, WORD_ORDER = 4, SOFT_LIMIT_ENABLE = 8,
    OVER_LIMIT_STOP = 16, INTERRUPTION = 32, POSITION_MODE = 64,
    POSITIVE_LIMIT = 128, NEGATIVE_LIMIT = 256,
    INPUT_POLARITY = 1u << 9, INPUT_X0 = 1u << 10, INPUT_X1 = 1u << 11,
    INPUT_X2 = 1u << 12, INPUT_X3 = 1u << 13, OUTPUT_POLARITY = 1u << 14,
    OUTPUT_Y0 = 1u << 15, OUTPUT_Y1 = 1u << 16, CUSTOM_OUTPUT = 1u << 17,
    SEGMENT_SPEED = 1u << 18, SEGMENT_ACCELERATION = 1u << 19,
    SEGMENT_DECELERATION = 1u << 20, SEGMENT_START_SPEED = 1u << 21,
    SEGMENT_PULSE_TARGET = 1u << 22,
    CONTROL_ALGORITHM = 1u << 23, CONFIGURED_ENCODER = 1u << 24,
    MAX_EFFECTIVE_CURRENT = 1u << 25, CLOSED_MAX_CURRENT = 1u << 26,
    CLOSED_BASE_CURRENT = 1u << 27, OPEN_MAX_CURRENT = 1u << 28,
    LOCK_CURRENT = 1u << 29, LOCK_DELAY = 1u << 30
};
enum class DriverKind : uint8_t { READ, UPDATE };
enum class DriverOutcome : uint8_t {
    NONE, SUCCESS, REPLY_ERROR, TRANSPORT_ERROR, CANCELLED, DEADLINE,
    TIMING_UNQUALIFIED, UNCONFIRMED_RESPONSE, READBACK_MISMATCH,
    UPDATE_NOT_ADMITTED ///< Application prior-read/update session retained the read, but rejected update admission; no write.
};
enum class DriverError : int32_t {
    INVALID_TARGET, INVALID_OPERATION, INVALID_DEADLINE, INVALID_STATE,
    INVALID_CANDIDATE, WRONG_CORRELATION, INVALID_EVENT, CLOCK_ERROR,
    STALE_SETTINGS, STATIONARY_REQUIRED, LIMIT_REFERENCE_REQUIRED,
    LIMIT_DEPENDENCY, PAIR_WRITE_UNSUPPORTED, DEADLINE_EXPIRED,
    TRANSPORT_FAILURE, CANCELLED, TIMING_UNQUALIFIED, UNCONFIRMED_RESPONSE,
    READBACK_MISMATCH, NOT_COMPLETE, WIRING_REQUIRED, IO_EVIDENCE_REQUIRED,
    IO_EFFECTS_REQUIRED, OUTPUT_FUNCTION_UNRESOLVED, CUSTOM_DEPENDENCY,
    SEGMENT_INDEX, SEGMENT_SIGN_UNRESOLVED, TRIGGER_POLICY_REQUIRED,
    MODEL_REQUIRED, CONTROL_EFFECTS_REQUIRED, CURRENT_LIMIT_UNRESOLVED,
    CURRENT_BASE_UNRESOLVED, CURRENT_DEPENDENCY, ZERO_ENCODER_SCALE,
    TUNING_EFFECTS_REQUIRED, TUNING_ACCESS_UNRESOLVED
};
/** Select fields explicitly; values in unselected fields are ignored.
 * POSITIVE_LIMIT/NEGATIVE_LIMIT (DRIVE group) are signed native limits written
 * as one FC10 of two words each (0x0037/2, 0x0039/2), never split FC06 halves.
 * They are admitted alone (one or both), within +-DRIVER_LIMIT_MAGNITUDE, at
 * 0x0019 known 0 and 0x0018 known 0 in the prior DRIVE read (wired bench 10).
 * SEGMENT_PULSE_TARGET (POSITION_SEGMENT group) is one FC10 of the record's
 * two pulse words (0x0060 + 6(n-1)) then a two-word readback, alone, at 0x0019
 * known 0 (DriverPrerequisites::wordOrder), within 0..DRIVER_LIMIT_MAGNITUDE:
 * the claim wired bench 13's W0a witness promoted (order 0, non-negative; the
 * sign witness did not run, so a negative target is UNSUPPORTED before TX).
 * interruption selects the external PV trigger level/edge, not serial bit 3. */
struct DriverRequest {
    DriverGroup group = DriverGroup::DRIVE;
    uint32_t fields = 0;
    uint32_t configurationGeneration = 0;
    uint16_t positionStartSpeed = 0; ///< POSITION_START_SPEED local bit 0; native 0..3000, physical scaling unresolved.
    HomingAuxiliary homingAuxiliary = HomingAuxiliary::KEEP_POSITION_SET_ZERO; ///< HOMING_AUXILIARY local bit 0; enumerated choices 0..11, never flags.
    DefaultDirection direction = DefaultDirection::NORMAL;
    uint16_t subdivision = 400;
    WordOrder wordOrder = WordOrder::HIGH_WORD_FIRST;
    SoftLimitEnable softLimitEnable = SoftLimitEnable::LIMITS_OFF;
    OverLimitStop overLimitStop = OverLimitStop::FREE_PARKING;
    PvTriggerMode interruption = PvTriggerMode::LEVEL;
    PositionMode positionMode = PositionMode::RELATIVE;
    int64_t positiveLimit = 0, negativeLimit = 0;
    InputFunction inputFunctions[4] = {};
    OutputFunction outputFunctions[2] = {};
    uint16_t inputPolarity = 0, outputPolarity = 0, customOutput = 0;
    uint8_t segmentIndex = 1; ///< Stored record number, 1..16.
    int32_t segmentSpeed = 0, segmentStartSpeed = 0;
    uint16_t segmentAcceleration = 0, segmentDeceleration = 0; ///< Native time words, not physical acceleration.
    int64_t segmentPulseTarget = 0; ///< Stored PT target in native pulses, 0..DRIVER_LIMIT_MAGNITUDE (wired bench 13).
    ControlAlgorithm controlAlgorithm = ControlAlgorithm::ALGORITHM_1;
    uint16_t encoderResolution = 0, maximumEffectiveCurrentMa = 0;
    uint16_t closedMaximumPercent = 0, closedBasePercent = 0,
             openMaximumPercent = 0, lockPercent = 0, lockDelayMs = 0;
    uint16_t tuningValues[DRIVER_FIELD_COUNT] = {}; ///< Tuning groups use local fields bits 0..count-1; native words only.
};
/** Copied closure/transport and checked-parser evidence. Raw prefix retains the
 * full supplied size separately. Delivery is not observation freshness. */
struct DriverEvidence {
    uint8_t step = 0;
    uint16_t reg = 0, count = 0;
    bool write = false;
    ReadEventKind event = ReadEventKind::FRAME;
    uint8_t raw[DRIVER_MAX_REPLY_BYTES] = {};
    std::size_t length = 0, receivedLength = 0, txAccepted = 0;
    bool txComplete = false, responseConfirmed = false, qualified = false;
    bool executionUnknown = false;
    uint64_t attemptedUs = 0; ///< Step eligibility bounds observation age; closure alone does not.
    uint64_t earliestUs = 0, latestUs = 0, deliveredUs = 0;
    int32_t transportDetail = 0;
    Status status;
    FrameError frameError = FrameError::NONE;
};
/** Whole successful read, not an atomic device snapshot. raw[] order is
 * direction, subdivision, word-order, soft-enable, over-limit, PV trigger,
 * external position mode, input polarity, four input functions, output polarity,
 * two output functions and custom-output mask. DRIVE and IO populate separate
 * groups. POSITION_START_SPEED and HOMING_AUXILIARY use raw[0] and local
 * knownFields bit 0 for native 0..3000 and choices 0..11 respectively;
 * unknown prior words remain readable. knownFields means
 * resolved legal codes/native domain, never active settings or physical units.
 * Limits assemble unsigned bits only; signed meaning/native scale are unresolved. */
struct DriverObservation {
    DriverGroup group = DriverGroup::DRIVE;
    ReadTarget target;
    uint32_t operationId = 0, configurationGeneration = 0;
    uint16_t raw[DRIVER_FIELD_COUNT] = {};
    uint8_t segmentIndex = 1; ///< Segment groups: raw[0..2] speed/ramp/ramp; PT raw[3..4] pulse words.
    uint32_t knownFields = 0;
    uint16_t positiveWords[2] = {}, negativeWords[2] = {};
    bool pairKnown = false;
    uint32_t positiveBits = 0, negativeBits = 0;
    /** Signed limits decoded only for the promoted claim: DRIVE read with
     * 0x0019 known 0 (high word first, two's complement; wired bench 10). */
    bool limitsDecoded = false;
    int32_t positiveLimit = 0, negativeLimit = 0;
    DriverEvidence provenance[DRIVER_READ_STEPS];
};
/** Where the reported status bit (0x0008/0x0009) sits relative to the polarity
 * inversion (0x0040/0x004B). The manuals do not say. BEFORE_INVERSION: the bit
 * is the physical level and the effective level is bit XOR polarity.
 * AFTER_INVERSION: the bit is the effective level. A non-UNKNOWN value is an
 * independently qualified source or measurement fact for one exact model and
 * firmware, never a family default; the core only applies it.
 * With UNKNOWN only unchanged default polarity has a known level, and an input
 * polarity change is admitted only when the terminal's previous function and
 * the candidate's final function are both 0 (no drive effect), connected or not.
 * An output polarity change with UNKNOWN is admitted only when the terminal's
 * declared load permits either level (IoOutputFacts::allowedLevels 3), because
 * its level after the inversion is not predicted; every other load refuses it. */
enum class IoStatusMapping : uint8_t { UNKNOWN, BEFORE_INVERSION, AFTER_INVERSION };
/** Caller-stated facts for one input terminal. A connected terminal needs
 * supplied facts for every change, including a no-function inversion, and a
 * confirmed FC06 reply (no echo-readback). */
struct IoInputFacts {
    bool supplied = false;
    IoStatusMapping statusMapping = IoStatusMapping::UNKNOWN; ///< Stated or recorded for this exact model/firmware.
    bool externalTriggersInhibited = false; ///< Continuing inhibition, not one inactive sample.
    bool protectionRemovalQualified = false; ///< Connected limit/release/stop removal or polarity change.
};
enum class IoOutputLoad : uint8_t { UNDECLARED, NO_LOAD, INDICATOR, CONTROL };
struct IoOutputFacts {
    bool supplied = false;
    IoStatusMapping statusMapping = IoStatusMapping::UNKNOWN;
    IoOutputLoad load = IoOutputLoad::UNDECLARED;
    uint8_t allowedLevels = 0; ///< Bit 0 permits inactive, bit 1 active, after inversion, at every intermediate write.
};
/** Application-supplied qualifications bound to the exact stopped observation.
 * Enabling soft limits additionally needs qualified native bounds, homing and
 * a reference from the same generation. Raw limit reads alone prove none of it.
 * No input function is silently changed or treated as disabled. */
struct DriverPrerequisites {
    uint32_t configurationGeneration = 0;
    DriverObservation previous;
    bool stationaryQualified = false, inputsPermit = false;
    ReadTarget stationaryTarget;
    uint16_t rawAlarm = 0, rawMotion = 0;
    uint64_t stationaryEarliestUs = 0, stationaryLatestUs = 0, maxAgeUs = 0; ///< Zero disables age expiry; provenance/generation and operation deadlines remain required.
    bool limitSemanticsQualified = false, homedReferenceQualified = false;
    ReadTarget referenceTarget;
    uint32_t referenceConfigurationGeneration = 0;
    /** Caller-qualified decoding must map these exact retained raw words to
     * the native signed bounds below. Neither a cast nor a host origin is proof. */
    uint16_t qualifiedPositiveWords[2] = {}, qualifiedNegativeWords[2] = {};
    int64_t positiveLimit = 0, negativeLimit = 0;
    InputWiring inputWiring[4] = {}, outputWiring[2] = {};
    bool ioLevelsQualified = false;
    ReadTarget ioTarget;
    uint32_t ioConfigurationGeneration = 0;
    uint16_t actualInputs = 0, actualOutputs = 0;
    uint64_t ioEarliestUs = 0, ioLatestUs = 0;
    IoInputFacts inputFacts[4];
    IoOutputFacts outputFacts[2];
    /** Caller qualification lifetime is separate from refreshed live I/O.
     * Zero maximum age disables expiry; the original operation deadline remains. */
    uint64_t ioQualificationUs = 0, ioQualificationMaxAgeUs = 0;
    /** Independent application qualification of the exact candidate and its
     * external effects. An asserted input may be reinterpreted by reassignment.
     * UNCONNECTED is distinct from a disabled function; none proves no output
     * electrical level and does not establish enable/release precedence. */
    uint32_t ioEffectsQualifiedFields = 0;
    DriverRequest qualifiedIo;
    /** IO verification policy for known UNCONNECTED affected terminals.
     * A polarity-word write requires every terminal in that word UNCONNECTED,
     * including unchanged bits. Also available for
     * segment configuration with independently qualified trigger inhibition,
     * or exact independently qualified control/tuning-setting effects.
     * A checked, on-time write echo with unconfirmed source or supplied execution
     * uncertainty may lead to a separate confirmed
     * readback. It never becomes an acknowledgement or proves activation. */
    bool allowEchoReadback = false;
    /** Independent qualification that external execution cannot occur during
     * this segment, position-start-speed or homing-auxiliary update. The latter
     * two also require inputsPermit; qualifiedSegment applies only to segments.
     * No input is silently disabled. */
    bool externalTriggerInhibitedQualified = false;
    ReadTarget triggerTarget;
    uint32_t triggerConfigurationGeneration = 0;
    uint64_t triggerEarliestUs = 0, triggerLatestUs = 0;
    DriverRequest qualifiedSegment;
    /** 0x0019 of the same target and configuration generation (a configuration
     * read). A stored PT target write needs it known and 0 (wired bench 13). */
    bool wordOrderKnown = false;
    WordOrder wordOrder = WordOrder::HIGH_WORD_FIRST;
    /** External exact-model identification remains independent of raw model
     * codes. Peak datasheet amperes are not an effective/RMS current ceiling. */
    IdentityObservation controlIdentity;
    bool exactModelQualified = false;
    uint32_t modelSourceId = 0;
    uint16_t qualifiedModelCode = 0, qualifiedFirmwareCode = 0;
    bool nativeCurrentLimitQualified = false;
    uint16_t maximumEffectiveLimitMa = 0;
    /** Independent exact-firmware evidence must establish that current
     * percentages use 0x0102. The manual's 0x2042 reference establishes no alias. */
    bool currentPercentBaseQualified = false;
    uint32_t controlEffectsQualifiedFields = 0;
    DriverRequest qualifiedControl;
    uint64_t controlEarliestUs = 0, controlLatestUs = 0;
    /** Independent effects qualification for the exact native tuning candidate,
     * including all intermediate writes. Shares the checked exact-model identity
     * above; no gain, filter-delay or arrival-time formula is inferred. */
    uint32_t tuningEffectsQualifiedFields = 0;
    DriverRequest qualifiedTuning;
    uint64_t tuningEarliestUs = 0, tuningLatestUs = 0;
};
/** Each selected field's exact progress. Active/persistence remain unknown:
 * checked echo and matching readback do not document activation or rollback. */
struct DriverProgress {
    DriverField field = DriverField::DIRECTION;
    uint16_t reg = 0, previous = 0, requested = 0, readback = 0;
    /** Two-word pair (a soft limit, 0x0037/0x0039, or a stored PT target,
     * 0x0060 + 6(n-1)): both words in address order, high word first at word
     * order 0; previous/requested/readback above repeat the first (high) word. */
    bool pair = false;
    uint16_t previousWords[2] = {}, requestedWords[2] = {}, readbackWords[2] = {};
    bool selected = false, acknowledged = false, readbackKnown = false;
    bool activeKnown = false;
    uint16_t active = 0;
    ActionExecution execution = ActionExecution::NOT_TRANSMITTED;
};
/** Caller-owned bounded state; treat fields as read-only between calls. There
 * are at most nine writes and nine reads (seven for DRIVE). Accepted writes retain effects and
 * uncertainty even when cancellation, expiry or a later read fails. Never retry
 * or rollback automatically. Historical previous/evidence keep original binding. */
struct DriverContext {
    DriverGroup group = DriverGroup::DRIVE;
    ReadTarget target;
    uint32_t operationId = 0, configurationGeneration = 0;
    DriverKind kind = DriverKind::READ;
    ReadState state = ReadState::EMPTY;
    DriverOutcome outcome = DriverOutcome::NONE;
    DriverRequest request;
    DriverPrerequisites prerequisites;
    uint64_t startedUs = 0, deadlineUs = 0, servicedUs = 0, eligibleUs = 0;
    uint8_t step = 0, completedSteps = 0, fieldCount = 0;
    uint8_t order[DRIVER_FIELD_COUNT] = {};
    DriverProgress progress[DRIVER_FIELD_COUNT];
    DriverEvidence observations[DRIVER_MAX_STEPS];
    uint32_t effects = 0; ///< Possibly changed field mask; never cleared on failure.
    bool uncertain = false; ///< A write lacks confirmed matching readback.
    Status status;
};
struct PreparedDriver {
    ActionWork kind = ActionWork::DONE;
    ReadTarget target;
    uint32_t operationId = 0;
    uint8_t step = 0;
    bool write = false;
    uint8_t function = 0; ///< 3, 6 or 16 (pair write: a soft limit or a stored PT target).
    uint16_t reg = 0, value = 0, count = 0;
    uint8_t bytes[DRIVER_MAX_REQUEST_BYTES] = {};
    std::size_t length = 0;
    uint64_t deadlineUs = 0;
};

/** All rejected preparations leave output unchanged, and yield no traffic.
 * Request/prerequisite inputs must not alias the output's embedded members;
 * this is checked before mutation and avoids a full-context stack copy. */
Status prepareDriverRead(DriverContext&, const ReadTarget&, uint32_t operationId,
                         uint32_t configurationGeneration, uint64_t nowUs, uint64_t deadlineUs,
                         DriverGroup group = DriverGroup::DRIVE, uint8_t segmentIndex = 1) noexcept;
Status prepareDriverSettings(DriverContext&, const ReadTarget&, uint32_t operationId,
                             const DriverRequest&, const DriverPrerequisites&,
                             uint64_t nowUs, uint64_t deadlineUs) noexcept;
/** Immutable write budget capped by all retained prerequisite observations.
 * READ returns the operation deadline. This also supplies application diagnostics. */
uint64_t driverWriteDeadline(const DriverContext&) noexcept;
/** Repeated calls yield the same token; admission/axis reservation are owned by
 * the application. Absolute deadline and readiness bounds are never renewed. */
Status nextDriver(const DriverContext&, uint64_t nowUs, PreparedDriver&) noexcept;
/** Invalid correlations/envelopes leave state unchanged. Qualified on-time final
 * completion can succeed when delivered later. A nonfinal late delivery cannot
 * start another transaction. Reads and update readbacks require responseConfirmed;
 * only explicit unconnected-IO, inhibited-segment or qualified-control/tuning readback policy can continue an unconfirmed
 * source write echo. CANCEL is local and never restores settings. */
Status advanceDriver(DriverContext&, const ActionEvent&, uint64_t nowUs) noexcept;
/** Only a complete READ publishes, leaving output unchanged on all failures. */
Status getDriver(const DriverContext&, DriverObservation&) noexcept;

/** Logical raw/progress indices 0..6 are DRIVE, 7..15 are IO; pairs have no
 * logical writable index. Segment groups use local indices 0..2 (shared start
 * speed only 0). Tuning groups use local bits 0..count-1, interpreted with their
 * group rather than the named global DriverField values. POSITION_START_SPEED
 * and HOMING_AUXILIARY use only local bit 0. An out-of-range index
 * returns the empty field. */
DriverField driverFieldAt(uint8_t index, DriverGroup group = DriverGroup::DRIVE) noexcept;
/** Select the single POSITION_START_SPEED native word (0..3000) before narrowing.
 * Accepts an empty request or an existing selection of this same field; other
 * selections and invalid values preserve request. Retains configurationGeneration.
 * Does not qualify physical RPM, activation or persistence. Execute through
 * prepareDriverRead/getDriver and prepareDriverSettings/nextDriver/advanceDriver.
 */
Status preparePositionStartSpeed(DriverRequest&, uint32_t native) noexcept;
/** Select one homing auxiliary choice 0..11, not a flag mask. Accepts an empty
 * request or its existing one-field selection; invalid casts/mixed selections
 * preserve request, including configurationGeneration. Execute via the checked
 * driver prior-read/update/readback engine (FC03 0x0030/1, FC06 0x0030, FC03).
 * Requires fresh stopped/input/trigger policy and a confirmed write reply.
 * Stored readback does not qualify active policy, offset interpretation or any
 * homing execution. No motion, save, replay or rollback is implied.
 */
Status prepareHomingAuxiliary(DriverRequest&, HomingAuxiliary) noexcept;
/** These setters select the same checked update engine; UNDEFINED explicitly
 * assigns function zero. Bounds/choices fail without changing the request. */
Status prepareInputFunction(DriverRequest&, uint8_t terminal, InputFunction) noexcept;
Status prepareOutputFunction(DriverRequest&, uint8_t terminal, OutputFunction) noexcept;

/** Stored settings only: no active/persistence/electrical-level assertion.
 * Complete read provenance and unknown/reserved raw values are retained. */
struct IoObservation {
    ReadTarget target;
    uint32_t operationId = 0, configurationGeneration = 0;
    uint16_t inputPolarity = 0, outputPolarity = 0, customOutput = 0;
    InputFunction inputFunctions[4] = {};
    OutputFunction outputFunctions[2] = {};
    uint16_t rawInputFunctions[4] = {}, rawOutputFunctions[2] = {};
    uint8_t knownInputFunctions = 0, knownOutputFunctions = 0;
    uint16_t unknownInputPolarityBits = 0, unknownOutputPolarityBits = 0,
             unknownCustomOutputBits = 0;
    DriverEvidence provenance[3];
};
Status getIo(const DriverContext&, IoObservation&) noexcept;

}} // namespace MotorControlRS::ESS_RS
