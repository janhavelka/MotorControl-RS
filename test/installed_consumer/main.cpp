// SPDX-License-Identifier: MIT
#include <MotorControlRS/MotorControlRS.h>
// Common intents must be usable before any concrete profile header is included.
static_assert(sizeof(MotorControlRS::VelocityRequest) > 0, "umbrella exports velocity intent");
#include <MotorControlRS/profiles/ess_rs/Homing.h>
// This target sees only the installed package's exported include directories.
#include <MotorControlRS/ReadOperation.h>
#include <MotorControlRS/profiles/ess_rs/Discovery.h>
#include <MotorControlRS/Axis.h>
#include <MotorControlRS/profiles/ess_rs/Reads.h>
#include <MotorControlRS/profiles/ess_rs/Actions.h>
#include <MotorControlRS/profiles/ess_rs/Position.h>
#include <MotorControlRS/VelocityOperation.h>
#include <MotorControlRS/profiles/ess_rs/Velocity.h>
#include <MotorControlRS/profiles/ess_rs/DriverSettings.h>
#include <MotorControlRS/profiles/ess_rs/Segments.h>
#include <MotorControlRS/profiles/ess_rs/ControlSettings.h>
#include <MotorControlRS/profiles/ess_rs/Tuning.h>
#include <MotorControlRS/profiles/ess_rs/Communication.h>
#include <MotorControlRS/profiles/ess_rs/Persistence.h>
#include <MotorControlRS/ActionOperation.h>
#include <MotorControlRS/MotorControlRS.h>
#include <MotorControlRS/MoveOperation.h>
#include <MotorControlRS/Status.h>
#include <MotorControlRS/Traffic.h>
#include <MotorControlRS/Units.h>
#include <MotorControlRS/Version.h>
#include <MotorControlRS/profiles/ess_rs/Codec.h>
#include <MotorControlRS/profiles/ess_rs/Defaults.h>
#include <MotorControlRS/profiles/ess_rs/Registers.h>
#include <MotorControlRS/profiles/ess_rs/Traffic.h>
#include <MotorControlRS/profiles/ess_rs/Types.h>
#include <cstring>

namespace {
bool scalarSettings(const MotorControlRS::ReadTarget& target) {
    using namespace MotorControlRS;
    using namespace MotorControlRS::ESS_RS;
    // Independent FC03/FC06 vectors: native start-speed maximum and auxiliary
    // choice 11. Neither stored result qualifies active behavior or a home.
    const struct {
        DriverGroup group;
        uint16_t reg, value;
        uint8_t read[8], write[8], readback[7];
    } cases[] = {
        {DriverGroup::POSITION_START_SPEED, 0x0020, 3000,
            {1,3,0,0x20,0,1,0x85,0xC0}, {1,6,0,0x20,0x0B,0xB8,0x8F,0x42},
            {1,3,2,0x0B,0xB8,0xBF,0x06}},
        {DriverGroup::HOMING_AUXILIARY, 0x0030, 11,
            {1,3,0,0x30,0,1,0x84,0x05}, {1,6,0,0x30,0,11,0xC8,0x02},
            {1,3,2,0,11,0xF9,0x83}}
    };
    const uint8_t unknownPrior[] = {1,3,2,0xFF,0xFF,0xB9,0xF4};
    for (const auto& item : cases) {
        DriverContext context; PreparedDriver work;
        if (!prepareDriverRead(context, target, 20, 3, 100, 5000, item.group) ||
            !nextDriver(context, 100, work) || work.write || work.count != 1 ||
            work.length != sizeof(item.read) || std::memcmp(work.bytes, item.read, sizeof(item.read))) return false;
        ActionEvent event; event.transport.target = target; event.transport.operationId = 20;
        event.transport.frame = unknownPrior; event.transport.length = sizeof(unknownPrior);
        event.transport.txAccepted = 8; event.transport.qualified = true;
        event.transport.earliestUs = 200; event.transport.latestUs = 220;
        event.txComplete = event.responseConfirmed = true;
        DriverPrerequisites prerequisites;
        if (!advanceDriver(context, event, 230) || !getDriver(context, prerequisites.previous) ||
            prerequisites.previous.raw[0] != 0xFFFF || prerequisites.previous.knownFields ||
            prerequisites.previous.provenance[0].length != 7) return false;
        prerequisites.configurationGeneration = 3;
        prerequisites.stationaryTarget = prerequisites.triggerTarget = target;
        prerequisites.triggerConfigurationGeneration = 3;
        prerequisites.stationaryQualified = prerequisites.inputsPermit = prerequisites.externalTriggerInhibitedQualified = true;
        prerequisites.stationaryEarliestUs = prerequisites.triggerEarliestUs = 230;
        prerequisites.stationaryLatestUs = prerequisites.triggerLatestUs = 230;
        prerequisites.maxAgeUs = 1000;
        DriverRequest request; request.configurationGeneration = 3;
        const Status selected = item.group == DriverGroup::POSITION_START_SPEED ?
            preparePositionStartSpeed(request, item.value) :
            prepareHomingAuxiliary(request, HomingAuxiliary::KEEP_POSITION_SUBTRACT_OFFSET);
        if (!selected || !prepareDriverSettings(context, target, 21, request, prerequisites, 300, 5000) ||
            !nextDriver(context, 300, work) || !work.write || work.reg != item.reg ||
            work.length != sizeof(item.write) || std::memcmp(work.bytes, item.write, sizeof(item.write)) ||
            driverWriteDeadline(context) != 1100) return false;
        event.transport.operationId = 21; event.transport.frame = item.write;
        event.transport.length = sizeof(item.write); event.transport.earliestUs = 400; event.transport.latestUs = 420;
        if (!advanceDriver(context, event, 430) || !nextDriver(context, 430, work) || work.write ||
            work.step != 1 || work.count != 1 || work.reg != item.reg ||
            std::memcmp(work.bytes, item.read, sizeof(item.read))) return false;
        event.transport.step = 1; event.transport.frame = item.readback; event.transport.length = sizeof(item.readback);
        event.transport.earliestUs = 500; event.transport.latestUs = 520;
        if (!advanceDriver(context, event, 530) || context.state != ReadState::SUCCEEDED ||
            context.effects != 1 || context.uncertain || context.prerequisites.previous.raw[0] != 0xFFFF ||
            !context.progress[0].acknowledged || !context.progress[0].readbackKnown ||
            context.progress[0].readback != item.value || context.progress[0].activeKnown) return false;
    }
    return true;
}

bool admittedSwitchHomes(const MotorControlRS::ReadTarget& target) {
    using namespace MotorControlRS;
    using namespace MotorControlRS::ESS_RS;
    // Fixture-supplied mechanical/input/index qualification is independent of
    // register reads. These representative limit/origin/combined methods cover
    // both index and non-index admission without running a physical trajectory.
    const struct {
        HomingMethod method;
        uint16_t functions[2];
        uint8_t roles;
        bool index;
        uint8_t staging[21];
    } cases[] = {
        {HomingMethod::METHOD_1, {3,0}, 4, true,
            {1,16,0,49,0,6,12,0,1,0,60,0,30,0,100,0,0,0,0,0x55,0x67}},
        {HomingMethod::METHOD_3, {1,0}, 1, true,
            {1,16,0,49,0,6,12,0,3,0,60,0,30,0,100,0,0,0,0,0x5E,0xDF}},
        {HomingMethod::METHOD_7, {1,2}, 3, true,
            {1,16,0,49,0,6,12,0,7,0,60,0,30,0,100,0,0,0,0,0x4B,0xEF}},
        {HomingMethod::METHOD_24, {1,2}, 3, false,
            {1,16,0,49,0,6,12,0,24,0,60,0,30,0,100,0,0,0,0,0x2F,0x3B}},
        {HomingMethod::METHOD_28, {1,3}, 5, false,
            {1,16,0,49,0,6,12,0,28,0,60,0,30,0,100,0,0,0,0,0x3A,0x0B}}
    };
    AxisConfig axis; axis.target = target; axis.generation = 3;
    for (const auto& item : cases) {
        HomeRequest request; request.method = item.method; request.configurationGeneration = 3;
        HomePrerequisites p; p.target = target; p.configurationGeneration = 3;
        p.qualifiedMethod = item.method; p.qualifiedSearchSpeed = 60; p.qualifiedReturnSpeed = 30; p.qualifiedRampTime = 100;
        p.methodQualified = p.nativeRatesQualified = p.nativeRampQualified = p.zeroOffsetQualified = true;
        p.auxiliaryQualified = p.inputsQualified = p.readinessQualified = p.referenceSemanticsQualified = true;
        p.observeInputs = true; p.indexQualified = item.index; p.availableInputs = item.roles;
        p.observedUs = p.qualifiedUs = p.ioObservedUs = p.readbackObservedUs = 2300;
        p.maximumAgeUs = p.qualificationMaximumAgeUs = 1000; p.rawMotion = 1;
        for (unsigned slot = 0; slot < 2; ++slot) {
            p.inputFunctions[slot] = item.functions[slot];
            if (item.functions[slot]) p.inputWiring[slot] = InputWiring::CONNECTED;
        }
        HomeContext home; PreparedHome work;
        if (!checkHomeQualification(p, 2400) || !MotorControlRS::prepareHome(home, axis, 22, request, p, 2400, 10000) ||
            !nextHome(home, 2400, work) || !work.write || work.reg != 0x0031 || work.count != 6 ||
            work.length != sizeof(item.staging) || std::memcmp(work.bytes, item.staging, sizeof(item.staging))) return false;
        ActionEvent cancel; cancel.transport.target = target; cancel.transport.operationId = 22;
        cancel.transport.kind = ReadEventKind::CANCEL;
        AxisReference reference; reference.nativePosition = 77;
        if (!advanceHome(home, cancel, 2500) || home.outcome != ActionOutcome::CANCELLED || home.stagingApplied ||
            home.execution != ActionExecution::NOT_TRANSMITTED || getHomeReference(home, 2500, 1000, reference) ||
            reference.nativePosition != 77) return false;
    }
    return true;
}
} // namespace

int main() {
    MotorControlRS::ESS_RS::PersistenceContext persistence;
    MotorControlRS::ESS_RS::PersistencePrerequisites backup;
    if (MotorControlRS::ESS_RS::prepareSave(persistence, MotorControlRS::ReadTarget(), 1, backup, 100, 200)) return 99;
    using namespace MotorControlRS;
    using namespace MotorControlRS::ESS_RS;
    ReadTarget target; target.id = 1; target.address = 1; target.generation = 2;
    if (!scalarSettings(target)) return 112;
    if (!admittedSwitchHomes(target)) return 113;
    ReadContext velocityRead; PreparedRead velocityWork;
    if (!readCapabilities().velocityParameters ||
        !prepareVelocityParameters(velocityRead, target, 17, 3, 100, 1000) ||
        !nextRead(velocityRead, 100, velocityWork) || velocityWork.first != 0x001D ||
        velocityWork.count != 3 || velocityWork.length != 8) return 106;
    const uint8_t velocityReply[] = {1,3,6,0,0x3C,0,0x32,0,0x64,0xD1,0x54};
    ReadEvent velocityEvent; velocityEvent.target = target; velocityEvent.operationId = 17;
    velocityEvent.frame = velocityReply; velocityEvent.length = sizeof(velocityReply);
    velocityEvent.qualified = true; velocityEvent.earliestUs = 200; velocityEvent.latestUs = 220; velocityEvent.txAccepted = 8;
    VelocityParametersObservation velocityParameters;
    if (!advanceRead(velocityRead, velocityEvent, 230) || !getVelocityParameters(velocityRead, velocityParameters) ||
        velocityParameters.configurationGeneration != 3 || velocityParameters.rawSpeed != 60 ||
        velocityParameters.rawAccelerationTime != 50 || velocityParameters.rawDecelerationTime != 100) return 107;
    ReadContext homingRead; PreparedRead homingWork;
    if (!readCapabilities().homingParameters || !prepareHomingParameters(homingRead,target,18,3,100,1000) ||
        !nextRead(homingRead,100,homingWork) || homingWork.first != 0x0030 || homingWork.count != 7 ||
        homingWork.length != 8 || expectedReadRegistersLen(homingWork.count) != 19) return 108;
    const uint8_t homingReply[] = {1,3,14,0,7,0,24,0,60,0,30,0,100,0x12,0x34,0xAB,0xCD,0xEE,0x29};
    ReadEvent homingEvent; homingEvent.target = target; homingEvent.operationId = 18;
    homingEvent.frame = homingReply; homingEvent.length = sizeof(homingReply);
    homingEvent.qualified = true; homingEvent.earliestUs = 200; homingEvent.latestUs = 220; homingEvent.txAccepted = 8;
    HomingParametersObservation homingParameters;
    if (!advanceRead(homingRead,homingEvent,230) || !getHomingParameters(homingRead,homingParameters) ||
        homingParameters.rawAuxiliary != 7 || homingParameters.rawMethod != 24 ||
        homingParameters.rawOffsetWords[0] != 0x1234 || homingParameters.rawOffsetWords[1] != 0xABCD ||
        !homingParameters.methodKnown || homingParameters.methodSupport != HomeSupport::IMPLEMENTED ||
        homingParameters.offsetWordOrderResolution != ReadResolution::WORD_ORDER_UNRESOLVED ||
        homingParameters.configurationGeneration != 3) return 109;
    PositionCommand desired;
    desired.subdivision = 1000; desired.target = target; desired.speedRpm = 60;
    desired.accelerationTime = desired.decelerationTime = 100;
    MoveContext nativeMove; PreparedMove nativeWork;
    if (!desired.prepareRelative(nativeMove, 12, 100, 100, 1000000) ||
        !nextMove(nativeMove, 100, nativeWork) || nativeWork.length != 19 ||
        nativeMove.admission != MoveAdmission::NATIVE_INTENT) return 104;
    uint8_t rawStart[8];
    if (buildStartPosition(1, true, rawStart, sizeof(rawStart)) != 8) return 105;
    // A fixture-supplied active profile qualifies a native stored retrigger.
    // Production callers retain this independently of new command intent.
    desired.setup = MoveSetup::USE_STORED;
    desired.interruption = PositionInterruption::INTERRUPT_CURRENT;
    desired.configurationGeneration = 3;
    StoredPositionQualification& stored = desired.stored;
    stored.target = target; stored.configurationGeneration = 3;
    stored.originalOperationId = 12; stored.subdivision = 1000;
    stored.profile.startSpeed = 0; stored.profile.speed = 60;
    stored.profile.accelerationTime = stored.profile.decelerationTime = 100;
    stored.profile.targetBits = 100; stored.qualified = true;
    stored.qualifiedUs = 100; stored.maximumAgeUs = 1000;
    MoveContext replacement;
    if (!desired.prepareRelative(replacement, 19, 100, 100, 1000000) ||
        !nextMove(replacement, 100, nativeWork) || nativeWork.length != 8 ||
        nativeWork.value != 9 || nativeWork.deadlineUs != 1100 ||
        replacement.stored.originalOperationId != 12 || replacement.prepared.endpointKnown ||
        buildStartPosition(1, true, PositionInterruption::INTERRUPT_CURRENT, rawStart, sizeof(rawStart)) != 8)
        return 110;
    const uint8_t interruptAck[] = {1,6,0,0x27,0,9,0xF9,0xC7};
    ActionEvent interruptEvent; interruptEvent.transport.target = target;
    interruptEvent.transport.operationId = 19; interruptEvent.transport.step = nativeWork.step;
    interruptEvent.transport.frame = interruptAck; interruptEvent.transport.length = sizeof(interruptAck);
    interruptEvent.transport.txAccepted = 8; interruptEvent.txComplete = true;
    interruptEvent.transport.qualified = true; interruptEvent.responseConfirmed = true;
    interruptEvent.transport.earliestUs = 200; interruptEvent.transport.latestUs = 220;
    if (!advanceMove(replacement, interruptEvent, 230) || replacement.outcome != ActionOutcome::ACKNOWLEDGED ||
        replacement.completion != ActionCompletion::NOT_OBSERVED) return 111;
    DiscoveryCapabilities discovery;
    ESS_RS::PreparedProbe probe;
    if (discoveryProfileCount() != 1 || !getDiscoveryCapabilities(DriveProfile::ESS_RS, discovery) ||
        !discovery.nonChanging || !MotorControlRS::prepareProbe(probe, DriveProfile::ESS_RS, target, 1, 100, 1000) ||
        probe.length != 8) return 100;
    const uint8_t probeReply[] = {1, 3, 2, 3, 5, 0x78, 0xB7};
    ReadEvent probeEvent; probeEvent.target = target; probeEvent.operationId = 1;
    probeEvent.frame = probeReply; probeEvent.length = sizeof(probeReply); probeEvent.qualified = true;
    probeEvent.earliestUs = 200; probeEvent.latestUs = 220; probeEvent.txAccepted = 8;
    ESS_RS::ProbeObservation responder;
    if (!MotorControlRS::checkProbe(probe, probeEvent, 230, responder) || responder.outcome != ProbeOutcome::RESPONDER ||
        responder.rawModel != 0x0305 || responder.confidence != ProbeConfidence::RESPONDER_MODEL_UNRESOLVED) return 101;
    ReadContext operation;
    if (!prepareIdentity(operation, target, 3, 100, 1000)) return 1;
    PreparedRead request;
    if (!nextRead(operation, 100, request) || request.length != 8 || request.first != 0 || request.count != 4) return 2;
    const uint8_t response[] = {1, 3, 8, 0x4E, 0xEA, 0x12, 0x34, 0, 1, 0xA5, 0x81, 3, 0xE3};
    ReadEvent event; event.target = target; event.operationId = 3; event.step = 0;
    event.frame = response; event.length = sizeof(response); event.qualified = true;
    event.txAccepted = 8;
    event.earliestUs = 200; event.latestUs = 220;
    if (!advanceRead(operation, event, 230)) return 3;
    IdentityObservation observation;
    if (!getIdentity(operation, observation) || observation.rawModel != 0x4EEA || observation.rawVersion != 0x1234) return 4;
    if (observation.activeSerial.known || observation.modelResolution != ReadResolution::MODEL_MAPPING_UNRESOLVED) return 5;
    if (!readCapabilities().config || !prepareConfig(operation, target, 4, 300, 1000)) return 6;
    if (!nextRead(operation, 300, request) || request.first != 0x10 || request.count != 2) return 7;
    ReadEvent cancelled; cancelled.target = target; cancelled.operationId = 4; cancelled.kind = ReadEventKind::CANCEL;
    if (!advanceRead(operation, cancelled, 310) || operation.outcome != ReadOutcome::CANCELLED) return 8;
    if (!readCapabilities().state || !prepareState(operation, target, 5, 400, 2000)) return 9;
    if (!nextRead(operation, 400, request) || request.first != 6 || request.count != 2) return 10;
    // Decode a completed block while the multi-read operation is still active.
    // This proves the installed consumer has the actual public state API.
    uint8_t motion[] = {1, 3, 4, 0, 0, 0, 0x10, 0, 0};
    const uint16_t crc = calcCrc16(motion, sizeof(motion) - 2);
    motion[7] = static_cast<uint8_t>(crc); motion[8] = static_cast<uint8_t>(crc >> 8);
    event = ReadEvent(); event.target = target; event.operationId = 5;
    event.frame = motion; event.length = sizeof(motion); event.qualified = true;
    event.earliestUs = 500; event.latestUs = 520; event.txAccepted = 8;
    if (!advanceRead(operation, event, 530)) return 11;
    StateObservation state;
    if (!getStateBlock(operation, 0, state) || !state.released || state.enabled || !state.alarmKnown) return 12;
    if (getStateBlock(operation, 2, state) || state.block != StateBlock::MOTION) return 13;
    AxisConfig axis; axis.target = target; axis.supportedRelativeBases = 1;
    VelocityRequest velocity; velocity.configurationGeneration = axis.generation;
    velocity.value = Rational(-30);
    PreparedVelocityTarget speed;
    if (!prepareVelocityTarget(velocity, axis, speed) || speed.nativeRpm != -30) return 31;
    PositionRequest position; position.configurationGeneration = axis.generation;
    position.value = Rational(INT64_C(9007199254740993));
    PreparedTarget prepared;
    if (!preparePosition(position, axis, nullptr, prepared) ||
        prepared.effectiveNative != position.value.numerator || !prepared.exactArithmetic) return 14;
    Rational number;
    if (!parseExactNumber("-0.125", number) || number.numerator != -1 || number.denominator != 8) return 15;
    ActionContext action;
    StopPolicy policy; policy.behavior = StopBehavior::CONFIGURED_DECELERATION;
    if (!MotorControlRS::prepareStop(action, target, 6, policy, 1000, 100000)) return 16;
    PreparedAction work;
    if (!nextAction(action, 1000, work) || !work.write || work.reg != 0x27 || work.value != 0x100) return 17;
    ActionEvent evidence; evidence.transport.target = target; evidence.transport.operationId = 6;
    evidence.transport.frame = work.bytes; evidence.transport.length = work.length;
    evidence.transport.txAccepted = work.length; evidence.transport.qualified = true;
    evidence.transport.earliestUs = 1100; evidence.transport.latestUs = 1200;
    evidence.txComplete = true; evidence.responseConfirmed = true;
    if (!advanceAction(action, evidence, 1300) || action.execution != ActionExecution::ACKNOWLEDGED ||
        action.completion != ActionCompletion::NOT_OBSERVED) return 18;
    if (!nextAction(action, 1300, work) || work.kind != ActionWork::WAIT) return 19;
    evidence = ActionEvent(); evidence.transport.target = target; evidence.transport.operationId = 6;
    evidence.transport.step = action.step; evidence.transport.kind = ReadEventKind::CANCEL;
    if (!advanceAction(action, evidence, 1400) || action.outcome != ActionOutcome::CANCELLED ||
        action.execution != ActionExecution::ACKNOWLEDGED) return 20;
    MovePrerequisites prerequisites; prerequisites.target = target;
    prerequisites.subdivision = 1000; prerequisites.configurationGeneration = axis.generation;
    prerequisites.commandUnitsVerified = prerequisites.relativeBasisVerified = true;
    prerequisites.configuredRampVerified = prerequisites.serialInputsPermit = prerequisites.readinessQualified = true;
    prerequisites.wordOrderKnown = prerequisites.startSpeedKnown = true;
    prerequisites.accelerationTime = prerequisites.decelerationTime = 100;
    prerequisites.startSpeed = 10; prerequisites.observedUs = 1900; prerequisites.maximumAgeUs = 1000;
    MoveRequest move; move.position.configurationGeneration = axis.generation;
    move.position.value = Rational(20); move.speedRpm = 60; move.ramp = MoveRamp::VERIFIED_CONFIGURED;
    MoveContext moving;
    if (!MotorControlRS::prepareMoveRelative(moving, axis, nullptr, 7, move, prerequisites, 2000, 100000)) return 21;
    PreparedMove stage;
    if (!nextMove(moving, 2000, stage) || stage.length != 19 || stage.function != 16 || stage.count != 5) return 22;
    evidence = ActionEvent(); evidence.transport.target = target; evidence.transport.operationId = 7;
    evidence.transport.kind = ReadEventKind::CANCEL;
    if (!advanceMove(moving, evidence, 2100) || moving.outcome != ActionOutcome::CANCELLED || moving.uncertain) return 23;
    axis.originKnown = true; axis.originSource = ScaleSource::QUALIFIED;
    axis.units.commandStepsPerMotorTurn = UnitScale(1000, 1, ScaleSource::QUALIFIED);
    AxisReference reference; reference.target = target; reference.configurationGeneration = axis.generation;
    reference.nativeKnown = reference.stationary = reference.idle = true; reference.source = ScaleSource::QUALIFIED;
    reference.nativePosition = 1990; reference.observedUs = 1900; reference.maximumAgeUs = 1000;
    move.position.relative = false; move.position.unit = PositionUnit::DEGREES;
    move.position.frame = CoordinateFrame::MOTOR; move.position.value = Rational(720);
    if (!MotorControlRS::prepareMoveAbsolute(moving, axis, &reference, 8, move, prerequisites, 2000, 100000) ||
        moving.prepared.effectiveNative != 2000 || moving.prepared.displacementNative != 10) return 24;
    reference.nativePosition = 990; move.position.wrapped = true; move.position.value = Rational(0);
    move.position.path = AnglePath::POSITIVE;
    if (!MotorControlRS::prepareMoveAngle(moving, axis, &reference, 9, move, prerequisites, 2000, 100000) ||
        moving.prepared.effectiveNative != 1000) return 25;
    if (!MotorControlRS::prepareSetDevicePosition(action, target, 10, 0, true, 2000, 100000) ||
        !nextAction(action, 2000, work) || work.reg != 0x2D || work.value != 49) return 26;
    reference.idle = true; reference.nowUs = 2000;
    AxisConfig candidate = axis;
    candidate.units.settings.velocity = VelocityUnit(PositionUnit::TURNS, TimeUnit::MINUTE);
    if (!configureAxis(axis, candidate, reference, &reference) || !reference.nativeKnown ||
        reference.configurationGeneration != axis.generation || reference.observedUs != 1900 ||
        reference.maximumAgeUs != 1000) return 27;
    if (!invalidateAxisReference(axis, reference) || axis.originKnown || reference.nativeKnown) return 28;
    DriverContext driver;
    if (!prepareDriverRead(driver, target, 11, axis.generation, 2100, 100000)) return 32;
    PreparedDriver setting;
    if (!nextDriver(driver, 2100, setting) || setting.write || setting.reg != 0x10 || setting.count != 2) return 33;
    evidence = ActionEvent(); evidence.transport.target = target; evidence.transport.operationId = 11;
    evidence.transport.kind = ReadEventKind::CANCEL;
    if (!advanceDriver(driver, evidence, 2200) || driver.outcome != DriverOutcome::CANCELLED) return 34;
    HomeContext home;
    HomeRequest homeRequest; homeRequest.configurationGeneration = axis.generation;
    HomePrerequisites homePrerequisites; homePrerequisites.target = target;
    homePrerequisites.configurationGeneration = axis.generation;
    homePrerequisites.qualifiedMethod = homeRequest.method;
    homePrerequisites.qualifiedSearchSpeed = homeRequest.searchSpeed;
    homePrerequisites.qualifiedReturnSpeed = homeRequest.returnSpeed;
    homePrerequisites.qualifiedRampTime = homeRequest.rampTime;
    homePrerequisites.methodQualified = homePrerequisites.nativeRatesQualified = homePrerequisites.nativeRampQualified = true;
    homePrerequisites.zeroOffsetQualified = homePrerequisites.auxiliaryQualified = homePrerequisites.inputsQualified = true;
    homePrerequisites.readinessQualified = true; homePrerequisites.observedUs = 2300; homePrerequisites.maximumAgeUs = 10000;
    if (!MotorControlRS::prepareHome(home, axis, 12, homeRequest, homePrerequisites, 2400, 100000)) return 35;
    PreparedHome homeWork;
    if (!nextHome(home, 2400, homeWork) || homeWork.length != 21 || homeWork.reg != 0x31) return 36;
    if (!checkHomeQualification(homePrerequisites, 2400) || !revokeHome(home) ||
        !home.qualificationRevoked || nextHome(home, 2400, homeWork)) return 36;
    DriverRequest io; io.group = DriverGroup::IO;
    if (!prepareInputFunction(io, 0, InputFunction::UNDEFINED) || !prepareOutputFunction(io, 1, OutputFunction::UNDEFINED)) return 37;
    if (!prepareDriverRead(driver, target, 13, axis.generation, 2500, 100000, DriverGroup::IO) ||
        !nextDriver(driver, 2500, setting) || setting.reg != 0x40 || setting.count != 5) return 38;
    IoObservation ioObservation;
    if (getIo(driver, ioObservation)) return 39;
    if (!prepareSegmentRead(driver, target, 14, axis.generation, 2600, 100000, SegmentKind::POSITION, 16) ||
        !nextDriver(driver, 2600, setting) || setting.reg != 0xBA || setting.count != 5) return 40;
    SegmentObservation segment;
    if (getSegment(driver, segment)) return 41;
    if (!prepareControlRead(driver, target, 15, axis.generation, 2700, 100000) ||
        !nextDriver(driver, 2700, setting) || setting.reg != 0x100 || setting.count != 4) return 42;
    ControlObservation control;
    if (getControl(driver, control)) return 43;
    if (!prepareTuningRead(driver, target, 16, axis.generation, 2800, 100000, DriverGroup::LA) ||
        !nextDriver(driver, 2800, setting) || setting.reg != 0x112 || setting.count != 4) return 44;
    TuningObservation tuning;
    if (getTuning(driver, tuning)) return 45;
    if (driverWriteDeadline(driver) != 100000) return 47;
    DriverRequest tuningCandidate;
    if (!prepareTuningValue(tuningCandidate, laStageParameter(2, LaStageField::NODE), 65535) ||
        tuningCandidate.group != DriverGroup::LA || tuningCandidate.fields != 32 || tuningCandidate.tuningValues[5] != 65535) return 46;
    CommunicationContext commissioning; PreparedCommunication communicationWork;
    if (nextCommunication(commissioning, 0, communicationWork)) return 48;
    CommunicationRequest communicationRequest; CommunicationPrerequisites communicationPrerequisites;
    communicationRequest.field = CommunicationField::BAUD;
    if (prepareCommunication(commissioning, target, 17, communicationRequest, communicationPrerequisites, 3000, 100000)) return 49;
    return 0;
}
