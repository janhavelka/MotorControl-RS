# 07 - Investigate reset-like and stop failures

Read [README](README.md), [execution contract](execution_contract.md) and the
[audit finding map](audit.md) together with this prompt.

Findings: **Q04, Q05**. Status: **NOT EXECUTED**.

## Entry gate

Actual ESS23-RS20/firmware 0x0029 or explicitly recorded replacement, instrumented supply, bounded authorized reproducer.

## Inspect before editing

Paths are relative to the repository root. Read implementations, callers and
existing tests, not just declarations or old reports:

docs/reports/2026-10-07_rate_boundary.md, 2026-10-07_position_completion.md, 2026-10-07_position_limits.md; existing host bench scripts; src/profiles/ess_rs/Position.cpp, Actions.cpp; standalone result/health capture.

## Work

1. Recover exact historical failing cases and preserve their original evidence.
   Correlate drive supply/transient measurement, UART traffic, host health,
   raw drive state and volatile settings. Distinguish drive reboot, communication
   loss, host reset and still-running motion; do not infer the cause from one.
2. Test only the agreed conservative reproduction envelope. The 200 kHz external
   pulse-input rating does not establish a serial internal-rate limit, and
   recorded resets below that rate disprove a simple universal threshold claim.
3. Investigate persistent RUNNING and normal-stop failures independently of the
   already corrected host stop observation budget. Retain stop and original
   motion outcomes separately.
4. Prepare a firmware/model/vendor evidence bundle. Do not contact a vendor
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
