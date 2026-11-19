// SPDX-License-Identifier: MIT
// Included in the standalone application's private namespace after hostSerialImpl.
// One retained commissioning session; all wire work uses the existing bus owner.
// A begin needs the operator's stated facts (`communication qualification install`).
MotorControlRS::ActiveSerialTuple communicationTuple(const HostTuple& host) {
    MotorControlRS::ActiveSerialTuple value;
    value.known = true; value.baud = host.baud; value.dataBits = 8;
    value.stopBits = host.format == HostFormat::N8_2 ? 2 : 1;
    value.parity = host.format == HostFormat::E8_1 ? MotorControlRS::SerialParity::EVEN :
        host.format == HostFormat::O8_1 ? MotorControlRS::SerialParity::ODD : MotorControlRS::SerialParity::NONE;
    return value;
}
HostTuple communicationHost(const MotorControlRS::ActiveSerialTuple& serial) {
    HostTuple value; value.baud = serial.baud;
    value.format = serial.parity == MotorControlRS::SerialParity::EVEN ? HostFormat::E8_1 :
        serial.parity == MotorControlRS::SerialParity::ODD ? HostFormat::O8_1 :
        serial.stopBits == 2 ? HostFormat::N8_2 : HostFormat::N8_1;
    return value;
}
// Stated facts (wired bench 17) are bound to one target (the selected axis, or
// for persistence the confirmed endpoint of an open communication session), the
// binding, configuration and host serial generations, and expire after
// COMMISSIONING_FACTS_MAX_AGE_US. Recovery and `useaddr` advance the binding.
template <typename Facts>
Probe::FactsValidity factsValidity(const App& a, const Facts& f, uint64_t now, const MotorControlRS::ReadTarget& expected) {
    using V = Probe::FactsValidity;
    if (!f.present) return V::NONE;
    if (!Probe::sameTarget(f.target, expected) || f.bindingGeneration != a.bindingGeneration) return V::BINDING_CHANGED;
    if (f.configurationGeneration != a.axis.generation) return V::CONFIGURATION_CHANGED;
    if (f.serialGeneration != a.serial.generation) return V::HOST_CHANGED;
    if (now < f.installedUs || now - f.installedUs > Probe::COMMISSIONING_FACTS_MAX_AGE_US) return V::EXPIRED;
    return V::VALID;
}
template <typename Facts>
void bindFacts(const App& a, Facts& f, uint64_t now, const MotorControlRS::ReadTarget& target) {
    f.present = true; f.target = target; f.bindingGeneration = a.bindingGeneration;
    f.configurationGeneration = a.axis.generation; f.serialGeneration = a.serial.generation; f.installedUs = now;
}
bool sameCommunicationRequest(const ESS::CommunicationRequest& x, const ESS::CommunicationRequest& y) {
    if (x.field != y.field) return false;
    return x.field == ESS::CommunicationField::ADDRESS ? x.address == y.address :
        x.field == ESS::CommunicationField::BAUD ? x.baud == y.baud : x.format == y.format;
}
void communicationView(const App& a, Probe::CommunicationView& out) {
    out.context = a.commissioning.operationId ? &a.commissioning : nullptr;
    out.pending = a.commissioningRequest.owner != nullptr;
    out.owned = a.owner.commissioningOwned(); out.host = a.serial;
    // The open session's route, or a valid stated route for the next begin.
    out.routeReady = (out.owned && a.persistenceParentSession && a.commissioningPrerequisites.routeBackQualified) ||
        (factsValidity(a, a.communicationFacts, uart.sample(), a.axis.target) == Probe::FactsValidity::VALID &&
         a.communicationFacts.routeBack);
}
// `communication qualification install|inspect|revoke` (wired bench 17 item 1).
// One work function behind the public API (installCommunicationFacts and its
// pair) and the console callback.
Probe::QualificationReply communicationFactsWork(App& a, const Probe::CommunicationFactsCommand& c,
                                                 Probe::CommunicationFactsView& out) {
    const uint64_t now = uart.sample();
    Probe::QualificationReply reply(Probe::Action::OK);
    if (c.action == Probe::FactsAction::REVOKE) a.communicationFacts = Probe::CommunicationFacts();
    else if (c.action == Probe::FactsAction::INSTALL) {
        const bool address = c.request.field == ESS::CommunicationField::ADDRESS;
        if (!c.stationary || !c.effects || !c.routeBack || address != c.dipOff ||
            (address && (c.request.address < 1 || c.request.address > 247)) ||
            (!address && (c.request.field > ESS::CommunicationField::FORMAT ||
                (c.request.field == ESS::CommunicationField::BAUD ? static_cast<uint8_t>(c.request.baud) :
                 static_cast<uint8_t>(c.request.format)) > 3))) reply = Probe::QualificationReply(Probe::Action::INVALID);
        else if (a.owner.commissioningOwned() || a.discovery.owned) reply = Probe::QualificationReply(Probe::Action::BUSY);
        else if (!a.axis.generation) {
            reply = Probe::QualificationReply(Probe::Action::UNAVAILABLE); reply.reason = "axis_configuration_unknown";
        } else {
            Probe::CommunicationFacts f;
            f.request = c.request; f.stationary = c.stationary; f.effects = c.effects;
            f.routeBack = c.routeBack; f.dipOff = c.dipOff;
            bindFacts(a, f, now, a.axis.target); a.communicationFacts = f;
        }
    }
    out.facts = a.communicationFacts; out.validity = factsValidity(a, a.communicationFacts, now, a.axis.target);
    out.nowUs = now;
    out.reason = reply.result == Probe::Action::OK ? nullptr : reply.reason;
    return reply;
}
Probe::QualificationReply factsReply(const MotorControlRS::Status& status, const char* reason) {
    Probe::QualificationReply reply(status ? Probe::Action::OK : status.detail ?
        static_cast<Probe::Action>(status.detail) : Probe::Action::FAILED);
    if (!status) reply.reason = reason;
    return reply;
}
Probe::QualificationReply communicationFactsCommand(void*, const Probe::CommunicationFactsCommand& c,
                                                    Probe::CommunicationFactsView& out) {
    if (c.action == Probe::FactsAction::INSTALL) {
        const auto status = installCommunicationFacts(c, out); // Sets out.reason before it is read.
        return factsReply(status, out.reason);
    }
    if (c.action == Probe::FactsAction::REVOKE) revokeCommunicationFacts();
    inspectCommunicationFacts(out);
    return Probe::QualificationReply(Probe::Action::OK);
}
Probe::Action submitCommunication(App& a, uint64_t now) {
    ESS::PreparedCommunication work;
    if (!ESS::nextCommunication(a.commissioning, now, work) || work.kind != ESS::ActionWork::TRANSACTION)
        return Probe::Action::INVALID;
    Rtu::BusRequest request;
    request.wire.bytes = work.bytes; request.wire.length = work.length;
    request.wire.replyLength = work.write ? ESS::WRITE_RESPONSE_LEN : ESS::expectedReadRegistersLen(work.count);
    request.wire.deadlineUs = work.deadlineUs;
    request.wire.responseTimeoutUs = a.serial.timing.responseTimeoutUs;
    request.wire.replyGapUs = a.serial.timing.replyGapUs;
    request.expected.address = work.target.address; request.expected.target = work.target.id;
    request.expected.targetGeneration = work.target.generation;
    request.expected.function = work.write ? 6 : 3;
    request.expected.first = work.reg; request.expected.count = work.write ? 1 : work.count; request.expected.value = work.value;
    request.validator = Rtu::essValidator();
    const auto admitted = a.owner.admitCommissioning(request, a.commissioningLease, now, a.commissioningRequest);
    if (admitted == Rtu::BusAdmission::ACCEPTED) {
        a.commissioningRequestBinding = a.bindingGeneration;
        return Probe::Action::OK;
    }
    // Rejected admission still closes this explicit attempt, without sending or retrying it.
    MotorControlRS::ActionEvent failed;
    failed.transport.target = work.target; failed.transport.operationId = work.operationId;
    failed.transport.step = work.step; failed.transport.kind = ReadEventKind::TRANSPORT_FAILURE;
    failed.transport.transportDetail = static_cast<int32_t>(admitted);
    ESS::advanceCommunication(a.commissioning, failed, now);
    return admissionResult(admitted);
}
void serviceCommissioning(App& a, uint64_t now) {
    if (!a.commissioningRequest.owner) return;
    if (!a.commissioningInvalidated && a.commissioning.step == 0 && a.owner.txAccepted(a.commissioningRequest)) {
        a.commissioningInvalidated = true;
        ++a.serial.generation; // A device-interface write also invalidates old transport-context observations.
        invalidateSerialConfidence(a, now);
    }
    const auto* result = a.owner.result(a.commissioningRequest);
    if (!result) return;
    MotorControlRS::ActionEvent event;
    event.transport.operationId = a.commissioning.operationId; event.transport.step = a.commissioning.step;
    event.transport.kind = result->transport.reason == Rtu::Reason::FRAME ? ReadEventKind::FRAME :
        result->outcome == Rtu::Outcome::CANCELLED ? ReadEventKind::CANCEL : ReadEventKind::TRANSPORT_FAILURE;
    copyCompletion(event.transport, *result);
    event.txComplete = result->transport.txComplete;
    event.responseConfirmed = event.transport.kind == ReadEventKind::FRAME &&
        (a.commissioning.step != 0 || a.commissioningResponseQualified);
    if (!ESS::advanceCommunication(a.commissioning, event, now)) return;
    if (a.commissioning.step && a.commissioning.observedActiveKnown &&
        a.commissioningRequestBinding == a.bindingGeneration)
        a.commissioningConfirmedGeneration = a.serial.generation;
    a.owner.release(a.commissioningRequest); a.commissioningRequest = Rtu::RequestId();
}
Probe::Action communication(void* context, const Probe::CommunicationCommand* command, Probe::CommunicationView& out) {
    App& a = *static_cast<App*>(context); communicationView(a, out);
    if (!command) return Probe::Action::OK;
    using Kind = Probe::CommunicationCommandKind;
    // A parsed begin takes the stated facts before any refusal (one-shot).
    const Probe::CommunicationFacts facts = command->kind == Kind::BEGIN ? a.communicationFacts : Probe::CommunicationFacts();
    if (command->kind == Kind::BEGIN) a.communicationFacts = Probe::CommunicationFacts();
    if (a.discovery.owned) return Probe::Action::BUSY;
    const uint64_t now = uart.sample();
    if (!platformReady) return Probe::Action::UNAVAILABLE;
    if (command->kind == Kind::PREVIEW) return Probe::Action::OK; // Console shows source effects and current fixture prerequisites.
    if (a.persistenceRequest.owner || a.persistenceCapture || a.node.view.pending) return Probe::Action::BUSY;
    if (command->kind == Kind::BEGIN) {
        // Do not move an unexpired physical-address wait to an unproven endpoint.
        if (command->request.field == ESS::CommunicationField::ADDRESS && enableSettling(a, command->address, now))
            return Probe::Action::BUSY;
        if (a.owner.commissioningOwned() || a.serial.blocked || !a.serial.activeKnown ||
            a.owner.active() || a.owner.pending() || reading(a) || acting(a) ||
            a.monitorState.settings.enabled || (a.recovery.operationId && !a.recovery.automatic) || axisReserved(a) ||
            groupSequencing(a)) return Probe::Action::BUSY;
        if (!a.nextOperationId || a.serial.generation == UINT32_MAX || a.bindingGeneration == UINT32_MAX)
            return Probe::Action::IDS_EXHAUSTED;
        // Facts are the operator's statements (wired bench 17): one exact request,
        // bound and age-limited, taken by this begin above.
        const bool stated = factsValidity(a, facts, now, a.axis.target) == Probe::FactsValidity::VALID &&
            sameCommunicationRequest(facts.request, command->request);
        auto& p = a.commissioningPrerequisites;
        p = ESS::CommunicationPrerequisites();
        if (!stated) return Probe::Action::UNAVAILABLE;
        p.stationaryQualified = facts.stationary; p.effectsQualified = facts.effects; p.routeBackQualified = facts.routeBack;
        p.addressDipOffQualified = facts.dipOff; p.qualifiedRequest = facts.request;
        p.maxAgeUs = a.observationAgeUs();
        p.previous = a.configuration; p.beforeSerial = communicationTuple(a.serial.active);
        MotorControlRS::ReadTarget target = a.axis.target; target.address = command->address;
        if (!Probe::sameTarget(target, a.configuration.target)) return Probe::Action::INVALID;
        const auto prepared = ESS::prepareCommunication(a.commissioningPrepared, target,
            a.nextOperationId, command->request, p, now, now + REQUEST_US);
        if (!prepared) return Probe::Action::INVALID;
        if (!a.owner.beginCommissioning(now, a.commissioningLease)) return Probe::Action::BUSY;
        a.commissioning = a.commissioningPrepared; ++a.nextOperationId;
        a.persistenceParentSession = true;
        a.persistenceBaselineKnown = false;
        a.commissioningInvalidated = false; a.commissioningConfirmedGeneration = 0;
        a.commissioningResponseQualified = writeResponseConfirmed;
        const auto admitted = submitCommunication(a, now); communicationView(a, out); return admitted;
    }
    if (!a.owner.commissioningOwned()) return Probe::Action::INVALID;
    if (!a.persistenceParentSession || !a.commissioning.operationId) return Probe::Action::INVALID;
    if (a.commissioningRequest.owner) return Probe::Action::BUSY;
    if (command->kind == Kind::SELECT_BEFORE || command->kind == Kind::SELECT_REQUESTED) {
        Probe::HostRequest request;
        request.tuple = communicationHost(command->kind == Kind::SELECT_BEFORE ?
            a.commissioning.beforeSerial : a.commissioning.requestedSerial);
        const uint32_t previousGeneration = a.serial.generation;
        const auto selected = hostSerialImpl(&a, &request, out.host, true);
        // Selecting an already-active tuple performs no reconfiguration and
        // does not invalidate its retained candidate response.
        if (selected != Probe::Action::OK || a.serial.generation != previousGeneration)
            a.commissioningConfirmedGeneration = 0;
        communicationView(a, out); return selected;
    }
    if (command->kind == Kind::CONFIRM_BEFORE || command->kind == Kind::CONFIRM_REQUESTED) {
        const bool before = command->kind == Kind::CONFIRM_BEFORE;
        const auto& target = before ? a.commissioning.beforeTarget : a.commissioning.requestedTarget;
        const auto& serial = before ? a.commissioning.beforeSerial : a.commissioning.requestedSerial;
        if (!a.serial.activeKnown || a.serial.blocked || a.owner.needsRecovery() || uart.needsRecovery())
            return Probe::Action::RECOVERY_REQUIRED;
        if (!sameTuple(a.serial.active, communicationHost(serial))) return Probe::Action::INVALID;
        if (!ESS::prepareCommunicationConfirmation(a.commissioning, target, serial, now, now + REQUEST_US))
            return Probe::Action::INVALID;
        a.commissioningConfirmedGeneration = 0;
        const auto admitted = submitCommunication(a, now); communicationView(a, out); return admitted;
    }
    if (command->kind != Kind::FINISH) return Probe::Action::INVALID;
    if (a.persistenceParentSession && a.persistence.operationId && !a.persistenceFinished)
        return Probe::Action::RECOVERY_REQUIRED;
    const auto& session = a.commissioning;
    const bool noWrite = session.execution == MotorControlRS::ActionExecution::NOT_TRANSMITTED && !session.effects;
    if (!a.serial.activeKnown || a.serial.blocked || a.owner.needsRecovery() || uart.needsRecovery() ||
        (noWrite && !sameTuple(a.serial.active, communicationHost(session.beforeSerial))) ||
        (!noWrite && (!session.observedActiveKnown || a.commissioningConfirmedGeneration != a.serial.generation ||
            !sameTuple(a.serial.active, communicationHost(session.observedActiveSerial))))) return Probe::Action::RECOVERY_REQUIRED;
    if (a.bindingGeneration == UINT32_MAX) return Probe::Action::IDS_EXHAUSTED;
    if (!a.owner.endCommissioning(a.commissioningLease, now)) return Probe::Action::BUSY;
    a.commissioningLease = 0;
    a.persistenceParentSession = false;
    a.persistenceBaselineKnown = false;
    a.commissioningPrerequisites = ESS::CommunicationPrerequisites(); // The session's facts end with it.
    if (!noWrite) {
        const uint8_t selected = session.observedActiveTarget.address;
        ++a.bindingGeneration;
        invalidateAxis(a);
        a.axis.target.address = a.axis.target.id = selected; a.axis.target.generation = a.bindingGeneration;
        a.address = selected;
        invalidateSerialConfidence(a, now);
    }
    communicationView(a, out); return Probe::Action::OK;
}
