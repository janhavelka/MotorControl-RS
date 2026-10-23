# 13 - Verify cross-operation faults and ordinary-route parity

Execute the [shared contract](execution_contract.md) and
[verification rules](verification.md). Scope: interactions among delivered
02-12 features. Prerequisites: their actual contracts and explicit dispositions;
blocked features remain excluded visibly, never substituted by fake success.

## Inspect and assign

Read final diffs, all affected app admission/result/cache paths, real console
grammar/help, host tool validators, owner/runner and coverage checker.
Lead owns cross-feature corrections. Spawn read-only reviewers for
(1) state/effect/result conservation and (2) public API/CLI/host parity.
A test agent may own one disjoint integration fixture after scenario contracts
are fixed. Reviewers use actual code and low-level scripted transport traces.

## Work

1. Exercise reads around start-speed/auxiliary changes, interruption and homing:
   stale read results cannot restore invalidated qualification or replace newer
   configuration; settings changes cannot mutate another pending request.
2. Review admission and invalidation across target/rebind/configuration changes,
   cancellation, full result storage and explicit host recovery. Original,
   replacement, home and urgent-stop effects remain attributable and retrievable.
   Do not let a new read, successful cleanup or statistics reset erase uncertainty.
3. Verify every new command reaches the same public/app API as programmatic
   callers. Inspect help, malformed/extra tokens, status/results, unknown raw
   codes, unsupported choices and both platform frontends. Preserve protocol-3
   fields/semantics; change host validators alongside any justified additions.
4. Extend relevant scripted host harness cases through real parsers/owners, not
   a separate model implementing the expected success. Avoid broad host-tool
   refactoring; extract only repeated behavior the new cases actually need.
5. Inspect confirmed defects, identify their owning transition, fix that cause
   simply and add a meaningful regression. Do not hide failures with longer
   timeouts, retries, catch-all success or generic recovery.
6. Reconcile public symbol classification and producing CLI routes in the checked
   ledger. Negative fixtures still reject invented routes, missing definitions
   and attempts to count raw codecs/getters as implemented operations.

## Acceptance

Run targeted sequences: parameter read -> setting update -> stale read delivery;
home qualification -> configuration change -> rejected home; active move ->
replacement -> urgent stop with full ordinary results; lost ACK -> read-only
reconciliation -> explicit new admission; old event arriving after rebind.
Choose exact expected states/effects/results before running each trace.

Review frame identity, immutable deadlines, zero traffic on rejected requests,
copied evidence, no replay and independent terminal retention. Run focused
integration/parity/inventory suites, then fresh quick verification and required
exact-commit CI. Record defects/fixes, actual method/route coverage and remaining
source/physical limits. Do not defer normal route implementation to this stage.

