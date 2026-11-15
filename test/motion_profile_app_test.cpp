// SPDX-License-Identifier: MIT
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
#include <cstdio>
#include <string>
#include <vector>
namespace {
using namespace MotorControlRS;
bool captureCandidateFrames=false;
std::vector<std::vector<uint8_t>> candidateFrames;
void step() {
    const auto writes=hardware.writes;
    advanceHardware(hardware.time+10); serviceApplication();
    if(captureCandidateFrames && hardware.writes!=writes) candidateFrames.push_back(hardware.tx);
}
void fixtureSetup() {
    assert(beginApplication({Board::kRs485TxPin,Board::kRs485RxPin,Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh},Board::kRs485ReceiverDisabledDuringTransmit));
}
std::vector<uint8_t> crc(std::vector<uint8_t> v) {
    const auto c=ESS::calcCrc16(v.data(),v.size()); v.push_back(c); v.push_back(c>>8); return v;
}
std::vector<uint8_t> words(std::initializer_list<uint16_t> w) {
    std::vector<uint8_t> v={1,3,static_cast<uint8_t>(2*w.size())};
    for (auto x:w) { v.push_back(x>>8); v.push_back(x); } return crc(v);
}
void refresh() {
    auto& c=app->configuration; c.operationId=1; c.target=app->axis.target; c.wordOrderKnown=true;
    c.raw.subdivision=1000; c.raw.inputFunctions[0]=1; c.raw.inputFunctions[1]=2; c.raw.inputFunctions[2]=3;
    for (auto& b:app->stateCache.blocks) {
        b.valid=true; b.value.target=app->axis.target; b.observedEarliestUs=b.observedLatestUs=hardware.time;
        b.invalidatedUs=0;
    }
    app->stateCache.blocks[2].value.pairKnown=true;
    app->stateCache.blocks[0].value.rawMotion=1; app->knownTargets[0]|=2;
}
void reply(const Rtu::RequestId& id, const std::vector<uint8_t>& bytes) {
    for (unsigned i=0;i<25000 && !app->owner.txAccepted(id);++i) step();
    assert(app->owner.txAccepted(id));
    scheduleReply(std::max(hardware.writeStarted+hardware.tx.size()*87+1000,hardware.time+1000),bytes);
}
void fixtureReply(const std::vector<uint8_t>& bytes) {
    const auto phase=app->motionProfile.view.phase; reply(app->motionProfile.request,bytes);
    for (unsigned i=0;i<25000 && app->motionProfile.view.pending && app->motionProfile.view.phase==phase;++i) step();
    assert(!app->motionProfile.view.pending || app->motionProfile.view.phase!=phase);
}
void testHostTupleWaitsForProfileHarvest() {
    resetHardware(); fixtureSetup(); hardware.txCharacterUs=87;
    assert(uart.startCapture(20,timing().holdUs)); refresh();
    Probe::MotionProfileView view;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,view)==Probe::Action::OK);
    // Service the real owner to a terminal result, leaving application harvest
    // explicitly pending. A cooperative caller must not depend on loop order.
    const auto request=app->motionProfile.request;
    for(unsigned i=0;i<25000 && !app->owner.txAccepted(request);++i) {
        advanceHardware(hardware.time+10); app->owner.service(uart.sample());
    }
    assert(app->owner.txAccepted(request));
    scheduleReply(std::max(hardware.writeStarted+hardware.tx.size()*87+1000,hardware.time+1000),words({10,100,100,30,0,7}));
    for(unsigned i=0;i<25000 && !app->owner.result(request);++i) {
        advanceHardware(hardware.time+10); app->owner.service(uart.sample());
    }
    assert(app->owner.result(request) && app->motionProfile.view.pending && !app->owner.active() && !app->owner.pending());
    Probe::HostRequest changed; changed.tuple=app->serial.active; changed.tuple.baud=9600;
    Probe::HostSnapshot serial;
    const auto calls=hardware.configCalls, generation=app->serial.generation;
    assert(hostSerial(app,&changed,serial)==Probe::Action::BUSY);
    assert(hardware.configCalls==calls && app->serial.generation==generation && app->motionProfile.request.owner);
    serviceMotionProfile(*app,hardware.time);
    assert(!app->motionProfile.view.pending && app->motionProfile.view.ok);
    assert(hostSerial(app,&changed,serial)==Probe::Action::OK && serial.active.baud==9600);
    Probe::HostRequest restore; restore.restore=true;
    assert(hostSerial(app,&restore,serial)==Probe::Action::OK && serial.active.baud==115200);
    app->~App(); std::free(app); app=nullptr;
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetUsbConsole(); platformReady=false;
}
void testUncertainRestoreRequiresExplicitReadOnlySettlement() {
    resetHardware(); fixtureSetup(); hardware.txCharacterUs=87;
    assert(uart.startCapture(20,timing().holdUs)); refresh();
    Probe::MotionProfileView view;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,view)==Probe::Action::OK);
    fixtureReply(words({10,100,100,30,0,7}));
    const auto generation=app->axis.generation;
    app->axis.originKnown=app->coordinateReference.nativeKnown=true;
    app->coordinateReference.target=app->axis.target;
    app->coordinateReference.configurationGeneration=generation;
    app->coordinateReference.observedUs=hardware.time;
    app->coordinateReference.maximumAgeUs=1000000;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::RESTORE,view)==Probe::Action::OK);
    auto bad=crc({1,16,0,0x21,0,5}); bad.back()^=0x80;
    fixtureReply(bad);
    const auto historical=app->motionProfile.view;
    assert(historical.restoreUnsettled && historical.writeExecutionUnknown);
    assert(!historical.ok && historical.writeTxAccepted && axisReserved(*app,1));
    assert(app->axis.generation==generation+1 && app->axis.originKnown && !app->coordinateReference.nativeKnown);
    const auto writes=hardware.writes;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::FORGET,view)==Probe::Action::BUSY);
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::RESTORE,view)!=Probe::Action::OK);
    assert(selectTarget(app,2)==Probe::Action::BUSY && hardware.writes==writes);
    uint32_t operation=0;
    assert(recover(app,1,operation)==Probe::Action::OK);
    for(unsigned i=0;i<80000 && app->owner.recovering();++i) step();
    assert(!app->owner.recovering() && !app->owner.needsRecovery());
    refresh();
    app->stateCache.blocks[0].observedEarliestUs=historical.writeDeliveredUs-1;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,view)==Probe::Action::OK);
    assert(view.phase==Probe::MotionProfilePhase::READBACK && view.deadlineUs>historical.writeDeadlineUs);
    fixtureReply(words({10,100,100,30,0,7}));
    assert(app->motionProfile.view.restoreUnsettled && !app->motionProfile.view.ok &&
        !std::strcmp(app->motionProfile.view.error,"stationary_required"));
    refresh();
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,view)==Probe::Action::OK);
    fixtureReply(words({10,100,100,30,0,7}));
    const auto& settled=app->motionProfile.view;
    assert(settled.restored && settled.ok && !settled.restoreUnsettled && settled.writeExecutionUnknown);
    assert(settled.writeDeadlineUs==historical.writeDeadlineUs &&
        settled.writeConfigurationGeneration==historical.writeConfigurationGeneration &&
        settled.writeSerialGeneration==historical.writeSerialGeneration && settled.writeBindingGeneration==historical.writeBindingGeneration);
    assert(settled.writeTxAccepted==historical.writeTxAccepted && settled.writeReplyLength==historical.writeReplyLength &&
        !std::memcmp(settled.writeReply,historical.writeReply,sizeof(settled.writeReply)) &&
        !std::memcmp(settled.writeTx,historical.writeTx,sizeof(settled.writeTx)));
    assert(hardware.writes==writes+2); // Explicit reconciliation reads, no replay of FC10.
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::FORGET,view)==Probe::Action::OK);
    app->~App(); std::free(app); app=nullptr;
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetUsbConsole(); platformReady=false;
}

void candidatePump(unsigned count=64) { for(unsigned i=0;i<count;++i) step(); }
void candidateCommand(const std::string& line) {
    Serial.input=line; Serial.output.clear();
    for(unsigned i=0;i<500 && !Serial.input.empty();++i) step();
    assert(Serial.input.empty()); candidatePump();
}
Probe::ResultView candidateView(uint32_t id) {
    Probe::ResultView result; assert(lookup(app,id,result)); return result;
}
void candidateReadReply(uint32_t id,const std::vector<uint8_t>& bytes) {
    const auto token=candidateView(id).typedRead->step;
    auto* record=findRecord(*app,id); assert(record); reply(record->requestId,bytes);
    for(unsigned i=0;i<25000 && candidateView(id).pending && candidateView(id).typedRead->step==token;++i) step();
    assert(!candidateView(id).pending || candidateView(id).typedRead->step!=token);
}
void candidateReadiness(ESS::WordOrder order=ESS::WordOrder::HIGH_WORD_FIRST) {
    candidateCommand("@601 read config\n"); const auto config=app->latestOperationId;
    candidateReadReply(config,words({0,1000})); candidateReadReply(config,words({0,0,0}));
    candidateReadReply(config,words({0,0,static_cast<uint16_t>(order)}));
    candidateReadReply(config,words({0,0,0,0,0})); candidateReadReply(config,words({3,4000}));
    candidatePump(); assert(releaseOperation(config));
    candidateCommand("@602 read state\n"); const auto state=app->latestOperationId;
    candidateReadReply(state,words({0,1})); candidateReadReply(state,words({0,0}));
    candidateReadReply(state,words({0,0,0})); candidatePump(); assert(releaseOperation(state));
    assert(singleSettingPolicy(*app,hardware.time));
}
void candidateSnapshot(uint16_t a=0,uint16_t b=7,uint16_t speed=30) {
    Probe::MotionProfileView snapshot;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,snapshot)==Probe::Action::OK);
    fixtureReply(words({10,100,100,speed,a,b}));
    assert(app->motionProfile.view.ok && app->motionProfile.view.saved);
}
void candidateFresh(ESS::WordOrder order=ESS::WordOrder::HIGH_WORD_FIRST,uint16_t a=0,uint16_t b=7) {
    captureCandidateFrames=false;
    if(app) { app->~App(); std::free(app); app=nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady=writeResponseConfirmed=false;
    ApplicationOptions options; options.observationMaxAgeMs=0;
    assert(beginApplication({Board::kRs485TxPin,Board::kRs485RxPin,Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh},Board::kRs485ReceiverDisabledDuringTransmit,options));
    hardware.txCharacterUs=87; assert(uart.startCapture(20,timing().holdUs));
    writeResponseConfirmed=true; candidateReadiness(order);
    candidateCommand("@603 read identity\n"); const auto identity=app->latestOperationId;
    candidateReadReply(identity,words({0x4EEA,0x0029,1,0})); candidatePump(); assert(releaseOperation(identity));
    candidateSnapshot(a,b);
    candidateFrames.clear(); captureCandidateFrames=true;
}
Probe::ProfileCandidate candidate(ESS::WordOrder order=ESS::WordOrder::HIGH_WORD_FIRST) {
    Probe::ProfileCandidate c; c.address=1; c.wordOrder=order;
    c.words[0]=100; c.words[1]=200; c.words[2]=60;
    c.words[order==ESS::WordOrder::HIGH_WORD_FIRST ? 3 : 4]=0;
    c.words[order==ESS::WordOrder::HIGH_WORD_FIRST ? 4 : 3]=100;
    return c;
}
Probe::ProfileCandidateResult candidateResult(uint32_t id) {
    Probe::ProfileCandidateResult result; assert(copyProfileCandidate(id,result)); return result;
}
uint32_t candidateSubmit(const Probe::ProfileCandidate& c) {
    uint32_t id=0; assert(submitProfileCandidate(c,id));
    assert(id && candidateResult(id).stage.pending && !releaseProfileCandidate(id)); return id;
}
void candidateReply(uint32_t id,const std::vector<uint8_t>& bytes) {
    const auto request=app->profileCandidate.request;
    assert(request.owner); reply(request,bytes);
    const auto same=[&] { const auto& current=app->profileCandidate.request;
        return current.owner==request.owner && current.slot==request.slot && current.generation==request.generation; };
    for(unsigned i=0;i<25000 && candidateResult(id).stage.pending && same();++i) step();
    assert(!candidateResult(id).stage.pending || !same());
}
void candidateWait(uint32_t id) {
    for(unsigned i=0;i<400000 && candidateResult(id).stage.pending;++i) step();
    assert(!candidateResult(id).stage.pending); candidatePump();
}
void candidateNoMotion(bool allowStop=false) {
    for(const auto& frame:candidateFrames) {
        if(frame.size()<6) continue;
        if(frame[1]==16) assert(frame[2]==0 && frame[3]==0x21 && frame[4]==0 && frame[5]==5);
        if(frame[1]==6) assert(allowStop && frame[2]==0 && frame[3]==0x27 &&
            (frame[4]==1 || frame[4]==2) && frame[5]==0);
    }
}
void candidateComplete(uint32_t id,const Probe::ProfileCandidate& c,uint16_t beforeA=0,uint16_t beforeB=7) {
    const auto deadline=candidateResult(id).stage.deadlineUs;
    candidateReply(id,words({10,100,100,30,beforeA,beforeB}));
    assert(candidateResult(id).stage.deadlineUs==deadline);
    candidateReply(id,crc({1,16,0,0x21,0,5}));
    std::vector<uint8_t> expected={1,16,0,0x21,0,5,10};
    for(const auto value:c.words) { expected.push_back(static_cast<uint8_t>(value>>8)); expected.push_back(static_cast<uint8_t>(value)); }
    assert(hardware.tx==crc(expected));
    candidateReply(id,words({c.words[0],c.words[1],c.words[2],c.words[3],c.words[4]}));
    candidatePump();
    const auto result=candidateResult(id);
    assert(result.stage.staged && !result.unchanged && result.stage.execution==ActionExecution::ACKNOWLEDGED);
    assert(result.stage.deadlineUs==deadline && result.stage.writeEvidence.txAccepted==19);
    assert(result.stage.readbackKnown && result.priorEvidence.length==17 && result.stage.readEvidence.length==15);
    assert(result.original[0]==10 && result.before[0]==10 && app->motionProfile.view.original[0]==10);
    assert(!app->rememberedMoveGeneration && !app->stage.applied);
    candidateNoMotion();
}
void testCandidateAdmissionAndRetainedResults() {
    for(const auto order:{ESS::WordOrder::HIGH_WORD_FIRST,ESS::WordOrder::LOW_WORD_FIRST}) {
        candidateFresh(order,order==ESS::WordOrder::HIGH_WORD_FIRST?0:7,order==ESS::WordOrder::HIGH_WORD_FIRST?7:0);
        const auto c=candidate(order); const auto before=hardware.writes;
        for(unsigned fault=0;fault<10;++fault) {
            auto bad=c;
            if(fault==0) bad.address=2;
            if(fault==1) bad.wordOrder=order==ESS::WordOrder::HIGH_WORD_FIRST?ESS::WordOrder::LOW_WORD_FIRST:ESS::WordOrder::HIGH_WORD_FIRST;
            if(fault==2) { bad.identitySpecified=true; bad.model=0x4EEB; bad.firmware=0x29; }
            if(fault==3) { bad.identitySpecified=true; bad.model=0x4EEA; bad.firmware=0x28; }
            if(fault==4) bad.words[0]=2001;
            if(fault==5) bad.words[1]=2001;
            if(fault==6) bad.words[2]=3001;
            if(fault==7) bad.wordOrder=static_cast<ESS::WordOrder>(2);
            if(fault==8) bad.words[order==ESS::WordOrder::HIGH_WORD_FIRST?3:4]=0x1000;
            if(fault==9) { bad.words[order==ESS::WordOrder::HIGH_WORD_FIRST?3:4]=0xF000; bad.words[order==ESS::WordOrder::HIGH_WORD_FIRST?4:3]=0; }
            uint32_t unchanged=777; const auto status=submitProfileCandidate(bad,unchanged);
            assert(!status && unchanged==777 && hardware.writes==before && !app->owner.pending());
            if(fault>=8) assert(status.detail==static_cast<int32_t>(Probe::Action::TARGET_RANGE));
        }
        auto qualified=c; qualified.identitySpecified=true; qualified.model=0x4EEA; qualified.firmware=0x29;
        const auto id=candidateSubmit(qualified);
        const auto original=app->motionProfile.view;
        Probe::MotionProfileView out; out.address=77;
        assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,out)==Probe::Action::BUSY);
        assert(motionProfileCommand(app,Probe::MotionProfileCommand::FORGET,out)==Probe::Action::BUSY);
        assert(out.address==original.address && !std::memcmp(original.original,app->motionProfile.view.original,sizeof(original.original)));
        uint32_t replacement=777; assert(!submitProfileCandidate(c,replacement) && replacement==777);
        candidateComplete(id,c,original.original[4],original.original[5]);
        const auto retained=candidateResult(id); const auto frames=hardware.writes;
        assert(!submitProfileCandidate(c,replacement) && replacement==777 && hardware.writes==frames);
        Probe::ProfileCandidateResult missing; missing.stage.operationId=777;
        assert(!copyProfileCandidate(id+100,missing) && missing.stage.operationId==777);
        assert(!releaseProfileCandidate(id+100));
        assert(candidateResult(id).stage.writeEvidence.txAccepted==retained.stage.writeEvidence.txAccepted);
        assert(releaseProfileCandidate(id) && !copyProfileCandidate(id,missing));
        candidateNoMotion();
    }
}
void testCandidateUnchangedAndWireRanges() {
    candidateFresh(); auto equal=candidate(); equal.words[1]=100; equal.words[2]=30; equal.words[4]=7;
    const auto id=candidateSubmit(equal); candidateReply(id,words({10,100,100,30,0,7}));
    const auto result=candidateResult(id);
    assert(!result.stage.pending && result.stage.staged && result.unchanged);
    assert(result.stage.execution==ActionExecution::NOT_TRANSMITTED && !result.stage.writeEvidence.txAccepted);
    assert(result.priorEvidence.length==17 && result.stage.readbackKnown && candidateFrames.size()==1);
    assert(app->motionProfile.view.saved && app->motionProfile.view.ok && !app->positionStartSpeedRelationRequired);
    assert(releaseProfileCandidate(id)); candidateNoMotion();
    // Equality is decided by the new read, even when the saved snapshot differs.
    candidateFresh(); const auto current=candidate(); const auto changed=candidateSubmit(current);
    candidateReply(changed,words({10,100,200,60,0,100}));
    const auto freshEqual=candidateResult(changed);
    assert(freshEqual.unchanged && freshEqual.stage.staged && freshEqual.stage.execution==ActionExecution::NOT_TRANSMITTED);
    assert(freshEqual.original[3]==30 && freshEqual.before[3]==60 && app->motionProfile.view.original[3]==30);
    assert(candidateFrames.size()==1 && !app->rememberedMoveGeneration && !app->motionProfile.view.ok);
    assert(releaseProfileCandidate(changed)); candidateNoMotion();
    for(bool maximum:{false,true}) {
        candidateFresh(); auto wire=candidate();
        wire.words[0]=wire.words[1]=maximum?2000:0; wire.words[2]=maximum?3000:0;
        wire.words[3]=maximum?0x0FFF:0xF000; wire.words[4]=maximum?0xFFFF:1;
        candidateComplete(candidateSubmit(wire),wire);
        assert(app->positionStartSpeedRelationRequired && !app->positionStartSpeedRelation.qualifiedUs);
        assert(!app->motionProfile.view.ok);
    }
    candidateFresh(ESS::WordOrder::HIGH_WORD_FIRST,0x1000,0);
    const auto raw=app->motionProfile.view; const auto frames=hardware.writes;
    Probe::MotionProfileView restore;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::RESTORE,restore)==Probe::Action::TARGET_RANGE);
    assert(!std::strcmp(restore.error,raw.error) && restore.ok==raw.ok && hardware.writes==frames);
    assert(app->motionProfile.view.saved && app->motionProfile.view.current[4]==0x1000 &&
        !std::memcmp(raw.original,app->motionProfile.view.original,sizeof(raw.original)));
}
void testCandidateCancellationAndFailures() {
    for(unsigned phase=0;phase<3;++phase) {
        candidateFresh(); const auto c=candidate(); const auto oldWitness=app->rememberedMoveGeneration;
        const auto id=candidateSubmit(c);
        if(phase) candidateReply(id,words({10,100,100,30,0,7}));
        if(phase==2) candidateReply(id,crc({1,16,0,0x21,0,5}));
        if(phase==2) reply(app->profileCandidate.request,words({100,200,60,0,100}));
        assert(cancel(app,id)==Probe::Action::OK); candidateWait(id);
        const auto result=candidateResult(id);
        assert(result.stage.cancelled && !result.stage.staged);
        assert(app->rememberedMoveGeneration==(phase==2?0:oldWitness));
        if(phase==2) assert(result.stage.readbackKnown && result.stage.execution==ActionExecution::ACKNOWLEDGED && result.stage.readEvidence.length==15);
        if(phase==0) assert(result.stage.execution==ActionExecution::NOT_TRANSMITTED && !result.stage.writeEvidence.txAccepted);
        candidateNoMotion(); assert(releaseProfileCandidate(id));
    }
    for(unsigned fault=0;fault<6;++fault) {
        candidateFresh(); const auto c=candidate(); const auto id=candidateSubmit(c);
        if(fault==4) {
            auto malformed=words({10,100,100,30,0,7}); malformed.back()^=0x80;
            candidateReply(id,malformed); const auto result=candidateResult(id);
            assert(!result.stage.staged && !result.stage.writeEvidence.txAccepted && result.priorEvidence.length==17);
            assert(result.priorEvidence.frameError!=ESS::FrameError::NONE && !result.priorEvidence.status);
            for(const auto& frame:candidateFrames) assert(frame[1]==3);
            assert(releaseProfileCandidate(id)); continue;
        }
        candidateReply(id,words({10,100,100,30,0,7}));
        if(fault==0) { // Accepted bytes, no physical TX completion.
            hardware.txCharacterUs=0; candidateWait(id);
        } else if(fault==1) candidateWait(id); // Lost FC10 ACK.
        else if(fault==5) {
            auto malformed=crc({1,16,0,0x21,0,5}); malformed.back()^=0x80; candidateReply(id,malformed);
        }
        else {
            candidateReply(id,crc({1,16,0,0x21,0,5}));
            auto readback=words({100,200,60,0,101});
            if(fault==3) readback.back()^=0x80;
            candidateReply(id,readback);
        }
        const auto result=candidateResult(id);
        assert(!result.stage.staged && result.stage.writeEvidence.txAccepted);
        if(fault<2 || fault==5) assert(result.stage.execution==ActionExecution::UNKNOWN);
        const auto frames=hardware.writes; candidatePump(1000);
        uint32_t unchanged=777; assert(!submitProfileCandidate(c,unchanged) && unchanged==777 && hardware.writes==frames);
        assert(candidateResult(id).stage.writeEvidence.txAccepted==result.stage.writeEvidence.txAccepted);
        candidateNoMotion(); assert(releaseProfileCandidate(id));
    }
}
void testCandidateBlocksOtherReads() {
    candidateFresh(); const auto c=candidate(); const auto id=candidateSubmit(c);
    for(unsigned phase=0;phase<2;++phase) {
        for(unsigned i=0;i<25000 && !app->owner.txAccepted(app->profileCandidate.request);++i) step();
        assert(app->owner.txAccepted(app->profileCandidate.request));
        const auto frames=hardware.writes; const auto request=app->profileCandidate.request;
        for(const auto line:{"@800 read config\n","@801 read state\n","@802 probe\n","@803 motion-profile read\n"}) {
            candidateCommand(line);
            assert(Serial.output.find("\"result\":\"busy\"")!=std::string::npos);
            assert(hardware.writes==frames && app->profileCandidate.request.owner==request.owner &&
                app->profileCandidate.request.slot==request.slot && app->profileCandidate.request.generation==request.generation);
        }
        candidateReply(id,phase==0?words({10,100,100,30,0,7}):crc({1,16,0,0x21,0,5}));
    }
    candidateReply(id,words({100,200,60,0,100}));
    assert(candidateResult(id).stage.staged && releaseProfileCandidate(id)); candidateNoMotion();
}
void testCandidateDeadlineAndStaleAdmission() {
    for(unsigned mode=0;mode<3;++mode) {
        candidateFresh(); const auto id=candidateSubmit(candidate());
        const auto deadline=candidateResult(id).stage.deadlineUs;
        if(mode==1) {
            // Let the prior read settle, but leave its result for application harvest.
            const auto request=app->profileCandidate.request;
            reply(request,words({10,100,100,30,0,7}));
            for(unsigned i=0;i<25000 && !app->owner.result(request);++i) {
                advanceHardware(hardware.time+10); app->owner.service(uart.sample());
            }
            assert(app->owner.result(request));
        }
        if(mode==2) {
            candidateReply(id,words({10,100,100,30,0,7}));
            assert(!app->owner.txAccepted(app->profileCandidate.request));
            // A newer invalidated I/O observation must cancel the queued write.
            app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)].valid=false;
        } else advanceHardware(deadline);
        candidateWait(id); const auto result=candidateResult(id);
        assert(!result.stage.staged && result.stage.deadlineUs==deadline);
        assert(result.stage.execution==ActionExecution::NOT_TRANSMITTED && !result.stage.writeEvidence.txAccepted);
        for(const auto& frame:candidateFrames) assert(frame.size()<2 || frame[1]==3);
        if(mode==1) assert(result.priorEvidence.length==17);
        assert(releaseProfileCandidate(id));
    }
}
void testCandidateStopRecoveryAndResultPressure() {
    for(bool recovery:{false,true}) {
        candidateFresh();
        std::vector<uint32_t> retained;
        for(unsigned i=0;i<REQUEST_CAPACITY;++i) {
            candidateCommand("@700 read identity\n"); const auto id=app->latestOperationId;
            candidateReadReply(id,words({0x4EEA,0x29,1,0})); candidatePump();
            assert(!candidateView(id).pending); retained.push_back(id);
        }
        const auto c=candidate(); const auto id=candidateSubmit(c);
        candidateReply(id,words({10,100,100,30,0,7}));
        candidateReply(id,crc({1,16,0,0x21,0,5}));
        reply(app->profileCandidate.request,words({100,200,60,0,100}));
        uint32_t operation=0;
        if(recovery) assert(recover(app,711,operation)==Probe::Action::OK);
        else {
            StopPolicy stop; stop.behavior=StopBehavior::DIRECT;
            assert(submitStop(1,stop,operation));
        }
        candidateWait(id);
        const auto result=candidateResult(id);
        assert(!result.stage.staged && result.stage.cancelled && result.stage.execution==ActionExecution::ACKNOWLEDGED);
        assert(result.stage.writeEvidence.txAccepted==19 && !app->rememberedMoveGeneration);
        if(!recovery) {
            assert(result.stage.readbackKnown && result.stage.readEvidence.length==15);
            auto* stop=findRecord(*app,operation); assert(stop);
            reply(stop->requestId,crc({1,6,0,0x27,2,0}));
            for(unsigned i=0;i<25000 && stop->action.step==0;++i) step();
            reply(stop->requestId,words({0,1}));
            for(unsigned i=0;i<25000 && !terminal(*app,*stop);++i) step();
            ActionResult stopped; assert(copyActionResult(operation,stopped) && !stopped.pending);
            assert(stopped.action.completion==ActionCompletion::OBSERVED && releaseOperation(operation));
        } else {
            for(unsigned i=0;i<250000 && app->owner.recovering();++i) step();
            assert(!app->owner.recovering());
        }
        for(const auto old:retained) { assert(!candidateView(old).pending); assert(releaseOperation(old)); }
        assert(candidateResult(id).stage.writeEvidence.txAccepted==19 && releaseProfileCandidate(id));
        candidateNoMotion(!recovery);
    }
}
void testCandidateNeedsNewRelationAndKeepsMotionLimits() {
    for(bool abovePolicy:{false,true}) {
        candidateFresh(); auto c=candidate(); c.words[2]=abovePolicy?3000:60;
        PositionStartSpeedRelation relation;
        relation.target=app->axis.target; relation.configurationGeneration=app->axis.generation;
        relation.rawStartSpeed=10; relation.minimumPositionSpeedRpm=1; relation.qualifiedUs=hardware.time;
        assert(installPositionStartSpeedRelation(relation));
        const auto id=candidateSubmit(c); candidateComplete(id,c); assert(releaseProfileCandidate(id));
        assert(app->positionStartSpeedRelationRequired && !app->positionStartSpeedRelation.qualifiedUs);
        candidateReadiness();
        Probe::MotionProfileView profile;
        assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,profile)==Probe::Action::OK);
        fixtureReply(words({10,c.words[0],c.words[1],c.words[2],c.words[3],c.words[4]}));
        PositionRequest position; position.value=Rational(100); position.unit=PositionUnit::STEPS;
        position.frame=CoordinateFrame::NATIVE; position.relative=true; position.basis=RelativeBasis::ACTUAL;
        position.configurationGeneration=app->axis.generation;
        const auto frames=hardware.writes; uint32_t move=777;
        assert(!submitMove(position,c.words[2],move) && move==777 && hardware.writes==frames);
        relation.target=app->axis.target; relation.configurationGeneration=app->axis.generation; relation.qualifiedUs=hardware.time;
        assert(installPositionStartSpeedRelation(relation));
        if(abovePolicy) assert(!submitMove(position,c.words[2],move) && move==777 && hardware.writes==frames);
        else {
            assert(submitMove(position,c.words[2],move));
            assert(cancel(app,move)==Probe::Action::OK);
            for(unsigned i=0;i<25000 && !terminal(*app,*findRecord(*app,move));++i) step();
            assert(!findRecord(*app,move)->move.stagingEvidence.txAccepted && hardware.writes==frames);
        }
        candidateNoMotion();
    }
}
void candidatePreservesSnapshotEvidence(bool restored) {
    candidateFresh();
    if(restored) {
        Probe::MotionProfileView view;
        assert(motionProfileCommand(app,Probe::MotionProfileCommand::RESTORE,view)==Probe::Action::OK);
        fixtureReply(crc({1,16,0,0x21,0,5})); fixtureReply(words({10,100,100,30,0,7}));
        assert(app->motionProfile.view.ok && app->motionProfile.view.restored);
        candidateReadiness();
    }
    const auto historical=app->motionProfile.view;
    const auto c=candidate(); const auto id=candidateSubmit(c); candidateComplete(id,c);
    Probe::MotionProfileView inspected;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::INSPECT,inspected)==Probe::Action::OK);
    assert(inspected.saved && !inspected.ok && !inspected.restored);
    assert(inspected.closureQualified==historical.closureQualified &&
        inspected.closureEarliestUs==historical.closureEarliestUs && inspected.closureLatestUs==historical.closureLatestUs);
    assert(inspected.rxLength==historical.rxLength && !std::memcmp(inspected.rx,historical.rx,historical.rxLength));
    assert(!std::memcmp(inspected.original,historical.original,sizeof(historical.original)) &&
        !std::memcmp(inspected.current,historical.current,sizeof(historical.current)));
    assert(inspected.writeTxAccepted==historical.writeTxAccepted && inspected.writeReplyLength==historical.writeReplyLength &&
        inspected.writeQualified==historical.writeQualified && inspected.writeExecutionUnknown==historical.writeExecutionUnknown &&
        !std::memcmp(inspected.writeReply,historical.writeReply,historical.writeReplyLength));
    assert(releaseProfileCandidate(id)); candidateNoMotion();
}
void testCandidateUnknownObservedWordOrder() {
    candidateFresh(static_cast<ESS::WordOrder>(2));
    assert(!app->configuration.wordOrderKnown && app->motionProfile.view.saved);
    const auto raw=app->motionProfile.view; const auto frames=hardware.writes;
    Probe::MotionProfileView out;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::RESTORE,out)==Probe::Action::UNAVAILABLE);
    uint32_t unchanged=777;
    assert(!submitProfileCandidate(candidate(),unchanged) && unchanged==777);
    assert(hardware.writes==frames && !app->owner.pending());
    assert(!std::memcmp(raw.original,app->motionProfile.view.original,sizeof(raw.original)) &&
        raw.rxLength==app->motionProfile.view.rxLength &&
        !std::memcmp(raw.rx,app->motionProfile.view.rx,raw.rxLength));
}
void emitSnapshotAfterCandidate(bool restored) {
    candidatePreservesSnapshotEvidence(restored);
    candidateCommand("@991 motion-profile inspect\n");
    assert(Serial.output.find("\"command\":\"motion-profile\"")!=std::string::npos);
    std::printf("%s",Serial.output.c_str());
}
std::string pendingCandidateStop(const std::string& phase) {
    candidateFresh();
    candidateCommand("@604 motion-profile stage 100 200 60 0 100 0 1\n");
    assert(Serial.output.find("\"result\":\"accepted\"")!=std::string::npos);
    const auto id=app->latestOperationId;
    const auto deadline=candidateResult(id).stage.deadlineUs;
    if(phase!="prior") candidateReply(id,words({10,100,100,30,0,7}));
    if(phase=="readback") candidateReply(id,crc({1,16,0,0x21,0,5}));
    assert(phase=="prior" || phase=="write" || phase=="readback");
    for(unsigned i=0;i<25000 && !app->owner.txAccepted(app->profileCandidate.request);++i) step();
    assert(app->owner.txAccepted(app->profileCandidate.request));
    candidateCommand("@605 stop normal\n");
    assert(Serial.output.find("\"result\":\"accepted\"")!=std::string::npos && app->latestOperationId!=id);
    const auto result=candidateResult(id);
    assert(result.stage.pending && result.stage.cancelled && !result.stage.staged);
    assert(!std::strcmp(result.error,"cancelled") && result.stage.deadlineUs==deadline);
    assert(result.stage.execution==(phase=="prior" ? ActionExecution::NOT_TRANSMITTED :
        phase=="write" ? ActionExecution::UNKNOWN : ActionExecution::ACKNOWLEDGED));
    assert(!releaseProfileCandidate(id));
    candidateCommand("@990 result "+std::to_string(id)+"\n");
    assert(Serial.output.find("\"profile_stage\":true")!=std::string::npos);
    candidateNoMotion(true);
    return Serial.output;
}
void emitCandidateResult(const std::string& scenario) {
    if(scenario.compare(0,13,"pending-stop-")==0) {
        const auto output=pendingCandidateStop(scenario.substr(13));
        std::printf("%s",output.c_str()); return;
    }
    candidateFresh(); auto c=candidate();
    if(scenario=="unchanged") { c.words[1]=100; c.words[2]=30; c.words[4]=7; }
    const auto id=candidateSubmit(c);
    if(scenario=="success") candidateComplete(id,c);
    else if(scenario=="malformed-prior") {
        auto response=words({10,100,100,30,0,7}); response.back()^=0x80; candidateReply(id,response);
    } else {
        candidateReply(id,words({10,100,100,30,0,7}));
        if(scenario=="lost-ack") candidateWait(id);
        else if(scenario=="malformed-write") {
            auto response=crc({1,16,0,0x21,0,5}); response.back()^=0x80; candidateReply(id,response);
        } else if(scenario=="malformed-readback") {
            candidateReply(id,crc({1,16,0,0x21,0,5}));
            auto response=words({100,200,60,0,100}); response.back()^=0x80; candidateReply(id,response);
        }
        else if(scenario=="cancelled") {
            candidateReply(id,crc({1,16,0,0x21,0,5}));
            reply(app->profileCandidate.request,words({100,200,60,0,100}));
            assert(cancel(app,id)==Probe::Action::OK); candidateWait(id);
        } else assert(scenario=="unchanged" && candidateResult(id).unchanged);
    }
    candidateCommand("@990 result "+std::to_string(id)+"\n");
    assert(Serial.output.find("\"profile_stage\":true")!=std::string::npos);
    std::printf("%s",Serial.output.c_str());
}
}
int main(int argc,char** argv) {
    if((argc==2 || argc==3) && std::string(argv[1])=="--emit-profile-snapshot-after-stage") {
        assert(argc==2 || std::string(argv[2])=="restored"); emitSnapshotAfterCandidate(argc==3); return 0;
    }
    if((argc==2 || argc==3) && std::string(argv[1])=="--emit-profile-stage") {
        emitCandidateResult(argc==3?argv[2]:"success"); return 0;
    }
    testCandidateAdmissionAndRetainedResults();
    testCandidateUnchangedAndWireRanges();
    testCandidateCancellationAndFailures();
    testCandidateBlocksOtherReads();
    testCandidateDeadlineAndStaleAdmission();
    testCandidateStopRecoveryAndResultPressure();
    for(const auto phase:{"prior","write","readback"}) pendingCandidateStop(phase);
    testCandidateNeedsNewRelationAndKeepsMotionLimits();
    candidatePreservesSnapshotEvidence(false);
    candidatePreservesSnapshotEvidence(true);
    testCandidateUnknownObservedWordOrder();
    captureCandidateFrames=false;
    if(app) { app->~App(); std::free(app); app=nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetUsbConsole(); platformReady=false;
    testHostTupleWaitsForProfileHarvest();
    testUncertainRestoreRequiresExplicitReadOnlySettlement();
    writeResponseConfirmed=false; // Alternate unknown-echo topology exercises normal API observation.
    resetHardware();
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, false));
    assert(app && !writeResponseConfirmed); hardware.txCharacterUs=87;
    assert(uart.startCapture(20,timing().holdUs)); refresh();
    Probe::MotionProfileView v;
    v.address = 77;
    unsigned char savedProfile[sizeof(app->motionProfile)];
    std::memcpy(savedProfile,&app->motionProfile,sizeof(savedProfile));
    const auto writesBeforeInvalidCommand = hardware.writes;
    assert(motionProfileCommand(app,static_cast<Probe::MotionProfileCommand>(255),v)==Probe::Action::INVALID);
    assert(v.address==77 && hardware.writes==writesBeforeInvalidCommand && !app->owner.pending());
    assert(!std::memcmp(savedProfile,&app->motionProfile,sizeof(savedProfile)));

    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,v)==Probe::Action::OK && v.pending);
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::FORGET,v)==Probe::Action::BUSY);
    assert(app->motionProfile.view.pending && app->motionProfile.request.owner);
    fixtureReply(words({10,100,100,30,0,7}));
    assert(app->motionProfile.view.saved && app->motionProfile.view.ok && !writeResponseConfirmed);
    auto request=MoveRequest(); request.position.value=Rational(100); request.position.configurationGeneration=app->axis.generation;
    request.speedRpm=60; request.ramp=MoveRamp::VERIFIED_CONFIGURED;
    ESS::MovePrerequisites p;
    assert(moveRequirements(*app,request,hardware.time,p) && p.negativeTwosComplementVerified);
    request.position.value=Rational(-100); assert(moveRequirements(*app,request,hardware.time,p)); // HW-13
    request.position.value=Rational(100); app->stateCache.blocks[1].value.inputs[1]=true;
    assert(!moveRequirements(*app,request,hardware.time,p)); app->stateCache.blocks[1].value.inputs[1]=false;
    uint32_t operation=0;
    assert(startMove(app,2,1,request,operation)==Probe::Action::OK);
    auto* r=findRecord(*app,operation); assert(r);
    // A copied-traffic observer under full console pressure cannot consume or
    // mutate the admitted move, its result or its bus queue.
    Probe::DebugSnapshot debug; Probe::DebugMode mode=Probe::DebugMode::RAW;
    assert(debugCommand(app,&mode,debug)==Probe::Action::OK);
    const auto queued=app->owner.pending(); const auto originalRequest=r->requestId;
    const auto outputCount=app->outputCount; const bool pendingOutput=app->console.outputPending();
    const auto dropped=app->debug.state.dropped;
    const uint8_t diagnostic[]={1,3,0,0,0,1,0x84,0x0A};
    app->debug.capture.begin(hardware.time); app->debug.capture.transmitted(diagnostic,sizeof(diagnostic),true,hardware.time);
    app->outputCount=OUTPUT_LINES-2;
    app->debug.service(app->console, app->outputCount >= OUTPUT_LINES-2);
    app->outputCount=outputCount;
    assert(app->debug.state.dropped==dropped+1 && app->owner.pending()==queued);
    assert(r->requestId.owner==originalRequest.owner && r->requestId.generation==originalRequest.generation && r->requestId.slot==originalRequest.slot);
    assert(app->owner.result(r->requestId)==nullptr && app->console.outputPending()==pendingOutput);

    // Ring loss and deliberate mode-change skips are distinct from copies that
    // were observed but could not be displayed. Changing mode flushes partial RX
    // before skipping it, preserving it for independent readers.
    const auto beforeDebug=app->debug.state;
    for (unsigned i=0;i<20;++i) app->debug.capture.event(TrafficKind::END,hardware.time);
    app->outputCount=OUTPUT_LINES-2;
    app->debug.service(app->console, app->outputCount >= OUTPUT_LINES-2);
    app->outputCount=outputCount;
    assert(app->debug.state.missed==beforeDebug.missed+4);
    assert(app->debug.state.observed==beforeDebug.observed+1);
    assert(app->debug.state.dropped==beforeDebug.dropped+1);
    app->debug.capture.received(0xAB,hardware.time-1,hardware.time,0,hardware.time);
    mode=Probe::DebugMode::DECODED;
    assert(debugCommand(app,&mode,debug)==Probe::Action::OK);
    assert(debug.skipped==beforeDebug.skipped+16 && !app->debug.requestKnown);
    assert(debug.emitted+debug.dropped==debug.observed);
    assert(debug.observed+debug.missed+debug.skipped==debug.cursor);
    uint64_t independent=0; TrafficRecord copied; bool partialPreserved=false;
    while (app->debug.capture.copyAfter(independent,copied))
        if (copied.kind==TrafficKind::RX && copied.length==1 && copied.bytes[0]==0xAB)
            partialPreserved=!copied.complete;
    assert(partialPreserved && app->owner.pending()==queued);

    reply(r->requestId,crc({1,16,0,0x21,0,5}));
    for (unsigned i=0;i<25000 && r->move.step==0;++i) step();
    assert(r->move.step==1);
    // An unconfirmed FC06 echo advances only to observations, retaining UNKNOWN.
    reply(r->requestId,crc({1,6,0,0x27,0,1}));
    for (unsigned i=0;i<25000 && r->move.step==1;++i) step();
    assert(r->move.execution==ActionExecution::UNKNOWN && r->move.step==2);
    reply(r->requestId,words({0,4}));
    for (unsigned i=0;i<25000 && r->move.step==2;++i) step();
    reply(r->requestId,words({0,1}));
    for (unsigned i=0;i<25000 && r->move.state==ActionState::ACTIVE;++i) step();
    assert(r->move.state==ActionState::SUCCEEDED && r->move.execution==ActionExecution::UNKNOWN);
    // Retained results are explicit; clear via observed urgent stop, never replay.
    ActionRequest stop; stop.kind=ActionKind::STOP; stop.stop.behavior=StopBehavior::CONFIGURED_DECELERATION; uint32_t stopped=0;
    assert(startAction(app,3,1,stop,stopped)==Probe::Action::OK);
    auto* st=findRecord(*app,stopped); reply(st->requestId,crc({1,6,0,0x27,1,0}));
    for (unsigned i=0;i<25000 && st->action.step==0;++i) step();
    reply(st->requestId,words({0,1}));
    for (unsigned i=0;i<25000 && st->action.state==ActionState::ACTIVE;++i) step();
    assert(st->action.state==ActionState::SUCCEEDED && st->action.execution==ActionExecution::UNKNOWN);
    r->delivered=st->delivered=true; // Direct callback fixture owns terminal consumption.
    assert(release(app,operation)==Probe::Action::OK); assert(release(app,stopped)==Probe::Action::OK);
    refresh();
    app->stateCache.blocks[2].value.rawPosition=99;
    request.position.relative=false; request.position.value=Rational(0);
    assert(absoluteMoveReady(*app,request,hardware.time));
    assert(moveRequirements(*app,request,hardware.time,p));
    app->stateCache.blocks[2].value.rawSpeed=23;
    assert(!absoluteMoveReady(*app,request,hardware.time));
    app->stateCache.blocks[2].value.rawSpeed=0;
    app->stateCache.blocks[2].value.rawPosition=0;
    assert(absoluteMoveReady(*app,request,hardware.time));
    app->stateCache.blocks[2].value.rawPosition=251;
    assert(absoluteMoveReady(*app,request,hardware.time));
    app->stateCache.blocks[2].value.rawSpeed=1;
    p.startSpeed = 777;
    unsigned char savedPrerequisites[sizeof(p)]; std::memcpy(savedPrerequisites,&p,sizeof(p));
    assert(!moveRequirements(*app,request,hardware.time,p));
    assert(!std::memcmp(savedPrerequisites,&p,sizeof(p)));

    app->stateCache.blocks[2].value.rawPosition=99;
    app->stateCache.blocks[2].value.rawSpeed=0;
    request.position.value=Rational(251);
    assert(absoluteMoveReady(*app,request,hardware.time));
    uint32_t returning=0;
    request.position.value=Rational(2147483648LL);
    assert(startMove(app,4,1,request,returning)==Probe::Action::INVALID && returning==0);
    request.position.value=Rational(501,2);
    assert(!absoluteMoveReady(*app,request,hardware.time));
    request.position.value=Rational(100);
    assert(absoluteMoveReady(*app,request,hardware.time));
    assert(startMove(app,4,1,request,returning)==Probe::Action::OK);
    assert(!findRecord(*app,returning)->move.prepared.displacementKnown);
    assert(!findRecord(*app,returning)->move.reference.nativeKnown);
    assert(findRecord(*app,returning)->move.prepared.effectiveNative==100);
    assert(cancel(app,returning)==Probe::Action::OK);
    for (unsigned i=0;i<1000 && !terminal(*app,*findRecord(*app,returning));++i) step();
    findRecord(*app,returning)->delivered=true;
    assert(release(app,returning)==Probe::Action::OK);
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::RESTORE,v)==Probe::Action::OK);
    fixtureReply(crc({1,16,0,0x21,0,5})); assert(app->motionProfile.view.phase==Probe::MotionProfilePhase::READBACK);
    fixtureReply(words({10,100,100,30,0,7})); assert(app->motionProfile.view.restored && app->motionProfile.view.ok);
    // Failed read-only refresh must not turn a known restoration write into
    // an unknown write on the following successful explicit refresh.
    const auto knownWrite=app->motionProfile.view;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,v)==Probe::Action::OK);
    auto malformedRefresh=words({10,100,100,30,0,7}); malformedRefresh.back()^=0x80;
    fixtureReply(malformedRefresh);
    assert(app->motionProfile.view.executionUnknown && !app->motionProfile.view.writeExecutionUnknown);
    // Exercise transport-only recovery independently of the application's
    // deliberate endpoint-generation change, keeping this snapshot binding.
    uint64_t refreshRecovery=0;
    assert(app->owner.recover(nowUs(),nowUs()+REQUEST_US,refreshRecovery)==Rtu::RecoveryAdmission::ACCEPTED);
    assert(uart.clear());
    for(unsigned i=0;i<50000 && app->owner.recovering();++i) {
        advanceHardware(hardware.time+10); app->owner.service(uart.sample(),true);
    }
    assert(!app->owner.needsRecovery());
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,v)==Probe::Action::OK);
    fixtureReply(words({10,100,100,30,0,7}));
    assert(app->motionProfile.view.ok && !app->motionProfile.view.executionUnknown &&
        app->motionProfile.view.writeDeadlineUs==knownWrite.writeDeadlineUs &&
        !std::memcmp(app->motionProfile.view.writeTx,knownWrite.writeTx,sizeof(knownWrite.writeTx)));
    // A cancelled unsent restore is still a retained attempt, with its original
    // request and zero accepted bytes. Ordinary reads do not erase its history.
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::RESTORE,v)==Probe::Action::OK);
    assert(app->owner.cancelUnsent(app->motionProfile.request,nowUs()));
    serviceMotionProfile(*app,nowUs());
    const auto unsent=app->motionProfile.view;
    assert(unsent.writeTxLength && !unsent.writeTxAccepted && !unsent.restoreUnsettled);
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,v)==Probe::Action::OK);
    fixtureReply(words({10,100,100,30,0,7}));
    assert(app->motionProfile.view.ok && !app->motionProfile.view.executionUnknown &&
        app->motionProfile.view.writeTxLength==unsent.writeTxLength &&
        app->motionProfile.view.writeDeadlineUs==unsent.writeDeadlineUs &&
        !std::memcmp(app->motionProfile.view.writeTx,unsent.writeTx,sizeof(unsent.writeTx)));
    // Refresh preserves originals; a second profile read cannot overwrite them.
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,v)==Probe::Action::OK);
    fixtureReply(words({10,100,100,60,0,100})); assert(app->motionProfile.view.original[3]==30 && app->motionProfile.view.current[3]==60);
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::RESTORE,v)==Probe::Action::OK);
    fixtureReply(crc({1,16,0,0x21,0,5})); fixtureReply(words({10,100,100,60,0,100}));
    assert(!app->motionProfile.view.ok && !app->motionProfile.view.restored && !std::strcmp(app->motionProfile.view.error,"readback_mismatch"));
    assert(app->motionProfile.view.restoreUnsettled);
    const auto writesBeforeSettlement=hardware.writes;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::FORGET,v)==Probe::Action::BUSY);
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,v)==Probe::Action::OK);
    fixtureReply(words({10,100,100,30,0,7}));
    assert(app->motionProfile.view.restored && !app->motionProfile.view.restoreUnsettled && hardware.writes==writesBeforeSettlement+1);
    Serial.input="@99 motion-profile inspect\n";
    for (unsigned i=0;i<1000;++i) step();
    assert(Serial.output.find("\"command\":\"motion-profile\"")!=std::string::npos);
    const auto stale=app->motionProfile;
    const unsigned writes=hardware.writes, resets=hardware.rxResets, configs=hardware.configCalls;
    assert(selectTarget(app,2)==Probe::Action::OK && selectTarget(app,1)==Probe::Action::OK);
    refresh();
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::RESTORE,v)==Probe::Action::UNAVAILABLE);
    assert(!std::memcmp(&stale,&app->motionProfile,sizeof(stale)) && hardware.writes==writes);
    app->monitorState.settings.enabled=true;
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::FORGET,v)==Probe::Action::BUSY);
    app->monitorState.settings.enabled=false;
    assert(app->owner.beginConfiguration(nowUs()));
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::FORGET,v)==Probe::Action::BUSY);
    assert(app->owner.finishConfiguration(timing(),nowUs()));

    // A host snapshot release cannot release a lost-acknowledgement operation
    // or its physical-axis conflict, even when both belong to this address.
    auto& retained=app->records[0]; retained.operationId=600;
    retained.address=1; retained.kind=App::OperationKind::ACTION;
    assert(ESS::prepareNormalStop(retained.action,app->axis.target,600,nowUs(),nowUs()+REQUEST_US));
    ActionEvent failure; failure.transport.target=app->axis.target;
    failure.transport.operationId=600; failure.transport.kind=ReadEventKind::TRANSPORT_FAILURE;
    failure.transport.txAccepted=8; failure.transport.executionUnknown=true; failure.txComplete=true;
    assert(ESS::advanceAction(retained.action,failure,nowUs()));
    updateActionReservation(*app,retained);
    const auto uncertain=retained.action;
    assert(axisReserved(*app,1));
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::FORGET,v)==Probe::Action::OK);
    assert(!v.saved && !v.pending && axisReserved(*app,1));
    Probe::ResultView result;
    assert(lookup(app,600,result) && result.actionContext->execution==ActionExecution::UNKNOWN);
    assert(!std::memcmp(&uncertain,result.actionContext,sizeof(uncertain)));
    assert(hardware.writes==writes && hardware.rxResets==resets && hardware.configCalls==configs);
    assert(motionProfileCommand(app,Probe::MotionProfileCommand::SNAPSHOT,v)==Probe::Action::OK && v.pending);
    fixtureReply(words({10,100,100,60,0,100}));
    assert(app->motionProfile.view.saved && app->motionProfile.view.original[3]==60 && axisReserved(*app,1));
}
