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
#include <string>
#include <vector>

namespace {
using namespace MotorControlRS;
uint16_t registers[5][0x140]; // Drives 1, 2 and 4 (wired bench 06 D9).
unsigned runningReads[5], served;
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
    assert(frame.size() >= 8 && frame[0] >= 1 && frame[0] <= 4 && frame[0] != 3);
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
    for (uint8_t address : {uint8_t(1), uint8_t(2), uint8_t(4)}) {
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
void assignmentInspectionUsesExactReads() {
    for (bool refreshSettings : {false,true}) {
        fresh(); wiring(0,InputWiring::CONNECTED); ioBaseline();
        auto q = facts(0); q.function = 7; assert(installIoQualification(q));
        assert(valid(0));
        if (refreshSettings) settingsRead(ESS::DriverGroup::IO);
        else read(ESS::ReadKind::STATE);
        IoQualification retained; bool current = true;
        assert(copyIoQualification(1,0,IoPurpose::ASSIGN,retained,current));
        assert(!current && retained.settingsOperationId == q.settingsOperationId &&
            retained.stateOperationId == q.stateOperationId && retained.qualifiedUs == q.qualifiedUs);
        const auto before = frames.size();
        Serial.output.clear(); Serial.input = "@77 io qualification inspect 1 x0 assign\n";
        until([] { return Serial.input.empty() && Serial.output.find("\"bus_traffic\":false}") != std::string::npos; });
        assert(Serial.output.find("\"command\":\"io-qualification-inspect\"") != std::string::npos);
        assert(Serial.output.find("\"present\":true,\"valid\":false") != std::string::npos);
        assert(frames.size() == before);
        refused(input(7));
    }
    for (auto purpose : {IoPurpose::CONFIGURE,IoPurpose::FINITE_MOVE,IoPurpose::SEGMENT}) {
        fresh(); read(ESS::ReadKind::CONFIG); read(ESS::ReadKind::STATE);
        auto q = facts(0,purpose); assert(installIoQualification(q));
        read(ESS::ReadKind::STATE);
        IoQualification retained; bool current = false;
        assert(copyIoQualification(1,0,purpose,retained,current) && current);
        assert(retained.stateOperationId == q.stateOperationId && retained.qualifiedUs == q.qualifiedUs);
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
ESS::DriverRequest polarityOnly(uint16_t mask) {
    ESS::DriverRequest request; request.group = ESS::DriverGroup::IO;
    request.configurationGeneration = app->axis.generation;
    request.fields = static_cast<uint32_t>(ESS::DriverField::INPUT_POLARITY); request.inputPolarity = mask;
    return request;
}
// Wired bench 05: a connected no-function terminal may be inverted with stated
// facts and UNKNOWN mapping, both ways and for both status outcomes. A function
// on either side, absent facts or a combined assignment stay refused.
void connectedNoFunctionInversion() {
    for (uint8_t terminal = 0; terminal < 4; ++terminal)
        for (uint16_t from : {uint16_t(0),uint16_t(1)})
            for (uint16_t level : {uint16_t(0),uint16_t(1)}) {
                if (!from && level) continue; // Default polarity, source off: the bit reads 0.
                fresh(); wiring(terminal,InputWiring::CONNECTED);
                registers[1][0x40] = static_cast<uint16_t>(from << terminal);
                registers[1][8] = static_cast<uint16_t>(level << terminal); ioBaseline();
                const auto request = polarityOnly(static_cast<uint16_t>((from ^ 1) << terminal));
                refused(request); // Connected: no facts, no write.
                auto q = facts(terminal); q.function = 0; q.polarity = from ^ 1;
                q.input.statusMapping = ESS::IoStatusMapping::UNKNOWN; q.input.externalTriggersInhibited = false;
                assert(installIoQualification(q));
                uint32_t id = 0; assert(update(request,id));
                until([&] { return !result(id).pending; });
                const auto& context = *result(id).driverContext;
                assert(context.outcome == ESS::DriverOutcome::SUCCESS && context.progress[7].acknowledged);
                assert(registers[1][0x40] == ((from ^ 1) << terminal));
            }
    for (uint16_t function : {uint16_t(1),uint16_t(2),uint16_t(7),uint16_t(14)}) {
        // A stored function: the polarity record and the write are refused.
        fresh(); wiring(0,InputWiring::CONNECTED); registers[1][0x41] = function; ioBaseline();
        auto q = facts(0); q.function = function; q.polarity = 1;
        q.input.statusMapping = ESS::IoStatusMapping::UNKNOWN; q.input.protectionRemovalQualified = function == 2;
        assert(!installIoQualification(q)); refused(polarityOnly(1));
        // One candidate that inverts and assigns the function: refused by the
        // application here and by the core (io_test).
        fresh(); wiring(0,InputWiring::CONNECTED); ioBaseline();
        q = facts(0); q.function = function; q.polarity = 1; q.input.statusMapping = ESS::IoStatusMapping::UNKNOWN;
        assert(!installIoQualification(q)); refused(input(function,1,true));
        if (function == 14) { // The same with a stated mapping is admitted: 14-17 are passive selectors.
            q.input.statusMapping = ESS::IoStatusMapping::AFTER_INVERSION; assert(installIoQualification(q));
            uint32_t id = 0; assert(update(input(function,1,true),id));
            until([&] { return !result(id).pending; });
            assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS);
        }
    }
    // Unconnected: the passive no-function inversion still needs no facts.
    fresh(); wiring(0,InputWiring::UNCONNECTED); ioBaseline();
    uint32_t id = 0; assert(update(polarityOnly(1),id));
    until([&] { return !result(id).pending; });
    assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS);
}
std::string consoleReply(const std::string& line) {
    Serial.output.clear(); Serial.input = line + "\n";
    until([] { return Serial.input.empty() && Serial.output.find("\"type\":\"reply\"") != std::string::npos &&
                      Serial.output.find('\n') != std::string::npos; });
    for (unsigned i = 0; i < 50; ++i) step();
    return Serial.output;
}
// Wired bench 05: the application record of the measured status mapping.
void recordedMapping() {
    const auto& record = RECORDED_INPUT_MAPPINGS[0];
    assert(sizeof(RECORDED_INPUT_MAPPINGS) / sizeof(RECORDED_INPUT_MAPPINGS[0]) == 1);
    assert(record.model == 0x4EEA && record.firmware == 0x0029 && record.sourceId == 20261119);
    for (const auto mapping : record.inputs) assert(mapping == ESS::IoStatusMapping::AFTER_INVERSION);
    fresh(); wiring(3,InputWiring::CONNECTED); registers[1][0x44] = 17; ioBaseline();
    auto mapping = ESS::IoStatusMapping::UNKNOWN;
    assert(!copyRecordedInputMapping(1,3,mapping) && mapping == ESS::IoStatusMapping::UNKNOWN); // No identity read.
    auto before = frames.size();
    auto text = consoleReply("@60 io qualification install 1 x3 17 1 assign recorded triggers-inhibited");
    assert(text.find("\"result\":\"mapping_not_recorded\"") != std::string::npos &&
           text.find("\"read_first\":\"read identity\"") != std::string::npos);
    assert(!valid(3) && frames.size() == before);
    refused(polarityOnly(8));
    read(ESS::ReadKind::IDENTITY);
    assert(copyRecordedInputMapping(1,3,mapping) && mapping == ESS::IoStatusMapping::AFTER_INVERSION);
    assert(!copyRecordedInputMapping(1,4,mapping) && !copyRecordedInputMapping(2,3,mapping)); // Outputs; another target.
    // M05: mapping unknown is refused for the PIN inversion, recorded is admitted.
    text = consoleReply("@61 io qualification install 1 x3 17 1 assign unknown triggers-inhibited");
    assert(text.find("\"ok\":false") != std::string::npos && !valid(3));
    refused(polarityOnly(8));
    text = consoleReply("@62 io qualification install 1 x3 17 1 assign recorded triggers-inhibited");
    assert(text.find("\"result\":\"done\"") != std::string::npos && valid(3));
    IoQualification installed; bool current = false;
    assert(copyIoQualification(1,3,IoPurpose::ASSIGN,installed,current) && current &&
           installed.input.statusMapping == ESS::IoStatusMapping::AFTER_INVERSION);
    uint32_t id = 0; assert(update(polarityOnly(8),id));
    until([&] { return !result(id).pending; });
    assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS && registers[1][0x40] == 8);
    // The inverse write with the record, then without facts: refused.
    fresh(); wiring(3,InputWiring::CONNECTED); registers[1][0x44] = 17; registers[1][0x40] = 8; registers[1][8] = 8;
    ioBaseline(); read(ESS::ReadKind::IDENTITY);
    refused(polarityOnly(0));
    text = consoleReply("@63 io qualification install 1 x3 17 0 assign recorded triggers-inhibited");
    assert(text.find("\"result\":\"done\"") != std::string::npos);
    assert(update(polarityOnly(0),id)); until([&] { return !result(id).pending; });
    assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS && registers[1][0x40] == 0);
    // An explicit mapping against the record is refused by the console and the API.
    fresh(); wiring(3,InputWiring::CONNECTED); registers[1][0x44] = 17; ioBaseline(); read(ESS::ReadKind::IDENTITY);
    auto q = facts(3); q.function = 17; q.polarity = 1; q.input.statusMapping = ESS::IoStatusMapping::BEFORE_INVERSION;
    assert(!installIoQualification(q) && !valid(3));
    text = consoleReply("@64 io qualification install 1 x3 17 1 assign before triggers-inhibited");
    assert(text.find("\"result\":\"mapping_contradicts_record\"") != std::string::npos && !valid(3));
    q.input.statusMapping = ESS::IoStatusMapping::AFTER_INVERSION; assert(installIoQualification(q));
    q.input.statusMapping = ESS::IoStatusMapping::UNKNOWN; q.polarity = 0; assert(installIoQualification(q)); // unknown stays allowed
    // A binding change makes the identity stale.
    fresh(); wiring(3,InputWiring::CONNECTED); registers[1][0x44] = 17; read(ESS::ReadKind::IDENTITY);
    assert(copyRecordedInputMapping(1,3,mapping));
    assert(host(app).selectTarget(app,2) == Probe::Action::OK && host(app).selectTarget(app,1) == Probe::Action::OK);
    wiring(3,InputWiring::CONNECTED); ioBaseline();
    mapping = ESS::IoStatusMapping::UNKNOWN; assert(!copyRecordedInputMapping(1,3,mapping));
    text = consoleReply("@65 io qualification install 1 x3 17 1 assign recorded triggers-inhibited");
    assert(text.find("\"read_first\":\"read identity\"") != std::string::npos);
    q = facts(3); q.function = 17; q.polarity = 1; q.input.statusMapping = ESS::IoStatusMapping::BEFORE_INVERSION;
    assert(installIoQualification(q)); // Without a fresh identity no record applies; the statement stands alone.
    // Another firmware has no record: refused without a read-first hint.
    fresh(); wiring(3,InputWiring::CONNECTED); registers[1][1] = 0x2A; registers[1][0x44] = 17; ioBaseline();
    read(ESS::ReadKind::IDENTITY);
    assert(!copyRecordedInputMapping(1,3,mapping));
    text = consoleReply("@66 io qualification install 1 x3 17 1 assign recorded triggers-inhibited");
    assert(text.find("\"result\":\"mapping_not_recorded\"") != std::string::npos && text.find("read_first") == std::string::npos);
    // Group member path: the member's own checked identity from its join.
    fresh(); registers[2][0x41] = 7;
    const uint8_t address = 2; assert(declareGroup(&address,1,id));
    until([&] { Probe::GroupResult r; return copyGroupResult(id,r) && !r.pending; }); idle();
    mapping = ESS::IoStatusMapping::UNKNOWN;
    assert(copyRecordedInputMapping(2,0,mapping,true) && mapping == ESS::IoStatusMapping::AFTER_INVERSION);
    assert(!copyRecordedInputMapping(2,0,mapping,false)); // Not the selected axis.
    assert(declareGroupWiring(2,0,InputWiring::CONNECTED)); assert(submitGroupStateRead(id));
    until([&] { Probe::GroupResult r; return copyGroupResult(id,r) && !r.pending; }); idle();
    text = consoleReply("@67 io qualification install member 2 x0 7 0 finite-move recorded triggers-inhibited");
    assert(text.find("\"result\":\"done\"") != std::string::npos && valid(0,IoPurpose::FINITE_MOVE,2));
    assert(clearGroup()); assert(!copyRecordedInputMapping(2,0,mapping,true));
}
// Wired bench 06 Implement 6: the recorded Y0 output status mapping (O2:
// after inversion). `recorded` and the stated-mapping refusals work as for
// inputs; with the record a predicted polarity change admits a one-level load.
ESS::DriverRequest outputPolarityOnly(uint16_t value) {
    ESS::DriverRequest r; r.group = ESS::DriverGroup::IO;
    r.fields = static_cast<uint32_t>(ESS::DriverField::OUTPUT_POLARITY); r.outputPolarity = value;
    r.configurationGeneration = app->axis.generation; return r;
}
void recordedOutputMapping() {
    const auto& record = RECORDED_OUTPUT_MAPPINGS[0];
    assert(sizeof(RECORDED_OUTPUT_MAPPINGS) / sizeof(RECORDED_OUTPUT_MAPPINGS[0]) == 1);
    assert(record.model == 0x4EEA && record.firmware == 0x0029 && record.sourceId == 20261119);
    assert(record.outputs[0] == ESS::IoStatusMapping::AFTER_INVERSION && record.outputs[1] == ESS::IoStatusMapping::UNKNOWN);
    fresh(); wiring(4,InputWiring::CONNECTED); registers[1][0x4C] = 9; ioBaseline();
    auto mapping = ESS::IoStatusMapping::UNKNOWN;
    assert(!copyRecordedOutputMapping(1,0,mapping) && mapping == ESS::IoStatusMapping::UNKNOWN); // No identity read.
    auto before = frames.size();
    auto text = consoleReply("@70 io qualification install 1 y0 9 1 0 assign recorded control active");
    assert(text.find("\"result\":\"mapping_not_recorded\"") != std::string::npos &&
           text.find("\"read_first\":\"read identity\"") != std::string::npos && frames.size() == before);
    // Without a mapping only an either-level load admits the inversion (Implement 1).
    text = consoleReply("@71 io qualification install 1 y0 9 1 0 assign unknown control active");
    assert(text.find("\"result\":\"done\"") != std::string::npos);
    refused(outputPolarityOnly(1));
    read(ESS::ReadKind::IDENTITY);
    assert(copyRecordedOutputMapping(1,0,mapping) && mapping == ESS::IoStatusMapping::AFTER_INVERSION);
    assert(!copyRecordedOutputMapping(1,1,mapping) && !copyRecordedOutputMapping(2,0,mapping)); // Y1 none; another target.
    assert(revokeIoQualification(1,4,IoPurpose::ASSIGN));
    text = consoleReply("@72 io qualification install 1 y0 9 1 0 assign before control active");
    assert(text.find("\"result\":\"mapping_contradicts_record\"") != std::string::npos && !valid(4));
    auto q = facts(4); q.function = 9; q.polarity = 1; q.output.load = ESS::IoOutputLoad::CONTROL; q.output.allowedLevels = 2;
    q.output.statusMapping = ESS::IoStatusMapping::BEFORE_INVERSION;
    assert(!installIoQualification(q) && !valid(4));
    // With the record: custom 0 inverted is active at every write, so an active-only load admits it.
    text = consoleReply("@73 io qualification install 1 y0 9 1 0 assign recorded control active");
    assert(text.find("\"result\":\"done\"") != std::string::npos && valid(4));
    IoQualification installed; bool current = false;
    assert(copyIoQualification(1,4,IoPurpose::ASSIGN,installed,current) && current &&
           installed.output.statusMapping == ESS::IoStatusMapping::AFTER_INVERSION);
    uint32_t id = 0; assert(update(outputPolarityOnly(1),id));
    until([&] { return !result(id).pending; });
    assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS && registers[1][0x4B] == 1);
    // An inactive-only load stays refused: the predicted level is active.
    fresh(); wiring(4,InputWiring::CONNECTED); registers[1][0x4C] = 9; ioBaseline(); read(ESS::ReadKind::IDENTITY);
    text = consoleReply("@74 io qualification install 1 y0 9 1 0 assign recorded control inactive");
    assert(text.find("\"result\":\"done\"") != std::string::npos);
    refused(outputPolarityOnly(1));
    // Another firmware has no record.
    fresh(); wiring(4,InputWiring::CONNECTED); registers[1][1] = 0x2A; registers[1][0x4C] = 9; ioBaseline();
    read(ESS::ReadKind::IDENTITY);
    assert(!copyRecordedOutputMapping(1,0,mapping));
    text = consoleReply("@75 io qualification install 1 y0 9 1 0 assign recorded control active");
    assert(text.find("\"result\":\"mapping_not_recorded\"") != std::string::npos && text.find("read_first") == std::string::npos);
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
    // Final custom=1 with polarity=1 is inactive, but polarity is written first.
    fresh(); wiring(4,InputWiring::CONNECTED); registers[1][0x4C] = 9; ioBaseline();
    auto q = facts(4); q.polarity = 1; q.custom = 1; q.output.allowedLevels = 1;
    assert(installIoQualification(q));
    ESS::DriverRequest request; request.group = ESS::DriverGroup::IO;
    request.configurationGeneration = app->axis.generation;
    request.fields = static_cast<uint32_t>(ESS::DriverField::OUTPUT_POLARITY) |
        static_cast<uint32_t>(ESS::DriverField::CUSTOM_OUTPUT);
    request.outputPolarity = request.customOutput = 1;
    refused(request);
    q.output.allowedLevels = 3; assert(installIoQualification(q));
    uint32_t id = 0; assert(update(request,id));
    until([&] { return !result(id).pending; });
    const auto& context = *result(id).driverContext;
    assert(context.outcome == ESS::DriverOutcome::SUCCESS && context.progress[12].acknowledged && context.progress[15].acknowledged);
}
void unchangedPolarityReplySource() {
    for (bool inputPolarity : {false,true})
        for (bool connected : {false,true})
            for (bool confirmed : {false,true}) {
                fresh(confirmed);
                for (uint8_t terminal = 0; terminal < 6; ++terminal)
                    wiring(terminal,InputWiring::UNCONNECTED);
                if (connected) wiring(inputPolarity ? 3 : 5,InputWiring::CONNECTED);
                ioBaseline();
                ESS::DriverRequest request; request.group = ESS::DriverGroup::IO;
                request.configurationGeneration = app->axis.generation;
                request.fields = static_cast<uint32_t>(inputPolarity ? ESS::DriverField::INPUT_POLARITY : ESS::DriverField::OUTPUT_POLARITY);
                if (connected && !confirmed) { refused(request); continue; }
                uint32_t id = 0; assert(update(request,id));
                until([&] { return !result(id).pending; });
                const auto& context = *result(id).driverContext;
                const auto& progress = context.progress[inputPolarity ? 7 : 12];
                assert(context.outcome == ESS::DriverOutcome::SUCCESS && progress.readbackKnown && progress.readback == 0);
                assert(progress.acknowledged == confirmed);
                assert(progress.execution == (confirmed ? ActionExecution::ACKNOWLEDGED : ActionExecution::UNKNOWN));
            }
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
// Wired bench 14: FINITE_MOVE records also state inhibition of 8-10 and 13 (the
// velocity and PV arms, serial velocity); an active one refuses like any control.
bool finiteAllowed(uint16_t function, bool active) {
    return !(active && function >= 2 && function <= 13);
}
// Serial moves keep refusing a speed, JOG or PV input whatever its record states.
bool moveAllowed(uint16_t function, bool active) {
    return finiteAllowed(function, active) && !(function >= 8 && function <= 10) && function != 13;
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
            assert(accepted.isOk() == moveAllowed(function,active != 0));
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
            assert(accepted.isOk() == moveAllowed(function,active != 0));
            if (!accepted) { assert(id == 777 && frames.size() == before); continue; }
            until([&] { Probe::GroupResult r; return copyGroupResult(id,r) && !r.pending; });
            Probe::GroupResult done; assert(copyGroupResult(id,done) && done.ok);
        }
}
// Wired bench 06 Implement 3: the D9 experiment ran on image A (Y0 = 10 followed
// custom bit 1, Y0 = 11 followed experimental bit 2; ledger notes) and its
// admission is removed. Production admission is unchanged: function 11, cross-
// mapped custom bits and reserved custom bits stay refused before any frame.
void readAt(uint8_t address, ESS::ReadKind kind) {
    uint32_t id = 0; assert(host(app).startTypedRead(app,1,address,kind,id) == Probe::Action::OK);
    findRecord(*app,id)->programmaticRead = true; // Released here, never reported.
    until([&] { return !result(id).pending; }); assert(result(id).typedRead->state == ReadState::SUCCEEDED); idle();
    assert(releaseOperation(id));
}
void drive4Reads() {
    readAt(4,ESS::ReadKind::CONFIG);
    uint32_t id = 0; assert(submitDriverRead(4,ESS::DriverGroup::IO,id));
    until([&] { return !result(id).pending; }); assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS);
    idle(); assert(releaseOperation(id)); readAt(4,ESS::ReadKind::STATE);
}
IoQualification y0Facts(uint16_t function, uint16_t custom) {
    IoQualification q; assert(inspectIoQualification(4,4,IoPurpose::ASSIGN,q));
    q.output.supplied = true; q.output.load = ESS::IoOutputLoad::INDICATOR; q.output.allowedLevels = 3;
    q.qualifiedUs = hardware.time; q.function = function; q.custom = custom;
    return q;
}
void consoleLine(const std::string& text) {
    Serial.input = text; Serial.output.clear();
    for (unsigned i = 0; i < 2000 && !Serial.input.empty(); ++i) step();
    for (unsigned i = 0; i < 200; ++i) step();
}
void experimentRemoved() {
    fresh(); assert(host(app).selectTarget(app,4) == Probe::Action::OK);
    wiring(4,InputWiring::CONNECTED); wiring(5,InputWiring::UNCONNECTED); drive4Reads();
    auto before = frames.size();
    consoleLine("@880 io experiment d9 y0 11 4 indicator either\n");
    assert(Serial.output.find("\"ok\":false") != std::string::npos && frames.size() == before);
    consoleLine("@881 io experiment inspect\n");
    assert(Serial.output.find("\"ok\":false") != std::string::npos && frames.size() == before);
    ESS::DriverRequest eleven;
    assert(ESS::prepareOutputFunction(eleven,0,static_cast<ESS::OutputFunction>(11)).code == Err::UNSUPPORTED);
    assert(!installIoQualification(y0Facts(11,0))); // Function 11 facts stay refused.
    // Y0 = 10 with custom 0 is an ordinary assignment; every set custom bit is refused.
    assert(installIoQualification(y0Facts(10,0)));
    ESS::DriverRequest y0; assert(ESS::prepareOutputFunction(y0,0,ESS::OutputFunction::CUSTOM_1));
    y0.configurationGeneration = app->axis.generation;
    uint32_t id = 0; assert(submitDriverSettings(4,y0,id));
    until([&] { return !result(id).pending; }); idle();
    assert(result(id).driverContext->outcome == ESS::DriverOutcome::SUCCESS && registers[4][0x4C] == 10);
    assert(releaseOperation(id));
    for (uint16_t custom : {1, 2, 3, 4}) {
        drive4Reads(); installIoQualification(y0Facts(10,custom & 1));
        ESS::DriverRequest r; r.group = ESS::DriverGroup::IO; r.fields = static_cast<uint32_t>(ESS::DriverField::CUSTOM_OUTPUT);
        r.customOutput = custom; r.configurationGeneration = app->axis.generation;
        before = frames.size(); uint32_t refused = 778;
        assert(!submitDriverSettings(4,r,refused) && refused == 778);
        for (unsigned i = 0; i < 100; ++i) step();
        assert(frames.size() == before && registers[4][0x4F] == 0);
    }
}
} // namespace
int main() {
    assignmentMatrix(); publicDriverRoutes(); exactFactsAndProtection(); polarityAndInhibition();
    assignmentInspectionUsesExactReads(); connectedNoFunctionInversion(); connectedOutputsAndReplySource();
    unchangedPolarityReplySource();
    lifetimeAndUncertainty(); revokeAfterFirstWrite();
    queuedStageExpiry(); settingsPurposePolicies(); independentQualificationAges();
    groupQualificationLifetime(); finiteMoveMatrix(); recordedMapping(); recordedOutputMapping(); experimentRemoved();
    if (app) { app->~App(); std::free(app); app = nullptr; }
}
