// SPDX-License-Identifier: MIT
// The actual standalone application and SDK adapter; fixture evidence is simulated.
#include "../examples/probe_cli/ProbeApp.cpp"
#include "fakes/esp32_uart/UsbConsoleFixture.h"
#include "../examples/probe_cli/ArduinoPlatform.cpp"
#include "../examples/probe_cli/main.cpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
using Kind = Probe::PersistenceCommandKind;
using Persistence = ESS::PersistenceKind;
void fresh() {
    if (app) { app->~App(); std::free(app); app = nullptr; }
    uart.~Esp32S3Uart(); new (&uart) Esp32S3Uart;
    resetHardware(); resetUsbConsole(); platformReady = writeResponseConfirmed = false;
    setup(); assert(app && !hardware.writes);
    hardware.txCharacterUs = 87; assert(uart.startCapture(20, timing().holdUs));
}
void step(uint32_t us = 10) { advanceHardware(hardware.time + us); loop(); }
void pump(unsigned count = 200) { for (unsigned i = 0; i < count; ++i) step(); }
std::vector<uint8_t> crc(std::vector<uint8_t> bytes) {
    const uint16_t value = ESS::calcCrc16(bytes.data(), bytes.size());
    bytes.push_back(static_cast<uint8_t>(value)); bytes.push_back(static_cast<uint8_t>(value >> 8)); return bytes;
}
std::vector<uint8_t> words(std::initializer_list<uint16_t> values, uint8_t address = 1) {
    std::vector<uint8_t> bytes = {address, 3, static_cast<uint8_t>(values.size() * 2)};
    for (auto value : values) { bytes.push_back(static_cast<uint8_t>(value >> 8)); bytes.push_back(static_cast<uint8_t>(value)); }
    return crc(bytes);
}
bool sameRequest(const Rtu::RequestId& a, const Rtu::RequestId& b) {
    return a.owner == b.owner && a.generation == b.generation && a.slot == b.slot;
}
void readReply(uint32_t id, const std::vector<uint8_t>& bytes) {
    auto* record = findRecord(*app, id); assert(record);
    const uint8_t token = record->read.step;
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(record->requestId); ++i) step();
    assert(app->owner.txAccepted(record->requestId));
    scheduleReply(std::max(hardware.writeStarted + 8 * 87 + 1000, hardware.time + 1000), bytes);
    for (unsigned i = 0; i < 25000 && record->read.state == MotorControlRS::ReadState::ACTIVE && record->read.step == token; ++i) step();
    assert(record->read.state != MotorControlRS::ReadState::ACTIVE || record->read.step != token);
}
uint32_t typed(ESS::ReadKind kind) {
    uint32_t id = 0;
    assert(host(app).startTypedRead(app, 1, 1, kind, id) == Probe::Action::OK); return id;
}
void configuration(uint16_t custom = 0) {
    const uint32_t id = typed(ESS::ReadKind::CONFIG);
    readReply(id, words({0, 400})); readReply(id, words({custom, 0, 0}));
    readReply(id, words({1, 0, 0})); readReply(id, words({0, 0, 0, 0, 0}));
    readReply(id, words({0, 4000})); pump(); assert(app->configuration.operationId == id);
}
void baseline() {
    const auto identity = typed(ESS::ReadKind::IDENTITY); readReply(identity, words({0x4EEA, 0x0029, 1, 0}));
    configuration();
    const auto state = typed(ESS::ReadKind::STATE);
    readReply(state, words({0, 1})); readReply(state, words({0, 0})); readReply(state, words({0, 0, 0})); pump();
    assert(app->identity.operationId == identity);
    assert(app->stateCache.blocks[0].valid && !app->stateCache.blocks[0].value.running);
}
Probe::PersistenceView inspect() {
    Probe::PersistenceView out; assert(host(app).persistence);
    assert(host(app).persistence(app, nullptr, out) == Probe::Action::OK); return out;
}
Probe::Action command(Kind kind, Persistence operation = Persistence::SAVE) {
    Probe::PersistenceCommand request; request.kind = kind; request.request = operation;
    Probe::PersistenceView out; return host(app).persistence(app, &request, out);
}
// The operator's stated facts through the console's API route (wired bench 17).
Probe::QualificationReply qualify(Persistence kind = Persistence::SAVE) {
    Probe::PersistenceFactsCommand c; c.action = Probe::FactsAction::INSTALL; c.kind = kind;
    c.stationary = c.effects = c.routeBack = true; c.backupToken = 77;
    Probe::PersistenceFactsView view;
    writeResponseConfirmed = true; // Supplied fixture evidence; no production bypass.
    app->persistencePrerequisites.maxAgeUs = 5000000; // Application observation age, not a stated fact.
    return host(app).persistenceFacts(app, c, view);
}
void communicationFacts(const ESS::CommunicationRequest& request) {
    Probe::CommunicationFactsCommand c; c.action = Probe::FactsAction::INSTALL; c.request = request;
    c.stationary = c.effects = c.routeBack = true; c.dipOff = request.field == ESS::CommunicationField::ADDRESS;
    Probe::CommunicationFactsView view;
    assert(host(app).communicationFacts(app, c, view).result == Probe::Action::OK);
    writeResponseConfirmed = true;
}
ESS::CommunicationRequest addressRequest(uint16_t address = 2) {
    ESS::CommunicationRequest r; r.field = ESS::CommunicationField::ADDRESS; r.address = address; return r;
}
void begin(Persistence kind = Persistence::SAVE) {
    qualify(kind); assert(command(Kind::BEGIN, kind) == Probe::Action::OK);
    assert(inspect().owned && inspect().pending && app->persistenceRequest.owner);
}
void waitTx() {
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(app->persistenceRequest); ++i) step();
    assert(app->owner.txAccepted(app->persistenceRequest));
}
void reply(const std::vector<uint8_t>& supplied = {}) {
    for (unsigned i = 0; i < 25000 && !app->persistenceReadRequest.owner && !app->persistenceRequest.owner; ++i) step();
    assert(app->persistenceReadRequest.owner || app->persistenceRequest.owner);
    auto& selected = app->persistenceReadRequest.owner ? app->persistenceReadRequest : app->persistenceRequest;
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(selected); ++i) step();
    assert(app->owner.txAccepted(selected)); const auto request = selected;
    const auto bytes = supplied.empty() ? hardware.tx : supplied;
    scheduleReply(std::max(hardware.writeStarted + 8 * 87 + 1000, hardware.time + 1000), bytes);
    for (unsigned i = 0; i < 50000 && sameRequest(request, selected); ++i) step();
    assert(!sameRequest(request, selected));
}
void verify(uint16_t custom = 0) {
    assert(command(Kind::VERIFY) == Probe::Action::OK);
    // Existing identity/configuration/state reads, one owner and one lease.
    reply(words({0x4EEA, 0x0029, 1, 0}));
    reply(words({0, 400})); reply(words({custom, 0, 0})); reply(words({1, 0, 0}));
    reply(words({0, 0, 0, 0, 0})); reply(words({0, 4000}));
    reply(words({0, 1})); reply(words({0, 0})); reply(words({0, 0, 0}));
    assert(!inspect().pending);
}
void snapshot(uint16_t custom = 0, uint8_t address = 1, uint16_t baud = 0, uint16_t active = 0) {
    assert(command(Kind::SNAPSHOT) == Probe::Action::OK);
    reply(words({0x4EEA, 0x0029, static_cast<uint16_t>(active ? active : address), 0}, address));
    reply(words({0, 400}, address)); reply(words({custom, baud, 0}, address)); reply(words({1, 0, 0}, address));
    reply(words({0, 0, 0, 0, 0}, address)); reply(words({0, 4000}, address));
    reply(words({0, 1}, address)); reply(words({0, 0}, address)); reply(words({0, 0, 0}, address));
    assert(!inspect().pending && app->persistenceBaselineKnown);
}
void recoverHost() {
    for (char c : std::string("@88 recover\n")) app->console.feed(c);
    const auto id = app->recovery.operationId; assert(id);
    for (unsigned i = 0; i < 5000 && !app->recovery.delivered; ++i) step(1000);
    assert(app->owner.recoveryResult(app->recovery.id));
    assert(app->owner.recoveryResult(app->recovery.id)->outcome == Rtu::RecoveryOutcome::RECOVERED);
    assert(host(app).release(app, id) == Probe::Action::OK);
    while (hardware.time <= app->recoveryGuardUntilUs) step(100);
    assert(app->owner.commissioningOwned());
}

void admissionGuards() {
    fresh(); const auto count = hardware.writes;
    assert(command(Kind::BEGIN) != Probe::Action::OK && hardware.writes == count && !inspect().owned);
    baseline(); assert(qualify().result == Probe::Action::OK); const auto before = hardware.writes;
    auto& motion = app->stateCache.blocks[0]; const auto saved = motion;
    motion.value.running = true; motion.value.rawMotion |= static_cast<uint16_t>(ESS::MotionStatusBit::RUNNING);
    assert(command(Kind::BEGIN) != Probe::Action::OK && hardware.writes == before && !inspect().owned);
    assert(!app->persistenceFacts.present); // A refused begin consumed the statement.
    motion = saved; motion.valid = false; qualify();
    assert(command(Kind::BEGIN) != Probe::Action::OK && hardware.writes == before && !inspect().owned);
    motion = saved; motion.observedEarliestUs = 1;
    step(5000000); qualify();
    assert(command(Kind::BEGIN) != Probe::Action::OK && hardware.writes == before && !inspect().owned);

    // Without a valid statement of the same kind nothing reaches the wire.
    fresh(); baseline(); const auto countBeforeRoute = hardware.writes;
    assert(command(Kind::BEGIN) == Probe::Action::UNAVAILABLE && !inspect().owned && !inspect().routeReady);
    assert(qualify().result == Probe::Action::OK && inspect().routeReady);
    assert(command(Kind::BEGIN, Persistence::FACTORY_RESTORE) == Probe::Action::UNAVAILABLE && !inspect().owned);
    assert(qualify().result == Probe::Action::OK);
    Probe::PersistenceFactsCommand revoke; revoke.action = Probe::FactsAction::REVOKE; Probe::PersistenceFactsView view;
    assert(host(app).persistenceFacts(app, revoke, view).result == Probe::Action::OK && !view.facts.present);
    assert(command(Kind::BEGIN) == Probe::Action::UNAVAILABLE && hardware.writes == countBeforeRoute);
    // The statement expires and follows the binding (recovery, useaddr) and host.
    assert(qualify().result == Probe::Action::OK);
    step(static_cast<uint32_t>(Probe::COMMISSIONING_FACTS_MAX_AGE_US + 1000));
    assert(host(app).persistenceFacts(app, Probe::PersistenceFactsCommand(), view).result == Probe::Action::OK &&
        view.validity == Probe::FactsValidity::EXPIRED);
    assert(command(Kind::BEGIN) == Probe::Action::UNAVAILABLE && hardware.writes == countBeforeRoute);
    baseline(); assert(qualify().result == Probe::Action::OK); const auto afterReads = hardware.writes;
    assert(host(app).selectTarget(app, 2) == Probe::Action::OK && host(app).selectTarget(app, 1) == Probe::Action::OK);
    assert(host(app).persistenceFacts(app, Probe::PersistenceFactsCommand(), view).result == Probe::Action::OK &&
        view.validity == Probe::FactsValidity::BINDING_CHANGED);
    assert(command(Kind::BEGIN) == Probe::Action::UNAVAILABLE && hardware.writes == afterReads);
}
void acknowledgementAndLiveEvidence() {
    fresh(); baseline(); begin(); const auto lease = app->commissioningLease;
    const auto before = app->persistence.before.beforeConfig;
    reply(); assert(app->persistence.outcome == ESS::PersistenceOutcome::ACKNOWLEDGED);
    assert(app->persistence.persistence == ESS::PersistenceKnowledge::UNVERIFIED && app->persistence.restartRequiredForProof);
    assert(app->persistence.before.beforeConfig.operationId == before.operationId &&
        !std::memcmp(&app->persistence.before.beforeConfig.raw, &before.raw, sizeof(before.raw)));
    assert(command(Kind::FINISH) != Probe::Action::OK && app->commissioningLease == lease);
    const auto count = hardware.writes; pump(); assert(hardware.writes == count);
    verify(); assert(app->persistence.liveReadbackKnown && app->persistence.matchingFields == 0x7FFF);
    assert(app->persistence.persistence == ESS::PersistenceKnowledge::UNVERIFIED && !app->persistence.verifiedFields);
    assert(command(Kind::FINISH) == Probe::Action::OK && !app->owner.commissioningOwned());
}
void exclusionAndUnconfirmedReply() {
    fresh(); baseline(); qualify(); writeResponseConfirmed = false;
    assert(command(Kind::BEGIN) == Probe::Action::OK);
    uint32_t unchanged = 99;
    assert(host(app).startProbe(app, 2, 1, unchanged) == Probe::Action::BUSY && unchanged == 99);
    assert(host(app).startTypedRead(app, 2, 1, ESS::ReadKind::IDENTITY, unchanged) == Probe::Action::BUSY);
    Probe::HostRequest hostRequest; hostRequest.tuple.baud = 9600; Probe::HostSnapshot snapshot;
    assert(host(app).hostSerial(app, &hostRequest, snapshot) == Probe::Action::BUSY);
    Probe::MonitorSettings settings; settings.enabled = true; settings.count = 1;
    Probe::MonitorSnapshot monitoring;
    assert(host(app).monitor(app, &settings, monitoring) == Probe::Action::BUSY);
    uint8_t bytes[8]; Rtu::BusRequest work;
    work.wire.bytes = bytes; work.wire.length = ESS::buildProbe(1, bytes, sizeof(bytes));
    work.wire.replyLength = 7; work.wire.responseTimeoutUs = 200000; work.wire.deadlineUs = hardware.time + REQUEST_US;
    work.expected.address = work.expected.target = 1; work.expected.targetGeneration = app->bindingGeneration;
    work.expected.function = 3; work.expected.count = 1; work.validator = Rtu::essValidator(); Rtu::RequestId id;
    assert(app->owner.admit(work, hardware.time, id) == Rtu::BusAdmission::CONFIGURING);
    assert(app->owner.admitUrgent(work, hardware.time, id) == Rtu::BusAdmission::CONFIGURING);
    reply(); assert(app->persistence.outcome == ESS::PersistenceOutcome::UNCONFIRMED_RESPONSE);
    assert(app->persistence.uncertain && app->persistence.execution == MotorControlRS::ActionExecution::UNKNOWN);
    verify(); assert(app->persistence.uncertain && app->persistence.writeEvidence.responseConfirmed == false);
    assert(command(Kind::FINISH) == Probe::Action::OK);
}
void lostReplyRetainsBackup() {
    fresh(); baseline(); app->axis.originKnown = true; begin(); const auto lease = app->commissioningLease;
    const auto saved = app->persistence.before.beforeConfig; waitTx();
    assert(!app->configuration.operationId && !app->axis.originKnown);
    for (unsigned i = 0; i < 1000 && inspect().pending; ++i) step(1000);
    assert(!inspect().pending && app->persistence.uncertain && app->persistence.effects);
    assert(app->persistence.execution == MotorControlRS::ActionExecution::UNKNOWN);
    assert(!std::memcmp(&saved.raw, &app->persistence.before.beforeConfig.raw, sizeof(saved.raw)));
    const auto count = hardware.writes; pump(); assert(hardware.writes == count);
    assert(command(Kind::FINISH) != Probe::Action::OK);
    recoverHost(); assert(app->commissioningLease == lease && hardware.writes == count);
    assert(command(Kind::FINISH) != Probe::Action::OK);
    verify(); assert(app->persistence.uncertain && hardware.writes == count + 9);
    assert(command(Kind::FINISH) == Probe::Action::OK);
}
void failedVerificationDoesNotReplaceBefore() {
    fresh(); baseline(); begin(); reply(); const auto saved = app->persistence.before.beforeConfig;
    const auto count = hardware.writes;
    assert(command(Kind::VERIFY) == Probe::Action::OK);
    reply(words({0x4EEA, 0x0029, 1, 0})); reply(crc({1, 0x83, 2}));
    assert(!inspect().pending && !app->persistence.liveReadbackKnown);
    assert(!std::memcmp(&saved.raw, &app->persistence.before.beforeConfig.raw, sizeof(saved.raw)));
    assert(command(Kind::FINISH) != Probe::Action::OK);
    pump(); assert(hardware.writes == count + 2 && inspect().owned);
    assert(command(Kind::VERIFY) == Probe::Action::OK);
    reply(words({0x4EEA, 0x0029, 1, 0})); reply(crc({1, 0x83, 2}));
    assert(app->persistenceVerificationAttempts == 2 && !app->persistence.liveReadbackKnown);
    assert(command(Kind::VERIFY) == Probe::Action::INVALID);
    pump(); assert(hardware.writes == count + 4 && inspect().owned);
}
// Factory restore stays refused before TX until the restricted recommissioning
// sequence (wired bench 17 item 6) exists, even with every fact stated.
void factoryRestoreRefusedBeforeTx() {
    fresh(); baseline(); const auto count = hardware.writes;
    Probe::PersistenceFactsCommand c; c.action = Probe::FactsAction::INSTALL; c.kind = Persistence::FACTORY_RESTORE;
    c.stationary = c.effects = c.routeBack = c.completeBackup = true; c.backupToken = 77;
    Probe::PersistenceFactsView view;
    const auto refused = host(app).persistenceFacts(app, c, view);
    assert(refused.result == Probe::Action::UNAVAILABLE && refused.reason &&
        !std::strcmp(refused.reason, "recommissioning_unavailable") && !view.facts.present);
    // The begin boundary names the refusal and still takes stated save facts.
    assert(qualify().result == Probe::Action::OK);
    Probe::PersistenceCommand request; request.kind = Kind::BEGIN; request.request = Persistence::FACTORY_RESTORE;
    Probe::PersistenceView out;
    assert(host(app).persistence(app, &request, out) == Probe::Action::UNAVAILABLE);
    assert(out.reason && !std::strcmp(out.reason, "recommissioning_unavailable") && !app->persistenceFacts.present);
    assert(!inspect().owned && hardware.writes == count && !app->persistence.operationId);
}
void changedCommunicationRetainsOwnership() {
    fresh(); baseline(); begin(); reply(); verify(2);
    assert(app->persistence.communicationChanged && app->persistence.liveReadbackKnown);
    const auto lease = app->commissioningLease;
    assert(command(Kind::FINISH) != Probe::Action::OK);
    assert(app->owner.commissioningOwned() && app->commissioningLease == lease);
}
void completedInvocationCannotReuseOldBaseline() {
    fresh(); snapshot(); begin(); reply(); verify();
    assert(command(Kind::FINISH) == Probe::Action::OK);
    const auto count = hardware.writes; qualify();
    assert(command(Kind::BEGIN) != Probe::Action::OK && hardware.writes == count);
    assert(!app->owner.commissioningOwned());
}
void readOnlySnapshotAndInvocationBudget() {
    fresh(); snapshot();
    assert(app->persistenceInvocations == 0 && hardware.writes == 9 && inspect().owned);
    assert(command(Kind::FINISH) == Probe::Action::OK && !inspect().owned);
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        snapshot(); begin(); reply(); verify();
        assert(command(Kind::FINISH) == Probe::Action::OK);
        assert(app->persistenceInvocations == attempt + 1);
    }
    snapshot(); qualify(); const auto count = hardware.writes;
    assert(command(Kind::BEGIN) == Probe::Action::RESULTS_FULL && hardware.writes == count);
    assert(app->persistenceInvocations == 2 && !app->persistenceFacts.present); // An exhausted budget still consumes.
}
void recoveryInvalidatesVerifiedFinish() {
    fresh(); baseline(); begin(); reply(); verify();
    const auto verified = app->persistence.verification.config.operationId;
    assert(verified && app->persistenceConfirmedGeneration);
    recoverHost();
    assert(app->persistence.verification.config.operationId == verified);
    assert(!app->persistenceConfirmedGeneration && command(Kind::FINISH) != Probe::Action::OK);
    verify(); assert(command(Kind::FINISH) == Probe::Action::OK);
}
Probe::Action communicationCommand(Probe::CommunicationCommandKind kind) {
    Probe::CommunicationCommand value; value.kind = kind; value.request.field = ESS::CommunicationField::ADDRESS;
    value.request.address = 2; Probe::CommunicationView out;
    return host(app).communication(app, &value, out);
}
void communicationReply(const std::vector<uint8_t>& supplied = {}) {
    for (unsigned i = 0; i < 25000 && !app->owner.txAccepted(app->commissioningRequest); ++i) step();
    assert(app->owner.txAccepted(app->commissioningRequest));
    const auto bytes = supplied.empty() ? hardware.tx : supplied;
    const auto character = hardware.txCharacterUs;
    scheduleReply(std::max(hardware.writeStarted + 8 * character + app->serial.timing.replyGapUs + 500,
        hardware.time + app->serial.timing.replyGapUs + 500), bytes, character);
    for (unsigned i = 0; i < 50000 && app->commissioningRequest.owner; ++i) step();
    assert(!app->commissioningRequest.owner);
}
void pendingAddressSaveKeepsLease() {
    fresh(); baseline();
    communicationFacts(addressRequest());
    assert(communicationCommand(Probe::CommunicationCommandKind::BEGIN) == Probe::Action::OK);
    const auto lease = app->commissioningLease;
    communicationReply();
    assert(communicationCommand(Probe::CommunicationCommandKind::CONFIRM_BEFORE) == Probe::Action::OK);
    communicationReply(words({2})); assert(app->commissioningConfirmedGeneration);
    qualify(); const auto writes = hardware.writes;
    assert(command(Kind::BEGIN) != Probe::Action::OK && hardware.writes == writes);
    snapshot(2); assert(app->commissioningLease == lease);
    begin(); assert(app->commissioningLease == lease);
    reply(); assert(app->persistence.before.beforeConfig.raw.customNode == 2);
    assert(!app->commissioningConfirmedGeneration);
    assert(communicationCommand(Probe::CommunicationCommandKind::FINISH) != Probe::Action::OK);
    assert(command(Kind::FINISH) != Probe::Action::OK && app->commissioningLease == lease);
    verify(2);
    assert(command(Kind::FINISH) == Probe::Action::OK && app->owner.commissioningOwned());
    assert(app->commissioningLease == lease); // Persistence cannot release the enclosing commissioning session.
    assert(communicationCommand(Probe::CommunicationCommandKind::FINISH) != Probe::Action::OK);
    assert(communicationCommand(Probe::CommunicationCommandKind::SELECT_BEFORE) == Probe::Action::OK);
    assert(app->owner.commissioningOwned());
    assert(communicationCommand(Probe::CommunicationCommandKind::CONFIRM_BEFORE) == Probe::Action::OK);
    communicationReply(words({2}));
    assert(communicationCommand(Probe::CommunicationCommandKind::FINISH) == Probe::Action::OK);
}
void historicalCommunicationCannotFinishNewLease() {
    fresh(); baseline();
    communicationFacts(addressRequest());
    assert(communicationCommand(Probe::CommunicationCommandKind::BEGIN) == Probe::Action::OK);
    assert(app->owner.cancel(app->commissioningRequest, hardware.time) == Rtu::Cancel::CANCELLED); pump();
    assert(!app->commissioning.effects);
    assert(communicationCommand(Probe::CommunicationCommandKind::FINISH) == Probe::Action::OK);
    const auto historical = app->commissioning.operationId;
    snapshot();
    assert(app->commissioning.operationId == historical && !app->persistenceParentSession);
    assert(command(Kind::SNAPSHOT) == Probe::Action::OK);
    reply(words({0x4EEA, 0x0029, 1, 0}));
    reply(words({0, 400})); reply(words({0, 0, 0})); reply(words({1, 0, 0}));
    reply(words({0, 0, 0, 0, 0})); reply(words({0, 4000}));
    reply(words({0, 1})); reply(words({0, 0})); reply(words({0, 0, 0}));
    begin(); reply(); const auto lease = app->commissioningLease;
    assert(communicationCommand(Probe::CommunicationCommandKind::FINISH) != Probe::Action::OK);
    assert(app->owner.commissioningOwned() && app->commissioningLease == lease);
    assert(app->commissioning.operationId == historical);
}
void requestedAddressSaveInvalidatesPhysicalAxis() {
    fresh(); baseline(); assert(installPositionClearQualification());
    communicationFacts(addressRequest());
    assert(communicationCommand(Probe::CommunicationCommandKind::BEGIN) == Probe::Action::OK);
    communicationReply();
    assert(communicationCommand(Probe::CommunicationCommandKind::CONFIRM_REQUESTED) == Probe::Action::OK);
    communicationReply(words({2}, 2));
    assert(app->commissioning.observedActiveTarget.address == 2 && app->axis.target.address == 1);
    snapshot(2, 2); app->axis.originKnown = app->axis.encoderOriginKnown = app->axis.softLimitsKnown = true;
    app->axis.units.commandStepsPerMotorTurn = MotorControlRS::UnitScale(400, 1, MotorControlRS::ScaleSource::QUALIFIED);
    app->commandPolarityKnown = true;
    const auto generation = app->axis.generation;
    begin(); waitTx();
    assert(app->persistence.target.address == 2 && app->axis.target.address == 1);
    assert(app->axis.generation > generation && !app->axis.originKnown && !app->axis.encoderOriginKnown && !app->axis.softLimitsKnown);
    assert(!app->axis.units.commandStepsPerMotorTurn.numerator && !app->commandPolarityKnown && !app->positionClearQualification.semanticsQualified);
    reply();
}
void failedHostSetupRepairNeedsFreshBaseline() {
    fresh(); baseline();
    ESS::CommunicationRequest baud; baud.field = ESS::CommunicationField::BAUD; baud.baud = ESS::BaudRateCode::BAUD_38400;
    communicationFacts(baud);
    auto baudCommand = [](Probe::CommunicationCommandKind kind) {
        Probe::CommunicationCommand value; value.kind = kind; value.request.field = ESS::CommunicationField::BAUD;
        value.request.baud = ESS::BaudRateCode::BAUD_38400; Probe::CommunicationView out;
        return host(app).communication(app, &value, out);
    };
    assert(baudCommand(Probe::CommunicationCommandKind::BEGIN) == Probe::Action::OK); communicationReply();
    assert(baudCommand(Probe::CommunicationCommandKind::CONFIRM_BEFORE) == Probe::Action::OK); communicationReply(words({1}));
    snapshot(0, 1, 1); const auto lease = app->commissioningLease; const auto count = hardware.writes;
    hardware.configFailCalls = {hardware.configCalls + 1, hardware.configCalls + 2};
    assert(baudCommand(Probe::CommunicationCommandKind::SELECT_REQUESTED) == Probe::Action::FAILED);
    assert(app->serial.blocked && !app->serial.activeKnown && app->owner.configurationOwned());
    assert(!app->persistenceBaselineKnown && app->commissioningLease == lease);
    assert(command(Kind::FINISH) != Probe::Action::OK);
    hardware.configFailCalls.clear();
    assert(command(Kind::SELECT_BEFORE) == Probe::Action::OK);
    assert(app->serial.activeKnown && !app->serial.blocked && app->serial.active.baud == 115200);
    assert(app->commissioningLease == lease && app->owner.commissioningOwned() && hardware.writes == count);
    qualify(); assert(command(Kind::BEGIN) != Probe::Action::OK && hardware.writes == count);
    assert(baudCommand(Probe::CommunicationCommandKind::FINISH) != Probe::Action::OK);
    // Host repair does not restore either old confirmation or snapshot eligibility.
    assert(baudCommand(Probe::CommunicationCommandKind::CONFIRM_BEFORE) == Probe::Action::OK); communicationReply(words({1}));
    snapshot(0, 1, 1); begin(); reply();
    assert(app->persistence.before.beforeConfig.raw.baud == 1 && app->commissioningLease == lease);
}

bool named(const Probe::QualificationReply& r, const char* reason) { return r.reason && !std::strcmp(r.reason, reason); }
// Wired bench 17 audit F5: a parsed begin takes the statement before any
// refusal; preview leaves it.
void factsConsumedByAnyBegin() {
    fresh(); baseline(); assert(qualify().result == Probe::Action::OK);
    assert(command(Kind::PREVIEW) == Probe::Action::OK && app->persistenceFacts.present);
    Probe::MonitorSettings settings; settings.enabled = true; settings.count = 1; settings.intervalMs = 100;
    Probe::MonitorSnapshot monitoring;
    assert(host(app).monitor(app, &settings, monitoring) == Probe::Action::OK);
    assert(command(Kind::BEGIN) == Probe::Action::BUSY && !app->persistenceFacts.present);
    settings.enabled = false; assert(host(app).monitor(app, &settings, monitoring) == Probe::Action::OK);
    pump(); baseline(); const auto reads = hardware.writes;
    assert(command(Kind::BEGIN) == Probe::Action::UNAVAILABLE && hardware.writes == reads);
    assert(!app->persistenceRequest.owner && !inspect().owned);
    begin(); reply(); verify(); assert(command(Kind::FINISH) == Probe::Action::OK);
}
// Audit F2: the facts, the snapshot and the save name one endpoint. Selected
// drive 1, responding drive 2: facts stated for 1 send nothing.
void saveFactsBoundToEndpoint() {
    fresh(); baseline(); communicationFacts(addressRequest());
    assert(communicationCommand(Probe::CommunicationCommandKind::BEGIN) == Probe::Action::OK); communicationReply();
    assert(qualify().result == Probe::Action::OK && app->persistenceFacts.target.address == 1);
    assert(communicationCommand(Probe::CommunicationCommandKind::CONFIRM_REQUESTED) == Probe::Action::OK);
    communicationReply(words({2}, 2));
    assert(app->commissioning.observedActiveTarget.address == 2 && app->axis.target.address == 1);
    snapshot(2, 2); const auto writes = hardware.writes;
    assert(command(Kind::BEGIN) == Probe::Action::UNAVAILABLE);
    assert(hardware.writes == writes && !app->persistenceRequest.owner && !app->persistence.operationId);
    begin(); assert(app->persistence.target.address == 2 && app->persistence.before.beforeConfig.target.address == 2);
    reply(); assert(app->persistence.outcome == ESS::PersistenceOutcome::ACKNOWLEDGED);
}
void changeCharacter() {
    hardware.txCharacterUs = (1000000 * bitsPerCharacter(app->serial.active.format) + app->serial.active.baud - 1) /
        app->serial.active.baud;
}
Probe::Action baudCommand(Probe::CommunicationCommandKind kind) {
    Probe::CommunicationCommand value; value.kind = kind; value.request.field = ESS::CommunicationField::BAUD;
    value.request.baud = ESS::BaudRateCode::BAUD_38400; Probe::CommunicationView out;
    return host(app).communication(app, &value, out);
}
void verifyAt(uint16_t baud, uint8_t address = 1) {
    assert(command(Kind::VERIFY) == Probe::Action::OK);
    reply(words({0x4EEA, 0x0029, address, 0}, address));
    reply(words({0, 400}, address)); reply(words({0, baud, 0}, address)); reply(words({1, 0, 0}, address));
    reply(words({0, 0, 0, 0, 0}, address)); reply(words({0, 4000}, address));
    reply(words({0, 1}, address)); reply(words({0, 0}, address)); reply(words({0, 0, 0}, address));
    assert(!inspect().pending);
}
// V2-Q1 (item 7): the exact D1/C2 order with the save under the open
// communication session, the two-confirmation budget and the return path.
void communicationToPersistenceHandoff() {
    fresh(); baseline();
    ESS::CommunicationRequest baud; baud.field = ESS::CommunicationField::BAUD; baud.baud = ESS::BaudRateCode::BAUD_38400;
    communicationFacts(baud);
    assert(baudCommand(Probe::CommunicationCommandKind::BEGIN) == Probe::Action::OK); communicationReply();
    const auto lease = app->commissioningLease;
    assert(baudCommand(Probe::CommunicationCommandKind::CONFIRM_BEFORE) == Probe::Action::OK); communicationReply(words({1}));
    assert(app->commissioningConfirmedGeneration && app->commissioning.readback == 1);
    snapshot(0, 1, 1); assert(app->persistenceParentSession && app->commissioningLease == lease);
    begin(); reply(); assert(app->persistence.before.beforeConfig.raw.baud == 1);
    verifyAt(1);
    assert(command(Kind::FINISH) == Probe::Action::OK && app->owner.commissioningOwned() && app->commissioningLease == lease);
    // The owner's restart follows here; the drive then answers at 38400.
    assert(baudCommand(Probe::CommunicationCommandKind::SELECT_REQUESTED) == Probe::Action::OK); changeCharacter();
    assert(app->serial.active.baud == 38400);
    assert(baudCommand(Probe::CommunicationCommandKind::CONFIRM_REQUESTED) == Probe::Action::OK); communicationReply(words({1}));
    assert(app->commissioning.observedActiveSerial.baud == 38400 && app->commissioning.confirmations == 2);
    const auto writes = hardware.writes;
    assert(baudCommand(Probe::CommunicationCommandKind::CONFIRM_BEFORE) == Probe::Action::INVALID); // Budget spent.
    assert(baudCommand(Probe::CommunicationCommandKind::CONFIRM_REQUESTED) == Probe::Action::INVALID && hardware.writes == writes);
    assert(baudCommand(Probe::CommunicationCommandKind::FINISH) == Probe::Action::OK && !app->owner.commissioningOwned());
    assert(app->serial.active.baud == 38400 && !app->commissioningPrerequisites.routeBackQualified);
}
// The same order with a lost save reply: UNKNOWN, retained, never resent; the
// return to the before tuple stays available inside the session.
void handoffLostSaveReplyAndReturn() {
    fresh(); baseline();
    ESS::CommunicationRequest baud; baud.field = ESS::CommunicationField::BAUD; baud.baud = ESS::BaudRateCode::BAUD_38400;
    communicationFacts(baud);
    assert(baudCommand(Probe::CommunicationCommandKind::BEGIN) == Probe::Action::OK); communicationReply();
    assert(baudCommand(Probe::CommunicationCommandKind::CONFIRM_BEFORE) == Probe::Action::OK); communicationReply(words({1}));
    snapshot(0, 1, 1); begin(); waitTx(); const auto count = hardware.writes;
    for (unsigned i = 0; i < 1000 && inspect().pending; ++i) step(1000);
    assert(app->persistence.uncertain && app->persistence.execution == MotorControlRS::ActionExecution::UNKNOWN);
    pump(); assert(hardware.writes == count);
    assert(command(Kind::FINISH) != Probe::Action::OK && app->owner.commissioningOwned());
    recoverHost(); verifyAt(1);
    assert(command(Kind::FINISH) == Probe::Action::OK && app->owner.commissioningOwned());
    assert(baudCommand(Probe::CommunicationCommandKind::SELECT_REQUESTED) == Probe::Action::OK); changeCharacter();
    assert(baudCommand(Probe::CommunicationCommandKind::SELECT_BEFORE) == Probe::Action::OK); changeCharacter();
    assert(app->serial.active.baud == 115200 && hardware.writes == count + 9);
    assert(baudCommand(Probe::CommunicationCommandKind::FINISH) == Probe::Action::RECOVERY_REQUIRED);
    assert(baudCommand(Probe::CommunicationCommandKind::CONFIRM_BEFORE) == Probe::Action::OK); communicationReply(words({1}));
    assert(baudCommand(Probe::CommunicationCommandKind::FINISH) == Probe::Action::OK && app->serial.active.baud == 115200);
}
// Audit F1 (item 8, D7): the node route back takes only the recorded S1 word
// of the same drive, the stated DIP observation and a fresh identity read.
Probe::QualificationReply node(Probe::NodeCommandKind kind, uint16_t word = 0, uint32_t source = 0, bool dip = true) {
    Probe::NodeCommand c; c.kind = kind; c.word = word; c.source = source; c.dipObserved = dip;
    Probe::NodeView view; return host(app).persistenceNode(app, c, view);
}
void nodeReply(const std::vector<uint8_t>& supplied = {}) {
    auto& s = app->node;
    for (unsigned i = 0; i < 25000 && !(s.request.owner && app->owner.txAccepted(s.request)); ++i) step();
    assert(s.request.owner && app->owner.txAccepted(s.request));
    const auto request = s.request;
    scheduleReply(std::max(hardware.writeStarted + 8 * 87 + 1000, hardware.time + 1000), supplied.empty() ? hardware.tx : supplied);
    for (unsigned i = 0; i < 50000 && sameRequest(request, s.request); ++i) step();
    assert(!sameRequest(request, s.request));
}
using Node = Probe::NodeCommandKind;
// D2: drive 2's S1 word 0 recorded; after D1 it stores 20; the switches set 2 again.
void nodeRestoreD2() {
    fresh(); assert(host(app).selectTarget(app, 2) == Probe::Action::OK);
    assert(named(node(Node::RECORD), "lease_required"));
    snapshot(0, 2);
    assert(node(Node::RECORD).result == Probe::Action::OK);
    const auto record = app->node.view.record;
    assert(record.present && record.address == 2 && record.word == 0 && record.source &&
        record.source == app->persistenceBaseline.beforeConfig.operationId);
    assert(command(Kind::FINISH) == Probe::Action::OK && app->node.view.record.present);
    snapshot(20, 2); const auto count = hardware.writes;
    assert(node(Node::RESTORE, 0, record.source).result == Probe::Action::OK);
    nodeReply(); nodeReply(words({0}, 2));
    const auto& r = app->node.view.restore;
    assert(r.ok && r.word == 0 && r.previous == 20 && r.readback == 0 && r.source == record.source && r.target.address == 2);
    assert(hardware.writes == count + 2 && hardware.tx[1] == 3 && !app->persistenceBaselineKnown);
    assert(named(node(Node::RESTORE, 0, record.source), "snapshot_required"));
    snapshot(0, 2); begin(); reply();
    assert(app->persistence.target.address == 2 && app->persistence.before.beforeConfig.raw.customNode == 0);
    verifyAt(0, 2); assert(command(Kind::FINISH) == Probe::Action::OK);
}
// Every other request is refused before bus admission.
void nodeRestoreRefusals() {
    fresh(); assert(host(app).selectTarget(app, 2) == Probe::Action::OK);
    snapshot(20, 2);
    assert(named(node(Node::RESTORE, 0, 5), "node_record_required"));
    assert(command(Kind::FINISH) == Probe::Action::OK);
    snapshot(0, 2); assert(node(Node::RECORD).result == Probe::Action::OK);
    const auto source = app->node.view.record.source;
    assert(command(Kind::FINISH) == Probe::Action::OK);
    assert(host(app).selectTarget(app, 1) == Probe::Action::OK);
    snapshot(20, 1); auto writes = hardware.writes;
    assert(named(node(Node::RESTORE, 0, source), "wrong_target"));
    assert(command(Kind::FINISH) == Probe::Action::OK && hardware.writes == writes);
    assert(host(app).selectTarget(app, 2) == Probe::Action::OK);
    snapshot(20, 2); writes = hardware.writes;
    assert(named(node(Node::RESTORE, 5, source), "record_mismatch"));
    assert(named(node(Node::RESTORE, 0, source + 1), "source_mismatch"));
    assert(named(node(Node::RESTORE, 0, source, false), "dip_fact_required"));
    step(static_cast<uint32_t>(Probe::COMMISSIONING_FACTS_MAX_AGE_US + 1000));
    assert(named(node(Node::RESTORE, 0, source), "dip_evidence_stale"));
    assert(command(Kind::FINISH) == Probe::Action::OK);
    snapshot(20, 2, 0, 20); // 0x0002 shows 20: the switches do not set address 2.
    assert(named(node(Node::RESTORE, 0, source), "dip_not_governing"));
    assert(command(Kind::FINISH) == Probe::Action::OK);
    snapshot(2, 2); // 0x0013 equals the address: 0x0002 cannot show the switches.
    assert(named(node(Node::RESTORE, 0, source), "dip_not_governing"));
    assert(hardware.writes == writes + 18 && !app->node.request.owner && !app->node.view.restore.attempted);
    assert(command(Kind::FINISH) == Probe::Action::OK);
}
}
int main() {
    admissionGuards(); acknowledgementAndLiveEvidence(); exclusionAndUnconfirmedReply();
    lostReplyRetainsBackup(); failedVerificationDoesNotReplaceBefore();
    factoryRestoreRefusedBeforeTx();
    changedCommunicationRetainsOwnership(); completedInvocationCannotReuseOldBaseline();
    recoveryInvalidatesVerifiedFinish(); pendingAddressSaveKeepsLease(); historicalCommunicationCannotFinishNewLease();
    requestedAddressSaveInvalidatesPhysicalAxis();
    failedHostSetupRepairNeedsFreshBaseline();
    readOnlySnapshotAndInvocationBudget();
    factsConsumedByAnyBegin(); saveFactsBoundToEndpoint(); communicationToPersistenceHandoff();
    handoffLostSaveReplyAndReturn(); nodeRestoreD2(); nodeRestoreRefusals();
}
