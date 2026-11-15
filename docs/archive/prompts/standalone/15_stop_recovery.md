# 15 - Finish stop delivery and result handling

Completed software instructions, archived 15 November 2026. Do not dispatch
again. See the [implementation and verification record](../../../reports/2026-11-15_standalone_15_stop_recovery.md).
Hardware checks remain separate.

Read the [execution contract](../../../prompts/standalone/execution_contract.md) and the
[current audit](../../../reports/2026-11-15_standalone_audit_revalidation.md).
This stage owns R-02..R-06, R-17 and the ESS portions of C1, C11, C12 and C14.
It does not implement a second manufacturer or a new bus scheduler.

Complete the blocks below separately. Freeze and test each contract before
changing the next one. A later block must not weaken earlier result retention.

## 15a - Finite velocity deadlines and the first stop transmission

Inspect `Velocity.cpp`, `PreparedVelocity`, `ProbeApp.cpp::admitVelocityStep`,
`advanceActions`, `holdStop`, the owner dispatch deadline and runner settlement.
Read the current FieldCore motor/stop result boundary read-only.

1. Reproduce C1 through the real owner with a status poll admitted before
   `stopDueUs` whose reply closes after it. Separate the last allowed poll
   dispatch from its bounded wire/settlement deadline. The run deadline and
   stop deadline remain immutable. Include healthy durations on both sides of
   the poll boundary; do not fix this by extending every timeout.
2. After a velocity transport fault, the run's explicit stop intent must use
   D1's bounded recovery path if no stop byte was accepted. Preserve the run
   failure, stop outcome and recovery result independently. Do not cancel the
   owned stop merely because recovery changes a binding generation.
3. Cover a stop whose owner result is already terminal with zero accepted
   bytes (R-02). `cancelUnsent` alone cannot recover that result. Consume and
   retain its evidence before admitting the first transmission after one
   bounded recovery. Partial or complete TX must never enter this path.
4. A lost stop ACK remains UNKNOWN. Only a new explicit stop may send again.
   No recovery loop, motion restart or replay is permitted.

Tests: poll closure just before/at/after `stopDueUs`, queue pressure, bus noise
before TX, partial TX, lost trigger ACK, lost stop ACK, recovery failure,
immutable deadline expiry, cancellation and explicit stop escalation. Assert
the exact frame count and independent retained results in every trace.

## 15b - Stop access and observation

1. Define stop behavior while discovery owns a tuple or commissioning owns
   the bus (R-03). A stop cannot guess the active target/tuple. Preserve the
   scan's restoration contract; reject clearly where that contract cannot
   deliver the stop. Remove unconditional claims that stop can never refuse.
2. Let cancelling an in-flight ordinary probe settle its existing transaction
   without creating a recovery fault (R-04). Preserve its result and cancel
   only unsent continuation. Test the next read and stop.
3. End stop observation honestly when the bus is interlocked (R-05), retaining
   its ACK and unobserved completion. Do not manufacture a stopped state.
4. Expose normal programmatic stop submission and copied result retrieval
   from `ProbeApp.h` (C12). The console must use that same application path.
   Do not export the internal `App` or make callers construct private Host
   callbacks. Copying a terminal programmatic result must acknowledge delivery
   under the existing retention/release policy; otherwise the third headless
   stop still fills both slots. Inventory the other internal-only callbacks
   (velocity, enable, release, alarm clear, position clear and recovery) and
   give each an explicit disposition. Do not expand every API in this block.
5. Fix C11's reproduced ordinary cross-address escalation failure. With normal
   stops retained for two supported addresses, a fast stop for either address
   must have reserved capacity without losing either old result. Use the
   smallest bounded storage/admission change. Group storage already has two
   sets of per-member stops; preserve its pressure cases and protocol-3 semantics.
6. Define the explicit recovery actions allowed after a transmitted stop
   fails with motion unresolved (R-17). Release, alarm clear and enable have
   different effects; do not grant them as one blanket exception. Preserve
   old uncertainty, require exact identity and supplied commissioning facts,
   and do not restart motion automatically. Physical release requires its
   existing fixture/authorization limits.

Tests: discovery fault and restore, commissioning lease, probe cancellation,
UART fault after stop ACK, two addresses, full ordinary results, undelivered
stop results, repeated escalation and exact API/CLI parity. Run three headless
stops with copied/released results and a headless normal-to-fast escalation;
wrong result IDs must preserve the caller's output.

## 15c - Host result accounting

Fix R-06 in `bench_probe.py`: firmware can reuse a delivered stop slot, while
the host still counts the old ordinary stop. Mirror the documented replacement
policy, keep the old terminal evidence in the run log, and collect the newly
accepted stop. Do not remove arbitrary retained operations to fit a quota.
Test accepted replacement, two pending stops, output pressure, wrong IDs and
the distinction between ordinary stop and group-stop storage.

## Finish

Run focused velocity, action, owner, stop-path, discovery, console and host
tests after each block, then fresh quick verification. Major owner/API changes
need required exact-commit CI. Update stop/velocity guides, host contracts and
the audit dispositions. Bench only within the existing authorization; native
velocity tests do not authorize an unbounded physical velocity experiment.
Keep the stage PARTIAL until every block has a tested disposition.
