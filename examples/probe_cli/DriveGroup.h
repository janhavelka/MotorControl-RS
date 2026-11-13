// SPDX-License-Identifier: MIT
#pragma once
#include <MotorControlRS/ActionOperation.h>
#include <MotorControlRS/MoveOperation.h>
#include <MotorControlRS/ReadOperation.h>
#include <MotorControlRS/Status.h>
#include "PositionStage.h"
#include <cstddef>
#include <cstdint>

namespace MotorControlRSExample { namespace Probe {
enum class Action : uint8_t; // Values are defined in ProbeConsole.h; zero is OK.

/** Drive group of the standalone application (multi-drive 02): up to the 15
 * drives the ESS hardware manual allows on one bus. Every dependent quota is
 * sized from this value: one move record per member beside the ordinary results,
 * a stop and an escalation per member, the owner's result and urgent quotas, and
 * the console output buffer for one group line. Plain data: no I/O or allocation. */
constexpr std::size_t GROUP_CAPACITY = 15;
/** Group stop operations retained at once: one stop and one escalation. */
constexpr std::size_t GROUP_STOP_OPERATIONS = 2;

/** What a group move does after a member's move fails before every start is
 * resolved, including a lost start reply. NONE is never admitted: the caller
 * always chooses. Later starts are never sent and a start is never resent. */
enum class GroupFailurePolicy : uint8_t { NONE, STOP_NORMAL, STOP_FAST, LEAVE };
enum class GroupOperationKind : uint8_t { SET, MOVE, STOP, READ_STATE };
/** Start evidence of one member. REJECTED is a checked drive exception. */
enum class GroupStart : uint8_t { NOT_SENT, STARTED, UNKNOWN, REJECTED };
/** Declaration evidence of one member. ABSENT: no checked reply. UNEXPECTED:
 * a reply arrived, but not a checked ESS identity of this exact node. */
enum class GroupJoin : uint8_t { NOT_READ, JOINED, ABSENT, UNEXPECTED };

/** STAGED: every member's five 0x0021 words are written and read back first,
 * with no start; then only the start words go out, back to back. SEQUENTIAL:
 * each member runs its ordinary move (write, then start) in turn. */
enum class GroupStartMode : uint8_t { STAGED, SEQUENTIAL };
/** Staging evidence of one member in a staged group move. */
enum class GroupStageReadback : uint8_t { NOT_READ, MATCH, MISMATCH };

/** One common finite relative displacement and native speed for every member.
 * Each member prepares and checks it as its own ordinary move. */
struct GroupMoveRequest {
    MotorControlRS::MoveRequest move; ///< configurationGeneration is replaced by each member's own.
    GroupFailurePolicy onFailure = GroupFailurePolicy::NONE;
    GroupStartMode start = GroupStartMode::STAGED;
};

/** Cached member facts; building this view causes no bus traffic. */
struct GroupMemberView {
    uint8_t address = 0;
    MotorControlRS::ReadTarget target;
    uint32_t configurationGeneration = 0;
    bool current = false; ///< Declared in the current binding and its facts are still bound.
    bool identityKnown = false, configKnown = false, profileKnown = false;
    uint16_t rawModel = 0, rawVersion = 0, rawActiveNode = 0;
    uint16_t profile[6] = {}; ///< Last checked 0x0020/6 words.
    MotorControlRS::InputWiring inputs[4] = {}, outputs[2] = {};
    bool motionFresh = false, ioFresh = false, feedbackFresh = false;
    uint16_t rawAlarm = 0, rawMotion = 0, rawInputs = 0;
    bool positionKnown = false;
    uint32_t rawPosition = 0;
    uint64_t motionObservedUs = 0;
    bool settling = false; ///< Post-enable wait of this member.
    uint64_t settleUntilUs = 0;
    bool reserved = false; ///< An operation or an uncertain-motion interlock owns this address.
    bool startSpeedRelationRequired = false;
};
struct GroupView {
    uint8_t count = 0;
    uint32_t declaration = 0; ///< Increments with every group set or clear.
    uint32_t bindingGeneration = 0;
    uint64_t nowUs = 0;
    bool sequencing = false; ///< A group set, read or start sequence admits no other ordinary work.
    uint32_t setOperationId = 0, readOperationId = 0, moveOperationId = 0;
    uint32_t stopOperationIds[GROUP_STOP_OPERATIONS] = {};
    GroupMemberView members[GROUP_CAPACITY];
};

/** Copied evidence of one member, independent of every other member. */
struct GroupMemberResult {
    uint8_t address = 0;
    uint32_t operationId = 0; ///< Child read, move or stop; zero when none was admitted.
    Action admission{}; ///< OK, or why the child was not admitted.
    GroupJoin join = GroupJoin::NOT_READ;
    MotorControlRS::ReadState readState = MotorControlRS::ReadState::EMPTY;
    MotorControlRS::ActionState state = MotorControlRS::ActionState::EMPTY;
    MotorControlRS::ActionOutcome outcome = MotorControlRS::ActionOutcome::NONE;
    MotorControlRS::ActionExecution execution = MotorControlRS::ActionExecution::NOT_TRANSMITTED;
    MotorControlRS::ActionExecution setupExecution = MotorControlRS::ActionExecution::NOT_TRANSMITTED;
    MotorControlRS::ActionCompletion completion = MotorControlRS::ActionCompletion::NOT_OBSERVED;
    MotorControlRS::Status status;
    bool uncertain = false, interruptedByStop = false;
    GroupStart start = GroupStart::NOT_SENT;
    int64_t effectiveNative = 0;
    bool startTxComplete = false; ///< Start write TX completion from the runner's physical evidence.
    uint64_t startTxEndUs = 0;
    uint32_t startTxUncertaintyUs = 0;
    bool runningObserved = false; ///< First RUNNING report, bounded by its poll reply closure.
    uint64_t runningEarliestUs = 0, runningLatestUs = 0;
    MotorControlRS::ActionState stageState = MotorControlRS::ActionState::EMPTY; ///< STAGED only; EMPTY: not attempted.
    MotorControlRS::ActionExecution stageExecution = MotorControlRS::ActionExecution::NOT_TRANSMITTED;
    GroupStageReadback stageReadback = GroupStageReadback::NOT_READ;
    bool stageTxComplete = false;
    uint64_t stageTxEndUs = 0, stageReadLatestUs = 0;
    MotorControlRS::Status stageStatus;
    uint32_t stopOperationId = 0; ///< Failure-policy stop; zero when none was sent.
    Action stopAdmission{};
    MotorControlRS::ActionState stopState = MotorControlRS::ActionState::EMPTY;
    MotorControlRS::ActionExecution stopExecution = MotorControlRS::ActionExecution::NOT_TRANSMITTED;
    MotorControlRS::ActionCompletion stopCompletion = MotorControlRS::ActionCompletion::NOT_OBSERVED;
    uint16_t rawModel = 0, rawVersion = 0, rawActiveNode = 0;
    bool configKnown = false, profileKnown = false, stateKnown = false, positionKnown = false;
    uint16_t rawAlarm = 0, rawMotion = 0, rawInputs = 0;
    uint32_t rawPosition = 0;
};
struct GroupResult {
    GroupOperationKind kind = GroupOperationKind::SET;
    uint32_t operationId = 0, commandId = 0;
    bool pending = false, ok = false;
    uint32_t declaration = 0, bindingGeneration = 0;
    GroupMoveRequest move; ///< MOVE only.
    MotorControlRS::StopBehavior stop = MotorControlRS::StopBehavior::UNSPECIFIED; ///< STOP only.
    bool aborted = false; ///< A member failed during declaration or before every start was resolved.
    uint8_t failedAddress = 0;
    uint64_t startedUs = 0, finishedUs = 0;
    uint8_t count = 0;
    GroupMemberResult members[GROUP_CAPACITY];
};

enum class GroupCommandKind : uint8_t { SHOW, STATUS, SET, CLEAR, WIRING, MOVE, STOP, READ_STATE };
/** One group request; copied by the application before returning. */
struct GroupCommand {
    GroupCommandKind kind = GroupCommandKind::SHOW;
    uint8_t addresses[GROUP_CAPACITY] = {}; ///< SET, in start order.
    std::size_t count = 0;
    uint8_t address = 0, terminal = 0; ///< WIRING: member and X0..X3=0..3, Y0..Y1=4..5.
    MotorControlRS::InputWiring wiring = MotorControlRS::InputWiring::UNKNOWN;
    GroupMoveRequest move;
    MotorControlRS::StopBehavior stop = MotorControlRS::StopBehavior::UNSPECIFIED;
};
struct GroupReply {
    uint32_t operationId = 0; ///< SET, MOVE, STOP and READ_STATE admission.
    uint8_t address = 0; ///< Member that caused a refusal; zero otherwise.
    const GroupView* view = nullptr; ///< Borrowed during the synchronous call only.
};

}} // namespace MotorControlRSExample::Probe
