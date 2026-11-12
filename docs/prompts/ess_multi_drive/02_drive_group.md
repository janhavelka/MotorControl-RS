# Multi-drive 02 - Add a drive group to the standalone application

Read the [set README](README.md) and the
[standalone execution contract](../standalone/execution_contract.md) together
with this prompt.

Finding: **M02**.

## Entry gate

None for software and native tests. Multi-drive physical claims need the
multi-drive 06 fixture. On the one-drive bench, test a group of one only.

Today a move verifies, stages and starts in one operation
(`Position.cpp:329-344`). Staging every member before any start therefore
needs [standalone 02](../standalone/02_profile_staging.md), which is NOT
STARTED. Without it, the group move runs each member's ordinary move one after
another. The measured spread then includes each member's staging writes.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
tests, not just declarations:

examples/probe_cli/ProbeApp.h, ProbeApp.cpp (`selectTarget` for `useaddr`,
`enableSettleAddress`, `axisReserved`, the `STOP_RECORDS` urgent quota),
CommunicationApp.h (binding generation), MotionReadinessApp.h,
SimpleMotion.h, SimpleMotionApp.h, StateCache.h, ProbeConsole.cpp/.h,
ConsoleText.cpp; examples/common/RtuBusOwner.h and its tests; docs/bus_owner.md,
docs/cli_contract.md, docs/console.md, docs/move_example.md; scripts/bench_probe.py;
test/probe_app_test.cpp, move_app_test.cpp and the owner tests.

## Work

1. **Group membership.** Add a fixed-capacity group of member addresses. Choose
   the capacity from measured RAM; the hardware manual's 15 drives on one bus is
   the upper bound.
   - Declaring a group is idle-only. It is a host selection, never a drive change.
   - Each member needs a fresh checked identity and configuration read before it
     joins. An absent or unexpected responder rejects the declaration before any
     write.
   - The single selected axis and `useaddr` keep their current behaviour.
2. **Per-member facts.** Keep, per member: target and generation, configuration,
   I/O and state caches, the staged-profile readback and the post-enable wait.
   - Replace the single `enableSettleAddress` slot with one slot per member.
   - Move-readiness rules must not be copied. Extract the facts that
     `MotionReadinessApp.h` reads into a per-target structure, and use the same
     readiness function for the selected axis and for group members.
3. **Group move.**
   - With standalone 02: stage each member's finite position move by unicast
     and check the readback. Then send only the start commands, back to back in
     member order. Without it: run each member's ordinary move in order, as the
     entry gate describes.
   - The members' targets may differ. A later broadcast start (multi-drive 03)
     sends one `0x0027` word, so it needs every member in the same relative or
     absolute mode.
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
   - The application has two urgent slots today (`STOP_RECORDS = 2`,
     `ProbeApp.cpp:51,76`). Size the urgent pending and result quotas to the
     group capacity. Keep the protocol-3 advertised outstanding capacity.
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

Bench: a group of one runs the short regression and the group routes. Mark every
multi-drive claim NOT RUN until multi-drive 06. Update docs/console.md,
docs/cli_contract.md, docs/ess_api_cli_coverage.md and the backlog. CI required:
application and API change.
