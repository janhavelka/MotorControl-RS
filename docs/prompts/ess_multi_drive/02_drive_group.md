# Multi-drive 02 - Add a drive group to the standalone application

Read the [set README](README.md) and the
[standalone execution contract](../standalone/execution_contract.md) together
with this prompt.

Finding: **M02**.

## Current disposition

The common-relative group implementation and staged starts are delivered, with
three-drive evidence in the [13 November report](../../reports/2026-11-13_multi_drive_02_drive_group.md).
The original requirement for different member targets is still open. M02 is
PARTIAL against that full requirement. The
[multi-drive 08](08_enable_wait_lifetime.md) software correction is delivered;
its board regression remains pending. Continue software work with
[multi-drive 09](09_member_motion_requests.md) for member-specific requests.
Do not rebuild the delivered group or treat its finite bench results as missing.

## Entry gate

None for software and native tests. The recorded bench has three drives at
1, 2 and 4; inspect its current state before any new trial. Further physical
qualification belongs to multi-drive 06.

`stagePositionMove` and `PositionStageApp.h` already stage a prepared finite
move without starting it. STAGED group moves use this capability for every
member before issuing starts. SEQUENTIAL remains an explicit alternative.
[Standalone 02](../../reports/2026-11-15_standalone_02_profile_staging.md) delivers
the separate archive/candidate console route; it is not a prerequisite for the
delivered group staging path.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
tests, not just declarations:

examples/probe_cli/GroupApp.h, PositionStageApp.h, ProbeApp.h, ProbeApp.cpp (`selectTarget` for `useaddr`,
`enableWaits`, `enableSettling`, `axisReserved`, the `STOP_RECORDS` urgent quota),
CommunicationApp.h (binding generation), MotionReadinessApp.h,
SimpleMotion.h, SimpleMotionApp.h, StateCache.h, ProbeConsole.cpp/.h,
ConsoleText.cpp; examples/common/RtuBusOwner.h and its tests; docs/bus_owner.md,
docs/cli_contract.md, docs/console.md, docs/move_example.md; scripts/bench_probe.py;
test/group_app_test.cpp, probe_console_test.cpp, probe_app_test.cpp,
move_app_test.cpp and the owner tests.

## Work

1. **Group membership.** Preserve the delivered fixed capacity of 15 members
   and measured storage. Hardware p11 describes up to 15 drives on a bus, but
   its DIP table assigns only addresses 1-14; all four address switches ON
   means factory reset. Capacity 15 does not authorize that switch setting.
   - Declaring a group is idle-only. It is a host selection, never a drive change.
   - Each member needs a fresh checked identity and configuration read before it
     joins. An absent or unexpected responder rejects the declaration before any
     write.
   - The single selected axis and `useaddr` keep their current behaviour.
2. **Per-member facts.** Keep, per member: target and generation, configuration,
   I/O and state caches, the staged-profile readback and the post-enable wait.
   - Preserve unexpired enable waits even when a member is removed or the group
     is redeclared. Host selection cannot shorten a drive's wait; see 08.
   - Move-readiness rules must not be copied. Extract the facts that
     `MotionReadinessApp.h` reads into a per-target structure, and use the same
     readiness function for the selected axis and for group members.
3. **Group move.**
   - Preserve STAGED: stage each member's finite position move by unicast and
     check every readback before sending any start. SEQUENTIAL runs ordinary
     member moves in order and includes staging in its measured start spread.
   - The delivered request contains one common relative move. Different member
     targets and absolute requests remain work for 09, with each member's own
     scale/reference and configuration generation. Broadcast remains disabled.
   - Only urgent stops may interleave.
   - Record per-member TX completion and the first RUNNING observation, so the
     start spread is measured rather than assumed.
   - Each member's outcome stays independent: started, unknown or not sent.
4. **Failure policy.** The group-move request must state what happens when a
   start fails or its ACK is lost: stop the members already started, or leave
   them. There is no default, so the caller always chooses. Remaining starts
   are never sent after a failure. A lost start is never resent.
5. **Group stop and status.**
   - `group stop normal|fast` sends an urgent unicast stop to each member, each
     with its own reservation and result.
   - Preserve the delivered urgent pending/result quotas and separate member
     move pool, sized from group capacity. Keep the protocol-3 advertised
     outstanding capacity.
   - `group read state` refreshes every member.
   - Cached `group status` reports members without bus traffic.
6. **Interfaces.** Expose the group through the public application API that
   upper firmware uses. The console routes call that same API. Use additive
   protocol-3 commands: `group set ADDR...`, `group show`, `group clear`,
   `group move`, `group stop`, `group read state`, `group status`. Update
   `bench_probe.py` and the host validators.

## Acceptance and handoff

Native tests with a fake bus of several members:
- declaration against absent and unexpected responders;
- a lost ACK and a timeout on the middle member, under both failure policies;
- an urgent stop during staging and during the start sequence;
- a full-group stop at maximum capacity;
- capacity limits and output-on-error;
- generation invalidation after `useaddr`, enable or recovery;
- a check that no write goes to a non-member.

Retain the existing three-drive staged/sequential evidence. New behavior needs
its own focused checks and short regression; physical capacity above three and
independent shaft/electrical timing remain NOT RUN. Update docs/console.md,
docs/cli_contract.md, docs/ess_api_cli_coverage.md and the backlog. CI required:
application and API change.
