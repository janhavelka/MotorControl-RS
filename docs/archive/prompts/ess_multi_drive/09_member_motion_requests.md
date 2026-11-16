# Multi-drive 09 - Give each group member its own motion request

Archived 16 November 2026. Do not dispatch this completed prompt again.
Use the [active set](../../../prompts/ess_multi_drive/README.md) for current work.

Read the [set README](../../../prompts/ess_multi_drive/README.md), the
[execution contract](../../../prompts/standalone/execution_contract.md) and the
[15 November revalidation](../../../reports/2026-11-15_ess_multi_drive_promptset_audit.md).

Finding: **M02**, the original different-target requirement. Multi-drive 08 must
be resolved first. The common-relative STAGED/SEQUENTIAL routes remain delivered.

## Inspect before editing

Read `DriveGroup.h` (`GroupMoveRequest`), `ProbeApp.h`, `GroupApp.h`
(`memberFacts`, `startGroupMove`), `PositionStageApp.h`, `MotionReadinessApp.h`,
`Axis.h`, `Position.h`/`Position.cpp`, the group console/host routes and tests.
Inspect the current FieldCore motor module/owner read-only and the current CI.

The existing request has one `MoveRequest`, copied to every member, and rejects
absolute and wrapped requests. Group members have no independent reference or
origin. Do not call this full per-member target support or close standalone 02's
separate archive-import work through the existing prepared staging API.

## Work in tested blocks

1. Add a bounded copied request containing exactly one intent per declared
   member. Reject duplicates, omissions, non-members and capacity errors before
   any staging write. Preserve the common-relative convenience route by mapping
   it to the same implementation. Keep explicit failure policy with no default.
2. Prepare different relative values through each member's existing conversion,
   readiness, subdivision/rate limits and word order. Retain each effective native
   target and exact intent. No member inherits another member's scale or evidence.
3. Add explicit per-member reference/origin facts for absolute requests. Bind
   them to target, configuration and observation generation. Reuse the ordinary
   core absolute preparation and completion rules. Reject unresolved conversion,
   stale references and unsupported angle policies before writes. Do not infer
   an origin from group declaration or raw encoder feedback alone.
4. For STAGED, validate the whole group first, then write and check every member
   before any start. Preserve independent staging/start/completion evidence.
   Unicast starts may use different modes; broadcast is still NO-GO. A lost start
   is UNKNOWN, later starts remain unsent and the explicit failure policy applies
   to every possibly started member. Never resend the lost start.
5. Add ordinary API and additive protocol-3 console/host routes. Keep command
   length, token, response and memory bounds explicit. A host request assembled
   over several commands must remain inert until complete and explicitly submitted;
   it is not a series of independently executing moves.

## Acceptance and handoff

Test mixed signs/values, member-specific subdivision/order, missing and stale
references, absolute limits, duplicate/missing/non-member intents and capacity.
An invalid last member must cause zero writes to all members. Verify every
staging readback precedes every start, exact frames name only declared members,
and middle-member uncertainty keeps independent outcomes under both failure
policies. Repeat urgent-stop, recovery, no-replay and output-on-error regressions.

Use the same public requests from native tests, console and upper firmware.
Run focused tests, the quick verifier and exact-final-commit firmware CI. Measure
application/result/console storage and stack effects at capacity 15. Physical
checks use only the recorded three-drive fixture and small finite moves, plus
the short regression; independently measured start skew remains a separate gate.

Update the guides, checked operation inventory, backlog and stage table. M02 can
close only when its retained lifecycle and per-member request requirements have
their own implementation and relevant verification evidence.
