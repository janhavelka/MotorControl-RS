// SPDX-License-Identifier: MIT
// Drive group (multi-drive 02) through the actual cooperative application, its
// console and its public API, on a fake bus with up to 15 simulated ESS drives.
// The drive simulator and every qualification here belong to this fixture only;
// they say nothing about the real bench. Both platform variants run these tests.
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
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace {
using namespace MotorControlRS;
using Policy = Probe::GroupFailurePolicy;
using Mode = Probe::GroupStartMode;

// One simulated ESS drive: plain registers plus a short RUNNING period after a start.
struct Drive {
    bool present = true;
    uint16_t regs[0x140] = {};
    unsigned runningReads = 0;
    unsigned startsApplied = 0;
    uint16_t ignoredWrite = 0; ///< Nonzero: this register keeps its value (readback mismatch).
    bool stoppedByInput = false; ///< Wired bench 07: a shared X3 stop input ended the motion short.
};
std::map<uint8_t, Drive> drives;
std::vector<std::vector<uint8_t>> frames; // Every transmitted request, in order.
std::function<bool(const std::vector<uint8_t>&)> dropReply; // True: this request gets no reply.
std::function<bool(const std::vector<uint8_t>&)> loseAcknowledgement; // Apply write, then lose its reply.
std::function<uint32_t(const std::vector<uint8_t>&)> replyDelay; // Extra reply latency per request (us).
unsigned servedWrites = 0;

std::vector<uint8_t> crc(std::vector<uint8_t> bytes) {
    const uint16_t value = ESS::calcCrc16(bytes.data(), bytes.size());
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8));
    return bytes;
}
uint16_t word(const std::vector<uint8_t>& frame, std::size_t at) {
    return static_cast<uint16_t>(frame[at] << 8 | frame[at + 1]);
}
void addDrive(uint8_t address) {
    Drive drive;
    drive.regs[0x0000] = 0x4EEA; drive.regs[0x0001] = 0x0029; drive.regs[0x0002] = address; drive.regs[0x0003] = 0;
    drive.regs[0x0007] = 1; // In position, enabled, no alarm.
    drive.regs[0x0010] = 0; drive.regs[0x0011] = 1000; // Direction, subdivision.
    drive.regs[0x0013] = address; drive.regs[0x0019] = 1; // Custom node; word order known.
    drive.regs[0x0040] = 0; drive.regs[0x0041] = 1; drive.regs[0x0042] = 2; drive.regs[0x0043] = 3; drive.regs[0x0044] = 0;
    drive.regs[0x0100] = 2; drive.regs[0x0101] = 4000;
    drive.regs[0x0020] = 10; drive.regs[0x0021] = 100; drive.regs[0x0022] = 100; drive.regs[0x0023] = 60; // Profile.
    drives[address] = drive;
}
uint16_t readWord(Drive& drive, uint16_t reg) {
    if (reg == 0x0007 && drive.stoppedByInput) return 0; // Stopped short: not running, not in position.
    if (reg == 0x0007 && drive.runningReads) { --drive.runningReads; return 4; } // RUNNING.
    return reg < 0x140 ? drive.regs[reg] : 0;
}
void writeWord(Drive& drive, uint16_t reg, uint16_t value) {
    if (reg >= 0x140 || reg == drive.ignoredWrite) return;
    drive.regs[reg] = value;
    if (reg == 0x0027 && (value & 1)) { drive.runningReads = 1; ++drive.startsApplied; } // RUNNING, then ARRIVED.
    if (reg == 0x0027 && (value & 0x0300)) drive.runningReads = 0; // Stop.
    if (reg == 0x002D && value == 17) drive.regs[0x0007] = 16; // Released.
    if (reg == 0x002D && value == 18) drive.regs[0x0007] = 1; // Enabled, arrived.
}
// Answer each new request once, as the addressed drive would, after its TX ends.
void serveBus() {
    if (hardware.writes == servedWrites) return;
    servedWrites = hardware.writes;
    const auto frame = hardware.tx;
    frames.push_back(frame);
    if (frame.size() < 8) return;
    const auto found = drives.find(frame[0]);
    if (found == drives.end() || !found->second.present || (dropReply && dropReply(frame))) return;
    auto& drive = found->second;
    const uint16_t first = word(frame, 2);
    std::vector<uint8_t> reply;
    if (frame[1] == 3) {
        const uint16_t count = word(frame, 4);
        reply = {frame[0], 3, static_cast<uint8_t>(count * 2)};
        for (uint16_t i = 0; i < count; ++i) {
            const uint16_t value = readWord(drive, static_cast<uint16_t>(first + i));
            reply.push_back(static_cast<uint8_t>(value >> 8)); reply.push_back(static_cast<uint8_t>(value));
        }
        reply = crc(reply);
    } else if (frame[1] == 6) {
        writeWord(drive, first, word(frame, 4));
        reply = frame;
    } else if (frame[1] == 16) {
        const uint16_t count = word(frame, 4);
        for (uint16_t i = 0; i < count; ++i) writeWord(drive, static_cast<uint16_t>(first + i), word(frame, 7 + 2 * i));
        reply = crc(std::vector<uint8_t>(frame.begin(), frame.begin() + 6));
    } else return;
    if (loseAcknowledgement && loseAcknowledgement(frame)) return;
    scheduleReply(std::max(hardware.writeStarted + frame.size() * 87 + 1000, hardware.time + 1000) +
        (replyDelay ? replyDelay(frame) : 0), reply);
}
void step(uint32_t us = 10) { advanceHardware(hardware.time + us); serviceApplication(); serveBus(); }
void pump(unsigned count = 64) { for (unsigned i = 0; i < count; ++i) step(); }
template <typename Done> void runUntil(Done done, unsigned limit = 400000) {
    for (unsigned i = 0; i < limit && !done(); ++i) step(20);
    assert(done());
}
std::vector<uint8_t> addresses(uint8_t count) {
    std::vector<uint8_t> list;
    for (uint8_t address = 1; address <= count; ++address) list.push_back(address);
    return list;
}
void fresh(const std::vector<uint8_t>& present = {1, 2, 4}) {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = false;
    drives.clear(); frames.clear(); dropReply = nullptr; loseAcknowledgement = nullptr; replyDelay = nullptr; servedWrites = 0;
    for (const auto address : present) addDrive(address);
    ApplicationOptions options; options.moveTimeoutMs = 3000; // Keeps unobservable moves short in fake time.
    assert(beginApplication({Board::kRs485TxPin, Board::kRs485RxPin, Board::kRs485DeRePin,
        Board::kRs485DeReActiveHigh}, Board::kRs485ReceiverDisabledDuringTransmit, options));
    assert(app && uart.ready() && app->owner.valid() && hardware.writes == 0);
    hardware.txCharacterUs = 87;
    assert(uart.startCapture(20, timing().holdUs));
    // Simulated fixture topology only: replies are established as the drive's.
    writeResponseConfirmed = true;
}
// A request without any reply interlocks the runner (NO_RESPONSE) until recovery.
void recoverBus(unsigned id) {
    Serial.input = "@" + std::to_string(id) + " recover\n"; Serial.output.clear();
    for (unsigned i = 0; i < 500 && !Serial.input.empty(); ++i) step();
    runUntil([] { return !app->owner.recovering() && !app->owner.needsRecovery() && app->recovery.delivered; });
}
void command(const std::string& text) {
    Serial.input = text; Serial.output.clear();
    for (unsigned i = 0; i < 500 && !Serial.input.empty(); ++i) step();
    assert(Serial.input.empty()); pump();
}
void contains(const char* text) {
    if (Serial.output.find(text) == std::string::npos) {
        std::fprintf(stderr, "missing %s in:\n%s\n", text, Serial.output.c_str()); assert(false);
    }
}
const App::GroupOperation& groupOp(uint32_t id) {
    const auto* op = groupOperation(*app, id);
    assert(op); return *op;
}
bool terminalGroup(uint32_t id) { return !groupOp(id).result.pending; }
uint32_t declare(const std::vector<uint8_t>& list) {
    uint32_t id = 0;
    assert(declareGroup(list.data(), list.size(), id));
    runUntil([&] { return terminalGroup(id); });
    return id;
}
uint32_t readGroupState() {
    uint32_t id = 0;
    assert(submitGroupStateRead(id));
    runUntil([&] { return terminalGroup(id); });
    assert(groupOp(id).result.ok);
    return id;
}
Probe::GroupMoveRequest groupMove(Policy policy, Mode mode = Mode::STAGED, int64_t steps = 200) {
    Probe::GroupMoveRequest request;
    request.move.position.value = Rational(steps); request.move.position.unit = PositionUnit::STEPS;
    request.move.position.frame = CoordinateFrame::NATIVE; request.move.position.relative = true;
    request.move.speedRpm = 60; request.move.ramp = MoveRamp::VERIFIED_CONFIGURED;
    request.onFailure = policy; request.start = mode;
    return request;
}
// A member-ready group (simulated).
void readyGroup(const std::vector<uint8_t>& list = {1, 2, 4}) {
    const auto set = declare(list);
    assert(groupOp(set).result.ok && app->group.declared);
    readGroupState();
}
bool isWrite(const std::vector<uint8_t>& frame) { return frame.size() >= 8 && (frame[1] == 6 || frame[1] == 16); }
bool isStart(const std::vector<uint8_t>& frame, uint8_t address) {
    return frame.size() == 8 && frame[0] == address && frame[1] == 6 && word(frame, 2) == 0x0027 && (word(frame, 4) & 1);
}
bool isAnyStart(const std::vector<uint8_t>& frame) { return frame.size() == 8 && frame[1] == 6 && word(frame, 2) == 0x0027 && (word(frame, 4) & 1); }
bool isStop(const std::vector<uint8_t>& frame, uint8_t address) {
    return frame.size() == 8 && frame[0] == address && frame[1] == 6 && word(frame, 2) == 0x0027 && (word(frame, 4) & 0x0300);
}
bool isStageFrame(const std::vector<uint8_t>& frame) {
    return frame.size() >= 8 && word(frame, 2) == 0x0021 && word(frame, 4) == 5 && (frame[1] == 16 || frame[1] == 3);
}
unsigned count(const std::function<bool(const std::vector<uint8_t>&)>& match, std::size_t from = 0) {
    unsigned n = 0;
    for (std::size_t i = from; i < frames.size(); ++i) if (match(frames[i])) ++n;
    return n;
}
void noWriteOutside(const std::vector<uint8_t>& members, std::size_t from = 0) {
    for (std::size_t i = from; i < frames.size(); ++i) {
        if (!isWrite(frames[i])) continue;
        bool member = false;
        for (const auto address : members) member = member || frames[i][0] == address;
        assert(member);
    }
}
const Probe::GroupMemberResult& memberResult(uint32_t id, uint8_t address) {
    const auto& r = groupOp(id).result;
    for (uint8_t i = 0; i < r.count; ++i) if (r.members[i].address == address) return r.members[i];
    assert(false); return r.members[0];
}
// Indices of start frames after from; true when they are adjacent in the frame order.
bool startsBackToBack(std::size_t from, unsigned expected) {
    std::size_t firstStart = 0, lastStart = 0; unsigned starts = 0;
    for (std::size_t i = from; i < frames.size(); ++i)
        if (isAnyStart(frames[i])) { if (!starts) firstStart = i; lastStart = i; ++starts; }
    return starts == expected && lastStart - firstStart + 1 == expected;
}

Probe::GroupMemberView memberView(uint8_t address) {
    Probe::GroupView view;
    assert(inspectGroup(view));
    for (uint8_t i = 0; i < view.count; ++i) if (view.members[i].address == address) return view.members[i];
    assert(false); return Probe::GroupMemberView();
}
Probe::GroupMemberMoves memberMoves(const std::vector<uint8_t>& order = {4, 1, 2},
                                  Policy policy = Policy::LEAVE, Mode mode = Mode::STAGED) {
    Probe::GroupView view; assert(inspectGroup(view));
    Probe::GroupMemberMoves request;
    request.declaration = view.declaration; request.bindingGeneration = view.bindingGeneration;
    request.count = order.size(); request.onFailure = policy; request.start = mode;
    assert(request.count <= Probe::GROUP_CAPACITY);
    for (std::size_t i = 0; i < request.count; ++i) {
        request.members[i].address = order[i]; request.members[i].move = groupMove(policy, mode).move;
        request.members[i].move.position.configurationGeneration = memberView(order[i]).configurationGeneration;
    }
    return request;
}
MoveRequest& memberMove(Probe::GroupMemberMoves& request, uint8_t address) {
    for (std::size_t i = 0; i < request.count; ++i)
        if (request.members[i].address == address) return request.members[i].move;
    assert(false); return request.members[0].move;
}
void sameIntent(const MoveRequest& actual, const MoveRequest& expected) {
    const auto& a = actual.position; const auto& e = expected.position;
    assert(a.value.numerator == e.value.numerator && a.value.denominator == e.value.denominator);
    assert(a.unit == e.unit && a.frame == e.frame && a.relative == e.relative && a.wrapped == e.wrapped);
    assert(a.path == e.path && a.tie == e.tie && a.basis == e.basis && a.rounding == e.rounding);
    assert(a.configurationGeneration == e.configurationGeneration && a.maximumQuantizationError == e.maximumQuantizationError);
    assert(a.approximate == e.approximate && a.rationalRadians == e.rationalRadians && a.radians == e.radians);
    assert(a.maximumApproximationError == e.maximumApproximationError);
    assert(actual.speedRpm == expected.speedRpm && actual.ramp == expected.ramp && actual.setup == expected.setup);
}
Probe::AxisView configureMember(uint8_t address, Probe::AxisField field, int64_t first, int64_t second = 1) {
    Probe::AxisCommand command; command.kind = Probe::AxisCommandKind::CONFIGURE; command.field = field;
    command.value = Rational(first); command.secondValue = Rational(second);
    Probe::AxisView view;
    assert(configureGroupAxis(address, command, view));
    return view;
}
Probe::GroupReference memberReference(uint8_t address, int64_t position, uint64_t maximumAge = 0) {
    Probe::GroupView view; assert(inspectGroup(view));
    const auto member = memberView(address);
    Probe::GroupReference supplied;
    supplied.declaration = view.declaration;
    supplied.configurationOperationId = member.configurationOperationId;
    supplied.motionOperationId = member.motionOperationId; supplied.feedbackOperationId = member.feedbackOperationId;
    auto& ref = supplied.reference;
    ref.target = member.target; ref.configurationGeneration = member.configurationGeneration;
    ref.nativeKnown = ref.idle = ref.stationary = true; ref.nativePosition = position;
    ref.basis = RelativeBasis::ACTUAL; ref.source = ScaleSource::QUALIFIED; ref.maximumAgeUs = maximumAge;
    return supplied;
}
void originMember(uint8_t address, int64_t origin, int64_t position) {
    assert(installGroupReference(memberReference(address, position)));
    Probe::AxisCommand command; command.kind = Probe::AxisCommandKind::ORIGIN; command.value = Rational(origin);
    Probe::AxisView view; assert(configureGroupAxis(address, command, view));
    assert(view.configuration.originKnown && view.configuration.originNative == origin);
}
void assertRejectedWithoutTraffic(const Probe::GroupMemberMoves& request) {
    uint32_t id = 777;
    const auto traffic = hardware.writes; const auto operationId = app->nextOperationId;
    assert(!submitGroupMoves(request, id));
    assert(id == 777 && hardware.writes == traffic && app->nextOperationId == operationId);
    for (std::size_t i = GROUP_MOVE_FIRST; i < RECORD_COUNT; ++i) assert(!app->records[i].operationId);
}
void exactStage(uint8_t address, uint16_t rpm, uint16_t first, uint16_t second, std::size_t from) {
    const auto expected = crc({address, 16, 0, 0x21, 0, 5, 10, 0, 100, 0, 100,
        static_cast<uint8_t>(rpm >> 8), static_cast<uint8_t>(rpm),
        static_cast<uint8_t>(first >> 8), static_cast<uint8_t>(first),
        static_cast<uint8_t>(second >> 8), static_cast<uint8_t>(second)});
    assert(count([&](const std::vector<uint8_t>& frame) { return frame == expected; }, from) == 1);
}

void testDeclarationRejectsAbsentAndUnexpectedResponders() {
    fresh();
    // Absent: drive 3 does not exist. The declaration is rejected after reads only.
    auto id = declare({1, 3, 4});
    const auto& absent = groupOp(id).result;
    assert(!absent.ok && absent.aborted && absent.failedAddress == 3);
    assert(memberResult(id, 1).join == Probe::GroupJoin::JOINED && memberResult(id, 3).join == Probe::GroupJoin::ABSENT);
    assert(memberResult(id, 4).join == Probe::GroupJoin::NOT_READ);
    assert(!app->group.declared && app->group.count == 0 && count(isWrite) == 0);
    assert(app->owner.needsRecovery()); // The absent reply is never retried.
    recoverBus(20);
    // Unexpected: drive 2 answers with another node in its identity.
    drives[2].regs[0x0002] = 7;
    id = declare({1, 2});
    assert(!groupOp(id).result.ok && memberResult(id, 2).join == Probe::GroupJoin::UNEXPECTED);
    assert(memberResult(id, 2).rawActiveNode == 7 && !app->group.declared && count(isWrite) == 0);
    drives[2].regs[0x0002] = 2; drives[2].regs[0x0000] = 0x4EEB; // Another model.
    id = declare({2});
    assert(memberResult(id, 2).join == Probe::GroupJoin::UNEXPECTED && !app->group.declared);
    drives[2].regs[0x0000] = 0x4EEA;
    // Success: identity, configuration and profile of each member, no write.
    id = declare({1, 2, 4});
    assert(groupOp(id).result.ok && app->group.declared && app->group.count == 3 && count(isWrite) == 0);
    for (const auto address : {1, 2, 4}) {
        assert(memberResult(id, address).join == Probe::GroupJoin::JOINED);
        assert(memberCurrent(*app, *groupMember(*app, address)));
    }
    // The selected axis is unchanged by the declaration.
    assert(app->axis.target.address == 1 && !app->configuration.operationId);
    command("@5 group set 1 2 4\n"); contains("\"command\":\"group-set\""); contains("\"result\":\"accepted\"");
    runUntil([] { return !app->group.set.result.pending && app->group.set.delivered; });
    pump(2000); // Drain the bounded USB writes.
    contains("\"type\":\"group\""); contains("\"join\":\"joined\"");
    const auto writes = hardware.writes;
    command("@6 group show\n"); contains("\"command\":\"group-show\""); contains("\"current\":true");
    command("@7 group status\n"); contains("\"command\":\"group-status\""); contains("\"bus_traffic\":false");
    assert(hardware.writes == writes);
}
void testFullCapacityDeclarationMoveStopAndOutputOnError() {
    const auto all = addresses(Probe::GROUP_CAPACITY);
    auto extra = all; extra.push_back(16);
    fresh(extra);
    uint32_t unchanged = 77;
    assert(!declareGroup(extra.data(), extra.size(), unchanged) && unchanged == 77);
    const uint8_t repeated[] = {1, 1};
    assert(!declareGroup(repeated, 2, unchanged) && unchanged == 77);
    const uint8_t zero[] = {0};
    assert(!declareGroup(zero, 1, unchanged) && unchanged == 77 && hardware.writes == 0);
    command("@1 group set 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16\n"); contains("invalid_arguments");
    command("@2 group set 1 1\n"); contains("duplicate_address");
    command("@3 group set 248\n"); contains("invalid_address");
    command("@4 group move relative 10 steps native 60 configured\n"); contains("failure_policy_required");
    command("@5 group move absolute 10 steps native 60 configured on-failure leave\n"); contains("unsupported");
    command("@6 group move relative 10 steps native 60 configured setup verify on-failure leave\n"); contains("invalid_arguments");
    command("@7 group move relative 10 steps native 60 configured start later on-failure leave\n"); contains("invalid_arguments");
    // No group yet: move, read and stop are refused without traffic.
    assert(!submitGroupMove(groupMove(Policy::LEAVE), unchanged) && unchanged == 77);
    assert(!submitGroupStop(StopBehavior::DIRECT, unchanged) && !submitGroupStateRead(unchanged) && unchanged == 77);
    assert(hardware.writes == 0);

    // Fifteen members, each with its own facts.
    readyGroup(all);
    assert(app->group.count == Probe::GROUP_CAPACITY && count(isWrite) == 0);
    command("@8 group show\n"); contains("\"count\":15");
    // Requests without a policy, for stored setup or verify-first staging are refused.
    assert(!submitGroupMove(groupMove(Policy::NONE), unchanged) && unchanged == 77);
    auto stored = groupMove(Policy::LEAVE); stored.move.setup = MoveSetup::USE_STORED;
    assert(!submitGroupMove(stored, unchanged) && unchanged == 77);
    auto verify = groupMove(Policy::LEAVE); verify.move.setup = MoveSetup::VERIFY_AND_UPDATE;
    assert(!submitGroupMove(verify, unchanged) && unchanged == 77);
    // A member that is not ready refuses the whole group before any frame.
    drives[15].regs[0x0007] = 4; readGroupState();
    assert(!submitGroupMove(groupMove(Policy::LEAVE), unchanged) && unchanged == 77 && count(isWrite) == 0);
    drives[15].regs[0x0007] = 1; readGroupState();

    // A staged move of all fifteen: every stage first, then fifteen adjacent starts.
    auto from = frames.size();
    uint32_t id = 0;
    assert(submitGroupMove(groupMove(Policy::STOP_FAST), id));
    // While it runs, another group move has no member records and is refused.
    assert(!submitGroupMove(groupMove(Policy::LEAVE), unchanged) && unchanged == 77);
    runUntil([&] { return terminalGroup(id); }, 4000000);
    const auto& r = groupOp(id).result;
    assert(r.ok && r.count == Probe::GROUP_CAPACITY);
    std::size_t firstStart = frames.size(), lastStage = 0;
    for (std::size_t i = from; i < frames.size(); ++i) {
        if (isAnyStart(frames[i]) && firstStart == frames.size()) firstStart = i;
        if (isStageFrame(frames[i])) lastStage = i;
    }
    assert(lastStage < firstStart && startsBackToBack(from, Probe::GROUP_CAPACITY));
    assert(count(isStageFrame, from) == 2 * Probe::GROUP_CAPACITY);
    uint64_t first = UINT64_MAX, last = 0;
    for (const auto address : all) {
        const auto& m = memberResult(id, address);
        assert(m.start == Probe::GroupStart::STARTED && m.completion == ActionCompletion::OBSERVED && !m.uncertain);
        assert(m.stageState == ActionState::SUCCEEDED && m.stageReadback == Probe::GroupStageReadback::MATCH);
        assert(m.stageExecution == ActionExecution::ACKNOWLEDGED && m.stageTxEndUs < m.startTxEndUs && m.runningObserved);
        first = std::min(first, m.startTxEndUs); last = std::max(last, m.startTxEndUs);
    }
    noWriteOutside(all, from);
    std::printf("Simulated 15-member staged start spread: %llu us\n", static_cast<unsigned long long>(last - first));
    assert(releaseOperation(id));
    for (std::size_t i = GROUP_MOVE_FIRST; i < RECORD_COUNT; ++i) assert(!app->records[i].operationId);

    // Full-group stop at capacity: a stop and an escalation per member, 30 records.
    readGroupState();
    from = frames.size();
    for (auto& drive : drives) drive.second.regs[0x0007] = 4; // RUNNING until a stop clears it.
    uint32_t normal = 0, fast = 0, third = 77;
    assert(submitGroupStop(StopBehavior::CONFIGURED_DECELERATION, normal));
    runUntil([&] { return count([](const std::vector<uint8_t>& f) { return f[1] == 6 && word(f, 2) == 0x0027; }, from) == Probe::GROUP_CAPACITY; });
    assert(submitGroupStop(StopBehavior::DIRECT, fast) && fast != normal);
    for (const auto stopId : {normal, fast}) for (uint8_t i = 0; i < Probe::GROUP_CAPACITY; ++i) {
        const auto& m = groupOp(stopId).result.members[i];
        const auto* record = findRecord(*app, m.operationId);
        assert(m.admission == Probe::Action::OK && record && record->groupChild);
        assert(record >= &app->records[GROUP_STOP_FIRST] && record < &app->records[GROUP_STOP_FIRST + GROUP_STOP_RECORDS]);
    }
    assert(!submitGroupStop(StopBehavior::DIRECT, third) && third == 77); // Both group stops pending.
    for (std::size_t i = REQUEST_CAPACITY + 1; i < GROUP_STOP_FIRST; ++i) assert(!app->records[i].operationId);
    for (auto& drive : drives) drive.second.regs[0x0007] = 1;
    runUntil([&] { return terminalGroup(normal) && terminalGroup(fast); }, 4000000);
    for (const auto address : all) {
        assert(count([address](const std::vector<uint8_t>& f) { return isStop(f, address); }, from) == 2);
        assert(memberResult(fast, address).completion == ActionCompletion::OBSERVED);
    }
    assert(groupOp(fast).result.ok && !groupOp(normal).result.ok);
    noWriteOutside(all, from);
    assert(submitGroupStop(StopBehavior::DIRECT, third) && third != 77);
    std::printf("Group storage bytes: App=%zu Record=%zu records=%zu GroupMember=%zu GroupOperation=%zu GroupResult=%zu GroupView=%zu StageSession=%zu urgent_records=%zu owner_results=%zu output_capacity=%zu\n",
        sizeof(App), sizeof(App::Record), RECORD_COUNT, sizeof(App::GroupMember), sizeof(App::GroupOperation),
        sizeof(Probe::GroupResult), sizeof(Probe::GroupView), sizeof(App::StageSession), URGENT_RECORDS, RESULT_SLOTS,
        Probe::OUTPUT_CAPACITY);
    // Locals of the admission path, for the loop-task stack budget.
    std::printf("Stack locals bytes: GroupCommand=%zu GroupReply=%zu MoveRequest=%zu MovePrerequisites=%zu PreparedMove=%zu MotionFacts=%zu ActionOptions=%zu\n",
        sizeof(Probe::GroupCommand), sizeof(Probe::GroupReply), sizeof(MoveRequest), sizeof(ESS::MovePrerequisites),
        sizeof(ESS::PreparedMove), sizeof(MotionFacts), sizeof(ActionOptions));
}
// The widest possible terminal group line still fits one console output line.
void testWorstCaseGroupLineFits() {
    fresh(addresses(Probe::GROUP_CAPACITY));
    auto& op = app->group.move;
    auto& r = op.result;
    r.kind = Probe::GroupOperationKind::MOVE; r.operationId = UINT32_MAX; r.commandId = UINT32_MAX;
    r.pending = false; r.ok = false; r.aborted = true; r.failedAddress = 247;
    r.declaration = r.bindingGeneration = UINT32_MAX; r.startedUs = r.finishedUs = UINT64_MAX;
    r.move = groupMove(Policy::STOP_NORMAL, Mode::SEQUENTIAL);
    r.move.move.position.value = Rational(INT64_MIN + 1, UINT32_MAX); r.move.move.position.unit = PositionUnit::FULL_STEPS;
    r.move.move.speedRpm = UINT16_MAX; r.count = Probe::GROUP_CAPACITY;
    for (auto& m : r.members) {
        m.request = r.move.move;
        m.request.position.value.denominator = UINT64_MAX;
        m.request.position.configurationGeneration = UINT32_MAX;
        m.request.position.maximumQuantizationError = std::numeric_limits<double>::max();
        m.request.position.radians = -std::numeric_limits<double>::max();
        m.request.position.maximumApproximationError = std::numeric_limits<double>::max();
        m.request.position.relative = m.request.position.approximate = m.request.position.rationalRadians = false;
        m.request.setup = MoveSetup::VERIFY_AND_UPDATE;
        m.address = 247; m.operationId = 0; m.admission = Probe::Action::QUALIFICATION_REQUIRED;
        m.status = m.stageStatus = Status(Err::INVALID_CONFIG, INT32_MIN, "x");
        m.start = Probe::GroupStart::NOT_SENT; m.state = m.stopState = ActionState::SUCCEEDED;
        m.outcome = ActionOutcome::UNCONFIRMED_RESPONSE; m.execution = m.setupExecution = m.stopExecution = m.stageExecution = ActionExecution::NOT_TRANSMITTED;
        m.effectiveNative = INT64_MIN; m.startTxEndUs = m.runningEarliestUs = m.runningLatestUs = UINT64_MAX;
        m.stageTxEndUs = m.stageReadLatestUs = UINT64_MAX; m.startTxUncertaintyUs = UINT32_MAX;
        m.startTxComplete = m.runningObserved = m.uncertain = m.interruptedByStop = m.stageTxComplete = false;
        m.stopOperationId = 0; m.stopAdmission = Probe::Action::QUALIFICATION_REQUIRED;
        m.stageState = ActionState::EMPTY; m.stageReadback = Probe::GroupStageReadback::MISMATCH;
    }
    op.delivered = true;
    for (bool individual : {false, true}) {
        r.individual = individual;
        command("@4294967295 result 4294967295\n");
        pump(4000);
        const auto begin = Serial.output.find("{\"type\":\"reply\",\"profile\":\"ess_rs\",\"id\":4294967295,\"command\":\"result\"");
        assert(begin != std::string::npos);
        const auto end = Serial.output.find('\n', begin);
        assert(end != std::string::npos && Serial.output.find("\"group_kind\":\"move\"", begin) < end);
        const std::size_t length = end - begin;
        assert(length < Probe::OUTPUT_CAPACITY && Serial.output[end - 1] == '}');
        std::printf("Worst-case 15-member group line (%s): %zu/%zu bytes\n", individual ? "individual" : "common", length, Probe::OUTPUT_CAPACITY);
    }
    clearGroupOperation(op);
}
void testSequentialMoveStartsEachMemberInOrder() {
    fresh(); readyGroup();
    const auto from = frames.size();
    uint32_t id = 0;
    assert(submitGroupMove(groupMove(Policy::STOP_FAST, Mode::SEQUENTIAL), id));
    for (const auto address : {1, 2, 4}) assert(axisReserved(*app, address));
    runUntil([&] { return terminalGroup(id); }, 2000000);
    const auto& r = groupOp(id).result;
    assert(r.ok && !r.aborted && r.count == 3);
    uint64_t previous = 0;
    for (const auto address : {1, 2, 4}) {
        const auto& m = memberResult(id, address);
        assert(m.start == Probe::GroupStart::STARTED && m.state == ActionState::SUCCEEDED);
        assert(m.completion == ActionCompletion::OBSERVED && !m.uncertain && m.effectiveNative == 200);
        assert(m.startTxComplete && m.startTxEndUs > previous && m.runningObserved && m.runningLatestUs >= m.runningEarliestUs);
        assert(m.stageState == ActionState::EMPTY && m.setupExecution == ActionExecution::ACKNOWLEDGED);
        previous = m.startTxEndUs;
        assert(count([&](const std::vector<uint8_t>& f) { return isStart(f, address); }, from) == 1);
    }
    // Member order: each start precedes the next member's own staging write.
    std::size_t start1 = 0, write2 = 0, start2 = 0, write4 = 0;
    for (std::size_t i = from; i < frames.size(); ++i) {
        if (!start1 && isStart(frames[i], 1)) start1 = i;
        if (!write2 && isWrite(frames[i]) && frames[i][0] == 2) write2 = i;
        if (!start2 && isStart(frames[i], 2)) start2 = i;
        if (!write4 && isWrite(frames[i]) && frames[i][0] == 4) write4 = i;
    }
    assert(start1 && start1 < write2 && write2 < start2 && start2 < write4);
    assert(count(isStageFrame, from) == 3); // Three FC10 writes, no stage readback.
    noWriteOutside({1, 2, 4}, from);
    assert(app->group.move.delivered && releaseOperation(id) && !app->group.move.result.operationId);
    // Member state was invalidated by the moves; a new move needs a fresh read.
    uint32_t again = 0;
    assert(!submitGroupMove(groupMove(Policy::LEAVE), again));
    readGroupState();
    command("@9 group move relative 50 steps native 60 configured start sequential on-failure leave\n");
    contains("\"command\":\"group-move\""); contains("\"result\":\"accepted\"");
    runUntil([] { return app->group.move.result.operationId && !app->group.move.result.pending && app->group.move.delivered; }, 2000000);
    pump(2000);
    contains("\"type\":\"group\""); contains("\"start\":\"sequential\""); contains("\"start_tx_spread_us\":");
}
// Wired bench 07 Implement 5. X3 = 5 on every member (one shared input line).
void sharedStopInput(const std::vector<uint8_t>& list) {
    for (const auto address : list) drives[address].regs[0x0044] = 5;
}
void assertStopInputRule(const std::vector<uint8_t>& list) {
    // A group with one member's X3 active refuses before any write, naming it.
    fresh(list); sharedStopInput(list); readyGroup(list);
    drives[2].regs[0x0008] = 8; readGroupState();
    const auto from = frames.size();
    uint32_t refused = 77;
    assert(!submitGroupMove(groupMove(Policy::LEAVE, Mode::SEQUENTIAL), refused) && refused == 77);
    assert(count(isWrite, from) == 0);
    Probe::SettingsRefusal refusal;
    assert(copySettingsRefusal(refusal) && !std::strcmp(refusal.reason, "stop_input_active") && refusal.terminal == 3);
    command("@31 group move relative 50 steps native 60 configured start sequential on-failure leave\n");
    contains("\"reason\":\"stop_input_active\""); contains("\"address\":2");
    assert(count(isWrite, from) == 0);
}
void testSharedStopInputGroupOutcomes() {
    const std::vector<uint8_t> list = {1, 2, 4};
    assertStopInputRule(list);
    // Sequential: member 1 reports the input before member 4 started. No later
    // start is sent; each member keeps its own outcome.
    fresh(list); sharedStopInput(list); readyGroup(list);
    // Hold member 2's staging reply so member 1's first poll lands before member 4 begins.
    replyDelay = [](const std::vector<uint8_t>& f) -> uint32_t { return f[0] == 2 && f[1] == 16 ? 40000 : 0; };
    uint32_t id = 0;
    assert(submitGroupMove(groupMove(Policy::LEAVE, Mode::SEQUENTIAL), id));
    const auto from = frames.size();
    runUntil([&] { return count([](const std::vector<uint8_t>& f) { return isStart(f, 1); }, from) == 1; });
    // The shared stop input: every drive stops short and reports X3 active.
    for (const auto address : list) { drives[address].regs[0x0008] = 8; drives[address].stoppedByInput = true; }
    const auto inputFrom = frames.size();
    runUntil([&] { return terminalGroup(id); }, 4000000);
    std::size_t inputSeen = 0;
    for (std::size_t i = inputFrom; i < frames.size() && !inputSeen; ++i)
        if (frames[i][0] == 1 && frames[i][1] == 3 && word(frames[i], 2) == 0x0006 && word(frames[i], 4) == 3) inputSeen = i;
    const auto& r = groupOp(id).result;
    assert(!r.ok && r.aborted);
    assert(inputSeen && count(isAnyStart, inputSeen + 1) == 0); // No start after the report.
    assert(count([](const std::vector<uint8_t>& f) { return isStart(f, 4); }, from) == 0);
    const auto& first = memberResult(id, 1);
    assert(first.start == Probe::GroupStart::STARTED && first.state == ActionState::FAILED);
    assert(first.status.detail == static_cast<int32_t>(MoveError::STOP_INPUT) && first.completion == ActionCompletion::NOT_OBSERVED);
    const auto& last = memberResult(id, 4);
    assert(last.start != Probe::GroupStart::STARTED && last.state == ActionState::FAILED);
    for (const auto address : list) assert(memberResult(id, address).completion != ActionCompletion::OBSERVED);
    noWriteOutside(list, from);
    // Staged: every member stopped by the shared input fails with its own STOP_INPUT.
    fresh(list); sharedStopInput(list); readyGroup(list);
    uint32_t staged = 0;
    assert(submitGroupMove(groupMove(Policy::LEAVE, Mode::STAGED), staged));
    const auto stagedFrom = frames.size();
    runUntil([&] { return count(isAnyStart, stagedFrom) == 3; });
    for (const auto address : list) { drives[address].regs[0x0008] = 8; drives[address].stoppedByInput = true; }
    runUntil([&] { return terminalGroup(staged); }, 4000000);
    for (const auto address : list) {
        const auto& m = memberResult(staged, address);
        assert(m.start == Probe::GroupStart::STARTED && m.state == ActionState::FAILED &&
               m.status.detail == static_cast<int32_t>(MoveError::STOP_INPUT));
    }
    assert(count(isAnyStart, stagedFrom) == 3); // No restart or replay.
}
void testStagedMoveStagesAllBeforeStarts() {
    fresh(); readyGroup();
    auto from = frames.size();
    command("@3 group move relative 200 steps native 60 configured on-failure stop fast\n"); contains("\"result\":\"accepted\"");
    runUntil([] { return app->group.move.result.operationId && !app->group.move.result.pending && app->group.move.delivered; }, 2000000);
    pump(3000);
    contains("\"start\":\"staged\""); contains("\"stage\":\"staged\""); contains("\"stage_readback\":\"match\"");
    const auto id = app->group.move.result.operationId;
    assert(groupOp(id).result.ok);
    // Each member: one FC10 write and one readback, then three adjacent starts.
    std::size_t lastStage = 0, firstStart = frames.size();
    for (std::size_t i = from; i < frames.size(); ++i) {
        if (isStageFrame(frames[i])) lastStage = i;
        if (isAnyStart(frames[i]) && firstStart == frames.size()) firstStart = i;
    }
    assert(count(isStageFrame, from) == 6 && lastStage < firstStart && startsBackToBack(from, 3));
    // The start relies on the staged words: each drive holds exactly them.
    for (const auto address : {1, 2, 4}) {
        assert(drives[static_cast<uint8_t>(address)].regs[0x0023] == 60 && drives[static_cast<uint8_t>(address)].regs[0x0024] == 200);
        assert(memberResult(id, address).setupExecution == ActionExecution::NOT_TRANSMITTED);
    }
    noWriteOutside({1, 2, 4}, from);
    command(("@4 release " + std::to_string(id) + "\n").c_str()); contains("\"result\":\"done\"");
}
// Staging failures on the middle member: no start reaches any member.
void testStagingFailureSendsNoStart() {
    for (const auto policy : {Policy::STOP_FAST, Policy::LEAVE}) for (unsigned failure = 0; failure < 2; ++failure) {
        fresh(); readyGroup();
        const auto from = frames.size();
        if (failure == 0) drives[2].ignoredWrite = 0x0024; // Readback differs from the staged words.
        else dropReply = [](const std::vector<uint8_t>& f) { return f[0] == 2 && f[1] == 16; }; // Lost stage reply.
        uint32_t id = 0;
        assert(submitGroupMove(groupMove(policy), id));
        runUntil([&] { return terminalGroup(id); }, 2000000);
        const auto& r = groupOp(id).result;
        assert(!r.ok && r.aborted && r.failedAddress == 2);
        assert(count(isAnyStart, from) == 0 && count([](const std::vector<uint8_t>& f) { return isStop(f, 1) || isStop(f, 2) || isStop(f, 4); }, from) == 0);
        assert(count([](const std::vector<uint8_t>& f) { return f[0] == 4; }, from) == 0);
        const auto& middle = memberResult(id, 2);
        assert(middle.stageState == ActionState::FAILED && middle.start == Probe::GroupStart::NOT_SENT);
        if (failure == 0) assert(middle.stageReadback == Probe::GroupStageReadback::MISMATCH && !middle.uncertain);
        else assert(middle.stageExecution == ActionExecution::UNKNOWN && middle.uncertain && app->owner.needsRecovery());
        assert(memberResult(id, 1).stageState == ActionState::SUCCEEDED && memberResult(id, 1).start == Probe::GroupStart::NOT_SENT);
        assert(memberResult(id, 4).stageState == ActionState::EMPTY && memberResult(id, 4).outcome == ActionOutcome::CANCELLED);
        for (const auto address : {1, 2, 4}) assert(!axisReserved(*app, address));
        noWriteOutside({1, 2, 4}, from);
    }
}
// The middle member's start reply is lost (UNKNOWN), or its sequential staging times out.
void lostOrTimedOutMiddle(Policy policy, bool lostStart, Mode mode) {
    fresh(); readyGroup();
    const auto from = frames.size();
    dropReply = [lostStart](const std::vector<uint8_t>& f) {
        return f[0] == 2 && (lostStart ? isStart(f, 2) : isWrite(f));
    };
    uint32_t id = 0;
    assert(submitGroupMove(groupMove(policy, mode), id));
    runUntil([&] { return terminalGroup(id); }, 4000000);
    const auto& r = groupOp(id).result;
    assert(!r.ok && r.aborted && r.failedAddress == 2);
    const auto& first = memberResult(id, 1); const auto& middle = memberResult(id, 2); const auto& last = memberResult(id, 4);
    assert(first.start == Probe::GroupStart::STARTED);
    assert(middle.start == (lostStart ? Probe::GroupStart::UNKNOWN : Probe::GroupStart::NOT_SENT) && middle.uncertain);
    assert(last.start == Probe::GroupStart::NOT_SENT && last.outcome == ActionOutcome::CANCELLED && !last.uncertain);
    // Never resent; nothing reaches the last member after the failure.
    assert(count([](const std::vector<uint8_t>& f) { return isStart(f, 2); }, from) == (lostStart ? 1u : 0u));
    assert(count([](const std::vector<uint8_t>& f) { return f[0] == 4 && (isWrite(f) || f[1] == 3); }, from) ==
        (mode == Mode::STAGED ? 2u : 0u)); // A staged last member was staged before the starts.
    const bool stop = policy != Policy::LEAVE;
    assert(count([](const std::vector<uint8_t>& f) { return isStop(f, 1); }, from) == (stop ? 1u : 0u));
    assert(count([](const std::vector<uint8_t>& f) { return isStop(f, 2); }, from) == (stop && lostStart ? 1u : 0u));
    assert((first.stopOperationId != 0) == stop && (middle.stopOperationId != 0) == (stop && lostStart));
    if (stop) assert(first.stopExecution == ActionExecution::ACKNOWLEDGED);
    noWriteOutside({1, 2, 4}, from);
    // An uncertain start or staging write interlocks its address until a checked stop.
    assert(axisReserved(*app, 2) == !(lostStart && stop));
}
void testLostStartAndTimeoutUnderBothPolicies() {
    for (const auto policy : {Policy::STOP_FAST, Policy::LEAVE}) {
        lostOrTimedOutMiddle(policy, true, Mode::STAGED);
        lostOrTimedOutMiddle(policy, true, Mode::SEQUENTIAL);
        lostOrTimedOutMiddle(policy, false, Mode::SEQUENTIAL);
    }
}
void testCancelledGroupStopsRetainPendingRecovery() {
    fresh({1, 2}); readyGroup({1, 2});
    const auto from = frames.size();
    hardware.rx.push_back(1); hardware.rx.push_back(3); uart.sample();
    assert(uart.needsRecovery());
    uint32_t normal = 0, fast = 0;
    assert(submitGroupStop(StopBehavior::CONFIGURED_DECELERATION, normal));
    runUntil([] { return app->owner.recovering(); });
    const auto recovery = app->recovery.operationId;
    const auto ownerRecovery = app->recovery.id;
    assert(submitGroupStop(StopBehavior::DIRECT, fast));
    step();
    for (const uint8_t address : {uint8_t(1), uint8_t(2)}) {
        const auto* child = findRecord(*app, memberResult(normal, address).operationId);
        assert(child && child->action.state == ActionState::FAILED && child->action.outcome == ActionOutcome::CANCELLED);
        assert(child->action.execution == ActionExecution::NOT_TRANSMITTED && !child->delivered);
        assert(child->stopRecoveryOperationId == recovery && child->stopRecoveryId == ownerRecovery && !child->stopRecoveryResult.id);
    }
    Probe::GroupResult pending;
    assert(copyGroupResult(normal, pending) && pending.pending && !groupOp(normal).delivered);
    assert(!releaseOperation(normal));
    uint32_t unchanged = 777;
    assert(!submitGroupStop(StopBehavior::DIRECT, unchanged) && unchanged == 777);
    assert(frames.size() == from && !app->stopEvidenceOverwritten);
    runUntil([&] { return terminalGroup(normal) && terminalGroup(fast); });
    assert(!groupOp(normal).result.ok && groupOp(fast).result.ok);
    for (const uint8_t address : {uint8_t(1), uint8_t(2)}) {
        const auto* child = findRecord(*app, memberResult(normal, address).operationId);
        assert(child && child->delivered && child->stopRecoveryOperationId == recovery);
        assert(child->stopRecoveryResult.id == ownerRecovery && child->stopRecoveryResult.outcome == Rtu::RecoveryOutcome::RECOVERED);
        assert(child->action.outcome == ActionOutcome::CANCELLED && child->action.execution == ActionExecution::NOT_TRANSMITTED);
        assert(count([address](const std::vector<uint8_t>& frame) { return isStop(frame, address); }, from) == 1);
    }
    assert(count([](const std::vector<uint8_t>& frame) {
        return frame.size() == 8 && frame[1] == 6 && word(frame, 2) == 0x0027 && word(frame, 4) == 0x0100;
    }, from) == 0);
    assert(releaseOperation(normal) && releaseOperation(fast));
    const auto completedFrames = frames.size(); pump(2000);
    assert(frames.size() == completedFrames && !app->stopEvidenceOverwritten);
}
void testUrgentStopDuringStagingAndStarts() {
    for (const auto mode : {Mode::STAGED, Mode::SEQUENTIAL}) {
        // A group stop while the middle member stages: nothing starts after it.
        fresh(); readyGroup();
        auto from = frames.size();
        uint32_t id = 0;
        assert(submitGroupMove(groupMove(Policy::STOP_NORMAL, mode), id));
        runUntil([&] { return hardware.writes && hardware.tx[0] == 2 && hardware.tx[1] == 16; });
        command("@3 group stop fast\n"); contains("\"command\":\"group-stop\""); contains("\"result\":\"accepted\"");
        const auto stopId = app->group.stops[0].result.operationId;
        assert(stopId);
        runUntil([&] { return !groupSequencing(*app); }); // Ends once the staging frame settles.
        runUntil([&] { return terminalGroup(id) && terminalGroup(stopId); }, 4000000);
        assert(groupOp(id).result.aborted && memberResult(id, 2).start == Probe::GroupStart::NOT_SENT);
        assert(memberResult(id, 4).start == Probe::GroupStart::NOT_SENT && memberResult(id, 2).interruptedByStop);
        assert(count([](const std::vector<uint8_t>& f) { return isStart(f, 2) || isStart(f, 4); }, from) == 0);
        if (mode == Mode::STAGED) assert(count(isAnyStart, from) == 0 && !groupMember(*app, 2)->storedKnown);
        for (const auto address : {1, 2, 4}) {
            // Exactly the one group stop per member; the policy added none.
            assert(count([address](const std::vector<uint8_t>& f) { return isStop(f, address); }, from) == 1);
            assert(memberResult(stopId, address).completion == ActionCompletion::OBSERVED);
            assert(!axisReserved(*app, address));
        }
        assert(groupOp(stopId).result.ok && memberResult(id, 1).stopOperationId == 0);
        noWriteOutside({1, 2, 4}, from);

        // A single urgent stop of a waiting member during the first start: no later start.
        fresh(); readyGroup();
        from = frames.size();
        assert(submitGroupMove(groupMove(Policy::STOP_FAST, mode), id));
        runUntil([] { return isStart(hardware.tx, 1); });
        command("@4 stop fast 4\n"); contains("\"result\":\"accepted\"");
        runUntil([&] { return terminalGroup(id); }, 4000000);
        pump(2000);
        assert(groupOp(id).result.aborted && groupOp(id).result.failedAddress == 4);
        assert(memberResult(id, 1).start == Probe::GroupStart::STARTED && memberResult(id, 2).start == Probe::GroupStart::NOT_SENT);
        assert(memberResult(id, 4).interruptedByStop && memberResult(id, 4).start == Probe::GroupStart::NOT_SENT);
        assert(count([](const std::vector<uint8_t>& f) { return isStart(f, 2) || isStart(f, 4); }, from) == 0);
        assert(count([](const std::vector<uint8_t>& f) { return isStop(f, 1); }, from) == 1); // Policy stop.
        assert(count([](const std::vector<uint8_t>& f) { return isStop(f, 4); }, from) == 1); // The explicit stop only.
        noWriteOutside({1, 2, 4}, from);
    }
}
// Between staging and the starts, a changed witness blocks every start.
void testChangedWordBetweenPhasesBlocksStarts() {
    for (unsigned change = 0; change < 2; ++change) {
        fresh(); readyGroup();
        const auto from = frames.size();
        uint32_t id = 0;
        assert(submitGroupMove(groupMove(Policy::STOP_FAST), id));
        runUntil([] { return groupMember(*app, 2)->storedKnown; });
        if (change == 0) groupMember(*app, 2)->storedWords[2] ^= 1; // Witness no longer matches the start's words.
        else invalidateMemberConfiguration(*app, 2, nowUs()); // A write, setting or restore reached the drive.
        runUntil([&] { return terminalGroup(id); }, 2000000);
        assert(groupOp(id).result.aborted && groupOp(id).result.failedAddress == 2);
        assert(count(isAnyStart, from) == 0 && count([](const std::vector<uint8_t>& f) { return isStop(f, 1) || isStop(f, 2) || isStop(f, 4); }, from) == 0);
        for (const auto address : {1, 2, 4}) assert(memberResult(id, address).start == Probe::GroupStart::NOT_SENT);
    }
    // Every invalidation ends a member's witness.
    fresh(); readyGroup();
    uint32_t id = 0;
    assert(submitGroupMove(groupMove(Policy::LEAVE), id));
    runUntil([&] { return terminalGroup(id); }, 2000000);
    assert(groupOp(id).result.ok && groupMember(*app, 4)->storedKnown);
    invalidateAfterBroadcast(*app, nowUs());
    for (const auto address : {1, 2, 4}) assert(!groupMember(*app, address)->storedKnown);
}
void testGenerationInvalidation() {
    uint32_t id = 77;
    // useaddr rebinding expires every member: declare again.
    fresh(); readyGroup();
    command("@1 useaddr 2\n"); contains("\"result\":\"done\"");
    assert(!memberCurrent(*app, app->group.members[0]));
    assert(!submitGroupMove(groupMove(Policy::LEAVE), id) && id == 77);
    assert(!submitGroupStateRead(id) && id == 77);
    readyGroup();
    assert(memberCurrent(*app, app->group.members[1]));
    // Enable: a member keeps its own post-enable wait and its state expires.
    command("@2 enable 4\n"); contains("\"result\":\"accepted\"");
    runUntil([] { return app->enableWaits[4].untilUs != 0; });
    command("@3 enable 2\n"); contains("\"result\":\"accepted\"");
    runUntil([] { return app->enableWaits[2].untilUs != 0; });
    // A later enable of another member keeps the first member's wait.
    assert(enableSettling(*app, 4, nowUs()) && enableSettling(*app, 2, nowUs()));
    pump(4000);
    assert(!submitGroupMove(groupMove(Policy::LEAVE), id) && id == 77);
    advanceHardware(hardware.time + ENABLE_SETTLE_US); pump();
    assert(!enableSettling(*app, 4, nowUs()) && !enableSettling(*app, 2, nowUs()));
    assert(!submitGroupMove(groupMove(Policy::LEAVE), id) && id == 77); // Stale state.
    readGroupState();
    assert(submitGroupMove(groupMove(Policy::LEAVE), id) && id != 77);
    runUntil([&] { return terminalGroup(id); }, 2000000);
    assert(releaseOperation(id));
    // Recovery changes the binding: members expire.
    readGroupState();
    recoverBus(4);
    id = 77;
    assert(!memberCurrent(*app, app->group.members[0]) && !submitGroupMove(groupMove(Policy::LEAVE), id) && id == 77);
    // A broadcast may change every drive: members expire too.
    readyGroup();
    invalidateAfterBroadcast(*app, nowUs());
    assert(!memberCurrent(*app, app->group.members[2]) && !submitGroupMove(groupMove(Policy::LEAVE), id) && id == 77);
    // A settings write to a member drive expires that member's configuration.
    readyGroup();
    invalidateDriverAssumptions(*app, 2, static_cast<uint32_t>(ESS::DriverField::SUBDIVISION), nowUs());
    assert(memberCurrent(*app, app->group.members[0]) && !memberCurrent(*app, app->group.members[1]));
    // A possible start-speed write keeps that member unready, also after re-declaration.
    invalidateDriverAssumptions(*app, 4, 1, nowUs(), nullptr, ESS::DriverGroup::POSITION_START_SPEED);
    readyGroup();
    assert(groupMember(*app, 4)->startSpeedRelationRequired && !groupMember(*app, 2)->startSpeedRelationRequired);
    id = 77;
    assert(!submitGroupMove(groupMove(Policy::LEAVE), id) && id == 77);
}
uint32_t enableDrive(uint8_t address, uint32_t commandId) {
    command("@" + std::to_string(commandId) + " enable " + std::to_string(address) + "\n");
    contains("\"result\":\"accepted\"");
    const uint32_t id = app->latestOperationId;
    runUntil([&] { const auto* r = findRecord(*app, id); return r && terminal(*app, *r) && r->delivered; });
    const auto& wait = app->enableWaits[address];
    assert(wait.operationId == id && wait.target.address == address && wait.observedUs < wait.untilUs);
    assert(wait.untilUs - wait.observedUs == ENABLE_SETTLE_US && wait.write.txAccepted == 8);
    assert(wait.execution == ActionExecution::ACKNOWLEDGED && wait.write.txComplete);
    assert(releaseOperation(id)); // The wait must outlive result storage too.
    return id;
}
void sameEnableWait(uint8_t address, const App::EnableWait& old) {
    const auto& now = app->enableWaits[address];
    assert(now.operationId == old.operationId && now.target.id == old.target.id && now.target.address == old.target.address &&
        now.target.generation == old.target.generation && now.serialGeneration == old.serialGeneration);
    assert(sameTuple(now.serialTuple, old.serialTuple) && now.observedUs == old.observedUs && now.untilUs == old.untilUs);
    assert(now.execution == old.execution && now.write.txAccepted == old.write.txAccepted &&
        now.write.txComplete == old.write.txComplete && now.write.executionUnknown == old.write.executionUnknown &&
        now.write.latestUs == old.write.latestUs && now.write.deliveredUs == old.write.deliveredUs &&
        now.write.length == old.write.length && now.write.status.code == old.write.status.code &&
        std::memcmp(now.write.raw, old.write.raw, old.write.length) == 0);
}
void testEnableWaitSurvivesGroupChanges() {
    for (unsigned change = 0; change < 6; ++change) {
        fresh(); readyGroup({1, 2}); enableDrive(1, 11); enableDrive(2, 12);
        const auto first = app->enableWaits[1], second = app->enableWaits[2];
        if (change == 0) readyGroup({2, 1});
        if (change == 1) readyGroup({1});
        if (change == 2) { assert(clearGroup()); readyGroup({1}); }
        if (change == 3) {
            const uint8_t invalid[] = {1, 1}; uint32_t unchanged = 777;
            assert(!declareGroup(invalid, 2, unchanged) && unchanged == 777);
            readyGroup({1});
        }
        if (change == 4) {
            drives[4].regs[0] = 0xFFFF;
            assert(!groupOp(declare({4})).result.ok); readyGroup({1});
        }
        if (change == 5) {
            recoverBus(20);
            assert(app->bindingGeneration != first.target.generation);
            uint32_t unchanged = 777;
            assert(!submitGroupMove(groupMove(Policy::LEAVE), unchanged) && unchanged == 777); // Fresh facts still required.
            readyGroup({1});
        }
        sameEnableWait(1, first); sameEnableWait(2, second);
        assert(nowUs() < first.untilUs && enableSettling(*app, 1, nowUs()) && !enableSettling(*app, 4, nowUs()));
        Probe::GroupView view; assert(inspectGroup(view));
        for (uint8_t i = 0; i < view.count; ++i) {
            assert(view.members[i].settling);
            assert(view.members[i].settleUntilUs == app->enableWaits[view.members[i].address].untilUs);
        }
        const auto from = frames.size(); uint32_t move = 777;
        const auto rejected = submitGroupMove(groupMove(Policy::LEAVE), move);
        assert(!rejected && rejected.detail == static_cast<int32_t>(Probe::Action::BUSY) && move == 777);
        pump(); assert(frames.size() == from && count(isAnyStart, from) == 0);
        assert(enableSettling(*app, 1, first.untilUs - 1) && !enableSettling(*app, 1, first.untilUs));
        advanceHardware(std::max(first.untilUs, second.untilUs));
        assert(submitGroupMove(groupMove(Policy::LEAVE), move));
        runUntil([&] { return terminalGroup(move); }, 2000000);
        assert(groupOp(move).result.ok);
        assert(count([](const std::vector<uint8_t>& f) { return isStart(f, 1); }, from) == 1);
        sameEnableWait(1, first); sameEnableWait(2, second);
    }
}
// Wired bench 16 (audit): accepted alarm-clear bytes on a member hold the group
// start until that member's enable-wait deadline; from the deadline it proceeds.
void testAlarmClearWaitHoldsGroupStart() {
    fresh(); readyGroup({1, 2});
    command("@11 alarm-clear 1\n"); contains("\"result\":\"accepted\"");
    const uint32_t id = app->latestOperationId;
    runUntil([&] { const auto* r = findRecord(*app, id); return r && terminal(*app, *r) && r->delivered; });
    const auto wait = app->enableWaits[1];
    assert(wait.operationId == id && wait.untilUs - wait.observedUs == ENABLE_SETTLE_US && wait.write.txAccepted == 8);
    hardware.clockStep = 0; // Each clock read would otherwise advance the fake time.
    assert(releaseOperation(id) && !enableSettling(*app, 2, hardware.time) && hardware.time < wait.untilUs - 1);
    advanceHardware(wait.untilUs - 1);
    const auto from = frames.size(); uint32_t move = 777;
    const auto rejected = submitGroupMove(groupMove(Policy::LEAVE), move);
    assert(!rejected && rejected.detail == static_cast<int32_t>(Probe::Action::BUSY) && move == 777);
    pump(); assert(frames.size() == from && count(isAnyStart, from) == 0);
    advanceHardware(std::max(hardware.time, wait.untilUs));
    readGroupState(); // The clear's TX expired the member's cached state, as an enable does.
    assert(submitGroupMove(groupMove(Policy::LEAVE), move));
    runUntil([&] { return terminalGroup(move); }, 2000000);
    assert(groupOp(move).result.ok);
    assert(count([](const std::vector<uint8_t>& f) { return isStart(f, 1); }, from) == 1);
}
void readSelectedFacts() {
    for (const char* line : {"@31 read config\n", "@32 motion-profile read\n", "@33 read state\n"}) {
        command(line);
        runUntil([] { return !app->owner.active() && !app->owner.pending() && !reading(*app) && !app->motionProfile.view.pending; });
    }
}
void testCandidateRelationSurvivesGroupChanges() {
    // An equal candidate leaves the existing startup policy alone. A changed
    // start word or any possible candidate write revokes it for that address.
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        fresh({1, 2});
        drives[1].regs[0x0041] = drives[1].regs[0x0042] = drives[1].regs[0x0043] = 0;
        readSelectedFacts(); readyGroup({1, 2});
        Probe::ProfileCandidate candidate;
        candidate.address = 1; candidate.wordOrder = app->configuration.wordOrder;
        std::memcpy(candidate.words, app->motionProfile.view.current + 1, sizeof(candidate.words));
        if (scenario == 1) drives[1].regs[0x0020] = 11; // Contradict the saved start word in the fresh prior read.
        if (scenario >= 2) candidate.words[1] = 200;
        if (scenario == 3) dropReply = [](const std::vector<uint8_t>& frame) { return frame[1] == 16; };
        const auto from = frames.size();
        uint32_t candidateId = 0;
        assert(submitProfileCandidate(candidate, candidateId));
        Probe::ProfileCandidateResult result;
        runUntil([&] { return copyProfileCandidate(candidateId, result) && !result.stage.pending; });
        assert(result.unchanged == (scenario < 2));
        assert(result.stage.staged == (scenario != 3));
        assert(result.stage.execution == (scenario < 2 ? ActionExecution::NOT_TRANSMITTED :
            scenario == 2 ? ActionExecution::ACKNOWLEDGED : ActionExecution::UNKNOWN));
        assert(count(isWrite, from) == (scenario < 2 ? 0U : 1U) && count(isAnyStart, from) == 0);
        assert(drives[1].regs[0x0020] == (scenario == 1 ? 11 : 10));
        assert(app->positionStartSpeedRelationRequired == (scenario != 0));
        assert(groupMember(*app, 1)->startSpeedRelationRequired == (scenario != 0));
        assert(!groupMember(*app, 2)->startSpeedRelationRequired);
        assert(releaseProfileCandidate(candidateId));
        if (scenario == 3) { dropReply = nullptr; recoverBus(40); }

        readyGroup({1, 2});
        if (scenario == 0) {
            uint32_t move = 0;
            assert(submitGroupMove(groupMove(Policy::LEAVE), move));
            runUntil([&] { return terminalGroup(move); }, 2000000);
            assert(groupOp(move).result.ok && releaseOperation(move));
            continue;
        }
        for (unsigned declaration = 0; declaration < 2; ++declaration) {
            if (declaration) { assert(clearGroup()); readyGroup({2, 1}); }
            assert(groupMember(*app, 1)->startSpeedRelationRequired);
            assert(!groupMember(*app, 2)->startSpeedRelationRequired);
            const auto before = frames.size();
            for (const auto mode : {Mode::STAGED, Mode::SEQUENTIAL}) {
                uint32_t move = 777;
                assert(!submitGroupMove(groupMove(Policy::LEAVE, mode), move) && move == 777);
            }
            pump(); assert(frames.size() == before);
        }
        // A different address still uses its own unrevoked startup policy.
        readyGroup({2});
        uint32_t move = 0;
        const auto before = frames.size();
        assert(submitGroupMove(groupMove(Policy::LEAVE), move));
        runUntil([&] { return terminalGroup(move); }, 2000000);
        assert(groupOp(move).result.ok && count(isAnyStart, before) == 1 && releaseOperation(move));
    }
}
void testUnchangedCandidateInvalidatesContradictedGroupProfile() {
    fresh({1, 2});
    drives[1].regs[0x0041] = drives[1].regs[0x0042] = drives[1].regs[0x0043] = 0;
    readSelectedFacts(); readyGroup({1, 2});
    auto& member = *groupMember(*app, 1);
    const auto historical = member.profile.view;
    const auto generation = member.axis.generation;
    Probe::ProfileCandidate candidate;
    candidate.address = 1; candidate.wordOrder = app->configuration.wordOrder;
    std::memcpy(candidate.words, historical.current + 1, sizeof(candidate.words));
    candidate.words[1] = drives[1].regs[0x0022] = 200;
    candidate.words[3] = drives[1].regs[0x0024] = 75;
    const auto from = frames.size();
    uint32_t candidateId = 0;
    assert(submitProfileCandidate(candidate, candidateId));
    Probe::ProfileCandidateResult result;
    runUntil([&] { return copyProfileCandidate(candidateId, result) && !result.stage.pending; });
    assert(result.stage.staged && result.unchanged && result.stage.execution == ActionExecution::NOT_TRANSMITTED);
    assert(count(isWrite, from) == 0 && result.before[2] == 200 && result.before[4] == 75);
    assert(!memberCurrent(*app, member) && !member.profile.view.ok && !member.profile.view.restored);
    assert(memberCurrent(*app, *groupMember(*app, 2)));
    assert(member.axis.generation == generation && member.profile.view.saved);
    assert(!member.startSpeedRelationRequired && !app->positionStartSpeedRelationRequired);
    assert(!std::memcmp(member.profile.view.original, historical.original, sizeof(historical.original)));
    assert(!std::memcmp(member.profile.view.current, historical.current, sizeof(historical.current)));
    assert(member.profile.view.rxLength == historical.rxLength &&
        !std::memcmp(member.profile.view.rx, historical.rx, historical.rxLength));
    assert(member.profile.view.closureQualified == historical.closureQualified &&
        member.profile.view.closureEarliestUs == historical.closureEarliestUs &&
        member.profile.view.closureLatestUs == historical.closureLatestUs);
    assert(releaseProfileCandidate(candidateId));
    uint32_t move = 777;
    const auto before = frames.size();
    for (const auto mode : {Mode::STAGED, Mode::SEQUENTIAL})
        assert(!submitGroupMove(groupMove(Policy::LEAVE, mode), move) && move == 777);
    pump(); assert(frames.size() == before);
    readyGroup({1});
    assert(groupMember(*app, 1)->profile.view.current[2] == 200);
    assert(submitGroupMove(groupMove(Policy::LEAVE), move));
    runUntil([&] { return terminalGroup(move); }, 2000000);
    assert(groupOp(move).result.ok && drives[1].regs[0x0022] == 200 && count(isAnyStart, before) == 1);
}
void testRemovedMemberSelectedWaitAndExpiry() {
    fresh(); readyGroup({1, 2}); enableDrive(1, 11); enableDrive(2, 12);
    const auto first = app->enableWaits[1], second = app->enableWaits[2];
    readyGroup({2}); // The first drive has no group member now.
    command("@21 useaddr 2\n"); command("@22 useaddr 1\n");
    assert(app->axis.target.address == 1 && app->bindingGeneration != first.target.generation);
    readSelectedFacts();
    assert(nowUs() < first.untilUs);
    PositionRequest position = groupMove(Policy::LEAVE).move.position;
    position.configurationGeneration = app->axis.generation;
    const auto from = frames.size(); uint32_t id = 777;
    hardware.clockStep = 0;
    advanceHardware(first.untilUs - 1);
    auto status = stagePositionMove(position, 60, id);
    assert(!status && status.detail == static_cast<int32_t>(Probe::Action::BUSY) && id == 777);
    status = submitMove(position, 60, id);
    assert(!status && status.detail == static_cast<int32_t>(Probe::Action::BUSY) && id == 777);
    assert(frames.size() == from && !app->owner.pending());
    sameEnableWait(1, first); sameEnableWait(2, second);
    advanceHardware(first.untilUs);
    assert(stagePositionMove(position, 60, id));
    Probe::PositionStageView staged;
    runUntil([&] { return copyPositionStage(id, staged) && !staged.pending; });
    assert(staged.staged && staged.execution == ActionExecution::ACKNOWLEDGED && count(isAnyStart, from) == 0);
    assert(submitMove(position, 60, id, MoveSetup::USE_STORED));
    MoveProgress progress;
    runUntil([&] { return moveProgress(id, progress) && !progress.pending; }, 2000000);
    assert(progress.completion == ActionCompletion::OBSERVED && count(isAnyStart, from) == 1);
}
void testEnableWaitCapacityAndAddressBounds() {
    auto list = addresses(16); list.push_back(247);
    fresh(list); readyGroup(addresses(15));
    for (uint8_t address = 1; address <= 15; ++address) enableDrive(address, 100 + address);
    readyGroup({16, 247}); enableDrive(16, 116); enableDrive(247, 347);
    assert(clearGroup());
    const uint64_t now = nowUs();
    for (unsigned address = 1; address <= 247; ++address) {
        const auto& wait = app->enableWaits[address];
        if (address <= 16 || address == 247) {
            assert(wait.target.address == address && wait.operationId && enableSettling(*app, static_cast<uint8_t>(address), now));
            assert(wait.write.txAccepted == 8 && wait.execution == ActionExecution::ACKNOWLEDGED);
        } else assert(!wait.operationId && !wait.untilUs && !enableSettling(*app, static_cast<uint8_t>(address), now));
    }
    ActionRequest enable; enable.kind = ActionKind::ENABLE; uint32_t unchanged = 777;
    const auto from = frames.size();
    for (const uint8_t invalid : {uint8_t(0), uint8_t(248), uint8_t(255)})
        assert(startAction(app, 400, invalid, enable, unchanged) == Probe::Action::INVALID && unchanged == 777);
    assert(frames.size() == from && !app->owner.pending());
}
void testPossibleEnableKeepsWaitAndEvidence() {
    for (bool incomplete : {false, true}) {
        fresh({1}); readyGroup({1});
        dropReply = [](const std::vector<uint8_t>& f) { return f[1] == 6; };
        if (incomplete) hardware.txCharacterUs = 0;
        command("@51 enable 1\n"); contains("\"result\":\"accepted\"");
        const auto id = app->latestOperationId;
        runUntil([&] { return terminal(*app, *findRecord(*app, id)); });
        const auto wait = app->enableWaits[1];
        assert(wait.operationId == id && wait.execution == ActionExecution::UNKNOWN && wait.write.txAccepted == 8);
        assert(wait.write.txComplete != incomplete && wait.untilUs - wait.observedUs == ENABLE_SETTLE_US);
        assert(findRecord(*app, id)->action.execution == ActionExecution::UNKNOWN);
        const auto writes = count(isWrite); pump(2000);
        assert(count(isWrite) == writes); sameEnableWait(1, wait);
        if (!incomplete) {
            dropReply = nullptr; recoverBus(52);
            assert(clearGroup()); readyGroup({1});
            sameEnableWait(1, wait);
            assert(count(isWrite) == writes); // Reads/recovery never replay enable or change its execution evidence.
        }
    }
    fresh({1}); readyGroup({1}); enableDrive(1, 61);
    const auto wait = app->enableWaits[1];
    ActionRequest enable; enable.kind = ActionKind::ENABLE; uint32_t id = 0;
    const auto from = frames.size();
    assert(startAction(app, 62, 1, enable, id) == Probe::Action::OK);
    assert(cancel(app, id) == Probe::Action::OK); // Cancel the second enable before dispatch.
    runUntil([&] { return terminal(*app, *findRecord(*app, id)); });
    assert(findRecord(*app, id)->action.execution == ActionExecution::NOT_TRANSMITTED);
    sameEnableWait(1, wait); assert(frames.size() == from);
}
void testAddressChangeCannotBypassEnableWait() {
    fresh({1}); readyGroup({1}); enableDrive(1, 71);
    const auto wait = app->enableWaits[1];
    Probe::CommunicationCommand command;
    command.kind = Probe::CommunicationCommandKind::BEGIN;
    command.address = 1; command.request.field = ESS::CommunicationField::ADDRESS; command.request.address = 2;
    Probe::CommunicationView view;
    const auto from = frames.size();
    assert(communication(app, &command, view) == Probe::Action::BUSY);
    assert(!app->owner.commissioningOwned() && frames.size() == from);
    sameEnableWait(1, wait); assert(!app->enableWaits[2].operationId);
    advanceHardware(wait.untilUs);
    // Expiry removes this guard, not the independent commissioning prerequisites.
    assert(communication(app, &command, view) == Probe::Action::UNAVAILABLE);
    assert(!app->owner.commissioningOwned() && frames.size() == from);
}
void testIdleOnlyDeclarationAndExclusiveSequence() {
    fresh(); readyGroup();
    uint32_t id = 0, unchanged = 77;
    assert(submitGroupMove(groupMove(Policy::LEAVE), id));
    // While stages and starts are being sent, only urgent stops interleave.
    assert(groupSequencing(*app));
    const uint8_t again[] = {1, 2};
    assert(!declareGroup(again, 2, unchanged) && unchanged == 77 && !clearGroup());
    assert(!submitGroupStateRead(unchanged) && unchanged == 77);
    command("@1 probe 5\n"); contains("\"result\":\"busy\"");
    command("@2 read state 5\n"); contains("\"result\":\"busy\"");
    command("@3 enable 5\n"); contains("\"result\":\"busy\"");
    runUntil([&] { return terminalGroup(id); }, 2000000);
    assert(releaseOperation(id) && clearGroup() && !app->group.count);
    // Wiring declarations are member-local and survive re-declaration.
    readyGroup();
    assert(declareGroupWiring(4, 0, InputWiring::UNCONNECTED) && declareGroupWiring(4, 5, InputWiring::UNCONNECTED));
    assert(!declareGroupWiring(9, 0, InputWiring::UNCONNECTED) && !declareGroupWiring(4, 6, InputWiring::UNCONNECTED));
    command("@4 group wiring 4 x1 unconnected\n"); contains("\"command\":\"group-wiring\""); contains("\"result\":\"done\"");
    declare({1, 4});
    const auto* member = groupMember(*app, 4);
    assert(member->inputWiring[0] == InputWiring::UNCONNECTED && member->inputWiring[1] == InputWiring::UNCONNECTED);
    assert(member->outputWiring[1] == InputWiring::UNCONNECTED && member->configuration.wiring[0] == InputWiring::UNCONNECTED);
    assert(app->inputWiring[0] == InputWiring::UNCONNECTED); // The selected axis keeps its own declaration.
}
// The reusable stage piece on the selected axis: stage, then a stored start only.
void testSelectedAxisStageThenStoredStart() {
    fresh({1});
    for (const char* line : {"@1 read config\n", "@2 motion-profile read\n", "@3 read state\n"}) {
        command(line);
        runUntil([] { return !app->owner.active() && !app->owner.pending() && !reading(*app) && !app->motionProfile.view.pending; });
    }
    PositionRequest position; position.value = Rational(150); position.unit = PositionUnit::STEPS;
    position.frame = CoordinateFrame::NATIVE; position.relative = true; position.configurationGeneration = app->axis.generation;
    auto from = frames.size();
    uint32_t stage = 0, unchanged = 77;
    assert(stagePositionMove(position, 60, stage) && stage);
    Probe::PositionStageView view;
    assert(!copyPositionStage(stage + 1, view));
    runUntil([&] { return copyPositionStage(stage, view) && !view.pending; });
    assert(view.staged && view.execution == ActionExecution::ACKNOWLEDGED && view.readbackKnown && view.txComplete);
    assert(count(isStageFrame, from) == 2 && count(isAnyStart, from) == 0 && drives[1].regs[0x0024] == 150);
    assert(drives[1].regs[0x0020] == 10); // Start speed is never written.
    // The stored start sends only the start word.
    from = frames.size();
    uint32_t move = 0;
    assert(submitMove(position, 60, move, MoveSetup::USE_STORED));
    MoveProgress progress;
    runUntil([&] { return moveProgress(move, progress) && !progress.pending; }, 2000000);
    assert(progress.completion == ActionCompletion::OBSERVED && count(isStageFrame, from) == 0);
    assert(count(isAnyStart, from) == 1 && releaseMove(move));
    // A move's staging write ends the witness: a stored start is refused before TX.
    command("@4 read state\n");
    runUntil([] { return !reading(*app); });
    assert(stagePositionMove(position, 60, stage));
    runUntil([&] { return copyPositionStage(stage, view) && !view.pending; });
    storedWordsChanged(*app, 1);
    from = frames.size();
    assert(!submitMove(position, 60, unchanged, MoveSetup::USE_STORED) && unchanged == 77 && frames.size() == from);
    // A stop before the write leaves cancels it unsent: nothing is staged.
    assert(stagePositionMove(position, 60, stage) && axisReserved(*app, 1));
    from = frames.size();
    ActionRequest stop; stop.kind = ActionKind::STOP; stop.stop.behavior = StopBehavior::DIRECT;
    uint32_t stopped = 0;
    assert(startAction(app, 9, 1, stop, stopped) == Probe::Action::OK);
    runUntil([&] { return copyPositionStage(stage, view) && !view.pending; });
    assert(view.cancelled && !view.staged && view.execution == ActionExecution::NOT_TRANSMITTED);
    assert(count(isStageFrame, from) == 0);
    runUntil([&] { return !findRecord(*app, stopped) || terminal(*app, *findRecord(*app, stopped)); });
    // A lost stage reply leaves the words uncertain: no witness, a stored start is refused.
    command("@5 read state\n");
    runUntil([] { return !reading(*app); });
    dropReply = [](const std::vector<uint8_t>& f) { return f[1] == 16; };
    assert(stagePositionMove(position, 60, stage) && axisReserved(*app, 1));
    runUntil([&] { return copyPositionStage(stage, view) && !view.pending; }, 2000000);
    assert(!view.staged && view.execution == ActionExecution::UNKNOWN && app->rememberedMoveGeneration == 0);
}
void testStopDuringStageReadbackKeepsEvidenceWithoutWitness() {
    fresh({1});
    for (const char* line : {"@1 read config\n", "@2 motion-profile read\n", "@3 read state\n"}) {
        command(line);
        runUntil([] { return !app->owner.active() && !app->owner.pending() && !reading(*app) && !app->motionProfile.view.pending; });
    }
    PositionRequest position;
    position.value = Rational(150);
    position.unit = PositionUnit::STEPS;
    position.frame = CoordinateFrame::NATIVE;
    position.relative = true;
    position.configurationGeneration = app->axis.generation;
    uint32_t stage = 0;
    const auto from = frames.size();
    assert(stagePositionMove(position, 60, stage));
    runUntil([] { return app->stage.phase == 2 && app->stage.request.owner &&
        app->owner.txAccepted(app->stage.request) != 0; });
    ActionRequest stop;
    stop.kind = ActionKind::STOP;
    stop.stop.behavior = StopBehavior::DIRECT;
    uint32_t stopped = 0;
    assert(startAction(app, 9, 1, stop, stopped) == Probe::Action::OK);
    Probe::PositionStageView view;
    runUntil([&] { return copyPositionStage(stage, view) && !view.pending; });
    assert(view.cancelled && !view.staged && !view.status.isOk());
    assert(view.execution == ActionExecution::ACKNOWLEDGED && view.readbackKnown);
    assert(view.writeEvidence.txAccepted == 19 && view.writeEvidence.length == 8 && view.writeEvidence.status);
    assert(view.readEvidence.txAccepted == 8 && view.readEvidence.length == 15 && view.readEvidence.status);
    assert(std::memcmp(view.words, view.readback, sizeof(view.words)) == 0);
    assert(app->rememberedMoveGeneration == 0);
    runUntil([&] { return terminal(*app, *findRecord(*app, stopped)); });
    assert(count(isStageFrame, from) == 2 && count(isAnyStart, from) == 0);
}
void testGroupStageCannotRenewExpiredParentDeadline() {
    fresh({1, 2}); readyGroup({1, 2});
    app->options.moveTimeoutMs = 1;
    uint32_t id = 0;
    const auto from = frames.size();
    assert(submitGroupMove(groupMove(Policy::LEAVE), id));
    const auto* first = findRecord(*app, groupOp(id).result.members[0].operationId);
    assert(first && !app->owner.pending()); // Group staging has not been admitted yet.
    const uint64_t deadline = first->deadlineUs;
    advanceHardware(deadline);
    runUntil([&] { return terminalGroup(id); });
    const auto& result = groupOp(id).result;
    assert(!result.ok && result.aborted && result.failedAddress == 1);
    assert(result.members[0].stageState == ActionState::FAILED);
    assert(frames.size() == from && count(isAnyStart, from) == 0);
    assert(findRecord(*app, result.members[0].operationId)->deadlineUs == deadline);
}
void testQueuedStageConfigurationChangePreventsTx() {
    for (bool group : {false, true}) {
        fresh({1});
        uint32_t id = 0;
        if (group) {
            readyGroup({1});
            assert(submitGroupMove(groupMove(Policy::LEAVE), id));
            serviceGroup(*app, nowUs()); // Admit the first stage, without servicing transport.
            assert(app->group.members[0].stage.view.pending && app->owner.pending());
            invalidateMemberConfiguration(*app, 1, nowUs());
        } else {
            readSelectedFacts();
            auto position = groupMove(Policy::LEAVE).move.position;
            position.configurationGeneration = app->axis.generation;
            assert(stagePositionMove(position, 60, id));
            assert(app->stage.view.pending && app->owner.pending());
            invalidateAxis(*app);
        }
        const auto from = frames.size();
        runUntil([&] { return group ? terminalGroup(id) : !app->stage.view.pending; });
        const auto& stage = group ? app->group.members[0].stage.view : app->stage.view;
        assert(stage.cancelled && !stage.staged && stage.execution == ActionExecution::NOT_TRANSMITTED);
        assert(stage.writeEvidence.event == ReadEventKind::CANCEL && !stage.writeEvidence.txAccepted);
        assert(frames.size() == from && count(isAnyStart, from) == 0);
    }
}
void testStageClosureEvidenceSurvivesDelayedDelivery() {
    for (bool cancelled : {false, true}) {
        fresh({1}); readSelectedFacts();
        auto position = groupMove(Policy::LEAVE).move.position;
        position.configurationGeneration = app->axis.generation;
        uint32_t id = 0;
        const auto from = frames.size();
        assert(stagePositionMove(position, 60, id));
        const uint64_t deadline = app->stage.view.deadlineUs;
        runUntil([] { return app->stage.phase == 2 && app->stage.request.owner; });
        // Run the actual owner/runner to completion while delaying application harvest.
        const auto request = app->stage.request;
        for (unsigned i = 0; i < 20000 && !app->owner.result(request); ++i) {
            advanceHardware(hardware.time + 20);
            app->owner.service(uart.sample()); serveBus();
        }
        const auto* result = app->owner.result(request);
        assert(result && result->outcome == Rtu::Outcome::SUCCESS);
        const uint64_t closure = result->transport.closureLatestUs;
        assert(closure <= deadline);
        advanceHardware(deadline + 1);
        if (cancelled) cancelPositionStages(*app, 1, nowUs());
        serviceSelectedStage(*app, nowUs());
        Probe::PositionStageView view; assert(copyPositionStage(id, view));
        assert(!view.pending && view.deadlineUs == deadline && view.readbackKnown);
        assert(view.staged != cancelled && view.cancelled == cancelled);
        assert(view.execution == ActionExecution::ACKNOWLEDGED && view.readEvidence.status);
        assert(view.readEvidence.latestUs == closure && view.readEvidence.deliveredUs > deadline);
        assert(view.readEvidence.length == 15 && view.readEvidence.raw[0] == 1 && view.readEvidence.raw[1] == 3);
        assert((app->rememberedMoveGeneration != 0) != cancelled);
        assert(count(isStageFrame, from) == 2 && count(isAnyStart, from) == 0);
    }
}
void testActiveOriginSharedReadiness() {
    fresh();
    for (auto& drive : drives) drive.second.regs[8] = 1; // Origin remains active after homing.
    readyGroup();
    uint32_t id = 999; assert(submitGroupMove(groupMove(Policy::LEAVE), id));
    runUntil([&] { return terminalGroup(id); }, 2000000);
    assert(groupOp(id).result.ok);
    // Active limits/release/stops and unqualified external starts still refuse.
    for (uint16_t function : {uint16_t(2), uint16_t(3), uint16_t(4), uint16_t(5), uint16_t(6), uint16_t(7), uint16_t(17)}) {
        fresh(); drives[2].regs[0x41] = function; drives[2].regs[8] = function <= 6 ? 1 : 0;
        readyGroup(); const auto before = frames.size(); id = 999;
        assert(!submitGroupMove(groupMove(Policy::LEAVE), id) && id == 999);
        pump(); assert(frames.size() == before);
    }
}
void testMemberIntentMappingAndCopiedFrames() {
    for (const auto mode : {Mode::STAGED, Mode::SEQUENTIAL}) {
        fresh();
        drives[1].regs[0x0011] = 1000; drives[2].regs[0x0011] = 2000; drives[4].regs[0x0011] = 4000;
        drives[2].regs[0x0019] = 0; // High word first, unlike the other members.
        readyGroup();
        auto request = memberMoves({4, 1, 2}, Policy::LEAVE, mode);
        memberMove(request, 1).position.value = Rational(65538);
        memberMove(request, 2).position.value = Rational(-65539);
        memberMove(request, 2).speedRpm = 80;
        memberMove(request, 4).position.value = Rational(37);
        memberMove(request, 4).speedRpm = 100;
        const auto original = request;
        const auto from = frames.size(); uint32_t id = 0;
        assert(submitGroupMoves(request, id));
        for (auto& intent : request.members) { intent.address = 99; intent.move.position.value = Rational(9999); }
        runUntil([&] { return terminalGroup(id); }, 2000000);
        assert(groupOp(id).result.ok && groupOp(id).result.individual);
        exactStage(1, 60, 0x0002, 0x0001, from);
        exactStage(2, 80, 0xFFFE, 0xFFFD, from);
        exactStage(4, 100, 37, 0, from);
        const int64_t targets[] = {65538, -65539, 37};
        std::vector<uint8_t> startOrder;
        std::size_t lastStage = 0, firstStart = frames.size();
        for (std::size_t i = from; i < frames.size(); ++i) {
            if (isStageFrame(frames[i])) lastStage = i;
            if (isAnyStart(frames[i])) {
                if (firstStart == frames.size()) firstStart = i;
                startOrder.push_back(frames[i][0]);
                assert(frames[i] == crc({frames[i][0], 6, 0, 0x27, 0, 1}));
            }
        }
        assert((startOrder == std::vector<uint8_t>{1, 2, 4}));
        if (mode == Mode::STAGED) assert(lastStage < firstStart && startsBackToBack(from, 3));
        else assert(count(isStageFrame, from) == 3);
        unsigned index = 0;
        for (const uint8_t address : {1, 2, 4}) {
            const auto& result = memberResult(id, address);
            auto expected = original;
            sameIntent(result.request, memberMove(expected, address));
            assert(result.effectiveNative == targets[index++] && result.completion == ActionCompletion::OBSERVED);
        }
        noWriteOutside({1, 2, 4}, from);
    }
}

void testMemberRequestRejectsShapeAndInvalidLast() {
    fresh(); readyGroup();
    const auto valid = memberMoves({1, 2, 4});
    for (unsigned failure = 0; failure < 18; ++failure) {
        auto invalid = valid;
        switch (failure) {
        case 0: invalid.count = 0; break;
        case 1: invalid.count = Probe::GROUP_CAPACITY + 1; break;
        case 2: invalid.count = 2; break;
        case 3: invalid.members[2].address = 1; break;
        case 4: invalid.members[2].address = 3; break;
        case 5: invalid.onFailure = Policy::NONE; break;
        case 6: invalid.start = static_cast<Mode>(255); break;
        case 7: ++invalid.declaration; break;
        case 8: ++invalid.bindingGeneration; break;
        case 9: ++invalid.members[2].move.position.configurationGeneration; break;
        case 10: invalid.members[2].move.position.value.denominator = 0; break;
        case 11: invalid.members[2].move.position.value = Rational(1, 2); break;
        case 12: invalid.members[2].move.speedRpm = 0; break;
        case 13: invalid.members[2].move.position.relative = false; invalid.members[2].move.position.wrapped = true; break;
        case 14: invalid.members[2].move.setup = MoveSetup::USE_STORED; break;
        case 15: invalid.members[2].move.position.value = Rational(0x10000000); break;
        case 16: invalid.members[2].move.position.radians = std::numeric_limits<double>::quiet_NaN(); break;
        case 17: invalid.members[2].move.position.maximumApproximationError = std::numeric_limits<double>::infinity(); break;
        }
        assertRejectedWithoutTraffic(invalid);
    }
    // Earlier valid members cannot leak writes when the last member's active
    // subdivision makes its rate exceed the shared operating limit.
    fresh(); drives[4].regs[0x0011] = 51200; readyGroup();
    assertRejectedWithoutTraffic(memberMoves({1, 2, 4})); // 60 rpm exceeds 50,000 increments/s.
    fresh(); readyGroup();
    configureMember(4, Probe::AxisField::NATIVE_LIMITS, -100, 100);
    auto invalid = memberMoves({1, 2, 4});
    memberMove(invalid, 4).position.value = Rational(101); memberMove(invalid, 4).position.relative = false;
    assertRejectedWithoutTraffic(invalid);
}

void testMemberScalesAreIndependent() {
    fresh(); drives[2].regs[0x0011] = 2000; drives[4].regs[0x0011] = 4000; readyGroup();
    const auto traffic = hardware.writes;
    const auto selectedScale = app->axis.units.commandStepsPerMotorTurn;
    configureMember(1, Probe::AxisField::COMMAND_SCALE, 1000);
    configureMember(2, Probe::AxisField::COMMAND_SCALE, 2000);
    configureMember(4, Probe::AxisField::COMMAND_SCALE, 4000);
    assert(hardware.writes == traffic && app->axis.units.commandStepsPerMotorTurn.numerator == selectedScale.numerator &&
        app->axis.units.commandStepsPerMotorTurn.denominator == selectedScale.denominator);
    auto request = memberMoves();
    for (auto& intent : request.members) {
        intent.move.position.unit = PositionUnit::TURNS; intent.move.position.frame = CoordinateFrame::MOTOR;
    }
    memberMove(request, 1).position.value = Rational(1, 2);
    memberMove(request, 2).position.value = Rational(-1, 4);
    memberMove(request, 4).position.value = Rational(3, 8);
    const auto from = frames.size(); uint32_t id = 0;
    assert(submitGroupMoves(request, id)); runUntil([&] { return terminalGroup(id); }, 2000000);
    assert(groupOp(id).result.ok);
    assert(memberResult(id, 1).effectiveNative == 500 && memberResult(id, 2).effectiveNative == -500 &&
        memberResult(id, 4).effectiveNative == 1500);
    exactStage(1, 60, 500, 0, from); exactStage(2, 60, 0xFE0C, 0xFFFF, from); exactStage(4, 60, 1500, 0, from);
    assert(releaseOperation(id));
    // A new declaration does not infer scales or origins from raw registers.
    readyGroup();
    for (const uint8_t address : {1, 2, 4}) {
        const auto view = memberView(address);
        assert(!view.axis.units.commandStepsPerMotorTurn.numerator && !view.axis.originKnown && !view.referenceCurrent);
    }
    request = memberMoves({1, 2, 4});
    memberMove(request, 4).position.unit = PositionUnit::TURNS; memberMove(request, 4).position.frame = CoordinateFrame::MOTOR;
    assertRejectedWithoutTraffic(request);
}

void testMemberAbsoluteOriginsAndMixedStarts() {
    for (const auto mode : {Mode::STAGED, Mode::SEQUENTIAL}) {
        fresh(); readyGroup();
        configureMember(1, Probe::AxisField::COMMAND_SCALE, 1000);
        configureMember(2, Probe::AxisField::COMMAND_SCALE, 2000);
        configureMember(4, Probe::AxisField::COMMAND_SCALE, 4000);
        originMember(1, 100, 50); originMember(2, 1000, 900); originMember(4, -100, -150);
        auto request = memberMoves({4, 1, 2}, Policy::LEAVE, mode);
        for (auto& intent : request.members) {
            intent.move.position.relative = false; intent.move.position.unit = PositionUnit::TURNS;
            intent.move.position.frame = CoordinateFrame::MOTOR; intent.move.position.value = Rational(1);
        }
        memberMove(request, 1).position.relative = true;
        const auto from = frames.size(); uint32_t id = 0;
        assert(submitGroupMoves(request, id));
        const auto* first = findRecord(*app, memberResult(id, 1).operationId);
        const auto* second = findRecord(*app, memberResult(id, 2).operationId);
        assert(first && second && first->move.reference.nativePosition == 50 && second->move.reference.nativePosition == 900);
        runUntil([] { return !groupMember(*app, 1)->coordinate.reference.nativeKnown; });
        assert(groupMember(*app, 2)->coordinate.reference.nativeKnown && groupMember(*app, 4)->coordinate.reference.nativeKnown);
        assert(first->move.reference.nativeKnown && first->move.reference.nativePosition == 50);
        runUntil([&] { return terminalGroup(id); }, 2000000);
        assert(groupOp(id).result.ok && memberResult(id, 1).effectiveNative == 1000 &&
            memberResult(id, 2).effectiveNative == 3000 && memberResult(id, 4).effectiveNative == 3900);
        for (const uint8_t address : {1, 2, 4}) {
            const auto start = crc({address, 6, 0, 0x27, 0, static_cast<uint8_t>(address == 1 ? 1 : 5)});
            assert(count([&](const std::vector<uint8_t>& frame) { return frame == start; }, from) == 1);
            const auto view = memberView(address);
            assert(view.axis.originKnown && !view.referenceCurrent);
            assert(memberResult(id, address).completion == ActionCompletion::OBSERVED);
        }
        if (mode == Mode::STAGED) assert(startsBackToBack(from, 3));
        noWriteOutside({1, 2, 4}, from);
    }
    // Device-native absolute retains the ordinary reference-free core path.
    fresh(); readyGroup();
    auto request = memberMoves();
    for (auto& intent : request.members) intent.move.position.relative = false;
    uint32_t id = 0; assert(submitGroupMoves(request, id));
    runUntil([&] { return terminalGroup(id); }, 2000000); assert(groupOp(id).result.ok);
    for (const uint8_t address : {1, 2, 4}) assert(!memberView(address).axis.originKnown);
}
void testMemberReferenceProvenanceAndOutputOnError() {
    fresh(); readyGroup();
    configureMember(4, Probe::AxisField::COMMAND_SCALE, 1000);
    auto request = memberMoves({1, 2, 4});
    auto& absolute = memberMove(request, 4).position;
    absolute.relative = false; absolute.unit = PositionUnit::TURNS; absolute.frame = CoordinateFrame::MOTOR;
    absolute.value = Rational(1);
    assertRejectedWithoutTraffic(request); // Raw zero feedback does not establish a host origin.
    const auto good = memberReference(4, 25);
    assert(installGroupReference(good));
    const auto kept = memberView(4);
    const auto traffic = hardware.writes;
    for (unsigned failure = 0; failure < 13; ++failure) {
        auto invalid = good;
        switch (failure) {
        case 0: ++invalid.declaration; break;
        case 1: ++invalid.configurationOperationId; break;
        case 2: ++invalid.motionOperationId; break;
        case 3: ++invalid.feedbackOperationId; break;
        case 4: ++invalid.reference.target.id; break;
        case 5: ++invalid.reference.target.generation; break;
        case 6: ++invalid.reference.configurationGeneration; break;
        case 7: invalid.reference.nativeKnown = false; break;
        case 8: invalid.reference.stationary = false; break;
        case 9: invalid.reference.idle = false; break;
        case 10: invalid.reference.basis = RelativeBasis::COMMANDED; break;
        case 11: invalid.reference.source = ScaleSource::UNKNOWN; break;
        case 12: invalid.reference.observedUs = nowUs() + 1; break;
        }
        assert(!installGroupReference(invalid));
        const auto after = memberView(4);
        assert(after.referenceCurrent && after.reference.nativePosition == 25 &&
            after.reference.observedUs == kept.reference.observedUs && after.axis.generation == kept.axis.generation);
        assert(hardware.writes == traffic);
    }
    Probe::AxisCommand origin; origin.kind = Probe::AxisCommandKind::ORIGIN; origin.value = Rational(100);
    Probe::AxisView unchanged; unchanged.configuration.generation = 777;
    assert(!configureGroupAxis(2, origin, unchanged) && unchanged.configuration.generation == 777);
    originMember(4, 100, 25);
    configureMember(4, Probe::AxisField::SOFT_LIMITS, -500, 1000);
    request = memberMoves({1, 2, 4});
    auto& bounded = memberMove(request, 4).position;
    bounded.relative = false; bounded.unit = PositionUnit::TURNS; bounded.frame = CoordinateFrame::MOTOR;
    bounded.value = Rational(1); // Endpoint 1100 is beyond this member's limit.
    assertRejectedWithoutTraffic(request);
    const auto stale = memberReference(4, 25);
    readGroupState();
    assert(!installGroupReference(stale)); // New read IDs cannot refresh the old witness.
    assert(installGroupReference(memberReference(4, 25)));
    assert(revokeGroupReference(4));
    assert(!memberView(4).referenceCurrent && !memberView(4).axis.originKnown);
    assert(!memberView(1).referenceCurrent && !memberView(2).referenceCurrent);
}

// Wired bench 20 K1 (image G): after a group stop the first member read showed
// RUNNING clear with the speed word 0x000C still 33 at an unchanged position;
// that late word revoked the member's coordinates and refused the return plan.
void testLateSpeedWordAfterGroupStopKeepsMemberOrigin() {
    for (const bool groupRead : {true, false}) {
        fresh(); readyGroup();
        for (const uint8_t a : {1, 2, 4}) configureMember(a, Probe::AxisField::COMMAND_SCALE, 1000);
        originMember(1, 100, 50); originMember(2, 1000, 900); originMember(4, -100, -150);
        auto request = memberMoves({1, 2, 4}, Policy::LEAVE, Mode::STAGED);
        for (auto& intent : request.members) {
            intent.move.position.relative = false; intent.move.position.unit = PositionUnit::TURNS;
            intent.move.position.frame = CoordinateFrame::MOTOR; intent.move.position.value = Rational(1);
        }
        uint32_t id = 0; assert(submitGroupMoves(request, id));
        runUntil([] { return drives[1].startsApplied && drives[2].startsApplied && drives[4].startsApplied; });
        for (const uint8_t a : {1, 2, 4}) drives[a].runningReads = 100000; // Running until the stop.
        uint32_t stop = 0; assert(submitGroupStop(StopBehavior::DIRECT, stop));
        runUntil([&] { return terminalGroup(id) && terminalGroup(stop); }, 2000000);
        const auto generation = memberView(1).configurationGeneration;
        assert(memberView(1).axis.originKnown);
        drives[1].regs[0x0007] = 0; drives[1].regs[0x000C] = 33; // Stopped short; the speed word lags.
        if (groupRead) {
            uint32_t read = 0; assert(submitGroupStateRead(read));
            runUntil([&] { return terminalGroup(read); }); // The member's sample may report contradicted state.
        } else {
            uint32_t read = 0; assert(typedRead(app, 0, 1, ESS::ReadKind::STATE, read) == Probe::Action::OK);
            runUntil([&] { const auto* r = findRecord(*app, read); return r && r->read.state != ReadState::ACTIVE; });
            releaseOperation(read);
        }
        assert(memberView(1).axis.originKnown && memberView(1).configurationGeneration == generation);
        drives[1].regs[0x000C] = 0; readGroupState(); // The first stationary sample ends the pending observation.
        assert(memberView(1).axis.originKnown && memberView(1).configurationGeneration == generation);
        for (const uint8_t a : {1, 2, 4})
            assert(installGroupReference(memberReference(a, static_cast<int32_t>(memberView(a).rawPosition))));
        auto back = memberMoves({1, 2, 4}, Policy::LEAVE, Mode::STAGED);
        for (auto& intent : back.members) {
            intent.move.position.relative = false; intent.move.position.unit = PositionUnit::TURNS;
            intent.move.position.frame = CoordinateFrame::MOTOR; intent.move.position.value = Rational(0);
        }
        drives[1].regs[0x0007] = 1; // Enabled and in position again for the return move.
        releaseOperation(id); releaseOperation(stop); // Free result capacity (RESULTS_FULL otherwise).
        uint32_t ret = 0; assert(submitGroupMoves(back, ret)); // Admitted: the origin was kept.
        runUntil([&] { return terminalGroup(ret); }, 2000000);
        assert(groupOp(ret).result.ok);
        assert(memberView(1).axis.originKnown && memberView(1).configurationGeneration == generation);
    }
    // Control: the same speed word on an idle member (no motion of ours pending) still revokes.
    fresh(); readyGroup();
    originMember(4, 100, 123);
    drives[4].regs[0x000C] = 33;
    uint32_t read = 0; assert(submitGroupStateRead(read));
    runUntil([&] { return terminalGroup(read); });
    assert(!memberView(4).axis.originKnown);
}
void testMemberReferenceExpiresAndContradictionsRevokeIt() {
    fresh(); readyGroup();
    assert(installGroupReference(memberReference(4, 123, 100000)));
    advanceHardware(hardware.time + 100001); pump();
    assert(!memberView(4).referenceCurrent);
    auto request = memberMoves({1, 2, 4});
    assertRejectedWithoutTraffic(request);
    memberMove(request, 4).position.relative = false;
    assertRejectedWithoutTraffic(request); // Stale installed evidence cannot silently disappear for admission.
    fresh(); readyGroup();
    originMember(4, 100, 123);
    drives[4].regs[0x0007] = 4;
    uint32_t read = 0; assert(submitGroupStateRead(read));
    runUntil([&] { return terminalGroup(read); });
    assert(!memberView(4).referenceCurrent);
    drives[4].regs[0x0007] = 1; readGroupState();
    assert(!memberView(4).referenceCurrent); // Stopped feedback does not manufacture a new relation.
    assert(installGroupReference(memberReference(4, 125)));
    invalidateMemberConfiguration(*app, 4, nowUs());
    assert(!memberView(4).referenceCurrent && !memberView(4).axis.originKnown);
}

void testMemberPlanIsInertAndBoundToConfiguration() {
    fresh(); readyGroup();
    const auto traffic = hardware.writes;
    assert(!beginGroupPlan(Mode::STAGED, Policy::NONE));
    assert(beginGroupPlan(Mode::STAGED, Policy::LEAVE));
    assert(!beginGroupPlan(Mode::STAGED, Policy::STOP_FAST));
    auto move = groupMove(Policy::LEAVE).move;
    assert(addGroupPlanMember(4, move));
    assert(!addGroupPlanMember(4, move) && !addGroupPlanMember(3, move));
    auto nonfinite = move; nonfinite.position.radians = std::numeric_limits<double>::quiet_NaN();
    assert(!addGroupPlanMember(1, nonfinite));
    nonfinite = move; nonfinite.position.maximumApproximationError = std::numeric_limits<double>::infinity();
    assert(!addGroupPlanMember(1, nonfinite));
    uint32_t id = 777; assert(!submitGroupPlan(id) && id == 777);
    Probe::GroupView view; assert(inspectGroup(view) && view.planActive && view.plan.count == 1);
    move.position.value = Rational(10); assert(addGroupPlanMember(1, move));
    move.position.value = Rational(-20); assert(addGroupPlanMember(2, move));
    pump(500); assert(hardware.writes == traffic);
    configureMember(2, Probe::AxisField::NATIVE_LIMITS, -1000, 1000);
    assert(!submitGroupPlan(id) && id == 777 && hardware.writes == traffic);
    assert(inspectGroup(view) && view.planActive && view.plan.count == 3);
    assert(clearGroupPlan());
    assert(beginGroupPlan(Mode::SEQUENTIAL, Policy::STOP_FAST));
    for (const uint8_t address : {4, 1, 2}) {
        move.position.value = Rational(address * 10); assert(addGroupPlanMember(address, move));
    }
    assert(hardware.writes == traffic && submitGroupPlan(id) && id != 777);
    assert(inspectGroup(view) && !view.planActive);
    runUntil([&] { return terminalGroup(id); }, 2000000);
    assert(groupOp(id).result.ok && groupOp(id).result.individual);
}

void testMemberStartLossAfterExecutionNeverReplays() {
    for (const auto mode : {Mode::STAGED, Mode::SEQUENTIAL})
        for (const auto policy : {Policy::LEAVE, Policy::STOP_NORMAL, Policy::STOP_FAST}) {
            fresh(); readyGroup();
            auto request = memberMoves({4, 1, 2}, policy, mode);
            memberMove(request, 1).position.value = Rational(50);
            memberMove(request, 2).position.value = Rational(-25);
            memberMove(request, 4).position.relative = false; memberMove(request, 4).position.value = Rational(100);
            const auto from = frames.size();
            loseAcknowledgement = [](const std::vector<uint8_t>& frame) { return isStart(frame, 2); };
            uint32_t id = 0; assert(submitGroupMoves(request, id));
            runUntil([&] { return terminalGroup(id); }, 4000000);
            const auto& first = memberResult(id, 1); const auto& middle = memberResult(id, 2); const auto& last = memberResult(id, 4);
            assert(groupOp(id).result.aborted && groupOp(id).result.failedAddress == 2);
            assert(first.start == Probe::GroupStart::STARTED && middle.start == Probe::GroupStart::UNKNOWN);
            assert(middle.uncertain && middle.execution == ActionExecution::UNKNOWN && middle.effectiveNative == -25);
            assert(last.start == Probe::GroupStart::NOT_SENT && last.outcome == ActionOutcome::CANCELLED && !last.uncertain);
            assert(drives[1].startsApplied == 1 && drives[2].startsApplied == 1 && drives[4].startsApplied == 0);
            assert(count([](const std::vector<uint8_t>& frame) { return isStart(frame, 2); }, from) == 1);
            const bool stop = policy != Policy::LEAVE;
            for (const uint8_t address : {1, 2, 4}) {
                assert(count([&](const std::vector<uint8_t>& frame) { return isStop(frame, address); }, from) ==
                    (stop && address != 4 ? 1u : 0u));
                sameIntent(memberResult(id, address).request, memberMove(request, address));
            }
            if (stop) assert(middle.stopExecution == ActionExecution::ACKNOWLEDGED);
            noWriteOutside({1, 2, 4}, from);
            loseAcknowledgement = nullptr;
            recoverBus(99); pump(1000);
            assert(drives[2].startsApplied == 1 && middle.start == Probe::GroupStart::UNKNOWN && middle.uncertain);
        }
}

uint32_t retainConsoleGroupStop(unsigned commandId) {
    Serial.writeCapacity = 0;
    while (app->outputCount < OUTPUT_LINES) assert(emit(app, "blocked", 7));
    Probe::GroupCommand request; request.kind = Probe::GroupCommandKind::STOP;
    request.stop = StopBehavior::DIRECT;
    Probe::GroupReply reply;
    assert(groupCommand(app, commandId, request, reply) == Probe::Action::OK);
    runUntil([&] { return terminalGroup(reply.operationId); }, 4000000);
    assert(groupOp(reply.operationId).result.ok && !groupOp(reply.operationId).delivered);
    return reply.operationId;
}
void testStopPolicyCapacityBeforeAnyWrite() {
    const auto all = addresses(Probe::GROUP_CAPACITY);
    for (const auto mode : {Mode::STAGED, Mode::SEQUENTIAL})
        for (const auto policy : {Policy::STOP_NORMAL, Policy::STOP_FAST})
            for (const bool individual : {false, true}) {
                fresh(all); readyGroup(all);
                const uint32_t old[] = {retainConsoleGroupStop(901), retainConsoleGroupStop(902)};
                readGroupState();
                const auto from = frames.size(); const auto nextId = app->nextOperationId;
                uint32_t id = 777;
                const auto refused = individual ? submitGroupMoves(memberMoves(all, policy, mode), id) :
                    submitGroupMove(groupMove(policy, mode), id);
                assert(!refused && refused.detail == static_cast<int32_t>(Probe::Action::RESULTS_FULL));
                assert(id == 777 && app->nextOperationId == nextId && frames.size() == from);
                // LEAVE needs no fallback stop storage and preserves both old batches.
                assert(individual ? submitGroupMoves(memberMoves(all, Policy::LEAVE, mode), id) :
                    submitGroupMove(groupMove(Policy::LEAVE, mode), id));
                runUntil([&] { return terminalGroup(id); }, 4000000);
                assert(groupOp(id).result.ok);
                for (const auto oldId : old) {
                    assert(!groupOp(oldId).delivered);
                    for (const auto address : all) {
                        const auto* record = findRecord(*app, memberResult(oldId, address).operationId);
                        assert(record && !record->delivered && confirmedStop(*record));
                    }
                }
            }
}
void testStopPolicyUsesRemainingCapacity() {
    const auto all = addresses(Probe::GROUP_CAPACITY);
    for (const auto mode : {Mode::STAGED, Mode::SEQUENTIAL})
        for (const auto policy : {Policy::STOP_NORMAL, Policy::STOP_FAST}) {
            fresh(all); readyGroup(all);
            const auto old = retainConsoleGroupStop(903); readGroupState();
            const auto from = frames.size(); uint32_t id = 0;
            loseAcknowledgement = [](const std::vector<uint8_t>& frame) { return isStart(frame, 15); };
            assert(submitGroupMoves(memberMoves(all, policy, mode), id));
            runUntil([&] { return terminalGroup(id); }, 4000000);
            assert(groupOp(id).result.aborted && !groupOp(old).delivered);
            for (const auto address : all) {
                const auto& member = memberResult(id, address);
                assert(member.stopAdmission == Probe::Action::OK && member.stopOperationId);
                assert(member.stopExecution == ActionExecution::ACKNOWLEDGED && member.stopCompletion == ActionCompletion::OBSERVED);
                assert(count([&](const std::vector<uint8_t>& f) { return isStop(f, address); }, from) == 1);
                const auto* prior = findRecord(*app, memberResult(old, address).operationId);
                assert(prior && !prior->delivered && confirmedStop(*prior));
            }
            assert(memberResult(id, 15).start == Probe::GroupStart::UNKNOWN);
        }
    // A whole-group stop consumes the remaining slots and removes every fallback
    // obligation. An escalation with no slot cannot partially consume capacity.
    fresh(all); readyGroup(all);
    const auto old = retainConsoleGroupStop(904); readGroupState();
    const auto from = frames.size(); uint32_t move = 0, stop = 0, rejected = 777;
    assert(submitGroupMoves(memberMoves(all, Policy::STOP_FAST), move));
    runUntil([] { return isStart(hardware.tx, 1); });
    assert(submitGroupStop(StopBehavior::DIRECT, stop));
    const auto nextId = app->nextOperationId;
    assert(!submitGroupStop(StopBehavior::DIRECT, rejected) && rejected == 777 && app->nextOperationId == nextId);
    runUntil([&] { return terminalGroup(move) && terminalGroup(stop); }, 4000000);
    assert(!groupOp(old).delivered && groupOp(stop).result.ok);
    for (const auto address : all) {
        const auto& member = memberResult(move, address);
        assert(member.interruptedByStop && !member.stopOperationId);
        assert(memberResult(stop, address).admission == Probe::Action::OK);
        assert(count([&](const std::vector<uint8_t>& f) { return isStop(f, address); }, from) == 1);
    }
    // An earlier sequential member can already be terminal when the explicit
    // stop takes over. Its original outcome stays unchanged and consumes no
    // second fallback slot from the fully occupied pool.
    fresh(all); readyGroup(all);
    retainConsoleGroupStop(905); readGroupState();
    assert(submitGroupMoves(memberMoves(all, Policy::STOP_FAST, Mode::SEQUENTIAL), move));
    drives[1].regs[6] = 1; // Real alarm reply ends member 1 before every start.
    auto* early = findRecord(*app, memberResult(move, 1).operationId);
    runUntil([&] { return terminal(*app, *early); }, 4000000);
    assert(early->move.state == ActionState::FAILED && classifyStart(early->move) == Probe::GroupStart::STARTED);
    assert(!app->group.move.startsDone);
    unsigned char original[sizeof(early->move)];
    std::memcpy(original, &early->move, sizeof(original));
    assert(submitGroupStop(StopBehavior::DIRECT, stop));
    runUntil([&] { return terminalGroup(move) && terminalGroup(stop); }, 4000000);
    assert(std::memcmp(original, &early->move, sizeof(original)) == 0);
    assert(!memberResult(move, 1).interruptedByStop);
    for (const auto address : all) assert(!memberResult(move, address).stopOperationId);
}
void testStopPolicyOperationIdBudget() {
    for (const bool individual : {false, true}) {
        fresh(); readyGroup();
        auto request = memberMoves({1, 2, 4}, Policy::STOP_FAST);
        const uint32_t followingIds = 2 * app->group.count + 1;
        app->nextOperationId = UINT32_MAX - followingIds + 1;
        const auto nextId = app->nextOperationId; const auto from = frames.size(); uint32_t id = 777;
        const auto refused = individual ? submitGroupMoves(request, id) : submitGroupMove(groupMove(Policy::STOP_FAST), id);
        assert(!refused && refused.detail == static_cast<int32_t>(Probe::Action::IDS_EXHAUSTED));
        assert(id == 777 && app->nextOperationId == nextId && frames.size() == from);
        --app->nextOperationId; // Exact budget: move IDs, fallback stops and one recovery.
        loseAcknowledgement = [](const std::vector<uint8_t>& frame) { return isStart(frame, 2); };
        assert(individual ? submitGroupMoves(request, id) : submitGroupMove(groupMove(Policy::STOP_FAST), id));
        assert(app->nextOperationId == 0);
        runUntil([&] { return terminalGroup(id); }, 4000000);
        assert(app->recovery.operationId == UINT32_MAX && !app->group.move.fallbackRecoveryId);
        for (const uint8_t address : {1, 2}) {
            const auto& member = memberResult(id, address);
            assert(member.stopAdmission == Probe::Action::OK && member.stopCompletion == ActionCompletion::OBSERVED);
            const auto* stop = findRecord(*app, member.stopOperationId);
            assert(stop && stop->operationId == stop->action.operationId && stop->stopRecoveryOperationId == UINT32_MAX);
            assert(count([&](const std::vector<uint8_t>& f) { return isStop(f, address); }, from) == 1);
        }
        assert(memberResult(id, 2).start == Probe::GroupStart::UNKNOWN);
        assert(memberResult(id, 4).start == Probe::GroupStart::NOT_SENT && !memberResult(id, 4).stopOperationId);
    }
    // Stops for a selected non-member may spend all remaining global IDs. They
    // cannot consume the admitted group's stop or shared recovery identities.
    const std::vector<uint8_t> members = {2, 4, 5};
    for (const auto mode : {Mode::STAGED, Mode::SEQUENTIAL})
        for (const auto policy : {Policy::STOP_NORMAL, Policy::STOP_FAST})
            for (const uint8_t lost : {4, 5}) {
                fresh({1, 2, 4, 5}); readyGroup(members);
                app->nextOperationId = UINT32_MAX - (2 * app->group.count + 1) - 2;
                loseAcknowledgement = [=](const std::vector<uint8_t>& f) { return isStart(f, lost); };
                uint32_t id = 0; assert(submitGroupMoves(memberMoves(members, policy, mode), id));
                const auto recoveryId = app->group.move.fallbackRecoveryId;
                StopPolicy stop; stop.behavior = StopBehavior::DIRECT;
                uint32_t outside[2] = {};
                assert(submitStop(1, stop, outside[0]) && submitStop(1, stop, outside[1]));
                assert(app->nextOperationId == 0);
                const auto from = frames.size();
                runUntil([&] { return terminalGroup(id); }, 4000000);
                assert(app->recovery.operationId == recoveryId && !app->group.move.fallbackRecoveryId);
                for (const auto address : members) {
                    const auto& member = memberResult(id, address);
                    if (address > lost) { assert(member.start == Probe::GroupStart::NOT_SENT && !member.stopOperationId); continue; }
                    assert(member.stopAdmission == Probe::Action::OK && member.stopCompletion == ActionCompletion::OBSERVED);
                    assert(count([&](const std::vector<uint8_t>& f) { return isStop(f, address); }, from) == 1);
                }
                for (const auto stopId : outside) assert(findRecord(*app, stopId));
                assert(memberResult(id, lost).start == Probe::GroupStart::UNKNOWN && app->nextOperationId == 0);
            }
    // A partly admitted explicit group stop transfers only its admitted member.
    // Remaining members still use their reserved fallback IDs and stop records.
    fresh({1, 2, 4, 5}); readyGroup(members);
    app->nextOperationId = UINT32_MAX - (2 * app->group.count + 1) - 2;
    loseAcknowledgement = [](const std::vector<uint8_t>& f) { return isStart(f, 5); };
    uint32_t move = 0, explicitStop = 0;
    assert(submitGroupMoves(memberMoves(members, Policy::STOP_FAST), move));
    runUntil([] { return isStart(hardware.tx, 5); });
    assert(submitGroupStop(StopBehavior::DIRECT, explicitStop));
    assert(memberResult(explicitStop, 2).admission == Probe::Action::OK);
    assert(memberResult(explicitStop, 4).admission == Probe::Action::IDS_EXHAUSTED && app->nextOperationId == 0);
    runUntil([&] { return terminalGroup(move) && terminalGroup(explicitStop); }, 4000000);
    assert(!memberResult(move, 2).stopOperationId);
    for (const uint8_t address : {4, 5}) assert(memberResult(move, address).stopCompletion == ActionCompletion::OBSERVED);
    for (const auto address : members) assert(count([&](const std::vector<uint8_t>& f) { return isStop(f, address); }) == 1);
    // All explicit children may consume the last ordinary IDs too.
    fresh({1, 2, 4, 5}); readyGroup(members);
    app->nextOperationId = UINT32_MAX - (2 * app->group.count + 1) - 4;
    loseAcknowledgement = [](const std::vector<uint8_t>& f) { return isStart(f, 5); };
    assert(submitGroupMoves(memberMoves(members, Policy::STOP_FAST), move));
    runUntil([] { return isStart(hardware.tx, 5); });
    assert(submitGroupStop(StopBehavior::DIRECT, explicitStop) && app->nextOperationId == 0);
    runUntil([&] { return terminalGroup(move) && terminalGroup(explicitStop); }, 4000000);
    assert(groupOp(explicitStop).result.ok);
    // After every start ACK, a lost explicit stop ACK still requires recovery
    // for the other unsent stops. Releasing the move cannot discard that reserve.
    // A later probe has a higher ID than the reserved recovery; recovery still
    // invalidates its cached result and excludes later delivery of older reads.
    fresh({1, 2, 4, 5}); readyGroup(members);
    app->nextOperationId = UINT32_MAX - (2 * app->group.count + 1) - 5;
    assert(submitGroupMoves(memberMoves(members, Policy::STOP_FAST), move));
    const auto recoveryId = app->group.move.fallbackRecoveryId;
    runUntil([] { return app->group.move.startsDone; });
    command("@906 probe 1\n");
    runUntil([&] { return app->modelKnown && app->modelOperationId > recoveryId; });
    assert(app->known && app->ok);
    loseAcknowledgement = [](const std::vector<uint8_t>& f) { return isStop(f, 2); };
    assert(submitGroupStop(StopBehavior::DIRECT, explicitStop) && app->nextOperationId == 0);
    runUntil([] { return app->owner.needsRecovery(); });
    assert(!terminalGroup(move) && !releaseOperation(move));
    runUntil([&] { return terminalGroup(move) && terminalGroup(explicitStop); }, 4000000);
    assert(app->recovery.operationId == recoveryId && !app->group.move.fallbackRecoveryId);
    assert(!app->known && !app->modelKnown && !app->ok && app->cacheOperationId == UINT32_MAX);
    assert(memberResult(explicitStop, 2).execution == ActionExecution::UNKNOWN);
    for (const uint8_t address : {4, 5}) assert(memberResult(explicitStop, address).completion == ActionCompletion::OBSERVED);
    for (const auto address : members) {
        assert(!memberResult(move, address).stopOperationId);
        assert(count([&](const std::vector<uint8_t>& f) { return isStop(f, address); }) == 1);
    }
}

void testFifteenIndependentRequestsAndStorage() {
    const auto all = addresses(Probe::GROUP_CAPACITY);
    fresh(all); readyGroup(all);
    auto reversed = all; std::reverse(reversed.begin(), reversed.end());
    auto request = memberMoves(reversed, Policy::STOP_FAST);
    for (auto& intent : request.members) intent.move.position.value = Rational((intent.address % 2 ? 1 : -1) * intent.address * 17);
    const auto from = frames.size(); uint32_t id = 0;
    assert(submitGroupMoves(request, id)); runUntil([&] { return terminalGroup(id); }, 4000000);
    assert(groupOp(id).result.ok && groupOp(id).result.count == 15 && startsBackToBack(from, 15));
    std::size_t lastReadback = 0, firstStart = frames.size();
    for (std::size_t i = from; i < frames.size(); ++i) {
        if (isStageFrame(frames[i]) && frames[i][1] == 3) lastReadback = i;
        if (isAnyStart(frames[i]) && firstStart == frames.size()) firstStart = i;
    }
    assert(count(isStageFrame, from) == 30 && lastReadback < firstStart);
    for (const auto address : all) {
        const auto& expected = memberMove(request, address);
        const auto& actual = memberResult(id, address);
        sameIntent(actual.request, expected);
        assert(actual.effectiveNative == expected.position.value.numerator && actual.completion == ActionCompletion::OBSERVED);
    }
    noWriteOutside(all, from);
    std::printf("Member request storage: GroupMemberMoves=%zu GroupReference=%zu GroupMemberResult=%zu GroupView=%zu GroupCommand=%zu App=%zu\n",
        sizeof(Probe::GroupMemberMoves), sizeof(Probe::GroupReference), sizeof(Probe::GroupMemberResult),
        sizeof(Probe::GroupView), sizeof(Probe::GroupCommand), sizeof(App));
}
void testMemberStageExpiresBeforeLaterMemberWithoutOldEvidence() {
    fresh(); readyGroup();
    app->options.observationMaxAgeMs = 1500;
    auto request = memberMoves({1, 2, 4});
    const auto from = frames.size(); uint32_t id = 0;
    assert(submitGroupMoves(request, id));
    auto* second = findRecord(*app, memberResult(id, 2).operationId);
    ESS::PreparedMove next;
    assert(second && ESS::nextMove(second->move, nowUs(), next));
    assert(next.deadlineUs < second->deadlineUs);
    runUntil([] { const auto& stage = groupMember(*app, 1)->stage; return stage.phase == 2 && stage.request.owner; });
    const auto readback = groupMember(*app, 1)->stage.request;
    for (unsigned i = 0; i < 20000 && !app->owner.result(readback); ++i) {
        advanceHardware(hardware.time + 20); app->owner.service(uart.sample()); serveBus();
    }
    const auto* result = app->owner.result(readback);
    assert(result && result->outcome == Rtu::Outcome::SUCCESS);
    advanceHardware(next.deadlineUs + 1);
    runUntil([&] { return terminalGroup(id); });
    assert(groupOp(id).result.aborted && groupOp(id).result.failedAddress == 2);
    assert(memberResult(id, 1).stageReadback == Probe::GroupStageReadback::MATCH);
    assert(memberResult(id, 2).stageExecution == ActionExecution::NOT_TRANSMITTED);
    assert(count(isAnyStart, from) == 0);
    assert(count([](const std::vector<uint8_t>& frame) { return frame[0] != 1; }, from) == 0);

    // A later operation that expires before its first stage cannot borrow the
    // preceding operation's successful stage or its TX/readback evidence.
    fresh(); readyGroup();
    request = memberMoves(); assert(submitGroupMoves(request, id));
    runUntil([&] { return terminalGroup(id); }, 2000000);
    assert(groupOp(id).result.ok && groupMember(*app, 1)->stage.view.staged && releaseOperation(id));
    readGroupState(); app->options.moveTimeoutMs = 1;
    request = memberMoves(); const auto secondFrom = frames.size();
    assert(submitGroupMoves(request, id));
    const auto* first = findRecord(*app, memberResult(id, 1).operationId);
    assert(first); advanceHardware(first->deadlineUs + 1);
    runUntil([&] { return terminalGroup(id); });
    const auto& expired = memberResult(id, 1);
    assert(expired.stageState == ActionState::FAILED && expired.stageExecution == ActionExecution::NOT_TRANSMITTED);
    assert(expired.stageReadback == Probe::GroupStageReadback::NOT_READ && !expired.stageTxComplete &&
        !expired.stageTxEndUs && !expired.stageReadLatestUs && frames.size() == secondFrom);
}

void testMemberReferenceInvalidationThroughOrdinaryActionsAndReads() {
    fresh(); readyGroup(); originMember(4, 100, 123);
    command("@301 motor-release 4\n"); contains("\"result\":\"accepted\"");
    runUntil([] { return !acting(*app); });
    assert(!memberView(4).axis.originKnown && !memberView(4).referenceCurrent);
    enableDrive(4, 302);
    advanceHardware(hardware.time + ENABLE_SETTLE_US); pump(); readGroupState();
    assert(!memberView(4).axis.originKnown && !memberView(4).referenceCurrent);
    for (bool release : {false, true}) {
        fresh(); readyGroup(); originMember(1, 100, 123);
        if (release) drives[1].regs[0x0007] = 16;
        else drives[1].regs[0x000A] = 1; // Changed raw feedback; no command relation inferred.
        command("@303 read config\n");
        runUntil([] { return !reading(*app); });
        command("@304 read state\n");
        runUntil([] { return !reading(*app); });
        assert(!memberView(1).axis.originKnown && !memberView(1).referenceCurrent);
    }
}

void testMemberConsolePlan(bool emit = false) {
    fresh(); readyGroup();
    const auto traffic = hardware.writes;
    const auto send = [emit](const std::string& line) {
        command(line); pump(4000);
        if (emit) std::fputs(Serial.output.c_str(), stdout);
    };
    send("@401 group show\n"); contains("\"axis\":"); contains("\"reference_current\":false");
    send("@402 group plan begin start staged on-failure leave\n"); contains("\"result\":\"done\"");
    send("@403 group plan add 4 absolute 100 steps native 60 configured\n"); contains("\"result\":\"done\"");
    send("@404 group plan add 1 relative 50 steps native 60 configured\n"); contains("\"result\":\"done\"");
    send("@405 group plan add 2 relative -25 steps native 60 configured\n"); contains("\"result\":\"done\"");
    send("@406 group plan show\n"); contains("\"active\":true"); contains("\"count\":3");
    assert(hardware.writes == traffic);
    command("@407 group plan submit\n"); contains("\"result\":\"accepted\"");
    runUntil([] { return app->group.move.result.operationId && !app->group.move.result.pending && app->group.move.delivered; }, 2000000);
    pump(4000);
    contains("\"individual\":true"); contains("\"command\":\"group-move\"");
    if (emit) std::fputs(Serial.output.c_str(), stdout);
    const auto id = app->group.move.result.operationId;
    assert(groupOp(id).result.ok && memberResult(id, 1).effectiveNative == 50 &&
        memberResult(id, 2).effectiveNative == -25 && memberResult(id, 4).effectiveNative == 100);
}

void testMemberConsoleCoordinates() {
    fresh(); readyGroup();
    const auto traffic = hardware.writes;
    command("@501 group axis 4 config set command 1000\n"); contains("\"result\":\"done\"");
    const auto reference = memberReference(4, 25);
    const auto& ref = reference.reference;
    const auto line = "@502 group reference 4 " + std::to_string(reference.declaration) + " " +
        std::to_string(ref.target.generation) + " " + std::to_string(ref.configurationGeneration) + " " +
        std::to_string(reference.configurationOperationId) + " " + std::to_string(reference.motionOperationId) +
        " 25 0 command-coordinate-qualified\n";
    assert(line.size() < Probe::LINE_CAPACITY);
    command(line); contains("\"result\":\"done\""); assert(memberView(4).referenceCurrent);
    command("@503 group axis 4 origin 100\n"); contains("\"result\":\"done\"");
    assert(memberView(4).axis.originKnown && memberView(4).axis.originNative == 100);
    command("@504 group reference 4 revoke\n"); contains("\"result\":\"done\"");
    assert(!memberView(4).referenceCurrent && !memberView(4).axis.originKnown && hardware.writes == traffic);
}

void testFullCapacityPlanLineFits() {
    const auto all = addresses(Probe::GROUP_CAPACITY);
    fresh(all); readyGroup(all);
    const auto traffic = hardware.writes;
    assert(beginGroupPlan(Mode::SEQUENTIAL, Policy::STOP_NORMAL));
    auto move = groupMove(Policy::LEAVE).move;
    move.position.value = Rational(INT64_MIN + 1, UINT64_MAX);
    move.position.unit = PositionUnit::ENCODER_COUNTS;
    move.position.maximumQuantizationError = move.position.maximumApproximationError = std::numeric_limits<double>::max();
    move.position.radians = -std::numeric_limits<double>::max(); move.setup = MoveSetup::VERIFY_AND_UPDATE;
    for (const auto address : all) assert(addGroupPlanMember(address, move));
    // Deliberately wide cached numbers exercise formatting, not motion admission.
    for (auto& member : app->group.members) {
        auto& axis = member.axis;
        axis.generation = UINT32_MAX; axis.originKnown = axis.encoderOriginKnown = axis.softLimitsKnown = true;
        axis.originNative = axis.encoderOriginNative = axis.softMinimum = INT64_MIN;
        axis.softMaximum = INT64_MAX;
        axis.originSource = axis.encoderOriginSource = ScaleSource::QUALIFIED;
        const UnitScale scale(UINT32_MAX, UINT32_MAX, ScaleSource::QUALIFIED);
        axis.units.commandStepsPerMotorTurn = axis.units.motorTurnsPerLoadTurn = axis.units.fullStepsPerMotorTurn = scale;
        axis.units.millimetresPerLoadTurn = axis.units.encoder.countsPerUnit = scale;
        axis.units.encoder.sourceId = UINT32_MAX;
        auto& reference = member.coordinate.reference;
        reference.nativeKnown = true; reference.nativePosition = INT64_MIN;
        reference.target.id = reference.target.generation = reference.configurationGeneration = UINT32_MAX;
        reference.target.address = 247;
        reference.observedUs = reference.maximumAgeUs = UINT64_MAX;
    }
    command("@4294967295 group plan show\n"); pump(5000);
    contains("\"command\":\"group-plan-show\""); contains("\"count\":15");
    const auto begin = Serial.output.find('{'); const auto end = Serial.output.find('\n', begin);
    assert(begin != std::string::npos && end != std::string::npos && Serial.output[end - 1] == '}');
    const auto length = end - begin;
    assert(length < Probe::OUTPUT_CAPACITY && hardware.writes == traffic);
    std::printf("Wide 15-member group view with full plan: %zu/%zu bytes\n", length, Probe::OUTPUT_CAPACITY);
}
} // namespace

// Wired bench 11: a member with an active limit input, or with 0x0018 not
// known 0 (members never bind soft limits), refuses the group before any write.
void testLimitAndSoftLimitMembers() {
    const std::vector<uint8_t> list = {1, 2, 4};
    fresh(list);
    for (const auto address : list) { drives[address].regs[0x0042] = 2; drives[address].regs[0x0043] = 3; }
    readyGroup(list);
    drives[2].regs[0x0008] = 2; readGroupState();
    auto from = frames.size();
    uint32_t refused = 77;
    assert(!submitGroupMove(groupMove(Policy::LEAVE, Mode::SEQUENTIAL), refused) && refused == 77);
    assert(count(isWrite, from) == 0);
    Probe::SettingsRefusal refusal;
    assert(copySettingsRefusal(refusal) && !std::strcmp(refusal.reason, "limit_input_active") && refusal.terminal == 1);
    fresh(list);
    drives[4].regs[0x0018] = 1;
    readyGroup(list);
    from = frames.size();
    assert(!submitGroupMove(groupMove(Policy::LEAVE, Mode::SEQUENTIAL), refused) && refused == 77);
    assert(count(isWrite, from) == 0);
    assert(copySettingsRefusal(refusal) && !std::strcmp(refusal.reason, "soft_limit_state_unknown"));
}
// Wired bench 19 G3/motion_b: a group refusal names the member check.
void testGroupRefusalNamesMemberCheck() {
    fresh(); readyGroup();
    revokePositionStartSpeedRelation(*app, 2);
    uint32_t id = 77; const auto writes = hardware.writes;
    assert(!submitGroupMove(groupMove(Policy::LEAVE), id) && id == 77 && hardware.writes == writes);
    assert(app->settingsRefusal.reason && !std::strcmp(app->settingsRefusal.reason, "start_speed_relation"));
    fresh(); readyGroup();
    invalidateMemberConfiguration(*app, 4, uart.sample());
    assert(!submitGroupMove(groupMove(Policy::LEAVE), id) && id == 77);
    assert(app->settingsRefusal.reason && !std::strcmp(app->settingsRefusal.reason, "member_configuration"));
    fresh(); readyGroup();
    assert(submitGroupMove(groupMove(Policy::LEAVE), id) && !app->settingsRefusal.reason); // Admitted: no reason.
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--emit-member-fixtures") {
        testMemberConsolePlan(true); return 0;
    }
    testLateSpeedWordAfterGroupStopKeepsMemberOrigin();
#if !MOTORCONTROLRS_TEST_IDF
    // The Arduino sketch widens the loop task that runs group moves (900 of 8192 bytes left).
    assert(getArduinoLoopTaskStackSize() == 16 * 1024);
#endif
    if (argc == 2 && std::string(argv[1]) == "--storage-layout") {
        std::printf("App=%zu GroupMember=%zu EnableWait=%zu EnableWaitArray=%zu ActionEvidence=%zu\n",
            sizeof(App), sizeof(App::GroupMember), sizeof(App::EnableWait), sizeof(app->enableWaits), sizeof(ESS::ActionEvidence));
        return 0;
    }
    testGroupRefusalNamesMemberCheck();
    testMemberIntentMappingAndCopiedFrames();
    testMemberRequestRejectsShapeAndInvalidLast();
    testMemberScalesAreIndependent();
    testMemberAbsoluteOriginsAndMixedStarts();
    testMemberReferenceProvenanceAndOutputOnError();
    testMemberReferenceExpiresAndContradictionsRevokeIt();
    testMemberPlanIsInertAndBoundToConfiguration();
    testMemberStartLossAfterExecutionNeverReplays();
    testStopPolicyCapacityBeforeAnyWrite();
    testStopPolicyUsesRemainingCapacity();
    testStopPolicyOperationIdBudget();
    testFifteenIndependentRequestsAndStorage();
    testMemberStageExpiresBeforeLaterMemberWithoutOldEvidence();
    testMemberReferenceInvalidationThroughOrdinaryActionsAndReads();
    testMemberConsolePlan();
    testMemberConsoleCoordinates();
    testFullCapacityPlanLineFits();
    testEnableWaitSurvivesGroupChanges();
    testAlarmClearWaitHoldsGroupStart();
    testCandidateRelationSurvivesGroupChanges();
    testUnchangedCandidateInvalidatesContradictedGroupProfile();
    testRemovedMemberSelectedWaitAndExpiry();
    testEnableWaitCapacityAndAddressBounds();
    testPossibleEnableKeepsWaitAndEvidence();
    testAddressChangeCannotBypassEnableWait();
    testActiveOriginSharedReadiness();
    testDeclarationRejectsAbsentAndUnexpectedResponders();
    testFullCapacityDeclarationMoveStopAndOutputOnError();
    testWorstCaseGroupLineFits();
    testSequentialMoveStartsEachMemberInOrder();
    testStagedMoveStagesAllBeforeStarts();
    testSharedStopInputGroupOutcomes();
    testLimitAndSoftLimitMembers();
    testStagingFailureSendsNoStart();
    testLostStartAndTimeoutUnderBothPolicies();
    testCancelledGroupStopsRetainPendingRecovery();
    testUrgentStopDuringStagingAndStarts();
    testChangedWordBetweenPhasesBlocksStarts();
    testGenerationInvalidation();
    testIdleOnlyDeclarationAndExclusiveSequence();
    testSelectedAxisStageThenStoredStart();
    testStopDuringStageReadbackKeepsEvidenceWithoutWitness();
    testGroupStageCannotRenewExpiredParentDeadline();
    testQueuedStageConfigurationChangePreventsTx();
    testStageClosureEvidenceSurvivesDelayedDelivery();
    std::puts("Drive group application tests passed");
    return 0;
}
