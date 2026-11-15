// SPDX-License-Identifier: MIT
// Supplied fixture facts and register replies enter the ordinary application.
// The simulator records frames; it does not implement the admission policy.
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
#include <vector>

namespace {
using namespace MotorControlRS;
uint16_t registers[3][0x140];
unsigned runningReads[3], served;
bool loseWriteReply;
std::vector<std::vector<uint8_t>> frames;

uint16_t word(const std::vector<uint8_t>& bytes, std::size_t offset) {
    return static_cast<uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
}
void crc(std::vector<uint8_t>& bytes) {
    const uint16_t value = ESS::calcCrc16(bytes.data(),bytes.size());
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8));
}
void serve() {
    if (served == hardware.writes) return;
    served = hardware.writes;
    const auto frame = hardware.tx; frames.push_back(frame);
    assert(frame.size() >= 8 && frame[0] >= 1 && frame[0] <= 2);
    const uint8_t address = frame[0]; const uint16_t first = word(frame,2);
    std::vector<uint8_t> response;
    if (frame[1] == 3) {
        const uint16_t count = word(frame,4);
        response = {address,3,static_cast<uint8_t>(count * 2)};
        for (uint16_t i = 0; i < count; ++i) {
            uint16_t value = registers[address][first+i];
            if (first+i == 7 && runningReads[address]) { --runningReads[address]; value = 4; }
            response.push_back(static_cast<uint8_t>(value >> 8)); response.push_back(static_cast<uint8_t>(value));
        }
        crc(response);
    } else if (frame[1] == 6) {
        registers[address][first] = word(frame,4);
        if (first == 0x27 && (word(frame,4) & 1)) runningReads[address] = 1;
        if (loseWriteReply) return;
        response = frame;
    } else {
        assert(frame[1] == 16);
        for (uint16_t i = 0; i < word(frame,4); ++i) registers[address][first+i] = word(frame,7+2*i);
        response.assign(frame.begin(),frame.begin()+6); crc(response);
    }
    scheduleReply(std::max(hardware.writeStarted + frame.size()*87 + 1000,hardware.time+1000),response);
}
void step(uint32_t us = 20) { advanceHardware(hardware.time+us); serviceApplication(); serve(); }
template<class Done> void until(Done done) {
    for (unsigned i = 0; i < 400000 && !done(); ++i) step();
    assert(done());
}
void idle() {
    until([] { return !app->owner.active() && !app->owner.pending() && !app->runner.busy() && !reading(*app) && !acting(*app); });
    for (unsigned i = 0; i < 100; ++i) step();
}
Probe::ResultView result(uint32_t id) { Probe::ResultView view; assert(host(app).result(app,id,view)); return view; }
void fresh(bool confirmed = true, uint32_t ageMs = 0) {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = false;
    std::memset(registers,0,sizeof(registers)); std::memset(runningReads,0,sizeof(runningReads));
    frames.clear(); served = 0; loseWriteReply = false;
    for (uint8_t address = 1; address <= 2; ++address) {
        auto& r = registers[address];
        r[0] = 0x4EEA; r[1] = 0x29; r[2] = address; r[7] = 1;
        r[0x11] = 1000; r[0x13] = address; r[0x19] = 1;
        r[0x20] = 10; r[0x21] = r[0x22] = 100; r[0x23] = 60;
        r[0x100] = 2; r[0x101] = 4000;
    }
    ApplicationOptions options; options.observationMaxAgeMs = ageMs; options.moveTimeoutMs = 3000;
    assert(beginApplication({Board::kRs485TxPin,Board::kRs485RxPin,Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh},confirmed,options));
    hardware.txCharacterUs = 87; assert(uart.startCapture(20,timing().holdUs));
}
void wiring(uint8_t terminal, InputWiring state) {
    Probe::WiringRequest request; request.terminal = terminal; request.state = state;
    Probe::WiringSnapshot snapshot;
    assert(host(app).wiring(app,&request,snapshot) == Probe::Action::OK);
}
void read(ESS::ReadKind kind) {
    uint32_t id = 0; assert(host(app).startTypedRead(app,1,1,kind,id) == Probe::Action::OK);
    until([&] { return !result(id).pending; }); assert(result(id).typedRead->state == ReadState::SUCCEEDED); idle();
}
void ioBaseline() {
    uint32_t id = 0; assert(submitDriverRead(1,ESS::DriverGroup::IO,id));
    until([&] { return !result(id).pending; }); assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS);
    idle(); read(ESS::ReadKind::STATE);
}
void settingsRead(ESS::DriverGroup group) {
    uint32_t id = 0; assert(submitDriverRead(1,group,id,group == ESS::DriverGroup::POSITION_SEGMENT ? 1 : 0));
    until([&] { return !result(id).pending; }); assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS); idle();
}
IoQualification facts(uint8_t terminal, IoPurpose purpose = IoPurpose::ASSIGN, uint8_t address = 1) {
    IoQualification q; const auto inspected = inspectIoQualification(address,terminal,purpose,q,address != 1);
    if (!inspected) std::fprintf(stderr,"IO inspect: address=%u terminal=%u purpose=%u time=%llu\n",
        address,terminal,unsigned(purpose),static_cast<unsigned long long>(hardware.time));
    assert(inspected);
    assert(!q.input.supplied && !q.output.supplied);
    q.qualifiedUs = hardware.time;
    if (terminal < 4) {
        q.input.supplied = true; q.input.statusMapping = ESS::IoStatusMapping::AFTER_INVERSION;
        q.input.externalTriggersInhibited = true;
    } else {
        q.output.supplied = true; q.output.statusMapping = ESS::IoStatusMapping::AFTER_INVERSION;
        q.output.load = q.wiring == InputWiring::CONNECTED ? ESS::IoOutputLoad::INDICATOR : ESS::IoOutputLoad::NO_LOAD;
        q.output.allowedLevels = 3;
    }
    return q;
}
bool valid(uint8_t terminal, IoPurpose purpose = IoPurpose::ASSIGN, uint8_t address = 1) {
    IoQualification q; bool current = false;
    return copyIoQualification(address,terminal,purpose,q,current,address != 1) && current;
}
ESS::DriverRequest input(uint16_t function, uint16_t polarity = 0, bool changePolarity = false) {
    ESS::DriverRequest request;
    assert(ESS::prepareInputFunction(request,0,static_cast<ESS::InputFunction>(function)));
    request.configurationGeneration = app->axis.generation;
    if (changePolarity) { request.fields |= static_cast<uint32_t>(ESS::DriverField::INPUT_POLARITY); request.inputPolarity = polarity; }
    return request;
}
Status update(const ESS::DriverRequest& request, uint32_t& id) {
    return submitDriverSettings(1,request,id);
}
void refused(const ESS::DriverRequest& request) {
    const auto before = frames.size(); uint32_t id = 778;
    assert(!update(request,id));
    for (unsigned i = 0; i < 100; ++i) step();
    assert(id == 778 && frames.size() == before);
}
void assignmentMatrix() {
    for (uint16_t function = 0; function <= 17; ++function)
        for (auto connection : {InputWiring::UNCONNECTED,InputWiring::CONNECTED})
            for (uint16_t active : {uint16_t(0),uint16_t(1)}) {
                fresh(); wiring(0,connection); registers[1][8] = active; ioBaseline();
                auto q = facts(0); q.function = function;
                const auto request = input(function);
                if (active && function >= 4 && function <= 13) {
                    assert(!installIoQualification(q)); refused(request); continue;
                }
                assert(installIoQualification(q));
                uint32_t id = 0; assert(update(request,id));
                until([&] { return !result(id).pending; });
                const auto& context = *result(id).driverContext;
                assert(context.outcome == ESS::DriverOutcome::SUCCESS && context.progress[8].acknowledged);
                assert(registers[1][0x41] == function && !valid(0));
            }
}
void publicDriverRoutes() {
    fresh(); ioBaseline();
    const auto before = frames.size(); uint32_t id = 778;
    assert(!submitDriverSettings(2,input(7),id));
    assert(!submitDriverRead(0,ESS::DriverGroup::IO,id));
    assert(!submitDriverRead(1,ESS::DriverGroup::POSITION_SEGMENT,id,17));
    assert(id == 778 && frames.size() == before);
    ESS::DriverContext copied; copied.operationId = 777;
    assert(!copySettingsResult(0xFFFFFFFE,copied) && copied.operationId == 777);
    const auto selected = app->ioSettings.operationId;
    assert(submitDriverRead(2,ESS::DriverGroup::IO,id));
    until([&] { return copySettingsResult(id,copied) && copied.state != ReadState::ACTIVE; }); idle();
    assert(copied.target.address == 2 && copied.kind == ESS::DriverKind::READ && copied.outcome == ESS::DriverOutcome::SUCCESS);
    assert(app->ioSettings.operationId == selected && releaseOperation(id));
    assert(!copySettingsResult(id,copied));
    auto q = facts(0); q.function = 7; assert(installIoQualification(q));
    assert(submitDriverSettings(1,input(7),id));
    until([&] { return copySettingsResult(id,copied) && copied.state != ReadState::ACTIVE; }); idle();
    assert(copied.kind == ESS::DriverKind::UPDATE && copied.outcome == ESS::DriverOutcome::SUCCESS);
    assert(copied.progress[8].acknowledged && copied.progress[8].readbackKnown);
    assert(releaseOperation(id));
}
void exactFactsAndProtection() {
    fresh(); wiring(0,InputWiring::CONNECTED); ioBaseline();
    refused(input(7));
    auto q = facts(0); q.function = 7;
    auto wrong = q; ++wrong.target.generation; assert(!installIoQualification(wrong));
    wrong = q; ++wrong.configurationGeneration; assert(!installIoQualification(wrong));
    wrong = q; ++wrong.settingsOperationId; assert(!installIoQualification(wrong));
    wrong = q; ++wrong.stateOperationId; assert(!installIoQualification(wrong));
    wrong = q; wrong.previousFunction = 1; assert(!installIoQualification(wrong));
    assert(installIoQualification(q)); refused(input(11));
    assert(revokeIoQualification(1,0,IoPurpose::ASSIGN)); refused(input(7));
    for (uint16_t previous : {uint16_t(2),uint16_t(3),uint16_t(4),uint16_t(5),uint16_t(6)}) {
        fresh(); wiring(0,InputWiring::CONNECTED); registers[1][0x41] = previous; ioBaseline();
        q = facts(0); q.function = 0; assert(!installIoQualification(q)); refused(input(0));
        q.input.protectionRemovalQualified = true; assert(installIoQualification(q));
        uint32_t id = 0; assert(update(input(0),id));
        until([&] { return !result(id).pending; }); assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS);
    }
}
void polarityAndInhibition() {
    // With an inactive old trigger, polarity-first would activate it before disable.
    fresh(); registers[1][0x41] = 7; ioBaseline();
    auto q = facts(0); q.function = 0; q.polarity = 1; assert(installIoQualification(q));
    refused(input(0,1,true));
    fresh(); registers[1][0x40] = 1; ioBaseline(); q = facts(0); q.function = 7;
    q.input.statusMapping = ESS::IoStatusMapping::UNKNOWN; assert(!installIoQualification(q)); refused(input(7));
    q.input.statusMapping = ESS::IoStatusMapping::AFTER_INVERSION;
    q.input.externalTriggersInhibited = false; assert(!installIoQualification(q)); refused(input(7));
    for (uint16_t function = 14; function <= 17; ++function) {
        q.function = function; assert(!installIoQualification(q)); refused(input(function));
    }
    // BEFORE_INVERSION is an explicit fixture fact, not a mutation of published bits.
    fresh(); registers[1][0x40] = 1; registers[1][8] = 1; ioBaseline();
    q = facts(0); q.function = 7; q.input.statusMapping = ESS::IoStatusMapping::BEFORE_INVERSION;
    assert(installIoQualification(q)); uint32_t id = 0;
    assert(update(input(7),id));
    assert(result(id).driverContext->prerequisites.actualInputs == 1);
    until([&] { return !result(id).pending; }); assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS);
}
void connectedOutputsAndReplySource() {
    for (bool confirmed : {false,true}) {
        fresh(confirmed); wiring(4,InputWiring::CONNECTED); ioBaseline();
        auto q = facts(4); q.function = 1; assert(installIoQualification(q));
        ESS::DriverRequest request; assert(ESS::prepareOutputFunction(request,0,ESS::OutputFunction::ALARM));
        request.configurationGeneration = app->axis.generation;
        if (!confirmed) { refused(request); continue; }
        uint32_t id = 0; assert(update(request,id));
        until([&] { return !result(id).pending; });
        assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS);
        assert(result(id).driverContext->progress[13].acknowledged);
    }
    // The old custom output is activated by polarity before its later disable.
    fresh(); wiring(4,InputWiring::CONNECTED); registers[1][0x47] = 9; ioBaseline();
    auto q = facts(4); q.function = 0; q.polarity = 1; q.output.allowedLevels = 1;
    assert(installIoQualification(q));
    ESS::DriverRequest request; assert(ESS::prepareOutputFunction(request,0,ESS::OutputFunction::UNDEFINED));
    request.configurationGeneration = app->axis.generation;
    request.fields |= static_cast<uint32_t>(ESS::DriverField::OUTPUT_POLARITY); request.outputPolarity = 1;
    refused(request);
}
void lifetimeAndUncertainty() {
    fresh(); wiring(0,InputWiring::CONNECTED); ioBaseline();
    auto q = facts(0); q.function = 7; q.maximumAgeUs = 100000; assert(installIoQualification(q));
    advanceHardware(hardware.time+100001); read(ESS::ReadKind::STATE);
    assert(!valid(0)); refused(input(7)); // Fresh samples do not renew supplied facts.
    fresh(); wiring(0,InputWiring::CONNECTED); ioBaseline(); q = facts(0); q.function = 7;
    assert(installIoQualification(q)); uint32_t id = 0; assert(update(input(7),id));
    assert(revokeIoQualification(1,0,IoPurpose::ASSIGN));
    const auto before = frames.size(); until([&] { return !result(id).pending; });
    assert(frames.size() == before && !result(id).driverContext->effects);
    fresh(); wiring(0,InputWiring::CONNECTED); ioBaseline(); q = facts(0); q.function = 7;
    assert(installIoQualification(q)); loseWriteReply = true; assert(update(input(7),id));
    until([&] { return !result(id).pending; });
    const auto& lost = *result(id).driverContext;
    assert(lost.uncertain && lost.progress[8].execution == ActionExecution::UNKNOWN && lost.effects);
    const auto sent = frames.size(); for (unsigned i = 0; i < 100; ++i) step();
    assert(frames.size() == sent && !valid(0));
    assert(revokeIoQualification(1,0,IoPurpose::ASSIGN));
    assert(lost.uncertain && lost.effects); // Revocation never clears a transmitted effect.
}
void revokeAfterFirstWrite() {
    fresh(); wiring(0,InputWiring::CONNECTED); wiring(1,InputWiring::CONNECTED); ioBaseline();
    auto first = facts(0); first.function = 7; assert(installIoQualification(first));
    auto second = facts(1); second.function = 11; assert(installIoQualification(second));
    auto request = input(7); assert(ESS::prepareInputFunction(request,1,ESS::InputFunction::START_HOMING));
    request.configurationGeneration = app->axis.generation;
    uint32_t id = 0; assert(update(request,id));
    until([&] { return result(id).driverContext->step == 1 && app->owner.txAccepted(findRecord(*app,id)->requestId); });
    assert(!valid(0)); // Own first write has consumed the future qualification.
    const auto before = frames.size();
    assert(revokeIoQualification(1,0,IoPurpose::ASSIGN));
    until([&] { return !result(id).pending; });
    const auto& retained = *result(id).driverContext;
    assert(retained.progress[8].acknowledged && retained.progress[8].readbackKnown);
    assert(retained.progress[9].execution == ActionExecution::NOT_TRANSMITTED);
    assert(frames.size() == before && registers[1][0x42] == 0);
}
void readySelected() {
    read(ESS::ReadKind::CONFIG); read(ESS::ReadKind::STATE);
    Probe::MotionProfileView profile;
    assert(host(app).motionProfile(app,Probe::MotionProfileCommand::SNAPSHOT,profile) == Probe::Action::OK);
    until([] { return !app->motionProfile.view.pending; }); idle(); assert(app->motionProfile.view.ok);
}
bool finiteAllowed(uint16_t function, bool active) {
    if (function == 8 || function == 9 || function == 10 || function == 13) return false;
    return !(active && function >= 2 && function <= 12);
}
void queuedStageExpiry() {
    for (bool callerAge : {false,true}) {
        fresh(true,callerAge ? 0 : 1000); registers[1][0x41] = 7; readySelected();
        auto q = facts(0,IoPurpose::FINITE_MOVE);
        q.maximumAgeUs = callerAge ? 1000000 : 0; assert(installIoQualification(q));
        const auto& io = app->stateCache.blocks[static_cast<uint8_t>(ESS::StateBlock::IO)];
        const auto expiry = callerAge ? q.qualifiedUs + q.maximumAgeUs : io.observedEarliestUs + 1000000;
        PositionRequest request; request.value = Rational(10); request.frame = CoordinateFrame::NATIVE;
        request.configurationGeneration = app->axis.generation;
        const auto before = frames.size(); uint32_t id = 0;
        assert(stagePositionMove(request,60,id));
        Probe::PositionStageView staged; assert(copyPositionStage(id,staged));
        assert(staged.pending && staged.deadlineUs <= expiry && frames.size() == before);
        advanceHardware(expiry+1);
        until([&] { return copyPositionStage(id,staged) && !staged.pending; });
        assert(!staged.staged && staged.execution == ActionExecution::NOT_TRANSMITTED);
        assert(frames.size() == before); // Expired readiness cannot dispatch the queued FC10.
    }
}
ESS::DriverRequest settingsCandidate(IoPurpose purpose) {
    ESS::DriverRequest request; request.configurationGeneration = app->axis.generation;
    if (purpose == IoPurpose::CONFIGURE) {
        request.fields = static_cast<uint32_t>(ESS::DriverField::INTERRUPTION) |
            static_cast<uint32_t>(ESS::DriverField::POSITION_MODE);
        request.interruption = ESS::PvTriggerMode::RISING_EDGE; request.positionMode = ESS::PositionMode::ABSOLUTE;
    } else {
        request.group = ESS::DriverGroup::POSITION_SEGMENT;
        request.fields = static_cast<uint32_t>(ESS::DriverField::SEGMENT_SPEED); request.segmentSpeed = 60;
    }
    return request;
}
void settingsBaseline(uint16_t function, bool active) {
    registers[1][0x41] = function; registers[1][8] = active ? 1 : 0;
    read(ESS::ReadKind::CONFIG); settingsRead(ESS::DriverGroup::DRIVE);
    settingsRead(ESS::DriverGroup::POSITION_SEGMENT); read(ESS::ReadKind::STATE);
}
bool installSettingsFacts(IoPurpose purpose, bool inhibited = true) {
    bool accepted = true;
    for (uint8_t terminal = 0; terminal < 4; ++terminal) {
        auto q = facts(terminal,purpose); q.input.externalTriggersInhibited = inhibited;
        if (!installIoQualification(q)) accepted = false;
    }
    return accepted;
}
void finishSettings(const ESS::DriverRequest& request) {
    uint32_t id = 0; const auto before = frames.size(); assert(update(request,id));
    until([&] { return !result(id).pending; }); assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS);
    unsigned writes = 0;
    for (std::size_t i = before; i < frames.size(); ++i) {
        if (frames[i][1] != 6) continue;
        const uint16_t expected = request.group == ESS::DriverGroup::DRIVE ? (writes ? 0x50 : 0x51) : 0x62;
        assert(frames[i][0] == 1 && word(frames[i],2) == expected);
        assert(word(frames[i],4) == (request.group == ESS::DriverGroup::DRIVE ? 1 : 60)); ++writes;
    }
    assert(writes == (request.group == ESS::DriverGroup::DRIVE ? 2u : 1u));
}
void settingsPurposePolicies() {
    for (IoPurpose purpose : {IoPurpose::CONFIGURE,IoPurpose::SEGMENT}) {
        for (uint16_t function = 0; function <= 17; ++function)
            for (bool active : {false,true}) {
                fresh(); settingsBaseline(function,active);
                const bool allowed = !(active && function >= 2 && function <= 13);
                assert(installSettingsFacts(purpose) == allowed);
                const auto candidate = settingsCandidate(purpose);
                if (allowed) finishSettings(candidate); else refused(candidate);
            }
        for (uint16_t function : {uint16_t(7),uint16_t(11),uint16_t(12),uint16_t(14),uint16_t(15),uint16_t(16),uint16_t(17)}) {
            fresh(); settingsBaseline(function,function >= 14);
            assert(!installSettingsFacts(purpose,false)); refused(settingsCandidate(purpose));
            assert(installSettingsFacts(purpose)); finishSettings(settingsCandidate(purpose));
        }
        fresh(); settingsBaseline(7,false);
        const auto candidate = settingsCandidate(purpose);
        const auto other = purpose == IoPurpose::CONFIGURE ? IoPurpose::SEGMENT : IoPurpose::CONFIGURE;
        assert(installSettingsFacts(other)); refused(candidate);
        assert(installSettingsFacts(IoPurpose::FINITE_MOVE)); refused(candidate);
        assert(installSettingsFacts(purpose)); finishSettings(candidate);
    }
}
void independentQualificationAges() {
    // A short new caller fact does not inherit the old MOTION sample's origin.
    fresh(); registers[1][0x41] = 7; readySelected();
    advanceHardware(hardware.time+1000000);
    auto q = facts(0,IoPurpose::FINITE_MOVE); q.maximumAgeUs = 10000; assert(installIoQualification(q));
    PositionRequest intent; intent.value = Rational(10); intent.frame = CoordinateFrame::NATIVE;
    intent.configurationGeneration = app->axis.generation;
    uint32_t id = 0; const auto before = frames.size(); assert(stagePositionMove(intent,60,id));
    Probe::PositionStageView staged; assert(copyPositionStage(id,staged));
    assert(staged.deadlineUs == q.qualifiedUs+q.maximumAgeUs);
    assert(revokeIoQualification(1,0,IoPurpose::FINITE_MOVE));
    until([&] { return copyPositionStage(id,staged) && !staged.pending; });
    assert(frames.size() == before && staged.execution == ActionExecution::NOT_TRANSMITTED);

    for (bool zeroOriginExpiresFirst : {false,true}) {
        fresh(); settingsBaseline(7,false);
        uint64_t expiry = 0;
        for (uint8_t terminal = 0; terminal < 4; ++terminal) {
            auto inputFacts = facts(terminal,IoPurpose::CONFIGURE);
            if (terminal == 0) {
                inputFacts.qualifiedUs = 0; inputFacts.maximumAgeUs = zeroOriginExpiresFirst ? 1000000 : 2000000;
                expiry = inputFacts.maximumAgeUs;
            } else if (terminal == 1) {
                inputFacts.maximumAgeUs = zeroOriginExpiresFirst ? 2000000 : 100000;
                expiry = std::min(expiry,inputFacts.qualifiedUs+inputFacts.maximumAgeUs);
            }
            assert(installIoQualification(inputFacts));
        }
        const auto count = frames.size(); assert(update(settingsCandidate(IoPurpose::CONFIGURE),id));
        assert(ESS::driverWriteDeadline(*result(id).driverContext) == expiry);
        advanceHardware(expiry+1); until([&] { return !result(id).pending; });
        assert(frames.size() == count && !result(id).driverContext->effects);
    }
}
void groupQualificationLifetime() {
    fresh(); registers[2][0x41] = 7;
    const uint8_t address = 2; uint32_t id = 0; assert(declareGroup(&address,1,id));
    until([&] { Probe::GroupResult r; return copyGroupResult(id,r) && !r.pending; }); idle();
    assert(declareGroupWiring(2,0,InputWiring::UNCONNECTED)); assert(submitGroupStateRead(id));
    until([&] { Probe::GroupResult r; return copyGroupResult(id,r) && !r.pending; }); idle();
    auto q = facts(0,IoPurpose::FINITE_MOVE,2); assert(installIoQualification(q));
    assert(valid(0,IoPurpose::FINITE_MOVE,2));
    IoQualification unchanged; unchanged.function = 123; bool current = true;
    assert(!copyIoQualification(2,0,IoPurpose::FINITE_MOVE,unchanged,current));
    assert(unchanged.function == 123 && current); // A selected-axis lookup cannot borrow member facts.
    const auto before = frames.size();
    assert(declareGroupWiring(2,0,InputWiring::CONNECTED)); assert(!valid(0,IoPurpose::FINITE_MOVE,2));
    assert(!installIoQualification(q) && frames.size() == before);
    assert(submitGroupStateRead(id));
    until([&] { Probe::GroupResult r; return copyGroupResult(id,r) && !r.pending; }); idle();
    q = facts(0,IoPurpose::FINITE_MOVE,2); assert(installIoQualification(q));
    assert(clearGroup()); assert(!valid(0,IoPurpose::FINITE_MOVE,2)); assert(!installIoQualification(q));
    assert(declareGroup(&address,1,id));
    until([&] { Probe::GroupResult r; return copyGroupResult(id,r) && !r.pending; }); idle();
    assert(!valid(0,IoPurpose::FINITE_MOVE,2) && !installIoQualification(q));
}
void finiteMoveMatrix() {
    for (uint16_t function = 0; function <= 17; ++function)
        for (uint16_t active : {uint16_t(0),uint16_t(1)}) {
            fresh(); registers[1][0x41] = function; registers[1][8] = active; readySelected();
            auto q = facts(0,IoPurpose::FINITE_MOVE);
            assert(installIoQualification(q).isOk() == finiteAllowed(function,active != 0));
            const auto before = frames.size(); uint32_t id = 777;
            const auto accepted = moveBy(Rational(10),PositionUnit::STEPS,id,60);
            assert(accepted.isOk() == finiteAllowed(function,active != 0));
            if (!accepted) { assert(id == 777 && frames.size() == before); continue; }
            until([&] { MoveProgress progress; return moveProgress(id,progress) && !progress.pending; });
            MoveProgress progress; assert(moveProgress(id,progress)); assert(progress.completion == ActionCompletion::OBSERVED);
        }
    // The group member uses the same policy with its own exact facts.
    for (uint16_t function = 0; function <= 17; ++function)
        for (uint16_t active : {uint16_t(0),uint16_t(1)}) {
            fresh(); registers[2][0x41] = function; registers[2][8] = active;
            const uint8_t address = 2; uint32_t id = 0; assert(declareGroup(&address,1,id));
            until([&] { Probe::GroupResult r; return copyGroupResult(id,r) && !r.pending; }); idle();
            assert(declareGroupWiring(2,0,InputWiring::UNCONNECTED));
            assert(submitGroupStateRead(id));
            until([&] { Probe::GroupResult r; return copyGroupResult(id,r) && !r.pending; }); idle();
            auto q = facts(0,IoPurpose::FINITE_MOVE,2);
            assert(installIoQualification(q).isOk() == finiteAllowed(function,active != 0));
            Probe::GroupMoveRequest request; request.move.position.value = Rational(10);
            request.move.speedRpm = 60; request.move.ramp = MoveRamp::VERIFIED_CONFIGURED;
            request.onFailure = Probe::GroupFailurePolicy::LEAVE; request.start = Probe::GroupStartMode::STAGED;
            const auto before = frames.size(); id = 777; const auto accepted = submitGroupMove(request,id);
            assert(accepted.isOk() == finiteAllowed(function,active != 0));
            if (!accepted) { assert(id == 777 && frames.size() == before); continue; }
            until([&] { Probe::GroupResult r; return copyGroupResult(id,r) && !r.pending; });
            Probe::GroupResult done; assert(copyGroupResult(id,done) && done.ok);
        }
}
} // namespace
int main() {
    assignmentMatrix(); publicDriverRoutes(); exactFactsAndProtection(); polarityAndInhibition();
    connectedOutputsAndReplySource(); lifetimeAndUncertainty(); revokeAfterFirstWrite();
    queuedStageExpiry(); settingsPurposePolicies(); independentQualificationAges();
    groupQualificationLifetime(); finiteMoveMatrix();
    if (app) { app->~App(); std::free(app); app = nullptr; }
}
