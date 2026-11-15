# Multi-drive 08 - Preserve enable waits across host selection changes

Software correction delivered; see the [stage table](README.md#stages) and
[verification report](../../reports/2026-11-15_multi_drive_08_enable_wait_lifetime.md).
Retain this prompt for its pending board regression. No early-start experiment
is needed, and HW-15 remains separate.

Read the [set README](README.md), the
[execution contract](../standalone/execution_contract.md) and the
[15 November revalidation](../../reports/2026-11-15_ess_multi_drive_promptset_audit.md).

Finding: **M02**, per-target readiness lifetime. Software work has no fixture gate.
Complete this correction before extending group move requests in 09.

## Inspect and reproduce

Read `ProbeApp.cpp` (enable-result handling, `enableSettling`, `selectTarget`),
`GroupApp.h` (`startGroupSet`, `clearGroupMembers`), `MotionReadinessApp.h`,
`ProbeApp.h` and `test/group_app_test.cpp`. Reinspect the current FieldCore
motor module and owner read-only. Inspect CI and reuse its verification commands.

Enable waits currently live in each group member and one legacy last-address
slot. Reconstructing or clearing the group discards member waits while preserving
only that last address. Reproduce with two enabled addresses and a redeclaration
that omits the last-enabled member. Check both selected-axis and group admission
before the earlier member's original wait expires. A host selection must not
erase an unexpired drive-side wait.

## Work

1. Retain each known target's unexpired wait independently of group membership.
   Use bounded caller-owned application storage. Account for address selection,
   clear/redeclare, failed declarations, recovery and binding changes explicitly.
   Rebinding or invalidating cache evidence is not proof that the drive's wait
   elapsed. Where identity cannot be preserved, fail closed until new evidence
   and the applicable wait permit motion.
2. Keep the existing wait duration and conservative treatment of possible enable
   writes. Retain its timestamp origin and the original write evidence. Do not
   repair this by replaying enable, shortening the wait or imposing a global wait
   that loses which target was affected.
3. Route selected-axis move/stage and group admission through the same retained
   fact. Expiry uses supplied monotonic time. Define bounded capacity and reuse
   only when expired or otherwise justified; no silent eviction of an active wait.
4. Measure storage changes and keep platform types out of the core. This is an
   application lifetime correction, not a new core transport or retry policy.

## Acceptance and handoff

- Reproduce the lost hold before the fix and show that no motion/start frame is
  sent before the original deadline after the fix.
- Cover two enables, subset/reordered/failed declarations, clear then re-add,
  selected-axis access to a removed member, recovery/rebinding, expiry and capacity.
- Assert that unrelated targets are not given another target's timestamp and
  that a possible transmitted enable never becomes NOT_TRANSMITTED.
- Run focused group/selected-axis/stop tests, the quick verifier, and exact-final-
  commit firmware CI under the execution contract. Do not weaken existing tests.
- When this behavior reaches the board, run a short bounded regression plus
  enable/reselection checks on the recorded fixture. No early-start experiment
  is needed: verify refusal before the deadline and one finite move after it.

Update the report, backlog and stage table with implementation and hardware
evidence separately. Do not close the separate HW-15 drive-timing investigation
merely because the host preserves its current one-second policy correctly.
