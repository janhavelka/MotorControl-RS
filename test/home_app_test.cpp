// SPDX-License-Identifier: MIT
// Actual cooperative application; all qualifications are simulated fixtures.
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
#include <vector>
namespace {
using namespace MotorControlRS;
void fresh() {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = writeResponseConfirmed = false;
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit));
    assert(app && !hardware.writes);
    hardware.txCharacterUs = 87; assert(uart.startCapture(20, timing().holdUs));
}
void step() { advanceHardware(hardware.time + 10); serviceApplication(); }
void pump(unsigned n = 1000) { while (n--) step(); }
Probe::ResultView view(uint32_t id) { Probe::ResultView v; assert(host(app).result(app, id, v)); return v; }
std::vector<uint8_t> crc(std::vector<uint8_t> v) {
    const auto c = ESS::calcCrc16(v.data(), v.size()); v.push_back(static_cast<uint8_t>(c)); v.push_back(static_cast<uint8_t>(c >> 8)); return v;
}
std::vector<uint8_t> words(uint16_t a, uint16_t b) { return crc({1,3,4,static_cast<uint8_t>(a >> 8),static_cast<uint8_t>(a),static_cast<uint8_t>(b >> 8),static_cast<uint8_t>(b)}); }
void waitTx(uint32_t id) {
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(findRecord(*app,id)->requestId); ++i) step();
    assert(app->owner.txAccepted(findRecord(*app,id)->requestId));
}
void reply(uint32_t id, const std::vector<uint8_t>& given = {}) {
    const auto initial = view(id); const auto token = initial.homeContext ? initial.homeContext->step : initial.actionContext->step;
    waitTx(id);
    const auto response = given.empty() ? crc(std::vector<uint8_t>(hardware.tx.begin(), hardware.tx.begin()+6)) : given;
    scheduleReply(std::max(hardware.writeStarted + hardware.tx.size()*87 + 1000, hardware.time + 1000), response);
    for (unsigned i = 0; i < 25000 && view(id).pending; ++i) {
        const auto current = view(id); if ((current.homeContext ? current.homeContext->step : current.actionContext->step) != token) break; step();
    }
    const auto result = view(id);
    assert(!result.pending || (result.homeContext ? result.homeContext->step : result.actionContext->step) != token);
}
ESS::HomeRequest request(int method = 35) {
    ESS::HomeRequest r; r.method = static_cast<ESS::HomingMethod>(method); r.configurationGeneration = app->axis.generation; return r;
}
void qualify(bool oldHomed = false) {
    writeResponseConfirmed = true; app->knownTargets[0] |= 2;
    app->configuration.target = app->axis.target; app->configuration.operationId = 99;
    auto& p = app->homePrerequisites; p.target = app->axis.target; p.configurationGeneration = app->axis.generation;
    // Fixture authority is independent of the subsequently admitted request.
    p.qualifiedMethod = ESS::HomingMethod::METHOD_35;
    p.qualifiedSearchSpeed = 60; p.qualifiedReturnSpeed = 30; p.qualifiedRampTime = 100;
    p.methodQualified = p.nativeRatesQualified = p.nativeRampQualified = p.zeroOffsetQualified = p.auxiliaryQualified = true;
    p.inputsQualified = p.indexQualified = p.readinessQualified = p.referenceSemanticsQualified = true;
    p.observedUs = hardware.time; p.maximumAgeUs = 1000000; p.rawMotion = oldHomed ? 3 : 1;
    auto& m = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    m.valid = true; m.value.target = app->axis.target; m.value.rawMotion = p.rawMotion;
    m.value.inPosition = m.value.enabled = true; m.value.homingComplete = oldHomed;
    m.observedEarliestUs = m.observedLatestUs = hardware.time;
    app->axis.originKnown = true; app->axis.originNative = 77; app->axis.originSource = ScaleSource::QUALIFIED;
}
uint32_t admit(int method = 35) {
    uint32_t id = 0; assert(host(app).startHome(app,1,1,request(method),id) == Probe::Action::OK);
    assert(id && view(id).homeContext && axisReserved(*app,1)); return id;
}
void productionAndFixtureGates() {
    fresh(); uint32_t unchanged = 77;
    assert(host(app).startHome(app,1,1,request(),unchanged) != Probe::Action::OK);
    assert(unchanged == 77 && !hardware.writes && !axisReserved(*app,1));
    qualify();
    for (unsigned parameter = 0; parameter < 4; ++parameter) {
        auto mismatch = request();
        if (parameter == 0) mismatch.method = ESS::HomingMethod::METHOD_33;
        if (parameter == 1) mismatch.searchSpeed = 61;
        if (parameter == 2) mismatch.returnSpeed = 31;
        if (parameter == 3) mismatch.rampTime = 101;
        assert(host(app).startHome(app,2,1,mismatch,unchanged) == Probe::Action::INVALID);
        assert(unchanged == 77 && !hardware.writes && !app->owner.pending() && !axisReserved(*app,1));
    }
    for (int method : {1,18,24,-1,15}) {
        const auto* descriptor = ESS::homeMethod(static_cast<ESS::HomingMethod>(method));
        const auto expected = !descriptor ? Probe::Action::INVALID :
            descriptor->support == ESS::HomeSupport::UNRESOLVED ? Probe::Action::UNRESOLVED : Probe::Action::UNIMPLEMENTED;
        assert(host(app).startHome(app,2,1,request(method),unchanged) == expected);
        assert(unchanged == 77 && !hardware.writes && !app->owner.pending());
        const std::string command = "@" + std::to_string(method + 100) + " home " + std::to_string(method) + " 60 30 100 zero\n";
        Serial.input = command; Serial.output.clear(); pump();
        const char* disposition = expected == Probe::Action::INVALID ? "invalid" : expected == Probe::Action::UNRESOLVED ? "unresolved" : "unimplemented";
        assert(Serial.input.empty() && Serial.output.find(std::string("\"result\":\"") + disposition + "\"") != std::string::npos);
        assert(!hardware.writes && !app->owner.pending() && !axisReserved(*app,1));
    }
    app->homePrerequisites.indexQualified = false;
    assert(host(app).startHome(app,3,1,request(33),unchanged) == Probe::Action::INVALID);
    auto r = request(); r.offset = 1;
    assert(host(app).startHome(app,4,1,r,unchanged) == Probe::Action::UNRESOLVED);
    assert(unchanged == 77 && !hardware.writes);
    // The invalid console cases above advance the fixture clock past the
    // readiness age; establish fresh simulated evidence for this reuse case.
    qualify(); auto mismatch = request(); mismatch.searchSpeed = 61;
    const auto next = app->nextOperationId;
    assert(startHome(app, 5, 1, mismatch, unchanged) == Probe::Action::INVALID);
    assert(unchanged == 77 && app->nextOperationId == next);
    assert(app->records[0].kind == App::OperationKind::EMPTY && !app->records[0].operationId);
    const auto reused = app->nextOperationId;
    for (const char value : std::string("@5 home 35 60 30 100 zero\n")) app->console.feed(value);
    assert(app->latestOperationId == reused);
    assert(findRecord(*app, reused) == &app->records[0] && app->records[0].kind == App::OperationKind::HOME);
    assert(cancel(app, reused) == Probe::Action::OK); pump();
    assert(!view(reused).pending && !hardware.writes && !axisReserved(*app, 1));
    assert(release(app, reused) == Probe::Action::OK && app->records[0].kind == App::OperationKind::EMPTY);
}
void completeAndInvalidateReference() {
    fresh(); qualify(); const uint32_t generation = app->axis.generation; const auto id = admit();
    reply(id); assert(hardware.tx.size() == 21 && hardware.tx[3] == 0x31 && hardware.tx[5] == 6);
    assert(!app->axis.originKnown && app->axis.generation == generation + 1);
    assert(view(id).homeContext->request.configurationGeneration == generation);
    reply(id); assert(hardware.tx[3] == 0x27 && hardware.tx[5] == 16);
    reply(id, words(0,3)); assert(view(id).pending && !app->coordinateReference.nativeKnown);
    reply(id, words(0,0));
    assert(!view(id).pending && view(id).homeContext->completion == ActionCompletion::OBSERVED);
    assert(!axisReserved(*app,1) && app->coordinateReference.nativeKnown && app->coordinateReference.nativePosition == 0);
    assert(app->coordinateReference.configurationGeneration == app->axis.generation && !app->axis.originKnown);
    assert(view(id).homeContext->request.configurationGeneration == generation);
}
void oldFlagsAndPartialSetup() {
    fresh(); qualify(true); auto id = admit(); reply(id); reply(id);
    reply(id,words(0,3)); assert(view(id).pending && !view(id).homeContext->homedLowObserved);
    assert(!app->coordinateReference.nativeKnown);
    assert(cancel(app,id) == Probe::Action::OK); pump();
    assert(!view(id).pending && view(id).homeContext->uncertain && axisReserved(*app,1));
    const auto writes = hardware.writes; pump(); assert(hardware.writes == writes);
    fresh(); qualify(); id = admit(); reply(id,crc({1,0x90,3}));
    assert(!view(id).pending && view(id).homeContext->setupExecution == ActionExecution::REJECTED);
    assert(view(id).homeContext->uncertain && view(id).homeContext->execution == ActionExecution::NOT_TRANSMITTED && axisReserved(*app,1));
    assert(hardware.writes == 1 && !app->axis.originKnown && !app->coordinateReference.nativeKnown);
    fresh(); qualify(); id = admit(); reply(id); waitTx(id);
    for (unsigned i = 0; i < 40000 && view(id).pending; ++i) step();
    assert(!view(id).pending && view(id).homeContext->execution == ActionExecution::UNKNOWN && app->owner.needsRecovery());
    const auto sent = hardware.writes; pump(); assert(hardware.writes == sent && axisReserved(*app,1));
    fresh(); qualify(); id = admit(); reply(id); reply(id); reply(id,words(0,4));
    assert(!view(id).pending && view(id).homeContext->outcome == ActionOutcome::REPLY_ERROR);
    assert(!app->coordinateReference.nativeKnown && view(id).homeContext->uncertain && axisReserved(*app,1));
    const auto unexpected = hardware.writes; pump(); assert(hardware.writes == unexpected);
}
void stopAtBoundariesAndUnsentExpiry() {
    for (unsigned phase = 0; phase < 5; ++phase) {
        fresh(); qualify(); const auto id = admit();
        if (phase >= 1) reply(id);
        if (phase >= 2) reply(id);
        if (phase >= 3) reply(id,words(0,3));
        if (phase == 4) waitTx(id);
        ActionRequest stop; stop.kind = ActionKind::STOP;
        stop.stop.behavior = StopBehavior::CONFIGURED_DECELERATION; uint32_t stopping = 0;
        assert(host(app).startAction(app,50,1,stop,stopping) == Probe::Action::OK);
        if (phase == 4) reply(id, words(0,0));
        pump(); assert(!view(id).pending && view(id).interruptedByStop);
        assert(!app->coordinateReference.nativeKnown);
        reply(stopping); reply(stopping,words(0,1));
        assert(!view(stopping).pending && !axisReserved(*app,1));
        assert(view(id).homeContext->completion == ActionCompletion::NOT_OBSERVED || phase == 4);
    }
    fresh(); qualify(); app->homePrerequisites.maximumAgeUs = 20000; const auto id = admit();
    const auto cap = view(id).homeContext->prerequisites.observedUs + 20000;
    advanceHardware(cap + 1000); serviceApplication(); pump();
    assert(!view(id).pending && !hardware.writes && !axisReserved(*app,1));
    assert(view(id).homeContext->status.detail == static_cast<int32_t>(ESS::HomeError::READINESS));
    fresh(); qualify(); const auto changed = admit(); reply(changed);
    invalidateAxis(*app); pump();
    assert(!view(changed).pending && view(changed).homeContext->outcome == ActionOutcome::CANCELLED && hardware.writes == 1);
    assert(!app->coordinateReference.nativeKnown && view(changed).homeContext->uncertain);
    fresh(); qualify(); app->axis.generation = UINT32_MAX;
    app->homePrerequisites.configurationGeneration = UINT32_MAX;
    const auto exhausted = admit(); reply(exhausted); pump();
    assert(!view(exhausted).pending && app->axis.generation == 0 && hardware.writes == 1);
    assert(view(exhausted).homeContext->outcome == ActionOutcome::CANCELLED);
    assert(!app->coordinateReference.nativeKnown && view(exhausted).homeContext->uncertain);
}

void orderedSharedMotionCachePreservesNewHomeReference() {
    // Each cache value represents a separately checked shared read. Exercise
    // the actual application loop's ordering against correlated home evidence.
    for (unsigned scenario = 0; scenario < 5; ++scenario) {
        fresh(); qualify(); app->homePrerequisites.qualifiedMethod = ESS::HomingMethod::METHOD_33;
        const auto id = admit(33); reply(id); reply(id); reply(id,words(0,4));
        auto& motion = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
        motion.valid = true; motion.value.target = app->axis.target; motion.value.rawAlarm = 0;
        const bool released = scenario >= 3;
        motion.value.rawMotion = released ? 16 : 4;
        motion.value.running = !released; motion.value.released = released;
        motion.value.enabled = !released; motion.value.alarmFlag = motion.value.inPosition = false;
        motion.observedEarliestUs = motion.observedLatestUs = hardware.time;
        const uint64_t oldLatest = motion.observedLatestUs;
        reply(id,words(0,3));
        const uint64_t stationary = view(id).homeContext->completionObservedUs;
        assert(oldLatest < stationary);
        // Equal bounds overlap the new conservative witness; newer values
        // remain evidence of external activity/release after the reference.
        if (scenario == 1 || scenario == 4)
            motion.observedEarliestUs = motion.observedLatestUs = stationary;
        if (scenario == 2)
            motion.observedEarliestUs = motion.observedLatestUs = hardware.time;
        reply(id,words(0,0));
        assert(!view(id).pending && view(id).homeContext->state == ActionState::SUCCEEDED);
        const bool definitelyOlder = scenario == 0 || scenario == 3;
        assert(app->coordinateReference.nativeKnown == definitelyOlder);
        assert(!app->axis.originKnown);
        if (definitelyOlder) {
            assert(app->coordinateReference.observedUs == stationary);
            const auto reference = axisReference(*app);
            assert(reference.nativeKnown && !reference.stationary);
            const unsigned writes = hardware.writes;
            assert(!setAxisOrigin(app->axis,0,reference));
            assert(!app->axis.originKnown && hardware.writes == writes);
        }
    }
    // An origin without a newer native witness still uses the cached external
    // activity rule. Old timestamps alone do not grant coordinate authority.
    fresh(); qualify(); assert(app->axis.originKnown && !app->coordinateReference.nativeKnown);
    auto& motion = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::MOTION)];
    motion.value.running = true; motion.value.rawMotion = 4;
    step(); assert(!app->axis.originKnown && !app->coordinateReference.nativeKnown);
}
void refreshState(uint16_t position) {
    uint32_t id = 0;
    assert(startTypedRead(app,88,1,ESS::ReadKind::STATE,id,false) == Probe::Action::OK);
    const std::vector<uint8_t> responses[] = {
        words(0,3), words(0,0), crc({1,3,6,0,0,static_cast<uint8_t>(position >> 8),static_cast<uint8_t>(position),0,0})
    };
    for (const auto& response : responses) {
        const auto token = findRecord(*app,id)->read.step;
        waitTx(id);
        scheduleReply(std::max(hardware.writeStarted + hardware.tx.size()*87 + 1000, hardware.time + 1000), response);
        for (unsigned i = 0; i < 25000 && view(id).pending && findRecord(*app,id)->read.step == token; ++i) step();
        assert(!view(id).pending || findRecord(*app,id)->read.step != token);
    }
    assert(!view(id).pending);
}
void feedbackAfterHomeUsesTheNewZeroWitness() {
    for (unsigned scenario = 0; scenario < 5; ++scenario) {
        fresh(); qualify();
        app->configuration.algorithmKnown = true;
        app->configuration.wordOrderKnown = scenario < 3;
        app->configuration.wordOrder = ESS::WordOrder::HIGH_WORD_FIRST;
        app->configuration.algorithm = ESS::ControlAlgorithm::ALGORITHM_1;
        auto& historical = app->stateCache.blocks[2];
        historical.valid = true; historical.value.target = app->axis.target;
        historical.value.pairKnown = true; historical.value.rawPosition = 123;
        historical.value.rawPositionWords[1] = 123;
        historical.value.configOperationId = app->configuration.operationId;
        historical.observedEarliestUs = historical.observedLatestUs = hardware.time;
        const auto id = admit(); reply(id); reply(id); reply(id,words(0,3)); reply(id,words(0,0));
        assert(app->coordinateReference.nativeKnown && historical.invalidatedUs);
        const auto generation = app->axis.generation;
        // A new zero matches the correlated home witness even though the older,
        // invalidated feedback reports a different position. A first new nonzero
        // or a later change from fresh zero must invalidate that witness.
        const bool firstNonzero = scenario == 2 || scenario == 4;
        refreshState(firstNonzero ? 1 : 0);
        assert(app->coordinateReference.nativeKnown == !firstNonzero);
        assert(app->axis.generation == generation + (firstNonzero ? 1U : 0U));
        assert(app->stateCache.blocks[2].value.pairKnown == (scenario < 3));
        if (scenario == 1 || scenario == 3) {
            refreshState(1);
            assert(!app->coordinateReference.nativeKnown && app->axis.generation == generation + 1);
        }
        assert(!app->axis.originKnown);
    }
}
const std::vector<uint8_t> HOME_PARAMETERS = {1,3,14,0,7,0,24,0,60,0,30,0,100,0x12,0x34,0xAB,0xCD,0xEE,0x29};
const std::vector<uint8_t> HOME_ZERO = {1,3,14,0,7,0,35,0,60,0,30,0,100,0,0,0,0,0xCE,0x1E};
uint32_t parameterRead() {
    uint32_t id = 0; assert(submitHomingParameterRead(id));
    assert(id && view(id).pending && view(id).typedRead); return id;
}
void parameterReply(uint32_t id, const std::vector<uint8_t>& bytes = HOME_PARAMETERS) {
    waitTx(id);
    const std::vector<uint8_t> expected = {1,3,0,0x30,0,7,4,7};
    assert(hardware.tx == expected);
    scheduleReply(std::max(hardware.writeStarted+8*87+1000,hardware.time+1000),bytes);
    for (unsigned i=0;i<25000 && view(id).pending;++i) step();
    assert(!view(id).pending); pump();
}
void command(const std::string& text) {
    Serial.input = text; Serial.output.clear();
    for (unsigned i=0;i<500 && !Serial.input.empty();++i) step();
    assert(Serial.input.empty()); pump(64);
}
void parameterReadApplicationPath() {
    fresh(); uint32_t unchanged = 77;
    app->discovery.owned = true;
    assert(!submitHomingParameterRead(unchanged) && unchanged == 77 && !hardware.writes);
    app->discovery.owned = false;
    assert(typedRead(app,1,2,ESS::ReadKind::HOMING_PARAMETERS,unchanged) == Probe::Action::INVALID);
    assert(unchanged == 77 && !hardware.writes);
    const auto generation = app->axis.generation;
    const auto id = parameterRead(); ESS::ReadContext copied;
    assert(copyReadResult(id,copied) && copied.state == ReadState::ACTIVE && !releaseOperation(id));
    ESS::HomingParametersObservation out; out.rawMethod = 999;
    assert(!ESS::getHomingParameters(copied,out) && out.rawMethod == 999);
    parameterReply(id);
    assert(copyReadResult(id,copied) && ESS::getHomingParameters(copied,out));
    assert(out.rawAuxiliary == 7 && out.rawMethod == 24 && out.rawSearchSpeed == 60 && out.rawReturnSpeed == 30 && out.rawRampTime == 100);
    assert(out.rawOffsetWords[0] == 0x1234 && out.rawOffsetWords[1] == 0xABCD && out.provenance.length == 19);
    assert(out.methodKnown && out.methodSupport == ESS::HomeSupport::UNIMPLEMENTED && out.configurationGeneration == generation);
    assert(hardware.writes == 1 && app->homingParameters.operationId == id && !app->homePrerequisites.methodQualified &&
        !app->homePrerequisites.auxiliaryQualified && !app->homePrerequisites.zeroOffsetQualified && !app->coordinateReference.nativeKnown);
    command("status\n"); pump(); assert(copyReadResult(id,copied));
    assert(releaseOperation(id)); copied.operationId = 999;
    assert(!copyReadResult(id,copied) && copied.operationId == 999 && !releaseOperation(id));
    const auto again = parameterRead(); parameterReply(again,HOME_ZERO);
    assert(hardware.writes == 2 && again != id && !app->homePrerequisites.methodQualified);
    assert(host(app).startHome(app,1,1,request(),unchanged) != Probe::Action::OK && unchanged == 77 && hardware.writes == 2);
    assert(releaseOperation(again));
    qualify(); command("@30 home 35 60 30 100 zero\n"); const auto home = app->latestOperationId;
    assert(!copyReadResult(home,copied) && copied.operationId == 999);
    assert(cancel(app,home) == Probe::Action::OK); pump(); assert(releaseOperation(home));
}
void parameterReadQualificationAndHistory() {
    fresh(); qualify(); const auto original = app->homePrerequisites;
    const auto origin = app->axis.originNative; const auto generation = app->axis.generation;
    const auto id = parameterRead(); parameterReply(id,HOME_ZERO);
    assert(app->homePrerequisites.methodQualified && app->homePrerequisites.nativeRatesQualified &&
        app->homePrerequisites.nativeRampQualified && app->homePrerequisites.zeroOffsetQualified &&
        app->homePrerequisites.auxiliaryQualified && app->homePrerequisites.readinessQualified);
    const auto changed = parameterRead();
    parameterReply(changed,{1,3,14,0xFF,0xFF,0xFF,0xFC,0xF1,0x23,0xE2,0x34,0xD3,0x45,0xC4,0x56,0xB5,0x67,0x38,1});
    const auto& p = app->homePrerequisites;
    assert(!p.methodQualified && !p.nativeRatesQualified && !p.nativeRampQualified && !p.zeroOffsetQualified &&
        !p.auxiliaryQualified && !p.readinessQualified && !p.referenceSemanticsQualified);
    assert(p.qualifiedMethod == original.qualifiedMethod && p.qualifiedSearchSpeed == original.qualifiedSearchSpeed &&
        p.qualifiedReturnSpeed == original.qualifiedReturnSpeed && p.qualifiedRampTime == original.qualifiedRampTime &&
        p.observedUs == original.observedUs && p.auxiliary == original.auxiliary);
    assert(app->axis.generation == generation && app->axis.originKnown && app->axis.originNative == origin);
    assert(!app->coordinateReference.nativeKnown && hardware.writes == 2);
    for (unsigned stale=0;stale<3;++stale) {
        fresh(); const auto historical = parameterRead(); const auto admittedGeneration = app->axis.generation;
        if (stale == 0) ++app->axis.generation;
        else if (stale == 1) ++app->bindingGeneration;
        else { app->homingParameters.operationId = historical+1; app->homingParameters.rawMethod = 321; }
        parameterReply(historical);
        ESS::ReadContext copy; ESS::HomingParametersObservation result;
        assert(copyReadResult(historical,copy) && ESS::getHomingParameters(copy,result));
        assert(result.configurationGeneration == admittedGeneration && result.rawOffsetWords[1] == 0xABCD);
        assert(app->homingParameters.operationId == (stale == 2 ? historical+1 : 0));
        if (stale == 2) assert(app->homingParameters.rawMethod == 321);
    }
}
void parameterReadConsoleFailureAndCapacity() {
    fresh();
    for (const char* invalid : {"@1 home read extra","@2 home read 1 extra","@3 home read 0","@4 home read 248",
        "@5 home read -1","@6 home read 2","@7 home reads"}) {
        command(std::string(invalid)+"\n");
        assert(Serial.output.find("\"ok\":false") != std::string::npos && !hardware.writes);
    }
    command("home read 2\n"); // Real admission refusal must name the callable family in human help.
    assert(Serial.output.find("Hint: help home shows syntax") != std::string::npos && !hardware.writes);
    command("@8 home read\n"); const auto id = app->latestOperationId;
    assert(Serial.output.find("\"command\":\"home-read\"") != std::string::npos);
    command("@9 result "+std::to_string(id)+"\n"); assert(Serial.output.find("\"read_kind\":\"homing_parameters\"") != std::string::npos);
    parameterReply(id);
    assert(Serial.output.find("\"raw_offset_words\":[4660,43981]") != std::string::npos);
    assert(releaseOperation(id)); command("@10 result "+std::to_string(id)+"\n");
    assert(Serial.output.find("\"ok\":false") != std::string::npos);
    const auto failed = parameterRead(); parameterReply(failed,{1,0x83,2,0xC0,0xF1});
    ESS::ReadContext copy;
    assert(copyReadResult(failed,copy) && copy.outcome == ReadOutcome::REPLY_ERROR && copy.observations[0].length == 5);
    assert(app->homingParameters.operationId == id && app->homingParameters.rawOffsetWords[1] == 0xABCD);
    assert(releaseOperation(failed));
    fresh(); const auto cancelled = parameterRead(); assert(cancel(app,cancelled) == Probe::Action::OK); pump();
    assert(copyReadResult(cancelled,copy) && copy.outcome == ReadOutcome::CANCELLED && !hardware.writes);
    assert(releaseOperation(cancelled));
    fresh(); uint32_t retained[REQUEST_CAPACITY] = {};
    for (auto& kept : retained) { kept = parameterRead(); parameterReply(kept); }
    uint32_t unchanged = 77; assert(!submitHomingParameterRead(unchanged) && unchanged == 77 && hardware.writes == REQUEST_CAPACITY);
    for (auto kept : retained) assert(copyReadResult(kept,copy) && copy.state == ReadState::SUCCEEDED);
    for (auto kept : retained) assert(releaseOperation(kept));
}
void parameterReadHumanTerminal() {
    const std::vector<uint8_t> unknownReply = {1,3,14,0xFF,0xFF,0xFF,0xFC,0xF1,0x23,0xE2,0x34,0xD3,0x45,0xC4,0x56,0xB5,0x67,0x38,1};
    for (const bool unknown : {false, true}) {
        fresh();
        const auto desired = app->simple.desired;
        const auto generation = app->axis.generation;
        command("home read\n");
        const auto id = app->latestOperationId;
        assert(id && view(id).pending && view(id).typedRead && !findRecord(*app,id)->programmaticRead);
        parameterReply(id, unknown ? unknownReply : HOME_ZERO);
        pump(); // Inspect the delivered human result before another command can recycle it.
        assert(Serial.output.find("[OK] home-read") != std::string::npos);
        for (const char* text : {unknown ? "Raw auxiliary: 65535" : "Raw auxiliary: 7",
            unknown ? "Raw method: 65532" : "Raw method: 35",
            unknown ? "Raw search speed: 61731" : "Raw search speed: 60",
            unknown ? "Raw return speed: 57908" : "Raw return speed: 30",
            unknown ? "Raw ramp time: 54085" : "Raw ramp time: 100",
            unknown ? "Raw offset words: 50262, 46439" : "Raw offset words: 0, 0",
            unknown ? "Auxiliary known: no" : "Auxiliary known: yes",
            unknown ? "Method known: no" : "Method known: yes",
            unknown ? "Method support: unknown" : "Method support: implemented",
            unknown ? "Search speed word within numeric policy: no" : "Search speed word within numeric policy: yes",
            unknown ? "Return speed word within numeric policy: no" : "Return speed word within numeric policy: yes",
            unknown ? "Ramp word within numeric policy: no" : "Ramp word within numeric policy: yes"})
            assert(Serial.output.find(text) != std::string::npos);
        for (const char* text : {"Speed scale resolution: unresolved scale", "Ramp scale resolution: unresolved scale",
            "Offset word order resolution: unresolved word-order applicability",
            "Offset signed resolution: unresolved signed encoding", "Offset scale resolution: unresolved scale",
            "Raw zero offset does not establish a calibrated origin.",
            "Range flags use library numeric policy bounds; drive limits remain unqualified."})
            assert(Serial.output.find(text) != std::string::npos);
        ESS::ReadContext retained;
        assert(copyReadResult(id,retained) && retained.state == ReadState::SUCCEEDED);
        assert(hardware.writes == 1 && hardware.tx[1] == 3 && !app->owner.pending() && !axisReserved(*app,1));
        const auto& p = app->homePrerequisites;
        assert(!p.methodQualified && !p.nativeRatesQualified && !p.nativeRampQualified && !p.zeroOffsetQualified &&
            !p.auxiliaryQualified && !p.inputsQualified && !p.indexQualified && !p.readinessQualified &&
            !p.referenceSemanticsQualified && p.observedUs == 0);
        assert(app->axis.generation == generation && !app->axis.originKnown && !app->coordinateReference.nativeKnown);
        const auto& current = app->simple.desired;
        assert(current.speedRpm == desired.speedRpm && current.acceleration == desired.acceleration &&
            current.deceleration == desired.deceleration && current.accelerationKnown == desired.accelerationKnown &&
            current.decelerationKnown == desired.decelerationKnown && current.setup == desired.setup &&
            current.scaleKnown == desired.scaleKnown && current.stepsPerTurn.numerator == desired.stepsPerTurn.numerator &&
            current.stepsPerTurn.denominator == desired.stepsPerTurn.denominator);
        assert(releaseOperation(id));
    }
}
}
int main() {
    parameterReadApplicationPath(); parameterReadQualificationAndHistory(); parameterReadConsoleFailureAndCapacity(); parameterReadHumanTerminal();
    productionAndFixtureGates(); completeAndInvalidateReference(); oldFlagsAndPartialSetup();
    stopAtBoundariesAndUnsentExpiry(); orderedSharedMotionCachePreservesNewHomeReference();
    feedbackAfterHomeUsesTheNewZeroWitness();
}
