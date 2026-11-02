# 07 - Investigate reset-like and stop failures

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **Q04, Q05**.

## Entry gate

Actual ESS23-RS20/firmware 0x0029 or explicitly recorded replacement, bounded authorized reproducer. Supply instrument required for causal claims on Q04; Q05 reply-level reproduction does not need it.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

docs/reports/2026-10-07_rate_boundary.md, 2026-10-07_position_completion.md, 2026-10-07_position_limits.md; scripts/bench_motion.py, bench_repeat.py, bench_scenarios.py; src/profiles/ess_rs/Position.cpp, Actions.cpp; standalone result/health capture.

## Work

1. Recover exact historical failing cases and preserve their original evidence.
   Correlate drive supply/transient measurement, UART traffic, host health,
   raw drive state and volatile settings. Distinguish drive reboot, communication
   loss, host reset and still-running motion; do not infer the cause from one.
2. Name each historical case and its explicit `PositionLimits` policy (D5). Run
   one case at a time. D5 authorizes named cases above the defaults, for example
   the reset case at 2,700 rpm and 4,000 subdivision. These are not "small finite"
   tests. This stage owns a typed `PositionLimits` route on the ordinary image
   before any D5 case. None exists today: `ApplicationOptions` has none and
   `ProbeApp.cpp` assigns no limits. Without it record PARTIAL. The datasheet's
   'MAX. INPUT FREQUENCY 200KHZ' and the hardware manual's 'Pulse frequency' row
   name no input. The row neither establishes nor excludes a limit on serial
   motion. The sub-200k counterexample applies only to reset-like loss (2,700 rpm
   at 4,000 subdivision, 180,000/s). Recorded persistent-RUNNING cases all have
   requested rates at or above 256,000/s (short moves may not reach the requested
   rate), yet some higher rates passed, so an internal rate constraint is not ruled
   out for that symptom.
3. Investigate persistent RUNNING and normal-stop failures independently of the
   already corrected host stop observation budget. Retain stop and original
   motion outcomes separately.
4. Prepare a firmware/model/vendor evidence bundle at
   `docs/reports/DATE_standalone_07_vendor_bundle.zip`. Do not contact a vendor
   without an explicit instruction to send it.
5. Change library policy only for a demonstrated host defect or defensible
   capability bound; do not tune away evidence or weaken completion checks.

## Acceptance and handoff

Deliver correlated measurements and a causal disposition, or explicitly
unresolved cause with the missing instrument/vendor fact. A passing narrower
campaign does not close broader FAIL records. Any confirmed code fix needs a
regression exercising the failure and full applicable verification/CI.

Follow the execution contract for local checks, impact-based CI, documentation,
owned commits/pushes and explicit remaining hardware/vendor gates. Update this
stage's status only from actual evidence.
