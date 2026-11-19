// SPDX-License-Identifier: MIT
// Commissioning facts for drive communication changes and persistence, and
// the custom node route back (wired bench 17). The operator states each fact by
// name; nothing here is inferred from a register, and a statement is local: it
// causes no bus traffic. The node restore runs only inside the commissioning
// lease of an explicit persistence snapshot.
#pragma once
#include "MotorControlRS/profiles/ess_rs/Actions.h"
#include "MotorControlRS/profiles/ess_rs/Communication.h"
#include "MotorControlRS/profiles/ess_rs/Persistence.h"

namespace MotorControlRSExample { namespace Probe {

/** An installed communication or persistence qualification admits one begin
 * that follows within this age of its statement. */
constexpr uint64_t COMMISSIONING_FACTS_MAX_AGE_US = 60000000;

enum class FactsAction : uint8_t { INSPECT, INSTALL, REVOKE };

/** Whether an installed qualification still admits its begin. A recovery or
 * `useaddr` changes the binding generation, a configuration change the axis
 * generation and a host change the serial generation. */
enum class FactsValidity : uint8_t { NONE, VALID, EXPIRED, BINDING_CHANGED, CONFIGURATION_CHANGED, HOST_CHANGED };

/** `communication qualification install FIELD VALUE stationary effects route-back [dip-off]`.
 * stationary: the drive is stopped and stays so; effects: the operator accepts
 * the change's effects on this bus; route-back: an owner power cycle and the
 * return path to the before tuple are available. dip-off (ADDRESS only, required
 * there): the operator will set SW2-SW5 OFF before the activation restart. */
struct CommunicationFactsCommand {
    FactsAction action = FactsAction::INSPECT;
    MotorControlRS::ESS_RS::CommunicationRequest request;
    bool stationary = false, effects = false, routeBack = false, dipOff = false;
};

/** One exact request, bound to the selected target and its generations. A
 * parsed `communication begin` takes it before any check, refused or not. */
struct CommunicationFacts {
    bool present = false;
    MotorControlRS::ESS_RS::CommunicationRequest request;
    MotorControlRS::ReadTarget target;
    uint32_t bindingGeneration = 0, configurationGeneration = 0, serialGeneration = 0;
    uint64_t installedUs = 0;
    bool stationary = false, effects = false, routeBack = false, dipOff = false;
};
struct CommunicationFactsView {
    CommunicationFacts facts;
    FactsValidity validity = FactsValidity::NONE;
    uint64_t nowUs = 0;
    const char* reason = nullptr; ///< Named refusal of the last install, or nullptr.
};

/** `persistence qualification install save|factory-restore stationary effects route-back backup TOKEN [complete-backup]`.
 * TOKEN is 1..8 hex digits, nonzero: the first 32 bits of the SHA-256 of the
 * harness field table that backs this drive's stored image. complete-backup
 * states that the table has a restore route for every field. FACTORY_RESTORE
 * is refused until the restricted recommissioning sequence exists. */
struct PersistenceFactsCommand {
    FactsAction action = FactsAction::INSPECT;
    MotorControlRS::ESS_RS::PersistenceKind kind = MotorControlRS::ESS_RS::PersistenceKind::SAVE;
    bool stationary = false, effects = false, routeBack = false, completeBackup = false;
    uint32_t backupToken = 0;
};
/** Bound like CommunicationFacts, but to the persistence endpoint: the confirmed
 * responding endpoint inside an open communication session, else the selected
 * axis. A begin needs it for the snapshot's endpoint, which the save writes to;
 * one `persistence begin` consumes it. */
struct PersistenceFacts {
    bool present = false;
    MotorControlRS::ESS_RS::PersistenceKind kind = MotorControlRS::ESS_RS::PersistenceKind::SAVE;
    MotorControlRS::ReadTarget target;
    uint32_t bindingGeneration = 0, configurationGeneration = 0, serialGeneration = 0;
    uint64_t installedUs = 0;
    bool stationary = false, effects = false, routeBack = false, completeBackup = false;
    uint32_t backupToken = 0;
};
struct PersistenceFactsView {
    PersistenceFacts facts;
    FactsValidity validity = FactsValidity::NONE;
    uint64_t nowUs = 0;
    const char* reason = nullptr; ///< Named refusal of the last install, or nullptr.
};

/** `persistence node record|restore WORD SOURCE dip-observed|inspect` (wired
 * bench 17 item 8, D7): the route back of `0x0013`. RECORD (local) retains the
 * custom node word of the current checked session snapshot, normally S1 after
 * an owner power cycle with no write since. RESTORE writes exactly that word
 * back (FC06, then an FC03 readback) inside a later snapshot lease of the same
 * drive. INSPECT copies the state. */
enum class NodeCommandKind : uint8_t { INSPECT, RECORD, RESTORE };
struct NodeCommand {
    NodeCommandKind kind = NodeCommandKind::INSPECT;
    uint16_t word = 0;        ///< RESTORE: must equal the record's word (0..255, 0 included).
    uint32_t source = 0;      ///< RESTORE: must equal the record's source configuration read ID.
    bool dipObserved = false; ///< RESTORE: the operator observed SW2-SW5 set to the drive's address.
};
/** The retained word: target address and ID, word and source read identity. */
struct NodeRecord {
    bool present = false;
    uint8_t address = 0;
    uint32_t targetId = 0;
    uint16_t word = 0;
    uint32_t source = 0; ///< Operation ID of the checked configuration read that held the word.
    uint64_t recordedUs = 0;
};
/** One restore attempt. Admission needs the record, its exact word and source,
 * the stated DIP observation, and a session snapshot of the same drive whose
 * identity read is at most COMMISSIONING_FACTS_MAX_AGE_US old and shows 0x0002
 * equal to the drive's address while 0x0013 holds another word. The write does
 * not change the responding address; the save then needs a fresh snapshot. */
struct NodeRestore {
    MotorControlRS::ReadTarget target;
    uint16_t word = 0, previous = 0, readback = 0;
    uint32_t source = 0, snapshotOperationId = 0;
    bool attempted = false, ok = false, readbackKnown = false;
    MotorControlRS::ActionExecution execution = MotorControlRS::ActionExecution::NOT_TRANSMITTED;
};
/** Copied node state. pending covers the write and its readback. */
struct NodeView {
    NodeRecord record;
    NodeRestore restore;
    bool pending = false;
    uint32_t operationId = 0;
    const char* error = "none";
    MotorControlRS::ESS_RS::ActionEvidence writeEvidence, readEvidence;
};

}} // namespace MotorControlRSExample::Probe
