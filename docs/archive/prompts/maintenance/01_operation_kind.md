# 01: make application operation selection explicit

> **Archived: Superseded, never executed.** Do not dispatch this file.
> Use the [active prompts](../../../prompts/README.md) and
> [current backlog](../../../backlog.md). The original instructions below
> describe their historical snapshot; they do not reopen completed work.

Superseded by [implemented prompt 01](../maintainability/01_operation_kind.md).
The original instructions below are retained as an unexecuted historical draft.

Read [the execution rules](README.md). Inspect `App::Record` in
`examples/probe_cli/ProbeApp.cpp`, all admission sites and all callers of
`terminal`, `actionEvent`, `advanceOperation`, `cancel`, `lookup`, `deliver`
and `recycleInteractiveResults`. Inspect the public operation-specific context
contracts and the actual FieldCore device binding/result boundary read-only.

Problem: a record stores separate read/action/move/velocity/driver/home booleans.
Several functions repeat a priority-ordered ternary/if chain to decide which
context applies. This permits invalid combinations and makes adding or reviewing
one path require checking implicit precedence in many places.

1. Inventory every record role, including plain probe/capture and unused slots,
   plus the separate `App::recovery` lifecycle. Record valid transitions and
   each role's storage, admission quota,
   cancellation, terminal detection, delivery and release behavior. Do not guess
   that all paths share action semantics.
2. Replace only the private selection booleans with one explicit enum. Keep all
   current contexts/storage first; do not combine this with a union, allocation
   reduction or public API migration. Initialization/reuse must set one kind.
3. Use direct exhaustive switches or short named helpers at selection sites.
   Keep operation-specific parser and result logic visible. Do not route every
   operation through a template framework merely to shorten a switch.
4. Exercise meaningful boundary cases: failed preparation, slot reuse, interrupted
   motion plus independent stop, ordinary-result pressure, undelivered results,
   failed restore, stale generation and recovery. Existing `move_app`,
   `simple_motion_app`, `velocity_app`, `home_app`, `driver_app`,
   `probe_app_monitor_control` and `discovery_app` fixtures provide the baseline;
   add only missing lifecycle cases uncovered by the inventory.

Accept when each role has one unambiguous selection, public results/frames and
memory ownership are unchanged, affected tests/full quick verification pass,
both firmware consumers compile, and exact-commit CI is green. Report any
measured size change without claiming a memory optimization.
