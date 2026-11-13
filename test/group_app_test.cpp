// SPDX-License-Identifier: MIT
// Drive group (multi-drive 02) through the actual cooperative application, its
// console and its public API, on a fake bus with several simulated ESS drives.
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

// One simulated ESS drive: plain registers plus a short RUNNING period after a start.
struct Drive {
    bool present = true;
    uint16_t regs[0x140] = {};
    unsigned runningReads = 0;
};
std::map<uint8_t, Drive> drives;
std::vector<std::vector<uint8_t>> frames; // Every transmitted request, in order.
std::function<bool(const std::vector<uint8_t>&)> dropReply; // True: this request gets no reply.
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
    if (reg == 0x0007 && drive.runningReads) { --drive.runningReads; return 4; } // RUNNING.
    return reg < 0x140 ? drive.regs[reg] : 0;
}
void writeWord(Drive& drive, uint16_t reg, uint16_t value) {
    if (reg >= 0x140) return;
    drive.regs[reg] = value;
    if (reg == 0x0027 && (value & 1)) drive.runningReads = 1; // One RUNNING report, then ARRIVED.
    if (reg == 0x0027 && (value & 0x0300)) drive.runningReads = 0; // Stop.
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
    scheduleReply(std::max(hardware.writeStarted + frame.size() * 87 + 1000, hardware.time + 1000), reply);
}
void step(uint32_t us = 10) { advanceHardware(hardware.time + us); serviceApplication(); serveBus(); }
void pump(unsigned count = 64) { for (unsigned i = 0; i < count; ++i) step(); }
template <typename Done> void runUntil(Done done, unsigned limit = 400000) {
    for (unsigned i = 0; i < limit && !done(); ++i) step(20);
    assert(done());
}
void fresh(std::initializer_list<uint8_t> present = {1, 2, 4}) {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = false;
    drives.clear(); frames.clear(); dropReply = nullptr; servedWrites = 0;
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
uint32_t declare(std::initializer_list<uint8_t> addresses) {
    std::vector<uint8_t> list(addresses);
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
Probe::GroupMoveRequest groupMove(Probe::GroupFailurePolicy policy, int64_t steps = 200) {
    Probe::GroupMoveRequest request;
    request.move.position.value = Rational(steps); request.move.position.unit = PositionUnit::STEPS;
    request.move.position.frame = CoordinateFrame::NATIVE; request.move.position.relative = true;
    request.move.speedRpm = 60; request.move.ramp = MoveRamp::VERIFIED_CONFIGURED;
    request.onFailure = policy;
    return request;
}
// A member-ready group of the three bench addresses (simulated).
void readyGroup(std::initializer_list<uint8_t> addresses = {1, 2, 4}) {
    const auto set = declare(addresses);
    assert(groupOp(set).result.ok && app->group.declared);
    readGroupState();
}
bool isWrite(const std::vector<uint8_t>& frame) { return frame.size() >= 8 && (frame[1] == 6 || frame[1] == 16); }
bool isStart(const std::vector<uint8_t>& frame, uint8_t address) {
    return frame.size() == 8 && frame[0] == address && frame[1] == 6 && word(frame, 2) == 0x0027 && (word(frame, 4) & 1);
}
bool isStop(const std::vector<uint8_t>& frame, uint8_t address) {
    return frame.size() == 8 && frame[0] == address && frame[1] == 6 && word(frame, 2) == 0x0027 && (word(frame, 4) & 0x0300);
}
unsigned count(const std::function<bool(const std::vector<uint8_t>&)>& match, std::size_t from = 0) {
    unsigned n = 0;
    for (std::size_t i = from; i < frames.size(); ++i) if (match(frames[i])) ++n;
    return n;
}
void noWriteOutside(std::initializer_list<uint8_t> members, std::size_t from = 0) {
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
    // Delivered set results are replaced by the next declaration; the console
    // carries the terminal line.
    command("@5 group set 1 2 4\n"); contains("\"command\":\"group-set\""); contains("\"result\":\"accepted\"");
    runUntil([] { return !app->group.set.result.pending && app->group.set.delivered; });
    pump(2000); // Drain the bounded USB writes.
    contains("\"type\":\"group\""); contains("\"join\":\"joined\"");
    const auto writes = hardware.writes;
    command("@6 group show\n"); contains("\"command\":\"group-show\""); contains("\"current\":true");
    command("@7 group status\n"); contains("\"command\":\"group-status\""); contains("\"bus_traffic\":false");
    assert(hardware.writes == writes);
}
void testCapacityLimitsAndOutputOnError() {
    fresh({1, 2, 4, 5, 6});
    const uint8_t five[] = {1, 2, 4, 5, 6};
    uint32_t unchanged = 77;
    assert(!declareGroup(five, 5, unchanged) && unchanged == 77);
    const uint8_t repeated[] = {1, 1};
    assert(!declareGroup(repeated, 2, unchanged) && unchanged == 77);
    const uint8_t zero[] = {0};
    assert(!declareGroup(zero, 1, unchanged) && unchanged == 77 && hardware.writes == 0);
    command("@1 group set 1 2 4 5 6\n"); contains("invalid_arguments");
    command("@2 group set 1 1\n"); contains("duplicate_address");
    command("@3 group set 248\n"); contains("invalid_address");
    command("@4 group move relative 10 steps native 60 configured\n"); contains("failure_policy_required");
    command("@5 group move absolute 10 steps native 60 configured on-failure leave\n"); contains("unsupported");
    // No group yet: move, read and stop are unavailable, without traffic.
    assert(!submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), unchanged) && unchanged == 77);
    assert(!submitGroupStop(StopBehavior::DIRECT, unchanged) && !submitGroupStateRead(unchanged) && unchanged == 77);
    assert(hardware.writes == 0);
    // Full capacity: four members, each with its own facts.
    readyGroup({1, 2, 4, 5});
    assert(app->group.count == Probe::GROUP_CAPACITY);
    // A request without a policy, or one for stored setup, is refused before preparation.
    assert(!submitGroupMove(groupMove(Probe::GroupFailurePolicy::NONE), unchanged) && unchanged == 77);
    auto stored = groupMove(Probe::GroupFailurePolicy::LEAVE); stored.move.setup = MoveSetup::USE_STORED;
    assert(!submitGroupMove(stored, unchanged) && unchanged == 77);
    // Members need their own ordinary results: none are taken on refusal.
    for (std::size_t i = 0; i + 1 < REQUEST_CAPACITY; ++i) { app->records[i].operationId = 9000 + i; app->records[i].kind = App::OperationKind::TRANSACTION; app->records[i].delivered = true; }
    const auto before = hardware.writes;
    assert(!submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), unchanged) && unchanged == 77);
    assert(hardware.writes == before && !app->group.move.result.operationId);
    for (std::size_t i = 0; i + 1 < REQUEST_CAPACITY; ++i) clearRecord(app->records[i]);
    // A member that is not ready refuses the whole group before any frame.
    drives[5].regs[0x0007] = 4; readGroupState(); // Member 5 reports RUNNING.
    assert(!submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), unchanged) && unchanged == 77);
    assert(hardware.writes == before + 3 * 4 && count(isWrite) == 0); // Only the state reads.
    std::printf("Group storage bytes: App=%zu Record=%zu GroupMember=%zu GroupOperation=%zu GroupResult=%zu GroupView=%zu urgent_records=%zu\n",
        sizeof(App), sizeof(App::Record), sizeof(App::GroupMember), sizeof(App::GroupOperation),
        sizeof(Probe::GroupResult), sizeof(Probe::GroupView), URGENT_RECORDS);
}
void testGroupMoveStartsEachMemberInOrder() {
    fresh(); readyGroup();
    const auto from = frames.size();
    uint32_t id = 0;
    assert(submitGroupMove(groupMove(Probe::GroupFailurePolicy::STOP_FAST), id));
    // Each member was prepared before any frame; members wait for their turn.
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
        previous = m.startTxEndUs;
        assert(count([&](const std::vector<uint8_t>& f) { return isStart(f, address); }, from) == 1);
    }
    // Member order: each start precedes the next member's first write.
    std::size_t start1 = 0, write2 = 0, start2 = 0, write4 = 0;
    for (std::size_t i = from; i < frames.size(); ++i) {
        if (!start1 && isStart(frames[i], 1)) start1 = i;
        if (!write2 && isWrite(frames[i]) && frames[i][0] == 2) write2 = i;
        if (!start2 && isStart(frames[i], 2)) start2 = i;
        if (!write4 && isWrite(frames[i]) && frames[i][0] == 4) write4 = i;
    }
    assert(start1 && start1 < write2 && write2 < start2 && start2 < write4);
    noWriteOutside({1, 2, 4}, from);
    assert(count([](const std::vector<uint8_t>& f) { return isStop(f, 1) || isStop(f, 2) || isStop(f, 4); }, from) == 0);
    // Release frees every member result.
    assert(app->group.move.delivered && releaseOperation(id) && !app->group.move.result.operationId);
    for (std::size_t i = 0; i < REQUEST_CAPACITY; ++i) assert(!app->records[i].operationId);
    // Member state was invalidated by the moves; a new move needs a fresh read.
    uint32_t again = 0;
    assert(!submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), again));
    readGroupState();
    command("@9 group move relative 50 steps native 60 configured on-failure leave\n");
    contains("\"command\":\"group-move\""); contains("\"result\":\"accepted\"");
    runUntil([] { return app->group.move.result.operationId && !app->group.move.result.pending && app->group.move.delivered; }, 2000000);
    pump(2000);
    contains("\"type\":\"group\""); contains("\"start\":\"started\""); contains("\"start_tx_spread_us\":");
}
// The middle member's start reply is lost (UNKNOWN) or its staging times out.
void lostOrTimedOutMiddle(Probe::GroupFailurePolicy policy, bool lostStart) {
    fresh(); readyGroup();
    const auto from = frames.size();
    dropReply = [lostStart](const std::vector<uint8_t>& f) {
        return f[0] == 2 && (lostStart ? isStart(f, 2) : isWrite(f));
    };
    uint32_t id = 0;
    assert(submitGroupMove(groupMove(policy), id));
    runUntil([&] { return terminalGroup(id); }, 4000000);
    const auto& r = groupOp(id).result;
    assert(!r.ok && r.aborted && r.failedAddress == 2);
    const auto& first = memberResult(id, 1); const auto& middle = memberResult(id, 2); const auto& last = memberResult(id, 4);
    assert(first.start == Probe::GroupStart::STARTED);
    assert(middle.start == (lostStart ? Probe::GroupStart::UNKNOWN : Probe::GroupStart::NOT_SENT) && middle.uncertain);
    assert(last.start == Probe::GroupStart::NOT_SENT && last.outcome == ActionOutcome::CANCELLED && !last.uncertain);
    // Never resent; nothing reaches the member after the failure.
    assert(count([](const std::vector<uint8_t>& f) { return isStart(f, 2); }, from) == (lostStart ? 1u : 0u));
    assert(count([](const std::vector<uint8_t>& f) { return f[0] == 4; }, from) == 0);
    const bool stop = policy != Probe::GroupFailurePolicy::LEAVE;
    assert(count([](const std::vector<uint8_t>& f) { return isStop(f, 1); }, from) == (stop ? 1u : 0u));
    assert(count([](const std::vector<uint8_t>& f) { return isStop(f, 2); }, from) == (stop && lostStart ? 1u : 0u));
    assert((first.stopOperationId != 0) == stop && (middle.stopOperationId != 0) == (stop && lostStart));
    if (stop) assert(first.stopExecution == ActionExecution::ACKNOWLEDGED);
    noWriteOutside({1, 2, 4}, from);
    // An uncertain start or staging write interlocks its address until a checked stop.
    assert(axisReserved(*app, 2) == !(lostStart && stop));
}
void testLostStartAndTimeoutUnderBothPolicies() {
    for (const auto policy : {Probe::GroupFailurePolicy::STOP_FAST, Probe::GroupFailurePolicy::LEAVE})
        for (const bool lostStart : {true, false}) lostOrTimedOutMiddle(policy, lostStart);
}
void testUrgentStopDuringStagingAndStartSequence() {
    // A group stop while the middle member stages: nothing else starts.
    fresh(); readyGroup();
    auto from = frames.size();
    uint32_t id = 0;
    assert(submitGroupMove(groupMove(Probe::GroupFailurePolicy::STOP_NORMAL), id));
    runUntil([&] { return hardware.writes && isWrite(hardware.tx) && hardware.tx[0] == 2 && hardware.tx[1] == 16; });
    command("@3 group stop fast\n"); contains("\"command\":\"group-stop\""); contains("\"result\":\"accepted\"");
    const auto stopId = app->group.stops[0].result.operationId;
    assert(stopId);
    runUntil([&] { return !groupSequencing(*app); }); // Ends once the staging frame settles.
    runUntil([&] { return terminalGroup(id) && terminalGroup(stopId); }, 4000000);
    assert(groupOp(id).result.aborted && memberResult(id, 2).start == Probe::GroupStart::NOT_SENT);
    assert(memberResult(id, 4).start == Probe::GroupStart::NOT_SENT && memberResult(id, 2).interruptedByStop);
    assert(count([](const std::vector<uint8_t>& f) { return isStart(f, 2) || isStart(f, 4); }, from) == 0);
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
    assert(submitGroupMove(groupMove(Probe::GroupFailurePolicy::STOP_FAST), id));
    runUntil([] { return isStart(hardware.tx, 1); });
    command("@4 stop fast 4\n"); contains("\"result\":\"accepted\"");
    runUntil([&] { return terminalGroup(id); }, 4000000);
    pump(2000);
    assert(groupOp(id).result.aborted && groupOp(id).result.failedAddress == 4);
    assert(memberResult(id, 1).start == Probe::GroupStart::STARTED && memberResult(id, 2).start == Probe::GroupStart::NOT_SENT);
    assert(memberResult(id, 4).interruptedByStop && memberResult(id, 4).start == Probe::GroupStart::NOT_SENT);
    assert(count([](const std::vector<uint8_t>& f) { return f[0] == 2 && isWrite(f); }, from) == 0);
    assert(count([](const std::vector<uint8_t>& f) { return isStop(f, 1); }, from) == 1); // Policy stop.
    assert(count([](const std::vector<uint8_t>& f) { return isStop(f, 4); }, from) == 1); // The explicit stop only.
    noWriteOutside({1, 2, 4}, from);
}
void testFullGroupStopAtMaximumCapacity() {
    fresh({1, 2, 4, 5}); readyGroup({1, 2, 4, 5});
    // Drives report RUNNING, so normal stops stay pending until the escalation.
    const auto from = frames.size();
    for (auto& drive : drives) drive.second.regs[0x0007] = 4;
    uint32_t normal = 0, fast = 0, third = 77;
    assert(submitGroupStop(StopBehavior::CONFIGURED_DECELERATION, normal));
    runUntil([&] { return count([](const std::vector<uint8_t>& f) { return f[1] == 6 && word(f, 2) == 0x0027; }, from) == 4; });
    assert(submitGroupStop(StopBehavior::DIRECT, fast) && fast != normal);
    // Each member has its own stop and escalation record; a third group stop waits.
    for (const auto id : {normal, fast}) for (uint8_t i = 0; i < Probe::GROUP_CAPACITY; ++i) {
        const auto& m = groupOp(id).result.members[i];
        const auto* record = findRecord(*app, m.operationId);
        assert(m.admission == Probe::Action::OK && record && record->groupChild);
        assert(record >= &app->records[GROUP_STOP_FIRST] && record < &app->records[GROUP_STOP_FIRST + GROUP_STOP_RECORDS]);
    }
    assert(!submitGroupStop(StopBehavior::DIRECT, third) && third == 77);
    // The explicit stop pool of the selected address is untouched.
    for (std::size_t i = REQUEST_CAPACITY + 1; i < GROUP_STOP_FIRST; ++i) assert(!app->records[i].operationId);
    for (auto& drive : drives) drive.second.regs[0x0007] = 1;
    runUntil([&] { return terminalGroup(normal) && terminalGroup(fast); }, 4000000);
    for (const auto address : {1, 2, 4, 5}) {
        assert(count([address](const std::vector<uint8_t>& f) { return isStop(f, address); }, from) == 2);
        const auto& escalated = memberResult(normal, address);
        assert(escalated.execution == ActionExecution::ACKNOWLEDGED && escalated.completion == ActionCompletion::NOT_OBSERVED);
        assert(memberResult(fast, address).completion == ActionCompletion::OBSERVED);
    }
    assert(groupOp(fast).result.ok && !groupOp(normal).result.ok);
    noWriteOutside({1, 2, 4, 5}, from);
    // Delivered results are replaced by the next stop; the pool is reusable.
    assert(submitGroupStop(StopBehavior::DIRECT, third) && third != 77);
}
void testGenerationInvalidation() {
    uint32_t id = 77;
    // useaddr rebinding expires every member: declare again.
    fresh(); readyGroup();
    command("@1 useaddr 2\n"); contains("\"result\":\"done\"");
    assert(!memberCurrent(*app, app->group.members[0]));
    assert(!submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), id) && id == 77);
    assert(!submitGroupStateRead(id) && id == 77);
    readyGroup();
    assert(memberCurrent(*app, app->group.members[1]));
    // Enable: a member keeps its own post-enable wait and its state expires.
    command("@2 enable 4\n"); contains("\"result\":\"accepted\"");
    runUntil([] { return app->group.members[2].enableSettleUntilUs != 0; });
    command("@3 enable 2\n"); contains("\"result\":\"accepted\"");
    runUntil([] { return app->group.members[1].enableSettleUntilUs != 0; });
    // A later enable of another member keeps the first member's wait.
    assert(enableSettling(*app, 4, nowUs()) && enableSettling(*app, 2, nowUs()));
    pump(4000);
    assert(!submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), id) && id == 77);
    advanceHardware(hardware.time + ENABLE_SETTLE_US); pump();
    assert(!enableSettling(*app, 4, nowUs()) && !enableSettling(*app, 2, nowUs()));
    assert(!submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), id) && id == 77); // Stale state.
    readGroupState();
    assert(submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), id) && id != 77);
    runUntil([&] { return terminalGroup(id); }, 2000000);
    assert(releaseOperation(id));
    // Recovery changes the binding: members expire.
    readGroupState();
    recoverBus(4);
    id = 77;
    assert(!memberCurrent(*app, app->group.members[0]) && !submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), id) && id == 77);
    // A broadcast may change every drive: members expire too.
    readyGroup();
    invalidateAfterBroadcast(*app, nowUs());
    assert(!memberCurrent(*app, app->group.members[2]) && !submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), id) && id == 77);
    // A settings write to a member drive expires that member's configuration.
    readyGroup();
    invalidateDriverAssumptions(*app, 2, static_cast<uint32_t>(ESS::DriverField::SUBDIVISION), nowUs());
    assert(memberCurrent(*app, app->group.members[0]) && !memberCurrent(*app, app->group.members[1]));
    // A possible start-speed write keeps that member unready, also after re-declaration.
    invalidateDriverAssumptions(*app, 4, 1, nowUs(), nullptr, ESS::DriverGroup::POSITION_START_SPEED);
    readyGroup();
    assert(groupMember(*app, 4)->startSpeedRelationRequired && !groupMember(*app, 2)->startSpeedRelationRequired);
    id = 77;
    assert(!submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), id) && id == 77);
}
void testIdleOnlyDeclarationAndExclusiveSequence() {
    fresh(); readyGroup();
    uint32_t id = 0, unchanged = 77;
    assert(submitGroupMove(groupMove(Probe::GroupFailurePolicy::LEAVE), id));
    // While starts are being sent, only urgent stops interleave.
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
} // namespace

int main() {
    testDeclarationRejectsAbsentAndUnexpectedResponders();
    testCapacityLimitsAndOutputOnError();
    testGroupMoveStartsEachMemberInOrder();
    testLostStartAndTimeoutUnderBothPolicies();
    testUrgentStopDuringStagingAndStartSequence();
    testFullGroupStopAtMaximumCapacity();
    testGenerationInvalidation();
    testIdleOnlyDeclarationAndExclusiveSequence();
    std::puts("Drive group application tests passed");
    return 0;
}
