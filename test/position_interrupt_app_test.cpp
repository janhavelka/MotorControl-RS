// SPDX-License-Identifier: MIT
// Real shared application/owner/runner with scripted SDK wire evidence.
// Reads establish every baseline; none of these fixtures qualifies hardware.
#include "../examples/probe_cli/ProbeApp.cpp"
#include "fakes/esp32_uart/UsbConsoleFixture.h"
#if MOTORCONTROLRS_TEST_IDF
#include "fakes/esp32_uart/FakeUsb.h"
#include "../examples/probe_idf/main/IdfPlatform.cpp"
#include "../examples/common/BoardPins.h"
#else
#include "../examples/probe_cli/ArduinoPlatform.cpp"
#include "../examples/probe_cli/main.cpp"
#endif
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

namespace {
using namespace MotorControlRS;
const std::vector<uint8_t> RELATIVE = {1,6,0,0x27,0,1,0xF8,0x01};
const std::vector<uint8_t> ABSOLUTE = {1,6,0,0x27,0,5,0xF9,0xC2};
const std::vector<uint8_t> INTERRUPT_RELATIVE = {1,6,0,0x27,0,9,0xF9,0xC7};
const std::vector<uint8_t> INTERRUPT_ABSOLUTE = {1,6,0,0x27,0,13,0xF8,0x04};
const std::vector<uint8_t> STOP = {1,6,0,0x27,1,0,0x38,0x51};
template<class T> struct Saved {
    unsigned char bytes[sizeof(T)];
    explicit Saved(const T& value) { std::memcpy(bytes, &value, sizeof(value)); }
    void unchanged(const T& value) const { assert(!std::memcmp(bytes, &value, sizeof(value))); }
};
void step(uint32_t us = 10) { advanceHardware(hardware.time + us); serviceApplication(); }
void pump(unsigned count = 64) { for (unsigned i = 0; i < count; ++i) step(); }
void fresh(uint32_t maximumAgeMs = 0) {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = writeResponseConfirmed = false;
    ApplicationOptions options; options.observationMaxAgeMs = maximumAgeMs;
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit, options));
    hardware.txCharacterUs = 87; assert(uart.startCapture(20, timing().holdUs));
    writeResponseConfirmed = true; // Supplied fake topology, never a bench observation.
}
std::vector<uint8_t> crc(std::vector<uint8_t> bytes) {
    uint16_t value = 0xFFFF;
    for (const uint8_t byte : bytes) {
        value ^= byte;
        for (unsigned bit = 0; bit < 8; ++bit) value = value & 1 ? (value >> 1) ^ 0xA001 : value >> 1;
    }
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8)); return bytes;
}
std::vector<uint8_t> words(std::initializer_list<uint16_t> values, uint8_t address = 1) {
    std::vector<uint8_t> bytes = {address,3,static_cast<uint8_t>(values.size()*2)};
    for (const uint16_t value : values) { bytes.push_back(static_cast<uint8_t>(value >> 8)); bytes.push_back(static_cast<uint8_t>(value)); }
    return crc(bytes);
}
Probe::ResultView view(uint32_t id) {
    Probe::ResultView result; assert(lookup(app,id,result)); return result;
}
void feed(const std::string& line) { for (const char c : line) app->console.feed(c); }
void command(const std::string& line) { Serial.output.clear(); feed(line); pump(300); }
void flushOutputOnly() {
    for (unsigned i=0; i<1000 && (app->outputCount || app->console.outputPending()); ++i) {
        app->console.serviceOutput(); drainOutput(*app);
    }
    assert(!app->outputCount && !app->console.outputPending());
}
void waitTx(uint32_t id, const std::vector<uint8_t>& expected = {}) {
    auto* record = findRecord(*app,id); assert(record);
    for (unsigned i=0; i<100000 && !app->owner.txAccepted(record->requestId) && view(id).pending; ++i) step();
    assert(app->owner.txAccepted(record->requestId));
    if (!expected.empty()) assert(hardware.tx == expected);
}
void schedule(const std::vector<uint8_t>& response) {
    scheduleReply(std::max(hardware.writeStarted + hardware.tx.size()*87 + 1000, hardware.time + 1000), response);
}
void terminalResult(uint32_t id) {
    for (unsigned i=0; i<100000 && view(id).pending; ++i) step();
    assert(!view(id).pending); pump(300);
}
void readReply(uint32_t id, const std::vector<uint8_t>& response) {
    const auto token = view(id).typedRead->step; waitTx(id); schedule(response);
    for (unsigned i=0; i<25000; ++i) {
        step(); const auto* record=findRecord(*app,id);
        if (!record || record->read.state!=ReadState::ACTIVE || record->read.step!=token) return;
    }
    assert(false);
}
void baseline(bool lowWordFirst = false) {
    command("@1 read config\n"); const uint32_t config = app->latestOperationId;
    readReply(config,words({0,1000})); readReply(config,words({0,0,0}));
    readReply(config,words({0,0,static_cast<uint16_t>(lowWordFirst ? 1 : 0)}));
    readReply(config,words({0,1,2,3,0})); readReply(config,words({3,4000}));
    pump(300); assert(app->configuration.operationId==config && releaseOperation(config));
    command("@2 read state\n"); const uint32_t state = app->latestOperationId;
    readReply(state,words({0,1})); readReply(state,words({0,0})); readReply(state,words({0,0,0}));
    pump(300); assert(releaseOperation(state));
    Probe::MotionProfileView profile;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,profile)==Probe::Action::OK);
    for (unsigned i=0; i<25000 && !app->owner.txAccepted(app->motionProfile.request); ++i) step();
    assert(app->owner.txAccepted(app->motionProfile.request));
    schedule(lowWordFirst ? words({10,100,100,60,20,0}) : words({10,100,100,60,0,20}));
    for (unsigned i=0; i<25000 && app->motionProfile.view.pending; ++i) step();
    assert(motionProfileBound(*app) && app->motionProfile.view.ok);
    assert(app->motionProfile.view.current[0]==10 && app->configuration.raw.subdivision==1000);
}
uint32_t original(bool relative = true) {
    uint32_t id = 0;
    assert(relative ? moveBy(Rational(20),PositionUnit::STEPS,id) : moveTo(Rational(20),PositionUnit::STEPS,id));
    assert(id && view(id).pending); return id;
}
void moveReply(uint32_t id, const std::vector<uint8_t>& response, const std::vector<uint8_t>& expected = {}) {
    const uint8_t token = view(id).moveContext->step; waitTx(id,expected); schedule(response);
    for (unsigned i=0; i<100000 && view(id).pending && view(id).moveContext->step==token; ++i) step();
    assert(!view(id).pending || view(id).moveContext->step!=token);
}
void startOriginal(uint32_t id, bool relative = true) {
    moveReply(id,crc({1,16,0,0x21,0,5}));
    moveReply(id,relative ? RELATIVE : ABSOLUTE,relative ? RELATIVE : ABSOLUTE);
    assert(view(id).pending && view(id).moveContext->execution==ActionExecution::ACKNOWLEDGED);
}
ESS::PositionCommand candidate(bool relative = true) {
    (void)relative;
    ESS::PositionCommand c; c.target=app->axis.target; c.configurationGeneration=app->axis.generation;
    c.subdivision=1000; c.wordOrder=app->configuration.wordOrder;
    c.accelerationTime=c.decelerationTime=100; c.speedRpm=60;
    c.setup=MoveSetup::USE_STORED; c.interruption=ESS::PositionInterruption::INTERRUPT_CURRENT;
    // This intentionally provides no qualification: the ordinary application
    // must obtain its independent original-operation witness itself.
    return c;
}
uint32_t interrupt(uint32_t old, bool relative = true) {
    uint32_t id=0; assert(submitPositionInterrupt(old,candidate(relative),20,relative,id));
    assert(id && id!=old && view(id).pending); return id;
}
ESS::MoveContext copied(uint32_t id, MoveRelation& relation) {
    ESS::MoveContext c; assert(copyMoveResult(id,c,relation)); return c;
}
uint32_t stop() {
    ActionRequest request; request.kind=ActionKind::STOP; request.stop.behavior=StopBehavior::CONFIGURED_DECELERATION;
    uint32_t id=0; assert(startAction(app,80,1,request,id)==Probe::Action::OK); return id;
}
void actionReply(uint32_t id, const std::vector<uint8_t>& response, const std::vector<uint8_t>& expected={}) {
    const uint8_t token=view(id).actionContext->step; waitTx(id,expected); schedule(response);
    for (unsigned i=0; i<100000 && view(id).pending && view(id).actionContext->step==token; ++i) step();
    assert(!view(id).pending || view(id).actionContext->step!=token);
}
void stopped(uint32_t id) { actionReply(id,STOP,STOP); actionReply(id,words({0,1})); assert(view(id).actionContext->completion==ActionCompletion::OBSERVED); }
uint32_t retainedProbe(uint32_t correlation) {
    command("@"+std::to_string(correlation)+" probe 2\n"); const uint32_t id=app->latestOperationId;
    waitTx(id); schedule(words({0x4EEA},2)); terminalResult(id);
    assert(view(id).probe.outcome==Rtu::Outcome::SUCCESS); return id;
}

void testProductionPathAndCopiedResults() {
    for (bool relative : {false,true}) for (bool lowOrder : {false,true}) {
        fresh(); baseline(lowOrder); const auto old=original(relative); startOriginal(old,relative);
        const auto setup=view(old).moveContext->stagingEvidence;
        const auto trigger=view(old).moveContext->triggerEvidence;
        const auto writes=hardware.writes; const auto id=interrupt(old,relative);
        assert(hardware.writes==writes && !findRecord(*app,id)->requestId.owner);
        MoveRelation before; const auto admitted=copied(id,before);
        assert(before.originalOperationId==old && before.replacementOperationId==id && !before.observationSuperseded);
        assert(admitted.stored.profile.startSpeed==10 && admitted.stored.profile.targetBits==20 && admitted.stored.qualified);
        const auto expected=relative ? INTERRUPT_RELATIVE : INTERRUPT_ABSOLUTE;
        waitTx(id,expected);
        assert(!view(old).pending && view(old).moveContext->outcome==ActionOutcome::CANCELLED);
        assert(!releaseMove(old)); // The live peer still needs this exact result.
        moveReply(id,expected,expected); terminalResult(id);
        MoveRelation relation; const auto result=copied(id,relation);
        assert(relation.originalOperationId==old && relation.replacementOperationId==id && relation.observationSuperseded);
        assert(result.admission==ESS::MoveAdmission::NATIVE_INTENT && result.outcome==ActionOutcome::ACKNOWLEDGED);
        assert(result.execution==ActionExecution::ACKNOWLEDGED && result.completion==ActionCompletion::NOT_OBSERVED);
        assert(!result.prepared.endpointKnown && !result.runningObserved && !result.observationKnown);
        assert(hardware.writes==writes+1 && axisReserved(*app,1) && triggeredMotion(*app,1));
        MoveRelation oldRelation; const auto previous=copied(old,oldRelation);
        assert(previous.uncertain && previous.setupExecution==ActionExecution::ACKNOWLEDGED && previous.execution==ActionExecution::ACKNOWLEDGED);
        assert(!std::memcmp(previous.stagingEvidence.raw,setup.raw,setup.length));
        assert(!std::memcmp(previous.triggerEvidence.raw,trigger.raw,trigger.length));
        assert(oldRelation.originalOperationId==old && oldRelation.replacementOperationId==id && oldRelation.observationSuperseded);
        MoveProgress progress; assert(moveProgress(id,progress) && !progress.pending && progress.completion==ActionCompletion::NOT_OBSERVED);
        assert(releaseMove(old) && releaseMove(id) && axisReserved(*app,1));
        uint32_t untouched=77; assert(!moveBy(Rational(20),PositionUnit::STEPS,untouched) && untouched==77);
        stopped(stop()); assert(!axisReserved(*app,1) && !triggeredMotion(*app,1));
        ESS::MoveContext missing; missing.operationId=999; MoveRelation missingRelation; missingRelation.originalOperationId=777;
        const Saved<ESS::MoveContext> contextBefore(missing); const Saved<MoveRelation> relationBefore(missingRelation);
        assert(!copyMoveResult(id,missing,missingRelation)); contextBefore.unchanged(missing); relationBefore.unchanged(missingRelation);
    }
}

void testRejectedCandidatesAndPhases() {
    fresh(); baseline(); const auto old=original();
    uint32_t untouched=77; const auto intended=candidate();
    assert(!submitPositionInterrupt(old,intended,20,true,untouched) && untouched==77);
    moveReply(old,crc({1,16,0,0x21,0,5}));
    assert(!submitPositionInterrupt(old,intended,20,true,untouched) && untouched==77);
    waitTx(old,RELATIVE);
    assert(!submitPositionInterrupt(old,intended,20,true,untouched) && untouched==77);
    moveReply(old,RELATIVE,RELATIVE);
    for (unsigned bad=0; bad<17; ++bad) {
        auto request=candidate(); uint32_t bits=20, originalId=old; bool relative=true;
        switch (bad) {
        case 0: request.setup=MoveSetup::WRITE_ALL; break;
        case 1: request.setup=MoveSetup::VERIFY_AND_UPDATE; break;
        case 2: request.interruption=ESS::PositionInterruption::IGNORE; break;
        case 3: request.interruption=static_cast<ESS::PositionInterruption>(255); break;
        case 4: ++request.target.id; break;
        case 5: ++request.target.address; break;
        case 6: ++request.target.generation; break;
        case 7: ++request.configurationGeneration; break;
        case 8: ++request.subdivision; break;
        case 9: request.wordOrder=ESS::WordOrder::LOW_WORD_FIRST; break;
        case 10: ++request.accelerationTime; break;
        case 11: ++request.decelerationTime; break;
        case 12: ++request.speedRpm; break;
        case 13: ++bits; break;
        case 14: relative=false; break;
        case 15: originalId=0; break;
        case 16: ++request.limits.maximumRpm; break;
        }
        const auto beforeId=app->nextOperationId, writes=hardware.writes;
        const Saved<ESS::MoveContext> before(*view(old).moveContext);
        assert(!submitPositionInterrupt(originalId,request,bits,relative,untouched) && untouched==77);
        before.unchanged(*view(old).moveContext);
        assert(app->nextOperationId==beforeId && hardware.writes==writes && view(old).pending);
    }
    const auto replacement=interrupt(old); const auto next=app->nextOperationId;
    assert(!submitPositionInterrupt(old,candidate(),20,true,untouched) && untouched==77 && app->nextOperationId==next);
    assert(cancel(app,replacement)==Probe::Action::OK); pump();
    assert(!view(replacement).pending && !view(replacement).moveContext->triggerEvidence.txAccepted);
}

void testInflightSettlementAndCompletionRace() {
    for (bool completes : {false,true}) for (bool reverseSlots : {false,true}) {
        fresh(); baseline(); uint32_t hole=0;
        if (reverseSlots) hole=retainedProbe(31);
        const auto old=original(); startOriginal(old);
        if (reverseSlots) assert(releaseOperation(hole));
        moveReply(old,words({0,4})); assert(view(old).moveContext->runningObserved);
        waitTx(old); assert(hardware.tx[1]==3);
        const auto writes=hardware.writes; const auto replacement=interrupt(old);
        if (reverseSlots) assert(findRecord(*app,replacement)<findRecord(*app,old));
        assert(!findRecord(*app,replacement)->requestId.owner && hardware.writes==writes);
        const uint64_t oldDeadline=view(old).moveContext->deadlineUs;
        schedule(words({0,static_cast<uint16_t>(completes ? 1 : 4)}));
        if (completes) {
            terminalResult(replacement);
            assert(view(old).moveContext->outcome==ActionOutcome::OBSERVED && view(old).moveContext->completion==ActionCompletion::OBSERVED);
            assert(view(replacement).moveContext->outcome==ActionOutcome::CANCELLED && !view(replacement).moveContext->triggerEvidence.txAccepted);
            assert(hardware.writes==writes && !axisReserved(*app,1));
            MoveRelation relation; copied(replacement,relation); assert(!relation.observationSuperseded);
        } else {
            waitTx(replacement,INTERRUPT_RELATIVE);
            assert(!view(old).pending && view(old).moveContext->lastObservation.raw[6]==4);
            moveReply(replacement,INTERRUPT_RELATIVE,INTERRUPT_RELATIVE);
            assert(view(replacement).moveContext->outcome==ActionOutcome::ACKNOWLEDGED && hardware.writes==writes+1);
        }
        assert(view(old).moveContext->deadlineUs==oldDeadline);
    }
}

void testPressureAndReservedStop() {
    fresh(); baseline(); uint32_t retained[7];
    for (unsigned i=0; i<7; ++i) retained[i]=retainedProbe(30+i);
    const auto old=original(); startOriginal(old);
    const Saved<ESS::MoveContext> before(*view(old).moveContext); const auto next=app->nextOperationId;
    uint32_t untouched=77; const auto refused=submitPositionInterrupt(old,candidate(),20,true,untouched);
    assert(!refused && refused.detail==static_cast<int32_t>(Probe::Action::RESULTS_FULL));
    assert(untouched==77 && app->nextOperationId==next); before.unchanged(*view(old).moveContext);
    const auto stopping=stop(); stopped(stopping);
    assert(!axisReserved(*app,1) && view(old).moveContext->outcome==ActionOutcome::CANCELLED);
    for (const auto id : retained) assert(view(id).probe.outcome==Rtu::Outcome::SUCCESS);
}

void testStopAtReplacementBoundaries() {
    for (unsigned phase=0; phase<4; ++phase) {
        fresh(); baseline(); const auto old=original(); startOriginal(old);
        if (!phase) waitTx(old);
        const auto replacement=interrupt(old);
        if (phase==1) {
            for (unsigned i=0; i<1000 && !findRecord(*app,replacement)->requestId.owner; ++i) step();
            assert(findRecord(*app,replacement)->requestId.owner && !app->owner.txAccepted(findRecord(*app,replacement)->requestId));
        }
        if (phase>=2) waitTx(replacement,INTERRUPT_RELATIVE);
        if (phase==3) moveReply(replacement,INTERRUPT_RELATIVE,INTERRUPT_RELATIVE);
        const auto writes=hardware.writes; const auto stopping=stop();
        if (!phase) schedule(words({0,4}));
        if (phase==2) schedule(INTERRUPT_RELATIVE);
        stopped(stopping); terminalResult(replacement);
        assert(!view(old).pending && !view(replacement).pending && !axisReserved(*app,1));
        const auto& result=*view(replacement).moveContext;
        if (phase<2) assert(result.outcome==ActionOutcome::CANCELLED && result.execution==ActionExecution::NOT_TRANSMITTED);
        else assert(result.execution==ActionExecution::ACKNOWLEDGED && result.completion==ActionCompletion::NOT_OBSERVED);
        assert(hardware.writes==writes+2); // Stop write/read only; never another interrupt.
        assert(view(stopping).actionContext->completion==ActionCompletion::OBSERVED);
        MoveRelation relation; copied(replacement,relation); assert(relation.originalOperationId==old);
        assert(relation.observationSuperseded==(phase!=0));
    }
    fresh(); baseline(); const auto old=original(); startOriginal(old); const auto stopping=stop();
    uint32_t untouched=77; assert(!submitPositionInterrupt(old,candidate(),20,true,untouched) && untouched==77);
    stopped(stopping);
}

void testOriginalFailureConservation() {
    for (unsigned phase=0; phase<4; ++phase) {
        fresh(); baseline(); const auto old=original();
        if (phase) moveReply(old,crc({1,16,0,0x21,0,5}));
        if (phase>=2) moveReply(old,RELATIVE,RELATIVE);
        waitTx(old); const auto writes=hardware.writes;
        uint32_t replacement=0;
        if (phase<2) {
            uint32_t unchanged=77;
            assert(!submitPositionInterrupt(old,candidate(),20,true,unchanged) && unchanged==77);
        } else {
            replacement=interrupt(old);
            if (phase==3) { auto bad=words({0,4}); bad.back()^=1; schedule(bad); }
        }
        // Lost staging/trigger ACK, or failed in-flight original observation.
        // No candidate operation can hide or supersede this old uncertainty.
        terminalResult(old);
        if (replacement) {
            terminalResult(replacement);
            assert(view(replacement).moveContext->outcome==ActionOutcome::CANCELLED);
            assert(!view(replacement).moveContext->triggerEvidence.txAccepted);
            MoveRelation relation; copied(replacement,relation); assert(!relation.observationSuperseded);
        }
        const auto& result=*view(old).moveContext;
        assert(result.uncertain && result.completion==ActionCompletion::NOT_OBSERVED && axisReserved(*app,1));
        if (!phase) assert(result.setupExecution==ActionExecution::UNKNOWN && result.execution==ActionExecution::NOT_TRANSMITTED);
        else if (phase==1) assert(result.setupExecution==ActionExecution::ACKNOWLEDGED && result.execution==ActionExecution::UNKNOWN);
        else assert(result.setupExecution==ActionExecution::ACKNOWLEDGED && result.execution==ActionExecution::ACKNOWLEDGED);
        uint32_t unchanged=77;
        assert(!submitPositionInterrupt(old,candidate(),20,true,unchanged) && unchanged==77);
        pump(1000); assert(hardware.writes==writes);
    }
}

void testWitnessAgeAndImmutableDeadline() {
    for (unsigned boundary=0; boundary<3; ++boundary) {
        fresh(10000); baseline(); const auto old=original(); startOriginal(old);
        const auto witness=findRecord(*app,old)->storedPosition;
        assert(witness.maximumAgeUs==10000000 && witness.qualified);
        const uint64_t expires=witness.qualifiedUs+witness.maximumAgeUs;
        const auto writes=hardware.writes;
        advanceHardware(expires+(boundary==2 ? 1 : 0)-(boundary==0 ? 1 : 0));
        const uint64_t admittedUs=hardware.time;
        uint32_t replacement=77;
        const auto admitted=submitPositionInterrupt(old,candidate(),20,true,replacement);
        if (boundary) {
            assert(!admitted && replacement==77 && view(old).pending);
            assert(!findRecord(*app,old)->cancelContinuation);
        } else {
            assert(admitted); const auto deadline=view(replacement).moveContext->deadlineUs;
            assert(findRecord(*app,replacement)->deadlineUs==admittedUs+REQUEST_US && deadline==admittedUs+REQUEST_US);
            advanceHardware(std::max(hardware.time,expires)); terminalResult(replacement);
            assert(!view(replacement).moveContext->triggerEvidence.txAccepted);
            assert(view(replacement).moveContext->deadlineUs==deadline);
        }
        assert(hardware.writes==writes);
    }
    fresh(); baseline(); const auto old=original(); startOriginal(old); const auto replacement=interrupt(old);
    const auto deadline=view(replacement).moveContext->deadlineUs, oldDeadline=view(old).moveContext->deadlineUs;
    const auto writes=hardware.writes;
    advanceHardware(deadline); terminalResult(replacement);
    assert(view(replacement).moveContext->outcome==ActionOutcome::DEADLINE);
    assert(!view(replacement).moveContext->triggerEvidence.txAccepted && hardware.writes==writes);
    assert(view(replacement).moveContext->deadlineUs==deadline && view(old).moveContext->deadlineUs==oldDeadline);
}

void testUnsupportedOriginsAndResultKinds() {
    for (unsigned mode=0; mode<4; ++mode) {
        fresh(); baseline();
        PositionRequest request; request.value=Rational(20); request.configurationGeneration=app->axis.generation;
        if (mode==0) { request.unit=PositionUnit::TURNS; request.frame=CoordinateFrame::MOTOR; request.value=Rational(1,50); }
        if (mode>=1) {
            // Explicit supplied coordinate convention, independent of raw drive
            // feedback. The original consumes it; replacement cannot reuse it.
            app->axis.originKnown=true; app->axis.originSource=ScaleSource::QUALIFIED;
            app->axis.units.commandStepsPerMotorTurn=UnitScale(1000,1,ScaleSource::QUALIFIED);
            app->axis.units.fullStepsPerMotorTurn=UnitScale(200,1,ScaleSource::DOCUMENTED);
            app->axis.units.motorTurnsPerLoadTurn=UnitScale(1,1,ScaleSource::ASSUMED);
            auto& reference=app->coordinateReference;
            reference.target=app->axis.target; reference.configurationGeneration=app->axis.generation;
            reference.idle=reference.stationary=reference.nativeKnown=true;
            reference.source=ScaleSource::QUALIFIED; reference.observedUs=hardware.time;
            if (mode==2) {
                app->axis.softLimitsKnown=true; app->axis.softMinimum=-100; app->axis.softMaximum=100;
            }
            if (mode==3) {
                request.relative=false; request.wrapped=true; request.unit=PositionUnit::DEGREES;
                request.frame=CoordinateFrame::MOTOR; request.value=Rational(36,5);
            }
        }
        const auto beforeSubmit=hardware.writes;
        uint32_t old=0; const auto submitted=submitMove(request,60,old);
        if (mode==3) { assert(!submitted && !old && hardware.writes==beforeSubmit); continue; }
        assert(submitted); startOriginal(old,request.relative);
        const Saved<ESS::MoveContext> before(*view(old).moveContext); const auto writes=hardware.writes;
        uint32_t unchanged=77; assert(!submitPositionInterrupt(old,candidate(),20,request.relative,unchanged) && unchanged==77);
        before.unchanged(*view(old).moveContext); assert(hardware.writes==writes && !findRecord(*app,old)->cancelContinuation);
    }
    fresh(); baseline(); command("@70 moveby 20 steps\n");
    const auto parent=app->simple.view.operationId; assert(parent && app->simple.view.pending);
    for (unsigned i=0; i<1000 && !app->simple.view.readOperationId; ++i) step();
    const auto state=app->simple.view.readOperationId; assert(state);
    readReply(state,words({0,1})); readReply(state,words({0,0})); readReply(state,words({0,0,0}));
    for (unsigned i=0; i<1000 && !app->simple.view.moveOperationId && !app->motionProfile.view.pending; ++i) step();
    if (app->motionProfile.view.pending) {
        for (unsigned i=0; i<25000 && !app->owner.txAccepted(app->motionProfile.request); ++i) step();
        assert(app->owner.txAccepted(app->motionProfile.request)); schedule(words({10,100,100,60,0,20}));
        for (unsigned i=0; i<25000 && app->motionProfile.view.pending; ++i) step();
        assert(app->motionProfile.view.ok && !app->motionProfile.view.pending);
    }
    for (unsigned i=0; i<1000 && !app->simple.view.moveOperationId; ++i) step();
    const auto child=app->simple.view.moveOperationId; assert(child && findRecord(*app,child)->simpleChild);
    startOriginal(child);
    for (const auto originalId : {parent,child}) {
        uint32_t unchanged=77; const auto writes=hardware.writes;
        assert(startPositionInterrupt(app,71,originalId,unchanged)!=Probe::Action::OK && unchanged==77);
        assert(hardware.writes==writes && view(child).pending && !findRecord(*app,child)->cancelContinuation);
    }
    fresh(); baseline(); uint32_t read=0; assert(submitVelocityParameterRead(read));
    ESS::MoveContext context; context.operationId=93;
    MoveRelation relation; relation.originalOperationId=94; relation.replacementOperationId=95; relation.observationSuperseded=true;
    const Saved<ESS::MoveContext> originalContext(context); const Saved<MoveRelation> originalRelation(relation);
    MoveProgress progress; progress.effectiveNative=96; const Saved<MoveProgress> originalProgress(progress);
    for (const auto id : {uint32_t(0),uint32_t(0xFFFFFFFF),read}) {
        assert(!copyMoveResult(id,context,relation) && !moveProgress(id,progress) && !releaseMove(id));
        originalContext.unchanged(context); originalRelation.unchanged(relation); originalProgress.unchanged(progress);
    }
}

void testCancellationAndUnknownReplies() {
    for (bool cancelOld : {false,true}) {
        fresh(); baseline(); const auto old=original(); startOriginal(old); waitTx(old);
        const auto replacement=interrupt(old), writes=hardware.writes;
        assert(cancel(app,cancelOld ? old : replacement)==Probe::Action::OK);
        schedule(words({0,4})); terminalResult(replacement);
        assert(view(replacement).moveContext->execution==ActionExecution::NOT_TRANSMITTED && hardware.writes==writes);
        assert(axisReserved(*app,1));
    }
    for (unsigned failure=0; failure<4; ++failure) {
        fresh(); baseline(); const auto old=original(); startOriginal(old); const auto replacement=interrupt(old);
        waitTx(replacement,INTERRUPT_RELATIVE); const auto writes=hardware.writes;
        const auto oldTrigger=view(old).moveContext->triggerEvidence;
        if (failure==0) schedule(RELATIVE); // Late old noninterrupting echo is not the new ACK.
        if (failure==1) { auto malformed=INTERRUPT_RELATIVE; malformed.back()^=1; schedule(malformed); }
        if (failure==2) { writeResponseConfirmed=false; schedule(INTERRUPT_RELATIVE); }
        // failure3: lost ACK. No implicit replay/recovery is permitted.
        terminalResult(replacement);
        assert(view(replacement).moveContext->execution==ActionExecution::UNKNOWN && view(replacement).moveContext->uncertain);
        assert(view(replacement).moveContext->completion==ActionCompletion::NOT_OBSERVED && axisReserved(*app,1));
        assert(!std::memcmp(view(old).moveContext->triggerEvidence.raw,oldTrigger.raw,oldTrigger.length));
        pump(1000); assert(hardware.writes==writes);
    }
}

void testInvalidationBeforeDispatch() {
    for (unsigned changed=0; changed<4; ++changed) {
        fresh(); baseline(); const auto old=original(); startOriginal(old); const auto replacement=interrupt(old);
        for (unsigned i=0; i<1000 && !findRecord(*app,replacement)->requestId.owner; ++i) step();
        assert(findRecord(*app,replacement)->requestId.owner && !app->owner.txAccepted(findRecord(*app,replacement)->requestId));
        const auto writes=hardware.writes;
        if (changed==0) ++app->axis.generation;
        if (changed==1) ++app->bindingGeneration;
        if (changed==2) ++app->serial.generation;
        if (changed==3) {
            auto& io=app->stateCache.blocks[1]; io.value.rawInputs=1; io.value.inputs[0]=true;
            io.observedEarliestUs=io.observedLatestUs=hardware.time; io.invalidatedUs=0;
        }
        terminalResult(replacement);
        assert(!view(replacement).moveContext->triggerEvidence.txAccepted && hardware.writes==writes);
        assert(axisReserved(*app,1));
    }
}

void testOwnerResultPressureKeepsAdmissionGuard() {
    fresh(); baseline(); const auto old=original(); startOriginal(old);
    assert(!findRecord(*app,old)->requestId.owner);
    Rtu::RequestId retained[8];
    const auto bytes=crc({2,3,0,0,0,1});
    // An independent producer retains all ordinary owner completions while the
    // application's original sequence is in WAIT. Only normal owner/port work
    // runs here; no operation outcome or accepted byte count is synthesized.
    for (auto& id : retained) {
        Rtu::BusRequest request;
        request.expected.address=2; request.expected.target=2; request.expected.targetGeneration=app->bindingGeneration;
        request.expected.function=3; request.expected.count=1; request.validator=Rtu::essValidator();
        request.wire.bytes=bytes.data(); request.wire.length=bytes.size(); request.wire.replyLength=7;
        request.wire.responseTimeoutUs=app->serial.timing.responseTimeoutUs;
        request.wire.replyGapUs=app->serial.timing.replyGapUs; request.wire.deadlineUs=nowUs()+REQUEST_US;
        assert(app->owner.admit(request,nowUs(),id)==Rtu::BusAdmission::ACCEPTED);
        bool replied=false;
        for (unsigned i=0; i<25000 && !app->owner.result(id); ++i) {
            advanceHardware(hardware.time+10); app->owner.service(uart.sample());
            if (!replied && app->owner.txAccepted(id)) { schedule(words({0x4EEA},2)); replied=true; }
        }
        assert(app->owner.result(id) && app->owner.result(id)->outcome==Rtu::Outcome::SUCCESS);
    }
    const auto replacement=interrupt(old); pump(10);
    assert(!view(old).pending && view(replacement).pending && !findRecord(*app,replacement)->requestId.owner);
    const auto writes=hardware.writes;
    auto& io=app->stateCache.blocks[1]; io.value.rawInputs=1; io.value.inputs[0]=true;
    io.observedEarliestUs=io.observedLatestUs=hardware.time; io.invalidatedUs=0;
    assert(app->owner.release(retained[0]));
    terminalResult(replacement);
    assert(!view(replacement).moveContext->triggerEvidence.txAccepted && hardware.writes==writes);
    for (unsigned i=1; i<8; ++i) assert(app->owner.release(retained[i]));
}

Rtu::WriteResult (*normalWrite)(void*,const uint8_t*,std::size_t)=nullptr;
bool shortInterrupt=false;
Rtu::WriteResult shortWrite(void* context, const uint8_t* bytes, std::size_t length) {
    return normalWrite(context,bytes,shortInterrupt && length==8 && bytes[1]==6 && bytes[3]==0x27 && bytes[5]==9 ? 3 : length);
}
void testPartialEnqueueAndCancelDuringTx() {
    for (bool partial : {false,true}) {
        fresh();
        // The SDK adapter normally enqueues a whole short frame. Inject only
        // a short port enqueue to exercise the real runner/owner conservation
        // boundary; all physical TX/capture/DE behavior remains the SDK fake.
        auto port=uart.port(); normalWrite=port.write; shortInterrupt=false; port.write=shortWrite;
        app->runner.~Runner(); new (&app->runner) Rtu::Runner(port,storage(app->tx,app->rx,app->trace),timing());
        baseline(); const auto old=original(); startOriginal(old); shortInterrupt=partial;
        const auto replacement=interrupt(old); waitTx(replacement);
        const auto writes=hardware.writes;
        assert(hardware.tx.size()==(partial ? 3u : 8u));
        const uint64_t physicalEnd=hardware.writeStarted+hardware.tx.size()*87;
        if (!partial) assert(cancel(app,replacement)==Probe::Action::OK);
        terminalResult(replacement);
        const auto& result=*view(replacement).moveContext;
        assert(result.triggerEvidence.txAccepted==(partial ? 3u : 8u) && result.execution==ActionExecution::UNKNOWN && result.uncertain);
        assert(result.completion==ActionCompletion::NOT_OBSERVED && axisReserved(*app,1));
        assert(hardware.deReleasedAt>=physicalEnd);
        pump(1000); assert(hardware.writes==writes);
    }
}

void testConsoleRouteAndRetention(bool emitResult = false) {
    fresh(); baseline();
    command("@37 help position\n");
    assert(Serial.output.find("position interrupt <original-id>")!=std::string::npos);
    command("@38 caps\n");
    assert(Serial.output.find("\"position_interrupt_stored\":true")!=std::string::npos);
    assert(Serial.output.find("\"position_interrupt_completion\":\"not_observed\"")!=std::string::npos);
    assert(Serial.output.find("\"position_interrupt_new_target\":false")!=std::string::npos);
    feed("@40 move relative 20 steps native 60 configured\n"); const auto old=app->latestOperationId;
    startOriginal(old); const auto before=hardware.writes, next=app->nextOperationId;
    for (const char* bad : {"@41 position interrupt\n","@42 position interrupt 0\n","@43 position interrupt -1\n",
                           "@44 position interrupt 1 extra\n","@45 position interrupt 1.5\n","@46 position other 1\n"}) {
        flushOutputOnly(); Serial.output.clear(); feed(bad); flushOutputOnly();
        assert(app->nextOperationId==next && Serial.output.find("\"ok\":false")!=std::string::npos);
    }
    // Do not let parser-negative checks generate an unacknowledged status poll.
    assert(hardware.writes==before);
    feed("@47 position interrupt "+std::to_string(old)+"\n"); const auto replacement=app->latestOperationId;
    assert(replacement!=old && findRecord(*app,replacement));
    moveReply(replacement,INTERRUPT_RELATIVE,INTERRUPT_RELATIVE); pump(1000);
    assert(Serial.output.find("\"completion\":\"not_observed\"")!=std::string::npos);
    assert(Serial.output.find("\"outcome\":\"acknowledged\"")!=std::string::npos);
    command("@48 result "+std::to_string(replacement)+"\n");
    assert(Serial.output.find("\"original_operation_id\":"+std::to_string(old))!=std::string::npos);
    assert(Serial.output.find("\"replacement_operation_id\":"+std::to_string(replacement))!=std::string::npos);
    if (emitResult) std::fwrite(Serial.output.data(),1,Serial.output.size(),stdout);
    command("@49 result "+std::to_string(old)+"\n");
    assert(Serial.output.find("\"replacement_operation_id\":"+std::to_string(replacement))!=std::string::npos);
    ESS::MoveContext c; MoveRelation relation; assert(!copyMoveResult(old,c,relation) && !copyMoveResult(replacement,c,relation));
    findRecord(*app,old)->interactive=true; findRecord(*app,replacement)->interactive=true;
    pump(1500); recycleInteractiveResults(app);
    assert(findRecord(*app,old) && findRecord(*app,replacement));
    assert(axisReserved(*app,1));
}
} // namespace
int main(int argc, char** argv) {
    // Host-parser parity consumes the actual production formatter after the
    // same checked reads, original trigger and replacement ACK used above.
    if (argc==2 && !std::strcmp(argv[1],"--emit-console-result")) {
        testConsoleRouteAndRetention(true); return 0;
    }
    assert(argc==1);
    testProductionPathAndCopiedResults(); testRejectedCandidatesAndPhases();
    testInflightSettlementAndCompletionRace(); testPressureAndReservedStop();
    testStopAtReplacementBoundaries(); testCancellationAndUnknownReplies();
    testOriginalFailureConservation(); testWitnessAgeAndImmutableDeadline(); testUnsupportedOriginsAndResultKinds();
    testInvalidationBeforeDispatch(); testOwnerResultPressureKeepsAdmissionGuard();
    testPartialEnqueueAndCancelDuringTx(); testConsoleRouteAndRetention();
}
